#include "dns_hijack.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

static const char *TAG = "dns";

#define DNS_PORT 53
#define DNS_MAX 512
#define HDR_LEN 12

static volatile bool s_run;
static TaskHandle_t s_task;

/* Length of the QNAME at p, including the terminating zero, or 0 if malformed. */
static size_t qname_len(const uint8_t *p, size_t max)
{
    size_t i = 0;
    while (i < max) {
        uint8_t l = p[i];
        if (l == 0) {
            return i + 1;
        }
        if (l & 0xC0) {
            return 0;  /* compression is not used in questions we answer */
        }
        i += (size_t)l + 1;
    }
    return 0;
}

static void dns_task(void *arg)
{
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (sock < 0 || bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "bind failed");
        if (sock >= 0) {
            close(sock);
        }
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    static const uint8_t ip[4] = {192, 168, 4, 1};
    uint8_t buf[DNS_MAX];

    while (s_run) {
        struct sockaddr_in from;
        socklen_t flen = sizeof(from);
        int n = recvfrom(sock, buf, sizeof(buf) - 16, 0, (struct sockaddr *)&from, &flen);
        if (n < HDR_LEN + 5) {
            continue;
        }
        uint16_t qdcount = (uint16_t)(buf[4] << 8 | buf[5]);
        bool is_query = (buf[2] & 0x80) == 0;
        size_t ql = qname_len(buf + HDR_LEN, (size_t)n - HDR_LEN);
        if (!is_query || qdcount != 1 || ql == 0 || HDR_LEN + ql + 4 > (size_t)n) {
            continue;
        }
        size_t qend = HDR_LEN + ql + 4;
        uint16_t qtype = (uint16_t)(buf[HDR_LEN + ql] << 8 | buf[HDR_LEN + ql + 1]);

        buf[2] = 0x81;  /* response, recursion desired copied as set */
        buf[3] = 0x80;  /* recursion available, no error */
        buf[6] = 0;
        buf[7] = qtype == 1 ? 1 : 0;  /* answer only A queries; others get an empty answer */
        memset(buf + 8, 0, 4);        /* no authority or additional records */
        size_t len = qend;
        if (qtype == 1) {
            static const uint8_t ans[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4};
            memcpy(buf + len, ans, sizeof(ans));
            len += sizeof(ans);
            memcpy(buf + len, ip, 4);
            len += 4;
        }
        sendto(sock, buf, len, 0, (struct sockaddr *)&from, flen);
    }
    close(sock);
    s_task = NULL;
    vTaskDelete(NULL);
}

void dns_hijack_start(void)
{
    s_run = true;  /* also keeps a task that is still running from a recent stop */
    if (s_task != NULL) {
        return;
    }
    xTaskCreate(dns_task, "dns_hijack", 3072, NULL, 4, &s_task);
}

void dns_hijack_stop(void)
{
    s_run = false;  /* the task exits within its 1 s receive timeout */
}
