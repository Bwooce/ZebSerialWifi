#pragma once
/* Persistent settings in NVS namespace "zebserial". */
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define SETTINGS_SSID_MAX 33   /* 32 chars + NUL */
#define SETTINGS_PASS_MAX 65   /* 64 chars + NUL */

esp_err_t settings_init(void);

bool settings_get_wifi(char ssid[SETTINGS_SSID_MAX], char pass[SETTINGS_PASS_MAX]);
esp_err_t settings_set_wifi(const char *ssid, const char *pass);
esp_err_t settings_clear_wifi(void);

/* Returns 0 when no baud has been saved. */
uint32_t settings_get_baud(void);
esp_err_t settings_set_baud(uint32_t baud);
