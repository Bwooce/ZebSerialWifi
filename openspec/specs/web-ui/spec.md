# web-ui Specification

## Purpose
The bridge's HTTP server and its pages: Status, Printer and Bridge, the JSON API behind them, the printer settings
form and the ZPL it sends, one-shot printer actions, the test label, and the Host and cross-site guards. It lives in
`firmware/main/web.c`, with `zpl_status.c` (settings to ZPL), `zpl_units.h`, `test_label.c` and `urlcodec.c`.
The Wi-Fi set-up page and `/save`, `POST /pstprnt` and the `/update` upload internals are specified by other
capabilities; they appear here only in the route table.

## Requirements

### Requirement: HTTP server configuration
The bridge SHALL start one ESP-IDF HTTP server from `HTTPD_DEFAULT_CONFIG()` with these changes: `max_open_sockets` 4,
`stack_size` 8192 bytes, `lru_purge_enable` true and `max_uri_handlers` 24. All other fields keep the ESP-IDF 6.1
defaults, so the server listens on TCP port 80. If `httpd_start` fails the bridge MUST log "HTTP server failed to
start" and register no handlers.

**Source**: `firmware/main/web.c` `web_start()`; `HTTPD_DEFAULT_CONFIG` in ESP-IDF 6.1 `esp_http_server.h` (`.server_port = 80`, `.stack_size = 4096`)

#### Scenario: Server starts
- **WHEN** `web_start()` runs at boot
- **THEN** the HTTP server SHALL listen on port 80 with an 8192-byte task stack, at most 4 open sockets and LRU purge on
- **AND** it SHALL log "HTTP server on port 80"

#### Scenario: Handler registration fails
- **WHEN** `httpd_register_uri_handler` fails for one route
- **THEN** the bridge SHALL log "cannot register" with the URI and error name and SHALL go on registering the rest

### Requirement: Route table
The bridge SHALL register exactly the 21 URI handlers listed in the route table in Notes, each for one method, and a
404 error handler. A request for any other URI off the set-up access point MUST get HTTP 404 with the text "not
found".

**Source**: `firmware/main/web.c` `web_start()` `uris[]`, `not_found()`

#### Scenario: Unknown URL on the LAN
- **WHEN** a client on the station network requests `GET /nothing`
- **THEN** the bridge SHALL reply 404 "not found"

#### Scenario: Wrong method
- **WHEN** a client sends `GET /printer_config` (registered only for POST)
- **THEN** the bridge SHALL NOT run the POST handler

### Requirement: Set-up access point routing
For a request that arrived over the set-up access point (`wifi_prov_sock_on_ap()` true for its socket), the bridge
SHALL serve the set-up page for `GET /`, and SHALL answer every other page, API and POST route except `/save`,
`/pstprnt` and the icon routes with `302 Found` and `Location: http://192.168.4.1/`. An unregistered URI over the access
point MUST get the same redirect.

**Source**: `firmware/main/web.c` `on_ap()`, `to_setup_page()`, `AP_HOME`, `root_get()`, `not_found()`, `settings_get_page()`

#### Scenario: Printer page asked over the set-up network
- **WHEN** a phone on the set-up network requests `GET /printer`
- **THEN** the bridge SHALL reply 302 with `Location: http://192.168.4.1/`

#### Scenario: Captive-portal probe
- **WHEN** a phone on the set-up network requests `GET /generate_204`
- **THEN** the bridge SHALL reply 302 with `Location: http://192.168.4.1/`

### Requirement: Host header guard
Against DNS rebinding, the bridge SHALL accept a request off the access point only if its Host header, with any port
removed, is `192.168.4.1`, the station IPv4 address, the configured hostname or `<hostname>.local`, `zebserial` or
`zebserial.local` (names compared ignoring case), or an IPv6 literal (starts with `[` or still holds `:`). A missing
or longer than 95-byte Host header MUST fail. Requests over the access point always pass.

**Source**: `firmware/main/web.c` `host_allowed()`

#### Scenario: Rebound name refused
- **WHEN** a browser sends `GET /api/status` with `Host: evil.example:80` to the station address
- **THEN** the bridge SHALL reply 403 "cross-site request refused"
- **AND** it SHALL log "refused request with untrusted Host header: evil.example"

#### Scenario: Custom hostname accepted
- **WHEN** the hostname is `labels` and a request has `Host: LABELS.local`
- **THEN** the Host check SHALL pass

### Requirement: Routes the Host guard covers
The bridge SHALL apply the Host guard to `GET /`, `/api/status`, `/api/printer_config`, `/printer` and `/bridge`, and,
through the cross-site guard, to every POST route except `/pstprnt`. It MUST NOT apply it to `/favicon.svg`,
`/favicon.ico`, `/apple-touch-icon.png`, `/apple-touch-icon-precomposed.png` or `/settings`. A refused request gets
HTTP 403 with the text "cross-site request refused".

**Source**: `firmware/main/web.c` `root_get()`, `status_get()`, `printer_config_get()`, `printer_get_page()`, `bridge_get_page()`, `same_origin()`, `cross_site()`, `favicon_get()`, `icon_get()`

#### Scenario: Icon with any Host
- **WHEN** a client requests `GET /favicon.svg` with an untrusted Host header
- **THEN** the bridge SHALL still serve the icon

### Requirement: Cross-site POST guard
For the state-changing POSTs `/save`, `/config`, `/wifi_join`, `/redetect`, `/forget`, `/printer_config`,
`/printer_action`, `/printer_test`, `/update` and `/boot_other` the bridge SHALL first apply the Host guard, then
read Origin, or Referer when Origin is absent. A request with neither header MUST pass. Otherwise the scheme's
`host[:port]` (up to `/`, `?` or `#`) MUST equal the whole Host header, ignoring case, or the reply is 403.

**Source**: `firmware/main/web.c` `same_origin()`, `cross_site()`; `firmware/main/urlcodec.c` `url_origin_matches_host()`

#### Scenario: Form posted from another site
- **WHEN** a page on `http://example.com` posts to `/printer_action` with `Origin: http://example.com` and `Host: zebserial.local`
- **THEN** the bridge SHALL reply 403 "cross-site request refused" and send nothing to the printer
- **AND** it SHALL log "refused cross-site POST to /printer_action from http://example.com"

#### Scenario: Script without browser headers
- **WHEN** curl posts `action=feed` to `/printer_action` with `Host: zebserial.local` and no Origin or Referer
- **THEN** the guard SHALL pass

#### Scenario: Opaque origin
- **WHEN** a POST carries `Origin: null`
- **THEN** the guard SHALL refuse it, since the value has no `://`

### Requirement: Form body limits
The bridge SHALL read a URL-encoded form body of at most 512 bytes (`BODY_MAX`) for `/save`, `/config`, `/wifi_join`,
`/printer_config`, `/printer_action` and `/printer_test`. A larger `Content-Length` MUST get HTTP 400 "form too
large" before any of the body is read, and a receive error or early close MUST get HTTP 400 "read failed".

**Source**: `firmware/main/web.c` `read_form()`, `BODY_MAX`

#### Scenario: Oversized form
- **WHEN** a POST to `/config` has `Content-Length: 600`
- **THEN** the bridge SHALL reply 400 "form too large" and change no setting

### Requirement: URL form decoding
The bridge SHALL find a form field by exact name in `name=value` pairs split on `&`, taking the first match, and
decode its value: `+` becomes a space and `%XX` with two hex digits (either case) becomes that byte. A malformed
escape MUST be copied literally. The result is truncated to fit the caller's buffer and always NUL-terminated.

**Source**: `firmware/main/urlcodec.c` `form_get()`, `decode_n()`, `hex_val()`, `url_decode()`

#### Scenario: Escapes decoded
- **WHEN** the body is `ssid=My+Net%21&pass=x`
- **THEN** `form_get(body, "ssid", ...)` SHALL give `My Net!`

#### Scenario: Malformed escape
- **WHEN** a value is `50%zz`
- **THEN** the decoded value SHALL be `50%zz`

#### Scenario: Name that is a prefix of another
- **WHEN** the body is `idle_x=1&idle=30`
- **THEN** `form_get(body, "idle", ...)` SHALL give `30`

### Requirement: JSON string escaping
Text from the printer, SSIDs, hostname and version strings placed in JSON replies SHALL be escaped by `json_escape()`:
quote, backslash, `\n`, `\r` and `\t` as two-character escapes, other control bytes and `<` and `>` as `\u00XX`, bytes
from 0x80 passed through. It MUST stop before an escape that would not fit, never writing half of one.

**Source**: `firmware/main/urlcodec.c` `json_escape()`; `firmware/main/web.c` `status_get()`, `printer_config_get()`

#### Scenario: Tag-closing SSID
- **WHEN** the SSID is `</script>`
- **THEN** `/api/status` SHALL carry it as `</script>`

### Requirement: Page frame and notices
The Status, Printer and Bridge pages SHALL share one frame: the inline logo, a heading, and a bar linking `/` (Status),
`/printer` (Printer) and `/bridge` (Bridge) with `aria-current=page` on the current one. The head MUST link
`/favicon.svg` and `/apple-touch-icon.png`. A `?done=<code>` query SHALL show a note only for a code in the page's
fixed list, with the list's text.

**Source**: `firmware/main/web.c` `PAGE_HEAD`, `send_page_start()`, `send_notice()`, `s_printer_notices`, `s_bridge_notices`

#### Scenario: Known notice
- **WHEN** a browser opens `/printer?done=feed`
- **THEN** the page SHALL show "Fed one label."

#### Scenario: Unknown notice code
- **WHEN** a browser opens `/printer?done=<b>hi</b>`
- **THEN** the page SHALL show no notice

### Requirement: Form error pages
When a form is refused, the bridge SHALL reply with the given HTTP status and an HTML page holding the logo, the
message HTML-escaped in a `note bad` paragraph, and a Back link to the page the form came from (`/`, `/printer`,
`/bridge` or `/bridge#wifi`).

**Source**: `firmware/main/web.c` `fail()`, `html_escape()`

#### Scenario: Bad value
- **WHEN** a POST to `/printer_config` holds a value the printer cannot take
- **THEN** the reply SHALL be 400 with the message and a Back link to `/printer`

### Requirement: Status JSON
`GET /api/status` SHALL reply `application/json` with `Cache-Control: no-store` and the objects `wifi`, `config`,
`printer`, `bridge`, `rlog`, `ota`, then `chip_temp`, `uptime_s`, `version` and `built`, with the fields listed in
Notes. Each call MUST first call `printer_link_want_status()` so a printer that sleeps is asked again.

**Source**: `firmware/main/web.c` `status_get()`; `firmware/main/printer_link.h` `printer_link_want_status()`

#### Scenario: Status read
- **WHEN** a client on the LAN requests `GET /api/status`
- **THEN** the reply SHALL be JSON whose `printer.baud` is the current link speed and whose `uptime_s` is whole seconds since boot

#### Scenario: Temperature not yet read
- **WHEN** the chip temperature has no valid reading
- **THEN** `chip_temp` SHALL be `null`

### Requirement: Status JSON size limit
The bridge SHALL build the status JSON in a 3072-byte heap buffer. If allocation fails it MUST reply HTTP 500 "out of
memory"; if the JSON would not fit it MUST log "status JSON needs ... bytes" and reply HTTP 500 "status too long".

**Source**: `firmware/main/web.c` `status_get()` `json_cap`

#### Scenario: Oversized status
- **WHEN** the formatted status needs 3072 bytes or more
- **THEN** the bridge SHALL reply 500 "status too long" rather than send truncated JSON

### Requirement: Printer configuration API
`GET /api/printer_config` SHALL return the cached `^HH` reading, asking the printer again (waiting up to 2000 ms for
the link) when the cache is missing, older than 60000 ms, or the query holds `refresh=1`. The reply is
`{"age_ms":n,"error":text|null,"pairs":[[name,value],...],"cfg":{hh_name:value,...}}` with `no-store`; `age_ms` is -1
and `pairs` empty when no reading exists.

**Source**: `firmware/main/web.c` `printer_config_get()`, `printer_config()`, `CFG_MAX_AGE_MS`, `CFG_LINK_WAIT_MS`

#### Scenario: Printer busy printing
- **WHEN** a print job holds the link for more than 2000 ms during a refresh
- **THEN** `error` SHALL be "Printer busy (printing or being detected)" and `pairs` SHALL hold the older reading if there is one

#### Scenario: Decoded settings
- **WHEN** the reading holds `DARKNESS` as `15.0`
- **THEN** `cfg` SHALL hold `"DARKNESS":"15"`, the form's value for that field

### Requirement: Configuration read errors
When the printer cannot be read, the bridge SHALL report: ESP_ERR_TIMEOUT "Printer busy (printing or being detected)",
ESP_ERR_INVALID_STATE "Printer not detected, or holding output paused", ESP_ERR_NOT_SUPPORTED "The printer is not in a
ZPL language (see Language on the status page), so its settings cannot be read", ESP_ERR_NO_MEM "Out of memory", any
other error "No complete reply to ^HH from the printer".

**Source**: `firmware/main/web.c` `printer_config()`

#### Scenario: Printer not in ZPL
- **WHEN** the printer's language is EPL only
- **THEN** the configuration error SHALL name the language and say the settings cannot be read

### Requirement: Status page contents
`GET /` off the access point SHALL serve the Status page: a Printer table (Printer, Language, Status, Serial link), a
Bridge table (Wi-Fi, Access point, Address, IPv6, Print client, Jobs, Bytes, Remote log, Chip temperature, Uptime,
Firmware), and the printer configuration. It MUST offer no setting changes; its only button re-reads the
configuration with `?refresh=1`.

**Source**: `firmware/main/web.c` `root_get()`, `STATUS_HTML`, `STATUS_SCRIPT` `upd()`, `cfg()`

#### Scenario: Printer not detected
- **WHEN** `printer.detected` is false and the link is at 9600 baud
- **THEN** the Printer row SHALL read "not detected (trying 9600 baud)"

#### Scenario: Printer silent
- **WHEN** the printer is detected but not answering
- **THEN** the Printer row SHALL say for how long and suggest paper or ribbon out, head open, or switched off

### Requirement: Status page polling
The Status page SHALL fetch `/api/status` on load, then every 2000 ms while the tab is visible, and again when the tab
becomes visible. It MUST NOT poll from a hidden tab. While a Wi-Fi join is `trying`, a failed fetch SHALL show a note
telling the user to connect to the new network and open `http://<hostname>.local/`.

**Source**: `firmware/main/web.c` `STATUS_SCRIPT` `upd()`, `join()`, `setInterval`, `visibilitychange`

#### Scenario: Hidden tab
- **WHEN** the Status page is open in a background tab
- **THEN** it SHALL send no `/api/status` requests until the tab is shown again

### Requirement: Status page configuration display
The Status page SHALL show the `^HH` pairs under the groups Print, Media, Communications, Sensors, Firmware and
hardware, and Usage and maintenance, with unknown names under Other. Serial settings appear once, in the Serial link
row. A group holding a setting the Printer form changes MUST link to `/printer#g-<first word>`. Lines not used with
the current media or mode SHALL be dimmed.

**Source**: `firmware/main/web.c` `STATUS_SCRIPT` `G`, `draw()`, `srow()`, `unused()`, `sbar()`, `ser()`; `send_ui_script()`

#### Scenario: Sensor readings
- **WHEN** the media type is gap/notch and `WEB S.` and `MEDIA S.` are both 0 to 100
- **THEN** the Sensors group SHALL show both readings on a 0 to 100 scale drawing

#### Scenario: Raw wording
- **WHEN** the user ticks "Also show the printer's own wording"
- **THEN** each row SHALL also show the printer's own name and value

### Requirement: Printer page contents
`GET /printer` SHALL show the notice, the settings form, a Routine group (Feed one label, Calibrate media, Print
configuration label, Mark head cleaned, Re-detect printer), the Test label form, and a Reset group (Zero user counter
1 and 2, Reload saved settings, Restore factory defaults). Calibrate, Mark head cleaned and every Reset button MUST ask
for confirmation first; the Reset buttons are styled as dangerous.

**Source**: `firmware/main/web.c` `printer_get_page()`, `send_action()`, `send_counter_reset()`

#### Scenario: Counter reset confirmation
- **WHEN** the user presses "Zero user counter 1"
- **THEN** the browser SHALL ask first, saying Total printed and printhead usage are not changed

### Requirement: Printer settings form fields
The settings form SHALL offer, in order, Darkness, Print speed, Print method, Print mode, Print width, Label length,
Tear-off adjust, Label top, Left position, Backfeed (group Print); Media type, Media type detection, Maximum label
length, Media at power up, Media at head close (Media); ZPL mode (Communications); Head maintenance early warning
(Usage and maintenance).

**Source**: `firmware/main/web.c` `s_printer_ui[]`, `send_printer_form()`, `group_anchor()`; `firmware/main/zpl_status.c` `s_cfg_desc[]`

#### Scenario: Group anchors
- **WHEN** the form is drawn
- **THEN** each group SHALL be a fieldset with id `g-print`, `g-media`, `g-communications` or `g-usage`

### Requirement: Printer form values from the printer
The form SHALL be filled from the cached `^HH` reading and MUST show only fields decoded from it, each with its current
value. A choice the printer cannot take SHALL be left out of its list. The serial settings are shown as text, not
offered. If the reading fails the page SHALL say why, and say when the values are from an earlier reading.

**Source**: `firmware/main/web.c` `send_printer_form()`, `send_printer_field()`; `firmware/main/zpl_status.c` `zpl_cfg_from_hh()`, `zpl_cfg_parse()`

#### Scenario: 300 dpi speed list
- **WHEN** the printer reports 12 dots/mm
- **THEN** Print speed SHALL offer only 2, 3 and 4 ips

#### Scenario: Setting not in the reading
- **WHEN** the `^HH` reply has no `EARLY WARNING` line
- **THEN** the form SHALL have no early warning control

### Requirement: Lengths entered in millimetres
When the resolution is known, Print width, Label length, Tear-off adjust, Label top, Left position and Maximum label
length SHALL be number inputs named `<key>_mm` in mm, with the range and value converted from dots to 0.01 mm, and the
dot count shown live as the user types. Without a resolution they MUST fall back to whole dots under `<key>`.

**Source**: `firmware/main/web.c` `send_printer_field()`, `fmt_mm_exact()`, `s_printer_ui[]` `unit`

#### Scenario: Width at 300 dpi
- **WHEN** the printer reports `1280 12/MM` and a print width of 1280 dots
- **THEN** the width field SHALL be named `width_mm` with value `108.37`

### Requirement: True dots per millimetre
The bridge SHALL convert mm and dots with the true head pitch: a reported 12 dots/mm is 300/25.4 and 24 dots/mm is
600/25.4 dots per mm; any other positive figure is used as reported. mm to dots MUST round to the nearest dot either
side of zero. The dpi for 8, 12 and 24 dots/mm is 203, 300 and 600.

**Source**: `firmware/main/zpl_units.h` `zpl_true_dpmm()`, `zpl_mm_to_dots()`, `zpl_dots_to_mm()`, `zpl_dpi()`

#### Scenario: 4 inch label at 300 dpi
- **WHEN** 101.6 mm is converted at a reported 12 dots/mm
- **THEN** the result SHALL be 1200 dots

### Requirement: Applying the printer settings form
`POST /printer_config` SHALL need a configuration reading, else reply 500 "The printer's current settings could not be
read; nothing was sent.". For each known field it SHALL take `<key>_mm` (from -10000 to 10000 mm, to dots) or `<key>`,
and validate it. Only fields that differ from the reading MUST be sent; with none it SHALL redirect 303 to
`/printer?done=same`.

**Source**: `firmware/main/web.c` `printer_config_post()`, `parse_mm()`; `firmware/main/zpl_status.c` `zpl_cfg_parse()`

#### Scenario: Not a number of mm
- **WHEN** `width_mm=abc` is posted
- **THEN** the reply SHALL be 400 "width is not a number of mm; nothing was sent." and nothing SHALL reach the printer

#### Scenario: Out of range
- **WHEN** `darkness=31` is posted
- **THEN** the reply SHALL be 400 "darkness is not a valid value for this printer; nothing was sent."

#### Scenario: Unchanged form
- **WHEN** the form is posted as it was drawn
- **THEN** the bridge SHALL send nothing and redirect to `/printer?done=same`

### Requirement: Printer setting ranges
The bridge SHALL accept Darkness 0 to 30; Print speed 2 to 4 at 12 dots/mm, else 2 to 6; ZPL mode 1 or 2; Print width
2 to the head width in dots (32000 if unknown); Label length 1 to 32000; Maximum length 50 times dots/mm (2 if unknown) to 32000;
Tear-off and Label top -120 to 120; Left position -9999 to 9999; Backfeed N, A, B, O or 10 to 90 in steps of 10.

**Source**: `firmware/main/zpl_status.c` `zpl_cfg_range()`, `zpl_cfg_value_valid()`, `s_cfg_desc[]` `letters`

#### Scenario: Backfeed percentage
- **WHEN** `backfeed=70` is posted
- **THEN** it SHALL be taken as 70 %, not as the letter `F`

#### Scenario: Letter fields
- **WHEN** `method=X` is posted
- **THEN** it SHALL be refused, since Print method takes only `D` or `T`

### Requirement: Settings to ZPL
The bridge SHALL build the changes as: `~SD%02d`, `~TA` (three characters, `~TA-%02d` when negative) and `~JS<letter or
percent>` first, each alone; then one `^XA`...`^XZ` with `^PR`, `^MT`, `^MM`, `^MN`, `^PW`, `^LL`, `^ML`, `^LT`, `^LS`,
`^MF`, `^JH,,,,,<E|D>` and `^SZ` last, for changed fields only; then `^XA^JUS^XZ` to save.

**Source**: `firmware/main/zpl_status.c` `zpl_cfg_commands()`

#### Scenario: Darkness and width changed
- **WHEN** darkness changes to 20 and print width to 800
- **THEN** the bridge SHALL send `~SD20^XA^PW800^XZ^XA^JUS^XZ`

#### Scenario: Negative tear-off
- **WHEN** tear-off changes to -5
- **THEN** the command SHALL be `~TA-05`

### Requirement: Media detection as media tracking
Media type and Media type detection SHALL share `^MN`: when detection is changed to Automatic the bridge MUST send
`^MNA`; when the media type changes, or detection changes to Fixed, it SHALL send `^MN<Y|M|N>` with the media type.
`^MF` always carries both the power-up and head-close actions, so both must be known.

**Source**: `firmware/main/zpl_status.c` `zpl_cfg_commands()` `^MN`, `^MF`; `s_cfg_desc[ZPL_CFG_SENSOR]`

#### Scenario: Switch to automatic detection
- **WHEN** Media type detection changes from Fixed to Automatic
- **THEN** the format SHALL hold `^MNA`

#### Scenario: Head-close action changed
- **WHEN** only Media at head close changes to Calibrate while power-up is Feed
- **THEN** the format SHALL hold `^MFF,C`

### Requirement: Sending the printer settings
The bridge SHALL send the settings ZPL with `printer_link_send_zpl()`, waiting up to 5000 ms for the link and holding it
500 ms after, then redirect 303 to `/printer?done=sent`. A link error MUST give HTTP 500 with: busy printing, paused
(XOFF), not detected, or not in a ZPL language; a command over 255 bytes gives 500 "Could not build the printer
commands.".

**Source**: `firmware/main/web.c` `printer_config_post()`, `link_error()`, `CMD_LINK_WAIT_MS`

#### Scenario: Printer paused
- **WHEN** the printer holds XOFF while the settings are sent
- **THEN** the reply SHALL be 500 "The printer is paused (XOFF: out of paper or head open?); the command was discarded."

### Requirement: Printer actions
`POST /printer_action` SHALL map `action` to: `calibrate` `~JC`, `config_label` `~WC`, `feed` `~PH`, `reset_counter1`
`~RO1`, `reset_counter2` `~RO2`, `reset_head_cleaned` `~ROC`, `reload_saved` `^XA^JUR^XZ`, `factory_defaults`
`^XA^JUF^JUS^XZ`. Any other name MUST give 400 "Unknown printer action.". Success redirects 303 to
`/printer?done=<action>`.

**Source**: `firmware/main/zpl_status.c` `zpl_action_command()`; `firmware/main/web.c` `printer_action_post()`

#### Scenario: Feed
- **WHEN** `action=feed` is posted
- **THEN** the bridge SHALL send `~PH` and redirect to `/printer?done=feed`

#### Scenario: Unknown action
- **WHEN** `action=format_disk` is posted
- **THEN** the reply SHALL be 400 and nothing SHALL be sent

### Requirement: Actions that change the serial port
For `factory_defaults` and `reload_saved` the bridge SHALL hold the link 3000 ms after sending and then request printer
re-detection, so the printer is found at its new speed and set back to the bridge's speed and XON/XOFF. Other actions
MUST be sent with no hold. All actions wait up to 5000 ms for the link.

**Source**: `firmware/main/web.c` `printer_action_post()` `port_may_change`; `firmware/main/printer_link.h` `printer_link_request_redetect()`

#### Scenario: Factory defaults
- **WHEN** `action=factory_defaults` is posted and sent
- **THEN** the bridge SHALL request re-detection and show "Factory defaults restored. The bridge is finding the printer again to set its serial port."

### Requirement: Re-detect printer
`POST /redetect` SHALL request printer re-detection and redirect 303 to `/printer?done=redetect`. It MUST read no body.

**Source**: `firmware/main/web.c` `redetect_post()`

#### Scenario: Re-detect pressed
- **WHEN** the user presses "Re-detect printer"
- **THEN** the Printer page SHALL show "Finding the printer again."

### Requirement: Test label form and centring guidance
The Printer page SHALL offer a test label form posting `w` and `l` in mm to `/printer_test`, filled from the cached
print width and label length to 0.1 mm. Its text MUST say nothing is saved, and that with left reading L mm and right R
mm on the corner scales, Left Position is lowered by (L - R) / 2 mm, raised if negative.

**Source**: `firmware/main/web.c` `send_test_label_form()`

#### Scenario: No reading yet
- **WHEN** no `^HH` reading is cached
- **THEN** the width and length fields SHALL be empty and required

### Requirement: Printing the test label
`POST /printer_test` SHALL need the resolution, else 503. Width MUST be 10 mm to the head width (108 mm if unknown) and
length 10 to 1000 mm, else 400. It SHALL build the label for the hostname's `.local` URL and the printer model in an
8192-byte buffer, send it waiting up to 5000 ms with no hold, and redirect 303 to `/printer?done=test_label`.

**Source**: `firmware/main/web.c` `printer_test_post()`, `parse_mm()`; `firmware/main/test_label.h` `TEST_LABEL_ZPL_MAX`

#### Scenario: Too wide
- **WHEN** `w=120` is posted for a 1280-dot 12 dots/mm head
- **THEN** the reply SHALL be 400 "The width must be a number of mm from 10 to 108.4 (the printhead's width)."

#### Scenario: Resolution unknown
- **WHEN** the printer has not answered `^HH`
- **THEN** the reply SHALL be 503 and nothing SHALL be printed

### Requirement: Test label outline
The test label SHALL be one `^XA`...`^XZ` format opening `^LH0,0^PW<w>^LL<l>^CI0`, with no `^JUS`, so nothing is
saved. It MUST draw a border box at one reported dots/mm in from each edge, line about 0.25 mm (at least 2 dots), and
mm rulers along the top and left: marks every 5 mm from the label edge, longer every 10 mm and numbered there where
room allows, up to 300 mm.

**Source**: `firmware/main/test_label.c` `test_label_zpl()`, `RULER_MAX_MM`

#### Scenario: Label too small
- **WHEN** the width or length is under 10 times the reported dots/mm
- **THEN** `test_label_zpl()` SHALL return 0 and the bridge SHALL reply 500 "Could not build the test label."

### Requirement: Test label edge scales
In both top corners the test label SHALL draw ticks from each side edge at 0 to 4 mm in 0.5 mm steps (true mm), at
least 3 dots wide, about 1.5 mm long on whole mm and 0.75 mm on half mm, numbered at 2 and 4, so the label's edge position
on each side shows how far the print is off centre.

**Source**: `firmware/main/test_label.c` `test_label_zpl()` (edge scale loop), `SCALE_HALF_MM`

#### Scenario: Tick positions
- **WHEN** a test label is built
- **THEN** each corner SHALL have 9 ticks, tick k (0 to 8) starting `zpl_mm_to_dots(k * 0.5)` dots in from the left edge and ending the same distance in from the right edge, so a tick's outer side is the reading
- **AND** the ticks at 2 and 4 mm SHALL carry the numbers "2" and "4"

### Requirement: Test label content
Below the rulers, as far as each fits, the test label SHALL show the title, model and size in mm, dpi and dots, and
bridge version; vertical and horizontal 1 to 4 dot lines, 8 mm long; a solid band 4 mm high; a 20 mm circle; a QR code
of the bridge URL; and a Code 128 barcode of `ZEBSERIAL`. Parts that do not fit MUST be left out.

**Source**: `firmware/main/test_label.c` `test_label_zpl()`, `put_text()`

#### Scenario: Narrow label
- **WHEN** the label is too narrow for the Code 128 barcode
- **THEN** the barcode SHALL be left out and the rest printed

#### Scenario: ZPL characters in the model
- **WHEN** the model text holds `^` or `~`
- **THEN** each SHALL be printed as a space

### Requirement: Bridge page contents
`GET /bridge` SHALL show: the latest Wi-Fi join result; a Join form (network name with scanned names, `?scan=1` to scan,
password); Forget Wi-Fi; a Save form with Network (hostname, static IPv4 address, netmask, gateway, DNS), Serial link
(speed, pins, idle timeout 5 to 3600 s) and Remote log (on, level); and the Firmware section.

**Source**: `firmware/main/web.c` `bridge_get_page()`, `send_wifi_section()`, `send_firmware_section()`, `SETTINGS_IDLE_MIN_S`, `SETTINGS_IDLE_MAX_S`

#### Scenario: Speed choices
- **WHEN** the Bridge page is drawn
- **THEN** Speed SHALL offer 115200, 57600, 38400, 19200 and 9600 baud and "Leave the printer as it is" (0)

### Requirement: Saving the bridge settings
`POST /config` SHALL validate everything before saving: a bad static address gives 400 "<key> is not a valid IPv4
address.", a bad speed, pins or idle 400 "A serial link setting is out of range.", a bad log level 400, a bad hostname
or address set 400. Unticked `rlog` and `static` boxes MUST clear those settings; absent other fields keep their
values.

**Source**: `firmware/main/web.c` `config_post()`; `firmware/main/settings.h` `settings_options_valid()`, `settings_net_valid()`

#### Scenario: Bad gateway
- **WHEN** `static=1&gw=abc` is posted
- **THEN** the reply SHALL be 400 "gw is not a valid IPv4 address." and no setting SHALL change

### Requirement: Applying the bridge settings
After saving, the bridge SHALL apply the target speed at once and restart the remote log if it changed. If the pins,
hostname, static flag, or (with static on) any address changed, it MUST show a page with the reconnect address and
restart 1500 ms later; otherwise it SHALL redirect 303 to `/bridge?done=saved`.

**Source**: `firmware/main/web.c` `config_post()`, `restart_soon()`; `printer_link_set_target_baud()`, `remote_log_apply()`

#### Scenario: Speed change only
- **WHEN** only the speed changes from 115200 to 57600
- **THEN** the bridge SHALL not restart and SHALL redirect to `/bridge?done=saved`

#### Scenario: Hostname change
- **WHEN** the hostname changes
- **THEN** the bridge SHALL show "Saved. Rebooting." with a link to the new `.local` name and restart

### Requirement: Old settings address
`GET /settings` SHALL redirect 303 See Other to `/printer` (302 to the set-up page over the access point). It MUST NOT
apply the Host guard.

**Source**: `firmware/main/web.c` `settings_get_page()`, `redirect_to()`

#### Scenario: Bookmark to the old page
- **WHEN** a browser on the LAN opens `/settings`
- **THEN** it SHALL be sent to `/printer`

### Requirement: Icons
The bridge SHALL serve `/favicon.svg` as `image/svg+xml` (the logo with its own light and dark colours),
`/favicon.ico` as `image/x-icon`, and `/apple-touch-icon.png` and `/apple-touch-icon-precomposed.png` as `image/png`
from the embedded icon files, all with `Cache-Control: max-age=86400` and on every network.

**Source**: `firmware/main/web.c` `favicon_get()`, `icon_get()`, `s_favicon`; `firmware/main/CMakeLists.txt` `EMBED_FILES`

#### Scenario: iOS home-screen icon
- **WHEN** Safari on iOS asks for `/apple-touch-icon-precomposed.png`
- **THEN** the bridge SHALL send the embedded PNG

### Requirement: Page script syntax gate
Page JavaScript in `web.c` MUST parse under `node --check` before a build is pushed. The script sits in C string
literals: `STATUS_SCRIPT`, the Firmware upload script in `send_firmware_section()`, the `var UI` script built by
`send_ui_script()`, and inline `onchange`, `oninput` and `onsubmit` attributes. No repository script runs this check.

**Source**: `firmware/main/web.c` `STATUS_SCRIPT`, `send_firmware_section()`, `send_ui_script()`, `send_action()`, `send_printer_field()`

#### Scenario: Checking from the source
- **WHEN** the C literals of `STATUS_SCRIPT` are joined, C escapes decoded and the `<script>` tags removed
- **THEN** `node --check` on the result SHALL exit 0

#### Scenario: Checking a running bridge
- **WHEN** the `<script>` bodies of `/`, `/printer` and `/bridge` served by a bridge are saved to files
- **THEN** `node --check` on each SHALL exit 0

## Notes

### Route table

| Method | URI | Handler | Set-up AP | Guard |
|---|---|---|---|---|
| GET | `/` | `root_get` | set-up page | Host |
| POST | `/save` | `save_post` | served | Origin (wifi-provisioning) |
| GET | `/api/status` | `status_get` | 302 | Host |
| GET | `/favicon.svg` | `favicon_get` | served | none |
| GET | `/favicon.ico` | `icon_get` | served | none |
| GET | `/apple-touch-icon.png` | `icon_get` | served | none |
| GET | `/apple-touch-icon-precomposed.png` | `icon_get` | served | none |
| GET | `/settings` | `settings_get_page` | 302 | none (303 to `/printer`) |
| GET | `/printer` | `printer_get_page` | 302 | Host |
| GET | `/bridge` | `bridge_get_page` | 302 | Host |
| POST | `/wifi_join` | `wifi_join_post` | 302 | Origin |
| POST | `/config` | `config_post` | 302 | Origin |
| POST | `/redetect` | `redetect_post` | 302 | Origin |
| POST | `/forget` | `forget_post` | 302 | Origin |
| GET | `/api/printer_config` | `printer_config_get` | 302 | Host |
| POST | `/printer_config` | `printer_config_post` | 302 | Origin |
| POST | `/printer_action` | `printer_action_post` | 302 | Origin |
| POST | `/printer_test` | `printer_test_post` | 302 | Origin |
| POST | `/update` | `update_post` | 302 | Origin (ota-updates) |
| POST | `/pstprnt` | `pstprnt_post` | 403 | none (http-print) |
| POST | `/boot_other` | `boot_other_post` | 302 | Origin |

"Origin" means `same_origin()`, which includes the Host check. 21 handlers against `max_uri_handlers` 24.

Where a route's behaviour is specified: `/save`, `/wifi_join` and `/forget` in wifi-provisioning; `/update` and
`/boot_other` in ota-updates; `/pstprnt` in http-print; everything else here. The Bridge page's Remote log fields are
stored as described in device-settings and act as described in remote-log.

### /api/status fields

- `wifi`: `ssid`, `ip`, `rssi`, `phy`, `channel`, `bssid`, `hostname`, `static_ip`, `ip6` (array),
  `join` {`state` ("", "trying", "joined", "failed"), `ssid`, `why`}
- `config`: `target_baud`, `pins`, `idle_timeout_s`
- `printer`: `detected`, `answering`, `reply_age_ms`, `sleeps`, `model`, `language`, `zpl`, `baud`, `handshake`,
  `tx_gpio`, `rx_gpio`, `hs_valid`, `hs_age_ms`, `hs` {`paper_out`, `pause`, `head_up`, `ribbon_out`,
  `partial_format`, `buffer_full`, `corrupt_ram`, `under_temp`, `over_temp`, `formats_in_buffer`}, `tx_bytes`,
  `rx_bytes`
- `bridge`: `active`, `client_ip`, `sessions`, `aborted`, `to_printer`, `from_printer`
- `rlog`: `enabled`, `listener`, `sent`, `dropped`
- `ota`: `running`, `pending`, `other_version`
- `chip_temp`: `null` or {`c`, `max_c`, `hot`, `warn_c`}
- `uptime_s`, `version`, `built` ("<date> <time>" of the build)

### Settings form keys

Form key, `^HH` name: `darkness` DARKNESS, `speed` PRINT SPEED, `method` PRINT METHOD, `mode` PRINT MODE, `media`
MEDIA TYPE, `sensor` SENSOR SELECT, `zpl` ZPL MODE, `width` PRINT WIDTH, `length` LABEL LENGTH, `max_length` MAXIMUM
LENGTH, `tear_off` TEAR OFF, `label_top` LABEL TOP, `left_pos` LEFT POSITION, `power_up` MEDIA POWER UP, `head_close`
HEAD CLOSE, `backfeed` BACKFEED, `early_warning` EARLY WARNING.
