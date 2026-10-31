#pragma once
/*
 * Firmware updates over the network into the other OTA slot, with rollback:
 * a new image must reach the network (station IP) within OTA_CONFIRM_DEADLINE_S
 * or the bridge reboots into the previous one. A crash before that also
 * rolls back, since the bootloader treats an unconfirmed image as failed.
 */
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_http_server.h"

#define OTA_CONFIRM_AFTER_S 15     /* a new image has run this long and is on Wi-Fi: keep it */
#define OTA_CONFIRM_DEADLINE_S 180 /* not on Wi-Fi by now: go back to the previous image */

/* Call once from the status loop, about every 100 ms: confirms or rolls back a new image. */
void ota_poll(void);

/*
 * Write the request body (a zebserial.bin app image) to the other slot and
 * make it the boot image. Checks it is an ESP32-C6 image of this project.
 * On failure *why says what went wrong; the running image is untouched.
 */
esp_err_t ota_receive(httpd_req_t *req, const char **why);

typedef struct {
    char running[16];       /* slot label, "ota_0" */
    char version[32];
    bool pending;           /* running a new image that is not confirmed yet */
    char other_version[32]; /* "" if the other slot has no valid image */
} ota_status_t;

void ota_get_status(ota_status_t *out);

/* Boot the image in the other slot (if it is a valid one) at the next restart. */
esp_err_t ota_boot_other(void);
