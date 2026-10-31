#include "ota.h"
#include <stdlib.h>
#include <string.h>
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "wifi_prov.h"

static const char *TAG = "ota";

#define OTA_BUF 4096
#define OTA_RECV_RETRIES 5
/* the image header, its first segment header and the app description, which the checks read */
#define OTA_HEAD (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))

static bool s_busy;
/* the other slot holds a whole, valid image: 1 yes, 0 no, -1 not checked since it was last written */
static int s_other_ok = -1;

void ota_poll(void)
{
    static bool done;
    if (done) {
        return;
    }
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        done = true;
        return;
    }
    int64_t up_s = esp_timer_get_time() / 1000000;
    if (up_s >= OTA_CONFIRM_AFTER_S && wifi_prov_state() == WIFI_ST_CONNECTED) {
        done = true;
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "new firmware confirmed: it reached the network");
    } else if (up_s >= OTA_CONFIRM_DEADLINE_S) {
        done = true;
        ESP_LOGE(TAG, "new firmware did not reach the network in %d s: going back to the previous one",
                 OTA_CONFIRM_DEADLINE_S);
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

/* Header checks on the first OTA_HEAD bytes: an ESP32-C6 app image built from this project. */
static const char *check_head(const uint8_t *buf)
{
    const esp_image_header_t *img = (const esp_image_header_t *)buf;
    if (img->magic != ESP_IMAGE_HEADER_MAGIC) {
        return "Not an ESP32 app image. Upload build/zebserial.bin, not the ELF or a merged image.";
    }
    if (img->chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID) {
        return "This image is for a different ESP32 chip.";
    }
    const esp_app_desc_t *desc =
        (const esp_app_desc_t *)(buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
    if (desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        return "The image has no app description; it is not an ESP-IDF app.";
    }
    if (strncmp(desc->project_name, esp_app_get_description()->project_name, sizeof(desc->project_name)) != 0) {
        return "This image is from a different project, not ZebSerialWifi.";
    }
    return NULL;
}

/* Fill buf[have..want) from the request; returns the new length, or -1 on a receive error. */
static int recv_some(httpd_req_t *req, uint8_t *buf, int have, int want)
{
    int retries = 0;
    while (have < want) {
        int n = httpd_req_recv(req, (char *)buf + have, want - have);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++retries <= OTA_RECV_RETRIES) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        have += n;
    }
    return have;
}

esp_err_t ota_receive(httpd_req_t *req, const char **why)
{
    *why = NULL;
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (next == NULL) {
        *why = "This bridge has no second firmware slot. Flash a build with the OTA partition table over USB once.";
        return ESP_ERR_NOT_FOUND;
    }
    if (req->content_len < OTA_HEAD || req->content_len > next->size) {
        *why = "The upload is the wrong size for a firmware image.";
        return ESP_ERR_INVALID_SIZE;
    }
    if (s_busy) {
        *why = "Another update is already in progress.";
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t *buf = malloc(OTA_BUF);
    if (buf == NULL) {
        *why = "Out of memory.";
        return ESP_ERR_NO_MEM;
    }
    s_busy = true;
    esp_ota_handle_t h = 0;
    bool begun = false;
    esp_err_t err = ESP_OK;
    size_t left = req->content_len;

    int have = recv_some(req, buf, 0, OTA_HEAD);
    if (have < 0) {
        *why = "The upload stopped.";
        err = ESP_FAIL;
        goto out;
    }
    if ((*why = check_head(buf)) != NULL) {
        err = ESP_ERR_INVALID_VERSION;
        goto out;
    }
    const esp_app_desc_t *desc =
        (const esp_app_desc_t *)(buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
    ESP_LOGI(TAG, "receiving %u bytes, version %.32s, into %s", (unsigned)req->content_len, desc->version,
             next->label);
    err = esp_ota_begin(next, OTA_WITH_SEQUENTIAL_WRITES, &h);
    if (err != ESP_OK) {
        *why = "Could not start writing the firmware slot.";
        goto out;
    }
    begun = true;
    s_other_ok = -1;  /* the slot's old image is being overwritten, whatever happens next */
    while (true) {
        err = esp_ota_write(h, buf, have);
        if (err != ESP_OK) {
            *why = "Writing the firmware slot failed.";
            goto out;
        }
        left -= have;
        if (left == 0) {
            break;
        }
        have = recv_some(req, buf, 0, left < OTA_BUF ? (int)left : OTA_BUF);
        if (have < 0) {
            *why = "The upload stopped.";
            err = ESP_FAIL;
            goto out;
        }
    }
    begun = false;
    err = esp_ota_end(h);  /* checks the image's segments and SHA-256 */
    if (err != ESP_OK) {
        *why = err == ESP_ERR_OTA_VALIDATE_FAILED ? "The image is damaged or incomplete." : "Finishing the update failed.";
        goto out;
    }
    err = esp_ota_set_boot_partition(next);
    if (err != ESP_OK) {
        *why = "Could not select the new firmware.";
        goto out;
    }
    ESP_LOGI(TAG, "update written to %s; it boots next", next->label);
out:
    if (begun) {
        esp_ota_abort(h);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "update refused: %s (%s)", *why, esp_err_to_name(err));
    }
    free(buf);
    s_busy = false;
    return err;
}

/* Check the whole image in the other slot once (its header alone survives a refused upload) */
static bool other_ok(const esp_partition_t *other)
{
    if (s_other_ok < 0) {
        esp_partition_pos_t pos = {.offset = other->address, .size = other->size};
        esp_image_metadata_t meta;
        s_other_ok = esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &pos, &meta) == ESP_OK;
    }
    return s_other_ok == 1;
}

void ota_get_status(ota_status_t *out)
{
    memset(out, 0, sizeof(*out));
    const esp_partition_t *run = esp_ota_get_running_partition();
    strlcpy(out->running, run->label, sizeof(out->running));
    strlcpy(out->version, esp_app_get_description()->version, sizeof(out->version));
    esp_ota_img_states_t state;
    out->pending = esp_ota_get_state_partition(run, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
    const esp_partition_t *other = esp_ota_get_next_update_partition(NULL);
    esp_app_desc_t desc;
    if (other != NULL && esp_ota_get_partition_description(other, &desc) == ESP_OK &&
        !(esp_ota_get_state_partition(other, &state) == ESP_OK &&
          (state == ESP_OTA_IMG_INVALID || state == ESP_OTA_IMG_ABORTED)) &&
        other_ok(other)) {
        strlcpy(out->other_version, desc.version, sizeof(out->other_version));
    }
}

esp_err_t ota_boot_other(void)
{
    ota_status_t st;
    ota_get_status(&st);
    const esp_partition_t *other = esp_ota_get_next_update_partition(NULL);
    if (other == NULL || st.other_version[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }
    return esp_ota_set_boot_partition(other);  /* checks the image first */
}
