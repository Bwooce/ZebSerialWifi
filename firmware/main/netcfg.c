#include "netcfg.h"

#include <string.h>

bool netcfg_hostname_valid(const char *name)
{
    size_t n = strlen(name);
    if (n == 0 || n > NETCFG_HOSTNAME_MAX || name[0] == '-' || name[n - 1] == '-') {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

bool netcfg_ipv4_parse(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    for (int part = 0; part < 4; part++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
        if (s[0] == '0' && s[1] >= '0' && s[1] <= '9') {
            return false;
        }
        unsigned octet = 0;
        int digits = 0;
        while (*s >= '0' && *s <= '9') {
            octet = octet * 10 + (unsigned)(*s - '0');
            s++;
            if (++digits > 3 || octet > 255) {
                return false;
            }
        }
        v = (v << 8) | octet;
        if (part < 3) {
            if (*s != '.') {
                return false;
            }
            s++;
        }
    }
    if (*s != '\0') {
        return false;
    }
    *out = v;
    return true;
}

bool netcfg_static_valid(uint32_t ip, uint32_t mask, uint32_t gw)
{
    if (mask == 0 || mask == 0xFFFFFFFFu) {
        return false;
    }
    uint32_t inv = ~mask;
    if ((inv & (inv + 1)) != 0) {
        return false;  /* host bits must be a contiguous run of low bits */
    }
    uint32_t host = ip & inv;
    if (host == 0 || host == inv) {
        return false;
    }
    if (gw != 0 && ((gw & mask) != (ip & mask) || gw == ip)) {
        return false;
    }
    return true;
}
