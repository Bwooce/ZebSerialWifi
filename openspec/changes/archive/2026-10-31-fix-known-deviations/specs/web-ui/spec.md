## MODIFIED Requirements

### Requirement: True dots per millimetre
The bridge SHALL convert mm and dots with the true head pitch: a reported 12 dots/mm is 300/25.4 and 24 dots/mm is
600/25.4 dots per mm; any other positive figure is used as reported. mm to dots MUST round to the nearest dot either
side of zero. The dpi for 8, 12 and 24 dots/mm is 203, 300 and 600. The Status page and the Printer form SHALL show
the same mm for the same dot count. The printer converts its own mm figures (`^HH` MAXIMUM LENGTH, `^MUm`) with
the rounded dots/mm, so the bridge decodes MAXIMUM LENGTH to dots that way and then shows true mm: at 300 dpi
`^ML600` is labelled 50MM by the printer and 50.8 mm by the bridge, which is the physical length.

**Source**: `firmware/main/zpl_units.h` `zpl_true_dpmm()`, `zpl_mm_to_dots()`, `zpl_dots_to_mm()`, `zpl_dpi()`;
`firmware/main/web.c` STATUS_SCRIPT `tdm()`, `val()`; `firmware/main/zpl_status.c` `decode_field()` MAXIMUM LENGTH

#### Scenario: 4 inch label at 300 dpi
- **WHEN** 101.6 mm is converted at a reported 12 dots/mm
- **THEN** the result SHALL be 1200 dots

#### Scenario: Same length on both pages
- **WHEN** the label length is 237 dots at a reported 12 dots/mm
- **THEN** the Status page and the Printer form SHALL both show 20.1 mm

#### Scenario: Maximum length in true mm
- **WHEN** the printer reports MAXIMUM LENGTH "2.0IN   50MM" at 12 dots/mm
- **THEN** the bridge SHALL hold 600 dots and show 50.8 mm
