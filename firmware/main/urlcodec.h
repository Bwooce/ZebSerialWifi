#pragma once
/* Form, URL and JSON text helpers for the web server. No ESP-IDF dependencies. */
#include <stdbool.h>
#include <stddef.h>

/*
 * Decode '+' and %XX escapes from in into out (always NUL-terminated).
 * Malformed escapes are copied literally. Returns the decoded length.
 */
size_t url_decode(const char *in, char *out, size_t out_len);

/* Find key in a form body and URL-decode its value into out. */
bool form_get(const char *body, const char *key, char *out, size_t out_len);

/*
 * Escape in as the inside of a JSON string: quote, backslash and control
 * characters, plus < and > so the text can never close a tag. Bytes from
 * 0x80 up pass through. Stops before an escape that would not fit, so out
 * never ends in half an escape. Returns the length written.
 */
size_t json_escape(const char *in, char *out, size_t out_len);

/*
 * True if the host[:port] of url (an Origin or Referer value, such as
 * "http://zebserial.local" or "http://10.0.0.5/settings") equals host
 * (a Host header), ignoring case. "null" and values without a scheme fail.
 */
bool url_origin_matches_host(const char *url, const char *host);
