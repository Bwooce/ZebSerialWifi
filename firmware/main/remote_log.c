/*
 * Remote log: every ESP_LOG line, at or above the chosen level, also goes to
 * an iot_log receiver. The vprintf hook runs in whatever task logged,
 * including the Wi-Fi, lwIP and FreeRTOS timer tasks with small stacks, so it
 * only formats into a static buffer and queues the line. One sender task
 * makes every iot_log call: the library is not thread safe, and its mDNS
 * discovery blocks for up to 1 s.
 */
#include "remote_log.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "esp_iot_log.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "log_line.h"
#include "settings.h"
#include "wifi_prov.h"

static const char *TAG = "remote_log";

#define RLOG_LINE_MAX 240           /* longer lines are cut */
#define QUEUE_BYTES 12288      /* holds the boot log, Wi-Fi chatter included, until Wi-Fi is up */
#define SENDER_STACK 6144      /* iot_log needs about 2 KB of buffers on the stack */
#define SENDER_PRIO 2

static vprintf_like_t s_prev_vprintf;
static RingbufHandle_t s_queue;
static TaskHandle_t s_task;
static volatile bool s_on;
static volatile int s_level = 3;
static char s_line[RLOG_LINE_MAX + 1];
static atomic_flag s_line_busy = ATOMIC_FLAG_INIT;
static volatile uint32_t s_hook_dropped;  /* queue full, or two tasks logging at once */
static volatile bool s_running;           /* iot_log initialised */

static int log_hook(const char *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int ret = s_prev_vprintf(fmt, args);
    if (s_on && !xPortInIsrContext() && xTaskGetCurrentTaskHandle() != s_task) {
        if (atomic_flag_test_and_set(&s_line_busy)) {
            s_hook_dropped++;
        } else {
            int n = vsnprintf(s_line, sizeof(s_line), fmt, copy);
            size_t len = n < 0 ? 0 : (size_t)n < sizeof(s_line) ? (size_t)n : sizeof(s_line) - 1;
            len = log_line_trim(s_line, len);
            int level = log_line_level(s_line, len);
            if (len > 0 && (level == 0 ? 3 : level) <= s_level &&
                xRingbufferSend(s_queue, s_line, len, 0) != pdTRUE) {
                s_hook_dropped++;
            }
            atomic_flag_clear(&s_line_busy);
        }
    }
    va_end(copy);
    return ret;
}

void remote_log_init(void)
{
    settings_rlog_t cfg;
    settings_get_rlog(&cfg);
    s_level = cfg.level;
    s_queue = xRingbufferCreate(QUEUE_BYTES, RINGBUF_TYPE_NOSPLIT);
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "no memory for the log queue: remote log off");
        return;
    }
    s_on = cfg.enabled;
    s_prev_vprintf = esp_log_set_vprintf(log_hook);
}

/* (Re)start iot_log from the saved settings. Sender task only. */
static void configure(void)
{
    if (s_running) {
        iot_log_deinit();
        s_running = false;
    }
    settings_rlog_t cfg;
    settings_get_rlog(&cfg);
    s_level = cfg.level;
    s_on = cfg.enabled;
    if (!cfg.enabled) {
        ESP_LOGI(TAG, "off");
        return;
    }
    settings_net_t net;
    settings_get_net(&net);
    char unicast[16] = "";
    if (cfg.unicast_ip != 0) {
        snprintf(unicast, sizeof(unicast), "%lu.%lu.%lu.%lu", (unsigned long)(cfg.unicast_ip >> 24),
                 (unsigned long)((cfg.unicast_ip >> 16) & 0xFF), (unsigned long)((cfg.unicast_ip >> 8) & 0xFF),
                 (unsigned long)(cfg.unicast_ip & 0xFF));
    }
    iot_log_config_t lc = IOT_LOG_CONFIG_DEFAULT();
    lc.device_name = net.hostname;      /* copied by iot_log_init */
    lc.level = (iot_log_level_t)cfg.level;
    lc.serial_mirror = false;           /* the lines came from the serial log already */
    lc.unicast_ip = unicast;
    if (iot_log_init(&lc) != 0) {
        ESP_LOGW(TAG, "iot_log did not start");
        return;
    }
    s_running = true;
}

static void sender_task(void *arg)
{
    (void)arg;
    char line[RLOG_LINE_MAX + 1];
    bool started = false;
    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, 0) != 0 || !started) {
            started = true;
            configure();
        }
        if (!s_on) {
            /* off: empty the queue and wait for a settings change */
            size_t len;
            void *item;
            while ((item = xRingbufferReceive(s_queue, &len, 0)) != NULL) {
                vRingbufferReturnItem(s_queue, item);
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            configure();
            continue;
        }
        if (!s_running || wifi_prov_state() != WIFI_ST_CONNECTED) {
            /* keep the queued lines until there is a network to send them on */
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        iot_log_poll();  /* mDNS discovery, at most every 60 to 300 s */
        size_t len;
        char *item = xRingbufferReceive(s_queue, &len, pdMS_TO_TICKS(1000));
        if (item == NULL) {
            continue;
        }
        if (len > RLOG_LINE_MAX) {
            len = RLOG_LINE_MAX;
        }
        memcpy(line, item, len);
        line[len] = '\0';
        vRingbufferReturnItem(s_queue, item);
        int level = log_line_level(line, len);
        /* iot_log sends its own level and timestamp, so drop the "W (1234) " prefix */
        iot_log((iot_log_level_t)(level == 0 ? 3 : level), "%s", line + log_line_body(line, len));
    }
}

void remote_log_start(void)
{
    if (s_queue == NULL) {
        return;
    }
    xTaskCreate(sender_task, "remote_log", SENDER_STACK, NULL, SENDER_PRIO, &s_task);
}

void remote_log_apply(void)
{
    if (s_task != NULL) {
        xTaskNotifyGive(s_task);
    }
}

void remote_log_get_status(remote_log_status_t *out)
{
    out->enabled = s_on;
    out->listener = s_running && iot_log_listener_active();
    out->sent = iot_log_get_sent_count();
    out->dropped = iot_log_get_dropped_count() + s_hook_dropped;
}
