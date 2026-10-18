# Fetching an Espressif datasheet returns an unreadable PDF

- Date: 2026-10-18
- Agent: main
- Seen before: no

What happened: WebFetch of the ESP32-C6 datasheet (espressif.com redirects to documentation.espressif.com) gave only
compressed PDF bytes, so the summary had no values.
Workaround: ran pdftotext -layout on the copy WebFetch saved under the session's tool-results directory, then grep.
Suggested fix: go straight to that saved file and pdftotext for any Espressif PDF.
