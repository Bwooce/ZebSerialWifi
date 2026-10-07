#include "zpl_status.h"

#include <stdio.h>
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

static bool flag(const frame_t *f, int idx, bool *out)
{
    if (!all_digits(f->field[idx])) {
        return false;
    }
    *out = f->field[idx][0] != '0' || strlen(f->field[idx]) > 1;
    return true;
}

static bool number(const frame_t *f, int idx, int *out)
{
    if (!all_digits(f->field[idx])) {
        return false;
    }
    int v = 0;
    for (const char *s = f->field[idx]; *s; s++) {
        v = v * 10 + (*s - '0');
    }
    *out = v;
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
    if (!all_digits(s1->field[0]) || !flag(s1, 1, &hs.paper_out) || !flag(s1, 2, &hs.pause) ||
        !all_digits(s1->field[3]) || !number(s1, 4, &hs.formats_in_buffer) || !flag(s1, 5, &hs.buffer_full) ||
        !flag(s1, 9, &hs.corrupt_ram) || !flag(s1, 10, &hs.under_temp) || !flag(s1, 11, &hs.over_temp)) {
        return false;
    }
    /* string 2: mmm,n,o,p,q,r,s,t,uuuuuuuu,v,www */
    if (!all_digits(s2->field[0]) || !flag(s2, 2, &hs.head_up) || !flag(s2, 3, &hs.ribbon_out)) {
        return false;
    }
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

bool zpl_hh_handshake(const uint8_t *buf, size_t len, char *out, size_t out_len)
{
    static const char key[] = "HANDSHAKE";
    const size_t klen = sizeof(key) - 1;
    size_t line = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i < len && buf[i] != '\r' && buf[i] != '\n') {
            continue;
        }
        /* line is buf[line, i) */
        bool has_key = false;
        for (size_t j = line; j + klen <= i; j++) {
            if (memcmp(buf + j, key, klen) == 0) {
                has_key = true;
                break;
            }
        }
        if (has_key) {
            size_t s = line;
            while (s < i && (buf[s] == ' ' || buf[s] == 0x02)) {
                s++;
            }
            size_t e = s;
            while (e < i && !(buf[e] == '.' && e + 1 < i && buf[e + 1] == '.')) {
                e++;
            }
            while (e > s && buf[e - 1] == ' ') {
                e--;
            }
            size_t n = e - s;
            if (n == 0 || n + 1 > out_len) {
                return false;
            }
            memcpy(out, buf + s, n);
            out[n] = '\0';
            return true;
        }
        line = i + 1;
    }
    return false;
}

bool zpl_handshake_is_xon(const char *value)
{
    for (const char *p = value; p[0] && p[1] && p[2]; p++) {
        if ((p[0] == 'x' || p[0] == 'X') && (p[1] == 'o' || p[1] == 'O') && (p[2] == 'n' || p[2] == 'N')) {
            return true;
        }
    }
    return false;
}
