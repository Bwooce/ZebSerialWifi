## Why

The Fable review of the specs (31 Oct 2026) listed four places where the code does not meet its spec, recorded as
Known deviations: no CORS preflight answer on `/pstprnt`, two lengths converted with the rounded dots/mm, a 10 mm
test label that fails at 300 dpi, and a cable page Address field that cannot be cleared. Each is a small fix, and
leaving them listed invites a contributor to treat the bug as the contract.

## What Changes

- `/pstprnt` answers an OPTIONS preflight with 204 and the CORS headers a browser needs, including Chrome's
  `Access-Control-Allow-Private-Network`, so a page that sends JSON or a custom header can print too.
- The Status page converts form-changed lengths with the true pitch, like the Printer form. MAXIMUM LENGTH keeps its
  decode at the printer's rounded figure (that is how the printer converts ^ML), and every page shows it in true mm;
  the spec now says why the printer's own label reads 1.6 % shorter at 300 dpi.
- The test label accepts any size of at least 10 mm at the true pitch (118 dots at 300 dpi).
- The cable page keeps an empty Address field empty while it is edited and falls back to `zebserial.local` only
  when the address is used, or when the field is left empty.
- Small README and comment corrections the review found, and `/save`'s success page links the saved hostname.

## Impact

- firmware/main/web.c, test_label.c, dns_hijack.c, ota.h/ota.c; tools/cable-labels.html; README.md; host tests.
- Specs: http-print, web-ui, cable-label-page (requirements below; their Known deviations lists are emptied on
  archive).
