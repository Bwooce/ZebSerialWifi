#include "temp_alarm.h"

temp_alarm_change_t temp_alarm_step(bool *hot, float c, float warn_c, float clear_c)
{
    if (!*hot && c >= warn_c) {
        *hot = true;
        return TEMP_ALARM_NOW_HOT;
    }
    if (*hot && c < clear_c) {
        *hot = false;
        return TEMP_ALARM_NOW_OK;
    }
    return TEMP_ALARM_SAME;
}
