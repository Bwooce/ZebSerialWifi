# A plausible library theory cost a power cycle: check the log for which access point first

- Date: 2026-10-18
- Agent: main
- Seen before: no

What happened: the bridge stuck after a wrong-password join; I blamed a cached WPA3 key in the supplicant and shipped
a radio stop/start to a test build. It stuck again. The answer was already in the log: the ba-add lines after the
return carried a different BSSID (the far access point, at -92 dBm).
Workaround: none needed after the fact; fixed in 0b31d82.
Suggested fix: when Wi-Fi misbehaves, first establish which BSSID it is on and whether it has IPv4 (the join log
lines now say so), before theorising about the closed Wi-Fi library.
