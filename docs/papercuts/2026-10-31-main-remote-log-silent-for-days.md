# The remote log went silent for days while the bridge ran, and nothing said so

- Date: 2026-10-31
- Agent: main
- Seen before: related to 2026-10-18-main-remote-log-loses-offline-lines.md

What happened: looking for when the user ran a calibration that fed metres of labels, the iot_log receiver had no
bridge lines for 92 h (20 Oct 05:13 to 24 Oct 01:37), 12 h (24-25 Oct) and 5 h (28 Oct 11:04-16:04). The bridge did
not restart (uptime ran straight through) and the receiver kept logging another device throughout. I first
read "no ~JC in the log" as "the calibration was started from the front panel", which the gaps do not support.
Likely cause, not proven: remote_log.c only sends while iot_log_listener_active(), found by mDNS discovery, and drops
lines without logging when it thinks there is no receiver; Bonjour across the user's access points is unreliable.
The drop counter is per boot, so the 31 Oct OTA erased the evidence.
Workaround: list gaps over an hour in the bridge's lines before treating a missing line as "it did not happen".
Suggested fix: log a line when the listener is lost and found (with the count dropped meanwhile), keep the dropped
count across reboots or show it with the time it last changed, and have the receiver warn per device when a device
that was sending goes quiet for more than a few minutes.

Update, same day (OpenSpec review): with no receiver found, `iot_log()` returns without counting the line, so lines
lost that way are not in the dropped count at all (firmware/main/remote_log.h). Lines are now held while the bridge
is offline (a 12288-byte queue), so the 2026-10-18 entry's offline loss is partly fixed; the send-time stamps remain.
