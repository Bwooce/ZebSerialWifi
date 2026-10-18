# The chip temperature reading looks frozen: the sensor reads in 1 °C steps

- Date: 2026-10-18
- Agent: main
- Seen before: no

What happened: /api/status showed exactly 56.3 °C for 5 minutes, which looked like temp_mon was not re-reading.
Workaround: a throwaway build logged every read while web traffic loaded the bridge; it rose 56.3 -> 61.3 and back in whole degrees.
Suggested fix: none needed; when checking it, load the bridge rather than wait for the number to move.
