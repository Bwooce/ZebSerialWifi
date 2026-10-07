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

/* Move the UART to other pins (waits for any print job to finish), then re-detect. */
esp_err_t printer_link_set_pins(int tx_gpio, int rx_gpio);

/* Ask the background task to re-run detection at the next idle moment. */
void printer_link_request_redetect(void);
