# Print project (Bambu Lab H2D)

`zebserial_case.3mf` is a Bambu Studio project with the tray and the lid on one plate, already sliced: about 62 min and 15.5 g of PETG, sliced from the enclosure at commit 7a8dd04. Open it in Bambu
Studio and print, or re-slice after changing anything.

## Settings and why

| Setting | Value | Why |
|---|---|---|
| Printer | Bambu Lab H2D, 0.4 mm nozzle | |
| Filament | Bambu PETG Basic (250 °C nozzle, 70 °C bed) | Snap clips flex without cracking, and it has heat margin on the back of a printer |
| Plate | Textured PEI | Stops PETG bonding too hard to smooth PEI |
| Process | 0.16 mm Balanced Quality, with the overrides below | Finer layers for the chamfers and the logo deboss |
| Walls | 4 loops (1.6 mm) | The case walls are designed as four perimeters |
| Top / bottom shells | 6 / 5 layers | About 1 mm solid skin, including under the logo |
| Infill | 25 % gyroid | The parts are mostly walls; gyroid is strong in every direction |
| Supports | Off | Both parts are designed to print without supports |
| Seam | Back | Keeps the seam off the front and the logo |
| Brim | None | Small parts with a flat base |
| First layer | 40 mm/s (infill 60 mm/s) | Clean logo on the lid's first layers |

Orientation is part of the model: the tray prints floor-down, and the lid prints top-down so the logo is on the bed.

## Rebuilding

The Bambu Studio CLI needs fully resolved profiles. `resolve_profile.py` flattens a system profile's `inherits`
chain, applies the overrides JSON, and marks the result as a user preset inheriting the system one. That last step is
what satisfies the CLI's printer/process compatibility check. To rebuild on a Mac with Bambu Studio installed, copy
this folder plus `enclosure/out/tray.stl` and `enclosure/out/lid.stl` into one working folder, then run
`run_slice.sh <that folder>`.
