#pragma once
/* Validation for the network settings form. No ESP-IDF dependencies. */
#include <stdbool.h>
#include <stdint.h>

#define NETCFG_HOSTNAME_MAX 32

/* One DNS label: 1..32 of [a-z0-9-], not starting or ending with '-'. Upper case is rejected. */
bool netcfg_hostname_valid(const char *name);

/* Strict dotted-quad parse into host byte order (a.b.c.d -> 0xAABBCCDD). */
bool netcfg_ipv4_parse(const char *s, uint32_t *out);

/*
 * Static address sanity: mask is contiguous and not /0 or /32, ip is not the
 * network or broadcast address, and gw (if non-zero) is in the same subnet
 * and differs from ip.
 */
bool netcfg_static_valid(uint32_t ip, uint32_t mask, uint32_t gw);
