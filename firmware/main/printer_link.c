#include "printer_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "settings.h"

static const char *TAG = "printer";

#define PORT UART_NUM_1
#define RX_BUF 4096
#define TX_BUF 8192
#define PROBE_TIMEOUT_MS 500
/* The GX430t's ^HH dump is 2522 bytes; ZT, ZD and Xi4 dumps are longer. Timeout from hh_timeout_ms(). */
#define HH_BUF 8192
#define HI_TIMEOUT_MS 1000
#define CMD_DRAIN_MS 3000
#define ETX_BYTE 0x03
#define SCAN_RETRY_MS 10000
#define POLL_INTERVAL_MS 30000
/* a printer that sleeps is only asked when someone wants its status, at most this often */
#define SLEEPY_MIN_POLL_MS 30000
/* gap before a second try when a printer that sleeps does not answer: the first command may only wake it */
#define SLEEPY_WAKE_MS 1000
/* Polls that read device.languages again after detection could not; 10 min, then a printer without SGD is left alone */
#define LANG_RETRIES 20
#define POLL_FAILS_BEFORE_RESCAN 3
/* while a found printer stays silent, rescan the bauds this often (in missed polls, SCAN_RETRY_MS apart) */
#define SILENT_RESCAN_EVERY 30
/*
 * The ESP sends XOFF to the printer when its own RX FIFO passes XOFF_THRESH.
 * The driver drains the FIFO at RX_FULL_THRESH, so XOFF only goes out if the
 * ISR falls behind. Received XON/XOFF pause our TX in hardware.
 */
#define RX_FULL_THRESH 64
#define XON_THRESH 32
#define XOFF_THRESH 120
/* Ring buffer item overhead: uart_tx_all queues a uart_tx_data_t item before data */
#define TX_ITEM_OVERHEAD 32

static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_status_lock;
static int s_tx_gpio;
static int s_rx_gpio;
static uint32_t s_baud;
static bool s_detected;
static bool s_hs_valid;
static zpl_hs_t s_hs;
static int64_t s_hs_time_us;
static char s_handshake[16];
static char s_model[32];
/* device.languages: OTHER means the printer would ignore ZPL or print it as text, so none is sent */
static zpl_lang_t s_lang;
static char s_lang_text[24];
/* time of the last reply to a poll or probe; a detected printer may stop answering (paper out, head open) */
static int64_t s_reply_us = -1;
static bool s_answering;
/* found through SGD while ~HS was silent: configure once ~HS answers */
static bool s_config_pending;
/* device.languages reads left to retry from the poll while the language is unknown */
static int s_lang_retries;
/*
 * The printer has an Energy Star or sleep timeout switched on: a poll every
 * POLL_INTERVAL_MS would keep it awake, so it is only asked for its status
 * when a web page or /api/status wants it (printer_link_want_status).
 */
static bool s_sleeps;
static volatile bool s_status_wanted;
/* 64-bit counters are two words on this core: update and read them under s_count_mux */
static portMUX_TYPE s_count_mux = portMUX_INITIALIZER_UNLOCKED;
static uint64_t s_tx_bytes;
static uint64_t s_rx_bytes;
static volatile bool s_redetect;
static bool s_raise_tried;
static bool s_probe_sgd;
/*
 * Set when the line echoes our own commands (a loopback plug). XON/XOFF is
 * then switched off: with the line looped, an XOFF we send comes straight
 * back and pauses our own transmitter for good. A real printer never echoes,
 * so it always gets flow control, detected or not.
 */
static volatile bool s_echo;
static bool s_pin_swap;
/* set once the unparsed ~HS bytes of a loopback have been logged, so a scan does not dump them every time */
static bool s_echo_logged;
/* ^HH output, used under s_lock by read_handshake and refresh_config */
static uint8_t s_hh_buf[HH_BUF];
/* last parsed ^HH configuration, under s_status_lock; s_cfg_time_us < 0 when there is none */
static zpl_hh_t s_cfg;
static int64_t s_cfg_time_us = -1;
static volatile uint32_t s_target_baud = 115200;
/* TX ring free space right after driver install, i.e. what "empty" reads as (less than TX_BUF: item headers) */
static size_t s_tx_free_empty;

static void count_tx(size_t n)
{
    taskENTER_CRITICAL(&s_count_mux);
    s_tx_bytes += n;
    taskEXIT_CRITICAL(&s_count_mux);
}

static void count_rx(size_t n)
{
    taskENTER_CRITICAL(&s_count_mux);
    s_rx_bytes += n;
    taskEXIT_CRITICAL(&s_count_mux);
}

static esp_err_t uart_setup(uint32_t baud)
{
    const uart_config_t cfg = {
        .baud_rate = (int)baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(PORT, RX_BUF, TX_BUF, 0, NULL, 0), TAG, "driver install");
    ESP_RETURN_ON_ERROR(uart_get_tx_buffer_free_size(PORT, &s_tx_free_empty), TAG, "tx free size");
    ESP_RETURN_ON_ERROR(uart_param_config(PORT, &cfg), TAG, "param config");
    ESP_RETURN_ON_ERROR(uart_set_pin(PORT, s_tx_gpio, s_rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG,
                        "set pin");
    /*
     * RX pull-up: idles at mark if the module is unpowered, and is what pulls RX
     * high behind a Schottky level-shift diode (module at 5 V, cathode to its TXD).
     */
    gpio_set_pull_mode(s_rx_gpio, GPIO_PULLUP_ONLY);
    ESP_RETURN_ON_ERROR(uart_set_rx_full_threshold(PORT, RX_FULL_THRESH), TAG, "rx thresh");
    ESP_RETURN_ON_ERROR(uart_set_sw_flow_ctrl(PORT, !s_echo, XON_THRESH, XOFF_THRESH), TAG, "sw flow ctrl");
    return ESP_OK;
}

/*
 * Change baud only once nothing is queued: bytes still in the TX ring or
 * FIFO would go out at the new rate as garbage. If the queue does not drain
 * (the printer holds XOFF), it is discarded instead.
 */
static void set_baud(uint32_t baud)
{
    int64_t deadline = esp_timer_get_time() + 2000 * 1000;
    while (!printer_link_tx_idle() && esp_timer_get_time() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (!printer_link_tx_idle()) {
        ESP_LOGW(TAG, "TX still queued after 2 s at %lu baud, discarding it before moving to %lu",
                 (unsigned long)s_baud, (unsigned long)baud);
        printer_link_abort_tx();
    }
    uart_set_baudrate(PORT, baud);
    s_baud = baud;
    vTaskDelay(pdMS_TO_TICKS(20));
    uart_flush_input(PORT);
}

static void send_str(const char *s)
{
    size_t n = strlen(s);
    uart_write_bytes(PORT, s, n);
    count_tx(n);
}

/* Collect printer output for up to timeout_ms, stopping early once `done` says so. */
static size_t collect(uint8_t *buf, size_t cap, uint32_t timeout_ms, bool (*done)(const uint8_t *, size_t))
{
    size_t len = 0;
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (len < cap && esp_timer_get_time() < deadline) {
        int n = uart_read_bytes(PORT, buf + len, cap - len, pdMS_TO_TICKS(20));
        if (n > 0) {
            len += (size_t)n;
            count_rx((size_t)n);
            if (done != NULL && done(buf, len)) {
                break;
            }
        }
    }
    return len;
}

static bool hs_done(const uint8_t *buf, size_t len)
{
    zpl_hs_t hs;
    return zpl_hs_parse(buf, len, &hs);
}

static bool sgd_done(const uint8_t *buf, size_t len)
{
    char v[32];
    return zpl_sgd_value(buf, len, v, sizeof(v));
}

/* Send ~HS at the current baud and decode the reply. */
static bool probe_hs(void)
{
    uint8_t buf[256];
    uart_flush_input(PORT);
    send_str("~HS");
    size_t len = collect(buf, sizeof(buf), PROBE_TIMEOUT_MS, hs_done);
    zpl_hs_t hs;
    if (!zpl_hs_parse(buf, len, &hs)) {
        if (len > 0 && s_echo) {
            /* a loopback plug sends our own ~HS back on every probe: say so once, not a dump per scan */
            if (!s_echo_logged) {
                s_echo_logged = true;
                ESP_LOGW(TAG, "~HS came back unparsed (%u bytes): the line echoes, not dumping further replies",
                         (unsigned)len);
            }
        } else if (len > 0) {
            /* bytes came back but did not parse: wrong baud, or a reply format we do not expect */
            ESP_LOGW(TAG, "%u bytes at %lu baud did not parse as ~HS:", (unsigned)len, (unsigned long)s_baud);
            ESP_LOG_BUFFER_HEXDUMP(TAG, buf, len < 64 ? len : 64, ESP_LOG_WARN);
        }
        return false;
    }
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_hs = hs;
    s_hs_valid = true;
    s_hs_time_us = esp_timer_get_time();
    xSemaphoreGive(s_status_lock);
    return true;
}

static bool sgd_get(const char *name, char *out, size_t out_len)
{
    char cmd[64];
    uint8_t buf[128];
    /* the leading CR LF ends any partial line (such as a ~HS just sent), so "!" starts a line */
    snprintf(cmd, sizeof(cmd), "\r\n! U1 getvar \"%s\"\r\n", name);
    uart_flush_input(PORT);
    send_str(cmd);
    size_t len = collect(buf, sizeof(buf), PROBE_TIMEOUT_MS, sgd_done);
    const char *bare = cmd + 2;
    size_t cmd_len = strlen(bare) - 2;  /* without either CR LF */
    size_t skip = 0;
    while (skip < len && (buf[skip] == '\r' || buf[skip] == '\n')) {
        skip++;
    }
    if (len - skip >= cmd_len && memcmp(buf + skip, bare, cmd_len) == 0) {
        if (!s_echo) {
            ESP_LOGW(TAG, "the serial line echoes our commands back at %lu baud: loopback plug, not a printer; "
                          "XON/XOFF off until a printer answers", (unsigned long)s_baud);
            s_echo = true;
            uart_set_sw_flow_ctrl(PORT, false, XON_THRESH, XOFF_THRESH);
        }
    }
    bool ok = zpl_sgd_value(buf, len, out, out_len);
    if (!ok && !s_echo) {
        ESP_LOGW(TAG, "SGD %s: no value in %u bytes at %lu baud", name, (unsigned)len, (unsigned long)s_baud);
        if (len > 0) {
            ESP_LOG_BUFFER_HEXDUMP(TAG, buf, len < 64 ? len : 64, ESP_LOG_WARN);
        }
    }
    return ok;
}

/* Baud the bridge uses while the printer is not detected: last known good, else the G-series default. */
static uint32_t fallback_baud(void)
{
    uint32_t saved = settings_get_baud();
    return saved ? saved : PRINTER_DEFAULT_BAUD;
}

/*
 * Try each candidate baud with ~HS (ZPL), then with an SGD getvar, which
 * also answers in EPL mode. On failure the UART goes back to the fallback
 * baud so jobs still go out at the last known good rate.
 */
static bool scan(void)
{
    uint32_t cand[ZPL_BAUD_MAX_CANDIDATES];
    size_t n;
    zpl_baud_candidates(settings_get_baud(), cand, &n);
    for (size_t i = 0; i < n; i++) {
        set_baud(cand[i]);
        if (s_lang != ZPL_LANG_OTHER && probe_hs()) {
            ESP_LOGI(TAG, "printer answered ~HS at %lu baud", (unsigned long)cand[i]);
            s_probe_sgd = false;
            return true;
        }
        char lang[32];
        if (sgd_get("device.languages", lang, sizeof(lang))) {
            /* the first ~HS after this board resets can be lost to line noise: ask once more now a printer is known */
            if (zpl_language_kind(lang) != ZPL_LANG_OTHER && probe_hs()) {
                ESP_LOGI(TAG, "printer answered ~HS at %lu baud on the second try", (unsigned long)cand[i]);
                s_probe_sgd = false;
                return true;
            }
            ESP_LOGW(TAG, "printer answered SGD at %lu baud but not ~HS; device.languages = %s",
                     (unsigned long)cand[i], lang);
            s_probe_sgd = true;
            return true;
        }
        ESP_LOGD(TAG, "no reply at %lu baud", (unsigned long)cand[i]);
    }
    set_baud(fallback_baud());
    ESP_LOGW(TAG, "printer not found at any baud, staying at %lu", (unsigned long)s_baud);
    return false;
}

/*
 * Is the printer there? ~HS first (it also refreshes the status flags), then
 * an SGD getvar: Zebra printers send no ~HS reply while paper or ribbon is
 * out, the head is open or it is too hot, but still answer SGD.
 */
static bool probe_alive(void)
{
    bool ok = false;
    if (s_lang != ZPL_LANG_OTHER && probe_hs()) {
        s_probe_sgd = false;
        ok = true;
    } else {
        char lang[32];
        if (sgd_get("device.languages", lang, sizeof(lang))) {
            s_probe_sgd = true;
            ok = true;
        }
    }
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_answering = ok;
    if (ok) {
        s_reply_us = esp_timer_get_time();
    }
    xSemaphoreGive(s_status_lock);
    return ok;
}

/* Long enough for a full HH_BUF at the current baud (10 bits a byte), plus 2 s for the printer to start. */
static uint32_t hh_timeout_ms(void)
{
    return (uint32_t)((uint64_t)HH_BUF * 10 * 1000 / (s_baud ? s_baud : PRINTER_DEFAULT_BAUD)) + 2000;
}

/* Stop reading the ^HH dump once its closing ETX has arrived. */
static bool etx_done(const uint8_t *buf, size_t len)
{
    return memchr(buf, ETX_BYTE, len) != NULL;
}

/*
 * Read the printer's handshake setting: SGD comm.handshake first (it can
 * say RTS/CTS, which the ~HS bit cannot), then the ~HS a7 bit, then the ^HH
 * configuration dump (ZPL printers only). Returns false if none answered.
 */
static bool read_handshake(char *out, size_t out_len)
{
    char sgd[24];
    bool sgd_ok = sgd_get("comm.handshake", sgd, sizeof(sgd));
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    bool from_hs = s_hs_valid && !s_probe_sgd;  /* only a ~HS from this detection */
    bool dtr = s_hs.handshake_dtr;
    xSemaphoreGive(s_status_lock);
    if (zpl_pick_handshake(sgd_ok ? sgd : NULL, from_hs, dtr, out, out_len)) {
        return true;
    }
    if (s_lang == ZPL_LANG_OTHER) {
        return false;
    }
    uart_flush_input(PORT);
    send_str("^XA^HH^XZ");
    size_t len = collect(s_hh_buf, sizeof(s_hh_buf), hh_timeout_ms(), etx_done);
    if (zpl_hh_handshake(s_hh_buf, len, out, out_len)) {
        return true;
    }
    ESP_LOGW(TAG, "no handshake in %u bytes of ^HH output", (unsigned)len);
    if (len > 0) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, s_hh_buf, len < 128 ? len : 128, ESP_LOG_WARN);
    }
    return false;
}

/* Model name: ~HI for a ZPL printer, else SGD device.product_name. Keeps the previous name if neither answers. */
static void read_model(void)
{
    char model[sizeof(s_model)] = "";
    if (s_lang != ZPL_LANG_OTHER) {
        uint8_t buf[96];
        uart_flush_input(PORT);
        send_str("~HI");
        size_t len = collect(buf, sizeof(buf), HI_TIMEOUT_MS, etx_done);
        if (!zpl_hi_model(buf, len, model, sizeof(model))) {
            ESP_LOGW(TAG, "no model in %u bytes of ~HI reply", (unsigned)len);
            model[0] = '\0';
        }
    }
    if (model[0] == '\0' && (!sgd_get("device.product_name", model, sizeof(model)) || strcmp(model, "?") == 0)) {
        return;
    }
    ESP_LOGI(TAG, "printer model %s", model);
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    strlcpy(s_model, model, sizeof(s_model));
    xSemaphoreGive(s_status_lock);
}

static void publish_handshake(const char *hs)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    strlcpy(s_handshake, hs, sizeof(s_handshake));
    xSemaphoreGive(s_status_lock);
}

/*
 * Send ^SC at `baud` (8N1, XON/XOFF, saved with ^JUS), move the UART there
 * and see whether the printer answers. The printer ignores an ^SC it cannot
 * take (an unsupported baud), so no answer can mean either a printer still
 * at the old baud or one at the new baud that this end cannot hear.
 */
static bool send_sc(uint32_t baud)
{
    char cmd[48];
    zpl_sc_command(cmd, sizeof(cmd), baud);
    send_str(cmd);
    uart_wait_tx_done(PORT, pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(1500));
    set_baud(baud);
    for (int retry = 0; retry < 3; retry++) {
        if (probe_alive()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return false;
}

/*
 * Try to move the printer from s_baud to `to`. On failure, find it again at
 * the old baud first; only if it is silent there too is the revert ^SC sent
 * at the new baud, for a printer that moved but whose replies this end
 * cannot receive. Returns 1 moved, 0 still at the old baud, -1 lost.
 */
static int try_raise(uint32_t to)
{
    uint32_t old_baud = s_baud;
    ESP_LOGI(TAG, "moving printer from %lu to %lu baud", (unsigned long)old_baud, (unsigned long)to);
    if (send_sc(to)) {
        ESP_LOGI(TAG, "printer now at %lu baud", (unsigned long)to);
        return 1;
    }
    set_baud(old_baud);
    if (probe_alive()) {
        ESP_LOGW(TAG, "printer stayed at %lu baud: it does not take %lu", (unsigned long)old_baud, (unsigned long)to);
        return 0;
    }
    ESP_LOGW(TAG, "no reply at %lu or %lu baud, sending the revert at %lu", (unsigned long)to,
             (unsigned long)old_baud, (unsigned long)to);
    set_baud(to);
    if (send_sc(old_baud)) {
        ESP_LOGI(TAG, "printer recovered at reverted %lu baud", (unsigned long)old_baud);
        return 0;
    }
    return -1;
}

/*
 * Make sure the printer uses XON/XOFF alone and, if a target is set, runs at
 * the target baud. The handshake is fixed first with an ^SC at the current
 * baud, so a printer that cannot take the target still gets XON/XOFF. ^SC
 * ends in ^JUS, which writes the printer's flash, so it is only sent when
 * something needs to change. If the handshake cannot be read at all, ^SC
 * is sent once and remembered in NVS so it is not rewritten on every boot.
 * A baud the printer would not take is remembered for its model, and the
 * next rung down is tried instead. Never called for a non-ZPL printer.
 * Returns false if the printer was lost and could not be found again.
 */
static bool configure_printer(void)
{
    char hs[24] = "";
    bool known = read_handshake(hs, sizeof(hs));
    if (known) {
        ESP_LOGI(TAG, "printer handshake: %s", hs);
    }
    publish_handshake(known ? hs : "");
    bool hs_bad = known ? !zpl_handshake_is_xon(hs) : !settings_get_flag("sc_sent");
    if (hs_bad) {
        ESP_LOGI(TAG, "configuring printer: XON/XOFF at %lu baud (%s)", (unsigned long)s_baud,
                 known ? "handshake was not XON/XOFF alone" : "handshake unreadable");
        if (!send_sc(s_baud)) {
            ESP_LOGW(TAG, "no reply after ^SC at %lu baud, rescanning", (unsigned long)s_baud);
            if (!scan()) {
                return false;
            }
        } else {
            settings_set_flag("sc_sent", true);
        }
        if (read_handshake(hs, sizeof(hs))) {
            ESP_LOGI(TAG, "printer handshake now: %s", hs);
            publish_handshake(hs);
        }
    }

    uint32_t target = s_target_baud;
    if (target == 0 || s_baud == target || s_raise_tried) {
        return true;
    }
    s_raise_tried = true;
    char model[sizeof(s_model)];
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    strlcpy(model, s_model, sizeof(model));
    xSemaphoreGive(s_status_lock);
    uint32_t cap = settings_get_baud_cap(model);
    /* a target below the current baud is taken as asked; a raise steps down from the target past known failures */
    uint32_t to = target < s_baud ? target : zpl_next_raise(target, s_baud, cap);
    if (to == 0 && cap != 0) {
        ESP_LOGI(TAG, "staying at %lu baud: %s did not take %lu before", (unsigned long)s_baud, model,
                 (unsigned long)cap);
    }
    while (to != 0) {
        int r = try_raise(to);
        if (r > 0) {
            return true;
        }
        if (r < 0) {
            ESP_LOGW(TAG, "printer lost after ^SC, rescanning");
            return scan();
        }
        settings_set_baud_cap(model, to);
        to = target < s_baud ? 0 : zpl_next_raise(target, s_baud, to);
    }
    return true;
}

/* Swap TX and RX; the caller holds s_lock. */
static void swap_pins(void)
{
    uart_wait_tx_done(PORT, pdMS_TO_TICKS(500));
    int tx = s_rx_gpio;
    int rx = s_tx_gpio;
    gpio_reset_pin(s_tx_gpio);
    gpio_reset_pin(s_rx_gpio);
    if (uart_set_pin(PORT, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "pin swap failed");
        return;
    }
    s_tx_gpio = tx;
    s_rx_gpio = rx;
    gpio_set_pull_mode(s_rx_gpio, GPIO_PULLUP_ONLY);
    uart_flush_input(PORT);
    ESP_LOGI(TAG, "trying swapped pins: TX=GPIO%d RX=GPIO%d", s_tx_gpio, s_rx_gpio);
}

/*
 * Noise on the printer's RX while this board resets (the TX pin floats
 * until the UART starts) can leave the printer inside a ^ format: ~HS then
 * reports a partial format and SGD commands go unanswered until it ends
 * (GX430t, 2026-10-17). With no format queued, ~JA cancels it without
 * printing; ^XZ would end it and print whatever it holds. Called with a ~HS
 * from this detection; no client job is open while detection runs.
 */
static void clear_partial_format(void)
{
    if (!s_hs_valid || s_probe_sgd || !s_hs.partial_format) {
        return;
    }
    if (s_hs.formats_in_buffer != 0) {
        ESP_LOGW(TAG, "the printer has a partial format open and %d formats queued: leaving them",
                 s_hs.formats_in_buffer);
        return;
    }
    send_str("~JA");
    vTaskDelay(pdMS_TO_TICKS(100));
    bool answered = probe_hs();
    ESP_LOGW(TAG, "the printer had a partial format open (line noise at reset?): sent ~JA, %s",
             !answered ? "no ~HS reply after it" : s_hs.partial_format ? "still open" : "cleared");
}

/* Ask SGD for device.languages; a printer without SGD that answered ~HS speaks ZPL. */
static void probe_language(void)
{
    char lang[sizeof(s_lang_text)];
    if (!sgd_get("device.languages", lang, sizeof(lang))) {
        lang[0] = '\0';
    }
    zpl_lang_t kind = zpl_language_kind(lang);
    if (kind == ZPL_LANG_UNKNOWN && !s_probe_sgd) {
        kind = ZPL_LANG_ZPL;
    }
    ESP_LOGI(TAG, "printer language: %s", lang[0] ? lang : "unknown");
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_lang = kind;
    strlcpy(s_lang_text, lang, sizeof(s_lang_text));
    xSemaphoreGive(s_status_lock);
}

/* Does the printer sleep when idle? A printer without the settings answers "?" or nothing. */
static void read_sleep(void)
{
    char v[16];
    bool sleeps = (sgd_get("power.energy_star.enable", v, sizeof(v)) && zpl_sgd_is_on(v)) ||
                  (sgd_get("power.sleep.enable", v, sizeof(v)) && zpl_sgd_is_on(v));
    if (sleeps) {
        ESP_LOGI(TAG, "printer sleeps when idle: status read only when asked for, at most every %d s",
                 SLEEPY_MIN_POLL_MS / 1000);
    } else {
        ESP_LOGI(TAG, "printer does not sleep: status polled every %d s", POLL_INTERVAL_MS / 1000);
    }
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_sleeps = sleeps;
    xSemaphoreGive(s_status_lock);
}

/* Full detection. Caller holds s_lock. */
static bool detect(void)
{
    bool found = scan();
    if (!found && s_pin_swap) {
        swap_pins();
        found = scan();
        if (!found) {
            swap_pins();  /* back to the first guess, which jobs use until a printer answers */
            set_baud(fallback_baud());
        }
    }
    if (found && s_pin_swap) {
        s_pin_swap = false;  /* keep these pins until reboot */
        ESP_LOGI(TAG, "printer found with TX=GPIO%d RX=GPIO%d", s_tx_gpio, s_rx_gpio);
    }
    if (!found) {
        s_detected = false;
        s_lang = ZPL_LANG_UNKNOWN;  /* whatever answers next may speak ZPL: let the scan try ~HS again */
        return false;
    }
    clear_partial_format();
    probe_language();
    s_lang_retries = LANG_RETRIES;
    read_model();
    read_sleep();
    if (s_lang == ZPL_LANG_OTHER) {
        /* EPL, CPCL or line print: ZPL would be ignored or printed, so the port is left as it is */
        ESP_LOGW(TAG, "printer language is %s, not ZPL: passing jobs through without configuring it", s_lang_text);
        s_config_pending = false;
    } else if (s_probe_sgd) {
        /* ~HS silent but SGD answers: most likely a fault (paper out, head open); configure once ~HS answers */
        ESP_LOGW(TAG, "printer answers SGD but not ~HS: configuring it once ~HS answers");
        s_config_pending = true;
    } else {
        s_config_pending = false;
        if (!configure_printer()) {
            s_detected = false;
            return false;
        }
    }
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_answering = true;
    s_reply_us = esp_timer_get_time();
    xSemaphoreGive(s_status_lock);
    s_detected = true;
    if (s_echo) {
        s_echo = false;
        s_echo_logged = false;
        uart_set_sw_flow_ctrl(PORT, true, XON_THRESH, XOFF_THRESH);
    }
    settings_set_baud(s_baud);
    return true;
}

/*
 * A printer busy with a stuck format ignores SGD, so the language read at
 * detection can fail, leaving it unknown until the next reboot (GX430t,
 * 2026-10-17). Read it again from the poll once ~HS shows nothing queued.
 */
static void retry_language(void)
{
    if (s_lang_text[0] != '\0' || s_lang_retries <= 0 || s_lang == ZPL_LANG_OTHER || s_probe_sgd || !s_hs_valid ||
        s_hs.partial_format || s_hs.formats_in_buffer != 0) {
        return;
    }
    s_lang_retries--;
    probe_language();
    if (s_lang_text[0] != '\0') {
        ESP_LOGI(TAG, "printer language read on a later poll");
    }
}

/*
 * Poll a detected printer. A printer that stops answering stays detected:
 * Zebra printers go silent to ~HS with paper out or the head open, and a
 * baud scan then would only send probes at wrong rates. The bauds are
 * scanned after POLL_FAILS_BEFORE_RESCAN misses and then every
 * SILENT_RESCAN_EVERY, in case the printer was reset to another baud.
 */
static void poll_once(int *fails)
{
    bool ok = probe_alive();
    if (!ok && s_sleeps) {
        vTaskDelay(pdMS_TO_TICKS(SLEEPY_WAKE_MS));
        ok = probe_alive();
    }
    if (ok) {
        if (*fails > 0) {
            ESP_LOGI(TAG, "printer answering again");
        }
        *fails = 0;
        if (s_config_pending && !s_probe_sgd) {
            s_config_pending = false;
            ESP_LOGI(TAG, "printer answers ~HS now: configuring it");
            if (!configure_printer()) {
                s_detected = false;
                return;
            }
            read_model();
        }
        retry_language();
        return;
    }
    if (++*fails == 1) {
        ESP_LOGW(TAG, "printer not answering (paper or ribbon out, head open, or switched off?)");
    }
    if (*fails % SILENT_RESCAN_EVERY != POLL_FAILS_BEFORE_RESCAN) {
        return;
    }
    uint32_t was = s_baud;
    ESP_LOGW(TAG, "printer silent for %d polls, scanning the bauds", *fails);
    if (scan()) {
        *fails = 0;
        xSemaphoreTake(s_status_lock, portMAX_DELAY);
        s_answering = true;
        s_reply_us = esp_timer_get_time();
        xSemaphoreGive(s_status_lock);
        if (s_baud != was) {
            ESP_LOGW(TAG, "printer found at %lu baud, was %lu: detecting it again", (unsigned long)s_baud,
                     (unsigned long)was);
            s_redetect = true;
        }
    }
}

static void link_task(void *arg)
{
    (void)arg;
    int poll_fails = 0;
    int64_t next_us = 0;
    int64_t last_poll_us = 0;
    bool stack_logged = false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(200));
        int64_t now = esp_timer_get_time();
        /* a printer that sleeps is not polled on a timer, only when its status is wanted */
        bool due = s_detected && s_sleeps
                       ? s_status_wanted && now - last_poll_us >= (int64_t)SLEEPY_MIN_POLL_MS * 1000
                       : now >= next_us;
        if (!s_redetect && !due) {
            continue;
        }
        if (!printer_link_acquire(0)) {
            continue;  /* a client is printing; try again shortly */
        }
        if (s_redetect || !s_detected) {
            s_redetect = false;
            detect();
            poll_fails = 0;
            int64_t finish = esp_timer_get_time();
            next_us = finish + (int64_t)(s_detected ? POLL_INTERVAL_MS : SCAN_RETRY_MS) * 1000;
            last_poll_us = finish;
            if (!stack_logged) {
                stack_logged = true;
                ESP_LOGI(TAG, "stack: %u bytes never used after the first detect",
                         (unsigned)uxTaskGetStackHighWaterMark(NULL));
            }
        } else if (!printer_link_tx_idle()) {
            /*
             * Earlier output is still queued, most likely behind an XOFF from
             * a printer out of paper. A probe would queue behind it and its
             * missing reply says nothing about the link, so skip this poll
             * without counting it as a miss.
             */
            ESP_LOGD(TAG, "TX queued, skipping the ~HS poll");
            int64_t finish = esp_timer_get_time();
            next_us = finish + (int64_t)SCAN_RETRY_MS * 1000;
        } else {
            poll_once(&poll_fails);
            int64_t finish = esp_timer_get_time();
            next_us = finish + (int64_t)(s_answering ? POLL_INTERVAL_MS : SCAN_RETRY_MS) * 1000;
            last_poll_us = finish;
            s_status_wanted = false;
        }
        printer_link_release();
    }
}

esp_err_t printer_link_init(int tx_gpio, int rx_gpio)
{
    s_tx_gpio = tx_gpio;
    s_rx_gpio = rx_gpio;
    s_lock = xSemaphoreCreateMutex();
    s_status_lock = xSemaphoreCreateMutex();
    s_hs_time_us = -1;
    uint32_t saved = settings_get_baud();
    s_baud = saved ? saved : PRINTER_DEFAULT_BAUD;
    ESP_LOGI(TAG, "UART1 TX=GPIO%d RX=GPIO%d, starting at %lu baud (saved %lu)", tx_gpio, rx_gpio,
             (unsigned long)s_baud, (unsigned long)saved);
    return uart_setup(s_baud);
}

void printer_link_start(void)
{
    xTaskCreate(link_task, "printer_link", 6144, NULL, 5, NULL);
}

bool printer_link_acquire(TickType_t wait)
{
    return xSemaphoreTake(s_lock, wait) == pdTRUE;
}

void printer_link_discard_input(void)
{
    uart_flush_input(PORT);
}

void printer_link_release(void)
{
    xSemaphoreGive(s_lock);
}

int printer_link_write(const uint8_t *data, size_t n, TickType_t wait)
{
    TickType_t start = xTaskGetTickCount();
    for (;;) {
        size_t free_sz = 0;
        uart_get_tx_buffer_free_size(PORT, &free_sz);
        if (free_sz > TX_ITEM_OVERHEAD) {
            size_t room = free_sz - TX_ITEM_OVERHEAD;
            size_t chunk = n < room ? n : room;
            int w = uart_write_bytes(PORT, data, chunk);
            if (w > 0) {
                count_tx((size_t)w);
            }
            return w;
        }
        if (xTaskGetTickCount() - start >= wait) {
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int printer_link_read(uint8_t *data, size_t n, TickType_t wait)
{
    int r = uart_read_bytes(PORT, data, n, wait);
    if (r > 0) {
        count_rx((size_t)r);
    }
    return r < 0 ? 0 : r;
}

bool printer_link_tx_idle(void)
{
    /*
     * uart_wait_tx_done only looks at the hardware FIFO, which can be empty
     * between refills or while XOFF holds the rest in the driver's ring, so
     * check the ring first. Nothing refills the ring from here, so ring empty
     * then FIFO empty means all sent.
     */
    size_t free_sz = 0;
    if (uart_get_tx_buffer_free_size(PORT, &free_sz) != ESP_OK || free_sz < s_tx_free_empty) {
        return false;
    }
    return uart_wait_tx_done(PORT, 0) == ESP_OK;
}

void printer_link_abort_tx(void)
{
    ESP_LOGW(TAG, "discarding queued printer data");
    uart_driver_delete(PORT);
    for (int retry = 0; retry < 3; retry++) {
        esp_err_t err = uart_setup(s_baud);
        if (err == ESP_OK) {
            return;
        }
        ESP_LOGE(TAG, "failed to reinit UART after abort (attempt %d): %s", retry + 1, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void printer_link_get_status(printer_status_t *out)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    out->baud = s_baud;
    out->detected = s_detected;
    out->hs_valid = s_hs_valid;
    out->hs = s_hs;
    out->hs_age_ms = s_hs_time_us < 0 ? -1 : (esp_timer_get_time() - s_hs_time_us) / 1000;
    strlcpy(out->handshake, s_handshake, sizeof(out->handshake));
    strlcpy(out->model, s_model, sizeof(out->model));
    strlcpy(out->language, s_lang_text, sizeof(out->language));
    out->zpl = s_lang != ZPL_LANG_OTHER;
    out->answering = s_detected && s_answering;
    out->sleeps = s_detected && s_sleeps;
    out->reply_age_ms = s_reply_us < 0 ? -1 : (esp_timer_get_time() - s_reply_us) / 1000;
    taskENTER_CRITICAL(&s_count_mux);
    out->tx_bytes = s_tx_bytes;
    out->rx_bytes = s_rx_bytes;
    taskEXIT_CRITICAL(&s_count_mux);
    out->tx_gpio = s_tx_gpio;
    out->rx_gpio = s_rx_gpio;
    out->target_baud = s_target_baud;
    xSemaphoreGive(s_status_lock);
}

void printer_link_want_status(void)
{
    s_status_wanted = true;
}

void printer_link_set_target_baud(uint32_t baud)
{
    if (baud != s_target_baud) {
        s_target_baud = baud;
        settings_set_baud_cap("", 0);  /* a new target gets a fresh try, even at a rate that failed before */
        printer_link_request_redetect();
    }
}

esp_err_t printer_link_set_pins(int tx_gpio, int rx_gpio)
{
    if (tx_gpio == s_tx_gpio && rx_gpio == s_rx_gpio) {
        return ESP_OK;
    }
    if (!printer_link_acquire(pdMS_TO_TICKS(30000))) {
        return ESP_ERR_TIMEOUT;
    }
    uart_wait_tx_done(PORT, pdMS_TO_TICKS(2000));
    /* release the old pins from the UART before claiming the new ones */
    gpio_reset_pin(s_tx_gpio);
    gpio_reset_pin(s_rx_gpio);
    esp_err_t err = uart_set_pin(PORT, tx_gpio, rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err == ESP_OK) {
        s_tx_gpio = tx_gpio;
        s_rx_gpio = rx_gpio;
        /* same as uart_setup: needed behind a level-shift diode, which only ever pulls RX low */
        gpio_set_pull_mode(s_rx_gpio, GPIO_PULLUP_ONLY);
        uart_flush_input(PORT);
        ESP_LOGI(TAG, "UART moved to TX=GPIO%d RX=GPIO%d", tx_gpio, rx_gpio);
    }
    printer_link_release();
    printer_link_request_redetect();
    return err;
}

void printer_link_allow_pin_swap(bool allow)
{
    s_pin_swap = allow;
}

void printer_link_request_redetect(void)
{
    s_raise_tried = false;
    s_redetect = true;
}

/* Wait for everything queued to reach the printer; false if it is still queued at the deadline (XOFF). */
static bool drain_tx(uint32_t timeout_ms)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (!printer_link_tx_idle()) {
        if (esp_timer_get_time() >= deadline) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

esp_err_t printer_link_refresh_config(TickType_t wait)
{
    if (!printer_link_acquire(wait)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = ESP_OK;
    zpl_hh_t *hh = NULL;
    if (!s_detected) {
        err = ESP_ERR_INVALID_STATE;
    } else if (s_lang == ZPL_LANG_OTHER) {
        err = ESP_ERR_NOT_SUPPORTED;
    } else if (!printer_link_tx_idle()) {
        err = ESP_ERR_INVALID_STATE;  /* output still held by XOFF: a reply would queue behind it */
    } else if ((hh = malloc(sizeof(*hh))) == NULL) {
        err = ESP_ERR_NO_MEM;
    } else {
        uart_flush_input(PORT);
        send_str("^XA^HH^XZ");
        size_t len = collect(s_hh_buf, sizeof(s_hh_buf), hh_timeout_ms(), etx_done);
        if (zpl_hh_parse(s_hh_buf, len, hh) && memchr(s_hh_buf, ETX_BYTE, len) != NULL) {
            xSemaphoreTake(s_status_lock, portMAX_DELAY);
            s_cfg = *hh;
            s_cfg_time_us = esp_timer_get_time();
            xSemaphoreGive(s_status_lock);
            ESP_LOGI(TAG, "^HH: %d settings from %u bytes", hh->count, (unsigned)len);
        } else {
            ESP_LOGW(TAG, "^HH: no complete configuration in %u bytes", (unsigned)len);
            err = ESP_FAIL;
        }
    }
    free(hh);
    printer_link_release();
    return err;
}

bool printer_link_get_config(zpl_hh_t *out, int64_t *age_ms)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    bool have = s_cfg_time_us >= 0;
    if (have) {
        *out = s_cfg;
        *age_ms = (esp_timer_get_time() - s_cfg_time_us) / 1000;
    }
    xSemaphoreGive(s_status_lock);
    return have;
}

void printer_link_invalidate_config(void)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_cfg_time_us = -1;
    xSemaphoreGive(s_status_lock);
}

esp_err_t printer_link_send_zpl(const char *zpl, TickType_t wait, uint32_t settle_ms)
{
    if (!printer_link_acquire(wait)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = ESP_OK;
    if (!s_detected) {
        err = ESP_ERR_INVALID_STATE;
    } else if (s_lang == ZPL_LANG_OTHER) {
        err = ESP_ERR_NOT_SUPPORTED;
    } else {
        ESP_LOGI(TAG, "sending %s", zpl);
        send_str(zpl);
        if (!drain_tx(CMD_DRAIN_MS)) {
            /*
             * Never leave it queued behind XOFF: it would go out whenever the
             * printer sends XON, ahead of the next job, or be lost at a baud
             * change. Discard it so the outcome is definite.
             */
            ESP_LOGW(TAG, "printer is holding XOFF, discarding the command");
            printer_link_abort_tx();
            err = ESP_ERR_NOT_FINISHED;
        } else if (settle_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(settle_ms));
        }
    }
    printer_link_release();
    printer_link_invalidate_config();
    return err;
}
