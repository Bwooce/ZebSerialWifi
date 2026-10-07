#include "web.h"

#include <stdio.h>
#include <string.h>
#include "bridge.h"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "printer_link.h"
#include "settings.h"
#include "urlcodec.h"
#include "wifi_prov.h"

static const char *TAG = "web";

#define MAX_SCAN 20
#define BODY_MAX 512
#define SCAN_MAX_AGE_US (60LL * 1000 * 1000)

static const char PAGE_HEAD[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>ZebSerial</title><style>"
    "body{font-family:system-ui,sans-serif;max-width:32em;margin:1em auto;padding:0 1em;color:#222;background:#fff}"
    "@media(prefers-color-scheme:dark){body{color:#ddd;background:#181818}input,select{background:#222;color:#ddd}}"
    "input,select,button{font-size:1em;padding:.4em;margin:.2em 0;width:100%;box-sizing:border-box}"
    "td{padding:.15em .6em .15em 0;vertical-align:top}td:first-child{opacity:.7}"
    "form.inline{display:inline-block;width:48%}"
    "</style></head><body>";
static const char PAGE_TAIL[] = "</body></html>";

static void restart_cb(void *arg)
{
    (void)arg;
    esp_restart();
}

static void restart_soon(void)
{
    static esp_timer_handle_t t;
    if (t == NULL) {
        const esp_timer_create_args_t args = {.callback = restart_cb, .name = "restart"};
        esp_timer_create(&args, &t);
    }
    esp_timer_start_once(t, 1500 * 1000);
}

/* Minimal HTML escaping for SSIDs. */
static void html_escape(const char *in, char *out, size_t out_len)
{
    size_t o = 0;
    for (; *in && o + 7 < out_len; in++) {
        const char *rep = NULL;
        switch (*in) {
        case '&': rep = "&amp;"; break;
        case '<': rep = "&lt;"; break;
        case '>': rep = "&gt;"; break;
        case '"': rep = "&quot;"; break;
        case '\'': rep = "&#39;"; break;
        default: break;
        }
        if (rep) {
            size_t l = strlen(rep);
            memcpy(out + o, rep, l);
            o += l;
        } else {
            out[o++] = *in;
        }
    }
    out[o] = '\0';
}

static esp_err_t send_setup_page(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, PAGE_HEAD);
    httpd_resp_sendstr_chunk(req, "<h2>ZebSerial WiFi setup</h2><form method=post action=/save>"
                                  "<label>Network</label><select name=ssid>");
    /*
     * Phones fire several captive-portal probes that all land here; scanning
     * on each one disrupts the AP, so reuse a recent scan unless asked.
     */
    static wifi_ap_record_t recs[MAX_SCAN];
    static int n;
    static int64_t scanned_at;
    char q[16] = "";
    bool want_rescan = httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK && strstr(q, "rescan") != NULL;
    int64_t now = esp_timer_get_time();
    if (want_rescan || scanned_at == 0 || now - scanned_at > SCAN_MAX_AGE_US) {
        n = wifi_prov_scan(recs, MAX_SCAN);
        scanned_at = now;
    }
    char esc[200];
    char line[512];
    for (int i = 0; i < n; i++) {
        const char *ssid = (const char *)recs[i].ssid;
        if (ssid[0] == '\0') {
            continue;
        }
        bool dup = false;
        for (int j = 0; j < i; j++) {
            dup |= strcmp(ssid, (const char *)recs[j].ssid) == 0;
        }
        if (dup) {
            continue;
        }
        html_escape(ssid, esc, sizeof(esc));
        snprintf(line, sizeof(line), "<option value=\"%s\">%s (%d dBm)</option>", esc, esc, recs[i].rssi);
        httpd_resp_sendstr_chunk(req, line);
    }
    httpd_resp_sendstr_chunk(req, "</select>"
                                  "<label>or type a network name</label><input name=ssid_manual maxlength=32>"
                                  "<label>Password</label><input name=pass type=password maxlength=64>"
                                  "<button type=submit>Save and reboot</button></form>"
                                  "<p><a href='/?rescan=1'>Scan again</a></p>");
    httpd_resp_sendstr_chunk(req, PAGE_TAIL);
    return httpd_resp_sendstr_chunk(req, NULL);
}

static const char STATUS_BODY[] =
    "<h2>ZebSerial</h2><table id=t></table>"
    "<form class=inline method=post action=/redetect><button>Re-detect printer</button></form> "
    "<form class=inline method=post action=/forget onsubmit=\"return confirm('Forget WiFi and reboot into setup mode?')\">"
    "<button>Forget WiFi</button></form>"
    "<script>"
    "function row(k,v){return '<tr><td>'+k+'</td><td>'+v+'</td></tr>'}"
    "function flags(h){var f=[];for(var k in h)if(h[k]===true)f.push(k.replace('_',' '));return f.length?f.join(', '):'ok'}"
    "async function upd(){try{var s=await(await fetch('/api/status')).json();var p=s.printer,b=s.bridge;"
    "document.getElementById('t').innerHTML="
    "row('WiFi',s.wifi.ssid+' '+s.wifi.ip+' ('+s.wifi.rssi+' dBm)')+"
    "row('Printer',p.detected?('detected at '+p.baud+' baud'):('not detected (trying '+p.baud+')'))+"
    "row('Status',p.hs_valid?flags(p.hs)+' ('+Math.round(p.hs_age_ms/1000)+' s ago)':'unknown')+"
    "row('Handshake',p.handshake||'unknown')+"
    "row('UART pins','TX GPIO'+p.tx_gpio+', RX GPIO'+p.rx_gpio)+"
    "row('Print client',b.active?('printing from '+b.client_ip):('idle, last '+(b.client_ip||'none')))+"
    "row('Jobs',b.sessions+' ('+b.aborted+' dropped)')+"
    "row('Bytes',b.to_printer+' to printer, '+b.from_printer+' back')+"
    "row('Uptime',Math.round(s.uptime_s/60)+' min')+"
    "row('Firmware',s.version+' '+s.built)}catch(e){}}"
    "upd();setInterval(upd,2000)"
    "</script>";

static esp_err_t root_get(httpd_req_t *req)
{
    if (wifi_prov_ap_active()) {
        return send_setup_page(req);
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, PAGE_HEAD);
    httpd_resp_sendstr_chunk(req, STATUS_BODY);
    httpd_resp_sendstr_chunk(req, PAGE_TAIL);
    return httpd_resp_sendstr_chunk(req, NULL);
}

static esp_err_t save_post(httpd_req_t *req)
{
    char body[BODY_MAX + 1];
    if (req->content_len > BODY_MAX) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "form too large");
    }
    int got = 0;
    while (got < (int)req->content_len) {
        int r = httpd_req_recv(req, body + got, req->content_len - got);
        if (r <= 0) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "read failed");
        }
        got += r;
    }
    body[got] = '\0';

    char ssid[SETTINGS_SSID_MAX] = "";
    char manual[SETTINGS_SSID_MAX] = "";
    char pass[SETTINGS_PASS_MAX] = "";
    form_get(body, "ssid", ssid, sizeof(ssid));
    form_get(body, "ssid_manual", manual, sizeof(manual));
    form_get(body, "pass", pass, sizeof(pass));
    memset(body, 0, sizeof(body));
    if (manual[0] != '\0') {
        strlcpy(ssid, manual, sizeof(ssid));
    }
    if (ssid[0] == '\0') {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no network chosen");
    }
    if (pass[0] != '\0' && strlen(pass) < 8) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "WPA passwords are at least 8 characters");
    }
    esp_err_t err = settings_set_wifi(ssid, pass);
    memset(pass, 0, sizeof(pass));
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not save");
    }
    ESP_LOGI(TAG, "saved WiFi network \"%s\", rebooting", ssid);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, PAGE_HEAD);
    httpd_resp_sendstr_chunk(req, "<p>Saved. Rebooting and joining the network. "
                                  "Then open <a href='http://zebserial.local/'>http://zebserial.local/</a>.</p>");
    httpd_resp_sendstr_chunk(req, PAGE_TAIL);
    httpd_resp_sendstr_chunk(req, NULL);
    restart_soon();
    return ESP_OK;
}

static esp_err_t status_get(httpd_req_t *req)
{
    printer_status_t p;
    bridge_stats_t b;
    printer_link_get_status(&p);
    bridge_get_stats(&b);
    char ip[16];
    wifi_prov_ip(ip);
    char ssid_esc[100];
    /* SSIDs can hold quotes and backslashes; drop anything that would break the JSON */
    size_t o = 0;
    for (const char *s = wifi_prov_ssid(); *s && o + 1 < sizeof(ssid_esc); s++) {
        if (*s != '"' && *s != '\\' && (unsigned char)*s >= 0x20) {
            ssid_esc[o++] = *s;
        }
    }
    ssid_esc[o] = '\0';
    const esp_app_desc_t *app = esp_app_get_description();

    char json[1024];
    snprintf(json, sizeof(json),
             "{\"wifi\":{\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d},"
             "\"printer\":{\"detected\":%s,\"baud\":%lu,\"handshake\":\"%s\",\"tx_gpio\":%d,\"rx_gpio\":%d,"
             "\"hs_valid\":%s,\"hs_age_ms\":%lld,\"hs\":{\"paper_out\":%s,\"pause\":%s,\"head_up\":%s,"
             "\"ribbon_out\":%s,\"buffer_full\":%s,\"corrupt_ram\":%s,\"under_temp\":%s,\"over_temp\":%s,"
             "\"formats_in_buffer\":%d},\"tx_bytes\":%llu,\"rx_bytes\":%llu},"
             "\"bridge\":{\"active\":%s,\"client_ip\":\"%s\",\"sessions\":%lu,\"aborted\":%lu,"
             "\"to_printer\":%llu,\"from_printer\":%llu},"
             "\"uptime_s\":%lld,\"version\":\"%s\",\"built\":\"%s %s\"}",
             ssid_esc, ip, wifi_prov_rssi(), p.detected ? "true" : "false", (unsigned long)p.baud, p.handshake,
             p.tx_gpio, p.rx_gpio, p.hs_valid ? "true" : "false", (long long)p.hs_age_ms,
             p.hs.paper_out ? "true" : "false", p.hs.pause ? "true" : "false", p.hs.head_up ? "true" : "false",
             p.hs.ribbon_out ? "true" : "false", p.hs.buffer_full ? "true" : "false",
             p.hs.corrupt_ram ? "true" : "false", p.hs.under_temp ? "true" : "false",
             p.hs.over_temp ? "true" : "false", p.hs.formats_in_buffer, (unsigned long long)p.tx_bytes,
             (unsigned long long)p.rx_bytes, b.active ? "true" : "false", b.client_ip, (unsigned long)b.sessions,
             (unsigned long)b.aborted, (unsigned long long)b.bytes_to_printer,
             (unsigned long long)b.bytes_from_printer, (long long)(esp_timer_get_time() / 1000000), app->version,
             app->date, app->time);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t redirect_home(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t redetect_post(httpd_req_t *req)
{
    printer_link_request_redetect();
    return redirect_home(req);
}

static esp_err_t forget_post(httpd_req_t *req)
{
    settings_clear_wifi();
    ESP_LOGW(TAG, "WiFi credentials cleared from the web page, rebooting");
    httpd_resp_sendstr(req, "WiFi forgotten. Rebooting into setup mode.");
    restart_soon();
    return ESP_OK;
}

/* Captive portal: any unknown URL while the setup AP is up goes to the setup page. */
static esp_err_t not_found(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    if (wifi_prov_ap_active()) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        return httpd_resp_send(req, NULL, 0);
    }
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");
}

void web_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;
    httpd_handle_t server;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server failed to start");
        return;
    }
    const httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get},
        {.uri = "/save", .method = HTTP_POST, .handler = save_post},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get},
        {.uri = "/redetect", .method = HTTP_POST, .handler = redetect_post},
        {.uri = "/forget", .method = HTTP_POST, .handler = forget_post},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found);
    ESP_LOGI(TAG, "HTTP server on port 80");
}
