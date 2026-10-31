#pragma once
/* ZPL for the Printer page's test label. No ESP-IDF dependencies. */
#include <stddef.h>

typedef struct {
    int width;          /* print width in dots (^PW) */
    int length;         /* label length in dots (^LL) */
    int dpmm;           /* dots per mm: 8 (203 dpi), 12 (300 dpi) or 24 (600 dpi) */
    const char *model;  /* printer model, "" if unknown */
    const char *version;/* bridge firmware version */
    const char *url;    /* the bridge's status page, for the QR code */
} test_label_t;

/*
 * A label with a border 1 mm in from the edges, mm rulers along the top and
 * left, 0 to 4 mm scales from each side edge in the top corners (to measure
 * how far the print is off centre), the size and resolution as text, line widths of 1 to 4 dots, a solid
 * band (white streaks show dead printhead dots), a 20 mm circle (an oval
 * means the job and the printer disagree on dpi), a Code 128 barcode and a
 * QR code. Parts that do not fit the label are left out. Nothing is saved in
 * the printer. Returns the length, or 0 if the size is unusable (under
 * 10 mm either way) or out is too small.
 */
size_t test_label_zpl(const test_label_t *t, char *out, size_t out_len);

/* Room test_label_zpl needs for any label up to 1 m long. */
#define TEST_LABEL_ZPL_MAX 8192
