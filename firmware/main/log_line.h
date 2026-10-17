#pragma once
/* Parsing of formatted ESP_LOG lines for the remote log. No ESP-IDF dependencies. */
#include <stddef.h>

/*
 * Level of a formatted log line ("W (1234) tag: text"): 1 error, 2 warning,
 * 3 info, 4 debug, 5 verbose (the iot_log levels), or 0 when the line does
 * not start with a level letter and timestamp (a raw printf, or a colour code).
 */
int log_line_level(const char *line, size_t len);

/*
 * Offset of the text after the level letter and timestamp ("W (1234) "), which
 * the remote log already carries in its own header; 0 if the line has no such
 * prefix.
 */
size_t log_line_body(const char *line, size_t len);

/* Length of the line without trailing CR and LF. */
size_t log_line_trim(const char *line, size_t len);
