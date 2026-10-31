# print-bridge Specification

## Purpose
The raw print server: a TCP listener on port 9100 that streams a client's job to the printer's UART and passes the
printer's replies back, plus `bridge_send_job` for jobs that arrive over HTTP, and the job statistics shown on the
Status page. It lives in `firmware/main/bridge.c` and `bridge.h` and uses the printer link in `printer_link.c`.

## Requirements

### Requirement: Dual-stack listener on the raw print port
The bridge SHALL listen for TCP connections on one AF_INET6 socket bound to the any-address, which in lwIP also
accepts IPv4, on port `CONFIG_ZEB_TCP_PORT` (default 9100), with `SO_REUSEADDR` and a listen backlog of 4. If bind
or listen fails, the bridge MUST log the error and stop the listener task without retrying.

**Source**: `firmware/main/bridge.c` `listen_task()`, `BACKLOG`; `firmware/main/Kconfig.projbuild` `ZEB_TCP_PORT`

#### Scenario: IPv4 and IPv6 clients
- **WHEN** a client connects to port 9100 over IPv4 or over IPv6
- **THEN** the bridge SHALL accept the connection on the same listening socket

#### Scenario: Port cannot be bound
- **WHEN** `bind()` or `listen()` fails at start-up
- **THEN** the bridge SHALL log "cannot listen on port" with the errno
- **AND** no print server SHALL run until the next restart

### Requirement: One client at a time
The bridge SHALL serve one port 9100 client at a time from a single task: it calls `accept()` again only after the
current session has ended. Later clients MUST wait in the listen backlog (4 deep) and are then served in turn; jobs
are queued in this way rather than refused.

**Source**: `firmware/main/bridge.c` `listen_task()`, `serve()`, `BACKLOG`

#### Scenario: Second client while a job prints
- **GIVEN** a client is being served
- **WHEN** a second client connects
- **THEN** the second connection SHALL wait in the backlog without being served
- **AND** it SHALL be accepted and served after the first session ends

### Requirement: Print connections refused on the setup access point
The bridge SHALL close, without reading from it, any accepted port 9100 connection whose local address is the setup
AP's address while the setup AP is active, and log a warning. A connection MUST also be refused when its local
address cannot be read. Such a connection SHALL NOT change the statistics.

**Source**: `firmware/main/bridge.c` `listen_task()`; `firmware/main/wifi_prov.c` `wifi_prov_sock_on_ap()`

#### Scenario: Client on the setup network
- **GIVEN** the setup AP is active
- **WHEN** a client on the AP network connects to port 9100
- **THEN** the bridge SHALL log "rejecting print connection from" the client address and close the socket
- **AND** `sessions` and `client_ip` SHALL be unchanged

#### Scenario: Client on the station network while the AP runs
- **GIVEN** the setup AP is active and the station is also connected
- **WHEN** a client connects through the station address
- **THEN** the bridge SHALL serve it as usual

### Requirement: Printer link lock shared with detection, polling and HTTP jobs
The bridge SHALL hold the printer link mutex (`printer_link_acquire`) for the whole of each session. A port 9100
client MUST wait up to 60 s (`ACQUIRE_WAIT_MS`) for it and is closed unserved if it stays busy. The same mutex is
taken by detection and `~HS` polling, configuration reads, ZPL commands from the web pages, pin changes and HTTP jobs,
so none of them runs during a session.

**Source**: `firmware/main/bridge.c` `listen_task()`, `ACQUIRE_WAIT_MS`; `firmware/main/printer_link.c` `printer_link_acquire()`, `link_task()`, `printer_link_set_pins()`, `printer_link_refresh_config()`, `printer_link_send_zpl()`

#### Scenario: Link busy for longer than 60 s
- **GIVEN** another holder keeps the printer link for more than 60 s
- **WHEN** a port 9100 client is accepted
- **THEN** the bridge SHALL log "printer link busy, rejecting" and close the socket without forwarding any data
- **AND** `client_ip` SHALL already show that client, while `sessions` SHALL be unchanged

#### Scenario: Poll due during a job
- **GIVEN** a session holds the link
- **WHEN** the link task's poll timer comes due
- **THEN** the link task SHALL skip the poll and try again later, sending nothing to the printer

### Requirement: Session start and end housekeeping
After taking the link the bridge SHALL discard any bytes waiting in the UART receive buffer (late replies to the
link task's probes), then count the session and mark it active. After every session it MUST shut down and close the
socket, clear `active`, release the link and call `printer_link_want_status()` so a printer that sleeps is asked for
its status after the job.

**Source**: `firmware/main/bridge.c` `listen_task()`; `firmware/main/printer_link.c` `printer_link_discard_input()`, `printer_link_want_status()`

#### Scenario: Stale probe reply
- **GIVEN** a late `~HS` reply to the link task's poll is still in the receive buffer
- **WHEN** a client's session starts
- **THEN** the bridge SHALL drop that reply instead of sending it to the client

#### Scenario: Session ends
- **WHEN** a session ends for any reason
- **THEN** the bridge SHALL close the client socket, release the printer link and request a status read

### Requirement: Client socket options
The bridge SHALL set on each served socket a 50 ms receive timeout, a 5 s send timeout, `TCP_NODELAY`, and TCP
keepalive with 10 s idle time, 5 s probe interval and 3 probes, so that a client that has vanished is noticed while
the printer holds XOFF.

**Source**: `firmware/main/bridge.c` `serve()`, `KEEPALIVE_IDLE_S`, `KEEPALIVE_INTVL_S`, `KEEPALIVE_COUNT`

#### Scenario: Peer powered off mid-job
- **GIVEN** the printer holds XOFF and the client's computer is switched off
- **WHEN** the keepalive probes go unanswered
- **THEN** the socket SHALL report an error, which the bridge SHALL treat as the client being gone

### Requirement: Cross-protocol HTTP requests refused on the print port
The bridge SHALL close the session without forwarding anything to the printer, and log a warning, when the first
chunk received from a client starts with `GET `, `PUT `, `POST ` or `OPTIONS ` (case-sensitive). Only the first
received chunk MUST be checked. No HTTP reply is sent. The session is still counted in `sessions`.

**Source**: `firmware/main/bridge.c` `is_cross_protocol()`, `serve()`

#### Scenario: Browser form posted to port 9100
- **WHEN** the first data from a client is `POST / HTTP/1.1` followed by headers
- **THEN** the bridge SHALL log "rejecting cross-protocol HTTP request on print port 9100" and close the connection
- **AND** no byte of the request SHALL reach the printer

#### Scenario: ZPL job
- **WHEN** the first data from a client is `^XA`
- **THEN** the bridge SHALL forward it to the printer

### Requirement: Job data streamed to the printer
The bridge SHALL read the client's data in chunks of up to 1024 bytes and queue each chunk to the printer UART in
full before reading the next, retrying `printer_link_write` in 100 ms waits while the transmit queue is full. Not
reading the socket while blocked MUST leave TCP flow control to slow the sender. No data is spooled.

**Source**: `firmware/main/bridge.c` `serve()`, `write_all()`, `CHUNK`; `firmware/main/printer_link.c` `printer_link_write()`

#### Scenario: Large graphic job
- **GIVEN** the printer sends XOFF while it renders
- **WHEN** a client sends a job larger than the UART transmit queue
- **THEN** the bridge SHALL stop reading the socket until the queue has room
- **AND** every byte the client sent SHALL be queued to the printer in order

### Requirement: Printer replies passed back to the client
While a session is active the bridge SHALL forward every byte the printer sends (for example `~HS` and SGD replies)
to the client, both between reads of the client's data and while waiting for the printer to take data. A failed or
short send to the client MUST be treated as the client being gone.

**Source**: `firmware/main/bridge.c` `pump_from_printer()`, `write_all()`, `serve()`

#### Scenario: Status query
- **WHEN** a client sends `~HS` and keeps its connection open
- **THEN** the bridge SHALL send the printer's three-line reply back on the same connection

### Requirement: Half-closed client served to the end
The bridge SHALL treat a client's FIN (recv returns 0) as the end of the job but not of the connection: it MUST
keep passing printer replies back. A later `ENOTCONN` from a peek, which lwIP returns after the FIN, SHALL count as
the client still being there; only a failed send or another socket error counts as gone.

**Source**: `firmware/main/bridge.c` `client_alive()`, `serve()`

#### Scenario: CUPS half-close
- **GIVEN** a CUPS client has sent its job and shut down its sending side
- **WHEN** the printer then sends a reply
- **THEN** the bridge SHALL forward the reply to the client
- **AND** it SHALL close the connection only after the job has drained and the final reply window has passed

### Requirement: Client idle timeout
The bridge SHALL close a client that has not finished sending once no data has passed in either direction for the
idle timeout, read once at the start of each session from the settings (`idle_s` in NVS, 5 to 3600 s, default
`CONFIG_ZEB_IDLE_TIMEOUT_S` = 30 s). Time spent waiting for the printer to take data MUST NOT count as idle.

**Source**: `firmware/main/bridge.c` `serve()`; `firmware/main/settings.c` `settings_get_options()`; `firmware/main/settings.h` `SETTINGS_IDLE_MIN_S`, `SETTINGS_IDLE_MAX_S`

#### Scenario: Silent client
- **GIVEN** the idle timeout is 30 s
- **WHEN** a client connects, sends part of a job and then sends nothing for more than 30 s while the printer is silent
- **THEN** the bridge SHALL log "client idle for 30 s, closing"
- **AND** what the client had sent SHALL still drain to the printer

#### Scenario: Timeout changed during a session
- **WHEN** the idle timeout is changed while a client is connected
- **THEN** the new value SHALL apply from the next session

### Requirement: Long XOFF waited out, job dropped only when the client is gone
While the printer does not take data the bridge SHALL keep waiting, logging a warning once after 10 s. It MUST
abandon the job only when the client is gone or after 30 minutes (`STALL_LIMIT_MS`) without progress. On abandon it
counts the session in `aborted`, discards the queued output with `printer_link_abort_tx()` and ends the session
without draining.

**Source**: `firmware/main/bridge.c` `write_all()`, `serve()`, `STALL_LIMIT_MS`; `firmware/main/printer_link.c` `printer_link_abort_tx()`

#### Scenario: Paper out
- **GIVEN** a client is sending a job and is still connected
- **WHEN** the printer holds XOFF for 20 minutes until paper is loaded
- **THEN** the bridge SHALL keep the job and continue sending once the printer resumes

#### Scenario: Client gives up during a stall
- **GIVEN** the printer holds XOFF
- **WHEN** the client's connection fails
- **THEN** the bridge SHALL log "client gone or printer stalled too long, discarding the rest of the job"
- **AND** it SHALL increment `aborted` and drop the bytes still queued for the printer

### Requirement: Queued job drained before the link is released
After the receive loop ends (client finished, receive error, failed reply send, or idle timeout) the bridge SHALL
wait until all queued output has left the UART before it releases the link, so the baud is never changed under part
of a job. If draining takes more than 30 minutes it MUST count the session as aborted and discard the rest.

**Source**: `firmware/main/bridge.c` `serve()`, `STALL_LIMIT_MS`; `firmware/main/printer_link.c` `printer_link_tx_idle()`

#### Scenario: Spooler resets after a complete job
- **WHEN** a client sends a whole job and then resets the connection
- **THEN** the bridge SHALL log the recv error and still let the queued job reach the printer

#### Scenario: Replies during the drain
- **WHEN** the printer sends replies while the job drains and the client is still there
- **THEN** the bridge SHALL forward them; if the client is gone it SHALL read and drop them so the receive buffer cannot fill

### Requirement: Final replies after the job
When the client ended its job with a FIN, the bridge SHALL, after the drain, pass back printer
replies for a further 300 ms before closing the connection. A session that ended by a receive error, a failed
reply send or the idle timeout MUST skip this window.

**Source**: `firmware/main/bridge.c` `serve()`

#### Scenario: Reply to the last command
- **WHEN** a job ends with `~HS` and the client half-closes
- **THEN** the bridge SHALL forward the reply that arrives within 300 ms of the drain finishing

### Requirement: Jobs from other sources through bridge_send_job
`bridge_send_job()` SHALL run one job supplied by a read callback on the caller's task. It MUST wait at most 15 s
(`HTTP_ACQUIRE_WAIT_MS`) for the printer link, returning `ESP_ERR_TIMEOUT` with nothing sent if it stays busy. It
discards stale input, counts the session, records the caller's client address and drops (but counts) printer replies.

**Source**: `firmware/main/bridge.c` `bridge_send_job()`, `drop_replies()`, `HTTP_ACQUIRE_WAIT_MS`; `firmware/main/bridge.h` `bridge_read_fn`

#### Scenario: Link busy
- **GIVEN** a port 9100 session holds the link for more than 15 s
- **WHEN** `bridge_send_job()` is called
- **THEN** it SHALL return `ESP_ERR_TIMEOUT` with `*sent` = 0 and `sessions` unchanged

### Requirement: bridge_send_job stall and sender loss
`bridge_send_job()` SHALL return `ESP_ERR_TIMEOUT` when no byte could be queued for longer than the caller's
`stall_ms`, measured from the last successful write and covering the final drain; it then counts `aborted` and
discards queued output. If the read callback returns below 0 it MUST return `ESP_FAIL` after letting what arrived drain.

**Source**: `firmware/main/bridge.c` `bridge_send_job()`

#### Scenario: Printer paused during an HTTP job
- **GIVEN** `stall_ms` is 30000
- **WHEN** the printer holds XOFF for more than 30 s during the job
- **THEN** the call SHALL log "printer paused (XOFF) for 30 s, discarding the rest of the job" and return `ESP_ERR_TIMEOUT`
- **AND** `*sent` SHALL hold the number of bytes queued to the UART before the stall

#### Scenario: Completed job
- **WHEN** the callback returns 0 and the queued output drains within `stall_ms`
- **THEN** the call SHALL return `ESP_OK`, release the link and request a status read

### Requirement: Job statistics
The bridge SHALL keep, under a spinlock, `active`, `client_ip` (current or last client, IPv4 or IPv6 text),
`sessions`, `aborted`, `bytes_to_printer` (bytes queued to the UART) and `bytes_from_printer`, counting port 9100 and
HTTP jobs alike. They MUST start at zero at boot; nothing resets them.

**Source**: `firmware/main/bridge.h` `bridge_stats_t`; `firmware/main/bridge.c` `STATS_UPDATE`, `bridge_get_stats()`; `firmware/main/web.c` `status_get()`

#### Scenario: Status API
- **WHEN** a client reads `/api/status`
- **THEN** the `bridge` object SHALL hold `active`, `client_ip`, `sessions`, `aborted`, `to_printer` and `from_printer`

#### Scenario: Status page
- **WHEN** the Status page is shown
- **THEN** its Jobs row SHALL show `sessions` followed by `aborted` as "dropped"

## Notes

| Constant | Value | Use |
|---|---|---|
| `CONFIG_ZEB_TCP_PORT` | 9100 | listening port |
| `BACKLOG` | 4 | listen backlog |
| `CHUNK` | 1024 bytes | socket read and HTTP job read size |
| `ACQUIRE_WAIT_MS` | 60 000 ms | port 9100 wait for the link |
| `HTTP_ACQUIRE_WAIT_MS` | 15 000 ms | `bridge_send_job` wait for the link |
| `STALL_LIMIT_MS` | 30 min | port 9100 stall and drain limit |
| keepalive | 10 s idle, 5 s interval, 3 probes | dead-client detection |
| final reply window | 300 ms | after the drain |

Printer replies dropped during a port 9100 drain after the client has gone are not added to `bytes_from_printer`;
replies dropped by `bridge_send_job` are.
