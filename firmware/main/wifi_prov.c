#include "wifi_prov.h"

#include <stdio.h>
#include <string.h>
#include "dns_hijack.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "settings.h"

static const char *TAG = "wifi";

#define STA_TIMEOUT_MS 60000
#define STA_RETRY_IN_AP_MS 30000  /* station retries disturb the AP channel, so back off */
#define STA_RETRY_MS 2000

static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
/*
 * start_ap runs from the fallback timer and stop_ap from the event task.
 * Both, and every write of s_state, happen under this lock, so a GOT_IP
 * that lands while the AP is starting cannot leave the AP up once connected.
 */
static SemaphoreHandle_t s_ap_lock;
static volatile wifi_prov_state_t s_state = WIFI_ST_CONNECTING;
static volatile bool s_ap_active;
static bool s_have_creds;
static char s_ssid[SETTINGS_SSID_MAX];
static char s_ap_ssid[20];
static esp_timer_handle_t s_fallback_timer;
static esp_timer_handle_t s_retry_timer;
static settings_net_t s_net;

static void apply_static_ip(void)
{
    esp_err_t err = esp_netif_dhcpc_stop(s_sta_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, "dhcpc stop: %s", esp_err_to_name(err));
    }
    esp_netif_ip_info_t info = {
        .ip.addr = htonl(s_net.ip),
        .netmask.addr = htonl(s_net.mask),
        .gw.addr = htonl(s_net.gw),
    };
    err = esp_netif_set_ip_info(s_sta_netif, &info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "static IP: %s", esp_err_to_name(err));
        return;
    }
    uint32_t dns = s_net.dns ? s_net.dns : s_net.gw;
    if (dns) {
        esp_netif_dns_info_t d = {.ip.type = ESP_IPADDR_TYPE_V4, .ip.u_addr.ip4.addr = htonl(dns)};
        esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &d);
    }
}

/* Set the mode with both interfaces at 20 MHz; 40 MHz would force the station down from 11ax to 11n. */
static void set_mode(wifi_mode_t mode)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        esp_err_t err = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW20);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "AP bandwidth: %s", esp_err_to_name(err));
        }
    }
    esp_err_t err = esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW20);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "STA bandwidth: %s", esp_err_to_name(err));
    }
}

static void start_ap(void)
{
    xSemaphoreTake(s_ap_lock, portMAX_DELAY);
    /* re-checked under the lock: the station may have got its IP since the caller looked */
    if (s_ap_active || s_state == WIFI_ST_CONNECTED) {
        xSemaphoreGive(s_ap_lock);
        return;
    }
    ESP_LOGI(TAG, "starting setup AP %s", s_ap_ssid);
    set_mode(WIFI_MODE_APSTA);
    wifi_config_t ap = {
        .ap = {
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    strlcpy((char *)ap.ap.ssid, s_ap_ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(s_ap_ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    s_ap_active = true;
    s_state = WIFI_ST_AP;
    dns_hijack_start();
    xSemaphoreGive(s_ap_lock);
}

static void stop_ap(void)
{
    xSemaphoreTake(s_ap_lock, portMAX_DELAY);
    if (s_ap_active) {
        ESP_LOGI(TAG, "station connected, stopping setup AP");
        dns_hijack_stop();
        set_mode(WIFI_MODE_STA);
        s_ap_active = false;
    }
    xSemaphoreGive(s_ap_lock);
}

static void set_state(wifi_prov_state_t st)
{
    xSemaphoreTake(s_ap_lock, portMAX_DELAY);
    s_state = st;
    xSemaphoreGive(s_ap_lock);
}

static void fallback_cb(void *arg)
{
    (void)arg;
    if (s_state != WIFI_ST_CONNECTED) {
        ESP_LOGW(TAG, "no IP after %d s", STA_TIMEOUT_MS / 1000);
        start_ap();
    }
}

static void retry_cb(void *arg)
{
    (void)arg;
    if (s_have_creds && s_state != WIFI_ST_CONNECTED) {
        esp_wifi_connect();
    }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_have_creds) {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        esp_netif_create_ip6_linklocal(s_sta_netif);
        if (s_net.static_ip) {
            apply_static_ip();  /* posts IP_EVENT_STA_GOT_IP */
        }
    } else if (base == IP_EVENT && id == IP_EVENT_GOT_IP6) {
        ip_event_got_ip6_t *e = data;
        ESP_LOGI(TAG, "got IPv6 " IPV6STR, IPV62STR(e->ip6_info.ip));
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = data;
        if (s_state == WIFI_ST_CONNECTED) {
            ESP_LOGW(TAG, "disconnected, reason %d", d->reason);
            esp_timer_stop(s_fallback_timer);
            esp_timer_start_once(s_fallback_timer, (uint64_t)STA_TIMEOUT_MS * 1000);
        }
        xSemaphoreTake(s_ap_lock, portMAX_DELAY);
        s_state = s_ap_active ? WIFI_ST_AP : WIFI_ST_CONNECTING;
        xSemaphoreGive(s_ap_lock);
        if (s_have_creds) {
            esp_timer_stop(s_retry_timer);
            esp_timer_start_once(s_retry_timer, (uint64_t)(s_ap_active ? STA_RETRY_IN_AP_MS : STA_RETRY_MS) * 1000);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&e->ip_info.ip));
        set_state(WIFI_ST_CONNECTED);
        esp_timer_stop(s_fallback_timer);
        stop_ap();
    }
}

void wifi_prov_start(void)
{
    s_ap_lock = xSemaphoreCreateMutex();
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    settings_get_net(&s_net);
    esp_netif_set_hostname(s_sta_netif, s_net.hostname);
    if (s_net.static_ip) {
        ESP_LOGI(TAG, "configured static IP " IPSTR, IP2STR(&(esp_ip4_addr_t){.addr = htonl(s_net.ip)}));
    }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "ZebSerial-%02X%02X", mac[4], mac[5]);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, on_event, NULL));

    const esp_timer_create_args_t fb = {.callback = fallback_cb, .name = "wifi_fb"};
    ESP_ERROR_CHECK(esp_timer_create(&fb, &s_fallback_timer));
    const esp_timer_create_args_t rt = {.callback = retry_cb, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&rt, &s_retry_timer));

    char pass[SETTINGS_PASS_MAX];
    s_have_creds = settings_get_wifi(s_ssid, pass);
    if (s_have_creds) {
        wifi_config_t sta = {0};
        strlcpy((char *)sta.sta.ssid, s_ssid, sizeof(sta.sta.ssid));
        strlcpy((char *)sta.sta.password, pass, sizeof(sta.sta.password));
        sta.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
        sta.sta.pmf_cfg.capable = true;
        /*
         * The default fast scan joins the first access point it hears with
         * this SSID; with several (a mesh, or a far one on the same name)
         * that can be one at -90 dBm while another is at -30. Scan every
         * channel and take the strongest.
         */
        sta.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        sta.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        set_mode(WIFI_MODE_STA);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
        ESP_LOGI(TAG, "connecting to \"%s\"", s_ssid);
        set_state(WIFI_ST_CONNECTING);
        esp_timer_start_once(s_fallback_timer, (uint64_t)STA_TIMEOUT_MS * 1000);
        ESP_ERROR_CHECK(esp_wifi_start());
        /* USB powered: modem sleep only adds latency (up to a DTIM interval) to every web and print reply */
        esp_wifi_set_ps(WIFI_PS_NONE);
    } else {
        ESP_LOGI(TAG, "no saved WiFi credentials");
        set_mode(WIFI_MODE_APSTA);
        ESP_ERROR_CHECK(esp_wifi_start());
        start_ap();
    }
    memset(pass, 0, sizeof(pass));
}

wifi_prov_state_t wifi_prov_state(void)
{
    return s_state;
}

bool wifi_prov_ap_active(void)
{
    return s_ap_active;
}

bool wifi_prov_sock_on_ap(int fd)
{
    if (!s_ap_active) {
        return false;
    }
    struct sockaddr_storage local;
    socklen_t len = sizeof(local);
    esp_netif_ip_info_t ap;
    if (getsockname(fd, (struct sockaddr *)&local, &len) != 0 || esp_netif_get_ip_info(s_ap_netif, &ap) != ESP_OK) {
        return true;
    }
    uint32_t addr;
    if (local.ss_family == AF_INET) {
        addr = ((struct sockaddr_in *)&local)->sin_addr.s_addr;
    } else if (local.ss_family == AF_INET6) {
        /*
         * Dual-stack sockets report IPv4 as ::ffff:a.b.c.d. A native IPv6
         * address is never the AP's: only the station gets a link-local.
         */
        const uint8_t *b = ((struct sockaddr_in6 *)&local)->sin6_addr.s6_addr;
        static const uint8_t v4_mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
        if (memcmp(b, v4_mapped, sizeof(v4_mapped)) != 0) {
            return false;
        }
        memcpy(&addr, b + 12, sizeof(addr));
    } else {
        return true;
    }
    return addr == ap.ip.addr;
}

bool wifi_prov_link(char *phy, size_t phy_len, int *channel, char bssid[18])
{
    phy[0] = '\0';
    *channel = 0;
    bssid[0] = '\0';
    wifi_ap_record_t ap;
    if (s_state != WIFI_ST_CONNECTED || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        return false;
    }
    *channel = ap.primary;
    snprintf(bssid, 18, "%02x:%02x:%02x:%02x:%02x:%02x", ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3],
             ap.bssid[4], ap.bssid[5]);
    wifi_phy_mode_t mode;
    if (esp_wifi_sta_get_negotiated_phymode(&mode) == ESP_OK) {
        const char *name = "";
        switch (mode) {
        case WIFI_PHY_MODE_HE20:
            name = "Wi-Fi 6 (802.11ax)";
            break;
        case WIFI_PHY_MODE_VHT20:
            name = "Wi-Fi 5 (802.11ac)";
            break;
        case WIFI_PHY_MODE_HT20:
        case WIFI_PHY_MODE_HT40:
            name = "Wi-Fi 4 (802.11n)";
            break;
        case WIFI_PHY_MODE_11G:
            name = "802.11g";
            break;
        case WIFI_PHY_MODE_11A:
            name = "802.11a";
            break;
        case WIFI_PHY_MODE_11B:
            name = "802.11b";
            break;
        case WIFI_PHY_MODE_LR:
            name = "Espressif long range";
            break;
        default:
            break;
        }
        strlcpy(phy, name, phy_len);
    }
    return true;
}

int wifi_prov_rssi(void)
{
    wifi_ap_record_t ap;
    if (s_state == WIFI_ST_CONNECTED && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return 0;
}

void wifi_prov_ip(char out[16])
{
    esp_netif_ip_info_t info = {0};
    out[0] = '\0';
    if (s_state == WIFI_ST_CONNECTED && esp_netif_get_ip_info(s_sta_netif, &info) == ESP_OK) {
        snprintf(out, 16, IPSTR, IP2STR(&info.ip));
    }
}

int wifi_prov_ip6(char out[WIFI_PROV_IP6_MAX][46])
{
    if (s_state != WIFI_ST_CONNECTED) {
        return 0;
    }
    esp_ip6_addr_t addrs[LWIP_IPV6_NUM_ADDRESSES];
    int n = esp_netif_get_all_ip6(s_sta_netif, addrs);
    int k = 0;
    for (int i = 0; i < n && k < WIFI_PROV_IP6_MAX; i++) {
        snprintf(out[k++], 46, IPV6STR, IPV62STR(addrs[i]));
    }
    return k;
}

const char *wifi_prov_ssid(void)
{
    return s_ssid;
}

const char *wifi_prov_ap_ssid(void)
{
    return s_ap_ssid;
}

int wifi_prov_scan(wifi_ap_record_t *out, int max)
{
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        return 0;
    }
    uint16_t n = (uint16_t)max;
    if (esp_wifi_scan_get_ap_records(&n, out) != ESP_OK) {
        return 0;
    }
    return n;
}
