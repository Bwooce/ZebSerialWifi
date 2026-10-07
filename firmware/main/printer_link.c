#include "printer_link.h"

#include <stdio.h>
#include <string.h>
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
#define XOFF_THRESH 110

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
static uint64_t s_tx_bytes;
static uint64_t s_rx_bytes;
static volatile bool s_redetect;
static bool s_raise_tried;

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
    ESP_RETURN_ON_ERROR(uart_param_config(PORT, &cfg), TAG, "param config");
    ESP_RETURN_ON_ERROR(uart_set_pin(PORT, s_tx_gpio, s_rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG,
                        "set pin");
    ESP_RETURN_ON_ERROR(uart_set_rx_full_threshold(PORT, RX_FULL_THRESH), TAG, "rx thresh");
    ESP_RETURN_ON_ERROR(uart_set_sw_flow_ctrl(PORT, true, XON_THRESH, XOFF_THRESH), TAG, "sw flow ctrl");
    return ESP_OK;
}

static void set_baud(uint32_t baud)
{
    uart_wait_tx_done(PORT, pdMS_TO_TICKS(2000));
    uart_set_baudrate(PORT, baud);
    s_baud = baud;
    vTaskDelay(pdMS_TO_TICKS(20));
    uart_flush_input(PORT);
}

static void send_str(const char *s)
{
    size_t n = strlen(s);
    uart_write_bytes(PORT, s, n);
    s_tx_bytes += n;
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
            s_rx_bytes += (uint64_t)n;
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
    return zpl_sgd_value(buf, len, out, out_len);
}

static void sgd_set(const char *name, const char *value)
{
    char cmd[96];
    snprintf(cmd, sizeof(cmd), "! U1 setvar \"%s\" \"%s\"\r\n", name, value);
    send_str(cmd);
    uart_wait_tx_done(PORT, pdMS_TO_TICKS(1000));
}

static bool scan(void)
{
    uint32_t cand[ZPL_BAUD_MAX_CANDIDATES];
    size_t n;
    zpl_baud_candidates(settings_get_baud(), cand, &n);
    for (size_t i = 0; i < n; i++) {
        set_baud(cand[i]);
        if (probe_hs()) {
            ESP_LOGI(TAG, "printer answered ~HS at %lu baud", (unsigned long)cand[i]);
            return true;
        }
        ESP_LOGD(TAG, "no ~HS reply at %lu baud", (unsigned long)cand[i]);
    }
    return false;
}

static void check_handshake(void)
{
    char hs[16] = "";
    if (sgd_get("comm.handshake", hs, sizeof(hs))) {
        ESP_LOGI(TAG, "comm.handshake = %s", hs);
        if (strstr(hs, "xon") == NULL) {
            ESP_LOGW(TAG, "setting comm.handshake to xon/xoff");
            sgd_set("comm.handshake", "xon/xoff");
            if (!sgd_get("comm.handshake", hs, sizeof(hs))) {
                hs[0] = '\0';
            }
        }
    } else {
        ESP_LOGW(TAG, "no reply to getvar comm.handshake");
    }
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    strlcpy(s_handshake, hs, sizeof(s_handshake));
    xSemaphoreGive(s_status_lock);
}

/* Full detection. Caller holds s_lock. */
static bool detect(void)
{
    if (!scan()) {
        s_detected = false;
        return false;
    }
    check_handshake();
    if (s_baud != PRINTER_TARGET_BAUD && !s_raise_tried) {
        s_raise_tried = true;
        uint32_t old = s_baud;
        ESP_LOGI(TAG, "raising printer from %lu to %d baud", (unsigned long)old, PRINTER_TARGET_BAUD);
        sgd_set("comm.baud", "115200");
        vTaskDelay(pdMS_TO_TICKS(1000));
        set_baud(PRINTER_TARGET_BAUD);
        if (probe_hs()) {
            ESP_LOGI(TAG, "printer now at %d baud", PRINTER_TARGET_BAUD);
        } else {
            ESP_LOGW(TAG, "no reply at %d baud after setvar, rescanning; the printer may need a power cycle",
                     PRINTER_TARGET_BAUD);
            if (!scan()) {
                s_detected = false;
                return false;
            }
        }
    }
    s_detected = true;
    settings_set_baud(s_baud);
    return true;
}

static void link_task(void *arg)
{
    (void)arg;
    int poll_fails = 0;
    int64_t next_us = 0;
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
            next_us = now + (int64_t)(s_detected ? POLL_INTERVAL_MS : SCAN_RETRY_MS) * 1000;
        } else {
            if (probe_hs()) {
                poll_fails = 0;
            } else if (++poll_fails >= POLL_FAILS_BEFORE_RESCAN) {
                ESP_LOGW(TAG, "printer stopped answering ~HS, rescanning");
                s_detected = false;
                poll_fails = 0;
            }
            next_us = now + (int64_t)(s_detected ? POLL_INTERVAL_MS : SCAN_RETRY_MS) * 1000;
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
    s_baud = saved ? saved : PRINTER_TARGET_BAUD;
    ESP_LOGI(TAG, "UART1 TX=GPIO%d RX=GPIO%d, starting at %lu baud (saved %lu)", tx_gpio, rx_gpio,
             (unsigned long)s_baud, (unsigned long)saved);
    return uart_setup(s_baud);
}

void printer_link_start(void)
{
    xTaskCreate(link_task, "printer_link", 4096, NULL, 5, NULL);
}

bool printer_link_acquire(TickType_t wait)
{
    return xSemaphoreTake(s_lock, wait) == pdTRUE;
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
        if (free_sz > 0) {
            size_t chunk = n < free_sz ? n : free_sz;
            int w = uart_write_bytes(PORT, data, chunk);
            if (w > 0) {
                s_tx_bytes += (uint64_t)w;
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
        s_rx_bytes += (uint64_t)r;
    }
    return r < 0 ? 0 : r;
}

bool printer_link_tx_idle(void)
{
    return uart_wait_tx_done(PORT, 0) == ESP_OK;
}

void printer_link_abort_tx(void)
{
    ESP_LOGW(TAG, "discarding queued printer data");
    uart_driver_delete(PORT);
    uart_setup(s_baud);
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
    out->tx_bytes = s_tx_bytes;
    out->rx_bytes = s_rx_bytes;
    out->tx_gpio = s_tx_gpio;
    out->rx_gpio = s_rx_gpio;
    xSemaphoreGive(s_status_lock);
}

void printer_link_request_redetect(void)
{
    s_raise_tried = false;
    s_redetect = true;
}
