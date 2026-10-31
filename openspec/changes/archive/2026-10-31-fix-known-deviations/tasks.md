## 1. Firmware

- [x] 1.1 Host test: a 10 x 10 mm test label builds at 12 dots/mm (118 dots); 9.9 mm does not.
- [x] 1.2 `test_label_zpl()` minimum is `zpl_mm_to_dots(10, m)`.
- [x] 1.3 Status page `val()` converts with `tdm(D)`.
- [x] 1.4 `OPTIONS /pstprnt` handler: 204, `Access-Control-Allow-Origin: *`, `-Methods: POST, OPTIONS`,
      `-Headers: Content-Type`, `-Private-Network: true`, `-Max-Age: 600`.
- [x] 1.5 `/save` success page links the saved hostname; `OTA_CONFIRM_DEADLINE_S` in ota.h, used by web.c;
      comments in web.c (setup AP routes) and dns_hijack.c (RD) match the code.
- [x] 1.6 Build with -Werror, host tests, page script check; OTA to the bridge; preflight and 10 mm label on the bench.

## 2. Cable page

- [x] 2.1 Address field: empty while edited, `zebserial.local` when used or left empty.

## 3. Docs and specs

- [x] 3.1 README corrections from the review.
- [x] 3.2 Archive this change; empty the Known deviations lists it fixes.
