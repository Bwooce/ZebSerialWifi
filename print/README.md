# Print project (Bambu Lab H2D)

- `zebserial_case.3mf`: the tray and the lid on one plate, already sliced (enclosure commit 0018b17). It takes
  about 1 h 25 min (1 h 24 min 47 s estimated) and uses 20.5 g of PETG for the case plus 0.4 g (0.37 g) for the logo. The logo is a second part of the lid
  object on filament 2, so it prints as a flush two-colour inlay in the lid's first four layers (0.68 mm: the 0.2 mm first layer and
  three 0.16 mm layers). Filament 1 is the case
  colour and filament 2 the logo colour (white by default); change the colours in Bambu Studio to suit the spools
  loaded.
- `zebserial_coupon.3mf`: a single-colour fit test of the front 14.5 mm of both parts: the flange recess, lips,
  bearing wall, spike pockets and front snaps. It takes about 44 min (44 min 29 s estimated) and uses 8.5 g, because the lid coupon is full
  height.

Open a file in Bambu Studio and print, or re-slice after changing anything.

## Settings and why

| Setting | Value | Why |
|---|---|---|
| Printer | Bambu Lab H2D, 0.4 mm nozzle | |
| Filament | Bambu PETG Basic (250 °C nozzle, 70 °C bed) | Snap clips flex without cracking, and it has heat margin on the back of a printer |
| Plate | Textured PEI | Stops PETG bonding too hard to smooth PEI |
| Process | 0.16 mm Balanced Quality, with the overrides below | Finer layers for the chamfers and the logo inlay |
| Walls | 4 loops (1.6 mm) | The case walls are designed as four perimeters |
| Top / bottom shells | 6 / 5 layers | About 1 mm solid skin; the 0.92 mm under the logo is solid as well |
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
this folder plus `enclosure/out/tray.stl`, `lid.stl`, `logo.stl`, `coupon_tray.stl` and `coupon_lid.stl` into one
working folder, then run `run_slice.sh <that folder> case` or `run_slice.sh <that folder> coupon`. The case uses an
assemble list (`--load-assemble-list`) to make the logo a part of the lid object. The second filament needs its own
preset name and colour (`filament_overrides_logo.json`); with two identical filament presets the CLI crashes.
