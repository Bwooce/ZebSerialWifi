#pragma once
/*
 * Pure parsers for Zebra printer replies. No ESP-IDF dependencies so the
 * host tests in test_host/ can build them.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Decoded ~HS host status. Field names follow the ZPL II guide. */
typedef struct {
    bool paper_out;
    bool pause;
    bool buffer_full;
    bool corrupt_ram;
    bool under_temp;
    bool over_temp;
    bool head_up;
    bool ribbon_out;
    int formats_in_buffer;
} zpl_hs_t;

/*
 * Find the three STX...ETX frames of a ~HS reply anywhere in buf and decode
 * them. Returns false unless the frames have the documented field counts
 * (12, 11, 2) and the decoded fields are numeric.
 */
bool zpl_hs_parse(const uint8_t *buf, size_t len, zpl_hs_t *out);

/* Copy the first "..." quoted value of an SGD getvar reply into out. */
bool zpl_sgd_value(const uint8_t *buf, size_t len, char *out, size_t out_len);

#define ZPL_BAUD_MAX_CANDIDATES 6

/*
 * Baud rates to probe, in order: saved (if a standard rate), then 115200,
 * 9600, 19200, 38400, 57600, without duplicates.
 */
void zpl_baud_candidates(uint32_t saved, uint32_t out[ZPL_BAUD_MAX_CANDIDATES], size_t *n);

/*
 * Build the ZPL that sets the serial port to baud, 8N1, XON/XOFF, no Zebra
 * protocol, and saves it (^JUS). Returns the length, or 0 if out is too small.
 */
size_t zpl_sc_command(char *out, size_t out_len, uint32_t baud);

/*
 * Find the handshake line in ^HH (configuration dump) output and copy its
 * value, e.g. "XON/XOFF" from "  XON/XOFF.........HOST HANDSHAKE".
 */
bool zpl_hh_handshake(const uint8_t *buf, size_t len, char *out, size_t out_len);

/* True if a handshake value (SGD or ^HH form) includes XON/XOFF. */
bool zpl_handshake_is_xon(const char *value);
