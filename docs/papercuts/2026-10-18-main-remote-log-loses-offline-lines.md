# The remote log loses the lines that matter in a Wi-Fi fault, and stamps lines when sent

- Date: 2026-10-18
- Agent: main
- Seen before: no

What happened: debugging a stuck Wi-Fi rejoin, the lines logged while the bridge was offline were mostly dropped
(149 on one build), and the bracketed device time is the send time, not the log time: a buffered burst arrives
10 ms apart and the first boot line reads 10 s. Early timing conclusions drawn from it were wrong.
Workaround: put esp_timer uptime ("t=%lld s") in the message text, and for the test build kept a Wi-Fi event ring in
RTC_NOINIT RAM, read over HTTP (removed before commit 0b31d82).
Suggested fix: hold the queue while there is no IPv4 instead of dropping after SEND_RETRIES, and stamp lines in
log_hook at log time.
