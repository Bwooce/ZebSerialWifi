# A test build stays in the fallback OTA slot after the real build goes on

- Date: 2026-10-18
- Agent: main
- Seen before: no

What happened: after a -dirty test build (warning at 30 °C) and then the real build, /api/status ota.other_version was the
test build, so a rollback would have booted it.
Workaround: sent the real build a second time so both slots held 7cd1707.
Suggested fix: after any test build, check ota.other_version and OTA the real build twice if it shows the test build.
