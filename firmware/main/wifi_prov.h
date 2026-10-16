#pragma once
/*
 * WiFi station with SoftAP provisioning fallback. With no saved credentials,
 * or no IP within 60 s, an open AP "ZebSerial-XXXX" serves the setup page.
 */
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "settings.h"

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

/*
 * The link to the access point: the negotiated standard ("Wi-Fi 6
 * (802.11ax)", ...), the channel and the access point's MAC address.
 * False (and empty values) when not connected.
 */
bool wifi_prov_link(char *phy, size_t phy_len, int *channel, char bssid[18]);
void wifi_prov_ip(char out[16]);

#define WIFI_PROV_IP6_MAX 3
/* IPv6 addresses of the station (link-local and SLAAC), as text. Returns how many. */
int wifi_prov_ip6(char out[WIFI_PROV_IP6_MAX][46]);
const char *wifi_prov_ssid(void);
const char *wifi_prov_ap_ssid(void);

/* Blocking scan, returns the number of records written (max entries). */
int wifi_prov_scan(wifi_ap_record_t *out, int max);

typedef enum {
    WIFI_JOIN_NONE,     /* no join asked for since boot */
    WIFI_JOIN_TRYING,
    WIFI_JOIN_JOINED,   /* the new network gave an address and is saved */
    WIFI_JOIN_FAILED,   /* it did not within 30 s; the saved network was restored */
} wifi_join_state_t;

/*
 * Try another network without giving up the current one: about 1 s from
 * now the station leaves for ssid, and the credentials are saved only once
 * it gets an address there within 30 s; otherwise it goes back to the
 * saved network. ESP_ERR_INVALID_STATE unless connected and no join is
 * already running.
 */
esp_err_t wifi_prov_join(const char *ssid, const char *pass);

/* The latest join and, for a failure or a retry, the last reason in words ("" if none). */
wifi_join_state_t wifi_prov_join_state(char ssid[SETTINGS_SSID_MAX], const char **why);
