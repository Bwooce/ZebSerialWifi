# http-print Specification

## Purpose
The `POST /pstprnt` route of the bridge's web server: the request body is sent to the printer as one print job, the
way Zebra's own network printers take one, so a web page from any site can print. It lives in
`firmware/main/web.c` (`pstprnt_post()` and helpers) and sends the job through `bridge_send_job()` in `bridge.c`.

## Requirements

### Requirement: Route and method
The bridge SHALL register `/pstprnt` for POST and OPTIONS. An OPTIONS request SHALL get 204 with no body and the
headers `Access-Control-Allow-Origin: *`, `Access-Control-Allow-Methods: POST, OPTIONS`,
`Access-Control-Allow-Headers: Content-Type`, `Access-Control-Allow-Private-Network: true` and
`Access-Control-Max-Age: 600`, and SHALL NOT touch the printer link. Other methods get the server's 405.

**Source**: `firmware/main/web.c` `web_start()` (`"/pstprnt"`, `HTTP_POST` and `HTTP_OPTIONS`), `pstprnt_options()`

#### Scenario: GET request
- **WHEN** a client sends `GET /pstprnt`
- **THEN** the bridge SHALL reply 405 and send nothing to the printer

#### Scenario: CORS preflight
- **WHEN** a browser sends `OPTIONS /pstprnt` as a preflight for a POST with `Content-Type: application/json`
- **THEN** the bridge SHALL reply 204 with the CORS headers above, and the browser then sends the POST

#### Scenario: Chrome private network preflight
- **WHEN** the preflight carries `Access-Control-Request-Private-Network: true`
- **THEN** the reply SHALL carry `Access-Control-Allow-Private-Network: true`

### Requirement: Any-origin access without Host or CSRF checks
The `/pstprnt` handler SHALL NOT call `host_allowed()` or `same_origin()`: it MUST accept a POST whatever its Host,
Origin or Referer headers. Every reply it sends SHALL carry `Access-Control-Allow-Origin: *`, so a page from any
site can read it after a request that needs no preflight. The state-changing POST routes, by contrast, refuse a
cross-site request with 403.

**Source**: `firmware/main/web.c` `pstprnt_post()`, `pstprnt_reply()`, `same_origin()`, `host_allowed()`, `cross_site()`

#### Scenario: Saved label page
- **GIVEN** a page opened from a file or another host posts ZPL with `Content-Type: text/plain`
- **WHEN** the request reaches the bridge with a foreign Origin header
- **THEN** the bridge SHALL print the body and the page SHALL be able to read the reply

#### Scenario: Unknown Host header
- **WHEN** a POST to `/pstprnt` carries a Host header that is not one of the bridge's names or addresses
- **THEN** the bridge SHALL still accept the job

### Requirement: Refused on the setup network
The bridge SHALL answer a `/pstprnt` request that arrived over the setup AP with `403 Forbidden` and the body
`Printing is not available on the setup network.` followed by a newline. This check MUST come first, before the body
length is looked at, and nothing is sent to the printer.

**Source**: `firmware/main/web.c` `pstprnt_post()`, `on_ap()`; `firmware/main/wifi_prov.c` `wifi_prov_sock_on_ap()`

#### Scenario: Phone on the setup AP
- **GIVEN** the setup AP is active
- **WHEN** a client on the AP network posts a job to `/pstprnt`
- **THEN** the bridge SHALL reply 403 and SHALL NOT take the printer link

### Requirement: Empty body refused
The bridge SHALL answer a `/pstprnt` request whose content length is 0, including one with no Content-Length header,
with `400 Bad Request` and the body `Send the print job (ZPL) as the request body.` followed by a newline. It MUST NOT
check the Content-Type, impose any upper limit on the body size, or look at the body's content.

**Source**: `firmware/main/web.c` `pstprnt_post()`

#### Scenario: POST with no body
- **WHEN** a client sends `POST /pstprnt` with `Content-Length: 0`
- **THEN** the bridge SHALL reply 400 and send nothing to the printer

#### Scenario: Large job
- **WHEN** a client posts a 2 MB graphic job
- **THEN** the bridge SHALL stream the whole body to the printer

### Requirement: Body streamed to the printer as it arrives
The bridge SHALL pass the body to `bridge_send_job()` through a read callback that reads at most the remaining
content length, 1024 bytes at a time, with `httpd_req_recv`, so the body is not buffered whole. A read that times out
(5 s, the server's default `recv_wait_timeout`) MUST be tried 3 times in all before the sender counts as gone.

**Source**: `firmware/main/web.c` `pstprnt_read()`, `pstprnt_body_t`, `web_start()`; `firmware/main/bridge.c` `bridge_send_job()`, `CHUNK`

#### Scenario: Slow upload
- **WHEN** the client pauses for 8 s in the middle of the body
- **THEN** the bridge SHALL keep the job and continue reading

### Requirement: Shared link, statistics and replies dropped
An HTTP job SHALL take the same printer link as a port 9100 client and be counted in the same statistics, with the
client's address as `client_ip` (an IPv4-mapped IPv6 peer shown as dotted IPv4). Anything the printer sends back
during the job MUST be dropped; the HTTP client never sees printer replies.

**Source**: `firmware/main/web.c` `pstprnt_post()`; `firmware/main/bridge.c` `bridge_send_job()`, `drop_replies()`

#### Scenario: Status query over HTTP
- **WHEN** a client posts `~HS` to `/pstprnt`
- **THEN** the reply body SHALL be only the byte count, not the printer's status

### Requirement: Success reply
When `bridge_send_job()` returns `ESP_OK` the bridge SHALL reply `200 OK`, `Content-Type: text/plain`,
`Cache-Control: no-store`, `Access-Control-Allow-Origin: *`, with the body `Sent N bytes to the printer.` and a
newline, where N MUST be the number of bytes queued to the printer UART. The reply is sent only after the job drained.

**Source**: `firmware/main/web.c` `pstprnt_post()`, `pstprnt_reply()`

#### Scenario: Label printed
- **WHEN** a client posts a 120-byte ZPL label and the printer takes it
- **THEN** the reply SHALL be 200 with the body `Sent 120 bytes to the printer.`

### Requirement: Busy reply after 15 s
When `bridge_send_job()` returns `ESP_ERR_TIMEOUT` with no byte sent, the bridge SHALL reply `503 Service
Unavailable` with the body `The printer is busy with another job. Try again.` and a newline. This MUST happen when
the printer link stays held for 15 s by any holder: a port 9100 job, another HTTP job, detection, polling or a
configuration command.

**Source**: `firmware/main/web.c` `pstprnt_post()`; `firmware/main/bridge.c` `HTTP_ACQUIRE_WAIT_MS`

#### Scenario: Port 9100 job in progress
- **GIVEN** a port 9100 client holds the printer link for more than 15 s
- **WHEN** a client posts a job to `/pstprnt`
- **THEN** the bridge SHALL reply 503 after about 15 s and send nothing to the printer

### Requirement: Stall reply after 30 s
When `bridge_send_job()` returns `ESP_ERR_TIMEOUT` after some bytes were sent, the bridge SHALL reply `504 Gateway
Timeout` with a body saying the printer stopped taking data after N bytes and the rest of the job was dropped. The
stall limit MUST be 30 s (`PSTPRNT_STALL_MS`), not the 30 minutes of a port 9100 job.

**Source**: `firmware/main/web.c` `pstprnt_post()`, `PSTPRNT_STALL_MS`; `firmware/main/bridge.c` `bridge_send_job()`

#### Scenario: Paper runs out
- **WHEN** the printer holds XOFF for more than 30 s during an HTTP job
- **THEN** the bridge SHALL discard the queued rest of the job, count it in `aborted` and reply 504
- **AND** the body SHALL begin `The printer stopped taking data (paper or ribbon out, or head open?) after`

### Requirement: No reply when the sender goes away
When `bridge_send_job()` returns `ESP_FAIL` because the body stopped arriving, the handler SHALL return `ESP_FAIL`
without sending a reply, so the HTTP server closes the connection. The part of the body that had arrived MUST still
be sent to the printer.

**Source**: `firmware/main/web.c` `pstprnt_post()`, `pstprnt_read()`; `firmware/main/bridge.c` `bridge_send_job()`

#### Scenario: Browser tab closed mid-upload
- **WHEN** the client disconnects after sending half the body
- **THEN** the received half SHALL drain to the printer and no HTTP reply SHALL be sent

### Requirement: Web server waits for the job
The bridge SHALL run an HTTP job on the web server's task, so the request MUST hold that task until the job ends:
up to 15 s waiting for the link, then the time the printer takes the job, with a 30 s stall limit. The bridge SHALL
log the client address, bytes sent of the content length, elapsed time and result of each job.

**Source**: `firmware/main/web.c` `pstprnt_post()`, `PSTPRNT_STALL_MS`; `firmware/main/bridge.h` `bridge_send_job()`

#### Scenario: Page requested during an HTTP job
- **GIVEN** an HTTP print job is waiting for the printer
- **WHEN** another client asks for the Status page
- **THEN** that request SHALL be answered only after the job's handler returns

## Notes

| Case | Status | Body |
|---|---|---|
| Over the setup AP | 403 Forbidden | `Printing is not available on the setup network.` |
| Content length 0 | 400 Bad Request | `Send the print job (ZPL) as the request body.` |
| Job sent | 200 OK | `Sent N bytes to the printer.` |
| Link busy 15 s | 503 Service Unavailable | `The printer is busy with another job. Try again.` |
| Printer stalled 30 s | 504 Gateway Timeout | `The printer stopped taking data ... after N bytes; the rest of the job was dropped.` |
| Sender went away | none | connection closed |
| Method other than POST | 405 (server default) | server default text, no CORS header |

All handler replies are `text/plain` with `Access-Control-Allow-Origin: *` and `Cache-Control: no-store`.

### Known deviations
None. The last ones were fixed in the change archived as `2026-10-31-fix-known-deviations`.
