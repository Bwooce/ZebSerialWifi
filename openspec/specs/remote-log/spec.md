# remote-log Specification

## Purpose
Copies of the bridge's ESP_LOG output sent over the network to an iot_log receiver, found by mDNS and reached by
UDP multicast, with no receiver address to configure. Implemented in `firmware/main/remote_log.c` and
`firmware/main/log_line.c` on top of the `bwooce/esp_iot_log` component.

## Requirements

### Requirement: Log hook copies every log line
The bridge SHALL install a vprintf hook that passes every log call to the previous output (the USB console) and
also queues a copy of the formatted line, cut to 240 characters, for the remote log. Calls from an ISR or from the
remote log's own sender task MUST NOT be queued.

**Source**: `firmware/main/remote_log.c` `remote_log_init()`, `log_hook()`, `RLOG_LINE_MAX`, `esp_log_set_vprintf()`

#### Scenario: Line logged during boot
- **WHEN** a component logs a line after `remote_log_init()` and before Wi-Fi is up
- **THEN** the line SHALL appear on the USB console and SHALL be queued for the remote log

#### Scenario: Sender task logs
- **WHEN** the remote log's sender task itself logs a line
- **THEN** the line SHALL go to the console only

### Requirement: No log queue, no remote log
If `remote_log_init()` cannot allocate its 12288-byte ring buffer (`QUEUE_BYTES`), the bridge MUST log "no memory for
the log queue: remote log off" and SHALL leave the remote log off for that boot, without installing the log hook.

**Source**: `firmware/main/remote_log.c` `remote_log_init()`, `xRingbufferCreate()`, `QUEUE_BYTES`

#### Scenario: Allocation fails at boot
- **WHEN** `xRingbufferCreate()` returns NULL
- **THEN** log lines SHALL go to the USB console only and nothing SHALL be sent to a receiver

### Requirement: Split lines are joined
When a task's log output arrives in several calls, the hook SHALL hold the text until a newline arrives and queue
it as one line. If another task logs while a line is unfinished, the unfinished line SHALL be queued as it stands.
If two tasks are in the hook at once, the second task's text SHALL be dropped and counted.

**Source**: `firmware/main/remote_log.c` `log_hook()`, `s_line`, `s_line_len`, `s_line_task`, `s_line_busy`, `s_hook_dropped`

#### Scenario: Wi-Fi driver prints prefix and text separately
- **WHEN** the Wi-Fi driver prints "I (471) wifi:" and then its text with a newline in a second call
- **THEN** the remote log SHALL queue a single line holding both parts

### Requirement: Level filter and level mapping
The bridge SHALL read a line's level from its first letter followed by " (" (E 1, W 2, I 3, D 4, V 5) and treat a
line without that prefix as level 3. It SHALL queue only lines at or below the saved level (1 errors, 2 warnings and
errors, 3 everything) and SHALL strip the "W (1234) " prefix and trailing CR/LF before sending.

**Source**: `firmware/main/log_line.c` `log_line_level()`, `log_line_body()`, `log_line_trim()`; `firmware/main/remote_log.c` `queue_line()`, `sender_task()`; `firmware/test_host/test_main.c` `test_log_line()`

#### Scenario: Warning at the errors-only level
- **WHEN** the level is 1 and a "W (1234) tag: text" line is logged
- **THEN** the line SHALL NOT be queued

#### Scenario: Raw printf line
- **WHEN** a line with no level letter and timestamp is logged at level 3
- **THEN** it SHALL be sent as an info line with its text unchanged

### Requirement: Call into iot_log
The sender SHALL send each line with one call `iot_log(level, "%s", body)`. `level` SHALL be the line's level number
cast to `iot_log_level_t` (1 `IOT_LOG_ERROR`, 2 `IOT_LOG_WARN`, 3 `IOT_LOG_INFO`, 4 `IOT_LOG_DEBUG`, 5
`IOT_LOG_VERBOSE`), with a line that has no prefix sent as `IOT_LOG_INFO`. `body` SHALL be the queued line after its
prefix. iot_log takes no tag, so the ESP_LOG tag stays at the start of the body. The configured iot_log level SHALL be
the saved level, cast the same way.

**Source**: `firmware/main/remote_log.c` `sender_task()`, `configure()` (`lc.level`); `firmware/main/log_line.c` `log_line_body()`; `firmware/managed_components/bwooce__esp_iot_log/include/esp_iot_log.h` `iot_log_level_t`, `iot_log()`

#### Scenario: Warning line sent
- **WHEN** "W (5120) printer: no reply at 9600 baud" is logged at level 2
- **THEN** the sender SHALL call `iot_log(IOT_LOG_WARN, "%s", "printer: no reply at 9600 baud")`

### Requirement: On by default, set on the Bridge page
The remote log SHALL start on at level 3 when nothing is saved (storage and defaults as in device-settings, Remote log
settings). The Bridge page's Remote log section SHALL offer an on/off checkbox and a level choice (Errors; Warnings and
errors; Everything); a level outside 1 to 3 MUST be refused with 400 "Unknown remote log level.".

**Source**: `firmware/main/settings.c` `settings_get_rlog()`, `settings_set_rlog()`; `firmware/main/settings.h` `SETTINGS_RLOG_LEVEL_MIN`, `SETTINGS_RLOG_LEVEL_MAX`; `firmware/main/web.c` `bridge_get_page()`, `config_post()`

#### Scenario: Fresh bridge
- **WHEN** NVS holds no `rlog_on` key
- **THEN** the remote log SHALL be on at level 3

#### Scenario: Turned off on the Bridge page
- **WHEN** the user clears the Remote log checkbox and saves
- **THEN** the bridge SHALL store `rlog_on` = 0 and apply it without a restart

### Requirement: Settings apply without restart
When the remote log setting or level changes, the bridge SHALL notify the sender task, which SHALL stop iot_log and
start it again from the saved settings. While off, the sender SHALL empty the queue and wait for the next change.
Saving the remote log settings SHALL also erase the obsolete `rlog_ip` NVS key.

**Source**: `firmware/main/remote_log.c` `remote_log_apply()`, `configure()`, `sender_task()`; `firmware/main/settings.c` `settings_set_rlog()`

#### Scenario: Level changed
- **WHEN** the user changes the level from 3 to 2 and saves
- **THEN** the sender SHALL call `iot_log_deinit()` and `iot_log_init()` with level 2

### Requirement: Receiver found by mDNS, no configured address
The bridge SHALL start iot_log with its hostname as device name, no serial mirror and no unicast address, so lines go
only to the multicast group 239.255.1.100 port 4210, and only while mDNS discovery of the `_esp-iot-log._udp` service
has found a receiver. Discovery SHALL run from the sender task through `iot_log_poll()`.

**Source**: `firmware/main/remote_log.c` `configure()` (`IOT_LOG_CONFIG_DEFAULT()`, `lc.device_name`, `lc.serial_mirror`), `sender_task()`; `firmware/managed_components/bwooce__esp_iot_log/include/esp_iot_log.h` `IOT_LOG_DEFAULT_MCAST_IP`, `IOT_LOG_DEFAULT_MCAST_PORT`, `IOT_LOG_SERVICE_NAME`

#### Scenario: Receiver on the network
- **WHEN** a receiver advertises `_esp-iot-log._udp` and the bridge's discovery finds it
- **THEN** the bridge SHALL send each queued line as a multicast datagram

#### Scenario: No receiver
- **WHEN** discovery has found no receiver
- **THEN** the bridge SHALL send nothing over the network

### Requirement: Lines held until Wi-Fi is up
While iot_log is not running or the station is not `WIFI_ST_CONNECTED`, the sender SHALL leave queued lines in the
12288-byte queue and check again every 1 s. Lines that do not fit the queue SHALL be dropped and counted.

**Source**: `firmware/main/remote_log.c` `sender_task()`, `QUEUE_BYTES`, `queue_line()`, `s_hook_dropped`

#### Scenario: Boot log before Wi-Fi
- **WHEN** the bridge logs its boot lines before it has an IPv4 address
- **THEN** those lines SHALL stay queued, up to 12288 bytes, and be sent once it is connected and a receiver is found

### Requirement: Lines consumed while no receiver is found
While connected, the sender SHALL take each queued line and pass it to iot_log. If no receiver has been found, the
line SHALL be discarded without being sent and without being counted as dropped.

**Source**: `firmware/main/remote_log.c` `sender_task()`; `firmware/managed_components/bwooce__esp_iot_log/esp_iot_log.c` `iot_log()` (sends only when `listener_active`)

#### Scenario: Connected, receiver not yet found
- **WHEN** the bridge is connected and discovery has not found a receiver
- **THEN** queued lines SHALL be removed from the queue and lost
- **AND** the dropped count SHALL not change

### Requirement: Retries and backlog pacing
When a send to a found receiver fails, the sender SHALL retry the line up to 3 more times, 20 ms apart, while a
receiver is still found, then count it as dropped. After each line it SHALL wait one FreeRTOS tick (10 ms at
`CONFIG_FREERTOS_HZ=100`) so a backlog is not sent as one burst.

**Source**: `firmware/main/remote_log.c` `sender_task()`, `SEND_RETRIES`, `SEND_RETRY_MS`, `s_send_dropped`, `vTaskDelay(1)`; `firmware/sdkconfig` `CONFIG_FREERTOS_HZ=100`

#### Scenario: Send to a found receiver fails
- **WHEN** iot_log's dropped count rises during a send while a receiver is still found
- **THEN** the sender SHALL retry after 20 ms, at most 3 times, then count the line as dropped

### Requirement: Remote log state on the Status page
`GET /api/status` SHALL include `rlog` with `enabled`, `listener` (a receiver found and iot_log running), `sent` and
`dropped`. The Status page SHALL show "off", or "sending to an iot_log receiver" or "no receiver found", followed by
"<n> lines sent".

**Source**: `firmware/main/remote_log.c` `remote_log_get_status()`; `firmware/main/web.c` `status_get()` JSON key `"rlog"`, Status page script `row('Remote log', ...)`

#### Scenario: On with no receiver
- **WHEN** the remote log is on and no receiver has been found
- **THEN** the Status page SHALL show "no receiver found, <n> lines sent"

## Notes
- `sent` comes from `iot_log_get_sent_count()` and resets each time iot_log is started (boot or settings change).
  `dropped` is the hook's and the sender's own counts since boot; the Status page does not show it.
- iot_log stamps each datagram when it is sent, not when the line was logged, so a held backlog arrives with send
  times (`docs/papercuts/2026-10-18-main-remote-log-loses-offline-lines.md`).
- Discovery runs at most every 60 to 300 s (`DISCOVERY_MIN_INTERVAL_MS`, `DISCOVERY_MAX_INTERVAL_MS` in the component),
  and each query blocks the sender for up to 1 s.
- The component logs "Log listener discovered" at info level and a lost receiver only at debug level; both come from
  the sender task, so they reach the USB console but not the remote log.
