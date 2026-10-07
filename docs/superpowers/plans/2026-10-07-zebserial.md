# ZebSerialWifi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firmware that bridges TCP port 9100 to a Zebra GK420t's RS232 port, running on a XIAO ESP32-C6, plus a
printed snap-fit enclosure.

**Architecture:**
- **Firmware:** ESP-IDF C application split into small units: board, zpl_status (pure), printer_link, bridge, wifi_prov,
  web, settings. The pure parsing code has host unit tests. Everything that touches IDF is checked by `idf.py build`
  and then on the bench.
- **Enclosure:** one parameterised OpenSCAD file that exports a tray and a lid.

**Tech Stack:** ESP-IDF v6.1 (`~/esp/esp-idf-v6.1`), target esp32c6, C11, cc for host tests, OpenSCAD 2021+ (Homebrew).

**Spec:** `docs/superpowers/specs/2026-10-07-zebserial-design.md`

## Global Constraints

- **Pins:**
  - UART1 on GPIO22/GPIO23. Default TX=22 and RX=23; at boot, the pin that reads high is RX.
  - GPIO3 low (RF switch on), GPIO14 low (ceramic antenna).
  - LED GPIO15, active low. BOOT button GPIO9.
- **Serial:** baud candidates in order: saved, 115200, 9600, 19200, 38400, 57600. 8N1, XON/XOFF.
- **TCP:** port 9100, one client, 30 s idle timeout.
- **Hostname:** mDNS `zebserial`, service `_pdl-datastream._tcp`.
- **SoftAP:** SSID `ZebSerial-XXXX`, open, at 192.168.4.1.
- **Factory reset:** hold BOOT for 5 s to clear WiFi.
- **Code style:**
  - 4-space indent, 120-column lines.
  - Statics use the `s_` prefix; public functions use the `<unit>_` prefix.
  - No exclamation marks in log strings or comments.
- **Commits:** none of them carry AI attribution lines.

## Review Focus

1. **Printer off or in EPL mode at boot.** Detection fails, and the bridge must still forward at the last saved baud.
   Covered by `printer_link` falling back.
2. **A client disconnects mid-job while the printer holds XOFF.** The `uart_write_bytes` call must not wedge the bridge
   forever: use bounded waits and `uart_flush`.
3. **A `~HS` reply split across reads, or with noise or leading bytes.** The parser must find the STX…ETX frames
   anywhere in the buffer. Host test `test_hs_with_leading_garbage`.
4. **Wrong-baud garbage that happens to contain 0x02 and 0x03.** The parser must reject frames that don't have the
   field counts for `~HS`. Host test `test_hs_rejects_short_frames`.
5. **An SSID or password with spaces, `+`, `%` or `&` in the setup form.** URL-decode properly. Host test for
   `url_decode`.

---

### Task 1: Pure parsers and host tests

**Files:**
- Create: `firmware/main/zpl_status.c`, `firmware/main/zpl_status.h`
- Create: `firmware/main/urlcodec.c`, `firmware/main/urlcodec.h`
- Test: `firmware/test_host/test_main.c`, `firmware/test_host/Makefile`

**Interfaces:**
- Produces:
  - `bool zpl_hs_parse(const uint8_t *buf, size_t len, zpl_hs_t *out)`: returns true when it finds 3 STX…ETX frames
    with the documented field counts (string 1: 12 fields, string 2: 11 fields, string 3: 2 fields).
  - `zpl_hs_t` fields:
    - `paper_out`, `pause`, `head_up`, `ribbon_out`, `under_temp`, `over_temp`, `buffer_full`, `corrupt_ram`
      (all `bool`)
    - `formats_in_buffer` (`int`)
  - `bool zpl_sgd_value(const uint8_t *buf, size_t len, char *out, size_t out_len)`: extracts the first `"..."` quoted
    value.
  - `size_t url_decode(const char *in, char *out, size_t out_len)` and
    `bool form_get(const char *body, const char *key, char *out, size_t out_len)`.
  - `void zpl_baud_candidates(uint32_t saved, uint32_t out[6], size_t *n)`: puts the saved baud first, removes
    duplicates, and rejects saved values that aren't in the standard list.

The `~HS` format comes from the ZPL II Programming Guide, `~HS` command:
- **String 1:** `aaa,b,c,dddd,eee,f,g,h,iii,j,k,l`. b = paper out, c = pause, eee = formats in buffer, f = buffer
  full, j = corrupt RAM, k = under temperature, l = over temperature.
- **String 2:** `mmm,n,o,p,q,r,s,t,uuuuuuuu,v,www`. o = head up, p = ribbon out.
- **String 3:** `xxxx,y`.

Test fixtures are typed from that documented layout.

- [ ] Write `test_main.c` with these tests:
  - `test_hs_ok` covers all flags clear, plus paper-out and head-up set.
  - `test_hs_with_leading_garbage`
  - `test_hs_rejects_short_frames`
  - `test_hs_needs_three_frames`
  - `test_sgd_value`
  - `test_url_decode` covers `a+b%26c%25` → `a b&c%`.
  - `test_form_get` covers the first, middle and missing key cases.
  - `test_baud_candidates` covers saved=9600, saved=0 and saved=12345.
- [ ] Run `make -C firmware/test_host` and confirm it fails because the symbols are missing.
- [ ] Implement both units.
- [ ] Run `make -C firmware/test_host` and confirm all tests pass.
- [ ] Commit: `git commit -m "Add ZPL status, SGD and form parsers with host tests" -- firmware/main/zpl_status.* firmware/main/urlcodec.* firmware/test_host`

### Task 2: IDF project skeleton, board, settings

**Files:**
- Create: `firmware/CMakeLists.txt`, `firmware/main/CMakeLists.txt`, `firmware/main/idf_component.yml` (adds
  `espressif/mdns`)
- Create: `firmware/sdkconfig.defaults`, `firmware/main/Kconfig.projbuild`
- Create: `firmware/main/board.c/.h`, `firmware/main/settings.c/.h`, `firmware/main/main.c`

**Interfaces:**
- `void board_init(void)` sets up the RF switch, antenna select, LED and button.
- `void board_detect_uart_pins(int *tx, int *rx)` uses pull-down inputs; the pin reading high is RX.
- `void board_led_set_mode(board_led_mode_t m)`, where the modes are `LED_AP`, `LED_CONNECTING`, `LED_READY`,
  `LED_NO_PRINTER`.
- `void board_led_activity(void)`
- `bool board_button_held(uint32_t ms)` is polled by a task in main.
- **settings:**
  - `esp_err_t settings_init(void)`
  - `bool settings_get_wifi(char ssid[33], char pass[65])`
  - `esp_err_t settings_set_wifi(const char *ssid, const char *pass)`
  - `esp_err_t settings_clear_wifi(void)`
  - `uint32_t settings_get_baud(void)` returns 0 when unset.
  - `esp_err_t settings_set_baud(uint32_t)`
- The build stamp comes from `PROJECT_VER`, set from `git describe --always --dirty`, plus `__DATE__ __TIME__`, logged
  at boot.
- `sdkconfig.defaults`:
  - `CONFIG_IDF_TARGET="esp32c6"`
  - `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`
  - `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`
  - `CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024`
  - `CONFIG_LWIP_MAX_SOCKETS=16`

- [ ] Write the files. `main.c` initialises NVS, settings and board, then logs the pins and the build stamp.
- [ ] `idf.py set-target esp32c6 build` passes.
- [ ] Commit with pathspec.

### Task 3: printer_link

**Files:** `firmware/main/printer_link.c/.h`

**Interfaces:**
- `esp_err_t printer_link_init(int tx, int rx)` installs UART1 with a 4 KB RX buffer and an 8 KB TX buffer, and sets
  `uart_set_sw_flow_ctrl(UART_NUM_1, true, 0, 0)`. The thresholds only matter for the RX side, since the ESP never sends
  XOFF, so they are passed as 0 with RX-side XOFF disabled. Check this against the IDF 6.1 API.
- `bool printer_link_detect(void)` runs the scan, handshake fix and raise to 115200 from the spec. It holds a mutex so
  it never runs while the bridge holds the link.
- `bool printer_link_acquire(TickType_t)` and `void printer_link_release(void)` are used by the bridge for exclusive
  access.
- `int printer_link_write(const uint8_t *d, size_t n, TickType_t to)` returns the number of bytes queued. It uses
  `uart_write_bytes` after waiting for space via `uart_get_tx_buffer_free_size` with a timeout.
- `int printer_link_read(uint8_t *d, size_t n, TickType_t to)`
- `void printer_link_get_status(printer_status_t *s)` returns the baud, `detected`, the last `zpl_hs_t`, a timestamp,
  and the tx/rx byte counters.
- `void printer_link_request_redetect(void)` sets a flag that a background task handles while the link is idle.

- [ ] Implement it.
- [ ] Build passes.
- [ ] Commit.

### Task 4: bridge

**Files:** `firmware/main/bridge.c/.h`

**Interfaces:**
- `void bridge_start(void)` starts a listener task on port 9100.
- **Per client:**
  - Acquire the link.
  - The socket→UART loop uses `recv` with `SO_RCVTIMEO` of 1 s, tracking idle time up to 30 s.
  - A UART→socket task runs with `printer_link_read` at 50 ms.
  - On close, release the link.
- A second client gets `accept` then `close`.
- `void bridge_get_stats(bridge_stats_t *s)` returns sessions, the active flag, the client IP, and bytes in and out.
- On an XOFF stall longer than 30 s with no progress, the client is dropped and the UART TX is flushed. This covers
  Review Focus 2.

- [ ] Implement it.
- [ ] Build passes.
- [ ] Commit.

### Task 5: wifi_prov and web

**Files:** `firmware/main/wifi_prov.c/.h`, `firmware/main/web.c/.h`, `firmware/main/dns_hijack.c/.h`

**Interfaces:**
- `void wifi_prov_start(void)` starts STA if credentials are saved, with SoftAP fallback after 60 s; with no
  credentials it goes straight to APSTA.
- `wifi_prov_state_t wifi_prov_state(void)` returns `WIFI_ST_AP`, `WIFI_ST_CONNECTING` or `WIFI_ST_CONNECTED`.
- `int wifi_prov_rssi(void)` and `void wifi_prov_ip(char out[16])`.
- `int wifi_prov_scan(wifi_ap_record_t *out, int max)` is used by the setup page.
- **dns_hijack:** a UDP port 53 task that answers every A query with 192.168.4.1, running only in AP mode.
- **web routes:**
  - `GET /`: the setup form in AP mode, otherwise the status page.
  - `POST /save`: uses `form_get`, then `settings_set_wifi`, then reboots.
  - `GET /api/status`: returns JSON.
  - `POST /redetect`
  - `POST /forget`
  - A catch-all 302 redirect to `/` while in AP mode.
- **mDNS** is started in main once there's an IP.

- [ ] Implement it.
- [ ] Build passes.
- [ ] Commit.

### Task 6: main wiring, LED state, button

- Init order:
  1. NVS
  2. settings
  3. board (detect pins)
  4. `printer_link_init`
  5. `wifi_prov_start`
  6. `web_start`
  7. `bridge_start`
  8. A detect task: an initial detect, then every 10 s until detected.
  9. A status task: LED mode from the wifi and printer state, and a BOOT hold of 5 s calls `settings_clear_wifi` and
     reboots.
- mDNS is set up on the IP event: hostname, `_pdl-datastream._tcp` on 9100 with TXT records, and `_http._tcp` on 80.

- [ ] Implement it.
- [ ] Build passes.
- [ ] Commit.

### Task 7: Bench verification (needs the user for hardware steps)

1. Delete `build/`, rebuild, flash `/dev/cu.usbmodem1201` after confirming the chip is an ESP32-C6, and check the boot
   log's build stamp against `git describe`.
2. Pin detection is logged. Provision from a phone or the Mac via SoftAP.
3. DB9 loopback (pins 2 and 3 bridged): `printf 'hello' | nc -w2 zebserial.local 9100` echoes `hello`.
4. Printer connected: the boot log shows the baud, and `~HS` is decoded. Then `printf '~HS' | nc -w2 zebserial.local 9100`.
5. Add the printer on macOS and print from Label LIVE.

### Task 8: Enclosure (independent, parallel)

**Files:** `enclosure/zebserial_case.scad`, `enclosure/README.md` (fit-check list), `enclosure/out/{tray,lid}.stl`,
`enclosure/renders/*.png`

- All dimensions are top-level parameters, grouped as given, measured, or standard defaults, as in the spec table.
- `part = "tray" | "lid" | "assembly"`, and `show_ghosts = true` draws the DB9, RS232 PCB and XIAO.
- Exported from the command line: `openscad -D 'part="tray"' -o out/tray.stl zebserial_case.scad`, and the same for
  the lid.
- PNG renders of the assembly, an exploded view and a section, inspected visually before reporting.
- Commit with pathspec.
