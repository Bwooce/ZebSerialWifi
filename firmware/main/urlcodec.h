#pragma once
/* application/x-www-form-urlencoded helpers. No ESP-IDF dependencies. */
#include <stdbool.h>
#include <stddef.h>

/*
 * Decode '+' and %XX escapes from in into out (always NUL-terminated).
 * Malformed escapes are copied literally. Returns the decoded length.
 */
size_t url_decode(const char *in, char *out, size_t out_len);

/* Find key in a form body and URL-decode its value into out. */
bool form_get(const char *body, const char *key, char *out, size_t out_len);
