#include "printer_link.h"

#include <stdio.h>
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
#define HH_TIMEOUT_MS 3000
#define ETX_BYTE 0x03
#define SCAN_RETRY_MS 10000
#define POLL_INTERVAL_MS 30000
#define POLL_FAILS_BEFORE_RESCAN 3
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
        if (len > 0) {
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
    snprintf(cmd, sizeof(cmd), "! U1 getvar \"%s\"\r\n", name);
    uart_flush_input(PORT);
    send_str(cmd);
    size_t len = collect(buf, sizeof(buf), PROBE_TIMEOUT_MS, sgd_done);
    size_t cmd_len = strlen(cmd) - 2;  /* without the CR LF */
    if (len >= cmd_len && memcmp(buf, cmd, cmd_len) == 0) {
        if (!s_echo) {
            ESP_LOGW(TAG, "the serial line echoes our commands back at %lu baud: loopback plug, not a printer; "
                          "XON/XOFF off until a printer answers", (unsigned long)s_baud);
            s_echo = true;
            uart_set_sw_flow_ctrl(PORT, false, XON_THRESH, XOFF_THRESH);
        }
    }
    return zpl_sgd_value(buf, len, out, out_len);
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
        if (probe_hs()) {
            ESP_LOGI(TAG, "printer answered ~HS at %lu baud", (unsigned long)cand[i]);
            s_probe_sgd = false;
            return true;
        }
        char lang[32];
        if (sgd_get("device.languages", lang, sizeof(lang))) {
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

static bool probe_current(void)
{
    if (s_probe_sgd) {
        char dummy[32];
        return sgd_get("device.languages", dummy, sizeof(dummy));
    }
    return probe_hs();
}

/* Stop reading the ^HH dump once its closing ETX has arrived. */
static bool etx_done(const uint8_t *buf, size_t len)
{
    return memchr(buf, ETX_BYTE, len) != NULL;
}

/*
 * Read the printer's handshake setting: SGD comm.handshake first (newer
 * firmware), then the ^HH configuration dump, which every ZPL printer
 * answers. Returns false if neither gave an answer.
 */
static bool read_handshake(char *out, size_t out_len)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    bool from_hs = s_hs_valid && !s_probe_sgd;  /* only a ~HS from this detection */
    bool dtr = s_hs.handshake_dtr;
    xSemaphoreGive(s_status_lock);
    if (from_hs) {
        /* ~HS string 1 bit a7, documented for every ZPL printer */
        strlcpy(out, dtr ? "DTR" : "XON/XOFF", out_len);
        return true;
    }
    /* "?" is the SGD reply for a variable this firmware does not know */
    if (sgd_get("comm.handshake", out, out_len) && strcmp(out, "?") != 0) {
        return true;
    }
    static uint8_t dump[2048];
    uart_flush_input(PORT);
    send_str("^XA^HH^XZ");
    size_t len = collect(dump, sizeof(dump), HH_TIMEOUT_MS, etx_done);
    if (zpl_hh_handshake(dump, len, out, out_len)) {
        return true;
    }
    ESP_LOGW(TAG, "no handshake in %u bytes of ^HH output", (unsigned)len);
    if (len > 0) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, dump, len < 128 ? len : 128, ESP_LOG_WARN);
    }
    return false;
}

static void publish_handshake(const char *hs)
{
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    strlcpy(s_handshake, hs, sizeof(s_handshake));
    xSemaphoreGive(s_status_lock);
}

/*
 * Make sure the printer uses XON/XOFF and, if a target is set, runs at the
 * target baud. Both are set with one ZPL ^SC ... ^JUS, which saves to the
 * printer's flash, so it is only sent when something needs to change. If
 * the handshake cannot be read at all, ^SC is sent once and remembered in
 * NVS so it is not rewritten on every boot.
 * Returns false if the printer was lost and could not be found again.
 */
static bool configure_printer(void)
{
    char hs[24] = "";
    bool known = read_handshake(hs, sizeof(hs));
    if (known) {
        ESP_LOGI(TAG, "printer handshake: %s", hs);
    }
    bool hs_bad = known ? !zpl_handshake_is_xon(hs) : !settings_get_flag("sc_sent");
    uint32_t target = s_target_baud;
    bool move = target != 0 && s_baud != target && !s_raise_tried;
    publish_handshake(known ? hs : "");
    if (!hs_bad && !move) {
        return true;
    }

    uint32_t old_baud = s_baud;
    uint32_t new_baud = move ? target : s_baud;
    char cmd[48];
    zpl_sc_command(cmd, sizeof(cmd), new_baud);
    if (move) {
        s_raise_tried = true;
    }
    ESP_LOGI(TAG, "configuring printer: %lu baud, XON/XOFF (%s%s%s)", (unsigned long)new_baud,
             move ? "baud change" : "", move && hs_bad ? ", " : "",
             hs_bad ? (known ? "handshake was not XON/XOFF" : "handshake unreadable") : "");
    send_str(cmd);
    uart_wait_tx_done(PORT, pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(1500));
    set_baud(new_baud);

    bool hs_ok = false;
    for (int retry = 0; retry < 3; retry++) {
        if (probe_hs()) {
            s_probe_sgd = false;
            hs_ok = true;
            break;
        }
        char lang[32];
        if (sgd_get("device.languages", lang, sizeof(lang))) {
            s_probe_sgd = true;
            hs_ok = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (!hs_ok && move) {
        ESP_LOGW(TAG, "no reply at %lu baud after ^SC, attempting blind revert to %lu baud",
                 (unsigned long)new_baud, (unsigned long)old_baud);
        zpl_sc_command(cmd, sizeof(cmd), old_baud);
        send_str(cmd);
        uart_wait_tx_done(PORT, pdMS_TO_TICKS(1000));
        vTaskDelay(pdMS_TO_TICKS(1500));
        set_baud(old_baud);
        if (probe_hs()) {
            s_probe_sgd = false;
            ESP_LOGI(TAG, "printer recovered at reverted %lu baud", (unsigned long)old_baud);
            return true;
        }
        char lang[32];
        if (sgd_get("device.languages", lang, sizeof(lang))) {
            s_probe_sgd = true;
            ESP_LOGI(TAG, "printer recovered at reverted %lu baud via SGD", (unsigned long)old_baud);
            return true;
        }
        /* not saved: only a baud the printer has answered at goes to NVS (detect() does that) */
        ESP_LOGW(TAG, "revert probe failed; rescanning");
        if (!scan()) {
            return false;
        }
    } else if (!hs_ok) {
        ESP_LOGW(TAG, "no reply after ^SC at %lu baud, rescanning", (unsigned long)new_baud);
        if (!scan()) {
            return false;
        }
    }

    if (hs_ok) {
        settings_set_flag("sc_sent", true);
    }
    if (read_handshake(hs, sizeof(hs))) {
        ESP_LOGI(TAG, "printer handshake now: %s", hs);
        publish_handshake(hs);
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
    if (!found || !configure_printer()) {
        s_detected = false;
        return false;
    }
    s_detected = true;
    if (s_echo) {
        s_echo = false;
        uart_set_sw_flow_ctrl(PORT, true, XON_THRESH, XOFF_THRESH);
    }
    settings_set_baud(s_baud);
    return true;
}

static void link_task(void *arg)
{
    (void)arg;
    int poll_fails = 0;
    int64_t next_us = 0;
    bool stack_logged = false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(200));
        int64_t now = esp_timer_get_time();
        if (!s_redetect && now < next_us) {
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
            if (probe_current()) {
                poll_fails = 0;
            } else if (++poll_fails >= POLL_FAILS_BEFORE_RESCAN) {
                ESP_LOGW(TAG, "printer stopped answering %s, rescanning",
                         s_probe_sgd ? "SGD" : "~HS");
                s_detected = false;
                poll_fails = 0;
            }
            int64_t finish = esp_timer_get_time();
            next_us = finish + (int64_t)(s_detected ? POLL_INTERVAL_MS : SCAN_RETRY_MS) * 1000;
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
    taskENTER_CRITICAL(&s_count_mux);
    out->tx_bytes = s_tx_bytes;
    out->rx_bytes = s_rx_bytes;
    taskEXIT_CRITICAL(&s_count_mux);
    out->tx_gpio = s_tx_gpio;
    out->rx_gpio = s_rx_gpio;
    out->target_baud = s_target_baud;
    xSemaphoreGive(s_status_lock);
}

void printer_link_set_target_baud(uint32_t baud)
{
    if (baud != s_target_baud) {
        s_target_baud = baud;
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
