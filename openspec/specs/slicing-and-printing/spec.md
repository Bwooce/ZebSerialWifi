# slicing-and-printing Specification

## Purpose
How the case STLs from `enclosure/out/` become the two Bambu Lab H2D print projects in `print/`
(`zebserial_case.3mf`, two-colour, and `zebserial_coupon.3mf`, the single-colour fit test), with the Bambu Studio
CLI driven by `print/run_slice.sh` and `print/resolve_profile.py`. Bambu Studio is installed on only one of the
user's Macs, so slicing is a step done there, not in every checkout.

## Requirements

### Requirement: Printer, material and base profiles
The print projects SHALL be sliced for a Bambu Lab H2D with a 0.4 mm nozzle, Bambu PETG Basic and a textured PEI
plate, from the system profiles "Bambu Lab H2D 0.4 nozzle" (machine), "0.16mm Balanced Quality @BBL H2D" (process)
and "Bambu PETG Basic @BBL H2D 0.4 nozzle" (filament).

**Source**: `print/run_slice.sh` (its `print/resolve_profile.py` calls); `print/process_overrides.json` `curr_bed_type`

#### Scenario: Resolving the profiles
- **WHEN** `run_slice.sh` runs
- **THEN** it SHALL write `machine.json`, `process.json` and `filament.json` in the working folder from the profiles under `/Applications/BambuStudio.app/Contents/Resources/profiles/BBL`

### Requirement: Process overrides
The process SHALL apply `print/process_overrides.json`: 4 wall loops, 6 top and 5 bottom shell layers, 25 %
gyroid infill, supports off, seam at the back, no brim, first layer 40 mm/s (infill 60 mm/s), textured PEI plate.

**Source**: `print/process_overrides.json`; `print/README.md` "Settings and why"

#### Scenario: Four perimeters
- **WHEN** the case is sliced
- **THEN** walls SHALL be four loops (1.6 mm with the 0.4 mm nozzle), matching the SCAD's `wall` = 1.6 and `min_wall` = 1.2 design

### Requirement: Fully resolved profiles for the CLI
`resolve_profile.py` SHALL flatten a system profile's `inherits` chain, apply an overrides JSON (a one-element list
override broadcast to every extruder variant), mark the result as a user preset (`from` "User", `inherits` the
system profile) so that the CLI's compatibility check accepts it, and set the ID the CLI matches presets by
(`printer_settings_id`, `print_settings_id` or `filament_settings_id`) to the preset's name.

**Source**: `print/resolve_profile.py` `resolve()`, `main()`; `print/README.md` "Rebuilding"

#### Scenario: A one-value override on the two-extruder H2D
- **WHEN** an override gives `["40"]` for a setting whose base value has one entry per extruder variant
- **THEN** the value SHALL be repeated for every variant

### Requirement: Two-colour case project
`run_slice.sh <workdir> case` SHALL slice `tray.stl`, `lid.stl` and `logo.stl` onto one plate as
`zebserial_case.3mf`, loading them with an assemble list in which the logo shares the tray's `assemble_index`, so the
logo is a second part of the tray object on filament 2 (`filament_overrides_logo.json`, white, its own preset name).

**Source**: `print/run_slice.sh` case branch, `assemble.json`, `--load-assemble-list`; `print/filament_overrides_logo.json`

#### Scenario: Logo alignment
- **WHEN** the case project is opened
- **THEN** the logo SHALL sit flush in the tray floor's first four layers (0.68 mm), because `logo.stl` is exported in the tray's coordinates and loaded at the same position

#### Scenario: Two identical filament presets
- **WHEN** the second filament has the same preset name as the first
- **THEN** the CLI crashes, so the logo filament SHALL keep its own name ("ZebSerial Bambu PETG Basic logo @BBL H2D 0.4 nozzle")

### Requirement: Coupon project
`run_slice.sh <workdir> coupon` SHALL slice `coupon_tray.stl` and `coupon_lid.stl`, single colour, as
`zebserial_coupon.3mf`, with `--orient 0 --arrange 1`.

**Source**: `print/run_slice.sh` coupon branch

#### Scenario: Slicing the coupons
- **WHEN** the coupon mode runs
- **THEN** both coupon parts SHALL be on one plate in their exported print orientation

### Requirement: Slicing is checked, not assumed
After slicing, `run_slice.sh` SHALL print `return_code` from `result.json`, list the 3MF, and print the G-code's total
estimated time and filament weight. A slice SHALL count as good only with `return_code 0` and a fresh 3MF.

**Source**: `print/run_slice.sh` final three commands (it deletes the old 3MF and `result.json` before slicing)

#### Scenario: Exit status 141
- **WHEN** the script ends with exit status 141
- **THEN** that SHALL be read as SIGPIPE from `grep -m2` closing the pipe, and the result SHALL be judged by `return_code 0` in `result.json`

### Requirement: Slicing runs where Bambu Studio is installed
Slicing SHALL run on a Mac with Bambu Studio at `/Applications/BambuStudio.app`. The working folder SHALL hold the
`print/` files (`run_slice.sh`, `resolve_profile.py`, the three overrides JSONs) and the five STLs from
`enclosure/out/`; the 3MFs SHALL be copied back into `print/` and the working folder removed.

**Source**: `print/run_slice.sh` `APP`, usage comment; `print/README.md` "Rebuilding"

#### Scenario: A checkout without Bambu Studio
- **WHEN** the STLs change on a machine without Bambu Studio
- **THEN** the files SHALL be copied to the Mac that has it, sliced there in a temporary folder, the 3MFs copied back, and the temporary folder deleted

### Requirement: 3MFs follow the STLs
The 3MFs SHALL be re-sliced and committed whenever `enclosure/out/*.stl` change, and `print/README.md` SHALL name
the enclosure commit they were sliced from and the slicer's time and filament estimates.

**Source**: `print/README.md` first section ("enclosure commit a69fcd6, revision 5"); git history: `enclosure/out` last changed in a69fcd6, the 3MFs in d89e83f

#### Scenario: STLs regenerated for a new module
- **WHEN** `build.sh` produces different STLs
- **THEN** the 3MFs in `print/` SHALL be treated as stale until re-sliced, and the README SHALL say so in the meantime

### Requirement: Print orientation comes from the model
The slicer SHALL not re-orient the case: the tray prints floor down with the logo on the bed and the lid prints top
down, as exported, and both print without supports.

**Source**: `enclosure/zebserial_case.scad` `part == "tray"`, `part == "lid"`; `print/README.md` "Orientation is part of the model"; `print/process_overrides.json` `enable_support`

#### Scenario: Opening the case project
- **WHEN** `zebserial_case.3mf` is opened in Bambu Studio
- **THEN** the tray SHALL lie floor down and the lid top down, with supports off

### Requirement: Coupons print before the case
For any change to a fit-setting parameter (module, DB9, XIAO position, clearances), the coupon project SHALL be
printed and the board, DB9 and XIAO test-fitted before the full case.

**Source**: `enclosure/README.md` "Concerns to check before printing" ("Print the coupons first"); `print/README.md` coupon entry

#### Scenario: Coupon fits
- **WHEN** the coupon tray takes the board on its pins with the flange between the lip and the backstop, and the coupon lid closes and snaps
- **THEN** the full case SHALL be printed from `zebserial_case.3mf`
