#pragma once
/*
 * Owns UART1 to the printer: baud detection, raising the printer to 115200,
 * XON/XOFF, and exclusive byte I/O for the TCP bridge.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "zpl_status.h"

#define PRINTER_DEFAULT_BAUD 9600   /* G-series factory setting */

typedef struct {
    uint32_t baud;
    bool detected;
    bool hs_valid;
    zpl_hs_t hs;
    int64_t hs_age_ms;        /* ms since the last good ~HS, -1 if never */
    char handshake[16];       /* comm.handshake as last read, "" if unknown */
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    int tx_gpio;
    int rx_gpio;
    uint32_t target_baud;     /* 0 = leave the printer's baud alone */
} printer_status_t;

esp_err_t printer_link_init(int tx_gpio, int rx_gpio);

/* Start the background task that detects the printer and polls ~HS while idle. */
void printer_link_start(void);

/* Exclusive access for the bridge. Detection and polling never run while held. */
bool printer_link_acquire(TickType_t wait);
/* Drop anything still in the receive buffer (late replies to our own probes). Caller holds the link. */
void printer_link_discard_input(void);
void printer_link_release(void);

/* Queue up to n bytes for the printer, waiting up to `wait` for buffer space. Returns bytes queued. */
int printer_link_write(const uint8_t *data, size_t n, TickType_t wait);

/* Read bytes from the printer. Returns bytes read, 0 on timeout. */
int printer_link_read(uint8_t *data, size_t n, TickType_t wait);

/* True when everything queued for the printer has been sent. */
bool printer_link_tx_idle(void);

/*
 * Drop anything still queued for the printer, for when a client is dropped
 * while the printer holds XOFF. Reinstalls the UART driver.
 */
void printer_link_abort_tx(void);

void printer_link_get_status(printer_status_t *out);

/*
 * Baud the printer should be moved to after detection; 0 leaves it where it
 * is. Changing it triggers a re-detect.
 */
void printer_link_set_target_baud(uint32_t baud);

/* Let detection swap TX and RX when no printer answers (pins not fixed or detected). */
void printer_link_allow_pin_swap(bool allow);

/* Move the UART to other pins (waits for any print job to finish), then re-detect. */
esp_err_t printer_link_set_pins(int tx_gpio, int rx_gpio);

/* Ask the background task to re-run detection at the next idle moment. */
void printer_link_request_redetect(void);

/*
 * Read the printer's configuration (^XA^HH^XZ) into the cache. Waits up to
 * `wait` for the link, which a print job holds. ESP_ERR_TIMEOUT if the link
 * stayed busy, ESP_ERR_INVALID_STATE if no printer is detected (or output is
 * held by XOFF), ESP_FAIL if no complete reply came back.
 */
esp_err_t printer_link_refresh_config(TickType_t wait);

/* Copy of the cached configuration and its age; false if none is cached. */
bool printer_link_get_config(zpl_hh_t *out, int64_t *age_ms);

/* Drop the cached configuration so the next reader asks the printer again. */
void printer_link_invalidate_config(void);

/*
 * Send ZPL to the printer with the link held, keep the link settle_ms longer
 * (for commands that make the printer busy or change its port), then drop
 * the cached configuration. Errors as for printer_link_refresh_config; also
 * ESP_ERR_TIMEOUT if the printer held XOFF and the command is still queued.
 */
esp_err_t printer_link_send_zpl(const char *zpl, TickType_t wait, uint32_t settle_ms);
