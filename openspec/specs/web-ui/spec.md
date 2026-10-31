# web-ui Specification

## Purpose
The bridge's HTTP server and its pages: Status, Printer and Bridge, the JSON API behind them, the printer settings
form and the ZPL it sends, one-shot printer actions, the test label, and the Host and cross-site guards. It lives in
`firmware/main/web.c`, with `zpl_status.c` (settings to ZPL), `zpl_units.h`, `test_label.c` and `urlcodec.c`.
The Wi-Fi setup page and `/save`, `POST /pstprnt` and the `/update` upload internals are specified by other
capabilities; they appear here only in the route table.

## Requirements

### Requirement: HTTP server configuration
The bridge SHALL start one ESP-IDF HTTP server from `HTTPD_DEFAULT_CONFIG()` with these changes: `max_open_sockets` 4,
`stack_size` 8192 bytes, `lru_purge_enable` true and `max_uri_handlers` 24. All other fields keep the ESP-IDF 6.1
defaults, so the server listens on TCP port 80. If `httpd_start` fails the bridge MUST log "HTTP server failed to
start" and register no handlers.

**Source**: `firmware/main/web.c` `web_start()`; `HTTPD_DEFAULT_CONFIG` in ESP-IDF 6.1 `esp_http_server.h` (`.server_port = 80`, `.stack_size = 4096`)

#### Scenario: Fifth connection
- **GIVEN** four clients hold open connections to port 80
- **WHEN** a fifth client connects
- **THEN** the server SHALL close the least recently used of the four (LRU purge) and serve the new one

#### Scenario: Handler registration fails
- **WHEN** `httpd_register_uri_handler` fails for one route
- **THEN** the bridge SHALL log "cannot register" with the URI and error name and SHALL go on registering the rest

### Requirement: Route table
The bridge SHALL register exactly the 22 URI handlers listed in the route table in Notes (21 URIs; `/pstprnt` has
one for POST and one for OPTIONS), and a 404 error handler. A request for any other URI off the setup access point MUST get HTTP 404 with the text "not
found".

**Source**: `firmware/main/web.c` `web_start()` `uris[]`, `not_found()`

#### Scenario: Unknown URL on the LAN
- **WHEN** a client on the station network requests `GET /nothing`
- **THEN** the bridge SHALL reply 404 "not found"

#### Scenario: Wrong method
- **WHEN** a client sends `GET /printer_config` (registered only for POST)
- **THEN** the bridge SHALL NOT run the POST handler

### Requirement: Setup access point routing
For a request that arrived over the setup access point (`wifi_prov_sock_on_ap()` true for its socket), the bridge
SHALL serve the setup page for `GET /`, accept `POST /save` and serve the icon routes, answer `POST /pstprnt` with
403 (see http-print), and answer every other route with `302 Found` and `Location: http://192.168.4.1/`. An
unregistered URI over the access point MUST get the same redirect.

**Source**: `firmware/main/web.c` `on_ap()`, `to_setup_page()`, `AP_HOME`, `root_get()`, `not_found()`, `settings_get_page()`

#### Scenario: Printer page asked over the setup network
- **WHEN** a phone on the setup network requests `GET /printer`
- **THEN** the bridge SHALL reply 302 with `Location: http://192.168.4.1/`

#### Scenario: Captive-portal probe
- **WHEN** a phone on the setup network requests `GET /generate_204`
- **THEN** the bridge SHALL reply 302 with `Location: http://192.168.4.1/`

#### Scenario: Status API from the setup network
- **WHEN** a client on the setup network requests `GET /api/status`
- **THEN** the bridge SHALL reply 302 to the setup page

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

#### Scenario: Firmware upload from another web site
- **WHEN** a browser posts to `/update` with an Origin that does not match the Host header
- **THEN** the bridge SHALL reply 403 "cross-site request refused" and SHALL NOT read the image

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

**Source**: `firmware/main/urlcodec.c` `form_get()`, `decode_n()`, `hex_val()`

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
and `pairs` empty when no reading exists. If its working buffer cannot be allocated it MUST reply 500 "out of memory".

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
side of zero. The dpi for 8, 12 and 24 dots/mm is 203, 300 and 600. The Status page and the Printer form SHALL show
the same mm for the same dot count.

**Source**: `firmware/main/zpl_units.h` `zpl_true_dpmm()`, `zpl_mm_to_dots()`, `zpl_dots_to_mm()`, `zpl_dpi()`;
`firmware/main/web.c` STATUS_SCRIPT `tdm()`, `val()`; `firmware/main/zpl_status.c` `decode_field()` MAXIMUM LENGTH

#### Scenario: 4 inch label at 300 dpi
- **WHEN** 101.6 mm is converted at a reported 12 dots/mm
- **THEN** the result SHALL be 1200 dots

#### Scenario: Same length on both pages
- **WHEN** the label length is 237 dots at a reported 12 dots/mm
- **THEN** the Status page and the Printer form SHALL both show 20.1 mm

#### Scenario: Maximum length in true mm
- **GIVEN** the printer converts its own mm figures (`^HH` MAXIMUM LENGTH, `^MUm`) with the rounded dots/mm
- **WHEN** the printer reports MAXIMUM LENGTH "2.0IN   50MM" at 12 dots/mm
- **THEN** the bridge SHALL decode it to 600 dots and show 50.8 mm, the physical length of `^ML600`

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
2 to the head width in dots (32000 if unknown); Label length 1 to 32000; Maximum length 50 times the reported (rounded) dots/mm (2 if unknown) to 32000;
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
length 10 to 1000 mm, else 400; 10 mm means `zpl_mm_to_dots(10, m)` dots at the true pitch, and the label SHALL build
for any size that passes. It SHALL build the label for the hostname's `.local` URL and the printer model in an
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

#### Scenario: Smallest label
- **WHEN** `w=10` and `l=10` are posted for a 300 dpi printer
- **THEN** the bridge SHALL print a 10 x 10 mm test label

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
`GET /settings` SHALL redirect 303 See Other to `/printer` (302 to the setup page over the access point). It MUST NOT
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
`send_ui_script()`, and inline `onchange`, `oninput` and `onsubmit` attributes. CI runs `tools/check_page_js.py`
on the first two; the others are checked on a running bridge.

**Source**: `firmware/main/web.c` `STATUS_SCRIPT`, `send_firmware_section()`, `send_ui_script()`, `send_action()`, `send_printer_field()`; `tools/check_page_js.py` `web_c_scripts()`; `.github/workflows/ci.yml` job `page-js`

#### Scenario: Checking from the source
- **WHEN** the C literals of `STATUS_SCRIPT` are joined, C escapes decoded and the `<script>` tags removed
- **THEN** `node --check` on the result SHALL exit 0

#### Scenario: Checking a running bridge
- **WHEN** the `<script>` bodies of `/`, `/printer` and `/bridge` served by a bridge are saved to files
- **THEN** `node --check` on each SHALL exit 0

## Notes

### Route table

| Method | URI | Handler | Setup AP | Guard |
|---|---|---|---|---|
| GET | `/` | `root_get` | setup page | Host |
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
| OPTIONS | `/pstprnt` | `pstprnt_options` | 403 | none (http-print) |
| POST | `/boot_other` | `boot_other_post` | 302 | Origin |

"Origin" means `same_origin()`, which includes the Host check. 22 handlers against `max_uri_handlers` 24.

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
- `ota`: see ota-updates (OTA state in the status API)
- `chip_temp`: `null` or {`c`, `max_c`, `hot`, `warn_c`}
- `uptime_s`, `version`, `built` ("<date> <time>" of the build)

### Settings form keys

Form key, `^HH` name: `darkness` DARKNESS, `speed` PRINT SPEED, `method` PRINT METHOD, `mode` PRINT MODE, `media`
MEDIA TYPE, `sensor` SENSOR SELECT, `zpl` ZPL MODE, `width` PRINT WIDTH, `length` LABEL LENGTH, `max_length` MAXIMUM
LENGTH, `tear_off` TEAR OFF, `label_top` LABEL TOP, `left_pos` LEFT POSITION, `power_up` MEDIA POWER UP, `head_close`
HEAD CLOSE, `backfeed` BACKFEED, `early_warning` EARLY WARNING.

### Form field names

| Route | Fields (URL-encoded body) |
|---|---|
| `POST /config` | `baud` (target baud, 0 = leave the printer alone), `pins` (0 auto, 1 TX on D3, 2 TX on D5), `idle` (s), `hostname`, `rlog` ("1" when ticked; absent clears it), `rlog_lvl` (1 to 3), `static` ("1" when ticked; absent clears it), and with `static`: `ip`, `mask`, `gw`, `dns` (dotted quads; empty is 0) |
| `POST /save` | `ssid` (from the list), `ssid_manual` (typed; wins when not empty), `pass` (empty for an open network) |
| `POST /wifi_join` | `ssid`, `pass` |
| `POST /printer_config` | each known field's form key from the decode table: `<key>` for letters and plain numbers, `<key>_mm` for lengths when dots/mm is known (`<key>` in dots otherwise) |
| `POST /printer_action` | `action` (one of the names `zpl_action_command()` knows; anything else is 400 "Unknown printer action.") |
| `POST /printer_test` | `w`, `l` (mm) |

Source: `firmware/main/web.c` `config_post()`, `save_post()`, `wifi_join_post()`, `printer_config_post()`, `printer_action_post()`, `printer_test_post()` (the `form_get(body, ...)` calls).

### /api/status value formats

- `wifi.phy`: "Wi-Fi 6 (802.11ax)", "Wi-Fi 5 (802.11ac)", "Wi-Fi 4 (802.11n)", "802.11g", "802.11a", "802.11b",
  "Espressif long range", or "" (unknown mode or not connected) (`wifi_prov_link()`).
- `wifi.bssid`: six lower-case hex pairs joined by ":", for example "02:00:00:12:34:56"; "" when not connected.
- `wifi.ip`: dotted quad while the state is `WIFI_ST_CONNECTED`, else "" (`wifi_prov_ip()`). `wifi.rssi` is 0 when
  not connected.
- `wifi.join.state`: "" (none since boot), "trying", "joined", "failed" (`join_names[]`).
- `version`: `esp_app_get_description()->version`, which ESP-IDF sets from `git describe` of the tree at build time
  (the project sets no `PROJECT_VER`), so a build from a dirty tree ends "-dirty".
- `built`: the app description's `date` and `time` (compile date and time) joined by a space, for example
  "Oct 31 2026 23:41:02".
- `printer.handshake`: up to 15 characters. The SGD `comm.handshake` answer as the printer worded it, when it gives
  one other than "?"; otherwise "XON/XOFF" or "DTR" from `~HS` (string 1, bit a7); "" when neither is known
  (`zpl_pick_handshake()`).
- `printer.model`: up to 31 characters from `~HI`, "" until a printer has answered.

### How ^HH lines are split

The printer form reads the cached ^HH dump. `zpl_hh_parse()` (firmware/main/zpl_status.c:419) splits it into up to `ZPL_HH_MAX_PAIRS` (128) name/value pairs, in printer order. A name holds at most 23 characters and a value at most 27 (`ZPL_HH_NAME_LEN` 24, `ZPL_HH_VALUE_LEN` 28, zpl_status.h:112-114). Longer text is truncated.

- A line is cut at CR or LF. STX and ETX at either end, and trailing spaces and NULs, are stripped. A line with any byte outside 0x20..0x7E is dropped (`hh_split_line()`, zpl_status.c:331-345). A last line with no line end and no ETX was cut off, and is skipped (`for_each_line()`, zpl_status.c:388).
- Fixed layout first. This is the GX430t layout: two spaces, the value in a 20-character field, then the name from column 22 (`HH_NAME_COL`, zpl_status.c:278). The rule applies when the line is longer than 22, starts with two spaces, the first non-space before column 21 is a value, column 21 is a space or dot, and column 22 is not. The value may be right-aligned, as in `   2.0IN   50MM       MAXIMUM LENGTH` (zpl_status.c:347-356).
- Fallback, the dot-leader layout of older firmware (`  9600.............BAUD`). The name starts after the last run of two or more dots that runs straight into a non-space (zpl_status.c:358-376).
- Both parts are trimmed of spaces. A trailing dot leader of two or more dots is dropped. A run of three or more dots inside the text becomes one space, so `2104k............R:` gives `2104k R:`. Text that is only dots (`...`) is kept (`copy_trimmed()`, zpl_status.c:290). A line whose value or name is empty is not a pair.
- `zpl_hh_get()` looks a name up exactly, and the first match wins (zpl_status.c:426).

`zpl_cfg_from_hh()` (zpl_status.c:714) reads RESOLUTION first. Then, for each `s_cfg_desc` entry (zpl_status.c:533), it looks up `hh_name` and decodes the value with `decode_field()` (zpl_status.c:647). It then range-checks the result with `zpl_cfg_value_valid()` (zpl_status.c:801), which uses the RESOLUTION figures. A field that is present, decodes and is in range gets its bit set in `known`. Any other field stays unknown.

### Printer form decode table (^HH to form)

Notes on the columns:

- Kind is `zpl_cfg_kind_t` (zpl_status.h:157). LETTER values are held as the ZPL letter's character code.
- Ranges come from `zpl_cfg_range()` (zpl_status.c:738). LETTER fields accept only the `letters` of `s_cfg_desc`.
- "Signed int, exact" means `parse_int_exact()`: an optional `+` or `-`, then 1 to 6 digits and nothing else (zpl_status.c:594, 618). Leading zeros are allowed.
- dpmm is the reported dots/mm and head_dots the head width, both from RESOLUTION.

| Form key (POST /printer_config) | ^HH name | Kind | ^HH value text and how it decodes | ZPL sent (`zpl_cfg_commands`) | Range check |
|---|---|---|---|---|---|
| `darkness` | `DARKNESS` | INT | `15.0` gives 15. A whole number, then an optional `.` and digits. Only the first decimal is read, the value is rounded half up (`(t + 5) / 10`), and there must be nothing after it. No sign is allowed (`parse_tenths_prefix`, zpl_status.c:625, 652-658). | `~SD%02d`, e.g. `~SD05` | 0..30 |
| `speed` | `PRINT SPEED` | INT | `4 IPS` gives 4. An integer followed by exactly ` IPS` (zpl_status.c:659-661). | `^PR%d` | 2 to 4 when dpmm is 12, otherwise 2 to 6 (including dpmm 0 or 24) (zpl_status.c:754) |
| `method` | `PRINT METHOD` | LETTER `DT` | `s_method_map` | `^MT%c` | D, T |
| `mode` | `PRINT MODE` | LETTER `TPRCD` | `s_mode_map` | `^MM%c` | T, P, R, C, D (`A`, applicator, is refused; test_main.c:629) |
| `media` | `MEDIA TYPE` | LETTER `YMN` | `s_media_map` | `^MN%c`, see the sensor note below | Y, M, N |
| `sensor` | `SENSOR SELECT` | LETTER `AM` | `s_sensor_map` | `^MNA` for A. For M it sends `^MN` with the media letter. | A, M |
| `zpl` | `ZPL MODE` | INT | `s_zpl_map`: the text, not a number | `^SZ%d`, last in the format | 1..2 |
| `width_mm` (or `width`) | `PRINT WIDTH` | INT | `1200` gives 1200 dots. Signed int, exact. | `^PW%d` | 2 up to head_dots, or 32000 when head_dots is unknown |
| `length_mm` (or `length`) | `LABEL LENGTH` | INT | `1938` gives 1938 dots. Signed int, exact. | `^LL%d` | 1..32000 |
| `max_length_mm` (or `max_length`) | `MAXIMUM LENGTH` | INT | `39.0IN   975MM` gives 975 × dpmm, which is 11700 at 12/MM. A tenths number, then `IN`, any spaces, an integer, then exactly `MM`. The inch figure is read and thrown away. Unknown when dpmm is 0. `2.0IN   50MM` gives 600 (zpl_status.c:662-681; test_main.c:497-502). | `^ML%d` | 50 × dpmm up to 32000, or 2..32000 when dpmm is 0 |
| `tear_off_mm` (or `tear_off`) | `TEAR OFF` | INT | `+000` gives 0, `-0023` gives -23. Signed int, exact. | `~TA%03d` for 0 and above, `~TA-%02d` below 0 (`~TA007`, `~TA-05`, `~TA-120`) | -120..120 |
| `label_top_mm` (or `label_top`) | `LABEL TOP` | INT | `+000` gives 0. Signed int, exact. | `^LT%d` | -120..120 |
| `left_pos_mm` (or `left_pos`) | `LEFT POSITION` | INT | `+0000` gives 0, `0237` gives 237. Signed int, exact. | `^LS%d` | -9999..9999 |
| `power_up` | `MEDIA POWER UP` | LETTER `FCLN` | `s_feed_map` | `^MF%c,%c`, power-up then head-close. Both are always sent, so both must be known (zpl_status.c:896-900, 964-967). | F, C, L, N |
| `head_close` | `HEAD CLOSE` | LETTER `FCLN` | `s_feed_map` | `^MF%c,%c`, as for `power_up` | F, C, L, N |
| `backfeed` | `BACKFEED` | PERCENT_OR_LETTER `NABO` | `60%` gives 60: an integer followed by exactly `%`. Otherwise `s_backfeed_map` (zpl_status.c:701-706). | `~JS%c` for a letter, `~JS%d` for a percentage | N, A, B, O, or 10..90 in steps of 10 |
| `early_warning` | `EARLY WARNING` | LETTER `ED` | `s_early_map` | `^JH,,,,,%c` | E, D |

How the ZPL is assembled (zpl_status.c:881-984). The ~ commands come first, each on its own: `~SD`, then `~TA`, then `~JS`. Then come any ^ commands in one `^XA...^XZ`, in this order: `^PR ^MT ^MM ^MN ^PW ^LL ^ML ^LT ^LS ^MF ^JH ^SZ`. The save, `^XA^JUS^XZ`, always ends it. The fixture test with every field changed gives `~SD05~TA-120~JS60^XA^PR3^MTT^MMP^MNM^PW812^LL1218^ML3000^LT-10^LS25^MFC,F^JH,,,,,E^SZ1^XZ^XA^JUS^XZ` (test_main.c:688-691).

Sensor and media share `^MN` (zpl_status.c:938-949):

- If `sensor` changed to A, the bridge sends `^MNA`, and a media change in the same post is not sent.
- Otherwise, if `media` or `sensor` changed, it sends `^MN<media letter>`.

On a G-series printer, SENSOR SELECT then reads AUTO after `^MNA` and MANUAL after any other `^MN` (comment at zpl_status.c:539).

What an unknown or unparsable value does:

- The field's bit stays clear in `known`. Spellings the code does not know, a value out of range, a line that is missing, and MAXIMUM LENGTH without a resolution all count.
- Nothing is guessed. For example, `SOMETHING NEW` for PRINT METHOD, and `9 IPS` with no RESOLUTION, both stay unknown (test_main.c:561-570).
- The form leaves the whole control out (`send_printer_form()`, web.c:1002). The post ignores the field (web.c:1542-1544). The `cfg` object of `GET /api/printer_config` omits it (web.c:795-805).
- The Status page still shows the line, through its own wording or the raw ^HH text. `val()` falls back to `nice(k, v)` when the name is not in `cfg` (web.c:450-451).
- No raw value appears on the form.

Form encoding and the post:

- The form's value for a field is `zpl_cfg_format()` (zpl_status.c:841): the letter itself, or `%d`.
- A choice field is a `<select name=<key>>` (web.c:951-966).
- A number field with a unit ("dots" or "dot rows" in `s_printer_ui`) becomes `<input name=<key>_mm type=number step=any>` when dpmm > 0 (web.c:924-938). Its min, max and value are the dot range and current value in mm, by `fmt_mm_exact()`. That is to 0.01 mm, with trailing zeros dropped, using the true dots/mm (web.c:900).
- A live hint shows `Math.round(mm × true dpmm)` dots.
- Without dpmm, and for `darkness`, the field is `<input name=<key> type=number step=1>` with the dot range (web.c:940-945).
- Fixture examples at 12/MM (true 11.811 dots/mm): width 1200 is 101.6 mm, length 1938 is 164.08 mm, maximum length 11700 is 990.6 mm, and the head 1280 is 108.37 mm.
- `printer_config_post()` (web.c:1507) works field by field, and only for known fields. When dpmm > 0 and `<key>_mm` is present, `parse_mm()` (web.c:1491) converts it. It accepts `strtod` text with nothing after it, in -10000..10000 mm, rounded to dots with `zpl_mm_to_dots()` (true dpmm, `lround`). The result is then checked as dots by `zpl_cfg_parse()`.
- Otherwise the post reads `<key>` and checks it with `zpl_cfg_parse()` (zpl_status.c:822). A LETTER field takes exactly one of its letters, case sensitive. A PERCENT_OR_LETTER field takes a letter or a strict integer. An INT field takes a strict integer: `15.0`, ` 15`, `1e1` and the empty string are refused (test_main.c:603-607).
- Any failure answers 400 `<key> is not a number of mm; nothing was sent.` or `<key> is not a valid value for this printer; nothing was sent.`, and nothing is sent.
- Values equal to the cached reading are not sent. If nothing changed, the page redirects to `/printer?done=same` (web.c:1564-1571).

### Text maps

Each map is an exact, case-sensitive match on the trimmed ^HH value (`map_text()`, zpl_status.c:579). Maps are at zpl_status.c:565-577. Form option texts are from `s_printer_ui`, web.c:824-847.

| Map (field) | ^HH text → stored value | Form options (`value=label`) |
|---|---|---|
| `s_method_map` (PRINT METHOD) | `DIRECT-THERMAL` → D; `THERMAL-TRANS.` → T; `THERMAL-TRANS` → T | `D=Direct thermal`, `T=Thermal transfer` |
| `s_mode_map` (PRINT MODE) | `TEAR OFF` → T; `PEEL OFF` → P; `REWIND` → R; `CUTTER` → C; `DELAYED CUT` → D | `T=Tear off`, `P=Peel off`, `R=Rewind`, `C=Cutter`, `D=Delayed cut` |
| `s_media_map` (MEDIA TYPE) | `GAP/NOTCH` → Y; `MARK` → M; `CONTINUOUS` → N | `Y=Gap/notch`, `M=Mark`, `N=Continuous` |
| `s_sensor_map` (SENSOR SELECT) | `AUTO` → A; `MANUAL` → M | `A=Automatic, found when it calibrates`, `M=Fixed, as set in Media type` |
| `s_zpl_map` (ZPL MODE) | `ZPL II` → 2; `ZPL` → 1 | `2=ZPL II`, `1=ZPL` |
| `s_feed_map` (MEDIA POWER UP and HEAD CLOSE) | `FEED` → F; `CALIBRATION` → C; `LENGTH` → L; `NO MOTION` → N | `FEED_OPTIONS`: `F=Feed`, `C=Calibrate`, `L=Measure length`, `N=No motion` |
| `s_backfeed_map` (BACKFEED) | `DEFAULT` → N; `AFTER` → A; `BEFORE` → B; `OFF` → O. A number with `%` (e.g. `60%`) → that number. | `N=Default (90% after printing)`, `A=After printing`, `B=Before the next label`, `O=Off`, then `10=10% after` up to `90=90% after` in steps of 10 |
| `s_early_map` (EARLY WARNING) | `MAINT. OFF` → D; `MAINT. ON` → E | `D=Off`, `E=On` |
| (PRINT SPEED, not a map) | `<n> IPS` → n | `2=51 mm/s (2 ips)`, `3=76 mm/s (3 ips)`, `4=102 mm/s (4 ips)`, `5=127 mm/s (5 ips)`, `6=152 mm/s (6 ips)` |

A backfeed value is a letter only if it is one of N, A, B and O (`is_letter_value()`, zpl_status.c:796). So 70, 80 and 90 stay percentages, and are not taken as 'F', 'P' and 'Z'. The form refuses `F`, `P` and `Z` (test_main.c:780-790).

The fixture GX430t (firmware/test_host/fixtures/gx430t_hh.bin) decodes as follows (test_main.c:536-561):

- `15.0` → 15
- `4 IPS` → 4
- `DIRECT-THERMAL` → D
- `TEAR OFF` → T
- `GAP/NOTCH` → Y
- `AUTO` → A
- `ZPL II` → 2
- `1200` and `1938` stay as they are
- `39.0IN   975MM` → 11700
- `+000`, `+000` and `+0000` → 0
- `NO MOTION` → N
- `FEED` → F
- `DEFAULT` → N
- `MAINT. OFF` → D

### Resolution line

- The line is `RESOLUTION`, e.g. `1280 12/MM FULL` (fixture).
- `zpl_cfg_from_hh()` (zpl_status.c:719-727) parses it as an integer (optional sign, up to 6 digits), then exactly one space, then an integer, then `/MM`. Anything after `/MM` (`FULL`) is ignored. Both numbers must be above 0.
- The first number becomes `head_dots` (head width in dots, 1280). The second becomes `dpmm`, the dots/mm the printer reports (12). If the line is missing or does not match, both stay 0.
- The Status page parses dots/mm separately in JavaScript, with `/ (\d+)\/MM/` (web.c:527).

The reported dpmm is used for:

- the speed limit (`dpmm == 12` gives at most 4 ips)
- the Maximum length minimum (50 × dpmm)
- decoding MAXIMUM LENGTH mm into dots (`t * cfg->dpmm`)

The code says why MAXIMUM LENGTH uses it: "The printer converts mm with its rounded dots/mm (12 for 300 dpi), so this stays dpmm, not zpl_units.h" (zpl_status.c:663-669).

`head_dots` sets the Print width maximum.

The true dpmm, `zpl_true_dpmm()` in firmware/main/zpl_units.h:21, is 300/25.4 (11.811) for 12, 600/25.4 (23.622) for 24, and dpmm itself otherwise. It is used for every mm figure the bridge shows or reads:

- `fmt_mm_exact()` and the live dot hint on the form
- `parse_mm()` through `zpl_mm_to_dots()` on the post
- `tdm()` on the Status page (web.c:405)

The header explains this. On a GX430t a 240-dot circle measured 20.57 mm, which fits 300 dpi, and the bridge converts with the true figure "so that a size typed in mm prints at that size" (zpl_units.h:5-18).

### Choices a printer cannot take

The rule is in `send_printer_field()` (web.c:913-966). Each `value=label` in a field's `s_printer_ui` options is offered only if `zpl_cfg_parse(cur, field, value)` accepts it (web.c:958). That is, the value must be one of the field's `letters`, or an integer within `zpl_cfg_range()` for this printer. The current value gets `selected`.

There are no per-model tables. The only limits that depend on the printer come from RESOLUTION:

- Print speed: options 5 and 6 are left out when dpmm is 12, so a GX430t gets 2, 3 and 4 (test_main.c:610, 723-733). Any other dpmm, or none, offers 2 to 6.
- Print width: the maximum is head_dots. This is a number field, so it is enforced through the HTML `max` and the post check, not by leaving out options.
- Maximum label length: the minimum is 50 × dpmm (number field).

The fixed letter sets also mean ZPL letters that have no option are never offered or accepted. For example, `^MMA` (applicator) is not in `TPRCD`.

A field the printer did not report, or reported in an unknown spelling, is left out of the form as a whole (web.c:1002).

### Maximum length limit

The limit uses the reported dots/mm, not the true one. The code is in `zpl_cfg_range()`, zpl_status.c:768-771:

```c
    case ZPL_CFG_MAX_LENGTH:
        /* ^ML: the dpi times two, up to the maximum label length (^LL's 32000 here) */
        *min = cfg->dpmm > 0 ? cfg->dpmm * 25 * 2 : 2;
        *max = 32000;
```

- The minimum is therefore 50 × reported dpmm dots: 600 at 12/MM, 400 at 8/MM, 1200 at 24/MM. When dpmm is unknown it is 2, but then the field is unknown anyway.
- The test treats 600 as "300 dpi x 2", and refuses 599 (test_main.c:623-624).
- On the form, this dot limit is shown in mm with the true dpmm. At 12/MM the minimum shows as 50.8 mm and the maximum, 32000 dots, as 2709.33 mm.

### Status page configuration: data and page parts

The Status page (`root_get`, web.c 546-559) sends `STATUS_HTML` (web.c 385-392), then the `UI` script from `send_ui_script()` (web.c 873-893), then `STATUS_SCRIPT` (web.c 394-542). `UI` must be defined before `STATUS_SCRIPT` runs.

Page parts used by the configuration display (web.c 389-391):

| Element | Purpose |
|---|---|
| `<p id=cs>` | Status line for the configuration read. Starts as "Reading...". |
| `<input type=checkbox id=raw onchange=draw()>` | Label "Also show the printer's own wording". Redraws on change. |
| `<div id=c>` | The grouped tables. |
| `<button onclick=cfg(1)>` | "Read the configuration from the printer again". |

Data source: `GET /api/printer_config[?refresh=1]` (`printer_config_get`, web.c 753-808). The reply is `{"age_ms":N,"error":null|"text","pairs":[["NAME","value"],...],"cfg":{"NAME":"text",...}}`.

- `pairs` are the ^HH lines in printer order, as the printer worded them.
- `cfg` holds only the settings the Printer page form changes, decoded, keyed by ^HH name. A value is a letter for letter fields (for example `"T"`) and a decimal integer otherwise (`zpl_cfg_format`, zpl_status.c 841-849). Only fields the bridge could decode are present.
- `age_ms` is -1 when there is no reading.

`UI` (web.c 870-893) is `{'^HH name':[label, group, inDots, {value: option text}]}`, built from `s_printer_ui` (web.c 824-848). `inDots` is 1 when the field has a unit ("dots" or "dot rows") and 0 otherwise. The options object is empty for a number field.

| ^HH name | UI label | inDots | Options (value: text) |
|---|---|---|---|
| DARKNESS | Darkness | 0 | none |
| PRINT SPEED | Print speed | 0 | 2: 51 mm/s (2 ips), 3: 76 mm/s (3 ips), 4: 102 mm/s (4 ips), 5: 127 mm/s (5 ips), 6: 152 mm/s (6 ips) |
| PRINT METHOD | Print method | 0 | D: Direct thermal, T: Thermal transfer |
| PRINT MODE | Print mode | 0 | T: Tear off, P: Peel off, R: Rewind, C: Cutter, D: Delayed cut |
| PRINT WIDTH | Print width | 1 | none |
| LABEL LENGTH | Label length | 1 | none |
| TEAR OFF | Tear-off adjust | 1 | none |
| LABEL TOP | Label top | 1 | none |
| LEFT POSITION | Left position | 1 | none |
| BACKFEED | Backfeed | 0 | N: Default (90% after printing), A: After printing, B: Before the next label, O: Off, 10: 10% after, 20: 20% after, ... 90: 90% after (steps of 10) |
| MEDIA TYPE | Media type | 0 | Y: Gap/notch, M: Mark, N: Continuous |
| SENSOR SELECT | Media type detection | 0 | A: Automatic, found when it calibrates; M: Fixed, as set in Media type |
| MAXIMUM LENGTH | Maximum label length | 1 | none |
| MEDIA POWER UP | Media at power up | 0 | F: Feed, C: Calibrate, L: Measure length, N: No motion |
| HEAD CLOSE | Media at head close | 0 | F: Feed, C: Calibrate, L: Measure length, N: No motion |
| ZPL MODE | ZPL mode | 0 | 2: ZPL II, 1: ZPL |
| EARLY WARNING | Head maintenance early warning | 0 | D: Off, E: On |

The ^HH names come from `s_cfg_desc` (zpl_status.c 533-551). The SENSOR SELECT option text contains a comma inside one option; the options string separates options with `|`, not commas.

### Status page configuration groups

From the `G` array (web.c 509-518), in this order:

| Group | ^HH lines, in order |
|---|---|
| Print | DARKNESS, PRINT SPEED, PRINT METHOD, PRINT MODE, PRINT WIDTH, LABEL LENGTH, TEAR OFF, LABEL TOP, LEFT POSITION, BACKFEED |
| Media | MEDIA TYPE, SENSOR SELECT, MAXIMUM LENGTH, MEDIA POWER UP, HEAD CLOSE |
| Communications | SER COMM. MODE, USB COMM., PARALLEL COMM., CONTROL CHAR, COMMAND CHAR, DELIM. CHAR, ZPL MODE |
| Sensors | SENSOR TYPE, WEB S., MEDIA S., WEB GAIN, MARK S., MARK GAIN, MARK MED S., MARK MEDIA GAIN, CONT MEDIA S., CONT MEDIA GAIN, RIBBON OUT, RIBBON GAIN, TAKE LABEL |
| Firmware and hardware | FIRMWARE, HARDWARE ID, SERIAL NUMBER, RESOLUTION, RAM, ONBOARD FLASH, XML SCHEMA, CONFIGURATION, ZBI, ZBI VERSION, FORMAT CONVERT, MODES ENABLED, MODES DISABLED, HEXDUMP |
| Usage and maintenance | HEAD USAGE, TOTAL USAGE, RESET CNTR1, RESET CNTR2, LAST CLEANED, EARLY WARNING |

How `draw()` (web.c 525-535) builds the groups:

1. `m` maps each ^HH name to its value. The first pair with a name wins; later duplicates are ignored for `m`.
2. The serial lines BAUD, DATA BITS, PARITY, HOST HANDSHAKE and PROTOCOL (`SERN`, web.c 520) are marked seen first. They never appear in the configuration tables. Their summary goes to the Serial link row instead (see below).
3. For each group, each member present in `m` becomes a row (`srow`), in `G` order, not printer order. A group with no present member is left out entirely.
4. A group's heading is `<h4>` with the group name. If any of its rows is in both `UI` and `cfg`, the heading gets ` <small><a href="/printer#g-NAME">change</a></small>`. NAME is the first word of the group name in lower case: `g-print`, `g-media`, `g-communications`, `g-sensors`, `g-firmware`, `g-usage`. The Printer page makes the same anchors (web.c 968-971).
5. The Sensors group has the sensor bar (`sbar`) between its heading and its table.
6. Every pair whose name is not seen goes into a final group "Other", in printer order. That loop does not mark names seen.
7. The whole result replaces `#c`.

### Friendly names

`nm(k)` (web.c 429) names a row: the `UI` label if `UI` has the name, else the `FN` name, else the ^HH name unchanged. `FN` (web.c 414-427):

| ^HH line | FN name |
|---|---|
| SENSOR TYPE | Media sensing |
| WEB S. | Gap sensor: gap reading |
| MEDIA S. | Gap sensor: label reading |
| WEB GAIN | Gap sensor gain |
| MARK S. | Mark sensor: mark reading |
| MARK MED S. | Mark sensor: media reading |
| MARK GAIN | Mark sensor gain |
| MARK MEDIA GAIN | Mark sensor media gain |
| CONT MEDIA S. | Continuous media reading |
| CONT MEDIA GAIN | Continuous media gain |
| RIBBON OUT | Ribbon-out threshold |
| RIBBON GAIN | Ribbon sensor gain |
| TAKE LABEL | Label-taken sensor threshold |
| ONBOARD FLASH | Onboard flash |
| BAUD | Serial speed |
| DATA BITS | Data bits |
| PARITY | Parity |
| HOST HANDSHAKE | Handshake |
| PROTOCOL | Protocol |
| SER COMM. MODE | Serial port mode |
| USB COMM. | USB port |
| PARALLEL COMM. | Parallel port |
| CONTROL CHAR | Control command prefix |
| COMMAND CHAR | Format command prefix |
| DELIM. CHAR | Delimiter |
| FIRMWARE | Firmware |
| HARDWARE ID | Hardware ID |
| SERIAL NUMBER | Serial number |
| RESOLUTION | Resolution |
| XML SCHEMA | XML schema |
| CONFIGURATION | Configuration |
| ZBI VERSION | ZBI version |
| FORMAT CONVERT | Format conversion |
| MODES ENABLED | Modes enabled |
| MODES DISABLED | Modes disabled |
| HEXDUMP | Hex dump |
| HEAD USAGE | Printhead usage |
| TOTAL USAGE | Total printed |
| RESET CNTR1 | User counter 1 |
| RESET CNTR2 | User counter 2 |
| LAST CLEANED | Since the head was cleaned |

RAM and ZBI are in `G` but in neither `UI` nor `FN`, so they show their ^HH names. The five serial names in `FN` are never shown, because those lines are suppressed (step 2 above).

### Dimmed rows

`unused(k, m)` (web.c 435-441) returns a note, or an empty string for a row in use. `t` is the raw MEDIA TYPE value, or an empty string if that line is missing. The rules are tried in this order and the first match decides:

| Rows (test on the ^HH name) | In use when | Note otherwise |
|---|---|---|
| exactly `WEB S.`, `MEDIA S.` or `WEB GAIN` | `t` contains GAP, NOTCH or WEB | not used with this media |
| name starts with `MARK` (MARK S., MARK GAIN, MARK MED S., MARK MEDIA GAIN) | `t` contains MARK | used for black-mark media only |
| name starts with `CONT ` (CONT MEDIA S., CONT MEDIA GAIN) | `t` contains CONT | used for continuous media only |
| name starts with `RIBBON` (RIBBON OUT, RIBBON GAIN) | PRINT METHOD does not contain DIRECT | not used in direct thermal mode |
| exactly `TAKE LABEL` | PRINT MODE contains PEEL | used in peel-off mode only |

All tests are case-sensitive substring tests on the printer's raw text in `m`, not on `cfg`. A missing MEDIA TYPE or PRINT MODE line counts as not matching, so those rows are dimmed. A missing PRINT METHOD leaves the ribbon rows undimmed. The rules apply to rows in any group, including Other, because `srow` always gets `m`.

A dimmed row (`srow`, web.c 453-455) has `style=opacity:.45` on its `<tr>`. The value cell gets ` <small>(note)</small>` after the value.

### Sensor bar

`sbar(m)` (web.c 457-465) draws where the gap and label readings sit on the printer's 0 to 100 scale.

- Inputs: `w = +m['WEB S.']` (gap reading) and `l = +m['MEDIA S.']` (label reading). These are the raw ^HH values, not `cfg`.
- Drawn only when both are numbers from 0 to 100 inclusive and MEDIA TYPE contains GAP, NOTCH or WEB. Otherwise it returns nothing.
- Only reached when the Sensors group has at least one row.
- First a paragraph: `<p><small>Sensor readings are on the printer's 0 to 100 scale and are set when it calibrates (Calibrate media on the Printer page). The further apart the gap and label readings, the more easily it tells them apart.</small></p>`.
- Then an SVG, `viewBox="0 0 108 17"`, `style="width:100%;max-width:26em"`:

| Part | Drawing |
|---|---|
| Scale line | x 4 to 104 at y 6, `stroke=currentColor`, `stroke-opacity=.3`, `stroke-width=1.5`. One unit of the reading is one SVG unit. |
| Gap marker | circle at cx = 4 + w, cy 6, r 2.5, fill `#c60`. Text "gap W" at x = 4 + w, y 15. |
| Label marker | circle at cx = 4 + l, cy 6, r 2.5, fill `#06c`. Text "label L" at x = 4 + l, y 15. |

The text is `font-size=4.5`, `text-anchor=middle`, `fill=currentColor`. The gap marker is drawn first, so the label marker is on top where they overlap. There are no tick marks or end labels.

### Value formatting

Each row is `srow(k, v, m)` (web.c 453-455): name cell `nm(k)`, value cell `val(k, v)`. If `val` gives `null`, the raw value is shown. All text is HTML-escaped by `e()` (web.c 397), which writes each of `& < > " '` as a numeric entity.

`val(k, v)` (web.c 450-451), for a row in both `UI` and `cfg` (`c = cfg[k]`, a string):

1. If the options object has `c` as a key, show the option text. Example: PRINT METHOD `T` gives "Thermal transfer".
2. Else if `inDots` is 1: with `D` known, show `len(c / tdm(D)) + ' (' + c + ' dots)'`, for example "104 mm (832 dots)". Without `D`, show `c + ' dots'`. The word is "dots" for "dot rows" fields too.
3. Else if the options object is empty (a plain number such as DARKNESS), show `c` as it is.
4. Else (options exist but `c` is not one of them, for example a backfeed percentage not in the list), fall through to `nice(k, v)` on the raw value.

Any other row uses `nice(k, v)` (web.c 442-448) on the raw value, in this order:

| Rule | Result |
|---|---|
| k is FIRMWARE | value with a trailing `<-` (and the spaces before it) removed |
| value matches `^(\d+)k[ .]*([A-Z]):$` (memory lines such as `4096k...........R:`) | "N KB" |
| k is SENSOR TYPE and value is a key of `ST` | the `ST` text |
| k ends in ` S.`, or is RIBBON OUT or TAKE LABEL, and value is all digits | "N of 100" (leading zeros dropped) |
| k ends in GAIN and value is all digits | the number, leading zeros dropped |
| otherwise | `si(k, v)` |

`ST` (web.c 430): WEB is "gaps between labels", MARK is "black marks", CONTINUOUS is "continuous media".

`si(n, v)` (web.c 406-412), first match wins:

| Value pattern | Result |
|---|---|
| `^(\d+) IPS$` | round(N x 25.4) + " mm/s" |
| `^(\d+) (\d+)/MM` (RESOLUTION, head dots then dots/mm) | `dpi(d)` + " dpi (" + `tdm(d)` rounded to 0.01 + " dots/mm), head " + N + " dots = " + `len(N / tdm(d))`. Example for `832 8/MM`: "203 dpi (8 dots/mm), head 832 dots = 104 mm" |
| `(\d+)MM$` (not anchored at the start) | the digits as written + " mm". Leading zeros are kept. |
| `^([\d,]+(\.\d+)?) ?IN$` | commas removed, inches x 25.4, through `len()` |
| `D` known, n is one of PRINT WIDTH, LABEL LENGTH, LEFT POSITION, TEAR OFF, LABEL TOP (`DOTS`, web.c 400), and value matches `^[+-]?\d+$` | number (sign and leading zeros normalised) + " dots = " + `len(v / tdm(D))` |
| otherwise | `null`, so the raw value shows |

The `DOTS` rule is only reached when `val` did not handle the row, that is when `cfg` lacks the field.

Helpers:

| Function | Behaviour |
|---|---|
| `len(mm)` (web.c 401-402) | Below 1000 mm: rounded to 0.1, " mm". Below 1,000,000 mm: metres to 0.1, " m". Otherwise kilometres to 0.01, " km". A leading "-" for negative input. Trailing zeros are dropped by the number formatting, so 104.0 shows as "104 mm". |
| `dpi(d)` (web.c 403) | 8 gives 203, 12 gives 300, 24 gives 600, anything else round(d x 25.4). |
| `tdm(d)` (web.c 404-405) | The true dots per mm: 12 and 24 become 300/25.4 and 600/25.4 (11.81 and 23.62). Any other value is returned as it is. This matches `zpl_true_dpmm` in zpl_units.h. |
| `D` (web.c 400, set in `draw`, web.c 527) | The integer from ` (\d+)/MM` in the raw RESOLUTION value, or 0 if absent. It is the printer's rounded figure (8, 12, 24). Every length conversion divides by `tdm(D)`, not by `D` itself. |
| `dur(s)` (web.c 466-467) | "Xd Yh Zm". Days only if non-zero; hours if days or hours are non-zero; minutes always. Used in the Printer and Bridge tables, not the configuration. |
| `flags(h)` (web.c 468) | The keys of `p.hs` whose value is exactly `true`, the first `_` in each turned into a space, joined with ", ". "ok" if none. Used in the Status row of the Printer table. |

### Other behaviour a rebuilder needs

Loading and refresh (`cfg`, web.c 536-541):

- `cfg(0)` runs once when the page loads. The button calls `cfg(1)`, which adds `?refresh=1` so the bridge asks the printer again. There is no timed refresh of the configuration.
- `cfg` sets `#cs` to "Reading...", fetches, stores the reply in `LC` and calls `draw()`.
- `#cs` then reads: the error text and ". " if `error` is set, followed by, when `age_ms` is 0 or more, "Read from the printer N s ago. Hold the pointer over a row to see the wording the printer uses." N is `age_ms / 1000` rounded. With neither, `#cs` is empty.
- If the fetch or JSON parse fails, `#cs` reads "Could not load the printer configuration." and the tables are left as they were.

RAW toggle and tooltips (`srow`, `draw`):

- `draw()` reads the checkbox into `RAW` each time. It does nothing until a reply has arrived.
- Every row has a `title` tooltip of the printer's wording, "NAME: raw value".
- With RAW on, the same text also shows under the value as `<br><small>NAME: raw value</small>`, after any dimmed-row note.

Serial settings (`ser`, web.c 521-524; Serial link row, web.c 487-488):

- `draw()` sets `SER` from the raw lines: "N data bits" from the leading digits of DATA BITS; "no parity" if PARITY is NONE, else the lower-cased PARITY + " parity"; "protocol " + lower-cased PROTOCOL if present and not NONE. They are joined with ", ".
- BAUD and HOST HANDSHAKE are not used by `ser`. The Serial link row takes baud and handshake from `/api/status` (`p.baud`, `p.handshake`).
- The Serial link row is "BAUD baud, SER, HANDSHAKE", then a new line "TX D3 (GPIO21), RX D5 (GPIO23)". The pin names come from `PIN` (web.c 433). If the configured target baud differs from the current baud, a further line says "set to N baud on the Bridge page".
- `SER` is empty until the configuration has been read, so the first Serial link row has no serial text.

Status polling (web.c 506-507): `upd()` fetches `/api/status` on load, then every 2000 ms while the tab is visible, and once each time the tab becomes visible again. It fills the Printer and Bridge tables only. It does not re-read the configuration.

### Known deviations
None. The last ones were fixed in the change archived as `2026-10-31-fix-known-deviations`.
