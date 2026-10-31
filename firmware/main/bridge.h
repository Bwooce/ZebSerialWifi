#pragma once
/*
 * Raw print server: TCP port 9100 to the printer UART, both directions.
 * One client at a time; later clients wait in the listen backlog.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool active;
    char client_ip[46];       /* current or last client, IPv4 or IPv6 */
    uint32_t sessions;
    uint32_t aborted;         /* sessions dropped because the printer stalled */
    uint64_t bytes_to_printer;
    uint64_t bytes_from_printer;
} bridge_stats_t;

void bridge_start(void);

/*
 * Reads the next part of a job into buf: bytes read, 0 at the end of the
 * job, or below 0 if the sender went away.
 */
typedef int (*bridge_read_fn)(void *ctx, uint8_t *buf, size_t n);

/*
 * Send one job that arrives some other way than port 9100 (an HTTP POST to
 * /pstprnt). Takes the printer link like a 9100 client, counts in the same
 * statistics, and drops whatever the printer says back. The caller's task
 * waits, so a printer that holds XOFF longer than stall_ms has the rest of
 * the job discarded. ESP_ERR_TIMEOUT if the link stayed busy or the printer
 * stalled, ESP_FAIL if the sender went away (what had arrived still prints).
 */
esp_err_t bridge_send_job(const char *client_ip, bridge_read_fn read, void *ctx, uint32_t stall_ms, size_t *sent);
void bridge_get_stats(bridge_stats_t *out);
