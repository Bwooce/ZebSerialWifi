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

### Requirement: Boot order
`app_main()` SHALL start the bridge in the order of Notes, Boot sequence: settings and the log hook first, then the
board, UART pins and printer link driver, then the network stack and its services, then the printer link task and
the `status` task. An error that `ESP_ERROR_CHECK` catches (in steps 1, 4, 7, 8, 9 and 10) SHALL panic and restart
the chip (`CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT`, the default). The status task SHALL run every
`STATUS_TICK_MS` (100 ms), as in Notes, Boot sequence.

**Source**: `firmware/main/main.c` `app_main()`, `status_task()`, `STATUS_TICK_MS`

#### Scenario: Printer detection waits for the network stack
- **WHEN** the bridge boots
- **THEN** the printer link task SHALL start after the Wi-Fi, mDNS, web server and port 9100 listener have been started

#### Scenario: Boot log on the remote log
- **WHEN** the remote log is on at level 3 and the bridge boots
- **THEN** the version line and the board's antenna line SHALL be queued for the remote log (when they are sent is remote-log's)

### Requirement: Tasks
The bridge's tasks SHALL be created with the names, stack sizes and priorities in Notes, Tasks. The project's five
(`bridge`, `printer_link`, `status`, `dns_hijack`, `remote_log`) SHALL use `xTaskCreate()`, with no core affinity (the
ESP32-C6 has one core, `CONFIG_FREERTOS_UNICORE`). A higher number is a higher priority: the port 9100 listener (6)
SHALL be highest, the printer link and the HTTP server next and equal (5), then the captive DNS server (4).

**Source**: `firmware/main/main.c` `app_main()` (`xTaskCreate(status_task, "status", 4096, NULL, 3, NULL)`); `firmware/main/printer_link.c` `printer_link_start()` (`xTaskCreate(link_task, "printer_link", 6144, NULL, 5, NULL)`); `firmware/main/bridge.c` `bridge_start()` (`xTaskCreate(listen_task, "bridge", 6144, NULL, 6, NULL)`); `firmware/main/remote_log.c` `remote_log_start()`, `SENDER_STACK`, `SENDER_PRIO`; `firmware/main/dns_hijack.c` `dns_hijack_start()` (`xTaskCreate(dns_task, "dns_hijack", 3072, NULL, 4, &s_task)`); `firmware/main/web.c` `web_start()` (`cfg.stack_size`)

#### Scenario: Job running while a page loads
- **WHEN** a port 9100 job and a page request are both ready to run
- **THEN** the `bridge` task (priority 6) SHALL run before `httpd` (priority 5)

## Notes

### Boot sequence

`app_main()` in `firmware/main/main.c`:

1. `settings_init()`: NVS (NVS storage).
2. `remote_log_init()`: the log hook, before anything else logs, so the boot log is queued for the remote log.
3. One log line "ZebSerialWifi <version> built <date> <time>, IDF <idf version>" from `esp_app_get_description()`.
4. `board_init()`: RF switch and antenna, LED off, BOOT button input with pull-up, the LED timer.
5. `temp_mon_init()`: the chip temperature sensor.
6. The UART pins: fixed by the `pins` setting, else `board_detect_uart_pins()`.
7. `printer_link_init()` with those pins (the UART at the saved baud, else the default), pin swap allowed only when
   the pins were not certain, and the target baud from the settings.
8. `esp_netif_init()` and `esp_event_loop_create_default()`.
9. `wifi_prov_start()`: the station or the setup access point (wifi-provisioning).
10. `mdns_setup()`: hostname and services (discovery).
11. `remote_log_start()`: the sender task.
12. `web_start()`: the HTTP server.
13. `bridge_start()`: the port 9100 listener.
14. `printer_link_start()`: the link task, which starts printer detection.
15. The `status` task; `app_main()` then returns. Every `STATUS_TICK_MS` (100 ms) the status task runs `ota_poll()`
    (OTA confirmation), `temp_mon_poll()`, the mDNS model update, the LED mode and the BOOT-button hold.

### Tasks

| Task name | Created by | Stack (bytes) | Priority | Notes |
|---|---|---|---|---|
| `main` | ESP-IDF, runs `app_main()` | 3584 (`CONFIG_ESP_MAIN_TASK_STACK_SIZE` default) | 1 (`ESP_TASK_MAIN_PRIO`) | ends when `app_main()` returns |
| `bridge` | `bridge_start()` | 6144 | 6 | port 9100 listener and its jobs |
| `printer_link` | `printer_link_start()` | 6144 | 5 | detection and polling |
| `httpd` | `web_start()` through `httpd_start()` | 8192 (`cfg.stack_size`) | 5 (`tskIDLE_PRIORITY+5`, `HTTPD_DEFAULT_CONFIG`) | every HTTP handler, HTTP print jobs, OTA uploads, scans |
| `dns_hijack` | `dns_hijack_start()`, the first time the setup access point starts | 3072 | 4 | lives for good once created |
| `status` | `app_main()` | 4096 | 3 | the 100 ms status loop |
| `remote_log` | `remote_log_start()` | 6144 (`SENDER_STACK`) | 2 (`SENDER_PRIO`) | only if the log queue was allocated |
| `mdns` | `mdns_init()` in `mdns_setup()` | 4096 (`CONFIG_MDNS_TASK_STACK_SIZE` default) | 1 (`CONFIG_MDNS_TASK_PRIORITY` default) | pinned to core 0 (`CONFIG_MDNS_TASK_AFFINITY`), the only core |

The Wi-Fi, lwIP, event loop and `esp_timer` tasks are ESP-IDF's, at their defaults. The project's `esp_timer`
callbacks (`led`, `wifi_fb`, `wifi_retry`, `wifi_noip`, `wifi_join`, `wifi_trial`, `restart`) run in the `esp_timer`
task. The bridge, printer link and status tasks each log their unused stack once.

### Stored lengths

| Key | Type | Longest value | Source |
|---|---|---|---|
| `ssid` | string | 32 characters | `SETTINGS_SSID_MAX` 33 |
| `pass` | string | 64 characters; an open network is stored as the empty string "" | `SETTINGS_PASS_MAX` 65, `settings_set_wifi()` |
| `hostname` | string | 32 characters | `settings_net_t.hostname[33]`, `NETCFG_HOSTNAME_MAX` 32 |
| `cap_model` | string | 31 characters (read into a 32-byte buffer; a longer stored value does not match) | `settings_get_baud_cap()` |
| `model` | string | 31 characters | `settings_get_model(char out[32])` |

The `pins` setting is stored here; how it selects the UART pins, the pin detection at boot and the swap
fallback are specified in the printer-link spec (Pin selection at boot, Passive pin probe, Loopback pin probe and
defaults, Pin swap fallback).
