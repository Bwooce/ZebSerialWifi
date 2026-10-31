# chip-temperature Specification

## Purpose
Monitoring of the ESP32-C6's own die temperature: periodic reads of the internal sensor, a warning with hysteresis
when the bridge gets too hot, and the reading on the Status page. Implemented in `firmware/main/temp_mon.c` and
`firmware/main/temp_alarm.c`, shown through `firmware/main/web.c`.

## Requirements

### Requirement: Sensor setup
At start-up the bridge SHALL install and enable the internal temperature sensor with the 20 to 100 °C range. If the
sensor will not start, the bridge MUST log a warning "chip temperature sensor would not start: <error>" and carry
on with no temperature readings.

**Source**: `firmware/main/temp_mon.c` `temp_mon_init()`, `TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100)`

#### Scenario: Sensor fails to install
- **WHEN** `temperature_sensor_install()` or `temperature_sensor_enable()` fails
- **THEN** the bridge SHALL log the warning and never report a reading

### Requirement: Read every 30 s
The bridge SHALL read the sensor from the status loop at its first poll after start-up and then every 30 s. A read
that fails SHALL be skipped until the next period. The first good reading SHALL be logged at info level as
"chip temperature <t> °C".

**Source**: `firmware/main/temp_mon.c` `temp_mon_poll()`, `TEMP_READ_US`; `firmware/main/main.c` status task (`temp_mon_poll()` every `STATUS_TICK_MS`)

#### Scenario: First reading after boot
- **WHEN** the status loop first calls `temp_mon_poll()` with the sensor running
- **THEN** the bridge SHALL read the sensor and log "chip temperature <t> °C" with one decimal place

### Requirement: Warning at 85 °C, cleared below 80 °C
The bridge SHALL enter the hot state when a reading is at or above 85 °C and leave it only when a reading is below
80 °C. Entering SHALL log a warning "chip too hot: <t> °C (warns at 85 °C); check the case is not in the sun or by a
heater"; leaving SHALL log "chip temperature back to <t> °C". Readings in between SHALL log nothing.

**Source**: `firmware/main/temp_alarm.h` `TEMP_ALARM_WARN_C`, `TEMP_ALARM_CLEAR_C`; `firmware/main/temp_alarm.c` `temp_alarm_step()`; `firmware/main/temp_mon.c` `temp_mon_poll()`; `firmware/test_host/test_main.c` `test_temp_alarm()` (tests the logic with other thresholds)

#### Scenario: Reading crosses 85 °C
- **WHEN** the bridge is not hot and a reading of 85.0 °C arrives
- **THEN** it SHALL become hot and log the "chip too hot" warning once

#### Scenario: Reading hovers near the line
- **WHEN** the bridge is hot and readings stay between 80 °C and 85 °C
- **THEN** it SHALL stay hot and log nothing more

#### Scenario: Cooling down
- **WHEN** the bridge is hot and a reading below 80 °C arrives
- **THEN** it SHALL leave the hot state and log "chip temperature back to <t> °C"

### Requirement: Highest reading since boot
The bridge SHALL keep the latest reading, the highest reading since boot and the hot flag, and SHALL report them as
invalid until the first good reading.

**Source**: `firmware/main/temp_mon.h` `temp_mon_status_t`; `firmware/main/temp_mon.c` `temp_mon_poll()`, `temp_mon_get_status()`

#### Scenario: Reading falls after a peak
- **WHEN** readings of 60 °C and then 57 °C arrive
- **THEN** the latest SHALL be 57 °C and the highest 60 °C

### Requirement: Temperature in the status API and on the Status page
`GET /api/status` SHALL include `chip_temp` as `{"c", "max_c", "hot", "warn_c"}` with one decimal place for `c` and
`max_c`, or `null` before the first reading. The Status page SHALL show a Chip temperature row "<c> °C (highest
since start <max> °C)", and while hot add "TOO HOT: at or above 85 °C. Check the case is not in the sun or by a heater".

**Source**: `firmware/main/web.c` `status_get()` (`temp_json`, `TEMP_ALARM_WARN_C`), Status page script `row('Chip temperature', ...)`

#### Scenario: No reading yet
- **WHEN** a client reads `/api/status` before the first good reading or with the sensor not running
- **THEN** `chip_temp` SHALL be `null` and the Status page SHALL show no Chip temperature row

#### Scenario: Bridge too hot
- **WHEN** the hot flag is set
- **THEN** the Status page SHALL show the reading with the "TOO HOT" line

## Notes
- The code sets no resolution of its own. The 20 to 100 °C range selects the driver's range that covers the warning
  level (comment in `temp_mon_init()`); the values are shown to one decimal place. In practice the sensor was seen to
  move in whole-degree steps (`docs/papercuts/2026-10-18-main-tsens-reads-in-1c-steps.md`).
- The sensor reads the die, which runs above the air in the case: about 56 °C in the case on a normal day, per the
  `temp_alarm.h` comment. The 85 °C level is chosen for the MAX3232 module and the printed case, not the chip.
- The bridge only warns; it takes no other action when hot.
