#pragma once
/* XIAO ESP32-C6 board support: RF switch, LED, BOOT button, UART pin detection. */
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LED_OFF,
    LED_AP,          /* fast blink: SoftAP setup mode */
    LED_CONNECTING,  /* slow blink */
    LED_READY,       /* solid on: WiFi up and printer detected */
    LED_NO_PRINTER,  /* double blink every 2 s: WiFi up, no printer */
} board_led_mode_t;

void board_init(void);

/*
 * Work out which of the two UART pins is the MAX3232 receiver output. With
 * both pins as pulled-down inputs, R1OUT idles high, so the high pin is RX.
 * Falls back to the Kconfig defaults when the result is inconclusive.
 * Returns true when detection was conclusive.
 */
bool board_detect_uart_pins(int *tx, int *rx);

void board_led_set_mode(board_led_mode_t mode);

/* Briefly invert the LED to show data flowing. Safe to call from any task. */
void board_led_activity(void);

/* True while the BOOT button is pressed. */
bool board_button_pressed(void);
