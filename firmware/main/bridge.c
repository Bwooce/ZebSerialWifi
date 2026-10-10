#include "bridge.h"

#include <string.h>
#include "board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "printer_link.h"
#include "sdkconfig.h"
#include "settings.h"

static const char *TAG = "bridge";

#define CHUNK 1024
#define BACKLOG 4
#define ACQUIRE_WAIT_MS 15000
/*
 * A printer out of paper or ribbon, or with the head up, holds XOFF until
 * someone fixes it, so a stall alone is no reason to drop the job. The job is
 * abandoned only when the client is gone (TCP keepalive notices a dead peer)
 * or after this safety limit.
 */
#define STALL_LIMIT_MS (30 * 60 * 1000)
#define KEEPALIVE_IDLE_S 10
#define KEEPALIVE_INTVL_S 5
#define KEEPALIVE_COUNT 3

/* Written only by the bridge task; the web server copies it under s_stats_mux (64-bit fields tear otherwise) */
static bridge_stats_t s_stats;
static portMUX_TYPE s_stats_mux = portMUX_INITIALIZER_UNLOCKED;
#define STATS_UPDATE(stmt) do { taskENTER_CRITICAL(&s_stats_mux); stmt; taskEXIT_CRITICAL(&s_stats_mux); } while (0)

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/* Forward whatever the printer has sent to the client. Returns false if the socket failed. */
static bool pump_from_printer(int sock, TickType_t wait)
{
    uint8_t buf[256];
    int n = printer_link_read(buf, sizeof(buf), wait);
    if (n <= 0) {
        return true;
    }
    STATS_UPDATE(s_stats.bytes_from_printer += (uint64_t)n);
    board_led_activity();
    return send(sock, buf, n, 0) == n;
}

/*
 * False once the peer has gone away. A half-closed socket (recv returns 0) is
 * still alive: CUPS shuts down its sending side and then waits for our EOF.
 */
static bool client_alive(int sock)
{
    uint8_t b;
    int r = recv(sock, &b, 1, MSG_PEEK | MSG_DONTWAIT);
    return r >= 0 || errno == EAGAIN || errno == EWOULDBLOCK;
}

/*
 * Write all n bytes to the printer. Returns false if the client vanished
 * while the printer held XOFF, or after STALL_LIMIT_MS without progress.
 */
static bool write_all(int sock, const uint8_t *data, size_t n)
{
    int64_t last_progress = now_ms();
    bool warned = false;
    while (n > 0) {
        int w = printer_link_write(data, n, pdMS_TO_TICKS(100));
        if (w > 0) {
            data += w;
            n -= (size_t)w;
            STATS_UPDATE(s_stats.bytes_to_printer += (uint64_t)w);
            last_progress = now_ms();
            board_led_activity();
        } else {
            int64_t stalled = now_ms() - last_progress;
            if (!warned && stalled > 10000) {
                ESP_LOGW(TAG, "printer paused (XOFF) for 10 s, waiting: check paper, ribbon and head");
                warned = true;
            }
            if (stalled > STALL_LIMIT_MS || !client_alive(sock)) {
                return false;
            }
        }
        /* keep status replies flowing back while we wait */
        if (!pump_from_printer(sock, 0)) {
            return false;
        }
    }
    return true;
}

static void serve(int sock)
{
    uint8_t buf[CHUNK];
    settings_options_t opt;
    settings_get_options(&opt);
    const int64_t idle_limit = (int64_t)opt.idle_timeout_s * 1000;
    int64_t last_activity = now_ms();
    bool client_done = false;

    struct timeval tv = {.tv_sec = 0, .tv_usec = 50 * 1000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    int ka_idle = KEEPALIVE_IDLE_S;
    int ka_intvl = KEEPALIVE_INTVL_S;
    int ka_cnt = KEEPALIVE_COUNT;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &ka_idle, sizeof(ka_idle));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &ka_intvl, sizeof(ka_intvl));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &ka_cnt, sizeof(ka_cnt));

    while (!client_done) {
        int n = recv(sock, buf, sizeof(buf), 0);
        if (n > 0) {
            last_activity = now_ms();
            if (!write_all(sock, buf, (size_t)n)) {
                ESP_LOGW(TAG, "client gone or printer stalled too long, discarding the rest of the job");
                STATS_UPDATE(s_stats.aborted++);
                printer_link_abort_tx();
                return;
            }
            last_activity = now_ms();  /* a long XOFF pause is not client idleness */
        } else if (n == 0) {
            client_done = true;  /* client finished sending (CUPS half-closes and waits for EOF) */
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGI(TAG, "recv error %d", errno);
            return;
        }
        uint64_t before = s_stats.bytes_from_printer;
        if (!pump_from_printer(sock, 0)) {
            return;
        }
        if (s_stats.bytes_from_printer != before) {
            last_activity = now_ms();
        }
        if (!client_done && now_ms() - last_activity > idle_limit) {
            ESP_LOGI(TAG, "client idle for %u s, closing", (unsigned)opt.idle_timeout_s);
            return;
        }
    }

    /* let the queued job reach the printer, then pass back any final replies */
    int64_t drain_start = now_ms();
    while (!printer_link_tx_idle()) {
        if (now_ms() - drain_start > STALL_LIMIT_MS) {
            ESP_LOGW(TAG, "job not drained to the printer: stall limit reached, discarding");
            STATS_UPDATE(s_stats.aborted++);
            printer_link_abort_tx();
            return;
        }
        if (client_alive(sock)) {
            pump_from_printer(sock, pdMS_TO_TICKS(50));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    int64_t tail_start = now_ms();
    while (now_ms() - tail_start < 300) {
        if (!pump_from_printer(sock, pdMS_TO_TICKS(50))) {
            return;
        }
    }
}

static void listen_task(void *arg)
{
    (void)arg;
    /* an IPv6 socket bound to the any-address also accepts IPv4 in lwIP (dual-stack) */
    int ls = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in6 addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(CONFIG_ZEB_TCP_PORT),
        .sin6_addr = IN6ADDR_ANY_INIT,
    };
    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(ls, BACKLOG) != 0) {
        ESP_LOGE(TAG, "cannot listen on port %d: errno %d", CONFIG_ZEB_TCP_PORT, errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "listening on port %d", CONFIG_ZEB_TCP_PORT);

    for (;;) {
        struct sockaddr_storage peer;
        socklen_t plen = sizeof(peer);
        int sock = accept(ls, (struct sockaddr *)&peer, &plen);
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        char client_ip[sizeof(s_stats.client_ip)];
        if (peer.ss_family == AF_INET) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)&peer)->sin_addr, client_ip, sizeof(client_ip));
        } else {
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&peer)->sin6_addr, client_ip, sizeof(client_ip));
        }
        STATS_UPDATE(memcpy(s_stats.client_ip, client_ip, sizeof(client_ip)));
        if (!printer_link_acquire(pdMS_TO_TICKS(ACQUIRE_WAIT_MS))) {
            ESP_LOGW(TAG, "printer link busy, rejecting %s", s_stats.client_ip);
            close(sock);
            continue;
        }
        /* late replies to the link task's probes must not reach this client */
        printer_link_discard_input();
        STATS_UPDATE(s_stats.sessions++; s_stats.active = true);
        uint64_t in0 = s_stats.bytes_to_printer;
        uint64_t out0 = s_stats.bytes_from_printer;
        int64_t t0 = now_ms();
        ESP_LOGI(TAG, "client %s connected", s_stats.client_ip);
        serve(sock);
        shutdown(sock, SHUT_RDWR);
        close(sock);
        STATS_UPDATE(s_stats.active = false);
        printer_link_release();
        ESP_LOGI(TAG, "client %s done: %llu bytes to printer, %llu back, %lld ms", s_stats.client_ip,
                 (unsigned long long)(s_stats.bytes_to_printer - in0),
                 (unsigned long long)(s_stats.bytes_from_printer - out0), (long long)(now_ms() - t0));
    }
}

void bridge_start(void)
{
    xTaskCreate(listen_task, "bridge", 6144, NULL, 6, NULL);
}

void bridge_get_stats(bridge_stats_t *out)
{
    taskENTER_CRITICAL(&s_stats_mux);
    *out = s_stats;
    taskEXIT_CRITICAL(&s_stats_mux);
}
