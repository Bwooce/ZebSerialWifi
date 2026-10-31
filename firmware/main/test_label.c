#include "test_label.h"
#include "zpl_units.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Rulers stop here, so a long label stays inside TEST_LABEL_ZPL_MAX */
#define RULER_MAX_MM 300
/* The edge scales run from 0 to 4 mm in half mm steps */
#define SCALE_HALF_MM 8

typedef struct {
    char *out;
    size_t cap;
    size_t len;
    bool overflow;
} buf_t;

static void put(buf_t *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void put(buf_t *b, const char *fmt, ...)
{
    if (b->overflow) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b->out + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= b->cap - b->len) {
        b->overflow = true;
        return;
    }
    b->len += (size_t)n;
}

/* Field data without the ZPL prefixes ^ and ~, which would end the field or start a command */
static void put_text(buf_t *b, const char *s)
{
    for (; s != NULL && *s != '\0' && !b->overflow; s++) {
        char c = (*s == '^' || *s == '~' || (unsigned char)*s < 0x20) ? ' ' : *s;
        put(b, "%c", c);
    }
}

/* mm to one decimal place, "101.6" or "100" */
static void fmt_mm(char *out, size_t len, double mm)
{
    int n = snprintf(out, len, "%.1f", mm);
    if (n >= 2 && (size_t)n < len && strcmp(out + n - 2, ".0") == 0) {
        out[n - 2] = '\0';
    }
}

/* Text at x,y with the scalable font, h dots high */
static void put_line(buf_t *b, int x, int y, int h, const char *text)
{
    put(b, "^FO%d,%d^A0N,%d,%d^FD", x, y, h, h);
    put_text(b, text);
    put(b, "^FS");
}

size_t test_label_zpl(const test_label_t *t, char *out, size_t out_len)
{
    const int m = t->dpmm;
    if (out_len == 0 || m <= 0 || t->width < 10 * m || t->length < 10 * m) {
        return 0;
    }
    const int w = t->width;
    const int l = t->length;
    buf_t b = {.out = out, .cap = out_len};
    out[0] = '\0';

    const int edge = m;                      /* border 1 mm in from the edges */
    const int bt = m / 4 > 2 ? m / 4 : 2;    /* border line, about 0.25 mm */
    const int in_l = edge + bt;              /* inside of the border */
    const int in_r = w - edge - bt;
    const int in_b = l - edge - bt;

    /* ^PW and ^LL set this label's size for this format only; nothing is saved */
    put(&b, "^XA^LH0,0^PW%d^LL%d^CI0", w, l);
    put(&b, "^FO%d,%d^GB%d,%d,%d^FS", edge, edge, w - 2 * edge, l - 2 * edge, bt);

    /*
     * Edge scales in the top corners: ticks every 0.5 mm from each side edge
     * in to 4 mm, longer on whole mm, numbered at 2 and 4. Where the label's
     * edge falls on each scale shows how far the print is off centre; ticks
     * that miss the label print on the backing, so both sides can be read.
     * Each tick's outer side is the reading. 3 dots wide or more: thinner
     * lines along the feed break up on some media.
     */
    const int sc_w = bt > 3 ? bt : 3;
    const int sc_long = (3 * m) / 2;
    const int sc_short = (3 * m) / 4;
    const int sc_num = (5 * m) / 4;          /* 1.25 mm digits */
    for (int k = 0; k <= SCALE_HALF_MM; k++) {
        int d = zpl_mm_to_dots(k * 0.5, m);
        int h = k % 2 == 0 ? sc_long : sc_short;
        put(&b, "^FO%d,%d^GB%d,%d,%d^FS", d, in_l, sc_w, h, sc_w);
        put(&b, "^FO%d,%d^GB%d,%d,%d^FS", w - d - sc_w, in_l, sc_w, h, sc_w);
        if (k == SCALE_HALF_MM / 2 || k == SCALE_HALF_MM) {
            char n[4];
            snprintf(n, sizeof(n), "%d", k / 2);
            const int ny = in_l + sc_long + m / 6;
            const int half = (sc_num * 3) / 10;  /* about half a digit's width */
            put_line(&b, d + sc_w / 2 - half, ny, sc_num, n);
            put_line(&b, w - d - sc_w / 2 - half, ny, sc_num, n);
        }
    }
    const int sc_end = zpl_mm_to_dots(SCALE_HALF_MM * 0.5 + 1.0, m);  /* the ruler keeps 1 mm clear */

    /* Rulers: marks every 5 mm measured from the label's own edge, numbered every 10 mm */
    const int tick = m / 6 > 2 ? m / 6 : 2;
    const int num_h = (5 * m) / 2;           /* 2.5 mm digits */
    /* the marks are at true mm (zpl_units.h); sizes elsewhere only need to be roughly right */
    for (int mm = 5; zpl_mm_to_dots(mm, m) < in_r && mm <= RULER_MAX_MM; mm += 5) {
        int x = zpl_mm_to_dots(mm, m);
        if (x - tick / 2 <= in_l || x + tick / 2 >= w - sc_end) {
            continue;
        }
        int len = mm % 10 == 0 ? 3 * m : (3 * m) / 2;
        put(&b, "^FO%d,%d^GB%d,%d,%d^FS", x - tick / 2, in_l, tick, len, tick);
        if (mm % 10 == 0 && x + 3 * m < in_r) {
            char n[8];
            snprintf(n, sizeof(n), "%d", mm);
            put_line(&b, x + m / 3, in_l + len - num_h, num_h, n);
        }
    }
    for (int mm = 5; zpl_mm_to_dots(mm, m) < in_b && mm <= RULER_MAX_MM; mm += 5) {
        int y = zpl_mm_to_dots(mm, m);
        if (y - tick / 2 <= in_l) {
            continue;
        }
        int len = mm % 10 == 0 ? 3 * m : (3 * m) / 2;
        put(&b, "^FO%d,%d^GB%d,%d,%d^FS", in_l, y - tick / 2, len, tick, tick);
        if (mm % 10 == 0 && y + num_h + m / 3 < in_b && mm >= 20) {
            char n[8];
            snprintf(n, sizeof(n), "%d", mm);
            put_line(&b, in_l + len + m / 3, y + m / 3, num_h, n);
        }
    }

    /* Content starts clear of the rulers */
    const int x0 = in_l + 8 * m;           /* clear of the left ruler's numbers */
    int y = in_l + 6 * m;
    const int right = in_r - m;
    const int bottom = in_b - m;
    const int gap = (3 * m) / 2;

    /* Text: title, then the size and resolution */
    const int title_h = 4 * m;
    const int text_h = 3 * m;
    const int dpi = zpl_dpi(m);
    char line[96];
    char wmm[12];
    char lmm[12];
    fmt_mm(wmm, sizeof(wmm), zpl_dots_to_mm(w, m));
    fmt_mm(lmm, sizeof(lmm), zpl_dots_to_mm(l, m));
    if (y + title_h <= bottom) {
        put_line(&b, x0, y, title_h, "ZebSerialWifi test label");
        y += title_h + gap;
    }
    snprintf(line, sizeof(line), "%s%s%s x %s mm", t->model != NULL && t->model[0] != '\0' ? t->model : "",
             t->model != NULL && t->model[0] != '\0' ? ", " : "", wmm, lmm);
    if (y + text_h <= bottom) {
        put_line(&b, x0, y, text_h, line);
        y += text_h + gap / 2;
    }
    snprintf(line, sizeof(line), "%d dpi, %d x %d dots", dpi, w, l);
    if (y + text_h <= bottom) {
        put_line(&b, x0, y, text_h, line);
        y += text_h + gap / 2;
    }
    snprintf(line, sizeof(line), "Bridge %s", t->version != NULL ? t->version : "");
    if (y + text_h <= bottom) {
        put_line(&b, x0, y, text_h, line);
        y += text_h + gap;
    }

    /* Line widths: 1 to 4 dots, vertical then horizontal, 8 mm long */
    const int bar = 8 * m;
    if (y + bar <= bottom && x0 + 4 * 2 * m + 4 * m + bar <= right) {
        int x = x0;
        for (int d = 1; d <= 4; d++) {
            put(&b, "^FO%d,%d^GB%d,%d,%d^FS", x, y, d, bar, d);
            x += 2 * m;
        }
        x += 2 * m;
        for (int d = 1; d <= 4; d++) {
            put(&b, "^FO%d,%d^GB%d,%d,%d^FS", x, y + (d - 1) * 2 * m, bar, d, d);
        }
        if (x + bar + m + 12 * m <= right) {
            put_line(&b, x + bar + m, y + bar / 2 - text_h / 2, text_h, "1-4 dots");
        }
        y += bar + gap;
    }

    /* Solid band across the content width: white streaks are dead printhead dots */
    const int band = 4 * m;
    if (y + band <= bottom && right - x0 > 10 * m) {
        put(&b, "^FO%d,%d^GB%d,%d,%d^FS", x0, y, right - x0, band, band);
        y += band + gap;
    }

    /* A 20 mm circle (round if the dpi is right) and a QR code to the bridge */
    const int circle = zpl_mm_to_dots(20, m);
    const int mag = m / 2 > 2 ? m / 2 : 2;
    if (y + circle <= bottom && x0 + circle <= right) {
        put(&b, "^FO%d,%d^GC%d,%d,B^FS", x0, y, circle, tick);
        int qx = x0 + circle + 3 * m;
        /* a short URL is version 2 or 3: at most 29 modules, plus ^BQ's own 10 dot top offset */
        if (t->url != NULL && t->url[0] != '\0' && qx + 29 * mag <= right && y + 29 * mag + 10 <= bottom) {
            put(&b, "^FO%d,%d^BQN,2,%d^FDQA,", qx, y, mag);
            put_text(&b, t->url);
            put(&b, "^FS");
        }
        y += circle + gap;
    }

    /* Code 128: about 13 mm wide per character at 0.25 mm modules, so only on a wide label */
    const int module = m / 4 > 2 ? m / 4 : 2;
    const int bc_h = 8 * m;
    const int bc_w = (11 * 9 + 35) * module;  /* ZEBSERIAL: start, 9 data, check, stop and quiet zones */
    if (y + bc_h + text_h <= bottom && x0 + bc_w <= right) {
        put(&b, "^FO%d,%d^BY%d^BCN,%d,Y,N,N^FDZEBSERIAL^FS", x0, y, module, bc_h);
    }

    put(&b, "^XZ");
    return b.overflow ? 0 : b.len;
}
