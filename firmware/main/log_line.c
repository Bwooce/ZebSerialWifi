#include "log_line.h"

int log_line_level(const char *line, size_t len)
{
    /* the letter, a space and the opening parenthesis of the timestamp */
    if (len < 3 || line[1] != ' ' || line[2] != '(') {
        return 0;
    }
    switch (line[0]) {
    case 'E':
        return 1;
    case 'W':
        return 2;
    case 'I':
        return 3;
    case 'D':
        return 4;
    case 'V':
        return 5;
    default:
        return 0;
    }
}

size_t log_line_body(const char *line, size_t len)
{
    if (log_line_level(line, len) == 0) {
        return 0;
    }
    size_t i = 3;
    while (i < len && line[i] >= '0' && line[i] <= '9') {
        i++;
    }
    if (i == 3 || i + 1 >= len || line[i] != ')' || line[i + 1] != ' ') {
        return 0;
    }
    return i + 2;
}

size_t log_line_trim(const char *line, size_t len)
{
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        len--;
    }
    return len;
}
