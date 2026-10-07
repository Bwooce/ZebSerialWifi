#include "urlcodec.h"

#include <string.h>

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Decode at most in_len bytes of in. */
static size_t decode_n(const char *in, size_t in_len, char *out, size_t out_len)
{
    size_t o = 0;
    if (out_len == 0) {
        return 0;
    }
    for (size_t i = 0; i < in_len && in[i] != '\0'; i++) {
        char c = in[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%') {
            int hi = (i + 1 < in_len) ? hex_val(in[i + 1]) : -1;
            int lo = (i + 2 < in_len) ? hex_val(in[i + 2]) : -1;
            if (hi >= 0 && lo >= 0) {
                c = (char)(hi * 16 + lo);
                i += 2;
            }
        }
        if (o + 1 >= out_len) {
            break;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return o;
}

size_t url_decode(const char *in, char *out, size_t out_len)
{
    return decode_n(in, strlen(in), out, out_len);
}

bool form_get(const char *body, const char *key, char *out, size_t out_len)
{
    size_t klen = strlen(key);
    const char *p = body;
    while (*p) {
        const char *amp = strchr(p, '&');
        size_t pair_len = amp ? (size_t)(amp - p) : strlen(p);
        if (pair_len > klen && strncmp(p, key, klen) == 0 && p[klen] == '=') {
            decode_n(p + klen + 1, pair_len - klen - 1, out, out_len);
            return true;
        }
        if (!amp) {
            break;
        }
        p = amp + 1;
    }
    return false;
}
