#include "settings.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define NS "zebserial"

esp_err_t settings_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

bool settings_get_wifi(char ssid[SETTINGS_SSID_MAX], char pass[SETTINGS_PASS_MAX])
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t ssid_len = SETTINGS_SSID_MAX;
    size_t pass_len = SETTINGS_PASS_MAX;
    bool ok = nvs_get_str(h, "ssid", ssid, &ssid_len) == ESP_OK && ssid[0] != '\0';
    if (ok && nvs_get_str(h, "pass", pass, &pass_len) != ESP_OK) {
        pass[0] = '\0';
    }
    nvs_close(h);
    return ok;
}

esp_err_t settings_set_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(h, "pass", pass);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t settings_clear_wifi(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    nvs_erase_key(h, "ssid");
    nvs_erase_key(h, "pass");
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

uint32_t settings_get_baud(void)
{
    nvs_handle_t h;
    uint32_t baud = 0;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u32(h, "baud", &baud);
        nvs_close(h);
    }
    return baud;
}

esp_err_t settings_set_baud(uint32_t baud)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u32(h, "baud", baud);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

bool settings_target_baud_valid(uint32_t baud)
{
    static const uint32_t ok[] = {0, 9600, 19200, 38400, 57600, 115200};
    for (size_t i = 0; i < sizeof(ok) / sizeof(ok[0]); i++) {
        if (ok[i] == baud) {
            return true;
        }
    }
    return false;
}

void settings_get_options(settings_options_t *out)
{
    out->idle_timeout_s = CONFIG_ZEB_IDLE_TIMEOUT_S;
    out->target_baud = 115200;
    out->pins = SETTINGS_PINS_AUTO;

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    if (nvs_get_u16(h, "idle_s", &u16) == ESP_OK && u16 >= SETTINGS_IDLE_MIN_S && u16 <= SETTINGS_IDLE_MAX_S) {
        out->idle_timeout_s = u16;
    }
    if (nvs_get_u32(h, "tgt_baud", &u32) == ESP_OK && settings_target_baud_valid(u32)) {
        out->target_baud = u32;
    }
    if (nvs_get_u8(h, "pins", &u8) == ESP_OK && u8 <= SETTINGS_PINS_TX_D5) {
        out->pins = (settings_pins_t)u8;
    }
    nvs_close(h);
}

esp_err_t settings_set_options(const settings_options_t *opt)
{
    if (opt->idle_timeout_s < SETTINGS_IDLE_MIN_S || opt->idle_timeout_s > SETTINGS_IDLE_MAX_S ||
        !settings_target_baud_valid(opt->target_baud) || opt->pins > SETTINGS_PINS_TX_D5) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u16(h, "idle_s", opt->idle_timeout_s);
    if (err == ESP_OK) {
        err = nvs_set_u32(h, "tgt_baud", opt->target_baud);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(h, "pins", (uint8_t)opt->pins);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}
