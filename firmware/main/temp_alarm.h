#pragma once
/*
 * Chip temperature warning with a gap between the warn and clear levels, so a reading on the line logs once.
 *
 * The sensor reads the die, which with the radio always on runs well above the air: 56 °C in its case on a
 * normal day (2026-10-18). The ESP32-C6 datasheet (Table 5-2) rates the chip for 105 °C of air around it but
 * gives no die limit, so the chip is not what overheats first: the MAX3232 module (0 to 70 °C for the common
 * C grade) and a printed case are. 85 °C on the die is about 30 °C above normal, the air around the bridge
 * having warmed by about that much: a case in the sun or by a heater.
 */
#include <stdbool.h>

#define TEMP_ALARM_WARN_C 85.0f
#define TEMP_ALARM_CLEAR_C 80.0f

typedef enum {
    TEMP_ALARM_SAME,
    TEMP_ALARM_NOW_HOT,
    TEMP_ALARM_NOW_OK,
} temp_alarm_change_t;

/* Updates *hot for reading c (°C) and says whether it changed: hot at warn_c or above, ok again below clear_c. */
temp_alarm_change_t temp_alarm_step(bool *hot, float c, float warn_c, float clear_c);
