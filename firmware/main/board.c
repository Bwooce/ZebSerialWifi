#include "board.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "board";

#define GPIO_RF_SWITCH_EN 3   /* drive low to power the RF switch */
#define GPIO_ANT_SELECT 14    /* low: ceramic antenna, high: U.FL */
#define GPIO_LED 15           /* user LED, active low */
#define GPIO_BOOT 9           /* BOOT button, active low */
#define GPIO_UART_A 22        /* XIAO D4 */
#define GPIO_UART_B 23        /* XIAO D5 */

#define LED_TICK_MS 50

static volatile board_led_mode_t s_mode = LED_OFF;
static volatile int s_activity_ticks;
static uint32_t s_tick;

static void led_write(bool on)
{
    gpio_set_level(GPIO_LED, on ? 0 : 1);
}

static bool pattern_on(board_led_mode_t mode, uint32_t tick)
{
    switch (mode) {
    case LED_AP:
        return (tick / 2) % 2 == 0;                 /* 100 ms on, 100 ms off */
    case LED_CONNECTING:
        return (tick / 10) % 2 == 0;                /* 500 ms on, 500 ms off */
    case LED_READY:
        return true;
    case LED_NO_PRINTER: {
        uint32_t t = tick % 40;                     /* 2 s period */
        return t < 2 || (t >= 4 && t < 6);
    }
    case LED_OFF:
    default:
        return false;
    }
}

static void led_tick(void *arg)
{
    (void)arg;
    s_tick++;
    bool on = pattern_on(s_mode, s_tick);
    if (s_activity_ticks > 0) {
        s_activity_ticks--;
        on = !on;
    }
    led_write(on);
}

void board_init(void)
{
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << GPIO_RF_SWITCH_EN) | (1ULL << GPIO_ANT_SELECT) | (1ULL << GPIO_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&out));
    gpio_set_level(GPIO_RF_SWITCH_EN, 0);
#if CONFIG_ZEB_EXTERNAL_ANTENNA
    gpio_set_level(GPIO_ANT_SELECT, 1);
    ESP_LOGI(TAG, "antenna: external U.FL");
#else
    gpio_set_level(GPIO_ANT_SELECT, 0);
    ESP_LOGI(TAG, "antenna: onboard ceramic");
#endif
    led_write(false);

    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << GPIO_BOOT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn));

    const esp_timer_create_args_t args = {.callback = led_tick, .name = "led"};
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, LED_TICK_MS * 1000));
}

bool board_detect_uart_pins(int *tx, int *rx)
{
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << GPIO_UART_A) | (1ULL << GPIO_UART_B),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in));
    esp_rom_delay_us(2000);

    /* sample several times so a byte arriving from the printer does not fool us */
    int high_a = 0;
    int high_b = 0;
    const int samples = 20;
    for (int i = 0; i < samples; i++) {
        high_a += gpio_get_level(GPIO_UART_A);
        high_b += gpio_get_level(GPIO_UART_B);
        esp_rom_delay_us(500);
    }
    gpio_reset_pin(GPIO_UART_A);
    gpio_reset_pin(GPIO_UART_B);

    bool a_high = high_a > samples * 3 / 4;
    bool b_high = high_b > samples * 3 / 4;
    bool a_low = high_a < samples / 4;
    bool b_low = high_b < samples / 4;
    if (a_high && b_low) {
        *rx = GPIO_UART_A;
        *tx = GPIO_UART_B;
    } else if (b_high && a_low) {
        *rx = GPIO_UART_B;
        *tx = GPIO_UART_A;
    } else {
        *tx = CONFIG_ZEB_UART_TX_GPIO;
        *rx = CONFIG_ZEB_UART_RX_GPIO;
        ESP_LOGW(TAG, "UART pin detection inconclusive (GPIO%d high %d/%d, GPIO%d high %d/%d), using defaults",
                 GPIO_UART_A, high_a, samples, GPIO_UART_B, high_b, samples);
        return false;
    }
    ESP_LOGI(TAG, "UART pins detected: TX=GPIO%d RX=GPIO%d", *tx, *rx);
    return true;
}

void board_led_set_mode(board_led_mode_t mode)
{
    s_mode = mode;
}

void board_led_activity(void)
{
    s_activity_ticks = 1;
}

bool board_button_pressed(void)
{
    return gpio_get_level(GPIO_BOOT) == 0;
}
