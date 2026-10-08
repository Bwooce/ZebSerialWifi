# ZebSerialWifi enclosure

A two-part PETG case (tray + lid) for the YL-97 MAX3232 RS232-TTL board with its right-angle male DB9, and a Seeed
XIAO ESP32-C6 mounted above it. The DB9 flange sits flush in the case front face, and only the D-shell with its pins
sticks out (6.0 mm), so the unit plugs straight into the printer. Two small lips in front of the flange hold the
flange and the loose shield. The two parts snap together, with no screws.

| File | What |
|------|------|
| `zebserial_case.scad` | The model. All dimensions are top-level parameters, grouped as *measured*, *XIAO (Seeed KiCad/STEP)*, *DB9 (manufacturer drawings)*, *header guesses*, *print/tolerance*, *style* and *clips and XIAO capture*. |
| `reference/` | Not in git (gitignored): the manufacturer drawings and the Seeed XIAO ESP32-C6 KiCad PCB and STEP the dimensions come from. |
| `out/tray.stl`, `out/lid.stl`, `out/logo.stl` | Print-ready, already in print orientation. `logo.stl` is the two-colour logo inlay, in the same coordinates as `lid.stl`. |
| `out/coupon_tray.stl`, `out/coupon_lid.stl` | Fit coupons: the front 14.5 mm of each part, single colour, for a quick test of the DB9 fit. |
| `renders/*.png` | Assembly, exploded, front, rear, sections, exterior style (`style_*.png`) and print-orientation views. |
| `build.sh` | Regenerates the STLs and renders, then runs the interference checks and the wall probe. Extra `-D` overrides apply to the checks, for example `./build.sh -D xiao_rest=1.5`. |
| `controls.sh` | Positive and negative controls for the interference checks (moves the ghosts and says what must and must not hit). Exits non-zero on any failure. |
| `stlinfo.py` | Reports STL volume and bounding box, and with `--components` the overlapping pieces. |
| `wallprobe.py` | Measures local wall thickness by ray casting from every face; `build.sh` lists everything under 1.2 mm. |

## Coordinates

- **Y:** 0 is the DB9 flange front face, which is also the case front face (flush). +Y runs towards the rear (the
  USB-C end). The flange back face (Y 0.38, `y_fb`) bears on the lid front wall. The RS232 PCB runs from Y 0 (flush
  with the flange front) to Y 26.
- **Z:** 0 is the RS232 PCB top surface.
- **X:** looking down with the DB9 nearest you:
  - −X is the side with the RS232 header and the XIAO D0..D6 pads.
  - +X is the side with the XIAO 5V/GND..D7 pads.

In the code, `y_cf` (0) is the case front and `y_fb` (`fl_t`) the flange back; before this revision `y_fb` meant both.

## Print settings (Bambu H2D, 0.4 mm nozzle, PETG)

- **Layer height:** 0.2 mm.
- **Walls:** 4 (1.6 mm).
- **Infill:** 25 % gyroid. Most of the part is walls anyway.
- **Supports:** none.
- **Brim:** none needed. The parts are 40 × 35 mm.
- **Orientation:** the STLs are already oriented.
  - Tray: floor on the bed (open side up).
  - Lid: top skin on the bed (open side up).
- **Overhangs:** a probe of the oriented STLs for down-facing faces steeper than 45° off the bed finds only: the
  tray's four catch-window tops (6.6 mm bridges), the lid's snap-hook catch faces (0.75 mm) and two 1.2 mm² bits of the USB relief's rounded corners beside the slot. These
  are fine in PETG. Everything else is vertical, at 45°, or opens upwards in print orientation:
  - The perimeter chamfers on the bed side of each part are 45°.
  - The top of each snap catch window in the tray is a 6.6 mm bridge.
  - The top flange lip on the lid hangs from a 45° chamfer (the lid prints upside down, so the chamfer is under
    the lip). The bottom lip on the tray has a 45° root chamfer underneath.
  - The board pressers grow out of the lid side walls with a 45° root.
  - The guide walls, the shield pad ring, the USB pad and the front strut grow straight up from the lid ceiling; their
    lead-ins are 45°.
  - The USB slot opens to the lid rim, and its rounded end is a cradle in print orientation. The tongue cradle and
    the leg-tail pockets open upwards.
- **Logo (two colours):** load `lid.stl` and `logo.stl` together and accept "load as one object with multiple
  parts" (Bambu Studio), so they stay aligned; give the logo part the second filament. The inlay is the first three
  0.2 mm layers on the bed, flush with the lid top. For a single-colour print set `logo_mode = "deboss"` (0.6 mm deep)
  or `"none"` and rebuild.
- **Size:** 39.9 × 35.4 × 31.2 mm assembled (the case shell, from the front face at Y 0). The two flange lips stand
  1.3 mm proud of the front face over |X| ≤ 8, and the metal D-shell 6.0 mm. The top is 25.0 mm above the RS232
  board, exactly the 25 mm limit (`max_above`): the Dupont wires' measured turning space sets it; see below.
- **Material:** 5.5 cm³ for the tray, 11.4 cm³ for the lid and 0.28 cm³ for the logo inlay, roughly 22 g of PETG at
  100 %, less with infill. The fit coupons are 1.96 cm³ (tray) and 5.02 cm³ (lid), about 9 g together.

## Exterior style: faceted pincushion

The outside copies the look of the user's factory Radar360 case. The look is defined in
`~/personal_scripts/ai-config/instructions/enclosure_styles.md`. `renders/style_plan.png`, `style_iso.png` and
`style_side.png` show it. I compared them with the reference STLs, rendered from the same cameras. On the
reference, the "triangular corner facets" are corner truncations rather than tall vertical chamfers, so the model
follows that.

| Feature | Parameter | Value |
|---------|-----------|-------|
| Side and rear faces bow inwards at mid-span (2 % of each face's length) | `sag_frac` | 0.02: 0.71 mm on the sides, 0.80 mm on the rear |
| Each face is three flat facets: a flat middle third, sloping to the corners | `facet_flat` | 0.3 (0 gives a two-facet V) |
| Each of the 8 corners cut off by a flat plane square to the body diagonal, leaving a triangular facet in plan, side and front views | `corner_facet` | 5.0 mm along each edge (12.5 % of the width) |
| Small 45° bevel on the vertical corner edges between the facets | `corner_chamfer` | 1.5 mm |
| Top and bottom perimeter edges chamfered at 45°, so the side profile is an elongated octagon | `edge_chamfer` | 1.5 mm |
| Shadow-line groove on the lid just above the seam, interrupted at the snap fingers | `seam_reveal`, `reveal_d`, `reveal_h` | on, 0.45 deep × 0.5 tall |

The tray and the lid are both cut from one `outer_solid()`, so the seam lines up exactly.

Deliberate departures from the reference:

- **Flat top and bottom.** The reference's top and bottom are concave too. Here they are flat:
  - The lid prints top down and carries the logo inlay.
  - The tray prints floor down.
  - A flat bed face prints cleanly and needs no support.
- **Flat front face** (`front_sag = 0`): the flange sits flush in it, and a concave front would leave a visible gap
  around the flange. The flange covers most of that face anyway.
- **Corner facets of 5 mm**, slightly smaller than the reference's ~15 %. The limit is the front bottom corners. There,
  the floor, the front skin and the side wall meet beside the open DB9 face, and a 5 mm facet leaves 1.27 mm square to
  the facet.

What the style changed underneath:

- **Wall thickness:**
  - Side walls are 4.0 mm at the corner ends (`side_t`), so that the deepest point of the bow keeps about 3.3 mm.
  - The rear wall is 0.8 mm thicker at its ends, so that the USB relief keeps its 1.6 mm depth at the middle of the
    concave rear face. The relief floor is still flush with the receptacle mouth.
- **Height:** the lid top is set so the 45° top chamfer leaves 1.2 mm over the USB overmould relief. That alone would put it 19.95 mm
  above the board; the Dupont wires' turning space (ceiling at least 23.4 mm above the PCB top) now raises it to 25.0 mm. The top skin over the XIAO stays a single 1.6 mm `ceil_t`, so the antenna still has only a thin
  skin over it.
- **Snap fingers:** every snap dimension is measured from `lc_face()`, the lowest point of the bowed side face over that
  clip.
  - Fingers are never thinner than 1.4 mm, and the wall behind each pocket is never thinner than 1.7 mm.
  - The rear fingers sit on a sloping facet, so they taper from 1.4 to about 1.8 mm across their width.
- **Snap catches:** these are through-windows in the tray wall instead of 0.85 mm blind recesses, because a blind
  recess would leave under 1.2 mm behind it.

## How it works

### What the first print showed (revision 2)

The first print (commit 731d0fb, PETG Basic) was test-fitted. The board-lock leg-tail pockets had been placed at the
lug screw-hole position (`lug_hole_y` 4.6), but the leg tails (the spikes under the PCB) are really 8.9 mm (as first measured on the print) behind the
flange front. The ghost legs used the same parameter, so the interference check could not see the error. With the
board pushed 4.3 mm forward so the spikes drop into the pockets, the model reproduces what the user saw: the flange
stands 4.3 mm in front of the case front face, the tray below it is open at the front while the lid front wall wraps
the top of the DB9 body, and the XIAO front end no longer lies on the DB9 body (its front edge at Y 10.5 is 2.3 mm
behind the moved body end at 8.2), so the side clamps carried it alone. The displacement does not explain the side
clips: in the displaced model they still reach the PCB edge (the −X hook ends level with the header's outer face,
and the rear +X clip overhangs the moved PCB's rear corner by 0.55 mm). They were simply small (0.5 mm hooks on 1.0 mm
fingers) and bent across the layer lines. This revision:

- puts the flange flush in the case front and holds it with lips and the lid front wall;
- gives the spikes their own parameter (`leg_y`, now measured to the centre: 9.5) and makes their pockets loose;
- adds a rigid backstop behind the board;
- replaces every flexing finger inside the case (RS232 board side clips and XIAO side clamps) with rigid features;
- rounds the USB-C opening;
- fixes the lid pressers, which in the first version touched the lid only along a 0.01 mm strip at the split and so
  started in mid-air in print orientation. They now grow out of the lid side walls.

### DB9 and the front face

- **Measured:**
  - Flange 30.8 × 12.5 × 0.38 mm steel, bottom level with the PCB top (`fl_bot = 0`), so the flange and D-shell
    centre are 6.25 mm up. D-shell 16.9 × 8.4, projecting 6.0 mm, so its outline runs from z 2.05 to 10.45.
  - Rear body 17.2 wide × 10.4 high, ending 12.5 mm behind the flange front (`body_end`).
  - Each side of the body, a plastic lug 3.4 high runs out to the full PCB width and back to Y 12.0 (`lug_end`).
  - The PCB front edge is flush with the flange front face, so the PCB runs under the flange.
  - Board-lock legs: soldered metal legs through the lugs, with round tails 3.1 mm across sticking out 3.0 mm below
    the PCB, centres at Y 9.5 (`leg_y`, measured with calipers) and about 24.9 mm apart (`lug_hole_x = 12.45`, approximate).
    `lug_hole_y` (4.6) is now only the screw-hole position on the ghost.
- **Flush front.** The case front face is at Y 0, flush with the flange front face. The flange sits in the front
  opening between the side walls (0.55 mm each side). Above the flange (from z 12.8, `fl_clr_z` 0.3 above it) the
  lid front face fills forward to Y 0. Below the flange the PCB front edge is flush with the tray front skin and floor.
- **Rear bearing face.** The lid front wall behind the flange (from Y 0.38, 3.2 mm deep) is what the flange back
  bears on. It takes the USB plug insertion force (plug → XIAO → front strut → lid front wall → flange). Its bottom
  front edge has a 0.5 mm 45° lead-in (`fw_lead`), so a board sitting slightly back is pushed forward onto the lips as
  the lid closes.
- **Retention lips** (`lip_*`). Two lips stand in front of the flange:
  - **Top lip** (lid): over the flange's top margin, from z 11.5 to the flange top, then up the front face.
  - **Bottom lip** (tray): over the PCB front edge and the flange's bottom margin, up to z 1.0.
  - Both span |X| ≤ 8 (`lip_x`), stand 1.3 mm proud of the front face (`lip_proud`) with a 0.1 mm gap to the flange
    (`lip_gap`), so they are 1.2 mm of plastic, and reach 1.0 mm over the flange (`lip_reach`). Each has a 0.3 mm
    lead-in on its back edge (`lip_lead`).
  - They stay 1.05 mm clear of the D-shell outline (the model warns below `lip_ds_clr` = 1.0) and well clear of the
    flange holes at X ±12.5, where the printer's jackposts come within about 1 mm of our flange at full mating.
    Mating stops when our D-shell tip meets the printer's flange, about 6 mm in front of ours, so 1.3 mm of lip does
    not reduce plug penetration.
  - With the lid front wall behind, the lips fix the flange (and with it the board and the loose shield) fore-aft with
    0.1 mm of play. Pulling a USB plug out (6–20 N) goes XIAO → lid rear wall → lid → top lip and snaps → tray →
    bottom lip → flange.
  - The user lost the shield when the mounting nuts were off. The lips hold the shield's flange over the middle 16 mm
    at top and bottom; the screw holes stay free so the nuts can go back on.
- **Leg-tail pockets** (`leg_pocket_on = true`): a secondary lock now, not the datum.
  - Slots at Y 9.5 with 0.6 mm clearance per side in Y (`leg_clr_y`) and 0.3 + 1.0 mm per side in X, a 0.6 mm 45°
    lead-in, in bosses standing up to 0.2 mm below the PCB. The floor under each pocket is 1.3 mm (minimum 0.8, the
    model warns below).
  - The side ledges now start behind the bosses (Y 13.45, `ledge_y0`), because the pocket lead-ins would otherwise
    leave 0.1–0.2 mm slivers of ledge over them.
  - The front lid snap catch windows (Y 3.7–10.3) end 0.6 mm above the boss tops; the model warns if they meet.
  - The red through-hole pin keep-out is cut back around the bosses (plus the board's 0.25 mm fore-aft play), which
    assumes no other pin within about 2 mm of each leg.
- **Not measured:** whether there is a step behind the flange. The user mentioned none, so `step_on = false`.
- **Renders:** `front_flush.png`, `front_elevation.png`, `section_lips.png` (X 0 plane through both lips),
  `section_leg_pockets.png`, `section_leg_pocket_length.png`.

### RS232 board

- **Board:** 31.5 × 26.0 × 1.6 mm, measured.
- **Support:** the board sits on a 1.2 mm front skin under its front edge and on 1.2 mm side ledges from Y 13.45.
  On the −X side the ledges are relieved under the header pins. A 0.4 mm gusset fills the corner between the floor
  and the front skin, keeping 1.2 mm square to the bottom front chamfer.
- **Backstop** (`bs_*`): a rigid 1.6 mm rib across the tray floor, 0.25 mm behind the PCB rear edge (Y 26.25–27.85),
  rising to 1.0 mm above the PCB top. Across the +X 5V/GND wire zone (X 5.7–11.8) it stops level with the PCB top.
  The 0.25 mm clearance means it never fights the flange datum (the lips and the lid front wall); it stops the board
  going back if it is pushed in the tray before the lid is on, and catches it if anything else gives.
- **Side clips removed.** The flexing tray clips that caught the PCB edge (1.0 mm fingers grown along Z, bending
  across the layer lines) were removed, not enlarged. The board is held down by rigid lid pressers instead.
- **Pressers:** ribs from the lid side walls come down to 0.1 mm above the board edge, covering 1.2 mm of it: −X at
  Y 13.8 (between the lugs, which end at Y 12.0, and the header, which starts at 15.6), +X at Y 13.8, 17 and 21 (moved from 24: nothing may press on the PCB top within 3 mm of its rear
  edge, `pcb_rear_keepout`, where the board has resistors; an orange keep-out ghost enforces it). Below
  the split they stand 0.2 mm off the tray wall; above it they are rooted in the lid wall with a 45° top.
- **Clear space:** there is 3 mm clear below the board. The red ghost volume shows the through-hole pin zone, kept
  1.25 mm in from the board edges.
- **Render:** `section_backstop.png`.

### XIAO

The XIAO dimensions come from Seeed's own XIAO ESP32-C6 v1.0 KiCad PCB, plus the user's measurements:

- outline 20.955 × 17.78 mm (measured 21.0), 22.5 mm including the USB-C (`usb_over` 1.5)
- PCB 1.2 mm thick (measured)
- USB-C 3.3 mm above the PCB top (4.5 overall); shield 2.2 mm above it (3.4 overall)
- pad 1 at 2.921 mm from the USB end, 2.54 mm pitch
- shield 12.6 × 10.6 mm at d 6.71–17.31
- ceramic antenna at X +0.5..+5.7 and U.FL at X −5.6..−3.0, both at the antenna end
- buttons at X ±5.3..7.9, d 0.3–1.9

(d = distance from the USB end.)

**Fore-aft position: exactly 2.0 mm on the DB9 body** (`xiao_rest = 2.0`). That gives the antenna edge at Y 10.5 and
the USB edge at Y 31.46, 5.46 mm past the RS232 PCB. The D4/D5 underside solder-joint zone starts 1.56 mm behind the
body end.

**Capture: all rigid, no flexing parts.** The side clamps of the first version broke on first use in PETG Basic: they
were fingers grown along Z, bending across the layer lines. They are gone. The XIAO is now boxed in by fixed features
with small gaps:

- **Below, front:** the top of the DB9 rear body (2.0 mm of the PCB lies on it). The model warns if the overlap drops
  below `front_rest_min` (2.0); there is no lid fallback any more.
- **Below, rear:** the tray tongue under the overhanging USB-C shell. Its top is a cradle shaped to the shell's
  stadium section, 0.05 mm under the shell bottom (`tongue_gap`) and 0.35 mm off the rounded ends
  (`tongue_side_clr` 0.3 + 0.05), so the guides, not the cradle, locate the XIAO in X. The tongue's front face is
  0.2 mm behind the PCB rear edge.
- **Above:** two rigid pads from the lid ceiling, 0.1 mm (`xpad_gap`) above:
  - the RF shield top: a 1.6 mm wall ring over the shield outline, inset 0.5 mm (X ±5.8, Y 14.65–24.25);
  - the USB-C shell's flat top (X ±2.8, from 0.5 mm behind the shell front to the rear wall). This is the support
    above the USB-C port: a plug pushed in at an angle now bears on the case.
  - Both stay clear of the antenna, the U.FL socket, the buttons and the wire zones.
- **Sides:** rigid guide walls hanging from the lid along the unused-pad stretches (where the clamps were), 1.6 mm
  thick, 0.2 mm (`guide_clr`) from the PCB edge, down to 0.6 mm below the PCB underside, with a 0.8 mm 45° lead-in at
  the tip (`guide_lead`), so the closing lid steers a XIAO that is up to 0.8 mm off in X into place.
  - −X: only d 2.2–5.16 (Y 26.3–29.26) remains of the d 2.2–10.5 stretch (D0–D3). The rest is cut out for the Dupont
    plug's wire-bend zone (header Y 15.64–25.8, plus 0.5 mm, `dupont_clr`); the model echoes the remaining segments.
  - +X: d 11.8–18.8 (D9–D7), clear of the 5V/GND wires; it starts just behind the DB9 body (Y 12.7).
  - The model warns if a guide comes within a pad and a half of a used pad.
- **Front:** the stop strut at X −1.25 (3 mm wide, in the gap between the U.FL socket and the ceramic antenna), 0.2 mm
  in front of the antenna-end PCB edge, from the lid front wall. It now has a 0.4 mm 45° lead-in on its bottom rear
  edge (`stop_lead`), so it pushes a XIAO that sits too far forward back as the lid closes. Because the XIAO lies on
  the body, the strut stops 0.1 mm above the body top and bears on the top 0.7 mm of the 1.2 mm edge.
- **Rear:** the lid rear wall, 0.2 mm behind the PCB rear edge beside the USB slot.
- **Renders:** `section_xiao_length.png` (X 0 plane: body, strut, shield ring, USB pad, tongue),
  `section_xiao_guide_plusx.png` and `section_xiao_guide_minusx.png` (guides and shield pad), `section_usb_pad.png`.

**Wire zones.** The space under the XIAO is kept clear for the four wires and their joints. The model carries magenta
keep-out volumes, and the interference check fails if any case feature enters them:

- under the −X D4/D5 pads
- under the +X 5V/GND pads (the backstop rib is cut down to the PCB top here)
- from the header top forward to D4/D5
- across under the XIAO from the header to 5V/GND

### USB-C opening

- **Plug relief:** the 12 × 7 mm overmould relief in the rear face, 1.6 mm deep, now has 2.5 mm corner radii
  (`usb_recess_r`). Its floor is flush with the receptacle mouth, so a fully seated plug bears on the case. A plug
  overmould larger than 12 × 7 mm will not seat fully; increase `usb_recess_w`/`usb_recess_h` if needed.
- **Receptacle slot:** the slot through the rear wall follows the USB-C shell's stadium section (r 1.65 + 0.2 mm
  clearance) at the top, so its visible ends are round. It stays open down to the lid rim so the closing lid passes
  over the receptacle; the tray tongue fills it from below, and its cradle rounds the lower corners. The tongue's
  edges beside the cradle rise to z 11.8 and are at least 0.5 mm wide (`tongue_tip`).
- Behind the relief floor the tongue is cut square (its rounded corners would otherwise leave knife-edge wedges on the
  tongue).
- **Renders:** `rear_usb.png` (with the XIAO) and `rear_usb_case_only.png`.

### Wires

There are no dedicated channels, because the interior is left open for the wires:

- **D4/D5:** these wires rise from the header on the −X side and reach their pads along the −X side, in front of the
  −X guide.
- **5V/GND:** these wires cross under the XIAO, between it and the RS232 board, and come up the +X side to their pads,
  behind the +X guide.

The green ghost wires show one feasible route, and the interference check confirms that no case feature crosses it.

### Lid-tray snaps

- **Fingers:** there are two snap fingers per side (Y 7 and 27.5), 1.4 mm thick and 6 mm wide, flush with the outside.
  - The free length is 9 mm: 4 mm is slotted into the lid wall, and 5 mm hangs below the split.
  - The bending strain at full hook deflection is about 1.6 %.
- **Catch:** the hooks catch on the top edge of a window through the tray wall, in pockets on its outer face. The
  window opens below the RS232 board, beside the ledge.
- **Opening:** there is a 0.9 mm pry gap below each finger tip. Push a thin screwdriver in and lever the finger
  outwards.
- **Unchanged in revision 2, and the next likely failure.** These fingers are also grown along Z and bend across the
  layer lines, the same weakness that broke the XIAO clamps and the board clips. They are thicker (1.4 mm) and
  longer (9 mm), at 1.6 % strain, but open and close them gently and expect them to be the first thing to crack.

### Dupont plug on the RS232 header

A Dupont housing is plugged onto the RS232 board's 4-pin header (−X edge, Y 15.64–25.8), and its wires bend over to
the XIAO pads. Revision 2's −X XIAO guide sat right where they bend. The model now carries a keep-out (cyan/grey
ghost, `ghost_dupont()`), and the interference checks fail if any case feature enters it:

- **Housing:** 10.16 × 2.54 mm on the header, up to `dupont_top` = 16.7 mm above the PCB top (measured: 18.3 mm
  above the PCB underside).
- **Wire-bend zone:** above the housing up to the ceiling, across in +X to the XIAO's −X edge (X −8.89), over the
  header's length. The wires need 25 mm of turning space measured from the PCB underside (the user's measurement), so the
  ceiling is at least `dupont_ceiling_min` = 23.4 mm above the PCB top (and at least `dupont_bend_h` = 4.0 above the
  housing). With the 1.6 mm top skin that puts the outside top at 25.0 mm, exactly `max_above`; there is no margin
  left in the envelope, and the model asserts it.
- The −X guide is cut back to d 2.2–5.16 (see XIAO). The ghost wires now leave the top of the housing.
- **Moved 1 mm** (`ghost_dupont_d`): +X and down stay clear. −X meets the side walls (the housing is 0.5 mm from the
  wall), −Y meets the −X board presser (0.84 mm away), +Y meets the remaining −X guide segment, and up meets the
  ceiling (the zone runs to it by definition).
- `renders/section_xiao_guide_minusx.png` cuts through the housing and the zone.

### Logo

- **Mark:** `assets/logo/zebserial-mark-lid.svg`, a bolder, simpler variant from `make_logo.py` for the lid: the
  D-shell with two wide stripes (instead of three tapered ones) and two constant-width WiFi arcs with round caps, every
  corner rounded. The web and marketing SVGs are unchanged.
- **Size:** 30 mm wide (`logo_w`), about 28 mm tall, centred on the lid top with about 1.5 mm to the edges of the top
  flat. At this size every stroke and gap is at least 1.2 mm (three 0.4 mm lines, `logo_min`). `part="check_logo"`
  checks it: what is thinner than `logo_min` survives "shape minus its opening", and narrower gaps survive "closing
  minus shape"; both are empty. The same mark at 20 mm fails (its 1.5-unit corner fillets fall under 0.6 mm), and
  passes at 22 mm.
- **Why:** the first print's 22 mm deboss had 0.8 mm features and sharp corners as gaps in the first layers on the
  bed, which strung badly and did not read.
- **Two-colour inlay (default, `logo_mode = "inlay"`):** `part="logo"` exports the logo body, `logo_depth` 0.6 mm
  thick, in lid print orientation and lid coordinates (`out/logo.stl`). The same body is cut from the lid with no
  clearance: the lid's volume drops by exactly the inlay's 0.280 cm³, and the two do not overlap.
- **Single-colour fallback:** `logo_mode = "deboss"` cuts the same bold mark 0.6 mm deep; `"none"` leaves the top plain.
- **Orientation:** the mark reads upright when you look down on the assembled case with the DB9 at the bottom edge,
  nearest you. The WiFi arcs point towards the USB-C end. `renders/lid_top_logo.png` (inlay, white) and
  `lid_top_logo_deboss.png` show it.
- **Skin:** the top skin is 1.6 mm (`ceil_t`), so 1.0 mm of lid remains under the 0.6 mm logo. The model asserts at
  least 1.0 mm. Nothing inside the lid comes above the ceiling.

### Fit coupons

`part="coupon_tray"` and `part="coupon_lid"` (`out/coupon_tray.stl`, `out/coupon_lid.stl`) are the front of each part,
cut flat at `coupon_y1` = 14.5 mm, in normal print orientation. They contain the flange recess, both lips, the lid
front wall that the flange bears on, the leg-tail pockets at Y 9.5, the front pair of lid snaps (Y 7), the front
pressers and the start of the side ledges, so the DB9 board can be test-fitted for about 9 g of PETG. The lid
coupon has no logo (single colour). The cut stops at 14.5 rather than 15 because the shield pad ring starts at
Y 14.65 and would leave a 0.35 mm slice. Volumes: tray coupon 1.957 cm³, lid coupon 5.018 cm³ (the lid coupon is now 26 mm tall). Both are single
pieces; the wall probe finds only the known 1.19 mm gusset and the snap hooks in them.

## Assembly order

1. **Board into the tray.** Lower the RS232 board (DB9 fitted, shield on) straight down, with the flange just behind
   the bottom lip. The leg tails drop into their pockets, the PCB lands on the front skin and the ledges, and its rear
   edge comes down in front of the backstop rib. The D-shell passes over the bottom lip (1.05 mm clear at the end).
2. **XIAO onto the board.** The wires are short and soldered, so keep it tethered:
   1. Plug the Dupont housing onto the header if it is not on yet. Fold the wires: they bend over the housing
      towards +X; D4/D5 go to their pads on the −X side, 5V/GND down beside the XIAO and under it to +X.
   2. Lay the XIAO's antenna end 2 mm onto the DB9 body top and its USB-C shell into the tongue cradle, with the PCB
      rear edge just in front of the tongue.
3. **Close.** Lower the lid straight down:
   - the top lip passes in front of the flange (0.3 mm lead-in) and the front wall comes down behind it (0.5 mm
     lead-in);
   - the guides' 0.8 mm lead-ins centre the XIAO in X, the strut's lead-in pushes it back if it sits forward, and the
     lid's USB slot comes down over the receptacle;
   - the pads stop 0.1 mm above the shield and the USB shell, the pressers 0.1 mm above the board edges;
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

- the flange back on the lid front wall (Y 0.38; it shows from z 4.5, above the front wall lead-in)
- the PCB on the front skin and the ledges
- the lid rim on the tray rim

**Controls** (`./controls.sh`, 42 lines, all must pass; about 80 s). Each moves one ghost and states whether a check must find
real overlap (any piece of at least 0.01 mm³) or none:

| Moved | Check | Result |
|-------|-------|--------|
| board + DB9 forward 0.05 | tray, lid | none (lips 0.1 in front) |
| board + DB9 forward 0.15 | tray / lid | hits the bottom lip / the top lip |
| board + DB9 forward 1.2 | tray | hits the bottom lip |
| board + DB9 back 0.2 | tray | none (backstop 0.25 behind) |
| board + DB9 back 0.2 | lid | hits the lid front wall (the datum, zero gap) |
| board + DB9 back 0.3 and 0.5 | tray | hits the backstop |
| leg tails ±0.5 in Y | tray | none |
| leg tails ±0.7 in Y | tray | hits the pockets |
| leg tails +0.9 in X | tray | none |
| XIAO up 0.05 | lid | none |
| XIAO up 0.15 and 0.3 | lid | hits both pads |
| XIAO down 0.04 / 0.06 | tray | none / hits the tongue |
| XIAO ±X 0.15 | tray, lid | none |
| XIAO ±X 0.3 | lid | hits a guide (and the USB slot side) |
| XIAO forward 0.3 / back 0.3 | lid | hits the strut / the rear wall |
| a −X guide at d 13–17, Dupont off | lid | hits the D4/D5 wire zone |
| Dupont keep-out +X 1 / down 1 | lid | none |
| Dupont keep-out −X 1 / −Y 1 / +Y 1 / up 1 | lid | hits the side wall / −X presser / −X guide / ceiling |
| −X guide not trimmed (`dupont_clr=-20`) | lid | hits the Dupont zone |
| +X presser back at Y 24 | lid | hits the rear-edge keep-out |
| logo at 30 mm / 20 mm | `check_logo` | none / hits |
| test block 0.5 under the XIAO edge (`ctl_undercut`) | lid / lid path | none / hits (2.0 mm³) |

Moving the board also moves the red pin-zone ghost against the leg-tail bosses; its cut-out allows the board's
0.25 mm of play, so those overlaps appear only beyond it (for example 0.62 mm³ at 0.3 mm back) and are a property of
the assumed keep-out, not a collision.

For `check_lid_path`, the control `-D ctl_undercut=true` adds a test block to the lid 0.5 mm under the XIAO's +X
edge. It is clear in the closed position (`check_lid` stays empty) but the swept check finds it.

**Wall thickness:** `build.sh` also exports both parts in assembly position (`part="tray_inplace"`/`"lid_inplace"`) and
runs `wallprobe.py`, which casts a ray inwards from sample points on every face and lists everything under 1.2 mm.
What remains is deliberate:

- the lid snap hooks (0.84 and 1.02 mm)
- the 1.0 mm skin under the logo pocket (the inlay fills it in the two-colour print)
- the tongue's top edges beside the USB cradle (0.93–0.95 mm; they taper to the 0.5 mm `tongue_tip` at the top). They
  only round the opening and carry nothing: the shell rests on the cradle bottom, 0.35 mm off these edges.
- 1.19 mm at the front bottom floor: the floor / front skin gusset, 1.2 mm by construction square to the 45° chamfer

The probe found and fixed, in this revision: knife-edge wedges where the rounded relief corners met the tongue
(0.04 mm), ledge slivers over the moved leg-pocket lead-ins (0.09–0.19 mm) and 0.9 mm presser ribs (now 1.2).

The probe casts rays along face normals, so it does not measure distance square to a 45° face. Checked by hand:

- the front bottom corner facets (1.27 mm)
- the bottom front chamfer against the front skin and gusset (1.2 mm)
- the top chamfer over the USB relief (1.2 mm, by construction of `z_top`)

## Fit-check list (measure before printing)

Measured and applied (2026-10-08): leg-tail centres at Y 9.5; Dupont top 18.3 mm above the PCB underside with
25 mm of wire turning space; presser footprints clear; resistors near the PCB rear edge (3 mm keep-out); flange
margins flat. Still open:

- **Flange and D-shell:** 30.8 × 12.5 × 0.38 flange, bottom level with the PCB top; D-shell outline 2.05–10.45 above
  the PCB top. The lips assume these (1.05 mm clear of the D-shell top and bottom). If the D-shell is taller, reduce
  `lip_reach`.
- **Board flush:** the PCB front edge flush with the flange front face (`pcb_front_y = 0`).
- **Lips and the printer:** nothing on the printer side should stand within 1.3 mm of our flange over the middle
  16 mm (|X| ≤ 8) above and below the D-shell.
- **USB-C shell:** 8.94 × 3.3 stadium, overhanging the XIAO PCB by 1.5 mm. The cradle and pads depend on it.
- **Step:** the user mentioned no step behind the flange, so `step_on = false`.

## Concerns to check before printing

- **Snaps:** see above; they are the remaining across-layer flexing parts.
- **Antenna proximity:** the front strut runs to 0.2 mm from the XIAO's antenna end, 0.25 mm from the ceramic antenna
  in X, and the DB9's metal flange and shell are not far away. If WiFi range is poor, use the U.FL socket.
- **Lip strength:** the lips are 1.2 mm thick and also bend across layer lines if the flange is pulled forward hard.
  They are short (the bottom one stands 2.6 mm above its root, the top one 1.3 mm) and only carry plug extraction
  force, but do not lever the case off the DB9 by the lips.
- **Light gap at the snaps:** the catch windows open into the space under the board. A sliver of light shows below
  each snap finger tip, where the pry gap meets the window.
- **Strut engagement:** the front strut bears on the top 0.7 mm of the 1.2 mm XIAO edge (the PCB rests on the DB9
  body, and the strut has a 0.4 mm lead-in).
