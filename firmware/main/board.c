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
#define GPIO_UART_A 21        /* XIAO D3 */
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

typedef enum { PIN_FLOATING, PIN_DRIVEN_LOW, PIN_DRIVEN_HIGH, PIN_MIXED } pin_state_t;

/* Fraction of samples reading high with the given pull, 0..20. */
static int sample_high(int gpio, bool pull_up)
{
    gpio_config_t in = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = pull_up ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
    };
    gpio_config(&in);
    esp_rom_delay_us(1000);
    int high = 0;
    for (int i = 0; i < 20; i++) {
        high += gpio_get_level(gpio);
        esp_rom_delay_us(250);
    }
    return high;
}

/*
 * A pin that reads the same with a pull-up and a pull-down is being driven by
 * something else; one that follows the pull is floating. MIXED means it is
 * toggling (data arriving) and the test should not be trusted.
 */
static pin_state_t probe_pin(int gpio)
{
    int up = sample_high(gpio, true);
    int down = sample_high(gpio, false);
    gpio_reset_pin(gpio);
    if (up == 20 && down == 20) {
        return PIN_DRIVEN_HIGH;
    }
    if (up == 0 && down == 0) {
        return PIN_DRIVEN_LOW;
    }
    if (up == 20 && down == 0) {
        return PIN_FLOATING;
    }
    return PIN_MIXED;
}

static const char *pin_state_name(pin_state_t s)
{
    static const char *names[] = {"floating", "driven low", "driven high", "toggling"};
    return names[s];
}

/* XIAO pad names and GPIOs, for the wiring diagnostic. */
static const struct {
    const char *pad;
    int gpio;
} XIAO_PADS[] = {
    {"D0", 0}, {"D1", 1}, {"D2", 2}, {"D3", 21}, {"D4", 22}, {"D5", 23},
    {"D6", 16}, {"D7", 17}, {"D8", 19}, {"D9", 20}, {"D10", 18},
};

/*
 * Probe one pin while the other is pulled up. If the other pin is T1IN, the
 * pull-up holds the RS232 line at mark, so with a loopback plug (or anything
 * else echoing) R1OUT still idles high instead of following a floating input.
 */
static pin_state_t probe_pin_other_high(int gpio, int other)
{
    gpio_config_t up = {
        .pin_bit_mask = 1ULL << other,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&up);
    esp_rom_delay_us(1000);
    pin_state_t s = probe_pin(gpio);
    gpio_reset_pin(other);
    return s;
}

/*
 * Drive one pin and watch the other: with a loopback plug, D-sub TX and RX
 * are joined, so RX follows TX through the MAX3232. Only ever called for
 * pins the passive probe found floating, so it never fights the MAX3232's
 * R1OUT driver. The sense pin is pulled UP: behind the RX Schottky diode
 * (anode on the pin, cathode to R1OUT) the module can only pull RX low, so
 * mark reads high only through our own pull-up. A pull-down would read low
 * whatever the module did.
 */
static bool probe_drive_sense(int drive_pin, int sense_pin)
{
    gpio_config_t in_cfg = {
        .pin_bit_mask = 1ULL << sense_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&in_cfg);
    gpio_config_t out_cfg = {
        .pin_bit_mask = 1ULL << drive_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&out_cfg);

    gpio_set_level(drive_pin, 1);  /* mark */
    esp_rom_delay_us(5000);
    int read_high = gpio_get_level(sense_pin);
    gpio_set_level(drive_pin, 0);  /* space */
    esp_rom_delay_us(5000);
    int read_low = gpio_get_level(sense_pin);
    gpio_set_level(drive_pin, 1);
    esp_rom_delay_us(5000);

    gpio_reset_pin(drive_pin);
    gpio_reset_pin(sense_pin);
    ESP_LOGI(TAG, "loopback test: drive GPIO%d, sense GPIO%d -> (1->%d, 0->%d)", drive_pin, sense_pin, read_high,
             read_low);
    return read_high == 1 && read_low == 0;
}

bool board_detect_uart_pins(int *tx, int *rx)
{
    pin_state_t a = probe_pin_other_high(GPIO_UART_A, GPIO_UART_B);
    pin_state_t b = probe_pin_other_high(GPIO_UART_B, GPIO_UART_A);
    ESP_LOGI(TAG, "passive probe: D3/GPIO%d %s, D5/GPIO%d %s", GPIO_UART_A, pin_state_name(a), GPIO_UART_B,
             pin_state_name(b));

    /*
     * Passive inconclusive and both pins floating (for example RX behind a
     * level-shift diode with no pull-up): try a loopback plug in each direction.
     */
    if (a == PIN_FLOATING && b == PIN_FLOATING) {
        bool b_to_a = probe_drive_sense(GPIO_UART_B, GPIO_UART_A);
        bool a_to_b = probe_drive_sense(GPIO_UART_A, GPIO_UART_B);
        if (b_to_a != a_to_b) {
            *tx = b_to_a ? GPIO_UART_B : GPIO_UART_A;
            *rx = b_to_a ? GPIO_UART_A : GPIO_UART_B;
            ESP_LOGI(TAG, "UART pins detected by loopback: TX=GPIO%d RX=GPIO%d", *tx, *rx);
            return true;
        }
    }

    /*
     * One pin high and the other floating fits two wirings: R1OUT wired
     * straight to the high pin with T1IN undriven (module at 3.3 V), or a
     * module at 5 V whose T1IN pull-up drives the TX pin high while RX
     * floats behind the level-shift diode. Use the defaults; the printer
     * link tries the swapped pins if no printer answers.
     */
    if ((a == PIN_DRIVEN_HIGH && b == PIN_FLOATING) || (b == PIN_DRIVEN_HIGH && a == PIN_FLOATING)) {
        *tx = CONFIG_ZEB_UART_TX_GPIO;
        *rx = CONFIG_ZEB_UART_RX_GPIO;
        ESP_LOGI(TAG, "one pin high, one floating: using the default TX=GPIO%d RX=GPIO%d, swapped if no printer "
                      "answers", *tx, *rx);
        return false;
    }

    /* MAX3232 R1OUT idles high (RS232 input open or at mark); T1IN is an undriven input */
    if (a == PIN_DRIVEN_HIGH && b != PIN_DRIVEN_HIGH) {
        *rx = GPIO_UART_A;
        *tx = GPIO_UART_B;
    } else if (b == PIN_DRIVEN_HIGH && a != PIN_DRIVEN_HIGH) {
        *rx = GPIO_UART_B;
        *tx = GPIO_UART_A;
    } else {
        *tx = CONFIG_ZEB_UART_TX_GPIO;
        *rx = CONFIG_ZEB_UART_RX_GPIO;
        ESP_LOGW(TAG, "UART pin detection inconclusive, using TX=GPIO%d RX=GPIO%d. "
                      "Check the module has power and its TXD pin goes to D3 or D5. With RX behind a "
                      "level-shift diode the pins look alike: fit a loopback plug at boot, or set the "
                      "pins on the settings page.", *tx, *rx);
        for (size_t i = 0; i < sizeof(XIAO_PADS) / sizeof(XIAO_PADS[0]); i++) {
            pin_state_t s = probe_pin(XIAO_PADS[i].gpio);
            if (s != PIN_FLOATING) {
                ESP_LOGW(TAG, "  pad %s (GPIO%d) is %s", XIAO_PADS[i].pad, XIAO_PADS[i].gpio, pin_state_name(s));
            }
        }
        return false;
    }
    ESP_LOGI(TAG, "UART pins detected by passive idle: TX=GPIO%d RX=GPIO%d", *tx, *rx);
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
