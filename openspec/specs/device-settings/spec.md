# device-settings Specification

## Purpose
The bridge's persistent settings and board-level behaviour: the NVS namespace and every key in it, defaults and
validation on load and save, what each setting controls, the BOOT-button reset, the status LED, the RF switch and
antenna choice, and the boot-time UART pin choice. Code: `firmware/main/settings.c`, `settings.h`, `board.c`,
`board.h`, `main.c`, `Kconfig.projbuild` and `sdkconfig.defaults`.

## Requirements

### Requirement: NVS storage
The bridge SHALL keep all its settings in the NVS namespace `zebserial` on the `nvs` partition (offset 0x9000, size
0x6000). At boot, if NVS reports no free pages or a new NVS version, it MUST erase the whole partition and initialise
it again.

**Source**: `firmware/main/settings.c` `NS`, `settings_init()`; `firmware/partitions.csv` `nvs`

#### Scenario: NVS format change
- **WHEN** `nvs_flash_init()` returns `ESP_ERR_NVS_NEW_VERSION_FOUND`
- **THEN** the bridge SHALL erase NVS and initialise it again, losing all saved settings

### Requirement: Wi-Fi credentials
The bridge SHALL store the network as string keys `ssid` (up to 32 characters) and `pass` (up to 64). A network counts
as saved only when `ssid` exists and is non-empty; a missing `pass` MUST read as an empty password. The Wi-Fi driver's
own storage SHALL be RAM only, so credentials live only in this namespace.

**Source**: `firmware/main/settings.c` `settings_get_wifi()`, `settings_set_wifi()`, `settings_clear_wifi()`; `firmware/main/settings.h` `SETTINGS_SSID_MAX`, `SETTINGS_PASS_MAX`; `firmware/main/wifi_prov.c` `esp_wifi_set_storage(WIFI_STORAGE_RAM)`

#### Scenario: SSID without password
- **WHEN** `ssid` is `Home` and there is no `pass` key
- **THEN** the bridge SHALL treat `Home` as an open network

### Requirement: Serial link options
The bridge SHALL store `idle_s` (u16, 5 to 3600 s, default `CONFIG_ZEB_IDLE_TIMEOUT_S` = 30), `tgt_baud` (u32, one of
0, 9600, 19200, 38400, 57600, 115200; default 115200; 0 leaves the printer's baud alone) and `pins` (u8, 0 detect, 1 TX
on D3, 2 TX on D5; default 0). On load, a missing or out-of-range value MUST give that option's default.

**Source**: `firmware/main/settings.c` `settings_get_options()`, `settings_target_baud_valid()`; `firmware/main/settings.h` `SETTINGS_IDLE_MIN_S`, `SETTINGS_IDLE_MAX_S`, `settings_pins_t`; `firmware/main/Kconfig.projbuild` `ZEB_IDLE_TIMEOUT_S`

#### Scenario: Bad stored idle timeout
- **WHEN** `idle_s` holds 2
- **THEN** the idle timeout SHALL be 30 s

### Requirement: Options saved only when valid
Saving the serial link options MUST fail with `ESP_ERR_INVALID_ARG`, writing nothing, if any option is out of range.
Otherwise all three keys SHALL be written and committed together.

**Source**: `firmware/main/settings.c` `settings_set_options()`, `settings_options_valid()`

#### Scenario: Unsupported baud
- **WHEN** options with target baud 4800 are saved
- **THEN** the save SHALL return `ESP_ERR_INVALID_ARG` and NVS SHALL be unchanged

### Requirement: What the serial link options control
The idle timeout SHALL be read at the start of each print session. The target baud SHALL apply at once (a change
forgets the learned baud cap and re-detects the printer). The pin choice MUST take effect only at the next boot, so a
change to it SHALL restart the bridge.

**Source**: `firmware/main/bridge.c` `serve()`; `firmware/main/printer_link.c` `printer_link_set_target_baud()`; `firmware/main/main.c` `app_main()`; `firmware/main/web.c` `config_post()`

#### Scenario: Pins changed on the Bridge page
- **WHEN** the pin choice is changed and saved
- **THEN** the bridge SHALL save it and restart 1.5 s after replying

### Requirement: Network settings
The bridge SHALL store `hostname` (string), `static_ip` (u8 0/1) and `ip`, `mask`, `gw`, `dns` (u32, IPv4 in host byte
order; gateway 0 means none, DNS 0 means use the gateway). Defaults MUST be hostname `zebserial` and DHCP. An invalid
stored hostname SHALL load as the default, and a stored static set SHALL be used only if it still validates.

**Source**: `firmware/main/settings.c` `settings_get_net()`, `settings_set_net()`, `settings_net_valid()`; `firmware/main/settings.h` `settings_net_t`, `SETTINGS_DEFAULT_HOSTNAME`

#### Scenario: Stale static set
- **WHEN** `static_ip` is 1 but the stored mask is 0
- **THEN** the bridge SHALL use DHCP

### Requirement: Network settings read at boot
Saving network settings MUST fail with `ESP_ERR_INVALID_ARG` if the hostname or static set is invalid. The bridge
SHALL read the hostname and addressing only at boot, so a saved change takes effect after a restart; which changes
on the Bridge page restart the bridge is specified in web-ui (Applying the bridge settings).

**Source**: `firmware/main/settings.c` `settings_set_net()`; `firmware/main/wifi_prov.c` `wifi_prov_start()`

#### Scenario: Invalid hostname saved
- **WHEN** `settings_set_net()` is given the hostname `-bad`
- **THEN** it SHALL return `ESP_ERR_INVALID_ARG` and store nothing

### Requirement: Remote log settings
The bridge SHALL store `rlog_on` (u8, default on) and `rlog_lvl` (u8, 1 errors, 2 warnings, 3 info; default 3; an
out-of-range stored value loads as 3). Saving MUST reject a level outside 1 to 3 and SHALL erase the legacy `rlog_ip`
key.

**Source**: `firmware/main/settings.c` `settings_get_rlog()`, `settings_set_rlog()`; `firmware/main/settings.h` `SETTINGS_RLOG_LEVEL_MIN`, `SETTINGS_RLOG_LEVEL_MAX`

#### Scenario: Nothing saved
- **WHEN** no remote log keys exist
- **THEN** the remote log SHALL be on at info level

### Requirement: Learned printer state
The bridge SHALL store what it learns about the printer: `baud` (u32, last baud the printer answered at; 0 or missing
means none, and 9600 is used), `cap_model` (string) with `cap_baud` (u32, lowest baud that model failed to move to,
read as 0 for another model), `sc_sent` (u8 flag, set once `^SC` was sent while the handshake could not be read) and `model` (string, last model from `~HI`).

**Source**: `firmware/main/settings.c` `settings_get_baud()`, `settings_get_baud_cap()`, `settings_get_flag()`, `settings_get_model()`; `firmware/main/printer_link.c` `fallback_baud()`, `PRINTER_DEFAULT_BAUD`

#### Scenario: Different printer fitted
- **WHEN** `cap_model` is `GX430t` and the printer now reports `ZD421`
- **THEN** the baud cap SHALL read as 0

### Requirement: BOOT-button reset
When the BOOT button (GPIO9, active low, pull-up on) is held for 5 s, sampled every 100 ms, the bridge SHALL erase
`ssid`, `pass`, `hostname`, `static_ip`, `ip`, `mask`, `gw` and `dns`, wait 200 ms and restart. All other keys MUST be
kept. Releasing the button earlier SHALL reset the count.

**Source**: `firmware/main/main.c` `status_task()`, `FACTORY_RESET_HOLD_MS`, `STATUS_TICK_MS`; `firmware/main/settings.c` `settings_clear_wifi()`, `settings_clear_net()`; `firmware/main/board.c` `GPIO_BOOT`, `board_button_pressed()`

#### Scenario: Locked out by a bad static address
- **WHEN** BOOT is held for 5 s
- **THEN** the bridge SHALL restart in setup mode with DHCP and hostname `zebserial`
- **AND** the serial link options, remote log settings, saved baud, baud cap, `sc_sent` and model SHALL be kept

#### Scenario: Short press
- **WHEN** BOOT is held for 3 s and released
- **THEN** nothing SHALL be cleared

### Requirement: Status LED patterns
The user LED (GPIO15, active low) SHALL be driven from a 50 ms tick: setup access point running, 100 ms on and 100 ms
off; connecting, 500 ms on and 500 ms off; connected with the printer answering, solid on; connected with no printer
answering, two 100 ms flashes 100 ms apart every 2 s. It MUST be off until the first status update.

**Source**: `firmware/main/board.c` `pattern_on()`, `LED_TICK_MS`, `GPIO_LED`, `led_write()`; `firmware/main/main.c` `status_task()`

#### Scenario: Printer switched off
- **WHEN** Wi-Fi is connected and the printer stops answering
- **THEN** the LED SHALL show the double flash every 2 s

### Requirement: LED data activity flicker
When print data flows, the bridge SHALL invert the LED's current pattern for one 50 ms tick per activity signal.

**Source**: `firmware/main/board.c` `board_led_activity()`, `led_tick()`; `firmware/main/bridge.c` `board_led_activity()`

#### Scenario: Job printing while solid on
- **WHEN** bytes pass through the bridge while the LED is solid on
- **THEN** the LED SHALL blink off briefly

### Requirement: RF switch and antenna
At boot the bridge SHALL drive GPIO3 low to power the RF switch, and drive GPIO14 low for the ceramic antenna, or high
for the U.FL connector when `CONFIG_ZEB_EXTERNAL_ANTENNA` is set (default off). The antenna MUST NOT be a runtime
setting.

**Source**: `firmware/main/board.c` `board_init()`, `GPIO_RF_SWITCH_EN`, `GPIO_ANT_SELECT`; `firmware/main/Kconfig.projbuild` `ZEB_EXTERNAL_ANTENNA`; `firmware/main/settings.h`

#### Scenario: U.FL build
- **WHEN** the firmware is built with `CONFIG_ZEB_EXTERNAL_ANTENNA=y`
- **THEN** GPIO14 SHALL be driven high at boot and the log SHALL say "antenna: external U.FL"

#### Scenario: Default build
- **WHEN** the firmware is built without `CONFIG_ZEB_EXTERNAL_ANTENNA`
- **THEN** GPIO14 SHALL be driven low and the log SHALL say "antenna: onboard ceramic"

### Requirement: Console
The bridge SHALL send its console log over the USB Serial/JTAG port; UART1 is the printer link.

**Source**: `firmware/sdkconfig.defaults` `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`

#### Scenario: USB connected
- **WHEN** a computer is connected to the XIAO's USB-C port
- **THEN** the boot log SHALL appear on its USB serial device

## Notes

The `pins` setting is stored here; how it selects the UART pins, the pin detection at boot and the swap
fallback are specified in the printer-link spec (Pin selection at boot, Passive pin probe, Loopback pin probe and
defaults, Pin swap fallback).
