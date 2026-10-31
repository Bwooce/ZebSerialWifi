# printer-link Specification

## Purpose
The printer link owns the RS-232 serial line from the bridge to the Zebra printer: UART set-up and pin selection,
baud detection, moving the printer to XON/XOFF and a target baud, language detection, status polling with `~HS`,
the `^HH` configuration cache, and the exclusive byte I/O API that the TCP bridge and web pages use. It lives in
`firmware/main/printer_link.c` and `printer_link.h`, with the pure reply parsers in `firmware/main/zpl_status.c`
and the boot-time pin probe in `firmware/main/board.c`.

## Requirements

### Requirement: UART configuration
The bridge SHALL drive the printer on UART1 at 8 data bits, no parity, 1 stop bit, with hardware flow control off,
a 4096-byte receive buffer and an 8192-byte transmit buffer. It SHALL start at the baud saved in NVS key `baud`,
or 9600 (`PRINTER_DEFAULT_BAUD`) when none is saved.

**Source**: `firmware/main/printer_link.c` `uart_setup()`, `printer_link_init()`, `PORT`, `RX_BUF`, `TX_BUF`; `firmware/main/printer_link.h` `PRINTER_DEFAULT_BAUD`; `firmware/main/settings.c` `settings_get_baud()`

#### Scenario: First boot
- **WHEN** no baud has been saved in NVS
- **THEN** the UART SHALL start at 9600 baud, 8N1

#### Scenario: Saved baud
- **WHEN** NVS key `baud` holds 115200
- **THEN** the UART SHALL start at 115200 baud

### Requirement: Software flow control in the UART
The bridge SHALL enable the UART's XON/XOFF software flow control with an XON threshold of 32 bytes and an XOFF
threshold of 120 bytes, and a receive FIFO full threshold of 64 bytes, so that XON/XOFF received from the printer
pauses and resumes transmission in hardware. Flow control SHALL be off only while the line is known to echo.

**Source**: `firmware/main/printer_link.c` `uart_setup()`, `XON_THRESH`, `XOFF_THRESH`, `RX_FULL_THRESH`, `s_echo`

#### Scenario: Printer sends XOFF
- **WHEN** the printer sends XOFF while a job is being sent
- **THEN** the UART SHALL stop transmitting until the printer sends XON

### Requirement: Internal pull-up on RX
The bridge SHALL enable the internal pull-up on whichever GPIO is the UART RX pin, at UART set-up and every time
the pins are moved or swapped, so that RX idles at mark behind a Schottky level-shift diode or with the module
unpowered.

**Source**: `firmware/main/printer_link.c` `uart_setup()`, `swap_pins()`, `printer_link_set_pins()` (`gpio_set_pull_mode(s_rx_gpio, GPIO_PULLUP_ONLY)`)

#### Scenario: Pins swapped
- **WHEN** detection swaps TX and RX
- **THEN** the pull-up SHALL be enabled on the new RX GPIO

### Requirement: Pin selection at boot
The bridge SHALL use TX GPIO21 (D3) and RX GPIO23 (D5) when the pins setting is `SETTINGS_PINS_TX_D3`, TX GPIO23
and RX GPIO21 when it is `SETTINGS_PINS_TX_D5`, and otherwise run `board_detect_uart_pins()`. Swapping the pins
during detection SHALL be allowed only when the pins came from an inconclusive probe, not when they were fixed or
detected.

**Source**: `firmware/main/main.c` `app_main()`; `firmware/main/printer_link.c` `printer_link_allow_pin_swap()`, `s_pin_swap`

#### Scenario: Pins fixed by setting
- **WHEN** the pins setting is `SETTINGS_PINS_TX_D5`
- **THEN** the UART SHALL use TX GPIO23 and RX GPIO21
- **AND** detection SHALL NOT swap the pins

### Requirement: Passive pin probe
In auto mode the bridge SHALL sample GPIO21 and GPIO23 20 times each with a pull-up and with a pull-down, while
the other pin is pulled up, and class each pin as driven high, driven low, floating or toggling. When one pin is
driven high and the other is neither driven high nor floating, the high pin SHALL be RX and the other TX, and the
pins count as detected.

**Source**: `firmware/main/board.c` `board_detect_uart_pins()`, `probe_pin_other_high()`, `probe_pin()`, `sample_high()`, `GPIO_UART_A`, `GPIO_UART_B`

#### Scenario: R1OUT idles high on D3
- **WHEN** GPIO21 reads high with both pulls and GPIO23 reads driven low
- **THEN** RX SHALL be GPIO21 and TX GPIO23, with no pin swap allowed

### Requirement: Loopback pin probe and defaults
When both pins read floating, the bridge SHALL drive each pin in turn (high, low, high, 5 ms each) while sensing
the other with its pull-up, and SHALL take the pins as detected only if exactly one direction follows. Otherwise,
including one pin high and one floating, it SHALL use the defaults `CONFIG_ZEB_UART_TX_GPIO` (23, D5) and
`CONFIG_ZEB_UART_RX_GPIO` (21, D3) and allow a swap.

**Source**: `firmware/main/board.c` `board_detect_uart_pins()`, `probe_drive_sense()`; `firmware/main/Kconfig.projbuild` `ZEB_UART_TX_GPIO`, `ZEB_UART_RX_GPIO`

#### Scenario: Loopback plug fitted at boot
- **WHEN** both pins float and driving GPIO23 makes GPIO21 follow, but driving GPIO21 does not move GPIO23
- **THEN** TX SHALL be GPIO23 and RX GPIO21, with no pin swap allowed

#### Scenario: Inconclusive probe
- **WHEN** GPIO21 reads driven high and GPIO23 reads floating
- **THEN** the bridge SHALL use TX GPIO23 and RX GPIO21 and allow a swap

### Requirement: Pin swap fallback
When a baud scan finds no printer and a swap is allowed, the bridge SHALL swap TX and RX and scan again. If that
also fails it SHALL swap back and return to the fallback baud. Once a printer answers while a swap is allowed, the
bridge SHALL keep the current pins and allow no further swap until reboot.

**Source**: `firmware/main/printer_link.c` `detect()`, `swap_pins()`, `s_pin_swap`

#### Scenario: Wires crossed
- **WHEN** no printer answers on the default pins and a swap is allowed
- **THEN** the bridge SHALL scan again with TX and RX swapped
- **AND** SHALL keep the swapped pins if the printer answers there

### Requirement: Baud candidate order
The bridge SHALL scan the bauds in this order: the saved baud if it is one of 115200, 9600, 19200, 38400 or 57600,
then the rest of 115200, 9600, 19200, 38400, 57600, without duplicates (always 5 candidates). When no candidate
answers, the UART SHALL return to the saved baud, or 9600 if none is saved.

**Source**: `firmware/main/zpl_status.c` `zpl_baud_candidates()`; `firmware/main/printer_link.c` `scan()`, `fallback_baud()`; `firmware/test_host/test_main.c` `test_baud_candidates()`

#### Scenario: Saved 9600
- **WHEN** the saved baud is 9600
- **THEN** the order SHALL be 9600, 115200, 19200, 38400, 57600

#### Scenario: Non-standard saved baud
- **WHEN** the saved baud is 12345 or none is saved
- **THEN** the order SHALL be 115200, 9600, 19200, 38400, 57600

### Requirement: Probing each baud
At each candidate the bridge SHALL send `~HS` (unless the language is already known not to be ZPL) and wait up to
500 ms for a reply that parses. Failing that it SHALL send `\r\n! U1 getvar "device.languages"\r\n` and wait up
to 500 ms. If SGD answers with a language that is not classed as non-ZPL, it SHALL send `~HS` once more.

**Source**: `firmware/main/printer_link.c` `scan()`, `probe_hs()`, `sgd_get()`, `PROBE_TIMEOUT_MS`

#### Scenario: ZPL printer at 9600
- **WHEN** the printer answers `~HS` at 9600
- **THEN** the printer SHALL be found at 9600 with a valid `~HS`

#### Scenario: SGD answers but ~HS is silent
- **WHEN** a printer with paper out answers the SGD getvar but neither `~HS`
- **THEN** the printer SHALL be found at that baud through SGD only

### Requirement: Baud change only with an empty transmit queue
Before changing the UART baud the bridge SHALL wait up to 2 s for queued output to finish, discard it (reinstalling
the driver) if it has not, then change the baud, wait 20 ms and flush the receive buffer.

**Source**: `firmware/main/printer_link.c` `set_baud()`, `printer_link_abort_tx()`

#### Scenario: Printer holds XOFF during a baud change
- **WHEN** bytes are still queued 2 s after a baud change was requested
- **THEN** they SHALL be discarded before the new baud is set

### Requirement: Loopback echo handling
When the reply to an SGD getvar starts, after any CR or LF, with the getvar command itself, the bridge SHALL treat
the line as echoing and turn XON/XOFF off. It SHALL turn XON/XOFF back on when a detection succeeds. SGD reply
parsing SHALL skip lines that start with `!`.

**Source**: `firmware/main/printer_link.c` `sgd_get()`, `detect()`, `s_echo`; `firmware/main/zpl_status.c` `zpl_sgd_value()`; `firmware/test_host/test_main.c` `test_sgd_value()`

#### Scenario: DB9 loopback plug
- **WHEN** `! U1 getvar "device.languages"` comes back unchanged
- **THEN** XON/XOFF SHALL be switched off and no value SHALL be read from the echo

### Requirement: ~HS reply parsing
The bridge SHALL accept a `~HS` reply only as three consecutive STX...ETX frames of 12, 11 and 2 comma-separated
fields, each field at most 11 printable characters, with the decoded fields numeric. Flags SHALL be true when
non-zero. Partial format SHALL be true only when string 1 field h is 1, and DTR handshake when bit 0x80 of field
aaa is set.

**Source**: `firmware/main/zpl_status.c` `zpl_hs_parse()`, `split_frame()`, `decode()`, `HS_MAX_FIELDS`, `HS_FIELD_LEN`; `firmware/test_host/test_main.c` `test_hs_ok()`, `test_hs_handshake_bit()`, `test_hs_partial_format()`, `test_hs_rejects_short_frames()`, `test_hs_needs_three_frames()`, `test_hs_multi_digit_flags()`

#### Scenario: Leading noise
- **WHEN** line noise and a stray STX precede a well-formed reply
- **THEN** the reply SHALL still parse

#### Scenario: Two frames only
- **WHEN** only strings 1 and 2 arrive
- **THEN** the parse SHALL fail

### Requirement: Partial format recovery at detection
When the `~HS` read during detection shows a partial format open and no formats in the buffer, the bridge SHALL
send `~JA` to cancel it and read `~HS` again after 100 ms. If formats are queued it SHALL leave them alone.

**Source**: `firmware/main/printer_link.c` `clear_partial_format()`

#### Scenario: Reset noise left a format open
- **WHEN** `~HS` shows partial format set and 0 formats in buffer
- **THEN** the bridge SHALL send `~JA`, not `^XZ`

### Requirement: Language detection
After a printer is found the bridge SHALL read SGD `device.languages`. A value containing `zpl` in any case SHALL
be ZPL; an empty value, `?` or no reply SHALL be unknown, and taken as ZPL when the printer answered `~HS`; any
other value SHALL be non-ZPL. While the language text is empty, up to 20 later polls SHALL read it again when
`~HS` shows no partial format and no formats queued.

**Source**: `firmware/main/printer_link.c` `probe_language()`, `retry_language()`, `LANG_RETRIES`; `firmware/main/zpl_status.c` `zpl_language_kind()`; `firmware/test_host/test_main.c` `test_language_kind()`

#### Scenario: Automatic language
- **WHEN** `device.languages` is `epl_zpl`
- **THEN** the language SHALL be ZPL

#### Scenario: EPL mode
- **WHEN** `device.languages` is `epl`, `cpcl` or `line_print`
- **THEN** the language SHALL be non-ZPL

### Requirement: Language gate
For a printer whose language is non-ZPL the bridge SHALL send no ZPL of its own: no `~HS`, `~HI`, `^HH` or `^SC`,
and no port configuration. Probes SHALL use SGD only, `printer_link_refresh_config()` and `printer_link_send_zpl()`
SHALL return `ESP_ERR_NOT_SUPPORTED`, and the status field `zpl` SHALL be false. A failed detection SHALL reset the
language to unknown.

**Source**: `firmware/main/printer_link.c` `detect()`, `scan()`, `probe_alive_publish()`, `read_model()`, `read_handshake()`, `printer_link_refresh_config()`, `printer_link_send_zpl()`, `printer_link_get_status()`

#### Scenario: EPL printer
- **WHEN** a printer reporting `epl` is detected
- **THEN** the bridge SHALL NOT configure its serial port
- **AND** a web request to send ZPL SHALL get `ESP_ERR_NOT_SUPPORTED`

### Requirement: Model read
The bridge SHALL read the model with `~HI` (1000 ms timeout) for a printer not classed as non-ZPL, taking the
first field of the STX...ETX reply without any `-<n>dpi` suffix, then fall back to SGD `device.product_name`. A
`?` or no reply SHALL keep the previous model name.

**Source**: `firmware/main/printer_link.c` `read_model()`, `HI_TIMEOUT_MS`; `firmware/main/zpl_status.c` `zpl_hi_model()`; `firmware/test_host/test_main.c` `test_hi_model()`

#### Scenario: GX430t
- **WHEN** `~HI` returns `<STX>GX430t-300dpi,V56.17.16Z,12,2104KB<ETX>`
- **THEN** the model SHALL be `GX430t`

### Requirement: Handshake read
The bridge SHALL read the printer's handshake from SGD `comm.handshake` when it gives a non-empty value other than
`?`; else from bit 0x80 of the `~HS` from this detection (`DTR` when set, `XON/XOFF` when clear); else, for a
printer not classed as non-ZPL, from the line whose name contains `HANDSHAKE` in the `^XA^HH^XZ` output.

**Source**: `firmware/main/printer_link.c` `read_handshake()`; `firmware/main/zpl_status.c` `zpl_pick_handshake()`, `zpl_hh_handshake()`; `firmware/test_host/test_main.c` `test_pick_handshake()`, `test_hh_handshake()`, `test_hh_handshake_fixture()`

#### Scenario: GX430t answers ? to comm.handshake
- **WHEN** SGD returns `?` and `~HS` field aaa is 030
- **THEN** the handshake SHALL read `XON/XOFF`

### Requirement: Setting XON/XOFF with ^SC
For a ZPL printer found through `~HS`, the bridge SHALL send `^XA^SC<baud>,8,N,1,X,N^JUS^XZ` at the current baud
when the handshake is not XON/XOFF alone, or when it cannot be read and NVS flag `sc_sent` is not set. XON/XOFF
alone SHALL mean the value contains `xon` and none of `dtr`, `dsr`, `rts` or `cts`, in any case.

**Source**: `firmware/main/printer_link.c` `configure_printer()`; `firmware/main/zpl_status.c` `zpl_sc_command()`, `zpl_handshake_is_xon()`; `firmware/test_host/test_main.c` `test_sc_command()`, `test_hh_handshake()`

#### Scenario: Combined G-series mode
- **WHEN** the handshake reads `DTR & XON/XOFF`
- **THEN** the bridge SHALL send `^SC` with handshake `X` at the current baud

#### Scenario: Already XON/XOFF
- **WHEN** the handshake reads `XON/XOFF`
- **THEN** no `^SC` SHALL be sent for the handshake

### Requirement: Checking an ^SC
After each `^SC` the bridge SHALL wait up to 1 s for it to leave the UART, wait 1500 ms, move the UART to the
`^SC` baud, and check for the printer up to 3 times, 500 ms apart. When the handshake `^SC` is answered it SHALL
set NVS flag `sc_sent` and read the handshake again; when it is not, it SHALL rescan the bauds, and the detection
fails if that finds nothing.

**Source**: `firmware/main/printer_link.c` `send_sc()`, `configure_printer()`

#### Scenario: Printer silent after ^SC
- **WHEN** the printer does not answer after the handshake `^SC`
- **THEN** the bridge SHALL scan all candidate bauds again

### Requirement: Moving the printer to the target baud
After the handshake step, when the target baud is non-zero, differs from the current baud and no raise has been
tried since the last re-detect request, the bridge SHALL send an `^SC` at the new baud. A target below the current
baud SHALL be used as given; otherwise the bridge SHALL try the highest rung of 115200, 57600, 38400, 19200 that is
at most the target, above the current baud and below the model's cap.

**Source**: `firmware/main/printer_link.c` `configure_printer()`, `s_raise_tried`, `s_target_baud`; `firmware/main/zpl_status.c` `zpl_next_raise()`; `firmware/test_host/test_main.c` `test_next_raise()`

#### Scenario: Raise from 9600
- **WHEN** the printer is at 9600, the target is 115200 and no cap is stored
- **THEN** the bridge SHALL send `^XA^SC115200,8,N,1,X,N^JUS^XZ`

#### Scenario: Model capped at 115200
- **WHEN** the stored cap for the model is 115200
- **THEN** the bridge SHALL try 57600 instead

### Requirement: Failed raise and the baud cap
When the printer does not answer at the new baud, the bridge SHALL look for it at the old baud; if it is silent
there too it SHALL send the reverting `^SC` at the new baud. If the printer is found at the old baud, the failed
rate SHALL be stored as the cap for the model (one model and cap in NVS keys `cap_model` and `cap_baud`) and the
next rung tried. If it is lost, the bridge SHALL rescan.

**Source**: `firmware/main/printer_link.c` `try_raise()`, `configure_printer()`; `firmware/main/settings.c` `settings_get_baud_cap()`, `settings_set_baud_cap()`

#### Scenario: Printer does not take 115200
- **WHEN** a printer at 9600 gets an `^SC` for 115200 and answers only at 9600
- **THEN** the cap SHALL be stored as 115200 for its model and 57600 tried next

### Requirement: Target baud changes
Before the link task starts, `printer_link_set_target_baud()` SHALL only store the target. Afterwards, a different
target SHALL clear the stored cap and request a re-detect, which allows a new raise. A target of 0 SHALL leave the
printer's baud alone.

**Source**: `firmware/main/printer_link.c` `printer_link_set_target_baud()`, `printer_link_request_redetect()`, `s_started`; `firmware/main/printer_link.h` `printer_status_t.target_baud`

#### Scenario: Target changed on the Bridge page
- **WHEN** the target changes from 115200 to 57600 while running
- **THEN** the stored cap SHALL be cleared and detection SHALL run again at the next idle moment

### Requirement: Detection outcome
A successful detection SHALL mark the printer detected and answering and save the current baud in NVS key `baud`.
A printer found only through SGD, and not non-ZPL, SHALL be configured at the first later poll that `~HS`
answers, followed by a model read. Detection SHALL fail when no baud answers or a configuration loses the printer.

**Source**: `firmware/main/printer_link.c` `detect()`, `poll_once()`, `s_config_pending`, `settings_set_baud()`

#### Scenario: Paper out at boot
- **WHEN** detection finds a ZPL printer through SGD only
- **THEN** it SHALL be detected without sending `^SC`
- **AND** `^SC` checks SHALL run once `~HS` answers

### Requirement: Sleep setting read
At detection the bridge SHALL read SGD `power.energy_star.enable` and `power.sleep.enable`, and SHALL treat the
printer as one that sleeps when either value is `on` in any case. `?`, `off`, empty or no reply SHALL mean it does
not sleep.

**Source**: `firmware/main/printer_link.c` `read_sleep()`, `s_sleeps`; `firmware/main/zpl_status.c` `zpl_sgd_is_on()`; `firmware/test_host/test_main.c` `test_sgd_is_on()`

#### Scenario: GX430t
- **WHEN** both reads return `?`
- **THEN** the printer SHALL be polled on the timer

### Requirement: Link task scheduling
The link task SHALL wake every 200 ms and run only when it gets the link without waiting. It SHALL detect at boot
and whenever not detected or a re-detect is requested, retrying every 10 s while none is found. A detected printer
SHALL be polled 30 s after the last poll or detection while answering and every 10 s while it is not.

**Source**: `firmware/main/printer_link.c` `link_task()`, `POLL_INTERVAL_MS`, `SCAN_RETRY_MS`, `printer_link_start()`

#### Scenario: Client printing at poll time
- **WHEN** a poll is due while a client holds the link
- **THEN** the poll SHALL wait until the link is free

### Requirement: Poll skipped while output is queued
When a poll is due but output is still queued for the printer, the bridge SHALL skip the poll without counting a
miss and SHALL send no probe for the next 10 s. A pending re-detect SHALL NOT wait for that 10 s.

**Source**: `firmware/main/printer_link.c` `link_task()`, `skip_until_us`, `printer_link_tx_idle()`

#### Scenario: Printer holding XOFF
- **WHEN** a job is stuck behind XOFF at poll time
- **THEN** no `~HS` SHALL be queued behind it and the printer SHALL stay answering

### Requirement: Liveness probe
Each poll SHALL send `~HS` (not for a non-ZPL printer) and, failing that, SGD `device.languages`. An answer to
either SHALL mark the printer answering and record the reply time; a `~HS` answer SHALL also update the cached
status and its time.

**Source**: `firmware/main/printer_link.c` `probe_alive_publish()`, `probe_hs()`, `s_answering`, `s_reply_us`, `s_hs_time_us`

#### Scenario: Head open
- **WHEN** the printer ignores `~HS` but answers SGD
- **THEN** it SHALL be reported answering, with the last `~HS` kept and its age growing

### Requirement: Silent printer handling
A detected printer that stops answering SHALL stay detected and be reported not answering. The bridge SHALL rescan
the bauds at the 3rd consecutive miss and every 30 misses after that (3, 33, 63, ...). A rescan that finds the
printer SHALL mark it answering, and SHALL trigger a full re-detect only when the baud has changed.

**Source**: `firmware/main/printer_link.c` `poll_once()`, `POLL_FAILS_BEFORE_RESCAN`, `SILENT_RESCAN_EVERY`, `s_redetect`

#### Scenario: Paper out
- **WHEN** the printer misses 3 polls in a row
- **THEN** the bridge SHALL scan the candidate bauds once
- **AND** the printer SHALL still count as detected

#### Scenario: Printer reset to another baud
- **WHEN** the rescan finds the printer at a baud other than the current one
- **THEN** the bridge SHALL run detection again

### Requirement: Sleep-aware polling
For a detected printer that sleeps and is answering, the bridge SHALL NOT poll on the timer: it SHALL poll only
after `printer_link_want_status()` was called and at least 30 s have passed since the last poll or detection, and
a completed poll SHALL clear the request. A printer that sleeps but is not answering SHALL be polled on the timer.

**Source**: `firmware/main/printer_link.c` `link_task()`, `printer_link_want_status()`, `SLEEPY_MIN_POLL_MS`, `s_status_wanted`; `firmware/main/bridge.c` (calls `printer_link_want_status()` after each job); `firmware/main/web.c` (calls it on status reads)

#### Scenario: ZD421 with Energy Star on
- **WHEN** nobody reads the status and no job runs
- **THEN** the printer SHALL NOT be sent `~HS`

#### Scenario: After a job
- **WHEN** a job ends and the bridge calls `printer_link_want_status()`
- **THEN** the printer SHALL be polled at the next idle moment at least 30 s after the last poll

### Requirement: Waking a sleeping printer
When a printer that sleeps misses the first probe of a poll, the bridge SHALL NOT report it not answering, but
SHALL wait 1000 ms and probe once more, and report the result of that second probe.

**Source**: `firmware/main/printer_link.c` `poll_once()`, `SLEEPY_WAKE_MS`

#### Scenario: First probe only wakes it
- **WHEN** the first `~HS` gets no reply and the second does
- **THEN** the printer SHALL stay reported answering

### Requirement: ^HH configuration read
`printer_link_refresh_config()` SHALL send `^XA^HH^XZ` and collect up to 8192 bytes, stopping at ETX, for at most
8192 x 10 x 1000 / baud + 2000 ms. It SHALL cache the parsed pairs with their time only when at least one line
parsed and an ETX arrived, else return `ESP_FAIL` and keep the old cache.

**Source**: `firmware/main/printer_link.c` `printer_link_refresh_config()`, `hh_timeout_ms()`, `HH_BUF`, `etx_done()`

#### Scenario: Read at 115200
- **WHEN** the printer is at 115200 baud
- **THEN** the read SHALL wait at most 2711 ms

#### Scenario: Truncated dump
- **WHEN** the dump arrives without its closing ETX
- **THEN** the call SHALL return `ESP_FAIL` and the cache SHALL be unchanged

### Requirement: ^HH refresh preconditions
`printer_link_refresh_config()` SHALL wait up to its `wait` argument for the link, returning `ESP_ERR_TIMEOUT` if
it stays held. It SHALL return `ESP_ERR_INVALID_STATE` when no printer is detected or output is still queued,
`ESP_ERR_NOT_SUPPORTED` for a non-ZPL printer, and `ESP_ERR_NO_MEM` when the parse buffer cannot be allocated.

**Source**: `firmware/main/printer_link.c` `printer_link_refresh_config()`; `firmware/main/printer_link.h`

#### Scenario: No printer
- **WHEN** the configuration is refreshed before any printer is detected
- **THEN** the call SHALL return `ESP_ERR_INVALID_STATE` without sending `^HH`

### Requirement: ^HH line parsing
The `^HH` parser SHALL split each line into value and name using the GX430t layout (two spaces, value field, name
at column 22) or, failing that, a dot leader. It SHALL keep at most 128 pairs, truncate names to 23 and values to
27 characters, reject lines with non-printable bytes, and skip a last line cut off before its line end or ETX.

**Source**: `firmware/main/zpl_status.c` `zpl_hh_parse()`, `hh_split_line()`, `for_each_line()`, `HH_NAME_COL`; `firmware/main/zpl_status.h` `ZPL_HH_MAX_PAIRS`, `ZPL_HH_NAME_LEN`, `ZPL_HH_VALUE_LEN`; `firmware/test_host/test_main.c` `test_hh_parse_fixture()`, `test_hh_parse_long()`, `test_hh_parse_edges()`

#### Scenario: GX430t fixture
- **WHEN** the 2522-byte GX430t `^HH` capture is parsed
- **THEN** it SHALL give 60 pairs, from `DARKNESS` to `EARLY WARNING`

### Requirement: Configuration cache access
`printer_link_get_config()` SHALL return a copy of the cached `^HH` pairs and their age in milliseconds, or false
when none is cached. `printer_link_invalidate_config()` SHALL drop the cache.

**Source**: `firmware/main/printer_link.c` `printer_link_get_config()`, `printer_link_invalidate_config()`, `s_cfg_time_us`

#### Scenario: Cache dropped
- **WHEN** `printer_link_invalidate_config()` has been called
- **THEN** `printer_link_get_config()` SHALL return false

### Requirement: Sending ZPL commands
`printer_link_send_zpl()` SHALL take the link (or return `ESP_ERR_TIMEOUT`), return `ESP_ERR_INVALID_STATE` when
no printer is detected and `ESP_ERR_NOT_SUPPORTED` for a non-ZPL printer, and otherwise send the text and wait up
to 3000 ms for it to leave. If it has not, it SHALL discard it and return `ESP_ERR_NOT_FINISHED`; else it SHALL
hold the link `settle_ms` longer. Once the link is taken it SHALL drop the config cache.

**Source**: `firmware/main/printer_link.c` `printer_link_send_zpl()`, `drain_tx()`, `CMD_DRAIN_MS`

#### Scenario: Printer holding XOFF
- **WHEN** the command is still queued after 3000 ms
- **THEN** it SHALL be discarded and `ESP_ERR_NOT_FINISHED` returned

### Requirement: Exclusive link access
`printer_link_acquire()` and `printer_link_release()` SHALL give one holder at a time exclusive use of the link,
and detection and polling SHALL run only while the link task holds it. `printer_link_discard_input()` SHALL flush
the receive buffer.

**Source**: `firmware/main/printer_link.c` `printer_link_acquire()`, `printer_link_release()`, `printer_link_discard_input()`, `s_lock`, `link_task()`

#### Scenario: Job in progress
- **WHEN** the TCP bridge holds the link for a job
- **THEN** no `~HS`, SGD probe or detection SHALL be sent to the printer

### Requirement: Byte I/O
`printer_link_write()` SHALL queue as many of the bytes as fit in the transmit ring less 32 bytes, retrying every
10 ms up to `wait`, and return the count queued or 0 on timeout. `printer_link_read()` SHALL return the bytes read
or 0 on timeout. Both SHALL add to 64-bit byte counters reported in the status.

**Source**: `firmware/main/printer_link.c` `printer_link_write()`, `printer_link_read()`, `TX_ITEM_OVERHEAD`, `count_tx()`, `count_rx()`

#### Scenario: Ring full behind XOFF
- **WHEN** the transmit ring stays full for longer than `wait`
- **THEN** `printer_link_write()` SHALL return 0

### Requirement: Transmit idle and abort
`printer_link_tx_idle()` SHALL be true only when the transmit ring is as empty as just after driver install and
the hardware FIFO has finished. `printer_link_abort_tx()` SHALL discard all queued output by deleting and
reinstalling the UART driver at the current baud, trying up to 3 times 50 ms apart.

**Source**: `firmware/main/printer_link.c` `printer_link_tx_idle()`, `printer_link_abort_tx()`, `s_tx_free_empty`

#### Scenario: Output held in the ring
- **WHEN** XOFF holds bytes in the driver ring while the FIFO is empty
- **THEN** `printer_link_tx_idle()` SHALL return false

### Requirement: Moving the pins at run time
`printer_link_set_pins()` SHALL return `ESP_OK` at once for the current pins. Otherwise it SHALL wait up to 30 s
for the link (`ESP_ERR_TIMEOUT` if busy), wait up to 2 s for output, release the old GPIOs, attach the UART to the
new ones with the RX pull-up, flush input, and request a re-detect.

**Source**: `firmware/main/printer_link.c` `printer_link_set_pins()`

#### Scenario: Pins changed on the Bridge page
- **WHEN** new pins are set while idle
- **THEN** the UART SHALL move to them and detection SHALL run again

### Requirement: Status report
`printer_link_get_status()` SHALL report the baud, detected, the last `~HS` and its age (-1 if none), handshake,
model, language text, `zpl` (false only for a non-ZPL language), answering and sleeps (each false unless
detected), age of the last reply (-1 if none), byte counters, TX and RX GPIOs and the target baud.

**Source**: `firmware/main/printer_link.c` `printer_link_get_status()`; `firmware/main/printer_link.h` `printer_status_t`

#### Scenario: Unknown language
- **WHEN** the language could not be read
- **THEN** `zpl` SHALL be true and `language` SHALL be empty
