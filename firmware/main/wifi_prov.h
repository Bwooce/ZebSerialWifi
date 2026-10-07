#pragma once
/*
 * WiFi station with SoftAP provisioning fallback. With no saved credentials,
 * or no IP within 60 s, an open AP "ZebSerial-XXXX" serves the setup page.
 */
#include "esp_wifi_types.h"

typedef enum {
    WIFI_ST_AP,          /* setup AP running, station not connected */
    WIFI_ST_CONNECTING,
    WIFI_ST_CONNECTED,
} wifi_prov_state_t;

/* Requires esp_netif_init() and the default event loop. */
void wifi_prov_start(void);

wifi_prov_state_t wifi_prov_state(void);

/* True while the setup AP is running (it can overlap with CONNECTING). */
bool wifi_prov_ap_active(void);

int wifi_prov_rssi(void);
void wifi_prov_ip(char out[16]);
const char *wifi_prov_ssid(void);
const char *wifi_prov_ap_ssid(void);

/* Blocking scan, returns the number of records written (max entries). */
int wifi_prov_scan(wifi_ap_record_t *out, int max);
