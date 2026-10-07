# ZebSerialWifi enclosure

A two-part PETG case (tray + lid) for the YL-97 MAX3232 RS232-TTL board with its right-angle male DB9, and a Seeed
XIAO ESP32-C6 mounted above it. The DB9 flange and D-shell stay outside the case, so the unit plugs straight into the
printer. The two parts snap together, with no screws.

| File | What |
|------|------|
| `zebserial_case.scad` | The model. All dimensions are top-level parameters, grouped as *given*, *XIAO (Seeed KiCad/STEP)*, *DB9 (manufacturer drawings)*, *header guesses*, *print/tolerance*, *style* and *clips*. |
| `reference/` | Not in git (gitignored): the manufacturer drawings and the Seeed XIAO ESP32-C6 KiCad PCB and STEP the dimensions come from. |
| `out/tray.stl`, `out/lid.stl` | Print-ready, already in print orientation. |
| `renders/*.png` | Assembly, exploded, sections, exterior style (`style_*.png`) and print-orientation views. |
| `build.sh` | Regenerates the STLs and renders, then runs the interference checks. Extra `-D` overrides apply to the checks, for example `./build.sh -D xiao_overhang=3`. |
| `stlinfo.py` | Reports STL volume and bounding box, and with `--components` the overlapping pieces. |
| `wallprobe.py` | Measures local wall thickness by ray casting from every face; `build.sh` lists everything under 1.2 mm. |

## Coordinates

- **Y:** 0 is the DB9 flange front face, and +Y runs towards the rear (the USB-C end). The case front face is the
  flange back (Y 0.38). The RS232 PCB runs from Y 0 (flush with the flange front, under the flange) to Y 26.
- **Z:** 0 is the RS232 PCB top surface.
- **X:** looking down with the DB9 nearest you:
  - −X is the side with the RS232 header and the XIAO D0..D6 pads.
  - +X is the side with the XIAO 5V/GND..D7 pads.

## Print settings (Bambu H2D, 0.4 mm nozzle, PETG)

- **Layer height:** 0.2 mm.
- **Walls:** 4 (1.6 mm).
- **Infill:** 25 % gyroid. Most of the part is walls anyway.
- **Supports:** none.
- **Brim:** none needed. The parts are 40 × 37 mm.
- **Orientation:** the STLs are already oriented.
  - Tray: floor on the bed (open side up).
  - Lid: top skin on the bed (open side up).
  - The spec's line "both print open side down" is wrong for the tray. Printed that way, the whole floor would be one
    bridge.
- **Overhangs:** the steepest unsupported features are the flat catch faces of the snap hooks, at 0.5–0.75 mm. These are
  fine in PETG. Everything else is either vertical, at 45°, a short bridge, or opens upwards in print orientation:
  - The perimeter chamfers on the bed side of each part are 45°.
  - The top of each snap catch window in the tray is a 6.6 mm bridge.
  - The USB slot opens to the lid rim. The optional leg-tail pockets open upwards with a 45° lead-in.
  - The ledges and lips on the XIAO clamps have 45° faces where they would otherwise overhang.
- **Size:** 39.9 × 35.0 × 26.1 mm assembled (the case shell, from the flange back). The top is 19.9 mm above the RS232
  board, against a 25 mm limit.
- **Material:** about 4.9 cm³ for the tray and 8.0 cm³ for the lid. That is roughly 16.5 g of PETG at 100 %, less with
  infill.

## Exterior style: faceted pincushion

The outside copies the look of the user's factory Radar360 case. The look is defined in
`~/personal_scripts/ai-config/instructions/enclosure_styles.md`. `renders/style_plan.png`, `style_iso.png` and
`style_side.png` show it. I compared them with the reference STLs, rendered from the same cameras. On the
reference, the "triangular corner facets" are corner truncations rather than tall vertical chamfers, so the model
follows that.

| Feature | Parameter | Value |
|---------|-----------|-------|
| Side and rear faces bow inwards at mid-span (2 % of each face's length) | `sag_frac` | 0.02: 0.74 mm on the sides, 0.80 mm on the rear |
| Each face is three flat facets: a flat middle third, sloping to the corners | `facet_flat` | 0.3 (0 gives a two-facet V) |
| Each of the 8 corners cut off by a flat plane square to the body diagonal, leaving a triangular facet in plan, side and front views | `corner_facet` | 5.0 mm along each edge (12.5 % of the width) |
| Small 45° bevel on the vertical corner edges between the facets | `corner_chamfer` | 1.5 mm |
| Top and bottom perimeter edges chamfered at 45°, so the side profile is an elongated octagon | `edge_chamfer` | 1.5 mm |
| Shadow-line groove on the lid just above the seam, interrupted at the snap fingers | `seam_reveal`, `reveal_d`, `reveal_h` | on, 0.45 deep × 0.5 tall |

The tray and the lid are both cut from one `outer_solid()`, so the seam lines up exactly.

Deliberate departures from the reference:

- **Flat top and bottom.** The reference's top and bottom are concave too. Here they are flat:
  - The lid prints top down and carries the logo deboss.
  - The tray prints floor down.
  - A flat bed face prints cleanly and needs no support.
- **Flat front face** (`front_sag = 0`):
  - It is the datum the DB9 flange sits against, and it is what stops the case sliding back along the connector. A
    concave front would leave the case free to move about 0.8 mm.
  - The flange covers most of that face anyway.
- **Corner facets of 5 mm**, slightly smaller than the reference's ~15 %. The limit is the front bottom corners. There,
  the floor, the front skin and the side wall meet beside the open DB9 face, and a 5 mm facet leaves 1.27 mm square to
  the facet.

What the style changed underneath:

- **Wall thickness:**
  - Side walls are 4.0 mm at the corner ends (`side_t`, was 3.2), so that the deepest point of the bow keeps the old
    3.26 mm.
  - The rear wall is 0.8 mm thicker at its ends, so that the USB relief keeps its 1.6 mm depth at the middle of the
    concave rear face. The relief floor is still flush with the receptacle mouth.
- **Height:** the lid top is set so the 45° top chamfer leaves 1.2 mm over the USB overmould relief. That puts it 20.7 mm
  above the board with the real DB9 and XIAO heights. The top skin over the XIAO stays a single 1.6 mm `ceil_t`, so
  the antenna still has only a thin skin over it. The XIAO side clamps hang from that ceiling; their 1.6 % strain
  already allows for the length.
- **Snap fingers:** every snap dimension is measured from `lc_face()`, the lowest point of the bowed side face over that
  clip.
  - Fingers are never thinner than 1.4 mm, and the wall behind each pocket is never thinner than 1.7 mm.
  - The rear fingers sit on a sloping facet, so they taper from 1.4 to about 1.8 mm across their width. That is
    slightly stiffer, at about 1.6 % strain.
- **Snap catches:** these are now through-windows in the tray wall instead of 0.85 mm blind recesses, because a blind
  recess would leave under 1.2 mm behind it.

## How it works

### DB9 location

- **Measured:**
  - Flange bottom level with the PCB top (`fl_bot = 0`), so the flange and D-shell centre are 6.25 mm up.
  - Rear body 17.2 wide × 10.4 high, ending 12.5 mm behind the flange front (`body_end`).
  - Each side of the body, a plastic lug 3.4 high runs out to the full PCB width and back to Y 12.0 (`lug_end`).
  - The PCB front edge is flush with the flange front face, so the PCB runs under the 0.38 mm flange. It sticks out
    0.38 mm ahead of the case front face, below the flange.
  - Board-lock legs: the lug "screw holes" are filled by soldered metal legs at Y 4.6. Their tails are round, 3.1 mm
    across, and stick out 3.0 mm below the PCB, so their tips reach the tray floor top. The leg centres are about
    24.9 mm apart (`lug_hole_x = 12.45`). That is approximate, so the pockets keep their ±1 mm extra in X.
- **Not measured:** whether there is a step behind the flange. The user mentioned none, so `step_on = false`.
- **Case front face:** the plane of the flange back. It stops the case moving forwards.
- **Anti-slide: leg-tail pockets** (`leg_pocket_on = true`). Each board-lock leg tail drops into a slot in the tray.
  That locks the board, and with it the case, fore-aft in both directions, so pulling a USB plug out (6–20 N) cannot
  slide the case off the connector.
  - **Slots:** 0.3 mm clearance in Y (fore-aft, the direction that matters) and 0.3 + 1.0 mm each way in X (the leg
    spacing is only approximately measured), with a
    0.6 mm 45° lead-in so the board drops in.
  - **Positive controls:** the legs shifted ±0.25 mm in Y give no contact; ±0.35 hits. The legs shifted ±0.9 mm in X
    give no contact.
  - **Bosses:** the tails end level with the floor top, so a pocket cut into the floor alone would be a 0.3 mm dimple
    with nothing beside the tail. Each slot therefore sits in a boss standing up to 0.2 mm below the PCB. The boss
    wall is 1.8 mm, so it is still 1.2 mm where the lead-in starts.
  - **Floor:** the slot goes 0.3 mm into the floor, below the tail tip, which leaves 1.3 mm of floor under it. That is
    above the 0.8 mm minimum, so no thickening was needed. The model `ECHO`s the figure and warns below 0.8.
  - **Keep-out:** the 3.1 mm tails and bosses sit outside the DB9 body (from X ±8.6). The red through-hole pin
    keep-out is cut back around the bosses, which assumes no other pin within about 2 mm of each leg.
  - **Renders:** `section_leg_pockets.png` and `section_leg_pocket_length.png`.
- **With the pockets off** (`leg_pocket_on = false`), only friction holds the case rearwards, and the model says so.
  The lid pins into the lug holes (`lug_pin_on`) stay off because the holes are filled; the code is kept for reference.

### RS232 board

- **Board:** 31.5 × 26.0 × 1.6 mm, measured.
- **Ledges:** the board sits on 1.2 mm side ledges, with a 1.2 mm front skin under its front edge.
  - The ledges start 7 mm behind the PCB front edge, behind the lugs.
  - On the −X side, the ledges are relieved under the header pins.
  - A 0.4 mm gusset fills the corner between the floor and the front skin. It keeps 1.2 mm of material square to the
    bottom front chamfer, and it stays outside the pin zone.
- **Clips:** cantilever clips in the tray side walls catch the PCB top edge.
  - Positions: −X at Y 13.9; +X at Y 13.9 and 20.5. They sit just behind the lugs (Y 12.0); the −X clip is also in
    front of the header (Y 15.6). The front lid snap moved forward to Y 7 to make room.
  - The clips are 3.5 mm wide with 0.5 mm side slots, so at least 1.2 mm of wall remains between each slot and the
    nearest snap pocket.
  - A clip that would collide with a lid-snap pocket is dropped, with an `ECHO` note. None is dropped now.
- **Pressers:** ribs on the lid (1.2 × 2 mm) come down to 0.1 mm above the board edge, to hold it down: +X at Y 17
  and 24. There is no room on −X: the lugs run to Y 12.0, the clip sits at 13.9 and the header starts at 15.6.
- **Clear space:** there is 3 mm clear below the board. The red ghost volume shows the through-hole pin zone, kept
  1.25 mm in from the board edges.

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

**Fore-aft position: exactly 2.0 mm on the DB9 body** (`xiao_rest = 2.0`; `xiao_front_y = -1` derives it, or set it
directly). That gives:

- the antenna edge at Y 10.5
- the USB edge at Y 31.46, which is 5.46 mm past the RS232 PCB

This is the shortest case with the 2 mm rest the user asked for, leaving more room for the wires under the XIAO.

- **Joint zone:** the D4/D5 underside solder-joint zone now starts 1.56 mm behind the body end.
- **Front support:** the body is the active front support, with nothing added by the lid.
  - At exactly 2.0 mm the model counts the body as enough (`front_rest_min` = 2.0, with a 0.01 mm tolerance).
  - I did not add the lid's D6 front rest for extra margin, because it would sit against the D4/D5 wire zone.
- **The model `ECHO`s:** the overlap, the joint-zone margin and the strut depth.

**Wire zones.** The user needs the space under the XIAO kept clear for the four wires and their joints. The model
carries magenta keep-out volumes, and the interference check fails if any case feature enters them:

- under the −X D4/D5 pads
- under the +X 5V/GND pads
- from the header top forward to D4/D5
- across under the XIAO from the header to 5V/GND

As a positive control, a clamp pushed into the D4/D5 zone gives about 18 mm³ of overlap.

It is supported on all sides:

- **Underneath at the front:** it rests on the top of the DB9 rear body, the way the user mounted it.
  - At the default, its front end lies exactly 2.0 mm onto the body.
  - If the overlap is under 2 mm (`front_rest_min`), the lid adds a front rest: a short −X clamp on the free D6 pad. It
    sits beside the front end of the +X clamp. The model `ECHO`s which front support is active.
  - The fallback (the lid's D6 rest) switches on when the overlap is under 2 mm, for example at `xiao_rest = 1.8`.
- **Underneath at the rear:** the tray tongue fills the rear-wall slot under the overhanging USB-C shell. Its top sits
  0.05 mm (`tongue_gap`) below the shell.
  - The tongue height follows `body_h + xiao_pcb_t`. Both are now measured (10.4 and 1.2).
  - The interference check proves the gap: moving the XIAO down 0.04 mm gives no overlap, and 0.06 mm hits the tongue.
- **Sides: two clamps hang from the lid.** Each is a 1.2 mm finger with a ledge under the PCB edge and a lip over it,
  overlapping the PCB by 0.8 mm each, with 0.1 mm clearance so the body and tongue stay the datums. They run along the
  unused-pad stretches only:
  - −X: d 2.2–10.5 (D0–D3). This keeps clear of the −X button (d < 1.9) and of the D4/D5 wires.
  - +X: d 11.8–18.8 (D9–D7). This keeps clear of the 5V/GND wires. Where the XIAO lies on the DB9 body the body carries
    it, so the clamp starts just behind the body (Y 12.6 at the default).
  - The KiCad PCB has no bottom-side parts within 7 mm of the long edges (only test pads near the centre), so the
    ledges bear on bare edge and pad copper.
  - Finger bending strain to snap past the 0.8 mm ledge is 1.6 %.
  - The model warns if a clamp comes within a pad and a half of a used pad.
  - `renders/section_xiao_clamp_plusx.png` and `section_xiao_clamp_minusx.png` cut through them.
- **Front stop:** one strut bears on the XIAO's front (antenna-end) PCB edge.
  - It sits at X −1.25, 3 mm wide, in the only straight-edge gap between the U.FL socket (to X −3.0) and the ceramic
    antenna (from X +0.5). It ends 0.25 mm from the antenna in X.
  - It runs from the lid front wall back to the XIAO edge, so USB insertion force puts it in compression.
  - Load path: plug → XIAO → strut → lid front wall → case front face on the flange back, with the snaps tying the lid
    to the tray. Pulling a plug out goes the other way: XIAO → clamps and rear wall → lid → snaps → tray → (with the
    leg pockets on) board → DB9; otherwise friction.
  - Because the XIAO front end lies on the body top, the strut cannot reach below the PCB. It stops 0.1 mm
    (`stop_z_clr`) above the body and bears on 1.1 of the 1.2 mm edge.
  - If the XIAO is set so far back that its front edge is clear of the body by 0.8 mm or more, a lip on the strut
    reaches below the PCB and it bears on the full edge.
- **Rear stop and USB:** the rear wall wraps the USB-C receptacle in a slot (receptacle + 0.2 mm per side). The slot is
  open downwards so the XIAO can be pushed into the lid.
  - Outside, a 12 × 7 mm plug-overmould relief is 1.6 mm deep, and its floor is flush with the receptacle mouth. A
    fully seated plug therefore bears on the case rather than on the XIAO. A plug overmould larger than 12 × 7 mm will
    not seat fully; increase `usb_recess_w`/`usb_recess_h` if needed.
- **Antenna:** the only material over the antenna end is the 1.6 mm top skin. The lid front wall ends at Y 3.6, about
  6.8 mm in front of the antenna edge. The front strut (X −1.25, 3 mm wide) runs from it to 0.2 mm from the PCB edge.

### Wires

There are no dedicated channels, because the interior is left open for the wires:

- **D4/D5:** these wires rise from the header on the −X side and reach their pads along the −X side, in front of the
  −X clamp.
- **5V/GND:** these wires cross under the XIAO, between it and the RS232 board, and come up the +X side to their pads.

The green ghost wires show one feasible route, and the interference check confirms that no case feature crosses it.

### Lid-tray snaps

- **Fingers:** there are two snap fingers per side (Y 7 and 27.5), 1.4 mm thick and 6 mm wide, flush with the outside.
  - The free length is 9 mm: 4 mm is slotted into the lid wall, and 5 mm hangs below the split. That keeps the catch
    windows above the leg-tail bosses.
  - The bending strain at full hook deflection is about 1.6 %, comfortable for PETG.
- **Catch:** the hooks catch on the top edge of a window through the tray wall, in pockets on its outer face. The
  window opens below the RS232 board, beside the ledge.
- **Opening:** there is a 0.9 mm pry gap below each finger tip. Push a thin screwdriver in and lever the finger
  outwards. A larger gap would bring the pocket floor within 1.2 mm of the bottom chamfer.

### Logo

- **Deboss:** the ZebSerial mark (`assets/logo/zebserial-mark-deboss.svg`) is cut 0.5 mm (`logo_depth`) into the lid's
  outer top face, centred, 22 mm wide (`logo_w`). `show_logo = false` leaves the top plain.
- **Orientation:** the mark reads upright when you look down on the assembled case with the DB9 at the bottom edge,
  nearest you. The WiFi arcs point towards the USB-C end. `renders/lid_top_logo.png` shows this view.
- **Skin:** the top skin is 1.6 mm (`ceil_t`), so 1.1 mm remains under the 0.5 mm deboss. The model asserts at least
  1.0 mm. Nothing inside the lid comes above the ceiling, so the deboss cannot meet a strut or clip.
- **Printing:** the lid prints top down, so the deboss is in the first 0.5 mm of layers and needs no support. At 22 mm
  wide the narrowest raised strip is about 1.0 mm (between the inner arc's ends and the shell) and the narrowest
  recess about 1.5 mm (the shell rim beside the stripes), against the 0.8 mm minimum for a 0.4 mm nozzle.

## Assembly order

1. **Tray:** press the RS232 board (with its DB9 fitted) down into the tray until the side clips snap over the PCB
   edge. With the leg pockets on, the board-lock leg tails drop into their pockets first. Its front edge goes against the front skin.
2. **XIAO into the lid:**
   1. Hold the lid upside down just above the tray. The wires are short and soldered, so keep the XIAO tethered.
   2. Seat the USB-C receptacle into the rear slot.
   3. Put the front (antenna) edge against the stop strut.
   4. Press the XIAO straight in until both side clamps snap over its edges. The lips stop it.
   5. If the XIAO rests less than 2 mm on the DB9 body (`xiao_rest` < 2), also snap the small D6 front rest.
3. **Close:**
   1. Flip the lid over the tray, folding the wires into the interior. D4/D5 go along the −X side, and 5V/GND go
      under the XIAO to +X.
   2. Lower the lid straight down. The XIAO's front end comes down onto the
      DB9 body, the tray tongue slides up under the USB shell, and the four snaps click.
4. **Check:** plug a USB-C cable in and out. Nothing should move.

## Interference checks

`build.sh` intersects:

- the ghosts with the tray
- the ghosts with the lid
- the tray with the lid

Each must have zero volume. Zero-thickness contact faces are designed contacts:

- the flange back on the case front face
- the PCB on the ledges
- the lid rim on the tray rim

**Wall thickness:** `build.sh` also exports both parts in assembly position (`part="tray_inplace"`/`"lid_inplace"`). It
runs `wallprobe.py`, which casts a ray inwards from sample points on every face and lists everything under 1.2 mm.
What remains is all deliberate, flexing or bearing-edge detail, not walls:

- the 1.0 mm board-clip fingers and their 0.3–0.5 mm hook tips
- the XIAO clamp ledges and lips (0.55–0.9 mm, and the 45° lead-in tips)
- the lid snap hooks (0.84–1.0 mm)
- the 1.1 mm skin under the logo deboss

The probe found and fixed these (most already existed before the restyle):

- 0.6–0.9 mm webs between board-clip slots and snap pockets
- a knife-edge cusp where the old DB9 socket met the body notch
- 0.5 mm over the USB relief
- a 0.1 mm strut tip at `xiao_overhang = 3`
- 0.4 mm webs at the ends of the seam groove

The probe casts rays along face normals, so it does not measure distance square to a 45° face. I checked those cases by
hand:

- the front bottom corner facets (1.27 mm)
- the bottom front chamfer against the front skin and gusset (1.2 mm)
- the top chamfer over the USB relief (1.2 mm, by construction of `z_top`)

A few probe readings come from rays grazing out of an edge, such as at a facet crease beside a snap finger, rather than
from thin material. These show up in sweeps at other `xiao_overhang` values.

**Positive controls** (move the XIAO ghost with `-D 'ghost_offset=[x,y,z]'`):

| Offset | Check | Overlap |
|--------|-------|---------|
| [1, −1, 0] | `check_lid` | about 11 mm³ (strut, +X clamp, USB slot) |
| [0, 0, −0.04] | `check_tray` | none |
| [0, 0, −0.06] | `check_tray` | hits the tongue |
| [0, 0, −0.09] | `check_lid` | none |
| [0, 0, −0.11] | `check_lid` | hits both clamp ledges |
| [0, 0, +0.09] | `check_lid` | none |
| [0, 0, +0.11] | `check_lid` | hits both clamp lips |

| `ghost_lug_dy` ±0.25 (moves only the ghost legs) | `check_tray` | none |
| `ghost_lug_dy` ±0.35 | `check_tray` | hits both leg pockets |
| `ghost_lug_dx` ±0.9 | `check_tray` | none (the slots tolerate an X error) |
| `ghost_lug_dx` 1.4 | `check_tray` | hits |
| `leg_tail_l=3.6` | — | floor-breach warning |
| an −X clamp at d 13–17 | `check_lid` | about 18 mm³ in the D4/D5 wire zone, plus a used-pad warning |

So the rear support and the clamps really sit at their 0.05 mm and 0.1 mm gaps, the leg pockets have 0.3 mm per side,
and the wire zones are enforced.

## Fit-check list

Everything the case depends on is now measured. The one open note:

- **Step:** the user mentioned no step behind the flange, so `step_on = false`. If there is one, set `step_on`,
  `step_h` and `step_d`, then rerun `./build.sh`.

## Concerns to check before printing

- **Antenna proximity:** the front strut runs to 0.2 mm from the XIAO's antenna end, 0.25 mm from the ceramic antenna
  in X, and the DB9's metal flange and shell are not far away. If WiFi range is poor, use the U.FL socket.
- **Board clip strain:** the board clips are 1.0 mm thick and bend from the floor top (about 5 mm free length). They
  work at roughly 3 % strain. The RS232 board is mainly held by the lid pressers. The clips only hold it during assembly.
- **Light gap at the snaps:** the catch windows open into the space under the board. A sliver of light shows below
  each snap finger tip, where the pry gap meets the window.
- **Board hold-down on −X:** there is no lid presser on the −X side; the −X clip and ledge hold that edge. The leg
  pockets also locate the board.
- **Strut engagement:** the front strut bears on 1.1 of the 1.2 mm XIAO edge, not the full edge, because the PCB rests
  on the DB9 body.
