#include "settings.h"

#include <string.h>
#include "nvs.h"
#include "netcfg.h"
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

uint32_t settings_get_baud_cap(const char *model)
{
    nvs_handle_t h;
    uint32_t cap = 0;
    char stored[32] = "";
    size_t len = sizeof(stored);
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_str(h, "cap_model", stored, &len) != ESP_OK || strcmp(stored, model) != 0 ||
            nvs_get_u32(h, "cap_baud", &cap) != ESP_OK) {
            cap = 0;
        }
        nvs_close(h);
    }
    return cap;
}

esp_err_t settings_set_baud_cap(const char *model, uint32_t cap)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, "cap_model", model);
    if (err == ESP_OK) {
        err = nvs_set_u32(h, "cap_baud", cap);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

void settings_get_rlog(settings_rlog_t *out)
{
    out->enabled = true;
    out->level = 3;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint8_t u8;
    if (nvs_get_u8(h, "rlog_on", &u8) == ESP_OK) {
        out->enabled = u8 != 0;
    }
    if (nvs_get_u8(h, "rlog_lvl", &u8) == ESP_OK && u8 >= SETTINGS_RLOG_LEVEL_MIN && u8 <= SETTINGS_RLOG_LEVEL_MAX) {
        out->level = u8;
    }
    nvs_close(h);
}

esp_err_t settings_set_rlog(const settings_rlog_t *rlog)
{
    if (rlog->level < SETTINGS_RLOG_LEVEL_MIN || rlog->level > SETTINGS_RLOG_LEVEL_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(h, "rlog_on", rlog->enabled ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_set_u8(h, "rlog_lvl", rlog->level);
    }
    if (err == ESP_OK) {
        /* earlier builds kept a fixed receiver address here; the log is multicast only now */
        esp_err_t e = nvs_erase_key(h, "rlog_ip");
        err = e == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : e;
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

void settings_get_model(char out[32])
{
    nvs_handle_t h;
    size_t len = 32;
    out[0] = '\0';
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_str(h, "model", out, &len) != ESP_OK) {
            out[0] = '\0';
        }
        nvs_close(h);
    }
}

esp_err_t settings_set_model(const char *model)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, "model", model);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
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

bool settings_options_valid(const settings_options_t *opt)
{
    return opt->idle_timeout_s >= SETTINGS_IDLE_MIN_S && opt->idle_timeout_s <= SETTINGS_IDLE_MAX_S &&
           settings_target_baud_valid(opt->target_baud) &&
           opt->pins >= SETTINGS_PINS_AUTO && opt->pins <= SETTINGS_PINS_TX_D5;
}

esp_err_t settings_set_options(const settings_options_t *opt)
{
    if (!settings_options_valid(opt)) {
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

bool settings_get_flag(const char *key)
{
    nvs_handle_t h;
    uint8_t v = 0;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, key, &v);
        nvs_close(h);
    }
    return v != 0;
}

esp_err_t settings_set_flag(const char *key, bool value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(h, key, value ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

void settings_get_net(settings_net_t *out)
{
    memset(out, 0, sizeof(*out));
    strlcpy(out->hostname, SETTINGS_DEFAULT_HOSTNAME, sizeof(out->hostname));
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    char name[sizeof(out->hostname)];
    size_t len = sizeof(name);
    if (nvs_get_str(h, "hostname", name, &len) == ESP_OK && netcfg_hostname_valid(name)) {
        strlcpy(out->hostname, name, sizeof(out->hostname));
    }
    uint8_t st = 0;
    uint32_t ip = 0;
    uint32_t mask = 0;
    uint32_t gw = 0;
    uint32_t dns = 0;
    nvs_get_u8(h, "static_ip", &st);
    nvs_get_u32(h, "ip", &ip);
    nvs_get_u32(h, "mask", &mask);
    nvs_get_u32(h, "gw", &gw);
    nvs_get_u32(h, "dns", &dns);
    nvs_close(h);
    /* only trust a static config that still validates */
    if (st && netcfg_static_valid(ip, mask, gw)) {
        out->static_ip = true;
        out->ip = ip;
        out->mask = mask;
        out->gw = gw;
        out->dns = dns;
    }
}

bool settings_net_valid(const settings_net_t *net)
{
    return netcfg_hostname_valid(net->hostname) &&
           (!net->static_ip || netcfg_static_valid(net->ip, net->mask, net->gw));
}

esp_err_t settings_set_net(const settings_net_t *net)
{
    if (!settings_net_valid(net)) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, "hostname", net->hostname);
    if (err == ESP_OK) {
        err = nvs_set_u8(h, "static_ip", net->static_ip ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(h, "ip", net->ip);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(h, "mask", net->mask);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(h, "gw", net->gw);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(h, "dns", net->dns);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t settings_clear_net(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    static const char *keys[] = {"hostname", "static_ip", "ip", "mask", "gw", "dns"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        nvs_erase_key(h, keys[i]);
    }
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}
