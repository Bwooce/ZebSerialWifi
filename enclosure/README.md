# ZebSerialWifi enclosure

A two-part PETG case (tray + lid) for the HW-044 MAX3232 RS232-TTL board ("MAX3232 ESE 2512", female right-angle
DB9), with a Seeed XIAO ESP32-C6 lying across the DB9 body. The DB9 flange sits flush in the case front face, and only
the D-shell sticks out (6.0 mm), so the unit plugs straight into the printer. Two small lips in front of the flange
hold the flange and the loose shield. The two parts snap together, with no screws.

This is revision 5, after the revision 4 print: the XIAO is turned round so that its USB-C comes out of the other side
and the RX diode at its D3 pad sits beside the Dupont plug instead of against the lid front wall. See
[Revision 5](#revision-5-the-xiao-turned-round). Revision 4 added room for the pin 9 power wire, more room behind and
above the Dupont plug, and no front wall where the DB9's jack screw hardware sits.

In use the unit hangs upside down on the printer: the board's solder side and the case's tray floor face up, and the
logo is inlaid in the tray floor. The XIAO's USB-C comes out of the +X side, the printer's parallel-port side (nothing
is plugged in there).

| File | What |
|------|------|
| `zebserial_case.scad` | The model. All dimensions are top-level parameters, grouped as *measured*, *XIAO (Seeed KiCad/STEP)*, *DB9*, *corner pins*, *header and Dupont*, *print/tolerance*, *style*, *logo* and *clips and XIAO capture*. |
| `reference/` | Not in git (gitignored): the manufacturer drawings and the Seeed XIAO ESP32-C6 KiCad PCB and STEP the dimensions come from. |
| `out/tray.stl`, `out/lid.stl`, `out/logo.stl` | Print-ready, already in print orientation. `logo.stl` is the two-colour logo inlay, in the same coordinates as `tray.stl`. |
| `out/coupon_tray.stl`, `out/coupon_lid.stl` | Fit coupons: the whole tray with the middle of its floor cut out, and the lid below Z 18, single colour. |
| `renders/*.png` | Assembly, exploded, as-plugged-in, front, side, sections, the XIAO with the lid off, exterior style (`style_*.png`) and print-orientation views. |
| `build.sh` | Regenerates the STLs and renders, then fails on any `ZEBCASE WARNING` echo and runs the interference checks and the wall probe. Extra `-D` overrides apply to the checks, for example `./build.sh -D xpad_gap=0.3`. |
| `controls.sh` | Positive and negative controls for the interference checks (moves the ghosts and says what must and must not hit). Exits non-zero on any failure. |
| `stlinfo.py` | Reports STL volume and bounding box, and with `--components` the overlapping pieces. |
| `wallprobe.py` | Measures local wall thickness by ray casting from every face; `build.sh` lists everything under 1.2 mm. |

## Coordinates

The model frame is the board's frame, the same as before:

- **Y:** 0 is the DB9 flange front face, which is also the case front face (flush). +Y runs towards the rear (the
  header end). The connector's front plate keeps the full flange outline back to Y 5.1 (`fl_plate_d`), and its back
  face bears on the lid front wall. The RS232 PCB runs from Y 0 (flush with the flange front) to Y 29.
- **Z:** 0 is the RS232 PCB top (component) surface; +Z is towards the components and the lid, −Z the tray floor.
- **X:** seen from the component side (+Z) with the DB9 nearest you, +X is to the right.

**On the printer.** The board hangs upside down, so you look at the solder side and the tray floor. In the user's
photo (solder side up, DB9 plugged in at the left, the board running to the right), +Y points right and the view
is from −Z. Then right × up must be −Z (towards the camera), and Y × X = −Z, so up in the photo is **+X: the
parallel-port side**. Since revision 5 the USB-C exits at **+X**, at the top of the photo (revisions 3 and 4: −X, at
the bottom). The photo agrees independently: the header's four solder pads are a column along the photo's vertical at
the board's right (rear) edge, centred, and the two mounting holes are at the right-hand (rear) corners.
`renders/plugged_floor_logo.png` is drawn in exactly that view (`part="plugged"`: DB9 at the left, +X up).

In the code, `y_cf` (0) is the case front and `y_fb` (`fl_t`) the flange back.

## Print settings (Bambu H2D, 0.4 mm nozzle, PETG)

- **Layer height:** 0.16 mm, with a 0.2 mm first layer (`print/process_overrides.json`, on the 0.16 mm Balanced
  Quality profile).
- **Walls:** 4 (1.6 mm).
- **Infill:** 25 % gyroid. Most of the part is walls anyway.
- **Supports:** none.
- **Brim:** none needed. The parts are 40 × 32 mm.
- **Orientation:** the STLs are already oriented.
  - Tray: floor (and logo) on the bed, open side up.
  - Lid: top skin on the bed, open side up.
- **Overhangs:** a probe of the oriented STLs for down-facing faces steeper than 45° off the bed finds only these.
  All are fine in PETG; everything else is vertical, at 45°, or opens upwards in print orientation.
  - The tray's four catch-window tops (6.6 mm bridges).
  - The lid's snap-hook catch faces (0.6 mm).
  - Two 1.6 mm² bits of the USB relief's rounded corners beside the slot (now in the +X wall).
  - The two flange lips' 45° chamfers, at the threshold.
  - In the tray, the roof of the logo pocket 0.68 mm above the bed, which the inlay fills.
- **Logo (two colours):** load `tray.stl` and `logo.stl` together and accept "load as one object with multiple
  parts" (Bambu Studio), so they stay aligned; give the logo part the second filament. The inlay is the first four
  layers on the bed (0.2 + 3 × 0.16 = 0.68 mm), flush with the tray floor. For a single-colour print set
  `logo_mode = "deboss"` (0.68 mm deep) or `"none"` and rebuild.
- **Size:** 40.4 × 33.5 × 31.2 mm (X × Y × Z, the case shell from the front face at Y 0; the rear face is concave, 32.65 at
  mid-span). The two flange lips stand 1.3 mm proud of the front face over |X| ≤ 8, so 34.8 mm in Y over the lips, and the metal
  D-shell 6.0 mm. Revision 3 was 40.4 × 31.7 × 31.2; revision 4 is 1.75 mm longer in Y for the Dupont plug and the pin 9
  wire; revision 5 is the same size as revision 4. The top is 25.0 mm above the RS232 board, exactly the 25 mm limit
  (`max_above`): the Dupont wires' measured turning space sets it; see below.
- **Material:** 4.68 cm³ for the tray, 11.34 cm³ for the lid (revision 4: 10.99; the −X stop web) and 0.22 cm³ for
  the logo inlay, roughly 21 g of PETG at 100 %, less with infill. The fit coupons are 4.08 cm³ (tray) and 5.79 cm³
  (lid). The slicer figures are in `print/README.md`.

## Exterior style: faceted pincushion

The outside copies the look of the user's factory Radar360 case. The look is defined in
`~/personal_scripts/ai-config/instructions/enclosure_styles.md`. `renders/style_plan.png`, `style_iso.png` and
`style_side.png` show it. I compared them with the reference STLs, rendered from the same cameras. On the
reference, the "triangular corner facets" are corner truncations rather than tall vertical chamfers, so the model
follows that.

| Feature | Parameter | Value |
|---------|-----------|-------|
| Side and rear faces bow inwards at mid-span (2 % of each face's length) | `sag_frac` | 0.02: 0.67 mm on the sides, 0.81 mm on the rear |
| Each face is three flat facets: a flat middle third, sloping to the corners | `facet_flat` | 0.3 (0 gives a two-facet V) |
| Each of the 8 corners cut off by a flat plane square to the body diagonal, leaving a triangular facet in plan, side and front views | `corner_facet` | 5.0 mm along each edge (12.4 % of the width) |
| Small 45° bevel on the vertical corner edges between the facets | `corner_chamfer` | 1.5 mm |
| Top and bottom perimeter edges chamfered at 45°, so the side profile is an elongated octagon | `edge_chamfer` | 1.5 mm |
| Shadow-line groove on the lid just above the seam, interrupted at the snap fingers | `seam_reveal`, `reveal_d`, `reveal_h` | on, 0.45 deep × 0.5 tall |

The tray and the lid are both cut from one `outer_solid()`, so the seam lines up exactly.

Deliberate departures from the reference:

- **Flat top and bottom.** The reference's top and bottom are concave too. Here they are flat:
  - The tray prints floor down and carries the logo inlay; it is the face that is up on the printer.
  - The lid prints top down, plain.
  - A flat bed face prints cleanly and needs no support.
- **Flat front face** (`front_sag = 0`): the flange sits flush in it, and a concave front would leave a visible gap
  around the flange. The flange covers most of that face anyway.
- **Corner facets of 5 mm**, slightly smaller than the reference's ~15 %. The limit is the front bottom corners. There,
  the floor, the front skin and the side wall meet beside the open DB9 face, and a 5 mm facet leaves 1.27 mm square to
  the facet.

What the style changed underneath:

- **Wall thickness:**
  - Side walls are 4.0 mm at the corner ends (`side_t`), so that the deepest point of the bow keeps about 3.4 mm. On
    the +X side that also gives the USB relief its depth (2.03 mm at its shallowest).
  - The rear wall is 1.65 mm at mid-span (`min_wall` + `reveal_d`), so that 1.2 mm is left behind the shadow-line
    groove.
- **Height:** the lid top is set so the 45° top chamfer leaves 1.2 mm over the USB overmould relief. That alone would
  put it 19.95 mm above the board; the Dupont wires' turning space (ceiling at least 23.4 mm above the PCB top) raises
  it to 25.0 mm. The top skin over the XIAO stays a single 1.6 mm `ceil_t`, so the antenna has only a thin skin over it.
- **Snap fingers:** every snap dimension is measured from `lc_face()`, the lowest point of the bowed side face over that
  clip.
  - Fingers are never thinner than 1.4 mm, and the wall behind each pocket is never thinner than 1.7 mm.
  - The rear fingers sit on a sloping facet, so they taper slightly across their width.
- **Snap catches:** these are through-windows in the tray wall instead of 0.85 mm blind recesses, because a blind
  recess would leave under 1.2 mm behind it.

## How it works

### Revision 5: the XIAO turned round

The user printed revision 4: "case almost fits, but the diode stops it. if we put the usb port on the other side it
should work." The RX diode is soldered right at the XIAO's D3 pad. In revisions 3 and 4 D3 was on the XIAO's front edge,
0.2 mm behind the lid front wall and over the DB9 body, so the diode had nowhere to go. What changed:

- **The XIAO is turned 180° about Z.** Its USB-C comes out of the +X wall (the printer's parallel-port side; nothing is
  plugged in there) instead of the −X wall. It lies where it did, on the DB9 body, 0.2 mm from the +X inner wall and
  0.2 mm behind the lid front wall: X −4.955..16.0, Y 7.3..25.08. The pad columns swap edges:
  - rear edge, towards the Dupont plug: D0..D6, so D3 (X 5.46) and D5 (X 0.38);
  - front edge, towards the DB9: 5V (X 13.08), GND (X 10.54), 3V3 (X 8.0), D10..D7.
  - Pads still count 1..7 from the USB end, which is now the +X end.
- **USB-C:** the slot, the tongue and cradle, and the overmould relief move to the +X wall, unchanged in shape. The
  relief is 2.03 mm deep at its shallowest, as before. The snap fingers are mirrored on both walls and were already
  placed to clear the slot, so they did not move; the −X wall now has the fingers only.
- **RX diode:** a keep-out for a DO-41 (1N5817/1N5819, the worse case) standing up beside the Dupont housing's +X end,
  behind the XIAO rear edge, with its anode lead bent to the D3 pad and its cathode lead up at the housing top, where
  the RXD wire joins it. See [The RX diode](#the-rx-diode).
- **5V/GND corridor:** the module's VCC and GND wires and the pin 9 wire leave the housing top, run +X over the housing
  and forward over the XIAO's USB-C shell to the 5V/GND pads on the front edge. The corridor is 5.0 mm wide (X 9.31 to
  14.31), between the shield pad ring and the USB shell pad. It replaces the D3/D5 channel over the shield.
- **D3/D5:** the TXD wire comes forward over the housing front face and down onto D5; the diode's anode lead reaches D3.
  Both joints are on the rear edge strip, 1.15 mm in front of the housing, with room under them too.
- **Capture:** the end stop rib moves to the −X (antenna) end. The antenna is now on the front side, so the rib cannot
  run forward into the front wall as before; it runs from just behind the antenna back to the rear edge, and a web at
  its rear end ties it to the −X side wall. The shield pad ring is whole again. The shield stop block sits between the
  D3 joint zone and the corridor. The USB pad covers the shell from the corridor to the +X wall.
- **Front wall:** one relief for the 5V, GND and 3V3 joints (X 5.93..15.15). Behind it the 0.3 mm jack screw lead-in on
  the wall's bottom front edge is left out: with the relief the wall is 1.2 mm deep, and the lead-in left 0.9 mm there
  (the wall probe found it).
- **Pin 9 wire:** the run under the board and the notch in the backstop are as before. The riser now goes up to the
  ceiling, then +X over the Dupont housing, behind the module wires, to the corridor and the 5V pad.
- **Checks:**
  - `box()` now asserts that its bounds are in order. An inverted box renders as nothing, silently, and revision 4
    lost one of the XIAO's two button ghosts that way.
  - `build.sh` and `controls.sh` fail on OpenSCAD `WARNING` lines as well as errors.
  - There are 97 controls (revision 4: 85).
- **Renders:** section cut faces of the case took the colour of the last ghost drawn, because coplanar cut faces merge
  in OpenSCAD 2026's manifold renderer. Ghosts are now cut 0.02 mm behind the case's cut face, so the case keeps its
  colour. `show_zones = false` hides the magenta keep-outs in a view and `show_lid = false` hides the lid; the checks
  always include the keep-outs.

### Revision 4: after the first print

The user printed revision 3 and fitted both boards. Both fit well, and how the boards are located and held is
unchanged. Four problems, in the user's words, and what changed:

- **"There's no space to route the 5v wire from under the pcb to the xiao."** The bridge is now powered from the
  printer through a wire from the DB9 pin 9 pad (solder side) to the XIAO's 5V pad. It has a path of its own, 2.0 mm
  wide for a 1.4 mm jumper wire, under the board to the rear edge, through a notch behind the PCB and up to the 5V/GND
  wires. See [The pin 9 power wire](#the-pin-9-power-wire).
- **"The back wall is too close to the dupont pins."** The interior rear face above the PCB moved back 1.75 mm, from
  0.48 to 2.23 mm behind the Dupont housing (`dupont_rear_clr` 1.5 for the plug, plus 0.25 so the pin 9 wire passes
  behind the PCB). Below the PCB top a backstop ledge still stands 0.25 behind the PCB rear edge (`bs_clr`), so the
  board is located exactly as before. The case is 1.75 mm longer in Y.
- **"There isn't enough space to route the wires from the dupont connector back to the xiao."** The ceiling is already
  at the 25 mm limit, so the room comes from the rear and from X. The wire-bend zone is 14.2 × 5.2 mm (was 10.2 ×
  3.7), the 5V/GND zone runs 1.5 mm further back, and the D3/D5 channel is 6.0 mm wide (was 5.0). See
  [Wires and the Dupont plug](#wires-and-the-dupont-plug).
- **"The wings behind the RS232 shell don't permit the screws."** The wings were the lid front wall either side of the
  DB9 body: Y 5.1–7.1, |X| 8.8–16.2, from the split (z 4) up. They were the plate's bearing face beside the body, and the
  jack screw tails and nuts behind the flange holes (X ±12.5, z 6.25) sat in them. They are cut back to z 10.05 over a
  keep-out for the hardware. The plate now bears on the front wall above it and across the DB9 body top. See
  [DB9 and the front face](#db9-and-the-front-face).

### Revision 3: the HW-044 board

Revisions 1 and 2 were for a YL-97 module (31.5 × 26 PCB, male DB9, header on the −X edge, XIAO lying lengthwise on
the DB9 body with its USB-C out of the rear). The user now uses an HW-044. What changed:

- **Board:** 32 × 29, female DB9, assumed to have the YL-97 DB9's flange, shell and body dimensions and heights.
- **Retention:** two tray pins through the board's rear-corner mounting holes, and lid pressers over them, replace
  the side pressers and the backstop rib. The leg-tail pockets are off; a keep-out replaces them.
- **XIAO:** turned 90°, its length along X with the USB-C out of the −X wall, as far forward as the lid front wall
  allows.
- **Header:** at the rear centre, pins along X. The Dupont housing and its wire-bend zone sit behind the XIAO.
- **Logo:** moved from the lid top to the tray floor, the face that is up on the printer. The lid top is plain.
- **Snaps:** front pair at Y 6.3 and rear pair at Y 25.65 (was 7 and 27.5), so that their lid slots stay at least
  1.2 mm clear of the USB slot in the −X wall.
- **Lid front wall:** 2.0 mm deep (was 3.2), so the XIAO fits between it and the Dupont housing.

What revision 2 fixed after the first print is still in: the flush flange held by lips and the lid front wall, no
flexing fingers inside the case, the rounded USB opening, and pressers rooted in the lid walls. Revision 2 found that
the YL-97's leg-tail pockets had been modelled at the wrong Y and the ghosts shared the error, so the check could not
see it. The HW-044's leg tails are only estimated, so this revision keeps the tray clear of them instead of locking
onto them.

### DB9 and the front face

- **Assumed (YL-97 measurements, applied to the HW-044's female DB9 as the user said):**
  - Flange 30.8 × 12.5 × 0.38 mm steel, bottom level with the PCB top (`fl_bot = 0`), so the flange and D-shell
    centre are 6.25 mm up. D-shell 16.9 × 8.4, projecting 6.0 mm, so its outline runs from z 2.05 to 10.45.
  - Rear body 17.2 wide × 10.4 high, ending 12.5 mm behind the flange front (`body_end`).
  - Each side of the body, a plastic lug 3.4 high runs out to the full PCB width and back to Y 12.0 (`lug_end`).
  - The PCB front edge is flush with the flange front face, so the PCB runs under the flange.
  - The front plate keeps the full flange outline back to Y 5.1 (`fl_plate_d`).
- **Flush front.** The case front face is at Y 0, flush with the flange front face. The flange sits in the front
  opening between the side walls (0.8 mm each side; the board is 0.5 mm wider than the YL-97). Above the flange (from
  z 12.8, `fl_clr_z` 0.3 above it) the lid front face fills forward to Y 0. Below the flange the PCB front edge is
  flush with the tray front skin and floor.
- **Front opening and rear bearing face.** The lid is open across the full interior width from the rim up to z 12.8
  and back to Y 5.1. Its front wall sits behind the plate, Y 5.1–7.1 (`front_wall_t` 2.0, was 3.2), and that face is
  what the plate back bears on, with no gap.
  - **Revision 4:** the wall beside the DB9 body (|X| 8.8–16.2, the "wings") stops at z 10.05, over the jack screw
    hardware (below). The plate bears on it above that, beside the body, and across the DB9 body top: 50.5 mm² in
    all in revision 4 (revision 3: 139 mm², down to the split), about 53 mm² in revision 5, where the lead-in is left
    out behind the 5V/GND relief. The front wall bears on the plate back only, so this carries the push as the unit
    goes onto the printer, which is well under what PETG takes on 50 mm².
  - The new bottom front edges (z 10.05 beside the body, z 10.6 over it) have 0.3 mm 45° lead-ins (`jack_lead`), so
    a board sitting slightly back is pushed forward onto the lips as the lid closes. Since revision 5 they stop at the
    5V/GND joint relief (X 5.93..15.15), where the wall is only 1.2 mm deep. The old 0.5 mm lead-in at the split
    (`fw_lead`) now has no wall left to cut, and 0.5 here left 1.17 mm where the wall is only 1.6 deep behind the XIAO
    lead-in.
  - `renders/lid_front.png` (lid alone, from the front), `section_plate.png` (X 12) and `section_jack.png` show it.
- **Jack screw hardware (revision 4).** The user fits the DB9's jack screws or hex posts. Behind each flange hole a
  gold keep-out stands for the screw tail and nut: 7.0 mm across (`jack_d`, an M3 or 4-40 nut is about 6 across the
  flats), from the plate back at Y 5.1 (`jack_y0`; the measured 5.1 is "the back of the flange plate screw holes") 5.0
  mm back (`jack_l`), centred on X ±12.5, z 6.25.
  - It is part of the DB9 ghost, so `check_lid`, `check_tray` and `check_lid_path` all test it.
  - The front wall is cut back 0.3 above it (`jack_clr`), from the DB9 body to the side walls. A cut shaped to the
    cylinder would leave 0.2 mm slivers beside the body and the side wall.
  - The lid side walls are 0.2 mm outside it. The XIAO's +X (USB) end overhangs it 0.65 mm above (underside z 10.4,
    keep-out top 9.75), and since revision 5 the 5V and GND pads on the XIAO's front edge are right over it, so their
    joints must be on the top side only.
  - In front of the flange the hex posts are clear: the lips span only |X| ≤ 8, and nothing else stands in front of
    Y 0.
- **Retention lips** (`lip_*`), unchanged. Two lips stand in front of the flange:
  - **Top lip** (lid): over the flange's top margin, from z 11.5 to the flange top, then up the front face.
  - **Bottom lip** (tray): over the PCB front edge and the flange's bottom margin, up to z 1.0.
  - Both span |X| ≤ 8 (`lip_x`), stand 1.3 mm proud of the front face (`lip_proud`) with a 0.1 mm gap to the flange
    (`lip_gap`), so they are 1.2 mm of plastic, and reach 1.0 mm over the flange (`lip_reach`). Each has a 0.3 mm
    lead-in on its back edge (`lip_lead`).
  - They stay 1.05 mm clear of the D-shell outline (the model warns below `lip_ds_clr` = 1.0) and well clear of the
    flange holes at X ±12.5, where the printer's jackposts come within about 1 mm of our flange at full mating.
  - With the lid front wall behind, the lips fix the flange (and with it the board and the loose shield) fore-aft with
    0.1 mm of play.
  - The lips hold the shield's flange over the middle 16 mm at top and bottom; the screw holes stay free.
- **Leg tails (not measured):** the HW-044 has board-lock leg tails too: two slotted pads about 8.6 mm behind the PCB
  front edge and about 25.3 mm apart, estimated from the photo only. The model assumes the YL-97's tails (round, 3.1
  across, 3.0 below the PCB, so their tips reach the floor top).
  - `leg_pocket_on = false`: no pockets, since the position is only estimated.
  - An orange keep-out ghost covers each tail ±1.5 mm (`leg_keepout`) in X and Y: X ±9.6..15.7, Y 5.55..11.65, from
    the PCB underside to the floor. It is part of `ghosts()`, so `check_tray` fails if any tray feature enters it.
  - The side ledges start 0.5 mm behind it (Y 12.15), which covers the board's fore-aft play. The side wall is 0.5 mm
    outside it. Nothing else of the tray is near.
  - `renders/section_leg_keepout.png` (Y 7.6–9.6, red pin zone hidden) shows it.
- **Not measured:** whether there is a step behind the flange. None was mentioned, so `step_on = false`.
- **Renders:** `front_flush.png`, `front_elevation.png`, `section_lips.png` (X 0 plane through both lips).

### RS232 board

- **Board:** 32.0 × 29.0 mm (measured), 1.6 mm thick (assumed as for the YL-97). Two mounting holes, 2.4 mm, at the
  rear corners, 26.5 mm apart and 2.0 mm in from the rear edge: X ±13.25, Y 27.0.
- **Support from below:** the board sits on a 1.2 mm front skin under its front edge, on 1.2 mm side ledges from Y
  12.15, and on the two corner-pin bosses (4.4 mm, under the solder side around each hole). A 0.4 mm gusset fills the
  corner between the floor and the front skin, keeping 1.2 mm square to the bottom front chamfer.
- **Corner pins** (`mh_*`), from the tray bosses up through the holes:
  - 2.0 mm across X (0.2 clearance per side). The tray side walls are also 0.2 from the PCB edges, so walls and
    pins both locate the board in X. What the pins add is a location that does not depend on the PCB's edge width,
    and they keep the board from walking sideways on the ledges before the lid is on.
  - In Y they are lens-shaped: the pin is the part of the 2.0 circle that stays inside the hole when the hole moves
    0.45 mm either way in Y (`mh_pin_clr_y`). So they never fight the flange datum (lips 0.1 in front, lid front wall
    at zero gap behind), and a hole up to 0.45 mm off its nominal Y still drops on.
  - The top is 0.1 below the PCB top, with a 0.4 mm 45° lead-in, so 1.1 mm of straight pin engages the hole. A pin
    standing proud of the board would need a bore in the presser that breaks out of its front face (that version
    left 0.0–0.8 mm slivers, which the wall probe caught).
- **Backstop.** A ledge across the tray rear wall stands 0.25 mm behind the PCB rear edge (`bs_clr`), up to 0.2 mm
  below the PCB top (`bs_top`). It stops the board going back before the lid is on, and never fights the flange
  datum. The pins could not do that job: their Y clearance is deliberately larger. In revision 3 the rear wall itself
  was the backstop; in revision 4 the wall above the board moved 1.75 mm back (for the Dupont plug and the pin 9
  wire), and the ledge keeps the old face. It has a 2.0 mm notch at X −9..−7 for the pin 9 wire.
- **Corner pressers** (`cp_*`): each is a block from the lid side and rear walls down to 0.25 mm above the PCB top
  (`press_gap`), over the corner around the hole: X from 2.0 mm inside the hole centre to the PCB edge, Y from 0.8 mm
  in front of the hole centre (Y 26.2) back to the rear wall (Y 31.0).
  - The front face stops at Y 26.2. In revisions 3 and 4 that kept it behind the XIAO's rear-edge 5V/GND joints above
    the −X presser; since revision 5 the rear-edge joints are D3 and D5, near the middle, so nothing near the pressers
    sets it any more and it was left where it was.
  - Below the split they stand 0.2 off the tray walls. Above it they are rooted in the lid walls with 45° tops, and
    they end below z 9, under the XIAO.
- **Side pressers removed.** The YL-97's edge pressers were placed around that board's parts. The HW-044's
  component side is not measured, so the board is held at four points only: the two lips at the front and the two
  corner pressers at the rear.
- **Header ledge relief removed:** the header is no longer at a side edge.
- **Clear space:** there is 3 mm clear below the board. The red ghost volume shows the through-hole pin zone, kept
  1.25 mm in from the board edges and cut back around the pin bosses (plus the board's play), which assumes no pin
  within about 2.4 mm of a hole centre.
- **Renders:** `section_pins.png` (Y 27 plane, both pins, bosses and pressers), `section_pin_x.png` (X 13.25 close-up).

### XIAO

The XIAO dimensions come from Seeed's own XIAO ESP32-C6 v1.0 KiCad PCB, plus the user's measurements:

- outline 20.955 × 17.78 mm (measured 21.0), 22.5 mm including the USB-C (`usb_over` 1.5)
- PCB 1.2 mm thick (measured)
- USB-C 3.3 mm above the PCB top (4.5 overall); shield 2.2 mm above it (3.4 overall)
- pad 1 at 2.921 mm from the USB end, 2.54 mm pitch
- shield 12.6 × 10.6 mm at d 6.71–17.31
- ceramic antenna at w +0.5..+5.7 and U.FL at w −5.6..−3.0, both at the antenna end
- buttons at w ±5.3..7.9, d 0.3–1.9

(d = distance from the USB end; w = across the width, + towards the 5V/GND column.)

**Placement.** The XIAO lies on the DB9 body, its underside at z 10.4 as before, its length along X. Since revision 5
the USB end is at +X (revisions 3 and 4: −X).

- **X:** X −4.955 to +16.0. The USB end is 0.2 mm from the +X inner wall, so the receptacle mouth (X 17.5) is 1.3 mm
  into the wall and the relief, slot and tongue work as before, mirrored.
  - Centred in X (X ±10.48), the mouth would be 4.2 mm inside the wall. The relief would then be a pocket about 8 mm
    deep, too deep for most plug overmoulds, and the tongue could not reach the shell.
- **Y:** Y 7.3 to 25.08, 0.2 mm behind the lid front wall, the furthest forward it can go. The flange plate (to Y
  5.1, up to z 12.5) and the front wall behind it (to Y 7.1) are in front of it. Its rear edge is 1.15 mm in front of
  the Dupont housing (the model asserts at least 0.5, `xiao_hdr_min`).
- It lies 5.2 mm deep on the DB9 body (Y 7.3 to 12.5) over X −4.955 to +8.6 (the model warns below 2.0,
  `front_rest_min`).
- **Pad columns:** the XIAO cannot be mirrored, so with the USB at +X the D0..D6 column is on the rear edge (towards
  the header) and the 5V..D7 column on the front edge (over the DB9 body and the +X jack screw hardware).
  - The firmware uses D3 and D5 (`board.c`: GPIO 21 and 23; RX goes through a Schottky diode), plus 5V and GND.
  - So D3 (X 5.46) and D5 (X 0.38) are on the rear edge, and 5V (X 13.08), GND (X 10.54) and 3V3 (X 8.0, for the
    optional pull-up) on the front edge.
  - The front pads lie over the DB9 body's +X end (3V3) and 0.65 mm above the +X jack screw hardware (5V, GND), so
    their joints must be on the top side only. The model echoes this as a reminder (`ZEBCASE NOTE (check)`).
  - The rear pads overhang the RS232 board, so D3 and D5 can be soldered from either side.

**Capture: all rigid, no flexing parts** (as in revision 4, turned round):

- **Below, front:** the top of the DB9 rear body (5.2 mm of the PCB lies on it).
- **Below, +X end:** the tray tongue under the overhanging USB-C shell, in the +X wall. Its top is a cradle shaped to
  the shell's stadium section, 0.05 mm under the shell bottom (`tongue_gap`) and 0.35 mm off the rounded ends.
- **Above:** rigid pads from the lid, 0.25 mm (`xpad_gap`) above:
  - **RF shield top:** a ring over the shield outline (inset 0.5), whole again (revisions 3 and 4 cut it for the D3/D5
    channel): X −0.81..8.79, Y 10.39..21.99.
  - **USB-C shell's flat top:** from the 5V/GND corridor (X 14.61) to the +X wall. This is the support above the USB-C
    port, so a plug pushed in at an angle bears on the case; the slot top, 0.2 mm above the shell, does the same in
    the wall.
  - **Rear edge strip:** the bottom of the shield stop block (below), X 7.53..9.01.
  - The lid's USB slot, 0.2 mm above the shell, is the first thing a rising XIAO meets: up 0.2 is clear, up 0.3
    hits.
- **+X:** the lid's +X wall, 0.2 mm from the USB-end PCB edge beside the USB slot. Plug extraction bears here.
- **−X:** the stop rib, 1.6 mm thick, 0.2 mm from the antenna-end PCB edge (X −6.755..−5.155). It takes the plug
  insertion force (6 to 20 N, pushing the XIAO towards −X).
  - It runs from 0.27 mm behind the ceramic antenna (Y 15.94; the antenna is now on the front side, Y 10.47..15.67)
    back to the XIAO rear edge (Y 25.08), past the U.FL socket (Y 19.19..21.79). Revision 4's rib ran forward into the
    front wall, but here that would put plastic right beside the antenna.
  - A web 1.6 mm thick (`stop_web_t`) at its rear end, Y 23.48..25.08, runs out to the −X side wall. With the ceiling
    it holds the rib. A 1.6 mm fin hanging from the ceiling alone would bend about 2 mm under 20 N (a cantilever
    estimate for PETG, not measured).
  - The rib and web reach 0.6 mm below the XIAO underside (z 9.8). Nothing is under them there: the −X jack screw
    hardware ends at Y 10.1, the −X corner presser starts at Y 26.2.
  - A 0.4 mm 45° lead-in on the rib's bottom inner edge (`stop_lead`) pushes a XIAO that sits too far −X back as the
    lid closes.
- **Front (−Y):** the lid front wall, 0.2 mm in front of the front PCB edge, plus the USB slot sides (0.2 mm from the
  shell).
  - Below the XIAO underside the wall's rear face is set back 0.4 mm, with a 45° lead-in up to z 10.8 (`xlead`), so a
    XIAO up to 0.4 mm too far forward is pushed back as the lid closes.
  - The wall is relieved 0.8 mm deep over X 5.93..15.15 (`fw_relief`) for the 5V, GND and 3V3 joints, leaving 1.2 mm
    of wall. The XIAO bears on the wall over X −4.955..5.93 and beside the USB slot.
- **Rear (+Y):**
  - At the +X end, the USB slot sides (0.2 mm).
  - Behind the shield, the shield stop block (`shield_stop_clr` 0.3) at X 7.53..9.01, between the D3 joint zone and
    the corridor, with a 0.4 mm 45° lead-in on its bottom front edge. Above the shield it joins the shield pad ring.
    It bears on the shield over 1.48 mm, less than revision 4's block (2.2 mm).
  - There is no room for a guide behind the rear edge: the Dupont housing is 1.15 mm away and the D3/D5 joints are
    there. The shield is assumed centred across the XIAO's width (Seeed data).
- **Renders:**
  - `section_plan_usb.png` and `section_plan_usb_case_only.png`: a plan section at the USB-C axis, z 13.25.
  - `section_plan_stop.png`: plan, z 9..14, the −X stop rib and web.
  - `section_usb.png`: Y 16.2, through the USB axis (tongue, slot, relief, USB pad, corridor, shield pad, stop rib).
  - `section_x0.png`: X 0, across the XIAO (body, front wall, D5 wire, Dupont housing).
  - `section_shield_stop.png`: X 7.6..9.0, the stop block joined to the ring, and the 3V3 relief.
  - `xiao_lid_off.png`: the XIAO, wires and diode with the lid off.

**Antenna.** The antenna end now points at −X, 11.2 mm from the −X wall, and the ceramic antenna is on the XIAO's
front side (X −4.8..−2.8, Y 10.47..15.67). Over it there is only the 1.6 mm lid skin (`ceil_t`). The nearest case
feature is the stop rib, 0.36 mm off its end in X and starting 0.27 mm behind it in Y. The antenna's front 2 mm are now
over the DB9 rear body (to Y 12.5), where revision 4 had them over open space; the DB9's metal is under the front of
the XIAO as before.

### USB-C opening

- **Side wall:** the opening is in the +X side wall (revisions 3 and 4: −X), centred on the USB axis (Y 16.19, z 13.25).
- **Plug relief:** 12 × 7 mm (`usb_recess_w`, `usb_recess_h`, along Y and Z), corner radii 2.5 mm
  (`usb_recess_r`). Its floor is flush with the receptacle mouth (X 17.5), so a fully seated plug bears on the case.
  - Its depth is set by the bowed side face: 2.03 mm at its shallowest. Revision 2 had 1.6 mm. The model asserts at
    least `usb_recess_d` (1.6).
  - **The plug limit is the same as before:** an overmould larger than 12 × 7 mm will not seat fully. Increase
    `usb_recess_w`/`usb_recess_h` if needed. The large right-angle plug in the user's photo is probably larger.
- **Receptacle slot:** the slot through the wall follows the USB-C shell's stadium section (r 1.65 + 0.2 mm
  clearance) at the top, so its visible ends are round. It stays open down to the lid rim so the closing lid passes
  over the receptacle; the tray tongue fills it from below, and its cradle rounds the lower corners. The tongue's
  edges beside the cradle rise to z 11.8 and are at least 0.5 mm wide (`tongue_tip`).
- Behind the relief floor the tongue is cut square (its rounded corners would otherwise leave knife-edge wedges on the
  tongue).
- **Renders:** `side_usb.png` (with the XIAO) and `side_usb_case_only.png`, both from +X.

### Wires and the Dupont plug

A Dupont housing is plugged onto the HW-044's 4-pin header (straight pins on the component side, in a row along X,
centred on the rear edge). The model carries it as a keep-out (grey/cyan ghost, `ghost_dupont()`), and the
interference checks fail if any case feature enters it.

- **Housing:** 10.16 × 2.54 mm on the header, centred at X 0 and at Y 27.5: the pin row is assumed to be 1.5 mm in
  from the rear edge (`hdr_row_in`, an estimate from the photo). It reaches up to `dupont_top` = 16.7 mm above the PCB
  top (measured: 18.3 mm above the PCB underside).
- **Room behind the housing (revision 4):** the keep-out runs 1.5 mm behind the housing (`dupont_rear_clr`), to Y
  30.27, from the PCB top to the housing top. The rear wall is at Y 31.0: 2.23 mm behind the housing (revision 3:
  0.48).
- **Wire-bend zone:** above the housing up to the ceiling, 2.0 mm wider than the housing on each side (`bend_side`),
  and from the XIAO's rear edge back to Y 30.27: 14.2 × 5.2 mm and 6.7 mm high. Since revision 5 the wire zones extend
  it from X −9 (over the pin 9 riser) to X 14.31 (over the corridor), so it is 23.3 mm wide.
  The ceiling is at least `dupont_ceiling_min` = 23.4 mm above the PCB top (the user's 25 mm of turning space from
  the PCB underside) and at least `dupont_bend_h` = 4.0 above the housing. With the 1.6 mm top skin that puts the
  outside top at 25.0 mm, exactly `max_above`; there is no margin left in the envelope, and the model asserts it.

The wires leave the top of the housing. Since revision 5 the XIAO's used pads are D3 and D5 on the rear edge, towards
the housing, and 5V and GND on the front edge. The magenta zones are keep-outs, and the interference checks fail if any
case feature enters them:

- **5V/GND corridor:** X 9.31 to 14.31 (`wch_hw` 2.5 either side of the middle of the 5V and GND pads), from the XIAO's
  front edge back to Y 30.27, from the XIAO top to the ceiling. It runs over the XIAO between the shield pad ring and
  the USB shell pad: over the gap between the shield and the USB-C shell and over the shell itself. The module's VCC
  and GND wires and the pin 9 wire run +X along the bend zone and forward along the corridor, 1.6 mm above the
  USB-C shell; three 1.4 mm wires abreast take 4.2 mm of its 5.0. In front of the shell they come down onto the pads.
- **Front-edge joints:** over and in front of the 5V, GND and 3V3 pads, X 6.23..14.85, 0.9 mm in front of the edge
  (in the front wall's relief) and back to the shield, from the XIAO underside to 3 mm above its top. The 5V pad takes
  two wires (module VCC and pin 9).
- **Rear-edge zone:** over the rear edge strip from the D5 joint to the D3 joint (X −1.39..7.23, Y 22.49..25.08), from
  the XIAO top to the ceiling. The TXD wire comes forward over the housing's front top edge and down onto D5. Joint
  zones round D3 and D5 run from 0.9 mm behind the edge to 2.6 mm in front of it, from 0.3 mm above the RS232 board to
  3 mm above the XIAO top, because the rear edge overhangs the RS232 board.
- **The RX diode:** its own keep-out; see below.
- **Pull-up (optional):** the user's notes call for 10 kΩ from D3 to 3V3 with a 1N5817/1N5819. 3V3 is on the front
  edge (X 8.0), with a joint zone and front wall relief (`pullup_pad`). A resistor from the D3 end can follow the
  module wires: up through the rear-edge zone, +X in the bend zone, forward along the corridor and down onto 3V3. This
  room is a side effect and did not drive the design.

The green ghost wires show one feasible route, and the interference check confirms that no case feature crosses it
(`section_plan_wires_only.png`, `xiao_lid_off.png`). Which header pin carries which signal is not modelled; the wires
can cross in the bend zone.

- **Moved 1 mm** (`ghost_dupont_d`, which moves the bend zone's extension too): ±X and down stay clear. −Y meets the
  −X stop rib and web and the shield stop block (the bend zone runs up to their rear faces), +Y meets the rear wall (the keep-out ends 0.73 mm from it; +Y 0.5 stays clear), and up meets
  the ceiling (the zone runs to it by definition).

### The RX diode

The RX line has a Schottky diode at the XIAO: anode on the D3 pad, cathode (band) to the module's RXD wire. Its size
and how it is bent are not measured, so the keep-out assumes the worse case, a DO-41 1N5817/1N5819: body 2.7 mm across
and 5.2 mm long (`diode_d`, `diode_l`), leads 0.8 mm (`diode_lead_d`). A BAT54 or a smaller axial diode fits in the
same room.

- **Default placement** (`diode_c`, `diode_dir`, all top-level parameters): standing up beside the Dupont housing's
  +X end, behind the XIAO rear edge. Body centre X 6.73, Y 26.73, z 15.1, so the body runs X 5.38..8.08, Y
  25.38..28.08, z 12.5..17.7, 0.3 mm from the housing's +X face. Its anode lead leaves the bottom, 0.6 mm straight
  (`diode_lead_out`), then bends forward and down onto the D3 pad (X 5.46, Y 24.48, on the XIAO top). Its cathode
  lead leaves the top, up to z 18.3, at the housing top, where the RXD wire from the housing joins it.
- **Keep-out:** the body grown by 0.3 mm (`diode_clr`) all round, both leads grown by 0.3 mm, and the D3 joint zone:
  X 5.08..8.38, Y 25.08..28.38, z 12.2..18.0 for the body, the cathode lead to z 19.0. It is part of the wire zones, so
  `check_lid` and `check_tray` enforce it, and the diode itself is swept in `check_lid_path` (a lid block under the
  body, `ctl_diode_undercut`, is the positive control).
- **Nearest case features:** the shield stop block's rear corner (X 7.53, Y 25.08) is 0.18 mm from the keep-out and
  0.48 mm from the body. The rear wall is 2.6 mm behind the keep-out, and the ceiling 4.4 mm above it. The
  corridor wires run over it at z 19.9 and up.
- **Other placements:** `diode_dir` turns the body and `diode_c` moves it (the derived default centre only suits
  `[0, 0, 1]`, so give `diode_c` with any other direction). The checks then say whether the diode clears the case, but
  not whether it clears the Dupont housing, the header or the XIAO: ghosts are never checked against each other.
  Lying along X behind the XIAO's rear edge (`diode_dir = [1, 0, 0]`, `diode_c = [9, 26.73, 13.3]`) clears the case,
  under the corridor wires; lying back along Y beside the housing does not fit (it hits the rear wall).
- `section_diode.png` (Y 24..27, from the front, keep-outs hidden), `section_diode_zones.png` (the same with them) and
  `diode_iso.png` (lid off) show it.

### The pin 9 power wire

The bridge takes its 5 V from the printer: a wire from the DB9 pin 9 pad on the solder side (−Z, facing the tray
floor) to the XIAO's 5V pad (front edge, X 13.08). See the top-level README for the wiring.

- **Pin 9 pad:** X −4.155, the −X end of the 4-pin row (DE-9 pitch 2.77, the connector centred on X 0; in the
  annotated photo, pin 9 is at the bottom of the 4-pin row, towards −X). Y 7.1 (`p9_y`) is an estimate from the photo:
  the 4-pin row is about 1.5 mm in front of the leg tails.
- **Which edge.** The front edge is under the flange. The −X edge is the nearest free one, but it does not work: the
  PCB is 0.2 mm from the side wall there, so the wire would need a groove in the wall, which carries the snap windows
  and finger slots. Under the board the leg tail is in the way too. Running +X under the board would cross the other
  DB9 pin joints. The rear edge needs only a notch in the backstop, and the revision 4 rear room gives it space behind
  the PCB.
- **The path** (magenta keep-out, all 2.0 mm wide, `wire_pass`; green ghost wire):
  1. **Joint:** 3 × 3 mm around the pad (`p9_joint`), 2.5 mm below the PCB underside (`p9_joint_h`): pin tail,
     solder and wire end. It ends 0.5 above the floor.
  2. **Run under the board:** from the pad −X to X −8 (`p9_run_x`), then back to the PCB rear edge. It is 2.0 deep
     below the PCB (z −1.6..−3.6), so a wire lying flat has 0.6 mm spare, and the floor is 1.0 mm below it. It
     passes 0.6 mm clear of the leg-tail keep-out and 2.05 mm from the −X corner-pin boss.
  3. **Up behind the PCB rear edge:** through the 2.0 mm notch in the backstop ledge (X −9..−7, Y 29.0–31.0, the
     PCB edge to the rear wall), then straight up to the ceiling between the −X corner presser (2.25 mm away) and the
     Dupont housing (1.92 mm away).
  4. **Over the housing (revision 5):** +X along the rear of the bend zone, above and behind the module wires, to the
     corridor, then forward with the module's 5V wire to the 5V pad. The ghost route is about 100 mm long; cut the wire
     with some spare.
- **Solder-side clearance.** There is 3.0 mm between the PCB underside and the floor everywhere under the board, so
  the joint and a flat wire fit with no relief. A floor relief would not be possible anyway: the pad is over the logo,
  where the floor is 0.92 mm.
- `renders/section_p9.png` (X −9..−7, from +X) shows the run, the notch and the riser; `section_plan_wires.png` (plan,
  z 11.3–23.3) shows the riser joining the bend zone, the corridor and the rear-edge zone.

### Lid-tray snaps

- **Fingers:** there are two snap fingers per side (Y 6.3 and 27.4; the rear pair moved back 1.75 mm with the rear
  wall in revision 4), 1.4 mm thick and 6 mm wide, flush with the outside.
  - Their positions are set by the USB slot, now in the +X wall. Each finger's lid slot stays at least 1.2 mm clear
    of it: 1.7 mm in front, 3.04 mm behind. The pattern is mirrored on both walls, so turning the XIAO round (revision
    5) changed nothing here; the −X wall now has no slot between its fingers. The −X stop web joins the −X wall
    inside the rear finger's slot span (Y 23.48..25.08), on the inner face, 1.46 mm from the slot.
  - The free length is 9 mm: 4 mm is slotted into the lid wall, and 5 mm hangs below the split.
  - In the lid wall the slot is 0.6 mm behind the finger (`lc_slot_back`), wide enough that PETG cannot bridge and
    weld it.
  - The hooks stand 0.6 mm proud of the finger (`lc_hook`); 0.45 mm of that passes the tray pocket floor, which is both
    the catch engagement and the finger deflection while it snaps over.
  - The bending strain at full deflection is about 1.2 % (1.5 · t · y / L², t 1.4, y 0.45, L 9).
- **Catch:** the hooks catch on the top edge of a window through the tray wall, in pockets on its outer face. The
  window opens below the RS232 board, beside the ledge. Its top is 0.35 mm above the hook top (`lc_win_up`), so the
  snaps still click home if the lid is held up to 0.35 mm high by a pad, a presser or the flange.
- **Opening:** there is a 0.9 mm pry gap below each finger tip. Push a thin screwdriver in and lever the finger
  outwards.
- **Still the likely first failure.** These fingers are grown along Z and bend across the layer lines, the same
  weakness that broke the YL-97 case's XIAO clamps and board clips. Open and close them gently.

### Logo

- **Mark:** `assets/logo/zebserial-mark-deboss.svg`, the web page's striped mark (D-shell with three stripes and two
  WiFi arcs), every corner rounded to `logo_round` 0.62.
- **Where:** inlaid in the outside of the tray floor, the face that is up when the unit hangs on the printer and the
  bed side in print.
- **Size:** 25 mm wide (`logo_w`), 24.6 mm tall, centred on the floor flat. The floor flat is 29.65 mm deep
  (Y 1.5–31.15 at mid-span), so it leaves 2.51 mm front and rear (5.5 mm at the sides). It was sized when the floor
  was 27.9 mm deep (revision 3), and left at 25 mm.
  - At this size every stroke and gap is at least 1.2 mm (three 0.4 mm lines, `logo_min`). `part="check_logo"`
    checks it: what is thinner than `logo_min` survives "shape minus its opening", and narrower gaps survive
    "closing minus shape". Both are empty.
  - The same mark at 20 mm fails.
- **Orientation:** the mark is mirrored in X (`logo_2d()`), so it reads correctly seen from outside the floor (from
  −Z). Seen from below with +Y up, +X is to the left. The D-shell end faces the front (the DB9, Y 0) and the WiFi
  arcs radiate towards the rear. `renders/plugged_floor_logo.png` shows the mark as it hangs on the printer (DB9 and
  printer at the left), with the stripes reading as on the web page; `plugged_floor_deboss.png` shows the
  single-colour fallback and `plugged_iso.png` a perspective view.
- **Two-colour inlay (default, `logo_mode = "inlay"`):** `part="logo"` exports the logo body, `logo_depth` 0.68 mm
  thick (the profile's first four layers: 0.2 + 3 × 0.16), in tray print orientation and tray coordinates
  (`out/logo.stl`). The same body is cut from the tray with no clearance.
- **Single-colour fallback:** `logo_mode = "deboss"` cuts the same mark 0.68 mm deep; `"none"` leaves the floor plain.
- **Skin:** the floor is 1.6 mm (`floor_t`), so 0.92 mm of tray remains under the 0.68 mm logo. The model asserts at
  least 0.9 mm. 0.92 mm is six 0.16 mm layers, all solid (5 bottom and 6 top shell layers overlap across it), the
  same skin the lid top had under the logo in revision 2. A thicker floor would only add height to the case.
- **Lid top:** plain, in the faceted-pincushion style.

### Fit coupons

Both coupons are single colour and in print orientation. Both are single pieces with nothing under 1.2 mm except the
known snap hooks, gusset and tongue edges.

- **`part="coupon_tray"`** (`out/coupon_tray.stl`, 4.08 cm³) is the whole tray with the middle of its floor cut out
  (`coupon_floor`).
  - It keeps the front skin and bottom lip, the ledges, both corner pins on their bosses, the backstop ledge, the
    snap pockets and the USB tongue.
  - So the riskiest dimension can be tested: the 27.0 mm from the flange to the hole centres, against the lips and
    the backstop.
- **`part="coupon_lid"`** (`out/coupon_lid.stl`, 5.79 cm³) is the lid below Z 18 (`coupon_z`), printed cut face down.
  - It keeps the front wall and its plate bearing face, the top lip, the corner pressers, the snap fingers, the USB
    slot and relief, and the −X stop rib with its web (the web roots it in the side wall below the cut).
  - It leaves out the ceiling and the XIAO pads (`lid(xcap = false)`), which would otherwise be loose islands.
  - The cut is above the USB relief (top at z 16.75): a cut through the relief left 0.1 mm slivers.

## Assembly order

1. **Board into the tray.** Solder the pin 9 wire to the pin 9 pad first, lying flat along the solder side towards
   −X and then to the rear edge at X −8 (about 4 mm outboard of the header's −X pin), and bend it up there. Leave it
   long: it goes up to the ceiling, over the Dupont housing and forward to the XIAO's 5V pad, about 100 mm. Lower the
   HW-044 (DB9 fitted with its jack screws, shield on) straight down, with the flange just behind the bottom lip; the
   wire goes up through the notch in the backstop behind the board. The rear-corner holes drop over the pins (0.4 mm
   lead-ins), and the PCB lands on the front skin, the ledges and the pin bosses. The D-shell passes over the bottom
   lip (1.05 mm clear at the end).
2. **XIAO onto the board.** The wires are short and soldered, so keep it tethered:
   1. Plug the Dupont housing onto the header if it is not on yet.
   2. Rear edge: the TXD wire comes forward over the top of the housing's front face and down onto D5. The diode
      stands beside the housing's +X end, band up: its anode lead bends forward and down onto D3, and its cathode lead
      meets the RXD wire at the housing top.
   3. Front edge: the VCC and GND wires and the pin 9 wire go +X over the housing, forward over the XIAO's USB-C shell
      (between the shield and the +X wall), and down onto the 5V (VCC and pin 9) and GND pads, in front of the shell.
      Solder these on the top side only, and keep the joints and wires off the button next to the 5V pad.
   4. Lay the XIAO on the DB9 body top, USB-C towards +X, its USB-C shell in the tongue cradle, and its front edge
      about 2.2 mm behind the flange plate.
3. **Close.** Lower the lid straight down:
   - the top lip passes in front of the flange (0.3 mm lead-in) and the front wall comes down behind it;
   - the stop rib's lead-in pushes the XIAO towards +X if it sits too far −X, the front wall's and the stop block's
     lead-ins centre it in Y, and the lid's USB slot comes down over the receptacle;
   - the pads stop 0.25 mm above the shield, the USB shell and the rear edge strip, the corner pressers 0.25 mm above
     the board;
   - the wires lie in the corridor between the shield pad ring and the USB pad; press them down onto the USB-C shell
     first so that the pads do not pinch them;
   - the four snaps click.
   No rigid feature has to pass through the XIAO, the diode, the board or the DB9 on the way: `check_lid_path` proves
   it (below).
4. **Check:** plug a USB-C cable in and out. Nothing should move.

## Interference checks

**Parameter warnings:** the model echoes `ZEBCASE WARNING: ...` when a parameter combination gives wrong geometry:
the top over the `max_above` envelope, a flange lip within `lip_ds_clr` of the D-shell, the lips reaching the
jackpost area, the XIAO lying under `front_rest_min` on the DB9 body, or a leg-tail pocket within 0.8 mm of the floor
bottom. `build.sh` evaluates the model once (OpenSCAD echo export, with its extra `-D` overrides) and prints each one
as a `FAIL:` line, which fails the build; for example `./build.sh -D lip_x=10` ends with
`FAIL: ZEBCASE WARNING: the lips reach the jackpost area at the flange holes`. `controls.sh` counts a run that echoes
one as an error. Reminders that print on a good build are `ZEBCASE NOTE` lines; `build.sh` prints them and passes.

`build.sh` intersects:

- the ghosts with the tray (`check_tray`)
- the ghosts with the lid (`check_lid`)
- the tray with the lid (`check_traylid`)
- the lid with the parts swept 30 mm downwards (`check_lid_path`): lid material directly below a part is what the lid
  would hit while closing straight down over parts already in the tray. The diode is swept too (revision 5); the
  wires are flexible and are left out.

Each must have zero volume. Zero-thickness contact faces are designed contacts:

- the flange plate back on the lid front wall (Y 5.1; it shows from z 10.35, above the new lead-in)
- the PCB on the front skin, the ledges and the pin bosses
- the leg-tail keep-out on the floor top (the tails reach it)
- the lid rim on the tray rim
- the shield stop block's and the −X stop rib and web's rear faces (Y 25.08) on the bend zone and its extensions
- the pin 9 wire's riser on the sides of the backstop notch (the notch is exactly `wire_pass` wide)

**Controls** (`./controls.sh`, 97 lines, all must pass; under a minute). An OpenSCAD error (an `ERROR` or `WARNING`
line, a `ZEBCASE WARNING` echo, a failed assert, or a non-zero exit that is not just an empty result) counts as a FAIL, in `build.sh` as well. Each
moves one ghost (or one feature) and states whether a check must find real overlap (any piece of at least 0.01 mm³)
or none. I looked at where each positive control hits, not only that it hits:

| Moved | Check | Result |
|-------|-------|--------|
| board + DB9 forward 0.05 | tray, lid | none (lips 0.1 in front) |
| board + DB9 forward 0.15 | tray / lid | hits the bottom lip / the top lip |
| board + DB9 forward 1.2 | tray | hits the bottom lip (and the pins, and the pin zone in the front skin) |
| board + DB9 back 0.2 | tray | none (backstop ledge 0.25 behind) |
| board + DB9 back 0.2 | lid | hits the lid front wall (the datum, zero gap) |
| board + DB9 back 0.3 | tray | hits the backstop ledge |
| mounting holes ±0.4 in Y | tray | none |
| mounting holes ±0.5 in Y | tray | hits the corner pins |
| board + DB9 ±0.15 in X | tray | none |
| board + DB9 ±0.3 in X | tray | hits the side walls, the ledges and the corner pins |
| mounting holes ±0.15 in X | tray | none |
| mounting holes ±0.3 in X | tray | hits the corner pins |
| leg-tail keep-out +0.4 / +0.6 in Y | tray | none / hits the side ledges |
| leg-tail keep-out +0.4 / +0.6 in X | tray | none / hits the side wall |
| RS232 PCB alone up 0.2 / 0.3 (`ghost_board_dz`) | lid | none / hits both corner pressers (0.25 above) |
| XIAO up 0.05 and 0.2 | lid | none |
| XIAO up 0.3 | lid | hits the USB slot, the USB pad, the shield pad and the rear strip pad |
| XIAO down 0.04 / 0.06 | tray | none / hits the tongue |
| XIAO ±X 0.15 | lid | none |
| XIAO +X 0.3 / −X 0.3 | lid | hits the +X wall beside the USB slot / the −X stop rib |
| XIAO ±Y 0.15 | lid (and tray for +Y) | none |
| XIAO forward 0.3 | lid | hits the front wall and the USB slot side (and the solder blobs reach the relief face) |
| XIAO back 0.35 | lid | hits the shield stop block and the USB slot side |
| USB shell pad over the whole shell (`ctl_usbpad_full`) | lid | hits the 5V/GND corridor (170 mm³) |
| front wall without its joint relief (`fw_relief=false`) | lid | hits the 5V/GND/3V3 joint zones (24 mm³) |
| corridor keep-out 0.2 / 0.4 wider than the case's (`ghost_wch_hw`) | lid | none / hits the USB pad and the shield stop block |
| rear-edge zones (D3/D5 joints, rear strip, diode) +X 0.2 / 0.4 (`ghost_rear_d`) | lid | none / hits the shield stop block |
| rear-edge zones −X 3.6 / 3.9 | lid | none / hits the −X stop rib |
| diode −Y 0.2 / 0.4 (`ghost_diode_d`) | lid | none / hits the shield stop block's rear corner (0.15 mm³) |
| diode +X 1, +Y 2 | lid | none |
| diode +Y 2.8 | lid | hits the rear wall |
| no diode (`diode_on=false`) | lid | none |
| test block under the diode body (`ctl_diode_undercut`) | lid / lid path | none / hits (0.15 mm³) |
| Dupont keep-out (with the bend zone's extension in the wire zones) ±X 1 / down 1 | lid | none |
| Dupont keep-out −Y 1 / up 1 | lid | hits the −X stop rib and web and the shield stop block / the ceiling |
| Dupont keep-out +Y 0.5 / +Y 1 | tray, lid | none / hits the rear wall |
| revision 3 rear face (`ctl_rear_rev3`) | tray / lid | hits the rear wall (Dupont rear room 41 mm³, pin 9 riser 26 mm³) / 370 mm³ |
| pin 9 wire keep-out down 0.4 / 0.6 | tray | none / hits the floor under the joint |
| pin 9 wire keep-out ±X 0.1 | tray | hits the backstop notch side |
| pin 9 wire keep-out +Y 0.1 | lid | hits the rear wall |
| backstop without the notch (`p9_notch=false`) | tray | hits the backstop (12 mm³) |
| jack hardware up 0.2 / 0.4 | lid | none / hits the cut-back front wall, both sides |
| jack hardware ±X 0.15 / 0.3 | lid | none / hits the lid side wall |
| revision 3 wings (`fw_wings=true`) | lid / lid path | hits both wings (67 and 41 mm³) / 111 mm³ |
| flange plate 0.5 deeper (`ghost_plate_d` 5.6) | lid / lid path | hits the front wall (32 mm³) |
| revision 2 lid (`fl_plate_d` 0.38) against the 5.1 plate | lid | hits (114 mm³): the fault revision 2 fixed |
| snap pockets 0.3 too tight (`clip_clr` −0.3) | tray / lid | hits the snap pockets (and the USB tongue, whose width uses `clip_clr`) |
| pressers into the tray wall (`press_wall_clr` −0.1) | tray / lid | hits both corner pressers |
| lid held up 0.3 (`ghost_lid_dz`) | tray / lid | none: the hooks are still under the catch window tops |
| lid held up 0.4, or 0.3 with the old 0.1 window | tray / lid | hits all four hooks (hook over the window top) |
| logo at 25 mm / 20 mm | `check_logo` | none / hits |
| test block 0.5 under the XIAO rear edge (`ctl_undercut`) | lid / lid path | none / hits (2.4 mm³) |

For `check_lid_path`, the control `-D ctl_undercut=true` adds a test block to the lid 0.5 mm under the XIAO's rear
edge, under the unused D6 pad (revision 4 had it at X −3..−1, which is now under the D5 joint zone). It is clear in
the closed position (`check_lid` stays empty) but the swept check finds it.

The volumes in the table are from this revision. Revision 4's README still gave 71, 124 and 266 mm³ for the plate,
wings and revision 2 controls, from before the front wall was cut back over the jack screw hardware; revision 4's
model gives 31.5 mm³ for the plate control under the same OpenSCAD. The revision 5 volumes differ a little from that
because the lead-in is left out behind the 5V/GND relief.

`box()` asserts that its bounds are in order, so a box built from reversed d / w ranges (which run along −X / −Y)
fails the build instead of quietly vanishing. Revision 4's model had one: the button ghost on the D0..D6 side was
never drawn.

**Wall thickness:** `build.sh` also exports both parts in assembly position (`part="tray_inplace"`/`"lid_inplace"`) and
runs `wallprobe.py`, which casts a ray inwards from sample points on every face and lists everything under 1.2 mm.
What remains is deliberate, and `build.sh` whitelists exactly these regions (`--allow` boxes with a minimum
thickness); any other sample under 1.2 mm fails the build:

- the lid snap hooks (0.95 and 1.10 mm)
- the 0.92 mm floor skin under the logo pocket, over the logo's footprint only (X ±13, Y 3.3–29.4): the inlay fills
  the pocket in the two-colour print, and 0.92 mm prints as six solid 0.16 mm layers. This entry moved from the lid
  top (revision 2) to the tray floor with the logo.
- the tongue's top edges beside the USB cradle, now in the +X wall (0.93–0.95 mm; they taper to the 0.5 mm
  `tongue_tip` at the top). They only round the opening and carry nothing: the shell rests on the cradle bottom,
  0.35 mm off these edges.
- 1.19 mm at the front bottom floor: the floor / front skin gusset, 1.2 mm by construction square to the 45° chamfer

The probe found and fixed in revision 5: 1.04 mm (0.9 mm at the very edge) where the 0.3 mm jack screw lead-in met
the 5V/GND relief on the cut-back front wall. The lead-in is left out behind the relief. The whitelist entry for the
tongue moved from −X to +X; there is no −X entry left, so a new sliver there would fail.

The probe found and fixed in revision 4: 1.17 and 1.19 mm at the cut-back front wall's bottom edge, where a 0.5 mm
lead-in met the XIAO lead-in on the 1.6 mm wall; the new lead-ins are 0.3 mm (`jack_lead`).

The probe found and fixed in revision 3:

- 0.0–0.8 mm slivers where a presser bore over a protruding pin broke out of the presser front face; the pin top is
  now below the PCB top.
- A 0.46 mm sliver where the front wall's XIAO lead-in cut ran into the −X side wall.
- A 0.94 mm web between two separate D3 and D5 reliefs; there is now one relief.
- 1.15 mm behind the shadow-line groove on the 1.6 mm rear wall; the rear wall is now 1.65 mm.

The probe casts rays along face normals, so it does not measure distance square to a 45° face. Checked by hand:

- the front bottom corner facets (1.27 mm)
- the bottom front chamfer against the front skin and gusset (1.2 mm)
- the top chamfer over the USB relief (1.2 mm, by construction of `z_top`)

## Fit-check list (measure before printing)

Measured by the user: board 32 × 29; mounting holes 2.4 mm, 26.5 mm apart, 2 mm from the rear edge; Dupont top 18.3
mm above the PCB underside with 25 mm of wire turning space. Everything below is assumed or estimated:

- **DB9 (female):** flange 30.8 × 12.5 × 0.38, bottom level with the PCB top; D-shell outline 2.05–10.45 above the
  PCB top; rear body 17.2 wide × 10.4 high to Y 12.5; lugs 3.4 high to Y 12.0; front plate full outline back to Y 5.1.
  These are the YL-97 male part's numbers. The lips assume the D-shell height (1.05 mm clear of it top and bottom). The
  XIAO's height is the body height, and the lid front wall bears on the plate back at Y 5.1.
- **PCB thickness 1.6** and **3 mm clear under the board** (the tallest solder-side pin or fillet).
- **Board flush:** the PCB front edge flush with the flange front face (`pcb_front_y = 0`).
- **Header row:** 1.5 mm in from the rear edge, centred in X (`hdr_row_in`). This sets the Dupont housing's front face
  (Y 26.23) and the XIAO's 1.15 mm rear clearance; the model asserts at least 0.5.
- **Leg tails:** position about 8.6 mm behind the front edge, about 25.3 mm apart, round 3.1 × 3.0 below the PCB. The
  case keeps ±1.5 mm around that clear, so they only need measuring if they are further out than that.
- **Component side at the rear corners:** the corner pressers rest 0.25 mm above the PCB over X ±11.25..16, Y 26.2–29.
  Check there is nothing taller than 0.2 mm there (the photo shows only the solder side).
- **Solder side at the corners:** the pin bosses (4.4 mm) sit under the holes; check there is no pin or fillet within
  2.2 mm of a hole centre.
- **XIAO shield** centred across the XIAO's width (the shield stop block bears 0.3 behind its rear face).
- **5V/GND/3V3 joints on top (revision 5):** those pads are on the front edge, over the DB9 body's +X end and 0.65 mm
  above the +X jack screw hardware, so nothing may stick out under them. D3 and D5 are on the rear edge, which
  overhangs the RS232 board, so they can be soldered either side.
- **The RX diode (revision 5, not measured):** the keep-out is for a DO-41 (2.7 × 5.2 mm body, 0.8 mm leads)
  standing up beside the housing's +X end, anode down to D3, band up. Measure the real diode and note how its anode
  lead is bent at the D3 pad; set `diode_d`, `diode_l`, `diode_c` and `diode_dir` and rebuild if it differs. The
  shield stop block is 0.48 mm from the body's default position, the nearest case feature.
- **BOOT / RESET buttons:** one is next to the 5V pad on the front edge (X 14.1..15.7, Y 8.3..10.9); keep the 5V
  joint and wires off it.
- **USB-C shell:** 8.94 × 3.3 stadium, overhanging the XIAO PCB by 1.5 mm. The cradle and pads depend on it.
- **Lips and the printer:** nothing on the printer side should stand within 1.3 mm of our flange over the middle
  16 mm (|X| ≤ 8) above and below the D-shell. In the photo a gender changer sits between the board and the printer.
- **Step:** none behind the flange (`step_on = false`).
- **Revision 4 estimates:**
  - **Pin 9 pad** at X −4.155, Y 7.1 (`p9_x`, `p9_y`): the joint keep-out is 3 × 3, so it only matters if the pad is
    more than 1.5 mm off.
  - **Jack screw hardware** behind the flange: within 7 mm across (`jack_d`), 5 mm behind the plate back
    (`jack_l`), and below z 9.75. The XIAO's +X end and its 5V/GND pads are only 0.65 mm above it.
  - **Wire:** 1.4 mm outside diameter for the pin 9 jumper (`wire_d`; the model asserts `wire_pass`, 2.0, leaves 0.4 spare).

## Concerns to check before printing

- **Snaps:** see above; they are the remaining across-layer flexing parts.
- **The DB9 numbers are borrowed.** If the female DB9's flange plate is shallower or deeper than 5.1 mm, the lid front
  wall either leaves the board loose fore-aft or will not close. If its body is lower or higher than 10.4 mm, the XIAO
  sits low on the stop rib or rides up into the pads. Print the coupons first.
- **USB plug size:** the relief takes an overmould up to 12 × 7 mm, as before. The large right-angle plug in the
  photo will probably not seat fully.
- **Antenna proximity:** the stop rib starts 0.27 mm behind the ceramic antenna and 0.36 mm off its end. Since
  revision 5 the antenna is on the front half of the XIAO, its front 2 mm over the DB9 rear body, and the 5V/GND
  corridor is about 12 mm away in X. If WiFi range is worse than with revision 4, use the U.FL socket (a mated U.FL
  plug, 2.5 mm tall, fits under the ceiling, but its cable has no exit).
- **−X stop:** a 1.6 mm rib held by the ceiling and a web at its rear end only (it cannot reach the front wall past
  the antenna). It takes the USB-C insertion force. If the XIAO shifts towards −X when a cable goes in, that is
  where to look.
- **XIAO rear (+Y) location** relies on the shield (a stop block bearing over 1.48 mm, revision 4: 2.2 mm) and the
  USB slot sides. If the shield is not centred on the XIAO, adjust `shield_stop_clr`.
- **Wire corridor:** the three front-edge wires (VCC, GND, pin 9) share a 5.0 mm corridor over the USB-C shell, with
  the USB pad on one side and the shield pad ring on the other. Thick or stiff wire, or a bulky 5V joint with two wires,
  may need the lid to be closed with care.
- **Board hold-down is four points:** two lips at the front and two corner pressers at the rear. A board that bows
  between them is not pressed in the middle.
- **Pin engagement upside down:** hanging on the printer, the board can drop away from the tray by the presser gap
  (0.25) plus the snap-window play (0.35). That leaves about 0.5 mm of the 1.1 mm straight pin in the holes. The side
  walls still locate the board in X, so this only matters if the walls are printed oversize.
- **Lip strength:** the lips are 1.2 mm thick and also bend across layer lines if the flange is pulled forward hard.
  They only carry plug extraction force, but do not lever the case off the DB9 by the lips.
- **Light gap at the snaps:** the catch windows open into the space under the board. A sliver of light shows below
  each snap finger tip, where the pry gap meets the window.
