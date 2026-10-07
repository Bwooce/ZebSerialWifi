# ZebSerialWifi design

Date: 2026-10-07

## Purpose

A small WiFi-to-RS232 bridge that plugs straight into the DB9 serial port of a Zebra GK420t
thermal transfer printer. The Mac prints to it as a network printer (CUPS `socket://`, so Label
LIVE sees it as a System printer), and the bridge forwards the bytes to the printer's serial port.

Success means:

- The printer appears in macOS "Add Printer" via Bonjour and prints a Label LIVE label.
- `~HS` status replies get back to the TCP client.
- Large jobs (graphics) print without dropped bytes.
- Joining a new WiFi network does not need a rebuild.
- The whole unit lives in a printed case that survives repeated USB-C plug and unplug.

## Hardware

- **Seeed XIAO ESP32-C6** on USB-C power. Pins used:
  - D4 = GPIO22 and D5 = GPIO23 for the UART.
  - GPIO3 is the RF switch enable (drive low).
  - GPIO14 selects the antenna: low for the ceramic antenna, high for U.FL.
  - GPIO15 is the user LED (active low).
  - GPIO9 is the BOOT button.
- **YL-97 RS232-TTL module** (MAX3232) with a male DB9 and a 4-pin header (VCC, GND, TXD, RXD).
  - **VCC must be on the XIAO 3V3 pad, not 5V.** The MAX3232's TTL output swings to VCC, and the C6's GPIOs are not
    5V-tolerant. The user is moving the wire.
- **GK420t serial port:** DTE with a female DB9 (pin 2 = printer RX, pin 3 = printer TX, pin 5 = GND). A direct plug-in
  needs the module's TX on pin 2.
  - Check: power the module from 3V3 with no printer attached. The pin idling at about −5 V relative to pin 5 is the
    module's TX.
  - If TX is on pin 3, a null-modem adapter is needed and the enclosure must change.
- **Flow control is XON/XOFF only**, since only TX/RX/GND are wired.
  - The C6 UART handles XON/XOFF in hardware (`uart_set_sw_flow_ctrl`).
  - Zebra's hardware handshake is DTR/DSR, so an RTS/CTS module would not help.

## Firmware

ESP-IDF v6.1 (clean checkout at `~/esp/esp-idf-v6.1`), target `esp32c6`, written in C. The project lives in `firmware/`.

### Components

| Unit | Responsibility | Depends on |
|------|----------------|------------|
| `board` | RF switch and antenna GPIOs, LED, BOOT button, and TX/RX pin detection | driver/gpio |
| `zpl_status` | Pure parser for `~HS` replies (STX…ETX framed, 3 lines) and SGD replies. No IDF dependencies. | none |
| `printer_link` | Owns UART1: baud scan, raise to 115200, set the handshake, persist the baud, and the byte I/O API | `zpl_status`, `settings` |
| `bridge` | TCP server on port 9100, one client, bidirectional, idle timeout | `printer_link` |
| `wifi_prov` | STA connect with saved credentials, otherwise SoftAP plus a captive setup page | `settings` |
| `web` | HTTP server: setup form (SoftAP), status page and JSON (STA) | `wifi_prov`, `printer_link`, `bridge` |
| `settings` | NVS wrapper: WiFi credentials, baud, pin swap, antenna | nvs_flash |
| `main` | Init order, mDNS, LED state | all |

### TX/RX pin detection

Both GPIO22 and GPIO23 are configured as inputs with pull-down. With no RS232 signal on its input, the MAX3232's
receiver input sits at 0 V through its internal 5 kΩ resistor, which reads as a high on its TTL output (R1OUT). So the
pin that reads high is RX and the other is TX. Both pins are inputs during the test, so nothing contends.

- If both pins read the same, use the saved or Kconfig default (TX = GPIO22, RX = GPIO23) and log a warning.
- When the printer is connected, its TX idles at mark (negative voltage), which also gives R1OUT high, so the test
  still works.

### Baud detection (`printer_link`)

1. **Candidate order:** the saved baud (NVS) first, then 115200, 9600, 19200, 38400, 57600. Always 8N1.
2. **Probe:** at each candidate, flush RX, send `~HS`, and wait up to 500 ms for a reply. A match is a reply containing
   STX and ETX that `zpl_status` parses as a valid host status. Garbage at the wrong baud fails the parse.
3. **Retry:** if no candidate answers, retry the whole scan every 10 s. The bridge still runs and forwards at the last
   saved baud, so a printer in EPL mode or switched off is not locked out.
4. **Set the handshake:**
   - Once a baud matches, send `! U1 getvar "comm.handshake"`.
   - If the reply is not `xon/xoff`, send `! U1 setvar "comm.handshake" "xon/xoff"`.
5. **Raise to 115200:**
   - If the matched baud is not 115200, send `! U1 setvar "comm.baud" "115200"`.
   - Wait for the TX FIFO to drain plus 1 s.
   - Switch the UART to 115200 and probe again with `~HS`.
   - If that succeeds, save 115200. If it fails, rescan from step 1, since the printer may need a power cycle.
6. **When detection runs:** only at boot, when manually triggered from the web page, and after 3 consecutive bridge
   sessions with no printer bytes and a failed `~HS`. Never while a TCP client is connected.

### Bridge

- **Listener:** `bridge` listens on port 9100 and accepts one client at a time. A second connection is accepted and
  closed straight away; the macOS CUPS backend retries.
- **Mac → printer:** the bridge reads up to 1 KB from the socket and writes to the UART with `uart_write_bytes`, which
  blocks while the printer holds XOFF. Because it stops reading the socket while blocked, TCP backpressure reaches the
  sender and no data is dropped.
- **Printer → Mac:** a separate task reads the UART and sends any bytes to the current client. Printer bytes with no
  client are discarded.
- **Idle timeout:** 30 s with no traffic in either direction, then the client is closed.
- **Counters:** bytes each way, sessions, and the last client IP, all reported to the status page.

### WiFi and provisioning

- **Boot:** if credentials are saved in NVS, connect as STA. If no IP arrives within 60 s, start SoftAP and keep
  retrying STA in the background.
- **SoftAP:** SSID `ZebSerial-XXXX` (last 2 MAC bytes), open network. A DNS hijack sends every name to 192.168.4.1 so the
  captive page pops up. The form lists scanned SSIDs plus a password field; on save, credentials go to NVS and the board
  reboots.
- **Factory reset:** hold BOOT for 5 s to clear WiFi credentials and reboot. A short press does nothing.
- **mDNS:**
  - Hostname `zebserial`.
  - Services:
    - `_pdl-datastream._tcp` on port 9100, with TXT `ty=Zebra GK420t`, `product=(GK420t)`, `note=ZebSerialWifi`.
    - `_http._tcp` on port 80.

### Status LED

The user LED on GPIO15 is active low.

| LED | Meaning |
|-----|---------|
| Fast blink | SoftAP setup mode |
| Slow blink | Connecting to WiFi |
| Solid on | Connected and printer detected |
| Double blink every 2 s | Connected but no printer detected |
| Flicker | Data flowing |

### Web status page (STA mode)

- **Page `/`:** IP, RSSI, detected baud, pin mapping, printer status (decoded from the last `~HS`: paper out, pause,
  head up, ribbon out), bridge counters, uptime, and firmware version with build stamp.
- **Buttons:** "Re-detect printer" and "Forget WiFi".
- **JSON:** the same data at `/api/status`.

### Logging

`ESP_LOG` over the USB Serial/JTAG console. UART0 is not used, and UART1 is the printer.

### Testing

- **Host unit tests** (`firmware/test_host/`, plain C with cc, no IDF):
  - The `zpl_status` parser against real `~HS` reply samples taken from the Zebra ZPL manual's documented format (not
    values produced by our own code).
  - SGD reply parsing.
  - Candidate ordering for the baud scan.
- **Build gate:** `idf.py build` must pass. The flashed image's build stamp (git SHA plus time) must match the commit,
  checked in the boot log.
- **Bench tests, in order:**
  1. Boot with no module power: confirm the RF switch setup and SoftAP provisioning from a phone.
  2. DB9 loopback (pins 2 and 3 bridged): TCP bytes sent to port 9100 echo back. This is the positive control for the
     UART, pin detection and module.
  3. Printer connected: boot log shows the baud found and `~HS` decoded. `printf '~HS' | nc zebserial.local 9100`
     returns the status.
  4. Add the printer in macOS via Bonjour, then print a test label from Label LIVE.
  5. Large graphic job (a 4×6 inch full-page `^GF`) with no dropped bytes and XOFF observed in the counters.

## Enclosure

OpenSCAD in `enclosure/`, fully parameterised, printed in PETG on a Bambu Lab H2D with a 0.4 mm nozzle. PETG suits it
because it flexes for snap-fits without cracking.

### Measured and given dimensions (parameters)

| Parameter | Value | Source |
|-----------|-------|--------|
| RS232 board width | 31.5 mm | user |
| RS232 board depth (flange back to rear edge, excluding the connector) | 28 mm | user |
| Clearance below the board | 3 mm | user |
| Maximum height above the board | 25 mm | user |
| PCB thickness | 1.0 mm (adjustable) | user estimate |
| Flange front face to the back of the screw-hole hardware | 5.1 mm | user |
| XIAO size | 21 × 17.8 mm, 1.0 mm PCB (check against the Seeed drawing) | Seeed |
| XIAO overhang past the RS232 board's rear edge | 5 mm (adjustable, probably less) | user |
| XIAO position | centred on the DB9, face up, USB-C to the rear, bottom level with the top of the DB9 rear body | user |
| DB9 flange | 30.8 × 12.5 mm, holes 25.0 mm apart, D-shell protruding about 6 mm | standard right-angle DB9 defaults, flagged for measurement |

### Construction

- **Parts:** a tray (bottom plus walls) and a lid, held together by snap clips (cantilever latches) on the long sides.
  No screws.
- **Front:**
  - The front wall sits directly behind the DB9 flange plate. The flange and D-shell stay outside, so it plugs straight
    into the printer.
  - Two blind sockets in the front wall take the hardware behind the flange's screw holes, 5.1 mm from the flange face,
    to locate the case.
  - The front wall is split at the board plane, with the tray and lid each holding half, so it can close around the
    connector.
- **RS232 board:** sits on a ledge, with side clips gripping its edges. The 3 mm space below the board is kept clear of
  through-hole pins.
- **XIAO:**
  - Sits in a pocket in the lid's underside (or on a shelf), level with the top of the DB9 rear body.
  - Clips on the two long sides are offset along the board so they clear the soldered pads (5V/GND end and D4/D5).
  - A rear wall wraps around the USB-C receptacle with a cut-out sized for a plug overmould (about 12 × 7 mm), so a plug
    pushing in bears on the case, not the solder joints.
  - The XIAO's front edge sits against a stop so it cannot slide forward.
- **Wires:** a channel at the header end for the four wires.
- **Antenna:** no wall material over the XIAO's ceramic antenna end beyond the 1.6 mm skin. There's a U.FL fallback if
  the signal is poor.
- **Print orientation:** the tray prints floor-down and the lid top-down, both without supports. (Printing the tray open side down would make its whole floor one long bridge.)

### Tolerances (parameters)

- 0.2 mm clearance per side on PCB pockets.
- 0.3 mm on clips.
- 1.6 mm walls (four perimeters at 0.4 mm).

### Deliverables

- `enclosure/zebserial_case.scad`, with ghost PCBs that can be switched on to check clearances.
- `enclosure/out/*.stl` for the tray and lid.
- PNG renders from several angles for review before printing.
- A one-page fit-check list of the measurements to confirm with calipers.

## Changes made during implementation (2026-10-07)

- **TX/RX detection:** each pin is probed with a pull-up and then a pull-down, which classifies it as driven high,
  driven low or floating. RX is the pin driven high. When the result is inconclusive, every XIAO pad is reported.
- **Fallback baud:** if no candidate answers, the UART returns to the last saved baud, or 9600 (the GK420t factory
  default) if none is saved. It is never left on the last baud it tried.
- **SGD fallback probe:** a second probe, `! U1 getvar "device.languages"`, also counts as detection, so a printer in EPL
  mode is still found. Probe replies that don't parse are hex-dumped to the log.
- **Status polling:** instead of the "3 failed sessions" heuristic, `~HS` is polled every 30 s while no client is
  connected. 3 failed polls trigger a rescan.
- **Second client:** a second client waits in the listen backlog (4 deep) until the first finishes, rather than being
  closed. Jobs are queued instead of failing.
- **Stalls:** the fixed 30 s limit is gone. A printer holding XOFF (paper out, head up) is waited out for as long as the
  client is alive. TCP keepalive (10 s idle, 5 s interval, 3 probes) detects a dead client. The job is discarded only
  when the client is gone, or after 30 minutes.
- **WiFi bandwidth:** both interfaces are fixed at 20 MHz, so the station stays on 802.11ax while the setup AP runs.
- **Setup page scan:** the WiFi scan is cached for 60 s, with a "Scan again" link.
- **Build verification:** a build is checked by the `ELF file SHA256` line in the boot log, matched against
  `shasum -a 256 build/zebserial.elf`. The version string only updates when CMake reconfigures.
- **Partition table:** single-app large (1.5 MB) with size optimisation. The image is about 0.95 MB.

## Out of scope

- OTA updates: USB-C is accessible.
- Multiple concurrent clients and spooling.
- LPD and IPP.
- Authentication: trusted LAN only.
