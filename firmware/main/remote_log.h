#pragma once
/*
 * Copies of the ESP_LOG output sent to an iot_log receiver
 * (github.com/Bwooce/esp-iot-log): multicast once mDNS finds a receiver
 * on the network, and unicast to a fixed address if one is set.
 */
#include <stdbool.h>
#include <stdint.h>

/* Install the log hook. Call once, early, after settings_init(); lines are queued from here on. */
void remote_log_init(void);

/* Start the sender task. Call after mdns_setup(), since iot_log shares the mDNS service. */
void remote_log_start(void);

/* Re-read the remote log settings (after they are saved). */
void remote_log_apply(void);

typedef struct {
    bool enabled;
    bool listener;      /* an iot_log receiver was found by mDNS */
    uint32_t sent;
    uint32_t dropped;   /* not sent: no receiver, queue full, or a send failed */
} remote_log_status_t;

void remote_log_get_status(remote_log_status_t *out);
