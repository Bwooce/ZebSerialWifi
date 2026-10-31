# wifi-provisioning Specification

## Purpose
How the bridge gets onto a Wi-Fi network and stays there: the open setup access point with its captive DNS and
setup page, the station join with saved credentials, the fallback to setup mode, the Wi-Fi Join trial from the
Bridge page, and the hostname and static IPv4 options. Code: `firmware/main/wifi_prov.c`, `dns_hijack.c`, `netcfg.c`
and the Wi-Fi routes in `web.c`.

## Requirements

### Requirement: Setup access point identity
The bridge SHALL run its setup access point as an open network (no password) named `ZebSerial-XXXX`, where `XXXX` is
the last two bytes of the SoftAP MAC address in upper-case hexadecimal. It MUST use channel 1, accept at most 4
stations, and fix both the AP and station interfaces at 20 MHz bandwidth.

**Source**: `firmware/main/wifi_prov.c` `wifi_prov_start()` (`"ZebSerial-%02X%02X"`, `ESP_MAC_WIFI_SOFTAP`), `start_ap()`, `set_mode()`

#### Scenario: Name from the MAC address
- **WHEN** the SoftAP MAC address ends in `a4:1f`
- **THEN** the setup network SHALL be named `ZebSerial-A41F`
- **AND** it SHALL accept stations with no password

### Requirement: Setup mode when no network is saved
The bridge MUST start the setup access point at boot, in AP+station mode, when no Wi-Fi network is saved (no `ssid`
key, or an empty one). With no saved network it SHALL NOT attempt a station connection.

**Source**: `firmware/main/wifi_prov.c` `wifi_prov_start()` (`s_have_creds`), `on_event()` `WIFI_EVENT_STA_START`, `retry_cb()`

#### Scenario: First boot
- **WHEN** the bridge boots with no saved Wi-Fi credentials
- **THEN** it SHALL start the open setup access point straight away
- **AND** the state SHALL be `WIFI_ST_AP`

### Requirement: Fallback to setup mode after 60 seconds without an address
With a saved network, the bridge SHALL start the setup access point alongside the station if it has no IPv4 address
60 s after boot, or 60 s after losing a connection it had. It MUST keep retrying the saved network while the access
point runs, and MUST stop the access point and its DNS server as soon as the station gets an IPv4 address.

**Source**: `firmware/main/wifi_prov.c` `STA_TIMEOUT_MS`, `fallback_cb()`, `start_ap()`, `stop_ap()`, `on_event()` `IP_EVENT_STA_GOT_IP`

#### Scenario: Router down at boot
- **WHEN** the saved network gives no IPv4 address within 60 s of boot
- **THEN** the bridge SHALL start the setup access point and keep trying the saved network

#### Scenario: Router comes back
- **WHEN** the station gets an IPv4 address while the setup access point runs
- **THEN** the bridge SHALL stop the access point and the captive DNS server and run as a station only

### Requirement: Station retry interval
After each station disconnect, when a network is saved, the bridge SHALL try to reconnect after 2 s, or after 30 s
while the setup access point is running (station scans disturb the AP channel).

**Source**: `firmware/main/wifi_prov.c` `STA_RETRY_MS`, `STA_RETRY_IN_AP_MS`, `on_event()` `WIFI_EVENT_STA_DISCONNECTED`, `retry_cb()`

#### Scenario: Retry while in setup mode
- **WHEN** the station is disconnected and the setup access point is running
- **THEN** the next connection attempt SHALL start 30 s later

### Requirement: Strongest access point and a -85 dBm floor
The station SHALL scan all channels and join the access point with the strongest signal for the SSID. It MUST skip
access points weaker than -85 dBm, unless a connect attempt fails with reason "no AP found in RSSI threshold"; then it
SHALL accept any signal strength until it next gets an IPv4 address.

**Source**: `firmware/main/wifi_prov.c` `set_sta_config()` (`WIFI_ALL_CHANNEL_SCAN`, `WIFI_CONNECT_AP_BY_SIGNAL`), `MIN_RSSI`, `s_weak_ok`, `WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD`

#### Scenario: Mesh with a far node
- **WHEN** the SSID is heard at -30 dBm and at -92 dBm
- **THEN** the station SHALL join the access point at -30 dBm

#### Scenario: Only weak access points
- **WHEN** a connect attempt ends with reason `WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD`
- **THEN** the next attempt SHALL use an RSSI threshold of -127 dBm
- **AND** the -85 dBm floor SHALL return once an IPv4 address is obtained

### Requirement: Station security settings
The station MUST require at least WPA2-PSK when the saved password is non-empty, and accept an open network when it is
empty. It SHALL advertise protected management frames as capable.

**Source**: `firmware/main/wifi_prov.c` `set_sta_config()` (`threshold.authmode`, `pmf_cfg.capable`)

#### Scenario: Open network saved
- **WHEN** the saved password is empty
- **THEN** the station auth threshold SHALL be `WIFI_AUTH_OPEN`

### Requirement: Leave an access point that gives no IPv4 address
The bridge SHALL disconnect from an access point when it has been associated for 20 s without getting an IPv4 address,
so the retry scans again.

**Source**: `firmware/main/wifi_prov.c` `NO_IP_MS`, `no_ip_cb()`, `on_event()` `WIFI_EVENT_STA_CONNECTED`

#### Scenario: Associated but no DHCP lease
- **WHEN** the station associates and no IPv4 address arrives within 20 s
- **THEN** the bridge SHALL disconnect and log the BSSID, channel and RSSI

### Requirement: Wi-Fi power save off
When it boots with a saved network, the bridge SHALL turn Wi-Fi power save off (`WIFI_PS_NONE`) after starting
Wi-Fi, so replies are not delayed by modem sleep.

**Source**: `firmware/main/wifi_prov.c` `wifi_prov_start()` `esp_wifi_set_ps(WIFI_PS_NONE)`

#### Scenario: Normal boot
- **WHEN** the bridge boots with a saved network
- **THEN** Wi-Fi power save SHALL be `WIFI_PS_NONE`

### Requirement: Captive DNS server
While the setup access point runs, the bridge SHALL answer DNS queries on UDP port 53 (bound to all interfaces). An A
query MUST get one answer, 192.168.4.1 with a TTL of 60 s; any other query type SHALL get a reply with no answers.
Responses, packets with other than one question, and malformed or compressed names MUST be ignored. If the socket
cannot be created or bound, it SHALL log "bind failed" and try again every 1 s while the server is meant to run.

**Source**: `firmware/main/dns_hijack.c` `serve()`, `qname_len()`, `DNS_PORT`, `dns_hijack_start()`, `dns_hijack_stop()`

#### Scenario: Phone probes a portal host
- **WHEN** a station on the setup network asks for the A record of any name
- **THEN** the reply SHALL give 192.168.4.1 with TTL 60

#### Scenario: AAAA query
- **WHEN** the query type is not A
- **THEN** the reply SHALL have no error and zero answers

#### Scenario: Port 53 not yet free
- **WHEN** `bind()` on UDP port 53 fails
- **THEN** the server SHALL wait 1 s and try again

#### Scenario: Stopping
- **WHEN** the setup access point stops
- **THEN** the DNS server SHALL close its socket within its 1 s receive timeout

### Requirement: Setup network sockets identified fail-closed
The bridge MUST treat a connection as coming over the setup network when the access point is running and the
socket's local IPv4 address (or IPv4-mapped IPv6 address) is the access point's. If the local or AP address cannot be
read while the access point runs, it SHALL treat the connection as on the setup network.

**Source**: `firmware/main/wifi_prov.c` `wifi_prov_sock_on_ap()`; `firmware/main/web.c` `on_ap()`

#### Scenario: Native IPv6 client
- **WHEN** the access point is up and a client connects over a native IPv6 address
- **THEN** the connection SHALL NOT count as on the setup network

### Requirement: Setup page content
The setup page SHALL offer a list of scanned network names (hidden and duplicate names left out, strongest first, each
with its RSSI in dBm), a typed network name of up to 32 characters, a password of up to 64 characters, and a "Scan
again" link to `/?rescan=1`. It MUST reuse a scan under 60 s old unless the query asks to rescan.

**Source**: `firmware/main/web.c` `send_setup_page()`, `send_ssid_options()`, `wifi_scan_cached()`, `SCAN_MAX_AGE_US`, `MAX_SCAN`

#### Scenario: Several captive probes
- **WHEN** a phone loads the setup page several times within 60 s
- **THEN** the bridge SHALL scan only for the first load

### Requirement: Saving a network from the setup page
`POST /save` SHALL take the typed name over the listed one, refuse an empty name or a password of 1 to 7 characters with
`400 Bad Request`, answer `500 Internal Server Error` if saving fails, and otherwise save the SSID and password and
restart the bridge 1.5 s later. It MUST pass the cross-site check, and is accepted on any network: there is no
setup-network gate, so a client on the station network can also save a network.

**Source**: `firmware/main/web.c` `save_post()`, `restart_soon()`, `same_origin()`; `firmware/main/settings.c` `settings_set_wifi()`

#### Scenario: Short password
- **WHEN** the form has password `abc`
- **THEN** the bridge SHALL answer `400 Bad Request` with "WPA passwords are at least 8 characters."

#### Scenario: Saved from the station network
- **WHEN** a client on the station network posts `/save` with no Origin or Referer and an allowed Host
- **THEN** the bridge SHALL save the network and restart

#### Scenario: Network saved
- **WHEN** the form names a network and a valid password
- **THEN** the bridge SHALL save them and restart about 1.5 s after replying

### Requirement: Wi-Fi Join request
`POST /wifi_join` SHALL start a join trial when the station is connected and no trial is running, then answer `303 See
Other` to `/`. It MUST answer `400 Bad Request` for an empty network name, a 1 to 7 character password or a name or
password `wifi_prov_join()` rejects as too long, and `409 Conflict` when a trial is running or the station is not
connected. From the setup network it SHALL redirect to the setup page.

**Source**: `firmware/main/web.c` `wifi_join_post()`, `redirect_to()`; `firmware/main/wifi_prov.c` `wifi_prov_join()`

#### Scenario: Join while one is running
- **WHEN** a join is requested while another join is in the trying state
- **THEN** the bridge SHALL answer `409 Conflict` with "A join is already running, or Wi-Fi is not connected."

### Requirement: Join trial with fallback to the saved network
About 1 s after a join is accepted, the station SHALL leave its network for the new one. If it gets an IPv4 address
within 30 s, the bridge MUST save the new SSID and password and report joined. Otherwise it SHALL report failed,
reconnect to the saved network and restart the 60 s setup fallback window.

**Source**: `firmware/main/wifi_prov.c` `JOIN_SWITCH_MS`, `JOIN_TRIAL_MS`, `join_switch_cb()`, `join_trial_cb()`, `join_got_ip()`

#### Scenario: Wrong password
- **WHEN** the new network gives no IPv4 address within 30 s
- **THEN** the join state SHALL be failed and the saved credentials SHALL be unchanged
- **AND** the station SHALL reconnect to the saved network

#### Scenario: Join succeeds
- **WHEN** the new network gives an IPv4 address within 30 s
- **THEN** its SSID and password SHALL be saved and used from the next boot

### Requirement: Join outcome and reason text
The bridge SHALL report the latest join (none, trying, joined or failed, held in RAM since boot) with its SSID and the
last disconnect reason other than 8 (leaving on purpose) as text: "network not found"; "wrong password?" for auth expiry,
auth failure or a handshake timeout; otherwise "Wi-Fi reason N". A failure with no reason MUST read "no address from the
network".

**Source**: `firmware/main/wifi_prov.c` `wifi_prov_join_state()`, `on_event()` (`WIFI_REASON_ASSOC_LEAVE`); `firmware/main/web.c` `status_get()` (`"join"`), `send_wifi_section()`

#### Scenario: Auth failure
- **WHEN** a trial ends after a disconnect with reason `WIFI_REASON_AUTH_FAIL`
- **THEN** `/api/status` `wifi.join` SHALL hold `state` `failed` and `why` `wrong password?`

### Requirement: Forget Wi-Fi
`POST /forget` from the station network SHALL clear the saved SSID and password and restart the bridge 1.5 s later, so
it comes up in setup mode. It MUST pass the cross-site check, and SHALL NOT clear the hostname or static address.

**Source**: `firmware/main/web.c` `forget_post()`, `restart_soon()`; `firmware/main/settings.c` `settings_clear_wifi()`

#### Scenario: Forget from the Bridge page
- **WHEN** a client on the station network posts `/forget`
- **THEN** the bridge SHALL erase the `ssid` and `pass` keys and restart into the setup access point

### Requirement: Hostname
The bridge SHALL use the saved hostname, default `zebserial`, as its DHCP hostname. A hostname MUST be 1 to 32
characters of `a-z`, `0-9` and `-`, not starting or ending with `-`; upper case SHALL be rejected.

**Source**: `firmware/main/wifi_prov.c` `wifi_prov_start()` `esp_netif_set_hostname()`; `firmware/main/netcfg.c` `netcfg_hostname_valid()`; `firmware/main/settings.h` `SETTINGS_DEFAULT_HOSTNAME`

#### Scenario: Upper case
- **WHEN** the hostname `Zebra` is submitted
- **THEN** it SHALL be rejected as invalid

### Requirement: Static IPv4 address
When static addressing is saved, the bridge SHALL stop the DHCP client on each association and set the saved address,
netmask and gateway. It MUST set the saved DNS server, or the gateway when DNS is 0, and set none when both are 0.

**Source**: `firmware/main/wifi_prov.c` `apply_static_ip()`, `on_event()` `WIFI_EVENT_STA_CONNECTED`

#### Scenario: No DNS given
- **WHEN** a static address is saved with gateway 192.168.1.1 and DNS 0
- **THEN** the main DNS server SHALL be 192.168.1.1

### Requirement: Static IPv4 validation
A static address MUST be accepted only if: its first octet is not 0, 127 or 224 and above; it is not in
192.168.4.0/24; the netmask is contiguous and neither /0 nor /32; the address is not the network or broadcast address;
and a non-zero gateway is in the same subnet and differs from the address. Dotted quads SHALL be strict (no leading
zeros, 4 parts).

**Source**: `firmware/main/netcfg.c` `netcfg_static_valid()`, `netcfg_ipv4_parse()`

#### Scenario: Clash with the setup subnet
- **WHEN** the static address 192.168.4.20 is submitted
- **THEN** it SHALL be rejected

#### Scenario: Leading zero
- **WHEN** the address `192.168.01.5` is submitted
- **THEN** it SHALL fail to parse

### Requirement: IPv6 link-local address
On each association the bridge SHALL create an IPv6 link-local address on the station interface; SLAAC addresses come
from `CONFIG_LWIP_IPV6_AUTOCONFIG`. Up to 3 station IPv6 addresses SHALL be reported while connected.

**Source**: `firmware/main/wifi_prov.c` `esp_netif_create_ip6_linklocal()`, `wifi_prov_ip6()`, `WIFI_PROV_IP6_MAX`; `firmware/sdkconfig.defaults` `CONFIG_LWIP_IPV6_AUTOCONFIG=y`

#### Scenario: Connected to a router with IPv6
- **WHEN** the station is connected
- **THEN** `/api/status` `wifi.ip6` SHALL list at most 3 addresses

## Notes

Routing of requests that arrive over the setup network is specified in web-ui (Setup access point routing); the
refusal of port 9100 connections from it in print-bridge; the 403 for `/pstprnt` in http-print.
