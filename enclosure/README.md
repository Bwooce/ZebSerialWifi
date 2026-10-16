# ZebSerialWifi enclosure

A two-part PETG case (tray + lid) for the HW-044 MAX3232 RS232-TTL board ("MAX3232 ESE 2512", female right-angle
DB9), with a Seeed XIAO ESP32-C6 lying across the DB9 body. The DB9 flange sits flush in the case front face, and only
the D-shell sticks out (6.0 mm), so the unit plugs straight into the printer. Two small lips in front of the flange
hold the flange and the loose shield. The two parts snap together, with no screws.

In use the unit hangs upside down on the printer: the board's solder side and the case's tray floor face up, and the
logo is inlaid in the tray floor. The XIAO's USB-C comes out of the side away from the printer's parallel port.

| File | What |
|------|------|
| `zebserial_case.scad` | The model. All dimensions are top-level parameters, grouped as *measured*, *XIAO (Seeed KiCad/STEP)*, *DB9*, *corner pins*, *header and Dupont*, *print/tolerance*, *style*, *logo* and *clips and XIAO capture*. |
| `reference/` | Not in git (gitignored): the manufacturer drawings and the Seeed XIAO ESP32-C6 KiCad PCB and STEP the dimensions come from. |
| `out/tray.stl`, `out/lid.stl`, `out/logo.stl` | Print-ready, already in print orientation. `logo.stl` is the two-colour logo inlay, in the same coordinates as `tray.stl`. |
| `out/coupon_tray.stl`, `out/coupon_lid.stl` | Fit coupons: the whole tray with the middle of its floor cut out, and the lid below Z 18, single colour. |
| `renders/*.png` | Assembly, exploded, as-plugged-in, front, side, sections, exterior style (`style_*.png`) and print-orientation views. |
| `build.sh` | Regenerates the STLs and renders, then runs the interference checks and the wall probe. Extra `-D` overrides apply to the checks, for example `./build.sh -D xpad_gap=0.3`. |
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
parallel-port side**. The USB-C therefore exits at **−X**, at the bottom of the photo. The photo agrees independently:
the header's four solder pads are a column along the photo's vertical at the board's right (rear) edge, centred, and
the two mounting holes are at the right-hand (rear) corners. `renders/plugged_floor_logo.png` is drawn in exactly
that view (`part="plugged"`: DB9 at the left, +X up), and shows the USB-C plug at the bottom.

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
  - Two 1.6 mm² bits of the USB relief's rounded corners beside the slot.
  - The two flange lips' 45° chamfers, at the threshold.
  - In the tray, the roof of the logo pocket 0.68 mm above the bed, which the inlay fills.
- **Logo (two colours):** load `tray.stl` and `logo.stl` together and accept "load as one object with multiple
  parts" (Bambu Studio), so they stay aligned; give the logo part the second filament. The inlay is the first four
  layers on the bed (0.2 + 3 × 0.16 = 0.68 mm), flush with the tray floor. For a single-colour print set
  `logo_mode = "deboss"` (0.68 mm deep) or `"none"` and rebuild.
- **Size:** 40.4 × 31.7 × 31.2 mm (X × Y × Z, the case shell from the front face at Y 0; the rear face is concave, 30.9 at
  mid-span). The two flange lips stand 1.3 mm proud of the front face over |X| ≤ 8, so 33.0 mm in Y over the lips, and the metal
  D-shell 6.0 mm. Revision 2 was 39.9 × 35.4 × 31.2 (36.65 over the lips). The top is 25.0 mm above the RS232 board,
  exactly the 25 mm limit (`max_above`): the Dupont wires' measured turning space sets it; see below.
- **Material:** 4.25 cm³ for the tray, 10.73 cm³ for the lid and 0.22 cm³ for the logo inlay, roughly 19 g of PETG at
  100 %, less with infill. The fit coupons are 3.65 cm³ (tray) and 5.53 cm³ (lid). The slicer figures are in
  `print/README.md`.

## Exterior style: faceted pincushion

The outside copies the look of the user's factory Radar360 case. The look is defined in
`~/personal_scripts/ai-config/instructions/enclosure_styles.md`. `renders/style_plan.png`, `style_iso.png` and
`style_side.png` show it. I compared them with the reference STLs, rendered from the same cameras. On the
reference, the "triangular corner facets" are corner truncations rather than tall vertical chamfers, so the model
follows that.

| Feature | Parameter | Value |
|---------|-----------|-------|
| Side and rear faces bow inwards at mid-span (2 % of each face's length) | `sag_frac` | 0.02: 0.63 mm on the sides, 0.81 mm on the rear |
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
    the −X side that also gives the USB relief its depth (2.07 mm at its shallowest).
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
  what the plate back bears on, with no gap: about 139 mm² in all, beside the DB9 body on both sides and across its
  top, the same area as before (only the wall's depth changed). The front wall's bottom front edge has a 0.5 mm 45°
  lead-in (`fw_lead`), so a board sitting slightly back is pushed forward onto the lips as the lid closes.
  `renders/lid_front.png` (lid alone, from the front) and `section_plate.png` (X 12) show it.
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
  - 2.0 mm across X (0.2 clearance per side). They locate the board in X.
  - In Y they are lens-shaped: the pin is the part of the 2.0 circle that stays inside the hole when the hole moves
    0.45 mm either way in Y (`mh_pin_clr_y`). So they never fight the flange datum (lips 0.1 in front, lid front wall
    at zero gap behind), and a hole up to 0.45 mm off its nominal Y still drops on.
  - The top is 0.1 below the PCB top, with a 0.4 mm 45° lead-in, so 1.1 mm of straight pin engages the hole. A pin
    standing proud of the board would need a bore in the presser that breaks out of its front face (that version
    left 0.0–0.8 mm slivers, which the wall probe caught).
- **Backstop rib removed.** The tray rear wall now stands 0.25 mm behind the PCB rear edge (`bs_clr`), the old rib's
  clearance, so the wall is the backstop. It stops the board going back before the lid is on, and never fights the
  flange datum. The pins could not do that job: their Y clearance is deliberately larger.
- **Corner pressers** (`cp_*`): each is a block from the lid side and rear walls down to 0.25 mm above the PCB top
  (`press_gap`), over the corner around the hole: X from 2.0 mm inside the hole centre to the PCB edge, Y from 0.8 mm
  in front of the hole centre (Y 26.2) to the PCB rear edge.
  - The front face stops at Y 26.2 because the XIAO's 5V/GND solder joints on its rear edge reach Y 25.98 above the
    −X presser. Moved 0.4 forward it hits them (a control).
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

**Placement.** The XIAO lies on the DB9 body, its underside at z 10.4 as before, turned 90°: its length runs along X
with the USB end at −X.

- **X:** X −16.0 to +4.96. The USB end is 0.2 mm from the −X inner wall, so the receptacle mouth (X −17.5) is
  1.3 mm into the wall. This is the old rear-wall arrangement turned 90°: the relief, slot and tongue all work as
  before.
  - Centred in X (X ±10.48), the mouth would be 4.2 mm inside the wall. The relief would then be a pocket about 8 mm
    deep, too deep for most plug overmoulds, and the tongue could not reach the shell.
  - The coordinator approved the shift: the user's requirement is USB-C out of the side, and "centred" was not one.
- **Y:** Y 7.3 to 25.08, 0.2 mm behind the lid front wall, the furthest forward it can go. The flange plate (to Y
  5.1, up to z 12.5) and the front wall behind it (to Y 7.1) are in front of it. Its rear edge is 1.15 mm in front of
  the Dupont housing (the model asserts at least 0.5, `xiao_hdr_min`). The front wall was thinned from 3.2 to 2.0
  mm for this; with 3.2 the XIAO would have reached into the housing.
- It lies 5.2 mm deep on the DB9 body (Y 7.3 to 12.5) over X −8.6 to +4.96 (the model warns below 2.0,
  `front_rest_min`).
- **Pad columns:** the XIAO cannot be mirrored, so turning the USB to −X puts the D0..D6 column on the front edge
  (over the DB9 body) and the 5V..D7 column on the rear edge (towards the header).
  - The firmware uses D3 and D5 (`board.c`: GPIO 21 and 23; RX goes through a Schottky diode), plus 5V and GND.
  - So D3 (X −5.46) and D5 (X −0.38) are on the front edge, and 5V (X −13.08) and GND (X −10.54) on the rear edge.
  - The D3/D5 pads lie on the DB9 body, so their joints must be on the top side only, with no solder blob
    underneath. The model echoes this as a check.

**Capture: all rigid, no flexing parts** (as in revision 2, turned 90°):

- **Below, front:** the top of the DB9 rear body (5.2 mm of the PCB lies on it).
- **Below, −X end:** the tray tongue under the overhanging USB-C shell, in the −X wall. Its top is a cradle shaped to
  the shell's stadium section, 0.05 mm under the shell bottom (`tongue_gap`) and 0.35 mm off the rounded ends.
- **Above:** rigid pads from the lid, 0.25 mm (`xpad_gap`) above:
  - **RF shield top:** the ring over the shield outline (inset 0.5), cut back on its +X side for the D3/D5 wire
    channel. What remains is a U over X −8.79..−5.72.
  - **USB-C shell's flat top:** from the shell's inner end to the −X wall. This is the support above the USB-C port,
    so a plug pushed in at an angle bears on the case.
  - **Rear edge strip:** the bottom of the shield stop block (below), over the unused D8/D7 pads, X −0.12..2.6.
  - The lid's USB slot, 0.2 mm above the shell, is the first thing a rising XIAO meets: up 0.2 is clear, up 0.3
    hits.
- **−X:** the lid's −X wall, 0.2 mm from the USB-end PCB edge beside the USB slot. Plug extraction bears here.
- **+X:** the stop rib, 1.6 mm thick, 0.2 mm from the antenna-end PCB edge (X 5.16). It replaces the revision 2
  front strut and does the same job, now against the plug insertion force (6 to 20 N, pushing the XIAO towards +X).
  - It runs from the lid front wall back to Y 16.44: the strut's old place in the gap between the U.FL socket
    (Y 10.6–13.2) and the ceramic antenna (from Y 16.71), ending 0.27 mm short of the antenna.
  - It hangs from the ceiling and is joined to the front wall, so it is not a free cantilever.
  - Over the DB9 body its bottom is 0.1 mm above the body top; behind the body (from Y 12.7) it reaches 0.6 mm below
    the XIAO underside.
  - A 0.4 mm 45° lead-in on its bottom inner edge (`stop_lead`) pushes a XIAO that sits too far +X back as the lid
    closes. Over the body it bears on the top 0.7 mm of the 1.2 mm PCB edge.
- **Front (−Y):** the lid front wall, 0.2 mm in front of the front PCB edge, plus the USB slot sides (0.2 mm from the
  shell).
  - Below the XIAO underside the wall's rear face is set back 0.4 mm, with a 45° lead-in up to z 10.8 (`xlead`). This
    replaces the old guides' lead-ins: a XIAO up to 0.4 mm too far forward is pushed back as the lid closes.
  - The wall is relieved 0.8 mm deep over X −7.53..1.69 (`fw_relief`) for the D3/D5 solder joints, leaving 1.2 mm
    of wall.
- **Rear (+Y):**
  - At the −X end, the USB slot sides (0.2 mm).
  - At the +X half, the shield stop block (`shield_stop_clr` 0.3) behind the shield's rear face, between the wire
    channel and the antenna. It has a 0.4 mm 45° lead-in on its bottom front edge.
  - There is no room for a guide behind the rear edge: the Dupont housing is 1.15 mm away and the 5V/GND joints are
    there. The shield is assumed centred across the XIAO's width (Seeed data).
- **Renders:**
  - `section_plan_usb.png` and `section_plan_usb_case_only.png`: a plan section at the USB-C axis, z 13.25.
  - `section_usb.png`: Y 16.2, through the USB axis (tongue, slot, relief, USB pad, shield pad, stop rib).
  - `section_x0.png`: X 0, across the XIAO (body, front wall and relief, channel, Dupont housing).
  - `section_shield_stop.png`: X 1.5.

**Antenna.** The antenna end points at +X, 11 mm from the +X wall. Over it there is only the 1.6 mm lid skin
(`ceil_t`). The nearest case features are both plastic: the stop rib, 0.36 mm off its end in X and ending 0.27 mm
short of it in Y, and the shield stop block, about 0.9 mm away diagonally. The DB9's metal is below the front of the
XIAO, as before.

### USB-C opening

- **Side wall:** the opening is in the −X side wall, centred on the USB axis (Y 16.19, z 13.25).
- **Plug relief:** 12 × 7 mm (`usb_recess_w`, `usb_recess_h`, along Y and Z), corner radii 2.5 mm
  (`usb_recess_r`). Its floor is flush with the receptacle mouth (X −17.5), so a fully seated plug bears on the case.
  - Its depth is set by the bowed side face: 2.07 mm at its shallowest. Revision 2 had 1.6 mm. The model asserts at
    least `usb_recess_d` (1.6).
  - **The plug limit is the same as before:** an overmould larger than 12 × 7 mm will not seat fully. Increase
    `usb_recess_w`/`usb_recess_h` if needed. The large right-angle plug in the user's photo is probably larger.
- **Receptacle slot:** the slot through the wall follows the USB-C shell's stadium section (r 1.65 + 0.2 mm
  clearance) at the top, so its visible ends are round. It stays open down to the lid rim so the closing lid passes
  over the receptacle; the tray tongue fills it from below, and its cradle rounds the lower corners. The tongue's
  edges beside the cradle rise to z 11.8 and are at least 0.5 mm wide (`tongue_tip`).
- Behind the relief floor the tongue is cut square (its rounded corners would otherwise leave knife-edge wedges on the
  tongue).
- **Renders:** `side_usb.png` (with the XIAO) and `side_usb_case_only.png`.

### Wires and the Dupont plug

A Dupont housing is plugged onto the HW-044's 4-pin header (straight pins on the component side, in a row along X,
centred on the rear edge). The model carries it as a keep-out (grey/cyan ghost, `ghost_dupont()`), and the
interference checks fail if any case feature enters it.

- **Housing:** 10.16 × 2.54 mm on the header, centred at X 0 and at Y 27.5: the pin row is assumed to be 1.5 mm in
  from the rear edge (`hdr_row_in`, an estimate from the photo). It reaches up to `dupont_top` = 16.7 mm above the PCB
  top (measured: 18.3 mm above the PCB underside).
- **Wire-bend zone:** above the housing up to the ceiling, and forward over the 1.15 mm gap to the XIAO's rear edge.
  The ceiling is at least `dupont_ceiling_min` = 23.4 mm above the PCB top (the user's 25 mm of turning space from
  the PCB underside) and at least `dupont_bend_h` = 4.0 above the housing. With the 1.6 mm top skin that puts the
  outside top at 25.0 mm, exactly `max_above`; there is no margin left in the envelope, and the model asserts it.

The wires leave the top of the housing. The magenta wire zones are keep-outs, and the interference check fails if any
case feature enters them:

- **5V/GND:** from the bend zone towards −X, over the gap in front of the housing, down onto the rear-edge pads. The
  zone is X −14.85 to −5.08, from the XIAO top to the ceiling. A second zone sits under those pads for underside
  joints.
- **D3/D5:** over the housing and forward across the XIAO through a 5 mm channel over the shield (X −5.42..−0.42,
  `wch_hw`), from the XIAO top to the ceiling. They come down onto the front-edge pads, in front of the shield, in the
  front wall's relief. The shield pad is cut back for the channel, and the shield stop block sits on the channel's +X
  side.
- The channel is also the room for the RX diode: 5 mm wide and 9.6 mm from the shield top to the ceiling, enough for a
  DO-41 body (2.7 × 5.2) laid along it.

The green ghost wires show one feasible route, and the interference check confirms that no case feature crosses it.
Which header pin carries which signal is not modelled; the wires can cross in the bend zone.

- **Moved 1 mm** (`ghost_dupont_d`): ±X and down stay clear. −Y meets the shield stop block (the bend zone runs up to
  it), +Y meets the rear wall (the housing is 0.48 mm from it), and up meets the ceiling (the zone runs to it by
  definition).

### Lid-tray snaps

- **Fingers:** there are two snap fingers per side (Y 6.3 and 25.65), 1.4 mm thick and 6 mm wide, flush with the
  outside.
  - Their positions are set by the −X USB slot. Each finger's lid slot stays at least 1.2 mm clear of it: 1.7 mm in
    front, 1.29 mm behind. The pattern is mirrored to the +X side.
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
- **Size:** 25 mm wide (`logo_w`), 24.6 mm tall, centred on the floor flat. The floor flat is 27.9 mm deep
  (Y 1.5–29.4 at mid-span), so the height sets the size and leaves 1.63 mm front and rear (5.6 mm at the sides).
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

- **`part="coupon_tray"`** (`out/coupon_tray.stl`, 3.65 cm³) is the whole tray with the middle of its floor cut out
  (`coupon_floor`).
  - It keeps the front skin and bottom lip, the ledges, both corner pins on their bosses, the rear wall (the
    backstop), the snap pockets and the USB tongue.
  - So the riskiest dimension can be tested: the 27.0 mm from the flange to the hole centres, against the lips and
    the rear wall.
- **`part="coupon_lid"`** (`out/coupon_lid.stl`, 5.53 cm³) is the lid below Z 18 (`coupon_z`), printed cut face down.
  - It keeps the front wall and its plate bearing face, the top lip, the corner pressers, the snap fingers, the USB
    slot and relief, and the stop rib.
  - It leaves out the ceiling and the XIAO pads (`lid(xcap = false)`), which would otherwise be loose islands.
  - The cut is above the USB relief (top at z 16.75): a cut through the relief left 0.1 mm slivers.

## Assembly order

1. **Board into the tray.** Lower the HW-044 (DB9 fitted, shield on) straight down, with the flange just behind the
   bottom lip. The rear-corner holes drop over the pins (0.4 mm lead-ins), and the PCB lands on the front skin, the
   ledges and the pin bosses. The D-shell passes over the bottom lip (1.05 mm clear at the end).
2. **XIAO onto the board.** The wires are short and soldered, so keep it tethered:
   1. Plug the Dupont housing onto the header if it is not on yet. 5V/GND go over the gap in front of the housing
      towards −X, down to their pads on the XIAO's rear edge. D3/D5 go over the housing and forward across the
      middle of the XIAO, between the shield's −X part and the stop block, down to their pads on the front edge.
   2. Lay the XIAO on the DB9 body top, USB-C towards −X, its USB-C shell in the tongue cradle, and its front edge
      about 2.2 mm behind the flange plate.
3. **Close.** Lower the lid straight down:
   - the top lip passes in front of the flange (0.3 mm lead-in) and the front wall comes down behind it (0.5 mm
     lead-in);
   - the stop rib's lead-in pushes the XIAO towards −X if it sits too far +X, the front wall's and the stop block's
     lead-ins centre it in Y, and the lid's USB slot comes down over the receptacle;
   - the pads stop 0.25 mm above the shield, the USB shell and the rear edge strip, the corner pressers 0.25 mm above
     the board;
   - the four snaps click.
   No rigid feature has to pass through the XIAO, the board or the DB9 on the way: `check_lid_path` proves it (below).
4. **Check:** plug a USB-C cable in and out. Nothing should move.

## Interference checks

`build.sh` intersects:

- the ghosts with the tray (`check_tray`)
- the ghosts with the lid (`check_lid`)
- the tray with the lid (`check_traylid`)
- the lid with the parts swept 30 mm downwards (`check_lid_path`): lid material directly below a part is what the lid
  would hit while closing straight down over parts already in the tray.

Each must have zero volume. Zero-thickness contact faces are designed contacts:

- the flange plate back on the lid front wall (Y 5.1; it shows from z 4.5, above the front wall lead-in)
- the PCB on the front skin, the ledges and the pin bosses
- the leg-tail keep-out on the floor top (the tails reach it)
- the lid rim on the tray rim, and the shield stop block's rear face on the Dupont bend zone

**Controls** (`./controls.sh`, 61 lines, all must pass; under a minute). An OpenSCAD error (an `ERROR` line such as a
failed assert, or a non-zero exit that is not just an empty result) counts as a FAIL, in `build.sh` as well. Each
moves one ghost (or one feature) and states whether a check must find real overlap (any piece of at least 0.01 mm³)
or none. I looked at where each positive control hits, not only that it hits:

| Moved | Check | Result |
|-------|-------|--------|
| board + DB9 forward 0.05 | tray, lid | none (lips 0.1 in front) |
| board + DB9 forward 0.15 | tray / lid | hits the bottom lip / the top lip |
| board + DB9 forward 1.2 | tray | hits the bottom lip |
| board + DB9 back 0.2 | tray | none (rear wall 0.25 behind) |
| board + DB9 back 0.2 | lid | hits the lid front wall (the datum, zero gap) |
| board + DB9 back 0.3 | tray | hits the rear wall |
| mounting holes ±0.4 in Y | tray | none |
| mounting holes ±0.5 in Y | tray | hits the corner pins |
| board + DB9 ±0.15 in X | tray | none |
| board + DB9 ±0.3 in X | tray | hits the corner pins |
| leg-tail keep-out +0.4 / +0.6 in Y | tray | none / hits the side ledges |
| leg-tail keep-out +0.4 / +0.6 in X | tray | none / hits the side wall |
| RS232 PCB alone up 0.2 / 0.3 (`ghost_board_dz`) | lid | none / hits both corner pressers (0.25 above) |
| corner pressers 0.4 forward (`cp_front` 1.2) | lid | hits the 5V/GND joints and their zone |
| XIAO up 0.05 and 0.2 | lid | none |
| XIAO up 0.3 | lid | hits the USB slot, the USB pad, the shield pad and the rear strip pad |
| XIAO down 0.04 / 0.06 | tray | none / hits the tongue |
| XIAO ±X 0.15 | lid | none |
| XIAO +X 0.3 / −X 0.3 | lid | hits the stop rib / the −X wall |
| XIAO ±Y 0.15 | lid (and tray for +Y) | none |
| XIAO forward 0.3 | lid | hits the front wall and the USB slot side |
| XIAO back 0.35 | lid | hits the shield stop block and the USB slot side |
| shield pad ring over the whole shield (`ctl_ring_full`) | lid | hits the D3/D5 channel |
| front wall without its joint reliefs (`fw_relief=false`) | lid | hits the D3/D5 pad zones |
| Dupont keep-out ±X 1 / down 1 | lid | none |
| Dupont keep-out −Y 1 / +Y 1 / up 1 | lid | hits the shield stop block / the rear wall / the ceiling |
| flange plate 0.5 deeper (`ghost_plate_d` 5.6) | lid / lid path | hits the front wall |
| revision 2 lid (`fl_plate_d` 0.38) against the 5.1 plate | lid | hits: the fault revision 2 fixed |
| snap pockets 0.3 too tight (`clip_clr` −0.3) | tray / lid | hits |
| pressers into the tray wall (`press_wall_clr` −0.1) | tray / lid | hits |
| lid held up 0.3 (`ghost_lid_dz`) | tray / lid | none: the hooks are still under the catch window tops |
| lid held up 0.4, or 0.3 with the old 0.1 window | tray / lid | hits (hook over the window top) |
| logo at 25 mm / 20 mm | `check_logo` | none / hits |
| test block 0.5 under the XIAO rear edge (`ctl_undercut`) | lid / lid path | none / hits |

For `check_lid_path`, the control `-D ctl_undercut=true` adds a test block to the lid 0.5 mm under the XIAO's rear
edge. It is clear in the closed position (`check_lid` stays empty) but the swept check finds it.

**Wall thickness:** `build.sh` also exports both parts in assembly position (`part="tray_inplace"`/`"lid_inplace"`) and
runs `wallprobe.py`, which casts a ray inwards from sample points on every face and lists everything under 1.2 mm.
What remains is deliberate, and `build.sh` whitelists exactly these regions (`--allow` boxes with a minimum
thickness); any other sample under 1.2 mm fails the build:

- the lid snap hooks (0.95 and 1.10 mm)
- the 0.92 mm floor skin under the logo pocket, over the logo's footprint only (X ±13, Y 2.5–28.5): the inlay fills
  the pocket in the two-colour print, and 0.92 mm prints as six solid 0.16 mm layers. This entry moved from the lid
  top (revision 2) to the tray floor with the logo.
- the tongue's top edges beside the USB cradle, now in the −X wall (0.93–0.95 mm; they taper to the 0.5 mm
  `tongue_tip` at the top). They only round the opening and carry nothing: the shell rests on the cradle bottom,
  0.35 mm off these edges.
- 1.19 mm at the front bottom floor: the floor / front skin gusset, 1.2 mm by construction square to the 45° chamfer

The probe found and fixed in this revision:

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
- **D3/D5 joints on top:** those pads rest on the DB9 body, so nothing may stick out under them.
- **USB-C shell:** 8.94 × 3.3 stadium, overhanging the XIAO PCB by 1.5 mm. The cradle and pads depend on it.
- **Lips and the printer:** nothing on the printer side should stand within 1.3 mm of our flange over the middle
  16 mm (|X| ≤ 8) above and below the D-shell. In the photo a gender changer sits between the board and the printer.
- **Step:** none behind the flange (`step_on = false`).

## Concerns to check before printing

- **Snaps:** see above; they are the remaining across-layer flexing parts.
- **The DB9 numbers are borrowed.** If the female DB9's flange plate is shallower or deeper than 5.1 mm, the lid front
  wall either leaves the board loose fore-aft or will not close. If its body is lower or higher than 10.4 mm, the XIAO
  sits low on the stop rib or rides up into the pads. Print the coupons first.
- **USB plug size:** the relief takes an overmould up to 12 × 7 mm, as before. The large right-angle plug in the
  photo will probably not seat fully.
- **Antenna proximity:** the stop rib runs to 0.27 mm from the end of the ceramic antenna, and the DB9's metal is
  under the other end of the XIAO. If WiFi range is poor, use the U.FL socket (a mated U.FL plug, 2.5 mm tall, fits
  under the ceiling, but its cable has no exit).
- **XIAO rear (+Y) location** relies on the shield (stop block) and the USB slot sides. If the shield is not centred
  on the XIAO, adjust `shield_stop_clr`.
- **Board hold-down is four points:** two lips at the front and two corner pressers at the rear. A board that bows
  between them is not pressed in the middle.
- **Lip strength:** the lips are 1.2 mm thick and also bend across layer lines if the flange is pulled forward hard.
  They only carry plug extraction force, but do not lever the case off the DB9 by the lips.
- **Light gap at the snaps:** the catch windows open into the space under the board. A sliver of light shows below
  each snap finger tip, where the pry gap meets the window.
