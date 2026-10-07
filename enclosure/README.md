# ZebSerialWifi enclosure

A two-part PETG case (tray + lid) for the YL-97 MAX3232 RS232-TTL board with its right-angle male DB9, and a Seeed
XIAO ESP32-C6 mounted above it. The DB9 flange and D-shell stay outside the case, so the unit plugs straight into the
printer. The two parts snap together, with no screws.

| File | What |
|------|------|
| `zebserial_case.scad` | The model. All dimensions are top-level parameters, grouped as *given*, *Seeed data*, *standard DB9 defaults (not measured)*, *header guesses*, *print/tolerance* and *clips*. |
| `out/tray.stl`, `out/lid.stl` | Print-ready, already in print orientation. |
| `renders/*.png` | Assembly, exploded, sections and print-orientation views. |
| `build.sh` | Regenerates the STLs and renders, then runs the interference checks. Extra `-D` overrides apply to the checks, for example `./build.sh -D xiao_overhang=3`. |
| `stlinfo.py` | Reports STL volume and bounding box, and with `--components` the overlapping pieces. |

## Coordinates

- **Y:** 0 is the DB9 flange front face, and +Y runs towards the rear (the USB-C end).
- **Z:** 0 is the RS232 PCB top surface.
- **X:** looking down with the DB9 nearest you:
  - −X is the side with the RS232 header and the XIAO D0..D6 pads.
  - +X is the side with the XIAO 5V/GND..D7 pads.

## Print settings (Bambu H2D, 0.4 mm nozzle, PETG)

- **Layer height:** 0.2 mm.
- **Walls:** 4 (1.6 mm).
- **Infill:** 25 % gyroid. Most of the part is walls anyway.
- **Supports:** none.
- **Brim:** none needed. The parts are 38 × 36 mm.
- **Orientation:** the STLs are already oriented.
  - Tray: floor on the bed (open side up).
  - Lid: top skin on the bed (open side up).
  - The spec's line "both print open side down" is wrong for the tray. Printed that way, the whole floor would be one
    bridge.
- **Overhangs:** the steepest unsupported features are the flat catch faces of the snap hooks, at 0.5–0.75 mm. These are
  fine in PETG. Everything else is either vertical or opens upwards in print orientation:
  - The DB9 sockets and the USB slot open to the lid rim.
  - The lips on the XIAO clips have a 45° underside.
- **Size:** 38.3 × 36.1 × 23.8 mm assembled, with the default `xiao_overhang = 5`. The top is 18.2 mm above the RS232
  board, against a 25 mm limit.
- **Material:** about 4.6 cm³ for the tray and 8.2 cm³ for the lid. That is roughly 16 g of PETG at 100 %, less with
  infill. Not sliced, so print time is not estimated.

## How it works

### DB9 location

- **Case front face:** the case front face is the plane of the flange back.
- **Sockets:** the lid's front wall has two blind U-shaped sockets that drop over the hardware behind the flange screw
  holes.
  - Socket diameter: hardware diameter + 2 × 0.5 mm, so a jackscrew can still turn.
  - Socket depth: they end 0.3 mm behind the measured 5.1 mm.
- **Stop plates:** behind each socket, a stop plate reaches down to 0.3 mm above the PCB.
- **Result:** the case cannot slide forward (the flange is against the case face) or back (the hardware is against the
  socket ends).

### RS232 board

- **Ledges:** the board sits on 1.2 mm side ledges, with a 1.2 mm front skin under its front edge.
  - The ledges stop 6 mm short of the flange (DB9 board-lock pegs).
  - On the −X side, the ledges are relieved under the header pins.
- **Clips:** cantilever clips in the tray side walls catch the PCB top edge.
  - Default positions: −X at Y 9.5; +X at Y 9.5 and 23.2.
  - A clip that would collide with a lid-snap pocket is dropped, with an `ECHO` note. This happens at
    `xiao_overhang` ≤ ~4.
- **Pressers:** ribs on the lid come down to 0.1 mm above the board edge, to hold it down.
- **Clear space:** there is 3 mm clear below the board. The red ghost volume shows the through-hole pin zone, kept
  1.25 mm in from the board edges.

### XIAO

- **Mounting:** the XIAO hangs face up in the lid, with its underside level with the top of the DB9 rear body.
- **C-clips:** two C-clips grip the PCB edge, with a lip above and a hook below. They are diagonally offset so they only
  touch unused pads:
  - −X clip centred on pad 2 (D1), covering D0–D2.
  - +X clip centred on pad 6 (D8), covering D9–D7.
  - The in-use pads are clear, with their wire exits open to the side: D4/D5 (−X, pads 5–6) and 5V/GND (+X, pads 1–2).
  - Pad positions are from the Seeed land pattern: 7 pads at 2.54 mm pitch, centred on the 21 mm length, so pad 1 is
    2.88 mm from the USB end.
- **Front stop:** two front stop struts bear on the XIAO's front (antenna-end) PCB edge.
  - They sit at X −3 and +2, each 3 mm wide, clear of the ceramic antenna (+X corner) and the U.FL socket (−X corner).
  - Each strut is a solid rib running from the lid front wall back to the XIAO edge, so the USB insertion force puts it
    in compression rather than bending a thin tab across its layer lines.
  - Load path: plug → XIAO → struts → lid front wall → DB9 hardware sockets, with the snaps tying the lid to the tray.
  - Behind the DB9 body, the strut tip reaches 0.6 mm below the XIAO PCB.
  - At `xiao_overhang` ≤ ~1.5, the XIAO front edge sits over the DB9 body. The strut can then only come down to the body
    top + 0.2 mm, so it bears on just the top 0.8 mm of the 1.0 mm PCB edge.
- **Rear wall:** the rear wall wraps the USB-C receptacle in a slot (receptacle + 0.2 mm per side). The slot is open
  downwards so the XIAO can be pushed into the lid.
  - A tongue on the tray rear wall fills the slot under the receptacle once the case is closed.
  - Outside, a 12 × 7 mm plug-overmould relief is 1.6 mm deep, and its floor is flush with the receptacle mouth. A
    fully seated plug therefore bears on the case rather than on the XIAO. A plug overmould larger than 12 × 7 mm will
    not seat fully; increase `usb_recess_w`/`usb_recess_h` if needed.
- **Antenna:** the only material over the antenna end is the 1.6 mm top skin. The lid front wall is about 6 mm forward
  of the XIAO's front edge.

### Wires

There are no dedicated channels, because the interior is left open for the wires:

- **D4/D5:** these wires rise from the header on the −X side and reach their pads along the −X side.
- **5V/GND:** these wires cross under the XIAO, between it and the RS232 board, and come up the +X side to their pads.

The green ghost wires show one feasible route, and the interference check confirms that no case feature crosses it.

### Lid-tray snaps

- **Fingers:** there are two snap fingers per side, 1.4 mm thick and 6 mm wide, flush with the outside.
  - The free length is 10 mm: 4 mm is slotted into the lid wall, and 6 mm hangs below the split.
  - The bending strain at full hook deflection is about 1.3 %, comfortable for PETG.
- **Catch:** the hooks catch in 0.85 mm recesses in pockets on the tray's outer wall.
- **Opening:** there is a 1.5 mm pry gap below each finger tip. Push a small screwdriver in and lever the finger
  outwards.

### Logo

- **Deboss:** the ZebSerial mark (`assets/logo/zebserial-mark-deboss.svg`) is cut 0.5 mm (`logo_depth`) into the lid's
  outer top face, centred, 22 mm wide (`logo_w`). `show_logo = false` leaves the top plain.
- **Orientation:** the mark reads upright when you look down on the assembled case with the DB9 at the bottom edge,
  nearest you. The WiFi arcs point towards the USB-C end. `renders/lid_top_logo.png` shows this view.
- **Skin:** the top skin is 2.23 mm, because the USB plug relief sets the lid height, so 1.73 mm remains under the
  deboss. The model asserts at least 1.0 mm. Nothing inside the lid comes above the ceiling, so the deboss cannot
  meet a strut or clip.
- **Printing:** the lid prints top down, so the deboss is in the first 0.5 mm of layers and needs no support. At 22 mm
  wide the narrowest raised strip is about 1.0 mm (between the inner arc's ends and the shell) and the narrowest
  recess about 1.5 mm (the shell rim beside the stripes), against the 0.8 mm minimum for a 0.4 mm nozzle.

## Assembly order

1. **Tray:** press the RS232 board (with its DB9 fitted) down into the tray until the side clips snap over the PCB
   edge. Its front edge goes against the front skin.
2. **XIAO into the lid:**
   1. Hold the lid upside down just above the tray. The wires are short and soldered, so keep the XIAO tethered.
   2. Seat the USB-C receptacle into the rear slot.
   3. Press the XIAO into the lid until both C-clips snap. The front edge should sit against the stop struts.
3. **Close:**
   1. Flip the lid over the tray, folding the wires into the interior. D4/D5 go along the −X side, and 5V/GND go
      under the XIAO to +X.
   2. Lower the lid straight down. The front sockets drop over the flange hardware, the tray tongue slides up under
      the receptacle, and the four snaps click.
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

**Positive control:** with `-D 'ghost_offset=[1,-1,0]'` (the XIAO ghost moved 1 mm into the clips and stops), `check_lid`
reports about 9 mm³ of overlap. It is spread over the stop struts, a C-clip and the USB slot. This confirms that the
check can see a collision.

## Fit-check list: measure these with calipers before printing

Every value below is a guess or a catalogue default. Change the named parameter, then rerun `./build.sh`, which also
reruns the interference checks.

| # | Measure | Parameter | Default | Why it matters |
|---|---------|-----------|---------|----------------|
| 1 | RS232 PCB thickness | `pcb_t` | 1.0 | Board clip catch height, ledge height |
| 2 | XIAO rear edge past the RS232 rear edge (the user expects less than 5) | `xiao_overhang` | 5.0 | Case length, rear wall, stop strut position |
| 3 | Flange width × height × thickness | `fl_w`, `fl_h`, `fl_t` | 30.8 × 12.5 × 1.0 | Flange must cover the front opening (case opening 31.9 wide) |
| 4 | Flange bottom edge relative to the PCB **top** (negative = below) | `fl_bot` | −2.0 | Sets the socket height: hole centre = `fl_bot + fl_h/2` = 4.25 |
| 5 | Screw-hole spacing | `fl_hole_pitch` | 25.0 | Socket positions |
| 6 | Diameter of the hardware behind each hole (jackscrew, spring, nut or rivet: take the widest part) and whether there is a bracket under it down to the PCB | `hw_d` | 5.0 | Socket fit; the stop plates come within 0.3 mm of the PCB at Y 5.4–6.6, X 10.2–15.95 |
| 7 | DB9 rear plastic body: width, height above PCB top, depth behind the flange back | `body_w`, `body_h`, `body_d` | 20 × 10.5 × 9.5 | `body_h` also sets the XIAO height. `body_w` is assumed to fit between the screw hardware, as photo 1 suggests; if the body is wider, the sockets merge into the body notch automatically |
| 8 | D-shell outer width × height and projection | `ds_w`, `ds_h`, `ds_proj` | 16.9 × 8.4 × 6 | Ghost only |
| 9 | Header position: inset of its outer edge from the −X PCB edge (negative if it overhangs) and from the rear edge; header height | `hdr_edge`, `hdr_rear`, `hdr_h` | 0.3, 0.2, 8.5 | There is a 0.6 mm wall relief beside the header; ledge relief under its pins |
| 10 | Any component or solder joint within 1.2 mm of the RS232 PCB side edges, underneath (ledges) or on top at the presser/clip Y positions | `ledge_w`, `press_y_*`, `bc_y_*` | — | Ledges, pressers and clips touch the board there |
| 11 | Through-hole pin protrusion under the RS232 board | `below_clear` | 3.0 | Ghost pin zone assumes 2.5 mm or less |
| 12 | XIAO outline and thickness | `xiao_l`, `xiao_w`, `xiao_pcb_t` | 21 × 17.8 × 1.0 | Pocket, clips. The outline is from the Seeed drawing (XIAO ESP32C3 page; the C6 uses the same outline per the Seeed wiki), not measured |
| 13 | USB-C receptacle width × height, overhang past the XIAO PCB end | `usb_w`, `usb_h`, `usb_over` | 8.94 × 3.26, 1.5 | Rear-wall wrap and flush mouth |
| 14 | Your USB-C cable's overmould size | `usb_recess_w`, `usb_recess_h` | 12 × 7 | Plug must seat fully |
| 15 | Height of the tallest thing on top of the XIAO, plus the wire and solder blob height on the used pads | `xiao_top_clear` | 4.5 | Lid ceiling clearance (shield assumed 2.5, receptacle 3.26) |
| 16 | Positions of the ceramic antenna and the U.FL socket at the XIAO front end | `stop_x` | −3, +2 | Stop struts must miss both |

## Concerns to check before printing

- **DB9 dimensions (items 3–7):** these are standard-part defaults, not measurements. The flange height relative to the
  PCB (`fl_bot`) and the hardware diameter (`hw_d`) decide whether the lid drops over the connector at all. Measure
  them first.
- **Stop plates:** the lid's stop plates sit directly behind the flange hardware, down to 0.3 mm above the PCB. If the
  DB9 has board-lock brackets or solder lugs there, reduce their reach (the `0.3` in `lid()`) or remove them. The
  sockets alone still locate the case.
- **Board clip strain:** the board clips are 1.0 mm thick and bend from the floor top (about 5 mm free length). They work at roughly 3 %
  strain. The RS232 board is mainly held by the lid pressers. The clips only hold it during assembly.
- **Overhang ≤ ~4 mm:** at `xiao_overhang` ≤ ~4, the second +X board clip is dropped to make room for the rear lid
  snap.
