#include "urlcodec.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

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

size_t json_escape(const char *in, char *out, size_t out_len)
{
    size_t o = 0;
    if (out_len == 0) {
        return 0;
    }
    for (; *in; in++) {
        unsigned char c = (unsigned char)*in;
        char esc[8];
        const char *rep = NULL;
        switch (c) {
        case '"': rep = "\\\""; break;
        case '\\': rep = "\\\\"; break;
        case '\n': rep = "\\n"; break;
        case '\r': rep = "\\r"; break;
        case '\t': rep = "\\t"; break;
        default:
            if (c < 0x20 || c == '<' || c == '>') {
                snprintf(esc, sizeof(esc), "\\u%04x", c);
                rep = esc;
            }
            break;
        }
        size_t l = rep ? strlen(rep) : 1;
        if (o + l + 1 > out_len) {
            break;
        }
        if (rep) {
            memcpy(out + o, rep, l);
        } else {
            out[o] = (char)c;
        }
        o += l;
    }
    out[o] = '\0';
    return o;
}

bool url_origin_matches_host(const char *url, const char *host)
{
    const char *p = strstr(url, "://");
    if (p == NULL || host[0] == '\0') {
        return false;
    }
    p += 3;
    size_t n = strcspn(p, "/?#");
    return n == strlen(host) && strncasecmp(p, host, n) == 0;
}
