#pragma once
/* The ESP32-C6's own temperature sensor: the chip's temperature, which runs above the air in the case. */
#include <stdbool.h>

typedef struct {
    bool valid;     /* false until the first reading, or if the sensor would not start */
    float temp_c;
    float max_c;    /* highest since boot */
    bool hot;       /* at or above TEMP_ALARM_WARN_C, until it falls below TEMP_ALARM_CLEAR_C */
} temp_mon_status_t;

void temp_mon_init(void);
/* Call often; it reads the sensor every 30 s and logs a warning when the chip gets too hot. */
void temp_mon_poll(void);
void temp_mon_get_status(temp_mon_status_t *out);
