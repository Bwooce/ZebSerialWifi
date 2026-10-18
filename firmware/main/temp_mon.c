#include "temp_mon.h"
#include "driver/temperature_sensor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "temp_alarm.h"

static const char *TAG = "temp";

#define TEMP_READ_US (30 * 1000000LL)

static temperature_sensor_handle_t s_tsens;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static temp_mon_status_t s_st;
static int64_t s_next_us;

void temp_mon_init(void)
{
    /* 20 to 100 °C: the driver's ±2 °C range that covers the warning level */
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    esp_err_t err = temperature_sensor_install(&cfg, &s_tsens);
    if (err == ESP_OK) {
        err = temperature_sensor_enable(s_tsens);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "chip temperature sensor would not start: %s", esp_err_to_name(err));
        s_tsens = NULL;
    }
}

void temp_mon_poll(void)
{
    int64_t now = esp_timer_get_time();
    if (s_tsens == NULL || now < s_next_us) {
        return;
    }
    s_next_us = now + TEMP_READ_US;
    float c;
    if (temperature_sensor_get_celsius(s_tsens, &c) != ESP_OK) {
        return;
    }
    bool first = !s_st.valid;
    bool hot = s_st.hot;
    temp_alarm_change_t change = temp_alarm_step(&hot, c, TEMP_ALARM_WARN_C, TEMP_ALARM_CLEAR_C);
    taskENTER_CRITICAL(&s_lock);
    s_st.valid = true;
    s_st.temp_c = c;
    if (first || c > s_st.max_c) {
        s_st.max_c = c;
    }
    s_st.hot = hot;
    taskEXIT_CRITICAL(&s_lock);
    if (first) {
        ESP_LOGI(TAG, "chip temperature %.1f °C", (double)c);
    }
    if (change == TEMP_ALARM_NOW_HOT) {
        ESP_LOGW(TAG, "chip too hot: %.1f °C (warns at %.0f °C); check the case is not in the sun or by a heater",
                 (double)c, (double)TEMP_ALARM_WARN_C);
    } else if (change == TEMP_ALARM_NOW_OK) {
        ESP_LOGI(TAG, "chip temperature back to %.1f °C", (double)c);
    }
}

void temp_mon_get_status(temp_mon_status_t *out)
{
    taskENTER_CRITICAL(&s_lock);
    *out = s_st;
    taskEXIT_CRITICAL(&s_lock);
}
