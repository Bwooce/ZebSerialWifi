# enclosure Specification

## Purpose
The two-part PETG case (tray and lid, snap clips, no screws) that holds the HW-044 MAX3232 RS-232 module and the
Seeed XIAO ESP32-C6 and plugs into the printer's DB9 through a male-to-male gender changer. It is one parametric OpenSCAD model,
`enclosure/zebserial_case.scad` (revision 5), with mechanical gates in `enclosure/build.sh` and
`enclosure/controls.sh`. The likely next change is a different RS-232 module, so this spec says which parameters
describe which part, what was measured and what was borrowed, and how to regenerate and check the STLs. The Notes
section at the end holds the parameter tables and the regeneration procedure.

## Requirements

### Requirement: One parametric model, two printed parts
The case SHALL be generated from `enclosure/zebserial_case.scad` alone, as a tray and a lid that join by four
cantilever snap clips with no screws. Every dimension that depends on the module, the XIAO or the print process SHALL
be a top-level parameter or derived from one, except the fixed values listed under Notes; the STLs SHALL never be
edited by hand.

**Source**: `enclosure/zebserial_case.scad` header comment, `module tray()`, `module lid()`, `part` selector at the end of the file

#### Scenario: Exporting the printed parts
- **WHEN** `openscad -D 'part="tray"' -D show_ghosts=false -o out/tray.stl zebserial_case.scad` is run (likewise `part="lid"`)
- **THEN** the STL SHALL be in print orientation: the tray floor on the bed (`translate([0, 0, -z_bot])`), the lid top on the bed (`rotate([180, 0, 0])`)

### Requirement: Coordinate frame and datums
All parameters SHALL be expressed in one assembly frame: Y 0 at the DB9 flange front face, which is also the case
front face (flush), +Y towards the header end; Z 0 at the RS-232 PCB top (component) surface, +Z towards the lid;
+X to the right seen from +Z with the DB9 nearest. The XIAO's own data SHALL use d (from its USB end, along -X)
and w (across its width, +w towards the 5V/GND column, along -Y).

**Source**: `enclosure/zebserial_case.scad` lines 5-11 and 86-88, `y_cf`, `y_fb`, `function dx()`, `function wy()`

#### Scenario: A module with a different flange position
- **WHEN** a new module's PCB front edge is not flush with its flange front face
- **THEN** the offset SHALL be entered as `pcb_front_y` (flange front face to PCB front edge) and Y 0 SHALL stay at the flange front face

#### Scenario: Negative pcb_front_y
- **WHEN** `pcb_front_y` is negative
- **THEN** the model SHALL put the PCB front edge at `fl_t`, just behind the steel flange (`y_pcb0`), whatever the value: a PCB edge in front of the flange face cannot be entered

### Requirement: Height envelope
The outside top of the case SHALL be at most `max_above` (25.0 mm) above the RS-232 PCB top. The model SHALL
fail with an assertion when the derived top `z_top` exceeds it.

**Source**: `enclosure/zebserial_case.scad` `max_above`, `z_top`, `assert(z_top <= max_above, ...)`

#### Scenario: A taller Dupont housing
- **WHEN** `dupont_top` or `dupont_ceiling_min` grows so that `z_top` exceeds 25.0
- **THEN** OpenSCAD SHALL exit 1 with "case taller than the 25 mm envelope above the board"
- **AND** `build.sh` (`set -eu`) SHALL stop at the first STL export with a non-zero exit and no FAIL line, having already deleted `out/*.stl` and `renders/*.png`

### Requirement: DB9 mating face
The DB9 D-shell SHALL project `ds_proj` (6.0 mm) forward of the flush case front face so that it mates with the
printer through the gender changer. Nothing but the two retention lips SHALL stand in front of Y 0, and the lips
SHALL span only |X| <= `lip_x` (8.0) so the printer's jackposts at X +-`fl_hole_pitch`/2 (12.5) stay clear.

**Source**: `enclosure/zebserial_case.scad` `ds_proj`, `lip_x`, `fl_hole_pitch`, `module top_lip()`, `module bottom_lip()`, echo "ZEBCASE LIPS"

#### Scenario: Lips widened into the jackpost area
- **WHEN** `lip_x` exceeds `fl_hole_pitch / 2 - 3.5` (9.0)
- **THEN** the model SHALL echo "ZEBCASE WARNING: the lips reach the jackpost area at the flange holes"
- **AND** `build.sh` SHALL fail with "FAIL: ZEBCASE WARNING: the lips reach the jackpost area at the flange holes" (see "Geometry warnings fail the build")

### Requirement: Flange fore-aft location
The flange SHALL be held fore-aft between the lips, `lip_gap` (0.1 mm) in front of the flange front face, and the
lid front wall, which bears on the connector plate back at Y `fl_plate_d` (5.1) with zero gap. The lips SHALL
stay at least `lip_ds_clr` (1.0) clear of the D-shell outline.

**Source**: `enclosure/zebserial_case.scad` `lip_gap`, `lip_reach`, `lip_ds_clr`, `fl_plate_d`, `front_wall_t`, `y_fw1`, `lid()` front wall boxes; `enclosure/controls.sh` "board fwd/back" lines

#### Scenario: Board too far forward
- **WHEN** controls.sh moves the board and DB9 ghosts 0.15 mm forward (`ghost_board_dy=-0.15`)
- **THEN** `check_tray` SHALL hit the bottom lip and `check_lid` SHALL hit the top lip

#### Scenario: Board too far back
- **WHEN** the ghosts move 0.2 mm back
- **THEN** `check_lid` SHALL hit the lid front wall and `check_tray` SHALL find no overlap; at 0.3 mm back `check_tray` SHALL hit the backstop ledge

#### Scenario: Lip near the D-shell
- **WHEN** a parameter change brings the top lip bottom (`lip_t_bot`) below `ds_top + lip_ds_clr` or the bottom lip top above `ds_bot - lip_ds_clr`
- **THEN** the model SHALL echo "ZEBCASE WARNING: a flange lip comes within lip_ds_clr of the D-shell"
- **AND** `build.sh` SHALL fail with a FAIL line naming that warning

### Requirement: RS-232 board location and hold-down
The board SHALL be located by two tray corner pins through its rear mounting holes (`mh_pin_clr` 0.2 per side in X,
lens-shaped with `mh_pin_clr_y` 0.45 in Y) and by the side walls (`pcb_clr` 0.2), stopped at the rear by a backstop
ledge `bs_clr` (0.25) behind the PCB, and held down by two lid corner pressers `press_gap` (0.25) above the PCB.

**Source**: `enclosure/zebserial_case.scad` `module corner_pin()`, `module corner_presser()`, `mh_*`, `bs_clr`, `bs_top`, `press_gap`, `cp_*`; `enclosure/controls.sh` "holes", "board +X/-X", "pcb up" lines

#### Scenario: Hole position error within tolerance
- **WHEN** the PCB ghost's holes move 0.4 mm in Y or 0.15 mm in X
- **THEN** `check_tray` SHALL find no overlap

#### Scenario: Hole position error beyond tolerance
- **WHEN** the holes move 0.5 mm in Y or 0.3 mm in X
- **THEN** `check_tray` SHALL hit the corner pins

#### Scenario: Board lifted
- **WHEN** the PCB ghost alone rises 0.3 mm (`ghost_board_dz=0.3`)
- **THEN** `check_lid` SHALL hit the corner pressers; at 0.2 mm it SHALL find no overlap

### Requirement: Solder-side clearance and leg-tail keep-out
The tray SHALL leave `below_clear` (3.0 mm) clear under the PCB for through-hole pins, and no tray feature SHALL
enter the board-lock leg-tail keep-out: each tail's estimated position grown by `leg_keepout` (1.5) in X and Y, from
the PCB underside to the floor. Leg-tail pockets SHALL stay off (`leg_pocket_on = false`) until the tails are measured.

**Source**: `enclosure/zebserial_case.scad` `below_clear`, `leg_y`, `lug_hole_x`, `leg_keepout`, `leg_pocket_on`, `ledge_y0`, `ghost_db9()` orange keep-out; `enclosure/controls.sh` "leg keep-out" lines

#### Scenario: Leg tails further out than estimated
- **WHEN** the keep-out moves 0.6 mm in +Y or +X
- **THEN** `check_tray` SHALL hit the side ledge or the side wall; at 0.4 mm it SHALL find no overlap

### Requirement: Jack screw hardware keep-out
The lid front wall SHALL be cut back over a keep-out for the DB9 jack screw tails and nuts behind each flange hole:
`jack_d` (7.0 mm) across, from Y `fl_plate_d` (or `jack_y0`) `jack_l` (5.0) back, centred on X +-12.5 and the
D-shell centre height, with `jack_clr` (0.3) above it. The keep-out SHALL be part of the DB9 ghost.

**Source**: `enclosure/zebserial_case.scad` `jack_on`, `jack_d`, `jack_l`, `jack_y0`, `jack_clr`, `jack_cut_z`, `ghost_db9()`, `lid()` cut-back; `enclosure/controls.sh` "jack hardware" and "rev 3 wings" lines

#### Scenario: Revision 3 front wall restored
- **WHEN** controls.sh sets `fw_wings=true`
- **THEN** `check_lid` and `check_lid_path` SHALL both hit the jack screw keep-out

### Requirement: XIAO placement
The XIAO SHALL lie on the DB9 rear body top (underside at Z `body_h`), its length along X with the USB end
`pcb_clr` from the +X inner wall and its front edge `pcb_clr` behind the lid front wall, unless `xiao_x0` or
`xiao_front_y` override that. Its rear edge SHALL be at least `xiao_hdr_min` (0.5) in front of the Dupont housing.

**Source**: `enclosure/zebserial_case.scad` `xiao_x0`, `xiao_front_y`, `xa0`, `xa1`, `yx0`, `yx1`, `zx0 = body_h`, `assert(hdr_y0 - yx1 >= xiao_hdr_min, ...)`, `front_rest_min`, `front_on_body`

#### Scenario: Housing too close
- **WHEN** a module change moves the header so that the XIAO rear edge is under 0.5 mm from the housing
- **THEN** OpenSCAD SHALL abort with "the XIAO rear edge is too close to the Dupont housing"

#### Scenario: Too little of the XIAO on the DB9 body
- **WHEN** the XIAO lies less than `front_rest_min` (2.0 mm) deep on the body
- **THEN** the model SHALL echo "ZEBCASE WARNING: the XIAO lies only ... mm on the DB9 body"
- **AND** `build.sh` SHALL fail with a FAIL line naming that warning

### Requirement: XIAO capture is rigid
The XIAO SHALL be held with no flexing parts: below by the DB9 body and a tray tongue cradle `tongue_gap` (0.05)
under the USB-C shell; above by lid pads `xpad_gap` (0.25) over the RF shield, the USB-C shell and the rear edge
strip; in X by the +X wall and a -X end stop rib, both `pcb_clr` off; in Y by the lid front wall and the USB slot sides at the front, and the shield stop block and the USB slot sides at the rear.

**Source**: `enclosure/zebserial_case.scad` `module xiao_pads()`, `module end_stop()`, `module usb_tongue()`, `xpad_gap`, `tongue_gap`, `stop_*`, `shield_stop_clr`; `enclosure/controls.sh` "xiao" lines

#### Scenario: XIAO displaced beyond its clearances
- **WHEN** the XIAO ghost moves up 0.3, down 0.06, +X 0.3, -X 0.3, forward 0.3 or back 0.35 mm
- **THEN** `check_lid` (or `check_tray` for down) SHALL hit the pads, the tongue, the +X wall, the stop rib, the front wall or the shield stop block respectively

#### Scenario: XIAO within its clearances
- **WHEN** the XIAO ghost moves up 0.2, down 0.04, or 0.15 mm in X or Y
- **THEN** the checks SHALL find no overlap

### Requirement: USB-C access
The XIAO's USB-C receptacle SHALL be reachable from outside through a slot in the +X wall with an overmould relief
`usb_recess_w` x `usb_recess_h` (12.0 x 7.0 mm, radius `usb_recess_r` 2.5) whose floor is flush with the receptacle
mouth. The relief SHALL be at least `usb_recess_d` (1.6 mm) deep, asserted by the model.

**Source**: `enclosure/zebserial_case.scad` `usb_*`, `usb_relief_depth`, `module usb_recess_cut()`, `assert(usb_relief_depth >= usb_recess_d - 1e-6, ...)`, `lid()` USB slot

#### Scenario: Side wall too thin for the relief
- **WHEN** `side_t`, `sag_frac` or the XIAO position leave the relief shallower than 1.6 mm
- **THEN** OpenSCAD SHALL abort with "the side face leaves the USB relief shallower than usb_recess_d"

### Requirement: Dupont housing and wire-bend room
The case SHALL keep clear a keep-out for the Dupont housing on the module header (to `dupont_top` above the PCB,
plus `dupont_rear_clr` behind it) and a wire-bend zone above it to the ceiling, `bend_side` wider each side. The
ceiling SHALL be at least `dupont_ceiling_min` (23.4) and `dupont_top + dupont_bend_h` above the PCB top.

**Source**: `enclosure/zebserial_case.scad` `hdr_*`, `dupont_*`, `bend_side`, `hdr_room`, `z_top`, `module ghost_dupont()`; `enclosure/controls.sh` "dupont" and "rev 3 rear face" lines

#### Scenario: Housing moved into the case
- **WHEN** the Dupont keep-out moves 1 mm -Y, 1 mm up or 1 mm +Y
- **THEN** `check_lid` SHALL hit the stop rib and shield stop block, the ceiling, or the rear wall respectively

### Requirement: Wire and joint keep-outs
The lid SHALL stay out of the wire keep-outs: the 5V/GND corridor over the XIAO (`wch_hw` 2.5 either side of the
5V and GND pads' mid-point), the front-edge joint zones at the 5V, GND and 3V3 pads (with a front wall relief,
`fw_relief`), and the rear-edge zone and joints at D3 and D5.

**Source**: `enclosure/zebserial_case.scad` `module wire_zones()`, `wch_c`, `wch_hw`, `used_pads_front`, `pullup_pad`, `used_pads_rear`, `fw_relief`; `enclosure/controls.sh` "USB pad over the corridor", "front wall without relief", "corridor zone", "rear zones" lines

#### Scenario: Front wall without its joint relief
- **WHEN** controls.sh sets `fw_relief=false`
- **THEN** `check_lid` SHALL hit the 5V/GND/3V3 joint zones

#### Scenario: Wider corridor than the case allows
- **WHEN** the corridor keep-out is 0.4 mm wider than the case's (`ghost_wch_hw=2.9`)
- **THEN** `check_lid` SHALL hit; at 0.2 mm wider (`2.7`) it SHALL find no overlap

### Requirement: Pads match the firmware's pins
The pads the case leaves room for SHALL be the ones the firmware and wiring use: `used_pads_rear = [4, 6]` (D3 RX
through the diode, D5 TX; GPIO21 and GPIO23 in `firmware/main/board.c`) and `used_pads_front = [1, 2]` (5V, GND),
plus `pullup_pad = 3` (3V3). A pin change in the firmware or wiring MUST be mirrored in these parameters.

**Source**: `enclosure/zebserial_case.scad` `used_pads_front`, `pullup_pad`, `used_pads_rear`, `function pad_x()`; `README.md` Wiring

#### Scenario: Pad positions
- **WHEN** the model is rendered with the current parameters
- **THEN** the echo "ZEBCASE PADS" SHALL put D3 at X 5.459 and D5 at X 0.379 on the rear edge (Y 25.08), and 5V at X 13.079 and GND at X 10.539 on the front edge (Y 7.3)

### Requirement: RX diode keep-out
The case SHALL keep clear a keep-out for the RX Schottky diode at the D3 pad: by default a DO-41 body (`diode_d`
2.7 x `diode_l` 5.2) standing up beside the Dupont housing's +X end, anode lead to D3, cathode (band) up to meet the
module's RXD wire, grown by `diode_clr` (0.3). The rigid diode body SHALL also be swept in `check_lid_path`.

**Source**: `enclosure/zebserial_case.scad` `diode_*`, `dio_c`, `module ghost_diode()`, `module diode_zone()`, `check_lid_path`; `enclosure/controls.sh` "diode" and "lid block under the diode" lines

#### Scenario: Diode moved towards the shield stop block
- **WHEN** the diode ghost moves 0.4 mm -Y
- **THEN** `check_lid` SHALL hit the shield stop block; at 0.2 mm it SHALL find no overlap

#### Scenario: A lid block under the diode
- **WHEN** controls.sh sets `ctl_diode_undercut=true`
- **THEN** `check_lid` SHALL find no overlap and `check_lid_path` SHALL hit

### Requirement: Pin 9 power wire path
The case SHALL keep a path for the wire from the DB9 pin 9 pad (solder side) to the XIAO 5V pad: a joint zone
under the pad, a run `wire_pass` (2.0) wide under the board at X `p9_run_x`, a notch in the backstop and a riser
behind the PCB to the ceiling. `wire_pass` MUST leave at least 0.4 mm round `wire_d` (asserted).

**Source**: `enclosure/zebserial_case.scad` `p9_*`, `wire_d`, `wire_pass`, `assert(!p9_on || wire_pass >= wire_d + 0.4 - 1e-6, ...)`, `wire_zones()`, `tray()` backstop notch; `enclosure/controls.sh` "pin 9 wire" and "no backstop notch" lines

#### Scenario: Backstop without its notch
- **WHEN** controls.sh sets `p9_notch=false`
- **THEN** `check_tray` SHALL hit the backstop

### Requirement: Joints over the jack screw hardware are top-side only
The model SHALL echo the X span of the front-edge joints and the gap between the jack screw keep-out top and the XIAO
underside; those joints SHALL be soldered on the top side only.

**Source**: `enclosure/zebserial_case.scad` echo "ZEBCASE NOTE (check): the front-edge 5V/GND/3V3 joints ..."; `enclosure/README.md` Fit-check list

#### Scenario: Reading the build output
- **WHEN** the model is rendered
- **THEN** the echo SHALL report the front joints at X 6.229..14.849 and 0.65 mm between the jack screw keep-out top (z 9.75) and the XIAO underside for the current module
- **AND** it SHALL be a "ZEBCASE NOTE", printed on every build, which does not fail `build.sh` or `controls.sh`

### Requirement: Interference gate
`build.sh` SHALL render four intersections, `check_tray` (ghosts and tray), `check_lid` (ghosts and lid),
`check_traylid` (tray and lid) and `check_lid_path` (lid and the rigid parts swept 30 mm down), and SHALL fail if any
holds a piece of at least 0.01 mm3 thicker than 0.005 mm in every axis. Zero-thickness contacts SHALL pass.

**Source**: `enclosure/build.sh` interference loop; `enclosure/stlinfo.py` `hits()`; `enclosure/zebserial_case.scad` `part == "check_*"`

#### Scenario: Nominal model
- **WHEN** `./build.sh` is run on the current parameters
- **THEN** all four checks SHALL report "none" and the script SHALL end "build: all gates passed" with exit 0

### Requirement: OpenSCAD errors fail the gates
`build.sh` and `controls.sh` SHALL treat an OpenSCAD `ERROR:` or `WARNING:` line, a failed assertion, "Execution
aborted", or a non-zero exit other than "Current top level object is empty." as a failure.

**Source**: `enclosure/build.sh` `osc_ok()`; `enclosure/controls.sh` `ctl()`

#### Scenario: An inverted box
- **WHEN** a parameter change makes a `box()` call with bounds out of order
- **THEN** the `box()` assertion SHALL fire instead of the box silently rendering as nothing
- **AND** if the box is in a ghost, the check run SHALL report "openscad error" and the gate SHALL fail; if it is in the tray or lid, `build.sh` SHALL stop at the first STL export with a non-zero exit and no FAIL line, after deleting `out/*.stl` and `renders/*.png`

### Requirement: Geometry warnings fail the build
The model SHALL echo "ZEBCASE WARNING: ..." only for wrong geometry and "ZEBCASE NOTE ..." for reminders.
`build.sh` SHALL evaluate the model once with its extra `-D` overrides (OpenSCAD echo export) and SHALL print each
"ZEBCASE WARNING" line as a FAIL line carrying its text, so the build fails. `controls.sh` SHALL count a run that
echoes one as got "error".

**Source**: `enclosure/build.sh` "parameter warnings" step; `enclosure/controls.sh` `ctl()`; `enclosure/zebserial_case.scad` `ZEBCASE WARNING` and `ZEBCASE NOTE` echoes

#### Scenario: A geometry warning is echoed
- **WHEN** `./build.sh` runs with an override that triggers a warning, for example `./build.sh -D lip_x=10`
- **THEN** `build.sh` SHALL print "FAIL: ZEBCASE WARNING: the lips reach the jackpost area at the flange holes"
- **AND** it SHALL end "build: 1 gate(s) FAILED" and exit 1

#### Scenario: Every geometry warning is gated
- **WHEN** an override sets the top over `max_above`, a lip within `lip_ds_clr` of the D-shell, the lips into the jackpost area, the XIAO under `front_rest_min` on the DB9 body, or the leg-tail pocket within 0.8 mm of the floor bottom
- **THEN** `build.sh` SHALL print that warning as a FAIL line and exit 1 (the `max_above` case also fails its assertion)

#### Scenario: Only notes are echoed
- **WHEN** `./build.sh` runs on the current parameters, which echo the "ZEBCASE NOTE (check)" line on the front-edge joints and no warning
- **THEN** the parameter step SHALL print the note and "parameter warnings: none"
- **AND** the script SHALL end "build: all gates passed" with exit 0

### Requirement: Wall thickness gate
`build.sh` SHALL export both parts in assembly position (`tray_inplace`, `lid_inplace`) and run `wallprobe.py`, and
SHALL fail on any sample thinner than 1.2 mm (wallprobe.py's `--min` default, not read from the SCAD; a change to `min_wall`
SHALL also pass `--min` in `build.sh`) outside the whitelisted boxes: the tongue edges, the floor
gusset and the floor under the logo (tray), and the snap hooks (lid).

**Source**: `enclosure/build.sh` `ALLOW_tray`, `ALLOW_lid`, wallprobe loop (no `--min`); `enclosure/wallprobe.py` `--allow`, `--min` default 1.2 (line 63)

#### Scenario: A new sliver
- **WHEN** a change leaves a wall under 1.2 mm outside a whitelist box
- **THEN** wallprobe SHALL list it as NEW, exit 1, and `build.sh` SHALL report "wall sliver(s) not on the whitelist"

### Requirement: Logo inlay
The logo SHALL be inlaid `logo_depth` (0.68 mm) into the outside of the tray floor as a separate body (`part="logo"`,
`out/logo.stl`, in tray coordinates), leaving at least 0.9 mm of floor (asserted), with every stroke and gap at least
`logo_min` (1.2 mm), checked by `part="check_logo"`.

**Source**: `enclosure/zebserial_case.scad` `logo_*`, `module logo_2d()`, `module logo_body()`, `part == "check_logo"`, the two logo asserts; `enclosure/build.sh` logo gate; `enclosure/controls.sh` "logo at 25 mm" / "logo at 20 mm"

#### Scenario: Logo made too small
- **WHEN** `logo_w` is 20
- **THEN** `check_logo` SHALL hit and the gate SHALL fail

### Requirement: Positive and negative controls
`controls.sh` SHALL run 97 controls, each moving one ghost or feature and stating whether a named check must find a
hit or none, and SHALL exit non-zero on any mismatch. A change that adds or moves a keep-out or a locating feature
MUST add or recalibrate a control that shows the check can see it.

**Source**: `enclosure/controls.sh` `ctl()` and its 97 `ctl` lines; `enclosure/README.md` Interference checks table

#### Scenario: Current model
- **WHEN** `./controls.sh` is run
- **THEN** every line SHALL print PASS and the script SHALL end "controls: 0 failed" with exit 0

#### Scenario: A control calibrated to old geometry
- **WHEN** a module change moves a feature so that a control's distance no longer straddles the clearance
- **THEN** that control SHALL fail, and its distance SHALL be recalibrated to the new clearance and the reason noted in the control's comment

### Requirement: Build outputs
`build.sh` SHALL delete and regenerate `out/tray.stl`, `out/lid.stl`, `out/logo.stl`, `out/coupon_tray.stl`,
`out/coupon_lid.stl` and every `renders/*.png`, and SHALL print each STL's volume and bounding box. These outputs
are tracked in git and SHALL be committed with the SCAD change that produced them.

**Source**: `enclosure/build.sh` `rm -f out/*.stl renders/*.png`, STL export and renders sections, final `enclosure/stlinfo.py` call

#### Scenario: Unchanged parameters
- **WHEN** `build.sh` runs on the committed SCAD with OpenSCAD 2026.06.12 and the manifold backend
- **THEN** the five STLs SHALL be byte-identical to the committed ones

### Requirement: Fit coupons
The model SHALL provide two single-colour fit coupons in print orientation: `coupon_tray` (the whole tray with the
middle of the floor cut out, `coupon_floor`) and `coupon_lid` (the lid below `coupon_z` 18.0, without the ceiling
and XIAO pads). For any change to a fit-setting parameter (module, DB9, XIAO position, clearances) the coupons SHALL be
printed and the board, DB9 and XIAO test-fitted before the full case.

**Source**: `enclosure/zebserial_case.scad` `part == "coupon_tray"`, `part == "coupon_lid"`, `coupon_floor`, `coupon_z`; `print/zebserial_coupon.3mf`; `enclosure/README.md` "Concerns to check before printing"

#### Scenario: First print for a new module
- **WHEN** the STLs are regenerated for a different RS-232 module
- **THEN** the coupons SHALL be sliced and printed first

#### Scenario: Coupon fits
- **WHEN** the board drops onto the coupon tray's corner pins with the flange behind the bottom lip and the PCB rear edge in front of the backstop ledge, the XIAO lies in place, and the coupon lid closes and its snaps engage
- **THEN** the full case SHALL be printed from `print/zebserial_case.3mf`

### Requirement: Exterior style
The exterior SHALL be the faceted pincushion style: side and rear faces bowed in by `sag_frac` (0.02) of their
length in three facets (`facet_flat` 0.3), a flat front face (`front_sag` 0), `corner_chamfer` 1.5, `corner_facet`
5.0, `edge_chamfer` 1.5, flat top and bottom, and a seam reveal groove on the lid. Both parts SHALL be cut from one `outer_solid()`.

**Source**: `enclosure/zebserial_case.scad` "Exterior style" parameters, `module outer_solid()`, `function outline()`, `module reveal_cut()`

#### Scenario: Seam contact
- **WHEN** `check_traylid` runs on the current parameters
- **THEN** it SHALL find no overlap, and its zero-thickness contact SHALL lie in the plane z = `split_z` (4.0), where the rims meet

### Requirement: Snap clips
The lid SHALL carry two snap fingers per side wall (`lc_w` 6.0 wide, `lc_t` 1.4 thick, at Y `lc_front_y` and
`lc_rear_from_back` from the interior rear face) whose hooks catch windows through the tray wall, and the snaps SHALL
still engage with the lid held up to `lc_win_up` (0.35 mm) high.

**Source**: `enclosure/zebserial_case.scad` `lc_*`, `module lid_clip_finger()`, `module lid_clip_tray_cut()`, `module lid_clip_lid_cut()`; `enclosure/controls.sh` "lid up", "snap pockets" lines

#### Scenario: Lid held high
- **WHEN** the lid is raised 0.3 mm in `check_traylid`
- **THEN** there SHALL be no overlap; at 0.4 mm, or 0.3 mm with `lc_win_up=0.1`, the hooks SHALL hit

### Requirement: Measurements recorded with their provenance
Each module or XIAO parameter SHALL say in its comment whether it was measured on the part in use, taken from a
datasheet or CAD file, borrowed from another part, or estimated. A borrowed or estimated value that sets a fit SHALL
be listed in the enclosure README's fit-check list until measured.

**Source**: `enclosure/zebserial_case.scad` parameter comments ("measured", "assumed as for the YL-97", "ESTIMATE"); `enclosure/README.md` "Fit-check list (measure before printing)"

#### Scenario: Estimates listed
- **WHEN** the SCAD marks `leg_y`, `hdr_row_in` or `p9_y` as ESTIMATE
- **THEN** the README's fit-check list SHALL list each of them

### Requirement: Ghosts are checked against the case only
The checks SHALL be understood to prove clearance between the case and each ghost or keep-out, not between ghosts.
A change that moves the diode, the XIAO, the header or the wires SHALL be checked against the other parts by
inspecting renders (for example `renders/diode_iso.png`, `xiao_lid_off.png`, `section_diode_zones.png`).

**Source**: `enclosure/zebserial_case.scad` `part == "check_tray"` and `part == "check_lid"` (each intersects one part with `ghosts()`); `enclosure/README.md` "The RX diode", Other placements

#### Scenario: Diode laid along X
- **WHEN** `check_lid` runs with `diode_dir = [1, 0, 0]` and `diode_c = [9, 26.73, 13.3]`
- **THEN** it SHALL find no overlap, although nothing checks that diode position against the housing, the header or the XIAO

### Requirement: Printable without supports
Both parts SHALL print in their exported orientation with supports off: every down-facing face SHALL be at most
45 degrees from vertical or a short bridge. This is checked by inspection of the slices only; no script in the
repository checks overhangs.

**Source**: `enclosure/README.md` "Print settings" (Overhangs list; the probe it mentions is not in the repository); `print/process_overrides.json` `enable_support` "0"

#### Scenario: A new feature with an overhang
- **WHEN** a change adds a down-facing face flatter than 45 degrees in print orientation
- **THEN** no gate SHALL catch it, and the slicer preview SHALL be inspected before printing

### Requirement: Lead-ins steer parts as the lid closes
The case SHALL carry 45 degree lead-ins that push misplaced parts into place as the lid goes on: `fw_lead` (0.5) and
`jack_lead` (0.3) on the lid front wall, `xlead` (0.4) on the front wall and shield stop block at XIAO height,
`stop_lead` (0.4) on the stop rib, `lip_lead` (0.3) on the lips and `mh_pin_lead` (0.4) on the corner pins.

**Source**: `enclosure/zebserial_case.scad` `fw_lead`, `jack_lead`, `xlead`, `stop_lead`, `lip_lead`, `mh_pin_lead`, `lid()`, `end_stop()`, `xiao_pads()`, `corner_pin()`

#### Scenario: XIAO slightly too far forward
- **WHEN** the lid is rendered with the current parameters
- **THEN** the front wall's rear face below the XIAO underside SHALL be set back `xlead` (0.4 mm), rising at 45 degrees to z 10.8, so a XIAO up to 0.4 mm too far forward is pushed back as the lid closes

### Requirement: Logo modes and orientation
`logo_mode` SHALL be "inlay" (default), "deboss" or "none", any other value failing an assertion. The mark SHALL be
mirrored in X so it reads correctly seen from outside the floor (-Z), with the D-shell end towards Y 0.

**Source**: `enclosure/zebserial_case.scad` `logo_mode`, `assert(logo_mode == "none" || ...)`, `module logo_2d()` (`mirror([1, 0])`)

#### Scenario: Unknown mode
- **WHEN** `logo_mode = "emboss"`
- **THEN** OpenSCAD SHALL fail with "logo_mode must be inlay, deboss or none"

### Requirement: Thin skin over the antenna
The only material over the XIAO's ceramic antenna SHALL be the lid top skin, `ceil_t` (1.6 mm), with no pad, rib or
wall above it.

**Source**: `enclosure/zebserial_case.scad` `ceil_t`, `ant_w`, `ant_d`, `st_y0` (`stop_ant_clr` behind the antenna), `xiao_pads()`

#### Scenario: Stop rib beside the antenna
- **WHEN** the model is rendered
- **THEN** the stop rib SHALL start at Y 15.94, 0.27 mm behind the antenna's end at Y 15.67 (echo "ZEBCASE SUPPORT XIAO")

### Requirement: Pressers clear of the tray walls
Below the split the corner pressers SHALL stand `press_wall_clr` (0.2 mm) off the tray side and rear walls.

**Source**: `enclosure/zebserial_case.scad` `press_wall_clr`, `module corner_presser()`; `enclosure/controls.sh` "presser into the tray wall"

#### Scenario: Presser into the wall
- **WHEN** controls.sh sets `press_wall_clr=-0.1`
- **THEN** `check_traylid` SHALL hit both corner pressers

### Requirement: What sets the case height
`z_top` SHALL be the largest of: the XIAO top plus `xiao_top_clear` and `ceil_t`; the USB relief top plus `wall`; the
relief top plus `edge_chamfer` plus `min_wall` times the square root of 2 (1.2 mm square to the top chamfer); and,
with `dupont_on`, the larger of `dupont_top + dupont_bend_h` and `dupont_ceiling_min`, plus `ceil_t`.

**Source**: `enclosure/zebserial_case.scad` `z_top`

#### Scenario: Current case
- **WHEN** the model is rendered with the current parameters
- **THEN** the Dupont term SHALL set `z_top` to 25.0 (23.4 + 1.6); the USB relief alone would give 19.95

## Notes

### Where the model leaves the module's shape to assumptions

These are built into the geometry, not parameters. A module that breaks one needs a SCAD change, not a parameter edit.

- The DB9 is centred on X 0; the header row is centred on X 0 on the rear edge (`hdr_hw` symmetric); the two mounting
  holes are at X +-`mh_pitch`/2 on the rear edge; the leg tails are mirrored at X +-`lug_hole_x`. There is no X offset
  parameter for any of them, and `mirror_x_both()` places pins, pressers and pockets in pairs.
- The header has one row (housing 2.54 mm deep, `hdr_y0`/`hdr_y1` = row centre +-1.27) at 2.54 mm pitch.
- The board is held at its two rear corner holes; there is no support for front holes or holes elsewhere.
- The XIAO underside sits on the DB9 rear body top (`zx0 = body_h`); there is no separate XIAO height parameter.
- The flange bottom is level with the PCB top (`fl_bot` 0) and the D-shell is centred on the flange (`hole_z`).
  `fl_bot` values other than 0 are untested: they move the flange, D-shell, lips and jack keep-out, but the rear
  body, the lugs and the lid's body cut stay at z 0.
- No component-side parts of the RS-232 board (the MAX3232, capacitors, LED) are modelled, so the checks cannot see
  them. Any part taller than 9.8 mm (`body_h - 0.6`, the stop rib and web bottom behind the DB9 body, web at X
  -16.2..-5.155), any part under the XIAO footprint (X -4.955..16, Y 7.3..25.08) taller than `body_h` (10.4), anything
  under the corner pressers (0.25 mm above the PCB) and anything under the lid front wall (lowest edge z 10.05 beside
  the body, 10.6 over it) collides unseen.
- Sentinels: `pcb_front_y` < 0 means `fl_t`; `lug_hole_x` < 0 means `fl_hole_pitch / 2`; `xiao_x0`, `xiao_front_y`,
  `wch_c`, `jack_y0`, `ghost_plate_d` and `ghost_wch_hw` = -1 and `diode_c` = [-1, -1, -1] mean derived.
- The D-shell ghost is a trapezoid with 1.5 mm side insets at the bottom (`ghost_db9()`), a fixed shape.
- The through-hole pin zone ghost is 1.25 mm in from the PCB edges and 2.5 mm deep, fixed in `ghost_rs232()`.

### Hard-coded positions that a module change must revisit

- Absolute values typed as numbers: `lc_front_y` 6.3 (set by the USB slot position), `coupon_z` 18.0, `p9_run_x` -8.0
  (set by the header joints and the corner-pin boss and presser X), `p9_x` -4.155 (worked out by hand from the DE-9
  pitch), `logo_w` 25 (sized to the floor flat; the margins are echoed, not asserted).
- The stop rib and web bottom `zx0 - 0.6` and their 1.2 mm minimum length (`end_stop()`), the snap hook height
  `lc_tip + 1.4`, the pin-zone ghost depth 2.5 (it does not follow `below_clear`), and the ghost wire diameter 1.6 in
  `wire()` (not `wire_d`).
- `ctl_undercut` box at X -4.5..-2.5 (under the unused D6 pad) and `ctl_diode_undercut` box at X 7.6..8.0, Y
  26.9..27.9, z 10.4..11.0, in `lid()`: absolute coordinates for two positive controls.
- `build.sh` render cameras and section positions (for example Y 16.2 through the USB axis, Y 27 through the pins,
  X 13.25 through a pin, z 13.25 at the USB axis, Y 24.0 through the diode, Y 7.6 through the leg tails).
- `build.sh` wall-probe whitelist boxes `ALLOW_tray` and `ALLOW_lid`, in absolute assembly coordinates.
- `controls.sh` distances, each calibrated to a current clearance (for example "rear zones -X 3.6/3.9", "diode +Y
  2.8", "corridor zone 0.2/0.4 wider").
- Joint-zone sizes in `wire_zones()`: front-edge zones run from 0.9 mm in front of the XIAO front edge back to the
  shield edge; rear-edge zones from 2.6 mm inside to 0.9 mm outside the rear edge, from z 0.3 to 3 mm above the XIAO
  top. Also the
  diode's anode joint offset (`dio_joint`: 0.6 in, 0.4 up), and the ghost wire routes in `ghost_wires()`.

### Other literals that depend on the module

Found by reading the SCAD on 2026-10-31. None is a parameter; each holds for the HW-044 and may not for another module.

- `skin_y1 = max(y_cf, y_pcb0) + 1.2`: the tray front skin supports the first 1.2 mm of the PCB's solder side, so
  nothing on the solder side may sit within 1.2 mm of the front edge.
- The side ledges end 0.5 mm in front of the PCB rear edge (`y_pcb1 - 0.5` in `tray()`), and start 0.05 mm behind the
  leg-tail keep-out plus the pin play (`ledge_y0`).
- The DB9 lugs are drawn from the body side out to the full PCB width (`box(body_w / 2, board_w / 2, ...)` in
  `ghost_db9()`): a module whose lugs stop short, or overhang the PCB, needs that ghost changed.
- `leg_boss_top = -pcb_t - 0.2` (only with `leg_pocket_on`).
- The coupon floor cut-out keeps `ledge_w + 1.5` at the sides, 3 mm behind `skin_y1` and 1.5 mm in front of the pin
  bosses (`part == "coupon_tray"`): with other hole positions the cut can reach the bosses.
- `check_lid_path` sweeps the parts 30 mm down; a part taller than that above the floor would not be swept fully.
- Header pins in the ghost wire routes sit at `-hdr_hw + 1.27 + i * 2.54` (`ghost_wires()`), the same one-row,
  2.54 mm-pitch assumption as the housing.

XIAO- and case-side literals (they move only if the XIAO or the style changes): the front wall joint relief is 0.8 mm
deep and runs to `zx1 + 3.3`; the shield stop block keeps 0.3 mm from the D3 joint zone and the corridor; the stop rib
and web reach 0.6 mm below the XIAO; the pressers root 0.5 mm into the lid walls; the snap hook is 1.4 mm tall
(`lc_hook_top`); the tongue width uses `clip_clr / 2`; the gusset keeps 1.2 mm square to the chamfer.

### RS-232 module parameters

| Group (SCAD heading) | Parameters | Current value and provenance |
|---|---|---|
| PCB | `board_w`, `board_d` | 32.0, 29.0: measured (HW-044) |
| PCB | `pcb_t`, `below_clear` | 1.6, 3.0: assumed as for the YL-97 |
| PCB | `pcb_front_y` | 0, flush: assumed (fit-check list) |
| Mounting holes | `mh_d`, `mh_pitch`, `mh_inset` | 2.4, 26.5, 2.0: measured |
| DB9 flange | `fl_w`, `fl_h`, `fl_t`, `fl_hole_pitch`, `fl_bot` | 30.8, 12.5, 0.38, 25.0, 0: YL-97 male part, assumed the same |
| DB9 plate | `fl_plate_d` | 5.1: measured on the YL-97 ("the back of the flange plate screw holes"), assumed the same |
| D-shell | `ds_w`, `ds_h`, `ds_proj` | 16.9, 8.4, 6.0: YL-97, assumed |
| Rear body | `body_w`, `body_h`, `body_end` (`body_d` derived) | 17.2, 10.4, 12.5: measured on the YL-97, assumed |
| Step behind flange | `step_on`, `step_h`, `step_d`, `step_w` | off; values from a Norcomp drawing (`reference/db9_male_ra`) |
| Lugs | `lug_h`, `lug_end`, `lug_hole_y` | 3.4, 12.0, 4.6: YL-97 |
| Leg tails | `leg_y`, `lug_hole_x` | 8.6, 12.65: ESTIMATE from a photo |
| Leg tails | `leg_tail_l`, `leg_w`, `leg_t` | 3.0, 3.1, 0 (round): YL-97 |
| Leg tails | `leg_keepout`, `leg_pocket_on`, `leg_clr`, `leg_clr_y`, `leg_slot_x`, `leg_leadin` | 1.5, off; pocket values used only when on |
| Lug pins (off) | `lug_pin_on`, `lug_hole_d`, `lug_hole_depth`, `pin_d`, `pin_flat`, `pin_engage`, `pin_boss_d` | off, kept for reference |
| Header | `hdr_pins`, `hdr_row_in`, `hdr_h` | 4; 1.5 ESTIMATE from a photo; 8.5 |
| Dupont plug | `dupont_on`, `dupont_top`, `dupont_ceiling_min`, `dupont_bend_h`, `dupont_rear_clr`, `bend_side` | on; 16.7 and 23.4 measured (18.3 above the PCB underside, 25 mm turning space); 4.0, 1.5, 2.0 design room |
| Jack screws | `jack_on`, `jack_d`, `jack_l`, `jack_y0`, `jack_clr`, `jack_lead` | on, 7.0, 5.0 (estimate: M3 or 4-40 nut), -1 = `fl_plate_d`, 0.3, 0.3 |
| Pin 9 pad | `p9_x`, `p9_y`, `p9_joint`, `p9_joint_h` | -4.155 (DE-9 pitch 2.77 centred on X 0); 7.1 ESTIMATE; 3.0; 2.5 |
| Pin 9 wire | `p9_on`, `wire_d`, `wire_pass`, `p9_run_x`, `p9_notch` | on, 1.4 (26 AWG jumper), 2.0, -8.0, on |
| Board location | `mh_pin_clr`, `mh_pin_clr_y`, `mh_pin_top`, `mh_pin_lead`, `mh_boss_d`, `bs_clr`, `bs_top` | 0.2, 0.45, -0.1, 0.4, 4.4, 0.25, -0.2 |
| Board hold-down | `cp_front`, `cp_inner`, `cp_bore_clr`, `press_gap`, `press_wall_clr`, `press_root` | 0.8, 2.0, 0.3, 0.25, 0.2, 1.5 |
| Retention lips | `lip_x`, `lip_proud`, `lip_gap`, `lip_reach`, `lip_ds_clr`, `lip_root`, `lip_lead` | 8.0, 1.3, 0.1, 1.0, 1.0, 1.0, 0.3 |
| Lid front wall | `front_wall_t`, `fl_clr_z`, `fw_lead`, `fw_relief` | 2.0, 0.3, 0.5, on |
| Side ledges | `ledge_w`, `ledge_front_skip` | 1.2, 7.0 |
| RX diode | `diode_on`, `diode_d`, `diode_l`, `diode_lead_d`, `diode_clr`, `diode_lead_out`, `diode_dir`, `diode_c` | on; DO-41 worst case 2.7, 5.2, 0.8 (not measured); 0.3, 0.6; [0, 0, 1]; derived |

### XIAO ESP32-C6 parameters

From Seeed's XIAO ESP32-C6 v1.0 KiCad PCB and STEP model, except where marked measured; `usb_w` from the GCT USB4105
drawing.

| Parameters | Values |
|---|---|
| `xiao_l`, `xiao_w`, `xiao_pcb_t` | 20.955, 17.78, 1.2 (measured) |
| `xiao_pitch`, `xiao_pad1` | 2.54, 2.921 (pad 1 centre from the USB end) |
| `usb_w`, `usb_h`, `usb_l`, `usb_over` | 8.94, 3.3 (measured), 7.35, 1.5 |
| `shield_w`, `shield_d0`, `shield_d1`, `shield_h` | 12.6, 6.71, 17.31, 2.2 (measured) |
| `ant_w`, `ant_d`, `ufl_w`, `ufl_d`, `ufl_h` | ceramic antenna w 0.52..5.72, d 18.8..20.8; U.FL w -5.6..-3.0, d 18.25..20.85, 2.5 high mated |
| `btn_w`, `btn_d` | buttons at abs(w) 5.3..7.9, d 0.3..1.9 |
| `used_pads_front`, `pullup_pad`, `used_pads_rear` | [1, 2] (5V, GND), 3 (3V3), [4, 6] (D3, D5) |
| `xiao_x0`, `xiao_front_y` | -1 (derived) |
| Capture: `xiao_top_clear`, `front_rest_min`, `xpad_gap`, `xpad_inset`, `xpad_wall`, `xlead`, `shield_stop_clr`, `tongue_gap`, `tongue_side_clr`, `tongue_tip`, `stop_ant_clr`, `stop_t`, `stop_web_t`, `stop_z_clr`, `stop_lead` | 4.5, 2.0, 0.25, 0.5, 1.6, 0.4, 0.3, 0.05, 0.3, 0.5, 0.27, 1.6, 1.6, 0.1, 0.4 |
| Corridor: `wch_c`, `wch_hw` | -1 (mid-point of 5V and GND), 2.5 |
| USB relief: `usb_clr`, `usb_recess_w`, `usb_recess_h`, `usb_recess_d`, `usb_recess_r`, `usb_mouth_inset` | 0.2, 12.0, 7.0, 1.6, 2.5, 0.0 |

### Case-wide parameters

| Group | Parameters | Values |
|---|---|---|
| Print and tolerance | `wall`, `side_t`, `floor_t`, `ceil_t`, `pcb_clr`, `clip_clr`, `split_z`, `min_wall` | 1.6, 4.0, 1.6, 1.6, 0.2, 0.3, 4.0, 1.2 |
| Envelope | `max_above` | 25.0 |
| Style | `sag_frac`, `front_sag`, `facet_flat`, `corner_chamfer`, `corner_facet`, `edge_chamfer`, `seam_reveal`, `reveal_d`, `reveal_h` | 0.02, 0, 0.3, 1.5, 5.0, 1.5, on, 0.45, 0.5 |
| Logo | `logo_mode`, `logo_w`, `logo_depth`, `logo_file`, `logo_svg_w`, `logo_svg_h`, `logo_svg_c`, `logo_min`, `logo_round` | "inlay", 25.0, 0.68, `../assets/logo/zebserial-mark-deboss.svg`, 52.77, 52.0, [32, 29.93], 1.2, 0.62 |
| Snaps | `lc_w`, `lc_t`, `lc_gap`, `lc_slot_back`, `lc_hook`, `lc_win_up`, `lc_above`, `lc_below`, `lc_side_gap`, `lc_pry`, `lc_front_y`, `lc_rear_from_back` | 6.0, 1.4, 0.15, 0.6, 0.6, 0.35, 4.0, 5.0, 0.5, 0.9, 6.3, 3.6 |
| Coupons | `coupon_floor`, `coupon_z` | true, 18.0 |
| Geometry | `$fn`, `eps` | 48 facets per circle, 0.01 overlap for clean booleans |
| View only | `part`, `show_ghosts`, `show_pin_zone`, `show_zones`, `show_lid`, `section`, `section_pos`, `section_t`, `explode` | do not affect the STLs |
| Controls only | `ghost_*`, `ctl_*`, `fw_wings`, `p9_notch`, `fw_relief` | defaults leave the nominal model; `enclosure/controls.sh` sets them |

Derived results for the current parameters (from the model's `ZEBCASE` echo lines): case 40.4 x 33.458 x 31.2 mm,
top 25.0 above the PCB top; XIAO X -4.955..16, Y 7.3..25.08, underside z 10.4; USB mouth X 17.5 at Y 16.19, z 13.25,
relief depth 2.03; corner pins at X +-13.25, Y 27; Dupont housing X +-5.08, Y 26.23..28.77; interior rear face Y 31.

### Measurements a new RS-232 module needs

Take each with calipers on the part to be used, in the frame above (Y from the flange front face, Z from the PCB
top). The current values came the same way: the HW-044 board outline, holes and Dupont height were measured; the
DB9 values were measured on the earlier YL-97 module and assumed for the HW-044; leg tails, header row and pin 9 pad
were estimated from photos. `enclosure/reference/` is gitignored and holds only
datasheets and the Seeed CAD files, not measurements.

1. PCB width `board_w`, depth `board_d` and thickness `pcb_t`; whether its front edge is flush with the flange front
   face (`pcb_front_y`).
2. Mounting holes: diameter `mh_d`, centre-to-centre pitch along X `mh_pitch`, rear PCB edge to the hole centres
   `mh_inset`. The model supports two holes at the rear corners only.
3. DB9 flange: width, height, steel thickness, screw-hole pitch, flange bottom height relative to the PCB top.
4. Depth from the flange front face to the back of the plate the lid front wall will bear on (`fl_plate_d`). Measure
   with the caliper depth rod from the flange front face, beside the D-shell, to the rear face of the plate at the
   screw holes (the current 5.1 was taken that way on the YL-97: "the back of the flange plate screw holes").
5. D-shell width, height and projection; rear body width, height and depth; any step behind the flange; side lugs.
6. Board-lock leg tails under the PCB: position (`leg_y` from the flange front face to the tail centre, `lug_hole_x`
   from the PCB centre line), shape (`leg_w`, `leg_t`) and protrusion below the PCB (`leg_tail_l`). Measure on the
   solder side: Y with the caliper depth rod from the flange front face, X as half the outside-to-outside distance
   between the two tails less half a tail width. Tallest solder-side pin or fillet (`below_clear`).
7. Header: pin count `hdr_pins`, row position from the rear edge `hdr_row_in`, X offset (the model assumes centred),
   pin height `hdr_h`; Dupont housing top above the PCB `dupont_top`. For the wire turning space
   (`dupont_ceiling_min`), plug the housing on with the real wires, bend them over towards the XIAO as they will lie,
   and measure the highest point of the bend from the PCB underside; subtract `pcb_t` (the current 23.4 is the
   user's 25 mm from the underside less 1.6).
8. Pin 9 pad position on the solder side; the jack screw hardware actually fitted (tail and nut size, length).
9. Every component-side part: its height and position. Any part taller than 9.8 mm (`body_h - 0.6`), any part
   under the XIAO footprint taller than `body_h`, and anything near the rear corners (pressers 0.25 above the PCB)
   collides, and the checks cannot see it, because component-side parts are not modelled. Solder side: nothing
   within 2.2 mm of a hole centre (pin bosses).
10. The diode actually fitted and how its anode lead is bent.

### Regeneration procedure

0. Decide whether parameters are enough. If the new module has mounting holes at the front or off the rear corners,
   holes or a DB9 off the X centre line, a two-row or offset header, or a flange whose bottom is not level with the PCB
   top, stop: the shape needs a SCAD edit first (see "Where the model leaves the module's shape to assumptions" and
   "Other literals that depend on the module" above), and that edit is a design change, not a regeneration.
1. Edit only the parameters in `enclosure/zebserial_case.scad` (and comments recording provenance). Mark every
   value that was not measured on the new part.
2. Run `./enclosure/build.sh` (needs OpenSCAD with the manifold backend at `/opt/homebrew/bin/openscad`, or set the
   `OPENSCAD` environment variable, which both scripts read as `OSC=${OPENSCAD:-/opt/homebrew/bin/openscad}`; the
   enclosure README does not mention it; python3 with numpy for `wallprobe.py`). About 20 s on 2026-10-31. It must end "build: all gates passed".
   `build.sh` deletes `out/*.stl` and `renders/*.png` first; if a gate fails, restore them with
   `git checkout enclosure/out enclosure/renders` before committing anything.
   Extra `-D` overrides apply to the gates (parameter warnings, interference, logo, wall thickness), not to the STL
   exports or renders, for trying a value before committing it.
3. A `ZEBCASE WARNING` echo fails the build with a FAIL line naming it. Read the `ZEBCASE NOTE` lines the build
   prints: they are reminders (the front-edge joints over the jack screw hardware), not failures.
4. Run `./enclosure/controls.sh` (about 50 s). It must end "controls: 0 failed". Recalibrate any control whose
   clearance moved, and the hard-coded positions listed above, as in the next section.
5. Look at the renders (`renders/*.png`) for ghost-to-ghost clashes, which no check sees.
6. Commit the SCAD, `out/*.stl` and `renders/*.png` together, and update `enclosure/README.md`.
7. Re-slice through `print/` (see the slicing-and-printing spec), print the coupons first, then the case.

### Evidence

On 2026-10-31 `build.sh` on the committed SCAD (OpenSCAD 2026.06.12, manifold backend) produced five STLs
byte-identical to the committed ones, and `controls.sh` passed 97 of 97. `check_lid` with the diode laid along X
(`diode_dir = [1, 0, 0]`, `diode_c = [9, 26.73, 13.3]`) found no overlap.

### Recalibrating a control

Each control moves one ghost by an offset chosen to sit just inside (want none) or just outside (want hit) one
clearance. When a module change moves that clearance, the control fails. For each failed line:

1. Find the clearance it tests: the comment above the `ctl` line names it, and the table below gives the parameter or
   `ZEBCASE` echo that holds its new value.
2. For a "hit" control, set the offset to the new clearance plus 0.1 mm. For a "none" control, set it to the new
   clearance less 0.05 mm (or half the clearance, if that is under 0.1 mm).
3. Re-run the control alone (`openscad -D 'part="check_lid"' -D ...` as in `ctl()`) and run
   `python3 enclosure/stlinfo.py --components` on the output to see where it hits; a hit must be on the feature the
   comment names, not elsewhere.
4. Update the control's comment with the new clearance, and the README's control table.

| Controls | Clearance | Where its value comes from |
|---|---|---|
| board fwd / back, plate deeper, rev 2 lid | `lip_gap` in front; 0 to the lid front wall; `bs_clr` to the backstop | parameters; echo "ZEBCASE LIPS" |
| holes X / Y | `mh_pin_clr` (X), `mh_pin_clr_y` (Y) | parameters; echo "ZEBCASE CORNER PINS" |
| board +X / -X | `pcb_clr` to the side walls and `mh_pin_clr` | parameters |
| leg keep-out X / Y | distance from the keep-out to the ledges (Y) and side wall (X) | echo "ZEBCASE leg-tail keep-out" (keep-out span and "ledges from Y") |
| pcb up | `press_gap` | parameter; echo "ZEBCASE CORNER PINS" ("pressers ... above the PCB") |
| xiao up / down / X / Y | `xpad_gap`, `usb_clr`, `tongue_gap`, `pcb_clr`, `shield_stop_clr` | parameters; echo "ZEBCASE SUPPORT XIAO" |
| corridor zone wider | gap from the corridor to the USB pad and shield stop block (0.3 mm in `xiao_pads()`) | echo "ZEBCASE SUPPORT XIAO" (corridor and stop block spans) |
| rear zones +X / -X | gap from the rear joint zones to the shield stop block (0.3 mm) and to the stop rib | echo "ZEBCASE PADS" (rear joints) against "ZEBCASE SUPPORT XIAO" (stop rib, shield stop) |
| diode -Y / +Y | gap from the diode keep-out to the shield stop block and the rear wall | echo "ZEBCASE DIODE" against "ZEBCASE SUPPORT XIAO" and "ZEBCASE DUPONT" (interior rear face) |
| dupont X / Y / up | gap from the housing and bend zone to the stop rib, block, rear wall and ceiling | echo "ZEBCASE DUPONT" |
| pin 9 wire | floor gap under the joint; the notch is exactly `wire_pass` wide | echo "ZEBCASE PIN 9 WIRE" |
| jack hardware | `jack_clr` above; the lid side wall beside it | parameters; echo "ZEBCASE JACK HARDWARE" |
| lid up | `lc_win_up` | parameter |
| logo | `logo_min` at `logo_w` | parameters |

The volumes `check_*` report for a hit are a second guide: a hit of a few mm3 where the README's table lists tens means
the control now only grazes the feature.
