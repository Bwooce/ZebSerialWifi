#pragma once
/*
 * WiFi station with SoftAP provisioning fallback. With no saved credentials,
 * or no IP within 60 s, an open AP "ZebSerial-XXXX" serves the setup page.
 */
#include <stdbool.h>
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

/*
 * True if the connected socket fd was reached through the setup AP (its
 * local address is the AP's). If that cannot be read while the AP is up,
 * answers true, so callers fail closed.
 */
bool wifi_prov_sock_on_ap(int fd);

int wifi_prov_rssi(void);
void wifi_prov_ip(char out[16]);

#define WIFI_PROV_IP6_MAX 3
/* IPv6 addresses of the station (link-local and SLAAC), as text. Returns how many. */
int wifi_prov_ip6(char out[WIFI_PROV_IP6_MAX][46]);
const char *wifi_prov_ssid(void);
const char *wifi_prov_ap_ssid(void);

/* Blocking scan, returns the number of records written (max entries). */
int wifi_prov_scan(wifi_ap_record_t *out, int max);
