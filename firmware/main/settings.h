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

/* Runtime options, editable from the status page. */
typedef enum {
    SETTINGS_PINS_AUTO = 0,     /* detect which pin is RX at boot */
    SETTINGS_PINS_TX_D3 = 1,    /* TX=GPIO21 (D3), RX=GPIO23 (D5) */
    SETTINGS_PINS_TX_D5 = 2,    /* TX=GPIO23 (D5), RX=GPIO21 (D3) */
} settings_pins_t;

/*
 * The antenna choice is deliberately not here: selecting U.FL with no
 * antenna fitted would cut WiFi and lock the user out of this page. It is
 * the build-time option CONFIG_ZEB_EXTERNAL_ANTENNA.
 */
typedef struct {
    uint16_t idle_timeout_s;    /* 5..3600 */
    uint32_t target_baud;       /* 0 = leave the printer at whatever baud it is on */
    settings_pins_t pins;
} settings_options_t;

#define SETTINGS_IDLE_MIN_S 5
#define SETTINGS_IDLE_MAX_S 3600

/* Saved options, or the Kconfig defaults for anything not saved. */
void settings_get_options(settings_options_t *out);

/* True if every option is in range (what settings_set_options checks). */
bool settings_options_valid(const settings_options_t *opt);

/* Validates and saves. Returns ESP_ERR_INVALID_ARG for out-of-range values. */
esp_err_t settings_set_options(const settings_options_t *opt);

/* True for 0 (leave as is) and the standard rates 9600..115200. */
bool settings_target_baud_valid(uint32_t baud);

/* Network identity and addressing. Addresses are IPv4 in host byte order. */
typedef struct {
    char hostname[33];          /* DHCP hostname and mDNS <hostname>.local */
    bool static_ip;
    uint32_t ip;
    uint32_t mask;
    uint32_t gw;                /* 0 = none */
    uint32_t dns;               /* 0 = use gw */
} settings_net_t;

#define SETTINGS_DEFAULT_HOSTNAME "zebserial"

/* Saved network settings, or DHCP with the default hostname. */
void settings_get_net(settings_net_t *out);

/* Hostname rules and, for a static address, subnet sanity (what settings_set_net checks). */
bool settings_net_valid(const settings_net_t *net);

/* Validates (hostname rules, subnet sanity) and saves. ESP_ERR_INVALID_ARG if invalid. */
esp_err_t settings_set_net(const settings_net_t *net);

/* Back to DHCP and the default hostname (used by the BOOT-button reset). */
esp_err_t settings_clear_net(void);

/* Small persistent flags (0/1), false when unset. */
bool settings_get_flag(const char *key);
esp_err_t settings_set_flag(const char *key, bool value);

/* Last printer model found (from ~HI), "" when none has been; names the bridge in Bonjour. */
void settings_get_model(char out[32]);
esp_err_t settings_set_model(const char *model);

/* Returns 0 when no baud has been saved. */
uint32_t settings_get_baud(void);

/*
 * Lowest baud a printer of this model failed to move to (0 if none known):
 * a model mismatch reads as 0, so a different printer starts from the top.
 * settings_set_baud_cap("", 0) forgets it.
 */
uint32_t settings_get_baud_cap(const char *model);
esp_err_t settings_set_baud_cap(const char *model, uint32_t cap);
esp_err_t settings_set_baud(uint32_t baud);
