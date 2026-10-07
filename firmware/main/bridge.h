#pragma once
/*
 * Raw print server: TCP port 9100 to the printer UART, both directions.
 * One client at a time; later clients wait in the listen backlog.
 */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool active;
    char client_ip[46];       /* current or last client, IPv4 or IPv6 */
    uint32_t sessions;
    uint32_t aborted;         /* sessions dropped because the printer stalled */
    uint64_t bytes_to_printer;
    uint64_t bytes_from_printer;
} bridge_stats_t;

void bridge_start(void);
void bridge_get_stats(bridge_stats_t *out);
