#include "zpl_status.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define STX 0x02
#define ETX 0x03
#define HS_MAX_FIELDS 12
#define HS_FIELD_LEN 12

typedef struct {
    int count;
    char field[HS_MAX_FIELDS][HS_FIELD_LEN];
} frame_t;

/*
 * Split the bytes between STX and ETX into comma-separated fields. Rejects
 * frames with too many fields, over-long fields, or non-printable bytes.
 */
static bool split_frame(const uint8_t *p, size_t n, frame_t *f)
{
    f->count = 1;
    size_t pos = 0;
    memset(f->field, 0, sizeof(f->field));
    for (size_t i = 0; i < n; i++) {
        uint8_t c = p[i];
        if (c == ',') {
            if (f->count == HS_MAX_FIELDS) {
                return false;
            }
            f->count++;
            pos = 0;
            continue;
        }
        if (c < 0x20 || c > 0x7e || pos + 1 >= HS_FIELD_LEN) {
            return false;
        }
        f->field[f->count - 1][pos++] = (char)c;
    }
    return true;
}

static bool all_digits(const char *s)
{
    if (*s == '\0') {
        return false;
    }
    for (; *s; s++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
    }
    return true;
}

static bool number(const frame_t *f, int idx, int *out)
{
    if (!all_digits(f->field[idx])) {
        return false;
    }
    int v = 0;
    for (const char *s = f->field[idx]; *s; s++) {
        if (v > (INT_MAX - 9) / 10) {
            return false;  /* a field this long is not a ~HS reply */
        }
        v = v * 10 + (*s - '0');
    }
    *out = v;
    return true;
}

/* A flag field is a number: "0" and "00" are false, anything non-zero true. */
static bool flag(const frame_t *f, int idx, bool *out)
{
    int v;
    if (!number(f, idx, &v)) {
        return false;
    }
    *out = v != 0;
    return true;
}

static bool decode(const frame_t fr[3], zpl_hs_t *out)
{
    static const int expected[3] = {12, 11, 2};
    for (int i = 0; i < 3; i++) {
        if (fr[i].count != expected[i]) {
            return false;
        }
    }
    const frame_t *s1 = &fr[0];
    const frame_t *s2 = &fr[1];
    zpl_hs_t hs = {0};
    /* string 1: aaa,b,c,dddd,eee,f,g,h,iii,j,k,l */
    if (!number(s1, 0, &hs.comm_settings) || !flag(s1, 1, &hs.paper_out) || !flag(s1, 2, &hs.pause) ||
        !all_digits(s1->field[3]) || !number(s1, 4, &hs.formats_in_buffer) || !flag(s1, 5, &hs.buffer_full) ||
        !flag(s1, 9, &hs.corrupt_ram) || !flag(s1, 10, &hs.under_temp) || !flag(s1, 11, &hs.over_temp)) {
        return false;
    }
    /* string 2: mmm,n,o,p,q,r,s,t,uuuuuuuu,v,www */
    if (!all_digits(s2->field[0]) || !flag(s2, 2, &hs.head_up) || !flag(s2, 3, &hs.ribbon_out)) {
        return false;
    }
    int partial;
    hs.partial_format = number(s1, 7, &partial) && partial == 1;
    hs.handshake_dtr = (hs.comm_settings & 0x80) != 0;
    *out = hs;
    return true;
}

bool zpl_hs_parse(const uint8_t *buf, size_t len, zpl_hs_t *out)
{
    /*
     * Collect consecutive well-formed frames. A malformed frame (for example
     * leading junk after a stray STX) resets the window, so the reply can
     * start anywhere in the buffer.
     */
    frame_t fr[3];
    int have = 0;
    size_t i = 0;
    while (i < len) {
        if (buf[i] != STX) {
            i++;
            continue;
        }
        size_t start = i + 1;
        size_t end = start;
        while (end < len && buf[end] != ETX && buf[end] != STX) {
            end++;
        }
        if (end >= len) {
            break;
        }
        if (buf[end] == STX) {
            /* frame interrupted by a new STX: discard and restart there */
            have = 0;
            i = end;
            continue;
        }
        frame_t f;
        if (split_frame(buf + start, end - start, &f)) {
            fr[have++] = f;
            if (have == 3) {
                if (decode(fr, out)) {
                    return true;
                }
                /* slide the window by one frame and keep looking */
                fr[0] = fr[1];
                fr[1] = fr[2];
                have = 2;
            }
        } else {
            have = 0;
        }
        i = end + 1;
    }
    return false;
}

bool zpl_sgd_value(const uint8_t *buf, size_t len, char *out, size_t out_len)
{
    /*
     * Skip lines that start with '!': they are our own getvar command coming
     * back (a loopback plug or an echoing device), not a printer reply.
     */
    size_t start = 0;
    for (;;) {
        while (start < len && (buf[start] == '\r' || buf[start] == '\n' || buf[start] == ' ')) {
            start++;
        }
        if (start >= len || buf[start] != '!') {
            break;
        }
        const uint8_t *nl = memchr(buf + start, '\n', len - start);
        if (nl == NULL) {
            return false;
        }
        start = (size_t)(nl - buf) + 1;
    }
    buf += start;
    len -= start;
    const uint8_t *open = memchr(buf, '"', len);
    if (open == NULL) {
        return false;
    }
    size_t rest = len - (size_t)(open + 1 - buf);
    const uint8_t *close = memchr(open + 1, '"', rest);
    if (close == NULL) {
        return false;
    }
    size_t n = (size_t)(close - open - 1);
    if (n + 1 > out_len) {
        return false;
    }
    memcpy(out, open + 1, n);
    out[n] = '\0';
    return true;
}

bool zpl_hi_model(const uint8_t *buf, size_t len, char *out, size_t out_len)
{
    const uint8_t *stx = memchr(buf, STX, len);
    if (stx == NULL) {
        return false;
    }
    size_t rest = len - (size_t)(stx + 1 - buf);
    const uint8_t *etx = memchr(stx + 1, ETX, rest);
    if (etx == NULL) {
        return false;
    }
    const uint8_t *comma = memchr(stx + 1, ',', (size_t)(etx - stx - 1));
    const uint8_t *end = comma != NULL ? comma : etx;
    size_t n = (size_t)(end - stx - 1);
    if (n == 0) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        if (stx[1 + i] < 0x20 || stx[1 + i] > 0x7e) {
            return false;
        }
    }
    /* "-300dpi": the resolution, which is not part of the model name */
    if (n > 3 && memcmp(end - 3, "dpi", 3) == 0) {
        const uint8_t *dash = end - 4;
        while (dash > stx + 1 && *dash >= '0' && *dash <= '9') {
            dash--;
        }
        if (dash > stx + 1 && *dash == '-' && dash < end - 4) {
            n = (size_t)(dash - stx - 1);
        }
    }
    if (n + 1 > out_len) {
        return false;
    }
    memcpy(out, stx + 1, n);
    out[n] = '\0';
    return true;
}

void zpl_baud_candidates(uint32_t saved, uint32_t out[ZPL_BAUD_MAX_CANDIDATES], size_t *n)
{
    static const uint32_t standard[] = {115200, 9600, 19200, 38400, 57600};
    const size_t n_std = sizeof(standard) / sizeof(standard[0]);
    size_t k = 0;
    bool saved_ok = false;
    for (size_t i = 0; i < n_std; i++) {
        if (standard[i] == saved) {
            saved_ok = true;
        }
    }
    if (saved_ok) {
        out[k++] = saved;
    }
    for (size_t i = 0; i < n_std; i++) {
        if (!(saved_ok && standard[i] == saved)) {
            out[k++] = standard[i];
        }
    }
    *n = k;
}

size_t zpl_sc_command(char *out, size_t out_len, uint32_t baud)
{
    int n = snprintf(out, out_len, "^XA^SC%lu,8,N,1,X,N^JUS^XZ", (unsigned long)baud);
    if (n < 0 || (size_t)n >= out_len) {
        if (out_len > 0) {
            out[0] = '\0';
        }
        return 0;
    }
    return (size_t)n;
}

/* Column where the name starts in the GX430t's ^HH layout: two spaces and a 20-character value field. */
#define HH_NAME_COL 22

static bool is_leader(uint8_t c)
{
    return c == ' ' || c == '.';
}

/*
 * Copy p[0, n) into out with surrounding spaces trimmed. A dot leader inside
 * the text ("2104k............R:") becomes one space and a trailing one is
 * dropped, but text that is only dots ("...") is kept as it is.
 */
static void copy_trimmed(const uint8_t *p, size_t n, char *out, size_t out_len)
{
    while (n > 0 && p[0] == ' ') {
        p++;
        n--;
    }
    while (n > 0 && p[n - 1] == ' ') {
        n--;
    }
    size_t content = n;
    while (content > 0 && is_leader(p[content - 1])) {
        content--;
    }
    if (content > 0 && n - content >= 2) {
        n = content;  /* trailing dot leader */
    }
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < out_len; i++) {
        if (p[i] == '.' && i + 2 < n && p[i + 1] == '.' && i > 0 && p[i - 1] != '.') {
            size_t j = i;
            while (j < n && is_leader(p[j])) {
                j++;
            }
            if (j < n && j - i >= 3) {
                out[o++] = ' ';
                i = j - 1;
                continue;
            }
        }
        out[o++] = (char)p[i];
    }
    if (out_len > 0) {
        out[o] = '\0';
    }
}

/*
 * Split one ^HH line (without CR/LF) into name and value. The fixed column
 * layout is tried first because values can hold runs of spaces ("<~>  7EH")
 * and dots ("..."); the dot-leader layout of older firmware is the fallback.
 */
static bool hh_split_line(const uint8_t *p, size_t n, char *name, size_t name_len, char *value,
                          size_t value_len)
{
    while (n > 0 && (p[0] == STX || p[0] == ETX)) {
        p++;
        n--;
    }
    while (n > 0 && (p[n - 1] == STX || p[n - 1] == ETX || p[n - 1] == ' ' || p[n - 1] == '\0')) {
        n--;
    }
    for (size_t i = 0; i < n; i++) {
        if (p[i] < 0x20 || p[i] > 0x7e) {
            return false;  /* wrong baud or binary noise, not a configuration line */
        }
    }
    size_t value_end;
    size_t name_start;
    /* values can be right-aligned ("   2.0IN   50MM"), so the value starts anywhere before the name */
    size_t first = 2;
    while (first < HH_NAME_COL && first < n && p[first] == ' ') {
        first++;
    }
    if (n > HH_NAME_COL && p[0] == ' ' && p[1] == ' ' && first < HH_NAME_COL - 1 && is_leader(p[HH_NAME_COL - 1]) &&
        !is_leader(p[HH_NAME_COL])) {
        value_end = HH_NAME_COL;
        name_start = HH_NAME_COL;
    } else {
        /* the last run of two or more dots that runs straight into the name */
        size_t run_start = 0;
        name_start = 0;
        for (size_t i = 1; i + 1 < n; i++) {
            if (p[i] == '.' && p[i + 1] == '.') {
                size_t j = i;
                while (j < n && p[j] == '.') {
                    j++;
                }
                if (j < n && p[j] != ' ') {
                    run_start = i;
                    name_start = j;
                }
                i = j - 1;
            }
        }
        if (name_start == 0) {
            return false;
        }
        value_end = run_start;
    }
    copy_trimmed(p, value_end, value, value_len);
    copy_trimmed(p + name_start, n - name_start, name, name_len);
    return value[0] != '\0' && name[0] != '\0';
}

/*
 * Call fn for each line of buf, stopping early when it returns true. A last
 * line with neither a line end nor the closing ETX was cut off and is skipped.
 */
static bool for_each_line(const uint8_t *buf, size_t len, bool (*fn)(const uint8_t *, size_t, void *), void *ctx)
{
    size_t line = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i < len && buf[i] != '\r' && buf[i] != '\n') {
            continue;
        }
        if (i == len && memchr(buf + line, ETX, i - line) == NULL) {
            break;
        }
        if (i > line && fn(buf + line, i - line, ctx)) {
            return true;
        }
        line = i + 1;
    }
    return false;
}

static bool hh_add_line(const uint8_t *p, size_t n, void *ctx)
{
    zpl_hh_t *hh = ctx;
    if (hh->count >= ZPL_HH_MAX_PAIRS) {
        return true;
    }
    zpl_hh_pair_t *pair = &hh->pair[hh->count];
    if (hh_split_line(p, n, pair->name, sizeof(pair->name), pair->value, sizeof(pair->value))) {
        hh->count++;
    }
    return false;
}

bool zpl_hh_parse(const uint8_t *buf, size_t len, zpl_hh_t *out)
{
    out->count = 0;
    for_each_line(buf, len, hh_add_line, out);
    return out->count > 0;
}

const char *zpl_hh_get(const zpl_hh_t *hh, const char *name)
{
    for (int i = 0; i < hh->count; i++) {
        if (strcmp(hh->pair[i].name, name) == 0) {
            return hh->pair[i].value;
        }
    }
    return NULL;
}

typedef struct {
    char *out;
    size_t out_len;
    bool found;
} handshake_ctx_t;

static bool hh_handshake_line(const uint8_t *p, size_t n, void *ctx)
{
    handshake_ctx_t *c = ctx;
    char name[ZPL_HH_NAME_LEN];
    char value[ZPL_HH_VALUE_LEN];
    if (!hh_split_line(p, n, name, sizeof(name), value, sizeof(value)) || strstr(name, "HANDSHAKE") == NULL) {
        return false;
    }
    if (strlen(value) + 1 <= c->out_len) {
        strcpy(c->out, value);
        c->found = true;
    }
    return true;
}

bool zpl_hh_handshake(const uint8_t *buf, size_t len, char *out, size_t out_len)
{
    handshake_ctx_t c = {.out = out, .out_len = out_len, .found = false};
    for_each_line(buf, len, hh_handshake_line, &c);
    return c.found;
}

/* Case-insensitive search for an ASCII word in value. */
static bool has_word(const char *value, const char *word)
{
    size_t n = strlen(word);
    for (const char *p = value; *p != '\0'; p++) {
        size_t i = 0;
        while (i < n && p[i] != '\0' && tolower((unsigned char)p[i]) == word[i]) {
            i++;
        }
        if (i == n) {
            return true;
        }
    }
    return false;
}

bool zpl_handshake_is_xon(const char *value)
{
    return has_word(value, "xon") && !has_word(value, "dtr") && !has_word(value, "dsr") &&
           !has_word(value, "rts") && !has_word(value, "cts");
}

bool zpl_pick_handshake(const char *sgd, bool hs_valid, bool hs_dtr, char *out, size_t out_len)
{
    if (sgd != NULL && sgd[0] != '\0' && strcmp(sgd, "?") != 0) {
        strlcpy(out, sgd, out_len);
        return true;
    }
    if (hs_valid) {
        /* ~HS string 1 bit a7: 0 = XON/XOFF, 1 = DTR */
        strlcpy(out, hs_dtr ? "DTR" : "XON/XOFF", out_len);
        return true;
    }
    return false;
}

zpl_lang_t zpl_language_kind(const char *value)
{
    if (value == NULL || value[0] == '\0' || strcmp(value, "?") == 0) {
        return ZPL_LANG_UNKNOWN;
    }
    return has_word(value, "zpl") ? ZPL_LANG_ZPL : ZPL_LANG_OTHER;
}

bool zpl_sgd_is_on(const char *value)
{
    if (value == NULL) {
        return false;
    }
    return tolower((unsigned char)value[0]) == 'o' && tolower((unsigned char)value[1]) == 'n' && value[2] == '\0';
}

uint32_t zpl_next_raise(uint32_t target, uint32_t current, uint32_t cap)
{
    static const uint32_t ladder[] = {115200, 57600, 38400, 19200};
    for (size_t i = 0; i < sizeof(ladder) / sizeof(ladder[0]); i++) {
        uint32_t b = ladder[i];
        if (b <= target && b > current && (cap == 0 || b < cap)) {
            return b;
        }
    }
    return 0;
}

/*
 * Editable settings. Commands and ranges are from the Zebra "ZPL II, ZBI 2,
 * Set-Get-Do, Mirror, WML Programming Guide" (P1134473-11EN Rev A); each
 * entry in zpl_cfg_commands names its command.
 */
static const zpl_cfg_desc_t s_cfg_desc[ZPL_CFG_COUNT] = {
    [ZPL_CFG_DARKNESS] = {"darkness", "DARKNESS", ZPL_CFG_INT, NULL},
    [ZPL_CFG_SPEED] = {"speed", "PRINT SPEED", ZPL_CFG_INT, NULL},
    [ZPL_CFG_METHOD] = {"method", "PRINT METHOD", ZPL_CFG_LETTER, "DT"},
    [ZPL_CFG_MODE] = {"mode", "PRINT MODE", ZPL_CFG_LETTER, "TPRCD"},
    [ZPL_CFG_MEDIA] = {"media", "MEDIA TYPE", ZPL_CFG_LETTER, "YMN"},
    /* G-series: SENSOR SELECT is AUTO after ^MNA (media type found at calibration), MANUAL after any other ^MN */
    [ZPL_CFG_SENSOR] = {"sensor", "SENSOR SELECT", ZPL_CFG_LETTER, "AM"},
    [ZPL_CFG_ZPL_MODE] = {"zpl", "ZPL MODE", ZPL_CFG_INT, NULL},
    [ZPL_CFG_WIDTH] = {"width", "PRINT WIDTH", ZPL_CFG_INT, NULL},
    [ZPL_CFG_LENGTH] = {"length", "LABEL LENGTH", ZPL_CFG_INT, NULL},
    [ZPL_CFG_MAX_LENGTH] = {"max_length", "MAXIMUM LENGTH", ZPL_CFG_INT, NULL},
    [ZPL_CFG_TEAR_OFF] = {"tear_off", "TEAR OFF", ZPL_CFG_INT, NULL},
    [ZPL_CFG_LABEL_TOP] = {"label_top", "LABEL TOP", ZPL_CFG_INT, NULL},
    [ZPL_CFG_LEFT_POS] = {"left_pos", "LEFT POSITION", ZPL_CFG_INT, NULL},
    [ZPL_CFG_POWER_UP] = {"power_up", "MEDIA POWER UP", ZPL_CFG_LETTER, "FCLN"},
    [ZPL_CFG_HEAD_CLOSE] = {"head_close", "HEAD CLOSE", ZPL_CFG_LETTER, "FCLN"},
    [ZPL_CFG_BACKFEED] = {"backfeed", "BACKFEED", ZPL_CFG_PERCENT_OR_LETTER, "NABO"},
    [ZPL_CFG_EARLY_WARNING] = {"early_warning", "EARLY WARNING", ZPL_CFG_LETTER, "ED"},
};

const zpl_cfg_desc_t *zpl_cfg_desc(zpl_cfg_field_t f)
{
    return (unsigned)f < ZPL_CFG_COUNT ? &s_cfg_desc[f] : NULL;
}

typedef struct {
    const char *text;
    int value;
} text_map_t;

/* ^HH spellings of letter settings. Only the ones seen on a G-series label or in its manual. */
static const text_map_t s_method_map[] = {
    {"DIRECT-THERMAL", 'D'}, {"THERMAL-TRANS.", 'T'}, {"THERMAL-TRANS", 'T'}, {NULL, 0}};
static const text_map_t s_mode_map[] = {
    {"TEAR OFF", 'T'}, {"PEEL OFF", 'P'}, {"REWIND", 'R'}, {"CUTTER", 'C'}, {"DELAYED CUT", 'D'}, {NULL, 0}};
static const text_map_t s_media_map[] = {{"GAP/NOTCH", 'Y'}, {"MARK", 'M'}, {"CONTINUOUS", 'N'}, {NULL, 0}};
static const text_map_t s_sensor_map[] = {{"AUTO", 'A'}, {"MANUAL", 'M'}, {NULL, 0}};
static const text_map_t s_zpl_map[] = {{"ZPL II", 2}, {"ZPL", 1}, {NULL, 0}};
static const text_map_t s_feed_map[] = {
    {"FEED", 'F'}, {"CALIBRATION", 'C'}, {"LENGTH", 'L'}, {"NO MOTION", 'N'}, {NULL, 0}};
/* the guide's default N (normal, 90 % after printing) shows as DEFAULT on the label */
static const text_map_t s_backfeed_map[] = {
    {"DEFAULT", 'N'}, {"AFTER", 'A'}, {"BEFORE", 'B'}, {"OFF", 'O'}, {NULL, 0}};
static const text_map_t s_early_map[] = {{"MAINT. OFF", 'D'}, {"MAINT. ON", 'E'}, {NULL, 0}};

static bool map_text(const text_map_t *map, const char *text, int *out)
{
    for (; map->text != NULL; map++) {
        if (strcmp(map->text, text) == 0) {
            *out = map->value;
            return true;
        }
    }
    return false;
}

/*
 * Parse an optionally signed decimal integer at s. Stops at the first
 * non-digit and points *end there. At most 6 digits.
 */
static bool parse_int_prefix(const char *s, int *out, const char **end)
{
    int sign = 1;
    if (*s == '+' || *s == '-') {
        sign = *s == '-' ? -1 : 1;
        s++;
    }
    int v = 0;
    int digits = 0;
    while (*s >= '0' && *s <= '9') {
        if (++digits > 6) {
            return false;
        }
        v = v * 10 + (*s - '0');
        s++;
    }
    if (digits == 0) {
        return false;
    }
    *out = sign * v;
    *end = s;
    return true;
}

static bool parse_int_exact(const char *s, int *out)
{
    const char *end;
    return parse_int_prefix(s, out, &end) && *end == '\0';
}

/* "15.0" -> 150, "39" -> 390; non-negative, one decimal place read, the rest ignored. */
static bool parse_tenths_prefix(const char *s, int *out, const char **end)
{
    int whole;
    if (*s < '0' || *s > '9' || !parse_int_prefix(s, &whole, end)) {
        return false;
    }
    int tenths = 0;
    s = *end;
    if (*s == '.') {
        s++;
        if (*s >= '0' && *s <= '9') {
            tenths = *s - '0';
        }
        while (*s >= '0' && *s <= '9') {
            s++;
        }
    }
    *out = whole * 10 + tenths;
    *end = s;
    return true;
}

static bool decode_field(const zpl_printer_cfg_t *cfg, zpl_cfg_field_t f, const char *text, int *out)
{
    const char *end;
    int t;
    switch (f) {
    case ZPL_CFG_DARKNESS:
        /* "15.0": ~SD only takes whole steps, so round to the nearest */
        if (!parse_tenths_prefix(text, &t, &end) || *end != '\0') {
            return false;
        }
        *out = (t + 5) / 10;
        return true;
    case ZPL_CFG_SPEED:
        /* "4 IPS" */
        return parse_int_prefix(text, out, &end) && strcmp(end, " IPS") == 0;
    case ZPL_CFG_MAX_LENGTH:
        /*
         * "39.0IN   975MM": ^ML takes dot rows. The mm figure is exact (a
         * GX430t shows ^ML6000 at 12 dots/mm as 500MM); the inch figure is
         * the mm divided by 25 (that same setting shows as 20.0IN), so it
         * is not used. The printer converts mm with its rounded dots/mm
         * (12 for 300 dpi), so this stays dpmm, not zpl_units.h.
         */
        if (cfg->dpmm <= 0 || !parse_tenths_prefix(text, &t, &end) || strncmp(end, "IN", 2) != 0) {
            return false;
        }
        end += 2;
        while (*end == ' ') {
            end++;
        }
        if (!parse_int_prefix(end, &t, &end) || strcmp(end, "MM") != 0) {
            return false;
        }
        *out = t * cfg->dpmm;
        return true;
    case ZPL_CFG_WIDTH:
    case ZPL_CFG_LENGTH:
    case ZPL_CFG_TEAR_OFF:
    case ZPL_CFG_LABEL_TOP:
    case ZPL_CFG_LEFT_POS:
        return parse_int_exact(text, out);
    case ZPL_CFG_METHOD:
        return map_text(s_method_map, text, out);
    case ZPL_CFG_MODE:
        return map_text(s_mode_map, text, out);
    case ZPL_CFG_MEDIA:
        return map_text(s_media_map, text, out);
    case ZPL_CFG_SENSOR:
        return map_text(s_sensor_map, text, out);
    case ZPL_CFG_ZPL_MODE:
        return map_text(s_zpl_map, text, out);
    case ZPL_CFG_POWER_UP:
    case ZPL_CFG_HEAD_CLOSE:
        return map_text(s_feed_map, text, out);
    case ZPL_CFG_BACKFEED:
        /* a percentage shows as "60%" */
        if (parse_int_prefix(text, out, &end) && strcmp(end, "%") == 0) {
            return true;
        }
        return map_text(s_backfeed_map, text, out);
    case ZPL_CFG_EARLY_WARNING:
        return map_text(s_early_map, text, out);
    default:
        return false;
    }
}

void zpl_cfg_from_hh(const zpl_hh_t *hh, zpl_printer_cfg_t *out)
{
    memset(out, 0, sizeof(*out));
    /* "1280 12/MM FULL": printhead width in dots, then dots per mm */
    const char *res = zpl_hh_get(hh, "RESOLUTION");
    const char *end;
    int dots;
    int dpmm;
    if (res != NULL && parse_int_prefix(res, &dots, &end) && *end == ' ' && parse_int_prefix(end + 1, &dpmm, &end) &&
        strncmp(end, "/MM", 3) == 0 && dots > 0 && dpmm > 0) {
        out->head_dots = dots;
        out->dpmm = dpmm;
    }
    for (int f = 0; f < ZPL_CFG_COUNT; f++) {
        const char *text = zpl_hh_get(hh, s_cfg_desc[f].hh_name);
        int v;
        if (text != NULL && decode_field(out, (zpl_cfg_field_t)f, text, &v) &&
            zpl_cfg_value_valid(out, (zpl_cfg_field_t)f, v)) {
            out->v[f] = v;
            out->known |= 1u << f;
        }
    }
}

void zpl_cfg_range(const zpl_printer_cfg_t *cfg, zpl_cfg_field_t f, int *min, int *max)
{
    *min = 0;
    *max = 0;
    switch (f) {
    case ZPL_CFG_DARKNESS:
        *min = 0;   /* ~SD##: 00 to 30 */
        *max = 30;
        break;
    case ZPL_CFG_SPEED:
        /*
         * ^PR p: 1 (600 dpi only) to 14, and the printer limits it to what it
         * can do. Zebra's GX430t (12 dots/mm) specification gives 4 ips; the
         * 8 dots/mm G-series models reach 5 or 6.
         */
        *min = 2;
        *max = cfg->dpmm == 12 ? 4 : 6;
        break;
    case ZPL_CFG_ZPL_MODE:
        *min = 1;   /* ^SZ: 1 = ZPL, 2 = ZPL II */
        *max = 2;
        break;
    case ZPL_CFG_WIDTH:
        *min = 2;   /* ^PW: 2 to the width of the label (printhead width when known) */
        *max = cfg->head_dots > 0 ? cfg->head_dots : 32000;
        break;
    case ZPL_CFG_LENGTH:
        *min = 1;   /* ^LL: 1 to 32000 */
        *max = 32000;
        break;
    case ZPL_CFG_MAX_LENGTH:
        /* ^ML: the dpi times two, up to the maximum label length (^LL's 32000 here) */
        *min = cfg->dpmm > 0 ? cfg->dpmm * 25 * 2 : 2;
        *max = 32000;
        break;
    case ZPL_CFG_TEAR_OFF:
    case ZPL_CFG_LABEL_TOP:
        *min = -120;    /* ~TA###, ^LTx: -120 to 120 */
        *max = 120;
        break;
    case ZPL_CFG_LEFT_POS:
        *min = -9999;   /* ^LSa: -9999 to 9999 */
        *max = 9999;
        break;
    case ZPL_CFG_BACKFEED:
        *min = 10;      /* ~JS percentage: 10 to 90 in steps of 10 */
        *max = 90;
        break;
    default:
        break;
    }
}

/*
 * True if v is one of the field's letters. Percentages are told apart from
 * letters this way, never by "is it in A..Z": 70, 80 and 90 are 'F', 'P'
 * and 'Z', none of which ~JS takes as a letter.
 */
static bool is_letter_value(const zpl_cfg_desc_t *d, int v)
{
    return d->letters != NULL && v > 0 && v < 128 && strchr(d->letters, v) != NULL;
}

bool zpl_cfg_value_valid(const zpl_printer_cfg_t *cfg, zpl_cfg_field_t f, int v)
{
    const zpl_cfg_desc_t *d = zpl_cfg_desc(f);
    if (d == NULL) {
        return false;
    }
    int min;
    int max;
    zpl_cfg_range(cfg, f, &min, &max);
    switch (d->kind) {
    case ZPL_CFG_INT:
        return v >= min && v <= max;
    case ZPL_CFG_PERCENT_OR_LETTER:
        return is_letter_value(d, v) || (v >= min && v <= max && v % 10 == 0);
    case ZPL_CFG_LETTER:
        return is_letter_value(d, v);
    default:
        return false;
    }
}

bool zpl_cfg_parse(const zpl_printer_cfg_t *cfg, zpl_cfg_field_t f, const char *text, int *out)
{
    const zpl_cfg_desc_t *d = zpl_cfg_desc(f);
    if (d == NULL) {
        return false;
    }
    int v;
    if (d->kind != ZPL_CFG_INT && text[0] != '\0' && text[1] == '\0' && is_letter_value(d, (unsigned char)text[0])) {
        v = (unsigned char)text[0];
    } else if (d->kind == ZPL_CFG_LETTER || !parse_int_exact(text, &v)) {
        return false;
    }
    if (!zpl_cfg_value_valid(cfg, f, v)) {
        return false;
    }
    *out = v;
    return true;
}

void zpl_cfg_format(zpl_cfg_field_t f, int v, char *out, size_t out_len)
{
    const zpl_cfg_desc_t *d = zpl_cfg_desc(f);
    if (d != NULL && is_letter_value(d, v)) {
        snprintf(out, out_len, "%c", v);
    } else {
        snprintf(out, out_len, "%d", v);
    }
}

typedef struct {
    char *out;
    size_t cap;
    size_t len;
    bool ok;
} appender_t;

static void append(appender_t *a, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void append(appender_t *a, const char *fmt, ...)
{
    if (!a->ok) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(a->out + a->len, a->cap - a->len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= a->cap - a->len) {
        a->ok = false;
        return;
    }
    a->len += (size_t)n;
}

static bool has(uint32_t mask, zpl_cfg_field_t f)
{
    return (mask & (1u << f)) != 0;
}

size_t zpl_cfg_commands(const zpl_printer_cfg_t *want, uint32_t changed, char *out, size_t out_len)
{
    if (out_len == 0) {
        return 0;
    }
    out[0] = '\0';
    const uint32_t all = (1u << ZPL_CFG_COUNT) - 1;
    if (changed == 0 || (changed & ~all) != 0 || (changed & ~want->known) != 0) {
        return 0;
    }
    for (int f = 0; f < ZPL_CFG_COUNT; f++) {
        if (has(changed, (zpl_cfg_field_t)f) && !zpl_cfg_value_valid(want, (zpl_cfg_field_t)f, want->v[f])) {
            return 0;
        }
    }
    /* ^MF sets both actions at once, so both have to be known */
    bool mf = has(changed, ZPL_CFG_POWER_UP) || has(changed, ZPL_CFG_HEAD_CLOSE);
    if (mf && !(has(want->known, ZPL_CFG_POWER_UP) && has(want->known, ZPL_CFG_HEAD_CLOSE))) {
        return 0;
    }
    const int *v = want->v;
    appender_t a = {.out = out, .cap = out_len, .len = 0, .ok = true};

    /* ~ commands act as soon as they arrive, so they go on their own ahead of the format */
    if (has(changed, ZPL_CFG_DARKNESS)) {
        append(&a, "~SD%02d", v[ZPL_CFG_DARKNESS]);           /* ~SD## Set Darkness, 00 to 30 */
    }
    if (has(changed, ZPL_CFG_TEAR_OFF)) {
        /* ~TA### Tear-off Adjust: three characters or the command is ignored */
        int t = v[ZPL_CFG_TEAR_OFF];
        if (t < 0) {
            append(&a, "~TA-%02d", -t);
        } else {
            append(&a, "~TA%03d", t);
        }
    }
    if (has(changed, ZPL_CFG_BACKFEED)) {
        /* ~JSb Change Backfeed Sequence: A, B, N, O or 10 to 90 */
        int b = v[ZPL_CFG_BACKFEED];
        if (is_letter_value(&s_cfg_desc[ZPL_CFG_BACKFEED], b)) {
            append(&a, "~JS%c", b);
        } else {
            append(&a, "~JS%d", b);
        }
    }

    uint32_t caret = changed & ~((1u << ZPL_CFG_DARKNESS) | (1u << ZPL_CFG_TEAR_OFF) | (1u << ZPL_CFG_BACKFEED));
    if (caret != 0) {
        append(&a, "^XA");
        if (has(changed, ZPL_CFG_SPEED)) {
            append(&a, "^PR%d", v[ZPL_CFG_SPEED]);               /* ^PRp Print Rate */
        }
        if (has(changed, ZPL_CFG_METHOD)) {
            append(&a, "^MT%c", v[ZPL_CFG_METHOD]);              /* ^MTa Media Type: T or D */
        }
        if (has(changed, ZPL_CFG_MODE)) {
            append(&a, "^MM%c", v[ZPL_CFG_MODE]);                /* ^MMa Print Mode */
        }
        /*
         * ^MNa Media Tracking. The GK420/GX420 user guide's cross-reference
         * gives SENSOR SELECT as ^MNA (auto-detect), so the two settings are
         * one command: A finds the media type at calibration, a media type
         * letter fixes it (and SENSOR SELECT reads MANUAL).
         */
        if (has(changed, ZPL_CFG_SENSOR) && v[ZPL_CFG_SENSOR] == 'A') {
            append(&a, "^MNA");
        } else if (has(changed, ZPL_CFG_MEDIA) || has(changed, ZPL_CFG_SENSOR)) {
            append(&a, "^MN%c", v[ZPL_CFG_MEDIA]);
        }
        if (has(changed, ZPL_CFG_WIDTH)) {
            append(&a, "^PW%d", v[ZPL_CFG_WIDTH]);               /* ^PWa Print Width */
        }
        if (has(changed, ZPL_CFG_LENGTH)) {
            append(&a, "^LL%d", v[ZPL_CFG_LENGTH]);              /* ^LLy Label Length */
        }
        if (has(changed, ZPL_CFG_MAX_LENGTH)) {
            append(&a, "^ML%d", v[ZPL_CFG_MAX_LENGTH]);          /* ^MLa Maximum Label Length */
        }
        if (has(changed, ZPL_CFG_LABEL_TOP)) {
            append(&a, "^LT%d", v[ZPL_CFG_LABEL_TOP]);           /* ^LTx Label Top */
        }
        if (has(changed, ZPL_CFG_LEFT_POS)) {
            append(&a, "^LS%d", v[ZPL_CFG_LEFT_POS]);            /* ^LSa Label Shift */
        }
        if (mf) {
            /* ^MFp,h Media Feed at power-up and after closing the head */
            append(&a, "^MF%c,%c", v[ZPL_CFG_POWER_UP], v[ZPL_CFG_HEAD_CLOSE]);
        }
        if (has(changed, ZPL_CFG_EARLY_WARNING)) {
            /* ^JH Early Warning Settings: G-series takes only f, early warning maintenance */
            append(&a, "^JH,,,,,%c", v[ZPL_CFG_EARLY_WARNING]);
        }
        if (has(changed, ZPL_CFG_ZPL_MODE)) {
            append(&a, "^SZ%d", v[ZPL_CFG_ZPL_MODE]);            /* ^SZa Set ZPL Mode, last in the format */
        }
        append(&a, "^XZ");
    }
    append(&a, "^XA^JUS^XZ");                                     /* ^JUS Configuration Update: save */
    if (!a.ok) {
        out[0] = '\0';
        return 0;
    }
    return a.len;
}

const char *zpl_action_command(const char *action)
{
    static const struct {
        const char *name;
        const char *zpl;
    } actions[] = {
        {"calibrate", "~JC"},                   /* ~JC Set Media Sensor Calibration */
        {"config_label", "~WC"},                /* ~WC Print Configuration Label */
        {"feed", "~PH"},                        /* ~PH Slew to Home Position: feeds one blank label */
        {"reset_counter1", "~RO1"},             /* ~ROc Reset Advanced Counters: 1 */
        {"reset_counter2", "~RO2"},             /* ~ROc: 2 */
        {"reset_head_cleaned", "~ROC"},         /* ~ROc: C, head cleaned counter (G-series supported) */
        {"reload_saved", "^XA^JUR^XZ"},         /* ^JUR recall the last saved settings */
        /* ^JUF reloads factory settings but they are lost at power-off unless saved, hence ^JUS */
        {"factory_defaults", "^XA^JUF^JUS^XZ"},
    };
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
        if (strcmp(actions[i].name, action) == 0) {
            return actions[i].zpl;
        }
    }
    return NULL;
}
