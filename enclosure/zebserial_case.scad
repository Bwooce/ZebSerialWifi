// ZebSerialWifi enclosure: HW-044 MAX3232 RS232-TTL board ("MAX3232 ESE 2512") + Seeed XIAO ESP32-C6.
// Two parts (tray + lid) joined by cantilever snap clips, no screws.
// Material PETG, Bambu H2D, 0.4 mm nozzle. All dimensions in mm.
//
// Coordinate frame (assembly):
//   X  across the case. Seen from the component side (+Z) with the DB9 towards you, +X is to the right.
//      In use the board hangs upside down (solder side and tray floor up); +X is then the printer's parallel-port
//      side, and the XIAO's USB-C comes out of the +X wall (revision 5; see README).
//   Y  0 at the DB9 flange FRONT face = case front face (flush), +Y towards the rear (the header end).
//      The flange sits in the front opening; its plate back (Y fl_plate_d) bears on the lid front wall.
//   Z  0 at the RS232 PCB TOP (component) surface, +Z towards the components and the lid. The tray floor is -Z.
//
// Usage:
//   openscad -D 'part="tray"' -D show_ghosts=false -o out/tray.stl zebserial_case.scad
//   openscad -D 'part="lid"'  -D show_ghosts=false -o out/lid.stl  zebserial_case.scad
//   part = "tray" | "lid" (print orientation), "assembly" | "exploded" (with optional ghosts),
//          "plugged" (the assembly as it hangs on the printer: tray floor up, case +Y to the right, +X up),
//          "check_tray" | "check_lid" | "check_traylid" (interference volumes, should be empty),
//          "coupon_tray" | "coupon_lid" (fit coupons, print orientation),
//          "logo" (two-colour inlay body, tray print orientation), "check_logo" (strokes / gaps under logo_min),
//          "check_lid_path" (lid material directly below a part: what the lid would hit closing straight down),
//          "ghosts" (ghost models only), "tray_inplace" | "lid_inplace" (assembly coordinates, for wallprobe.py).

/* [View] */
part = "assembly";
show_ghosts = true;
show_pin_zone = true;    // the red through-hole pin zone ghost (hide it to see the leg-tail keep-out in a render)
show_zones = true;       // the magenta wire / diode keep-outs in the views (the checks always include them)
show_lid = true;         // the lid in the assembly view (hide it to look at the XIAO, the wires and the diode)
// "none", or a slab: "x" keeps section_pos-section_t < x < section_pos (view from +X),
// "y" keeps section_pos < y < section_pos+section_t (view from the front),
// "z" keeps section_pos-section_t < z < section_pos (plan, view from above)
section = "none";
section_pos = 0;
section_t = 3;           // slab thickness, so walls behind the cut do not hide it
explode = 25;
// Positive-control offsets for the interference checks.
ghost_offset = [0, 0, 0];   // moves the XIAO ghost
ghost_lug_dy = 0;        // moves only the ghost board-lock legs (and their keep-out) in Y
ghost_lug_dx = 0;        // moves only the ghost legs (and their keep-out) in X
ghost_plate_d = -1;      // plate ghost depth (-1 = fl_plate_d)
ghost_board_dy = 0;      // moves the RS232 board + DB9 ghosts (with their legs) in Y
ghost_board_dx = 0;      // moves the RS232 board + DB9 ghosts in X (the corner pins locate it in X)
ghost_mh_dx = 0;         // moves only the PCB ghost's mounting holes in X (pin clearance control)
ghost_mh_dy = 0;         // moves only the PCB ghost's mounting holes in Y (pin clearance control)
ghost_board_dz = 0;      // moves only the RS232 board ghost (PCB, pin zone, header) in Z
ghost_lid_dz = 0;        // raises the lid in check_traylid (snap engagement with the lid held high)
coupon_floor = true;     // fit coupons: the tray coupon is the whole tray with the middle of the floor cut out,
coupon_z = 18.0;         // the lid coupon is the lid below this Z (walls, front wall, top lip, corner pressers, snaps,
                         // the whole USB relief: a cut through the relief leaves 0.1 mm slivers)
ctl_usbpad_full = false; // positive control: the USB shell pad over the whole shell top, across the 5V/GND corridor
fw_relief = true;        // front wall reliefs at the 5V/GND/3V3 joints (false = positive control)
ctl_undercut = false;    // positive control for check_lid_path: a lid block 0.5 under the XIAO rear edge, clear when closed
ctl_diode_undercut = false; // the same under the RX diode body, outside every keep-out: clear when closed
ctl_rear_rev3 = false;   // positive control: the interior rear face back at revision 3's place (bs_clr behind the PCB)
fw_wings = false;        // positive control: revision 3's lid front wall, full width behind the plate (the "wings")
p9_notch = true;         // the pin 9 wire's notch in the backstop (false = positive control)
ghost_p9_d = [0, 0, 0];  // positive control: moves the pin 9 wire keep-out
ghost_jack_d = [0, 0, 0];   // positive control: moves the jack screw hardware keep-outs
ghost_wch_hw = -1;       // 5V/GND corridor keep-out half width (-1 = wch_hw; a positive control sets it apart from the case)
ghost_rear_d = [0, 0, 0];   // positive control: moves the rear-edge D3/D5 joint zones, the rear wire zone and the diode
ghost_diode_d = [0, 0, 0];  // positive control: moves only the diode (ghost and keep-out, leads and D3 joint end too)

/* [Measured by the user (calipers)] */
board_w = 32.0;          // HW-044 board width (along the DB9 face, X)
board_d = 29.0;          // HW-044 PCB depth, its front edge to its rear edge
pcb_front_y = 0;         // DB9 flange FRONT face to the RS232 PCB front edge: flush, so the PCB runs under the flange
below_clear = 3.0;       // clear space under the RS232 PCB for through-hole pins (assumed as for the YL-97)
max_above = 25.0;        // maximum height envelope above the RS232 board
pcb_t = 1.6;             // RS232 PCB thickness (assumed as for the YL-97)
// HW-044 mounting holes: two, at the REAR corners
mh_d = 2.4;              // hole diameter
mh_pitch = 26.5;         // centre to centre, along X
mh_inset = 2.0;          // rear PCB edge to the hole centres
// The connector's front plate (steel flange plus the plastic behind it) keeps the full flange outline back to here:
// the user's first measurement, "the back of the flange plate screw holes is 5.1 mm from the front flat face"
// (YL-97 DB9; the HW-044's female DB9 is assumed to be the same).
fl_plate_d = 5.1;        // flange front face to the plate back = the lid front wall bearing face
// XIAO position: lying on the DB9 body, turned so its length runs along X with the USB-C out of the +X wall
// (revision 5; revisions 3 and 4 had it out of the -X wall, and the RX diode at D3 hit the lid front wall).
// -1 = derived: the USB end pcb_clr from the +X inner wall (X), the front edge pcb_clr behind the lid front wall (Y)
xiao_x0 = -1;            // X of the XIAO's USB-end PCB edge
xiao_front_y = -1;       // Y of the XIAO's front PCB edge (the 5V..D7 pad column)

/* [XIAO ESP32-C6, Seeed data] */
// Seeed XIAO ESP32-C6 v1.0 KiCad PCB (enclosure/reference/xiao_esp32c6) and its STEP model.
// d = distance from the USB end of the PCB (along -X in the case); w = offset across the width from the XIAO centre
// line, +w towards the 5V/GND column, which faces the front (-Y) of the case: X = USB end - d, Y = centre line - w.
xiao_l = 20.955;
xiao_w = 17.78;
xiao_pcb_t = 1.2;        // measured
xiao_pitch = 2.54;
xiao_pad1 = 2.921;       // pad 1 (5V / D0) centre from the USB end; the pads are not centred on the length
usb_w = 8.94;            // GCT USB4105 receptacle (drawing in reference/)
usb_h = 3.3;             // measured (4.5 overall at the USB-C); shell bottom sits on the PCB top
usb_l = 7.35;
usb_over = 1.5;          // receptacle mouth past the XIAO PCB end
shield_w = 12.6;         // RF shield / label can (across the width, centred)
shield_d0 = 6.71;        // shield spans d 6.71 .. 17.31
shield_d1 = 17.31;
shield_h = 2.2;          // measured (3.4 overall over the shield)
ant_w = [0.52, 5.72];    // ceramic antenna ANT1, w range and d range
ant_d = [18.8, 20.8];
ufl_w = [-5.6, -3.0];    // U.FL socket ANT2
ufl_d = [18.25, 20.85];
ufl_h = 2.5;             // with a mated plug
btn_w = [5.3, 7.9];      // BOOT / RESET buttons, both sides (|w|)
btn_d = [0.3, 1.9];
// Pad indices counted from the USB end (+X), 1..7.
// Front column (+w, faces the DB9 / lid front wall): 5V GND 3V3 D10 D9 D8 D7
// Rear column (-w, faces the header and the Dupont plug): D0 D1 D2 D3 D4 D5 D6
used_pads_front = [1, 2];    // 5V (module VCC and the pin 9 wire), GND
pullup_pad = 3;              // 3V3: room for an optional 10k pull-up from D3 (joint zone and front wall relief only;
                             // 0 = none). It does not drive the design.
used_pads_rear = [4, 6];     // D3 (RX, through the diode), D5 (TX) (firmware board.c: UART on GPIO21 / GPIO23)

/* [Right-angle DB9 female (HW-044), assumed the same as the measured YL-97 male part] */
fl_w = 30.8;             // flange width
fl_h = 12.5;             // flange height
fl_t = 0.38;             // steel flange thickness; plastic behind it counts as body
fl_hole_pitch = 25.0;    // screw-hole spacing
fl_bot = 0;              // flange bottom relative to the RS232 PCB top: flush with the body bottom (seating plane)
ds_w = 16.9;             // D-shell outer width (top)
ds_h = 8.4;              // D-shell outer height
ds_proj = 6.0;           // D-shell projection forward of the flange
body_w = 17.2;           // measured (YL-97): main rear body width
body_h = 10.4;           // measured (YL-97): rear body height above the PCB top = XIAO underside
body_end = 12.5;         // measured (YL-97): flange FRONT face to the back of the rear body
body_d = body_end - fl_t;
step_on = false;         // taller step right behind the flange (Norcomp); no information for this part
step_h = 12.2;
step_d = 2.9;
step_w = -1;             // -1 = body_w
// Lugs: plastic each side of the body, 3.4 high, out to the full PCB width, with a vertical screw hole
lug_h = 3.4;             // measured (YL-97)
lug_end = 12.0;          // measured (YL-97): flange FRONT face to the lug rear faces
lug_hole_y = 4.6;        // measured (YL-97): screw-hole centre behind the flange front face (ghost of the hole only)
// Board-lock leg tails below the PCB: on the HW-044 two slotted pads, estimated from the photo only
leg_y = 8.6;             // ESTIMATE: flange front face to the leg tail centres
lug_hole_x = 12.65;      // ESTIMATE: tail centres about 25.3 apart
lug_hole_d = 2.5;        // only used by the (off) pins
lug_hole_depth = 3.4;    // ghost only
lug_pin_on = false;      // kept for reference: lid pins into empty lug holes
// Anti-slide via the board-lock leg tails: tray bosses with pockets around them. Off: the HW-044 tails are not
// measured. A keep-out (leg_keepout either way of the estimate) keeps every tray feature away from them instead.
leg_pocket_on = false;
leg_keepout = 1.5;       // clear space either way of the estimated tail position, in X and Y
leg_tail_l = 3.0;        // tail protrusion below the RS232 PCB (YL-97 measurement; the tip reaches the floor top)
leg_w = 3.1;             // tail diameter (round) or size across X (YL-97 measurement)
leg_t = 0;               // tail size along Y; 0 = round tail of diameter leg_w
leg_clr = 0.3;           // pocket clearance per side in X (plus leg_slot_x)
leg_clr_y = 0.6;         // pocket clearance per side in Y
leg_slot_x = 1.0;        // extra slot length either side in X
leg_leadin = 0.6;        // 45 degree lead-in at the pocket mouth
front_wall_t = 2.0;      // lid front wall depth behind the flange plate (was 3.2: the XIAO now lies right behind it)
fl_clr_z = 0.3;          // flange top to the lid flange-slot ceiling
fw_lead = 0.5;           // 45 degree lead-in on the lid front wall bottom edge: steers the flange forward as the lid closes
// Retention lips in front of the flange (top from the lid, bottom from the tray): they hold the flange (and the
// loose shield) against the lid front wall without reaching the D-shell or the printer's jackposts at X +-12.5.
lip_x = 8.0;             // lips span |X| <= lip_x
lip_proud = 1.3;         // lip outer face in front of the case front face (Y 0); 1.2 of plastic over lip_gap
lip_gap = 0.1;           // lip back face to the flange front face (fore-aft play of the board)
lip_reach = 1.0;         // lip overlap on the flange margin, from the flange top / bottom edge
lip_ds_clr = 1.0;        // minimum lip clearance to the D-shell outline (top z 10.45, bottom z 2.05)
lip_root = 1.0;          // straight lip face beyond the flange slot / PCB before the 45 degree root chamfer
lip_lead = 0.3;          // 45 degree lead-in on the lip back edge (board drops in, lid closes)
pin_d = 2.2;             // (off) lug pins
pin_flat = 1.6;
pin_engage = 1.5;
pin_boss_d = 4.0;

/* [Corner pins: tray pins through the HW-044 mounting holes] */
// The pins locate the board in X (as the side walls do, both at 0.2). In Y they have more clearance (a lens-shaped pin), so they never fight the flange
// datum (lips 0.1 in front, lid front wall at zero gap behind) and tolerate an error in the hole position; the tray
// rear wall, bs_clr behind the PCB, is the backstop that the old rib was.
mh_pin_clr = 0.2;        // per side in X: pin 2.0 across X in the 2.4 hole
mh_pin_clr_y = 0.45;     // per side in Y, everywhere round the hole: pin 1.5 across Y
bs_clr = 0.25;           // PCB rear edge to the tray rear wall (the backstop; it never fights the flange datum)
mh_pin_top = -0.1;       // pin top relative to the PCB top: just below it, so the presser bears on the PCB, not the pin
                         // (a protruding pin would need a bore that breaks out of the presser's front face)
mh_pin_lead = 0.4;       // 45 degree lead-in at the pin top
mh_boss_d = 4.4;         // tray boss under the solder side around the pin root (the board sits on it)
// Lid corner pressers over the holes: they hold the board's rear corners down. (In revisions 3 and 4 their front edge
// had to stay behind the XIAO's rear-edge 5V/GND joints; the rear-edge joints are now D3/D5, nowhere near them.)
cp_front = 0.8;          // presser front face in front of the hole centre
cp_inner = 2.0;          // presser inner face from the hole centre (towards the middle)
cp_bore_clr = 0.3;       // bore over the pin, per side and above its top (only if the pin top reaches the presser)

/* [RS232 header and Dupont plug, HW-044] */
hdr_pins = 4;            // straight pins on the component side, in a row along X, centred on the rear edge
hdr_row_in = 1.5;        // ESTIMATE from the photo: pin row centre in from the rear PCB edge
hdr_h = 8.5;             // header (pins) height above the PCB, inside the housing
dupont_on = true;
dupont_top = 16.7;       // measured: housing top 18.3 above the PCB underside = 16.7 above the PCB top
dupont_bend_h = 4.0;     // minimum wire-bend space above the housing
dupont_ceiling_min = 23.4;   // measured: the wires need 25 mm of turning space from the PCB underside, so the
                             // lid ceiling must be at least 25 - 1.6 above the PCB top
xiao_hdr_min = 0.5;      // minimum gap from the XIAO rear edge to the housing front face
ghost_dupont_d = [0, 0, 0];   // positive control: moves the Dupont housing and bend zone
// Revision 4 (after the first print, "the back wall is too close to the dupont pins" and "there isn't enough space to
// route the wires from the dupont connector back to the xiao"): the housing keep-out, the wire-bend zone and the
// 5V/GND zone all run dupont_rear_clr further back, and the interior rear face stands behind them. Below the PCB top
// a backstop ledge stays bs_clr behind the PCB, so the board is located exactly as before.
dupont_rear_clr = 1.5;   // extra room behind the Dupont housing (revision 3 had 0.48 to the rear wall)
bend_side = 2.0;         // the wire-bend zone reaches this far either side of the housing in X
bs_top = -0.2;           // backstop ledge top, relative to the PCB top (it bears on the PCB rear edge below this)

/* [Pin 9 power wire (revision 4)] */
// The bridge is powered from the printer: a wire from the DB9 pin 9 pad (solder side, -Z) to the XIAO 5V pad. It
// runs under the board to the rear edge, up through a notch in the backstop behind the PCB, up to the ceiling beside
// the -X corner presser, then +X over the Dupont housing with the module wires and forward through the 5V/GND
// corridor to the 5V pad on the XIAO's front edge (revision 5). The -X edge is nearer, but the PCB is 0.2 from the
// side wall there, the side wall carries the snap windows, and the leg tail is in the way under it.
p9_on = true;
p9_x = -4.155;           // pin 9 pad: the -X end of the 4-pin row (DE-9 pitch 2.77, connector centred on X 0)
p9_y = 7.1;              // ESTIMATE from the annotated photo: the 4-pin row about 1.5 in front of the leg tails
p9_joint = 3.0;          // joint zone around the pad (square)
p9_joint_h = 2.5;        // joint zone depth below the PCB underside (pin tail, solder, wire end); 0.5 above the floor
wire_d = 1.4;            // jumper wire outside diameter (26 AWG Dupont type); wire_pass must leave 0.4 spare
wire_pass = 2.0;         // passage width for one wire, and the run's depth under the board
p9_run_x = -8.0;         // X of the run under the board and of the riser behind the PCB rear edge: clear of the
                         // header joints (X -3.81), the corner-pin boss (from X -11.05) and the presser (from -11.25)

/* [RX diode at the D3 pad (revision 5)] */
// The RX line has a Schottky diode at the XIAO: anode at the D3 pad, cathode to the module's RXD wire. Not measured:
// the keep-out assumes the worse case, a DO-41 1N5817/1N5819. Default placement: standing up beside the Dupont
// housing's +X end, behind the XIAO rear edge, its anode lead bent forward and down onto the D3 pad and its cathode
// lead up at the housing top, where the RXD wire from the housing joins it. The keep-out is the body grown by
// diode_clr, the two leads grown by diode_clr, and the D3 joint zone; the interference checks enforce it.
diode_on = true;
diode_d = 2.7;           // body diameter (DO-41)
diode_l = 5.2;           // body length (DO-41)
diode_lead_d = 0.8;      // lead diameter (DO-41)
diode_clr = 0.3;         // keep-out margin round the body and the leads
diode_lead_out = 0.6;    // straight lead out of each body end before the bend
diode_dir = [0, 0, 1];   // body axis, anode end to cathode end ([0, 0, 1]: standing up, anode down)
diode_c = [-1, -1, -1];  // body centre; [-1, -1, -1] = derived: diode_clr off the housing's +X face and behind the
                         // XIAO rear edge, its anode end diode_lead_out + diode_clr above the XIAO top

/* [DB9 jack screw hardware (revision 4)] */
// The user fits the DB9's jack screws / hex posts. Their tails and nuts stand behind the flange plate at the two
// flange holes, where revision 3's lid front wall ran full width ("the wings"). The front wall is cut back over
// them; the plate now bears on it above the hardware and across the top of the DB9 body.
jack_on = true;
jack_d = 7.0;            // keep-out diameter (M3 / 4-40 nut or screw tail, about 6 across flats)
jack_l = 5.0;            // keep-out length behind its start
jack_y0 = -1;            // start of the keep-out; -1 = fl_plate_d, the back of the flange plate screw holes
jack_clr = 0.3;          // front wall cut-back above the keep-out
jack_lead = 0.3;         // 45 degree lead-in on the cut-back front wall's bottom front edges (the wall is 1.6 deep
                         // there, behind the XIAO lead-in; fw_lead 0.5 would leave under min_wall)

/* [Print / tolerance] */
wall = 1.6;              // minimum wall (4 perimeters)
side_t = 4.0;            // side walls at the corners; the pincushion sag thins them towards mid-span
floor_t = 1.6;           // also the skin under the logo inlay (less logo_depth)
ceil_t = 1.6;            // top skin, also the only material over the XIAO antenna
pcb_clr = 0.2;           // per side on PCB pockets
clip_clr = 0.3;          // on clips
split_z = 4.0;           // tray / lid split plane (side and rear walls)
xiao_top_clear = 4.5;    // XIAO PCB top to lid ceiling (USB-C + wire blobs)
usb_clr = 0.2;
usb_recess_w = 12.0;     // plug overmould relief (along Y)
usb_recess_h = 7.0;
usb_recess_d = 1.6;      // minimum relief depth behind the receptacle mouth (the side face sets the real depth)
usb_recess_r = 2.5;      // relief corner radius
usb_mouth_inset = 0.0;   // receptacle mouth to relief floor (0 = flush, plug pushes on the case)

/* [Exterior style: faceted pincushion] */
// See ~/personal_scripts/ai-config/instructions/enclosure_styles.md. Applied to both halves so the seam lines up.
sag_frac = 0.02;         // inward bow of each face at mid-span, as a fraction of that face's length
front_sag = 0;           // the front face stays flat: it is the datum the DB9 flange sits against
facet_flat = 0.3;        // flat middle facet as a fraction of each face (0 = two facets meeting in a V)
corner_chamfer = 1.5;    // small vertical corner bevels, 45 degrees in plan
corner_facet = 5.0;      // each of the 8 corners cut off by a plane square to the body diagonal: triangular
                         // facets, leg length along each edge (about 12 % of the width, as on the reference)
edge_chamfer = 1.5;      // top and bottom perimeter chamfers, 45 degrees (prints without support)
seam_reveal = true;      // shadow-line groove on the lid just above the seam
reveal_d = 0.45;
reveal_h = 0.5;
min_wall = 1.2;          // checked by wallprobe.py, not just assumed

/* [Logo] */
// Inlaid into the outside of the tray floor, centred: the face that is up when the unit hangs on the printer, and
// the bed side in print. Mirrored so it reads correctly seen from outside (-Z), D-shell towards the DB9 (Y 0),
// WiFi arcs towards the rear.
// "inlay": flush two-colour inlay (part="logo" -> out/logo.stl, cut from the tray with no clearance; load both STLs
// in the slicer as one object with two parts). "deboss": single-colour fallback. "none": plain floor.
logo_mode = "inlay";
logo_w = 25.0;           // mark width: as large as the floor flat allows with about 1.5 mm to its front and rear edges
logo_depth = 0.68;       // inlay thickness / deboss depth: the first 4 layers of the profile (0.2 first layer + 3 x 0.16)
logo_file = "../assets/logo/zebserial-mark-deboss.svg";   // the web page's striped mark at 1 unit = 1 mm (make_logo.py)
logo_svg_w = 52.77;      // mark width in that file (imported bounding box)
logo_svg_h = 52.0;       // mark height in that file
logo_svg_c = [32, 29.93]; // mark centre once imported (OpenSCAD flips the SVG Y axis)
logo_min = 1.2;          // every stroke and gap at least this (three 0.4 mm lines); part="check_logo" checks it
logo_round = 0.62;       // round every corner to this radius (just over logo_min / 2): the striped mark's stripe ends
                         // and arc tips are sharp, and a 0.4 mm nozzle cannot print them anyway

/* [Clips and XIAO capture] */
// Lid-to-tray snaps (on both side walls)
lc_w = 6.0;              // finger width
lc_t = 1.4;              // finger thickness
lc_gap = 0.15;           // finger to pocket floor (tray)
lc_slot_back = 0.6;      // finger to the lid wall in the slot above the split: wide enough that PETG cannot weld it
lc_hook = 0.6;           // hook protrusion (0.45 of it past the pocket floor = catch engagement and finger deflection)
lc_win_up = 0.35;        // catch window top above the hook top: the snap still engages with the lid held this high
lc_above = 4.0;          // finger free length above the split (slots into the lid wall)
lc_below = 5.0;          // finger length below the split
lc_side_gap = 0.5;       // slot width either side of the finger in the lid wall
lc_pry = 0.9;            // gap under the finger tip for a screwdriver (keeps min_wall above the bottom chamfer)
lc_front_y = 6.3;        // first lid clip centre (Y): its lid slot ends 1.7 in front of the USB slot (now in +X; the
                         // clips are mirrored on both walls, so the clearance is the same as in revision 4)
lc_rear_from_back = 3.6; // second lid clip centre, distance from the interior rear face (slot 3.0 behind the USB slot)
ledge_w = 1.2;           // board support ledge, from the wall inner face
ledge_front_skip = 7.0;  // ledges start at least this far behind the PCB front edge, and behind the leg-tail keep-out
press_wall_clr = 0.2;    // lid presser to the tray wall below the split
press_gap = 0.25;        // presser bottom above the RS232 PCB top
press_root = 1.5;        // presser root: grows out of the lid walls over this height above the split, 45 degree top
// XIAO capture, all rigid: below on the DB9 body (front) and the tray tongue under the USB-C shell (+X end); above,
// lid pads on the RF shield and the USB-C shell and a block over the rear edge strip; X, the +X wall and the -X end
// stop rib; Y, the lid front wall and the USB slot sides at the front, the shield stop block and the USB slot sides
// at the rear.
front_rest_min = 2.0;    // the XIAO must lie at least this far on the DB9 body (it is the only front support)
xpad_gap = 0.25;         // lid pads to the RF shield top, the USB-C shell top and the rear edge strip
xpad_inset = 0.5;        // pads inset from the shield / shell outline
xpad_wall = 1.6;         // the shield pad is a ring of this wall
xlead = 0.4;             // 45 degree lead-ins on the front wall and the shield stop block at XIAO height
shield_stop_clr = 0.3;   // shield stop block to the shield's rear face
tongue_gap = 0.05;       // tray tongue cradle to the USB shell bottom (support under the USB end)
tongue_side_clr = 0.3;   // cradle to the shell's rounded ends in Y: the slot sides, not the cradle, locate the XIAO
tongue_tip = 0.5;        // minimum width of the tongue's top edges beside the cradle
// -X end stop rib (revision 5; the +X rib of revisions 3 and 4 turned round), bearing on the XIAO's antenna-end PCB
// edge from just behind the ceramic antenna (w 0.52..5.72, now on the front side) back to the rear edge, past the
// U.FL socket (w -5.6..-3.0). The antenna stops it reaching the front wall, so a web at its rear end ties it to the
// -X side wall: with the ceiling that holds it against the plug insertion force.
stop_ant_clr = 0.27;     // rib front end behind the antenna (as revision 4's rib end)
stop_t = 1.6;            // rib thickness in X
stop_web_t = 1.6;        // web thickness in Y, at the rib's rear end, out to the -X side wall
stop_z_clr = 0.1;        // rib bottom above the DB9 body top where it runs over the body
stop_lead = 0.4;         // 45 degree lead-in on the rib's bottom inner edge (pushes a misplaced XIAO towards +X)
// 5V/GND corridor (revision 5): the module VCC and GND wires and the pin 9 wire leave the Dupont housing top, run +X
// over the housing and forward over the XIAO between the shield pad ring and the USB shell pad, and come down onto
// the 5V/GND pads on the front edge. It replaces revisions 3 and 4's D3/D5 channel over the shield.
wch_c = -1;              // corridor centre X; -1 = midway between the 5V and GND pads
wch_hw = 2.5;            // corridor half width: three 1.4 mm wires abreast (4.2) with 0.8 to spare

$fn = 48;
eps = 0.01;

// ---------------------------------------------------------------- derived
y_cf = 0;                            // case front face, flush with the flange front face
y_fb = fl_t;                         // flange back face
y_pcb0 = pcb_front_y < 0 ? fl_t : pcb_front_y;   // RS232 PCB front edge
y_pcb1 = y_pcb0 + board_d;           // RS232 PCB rear edge
x_in = board_w / 2 + pcb_clr;
x_out = x_in + side_t;               // outermost X (at the corner ends of the side faces)
y_fw1 = lug_pin_on ? lug_hole_y + max(lug_hole_d, pin_boss_d) / 2 + min_wall   // lid front wall rear face
                   : fl_plate_d + front_wall_t;
// XIAO: antenna end at xa0 (-X), USB end at xa1 (+X); front (5V..D7) edge at yx0, rear (D0..D6) edge at yx1
xa1 = xiao_x0 == -1 ? x_in - pcb_clr : xiao_x0;
xa0 = xa1 - xiao_l;
yx0 = xiao_front_y >= 0 ? xiao_front_y : y_fw1 + pcb_clr;
yx1 = yx0 + xiao_w;
xcy = (yx0 + yx1) / 2;               // XIAO centre line (w = 0)
function dx(d) = xa1 - d;            // d (from the USB end) to X
function wy(w) = xcy - w;            // w (across the width, + towards the 5V/GND column) to Y
x_mouth = xa1 + usb_over;            // receptacle mouth (relief floor)
y_body1 = y_fb + body_d;             // back of the DB9 rear body
body_overlap = y_body1 - yx0;        // how far the XIAO front edge lies on the body top
front_on_body = body_overlap >= front_rest_min - 0.01;
step_wx = step_w < 0 ? body_w : step_w;
lug_hx = lug_hole_x < 0 ? fl_hole_pitch / 2 : lug_hole_x;
joint_half = xiao_pitch / 2 + 0.5;   // solder joint zone either side of a pad centre
zx0 = body_h;                        // XIAO underside
zx1 = zx0 + xiao_pcb_t;              // XIAO top
usb_cz = zx1 + usb_h / 2;
y_in0 = y_cf;
y_bs = y_pcb1 + bs_clr;              // backstop face behind the PCB rear edge (below bs_top)
// interior rear face: dupont_rear_clr more room than revision 3, and room for the pin 9 wire behind the PCB
y_in1 = ctl_rear_rev3 ? y_bs : max(y_bs + dupont_rear_clr, p9_on ? y_pcb1 + wire_pass : 0);
y_rear_face = y_in1 + max(wall, min_wall + reveal_d);   // rear face at its mid-span: min_wall behind the shadow-line groove
rear_sag = sag_frac * 2 * x_out;
y_out1 = y_rear_face + rear_sag;     // outermost Y; the concave rear face comes back to y_rear_face at mid-span
z_floor = -pcb_t - below_clear;
z_bot = z_floor - floor_t;
// The top also has to clear the USB overmould relief by min_wall measured square to the 45 degree top chamfer.
z_top = max(zx1 + xiao_top_clear + ceil_t, usb_cz + usb_recess_h / 2 + wall,
            usb_cz + usb_recess_h / 2 + edge_chamfer + min_wall * sqrt(2),
            dupont_on ? max(dupont_top + dupont_bend_h, dupont_ceiling_min) + ceil_t : 0);   // ceiling over the Dupont wires
z_ceil = z_top - ceil_t;
hole_z = fl_bot + fl_h / 2;
z_fl_top = fl_bot + fl_h;
z_slot_top = z_fl_top + fl_clr_z;    // lid flange-slot ceiling; the lid front face fills to Y 0 above it
ds_top = hole_z + ds_h / 2;          // D-shell outline
ds_bot = hole_z - ds_h / 2;
lip_t_bot = z_fl_top - lip_reach;    // top lip bottom
lip_b_top = fl_bot + lip_reach;      // bottom lip top
body_cut_x = body_w / 2 + pcb_clr;
usb_slot_w = usb_w + 2 * usb_clr;
// header and Dupont housing: rear centre, pins along X
hdr_yc = y_pcb1 - hdr_row_in;
hdr_hw = hdr_pins * 2.54 / 2;
hdr_y0 = hdr_yc - 1.27;
hdr_y1 = hdr_yc + 1.27;
hdr_room = hdr_y1 + dupont_rear_clr;  // the Dupont keep-out, bend zone and 5V/GND zone run back to here
// corner pins
mh_x = mh_pitch / 2;
mh_y = y_pcb1 - mh_inset;
mh_pin_dx = mh_d - 2 * mh_pin_clr;   // pin size across X
mh_pin_dy = mh_d - 2 * mh_pin_clr_y; // pin size across Y (lens)
cp_y0 = mh_y - cp_front;
skin_y1 = max(y_cf, y_pcb0) + 1.2;   // front skin under the PCB front edge
leg_tip = -pcb_t - leg_tail_l;       // bottom of the board-lock leg tails
leg_pocket_bot = leg_tip - leg_clr;
leg_px = leg_w / 2 + leg_clr + leg_slot_x;           // pocket half sizes (slotted in X)
leg_py = (leg_t == 0 ? leg_w : leg_t) / 2 + leg_clr_y;
leg_boss_top = -pcb_t - 0.2;
leg_boss_wall = min_wall + leg_leadin;
leg_ko_x = leg_w / 2 + leg_keepout;  // leg-tail keep-out half sizes
leg_ko_y = (leg_t == 0 ? leg_w : leg_t) / 2 + leg_keepout;
// side ledges start behind the leg-tail pockets, or behind the keep-out plus the board's fore-aft play at the pins
ledge_y0 = max(y_pcb0 + ledge_front_skip,
               leg_pocket_on ? leg_y + leg_py + leg_boss_wall : leg_y + leg_ko_y + mh_pin_clr_y + 0.05);
lc_ys = [lc_front_y, y_in1 - lc_rear_from_back];
pocket = lc_t + lc_gap;
// pads, and the 5V/GND corridor over the XIAO (X)
function pad_x(i) = dx(xiao_pad1 + (i - 1) * xiao_pitch);
front_pads = pullup_pad > 0 ? concat(used_pads_front, [pullup_pad]) : used_pads_front;   // front-edge joints
front_jx0 = min([for (i = front_pads) pad_x(i)]) - joint_half;   // X span of the front-edge joint zones
front_jx1 = max([for (i = front_pads) pad_x(i)]) + joint_half;
wch_cx = wch_c >= 0 ? wch_c : (pad_x(used_pads_front[0]) + pad_x(used_pads_front[1])) / 2;
wch0 = wch_cx - wch_hw;
wch1 = wch_cx + wch_hw;
gwch_hw = ghost_wch_hw < 0 ? wch_hw : ghost_wch_hw;
// rear-edge D3/D5 wire zone over the rear edge strip (X), from the D5 joint to the D3 joint
rear_jx0 = min([for (i = used_pads_rear) pad_x(i)]) - joint_half;
rear_jx1 = max([for (i = used_pads_rear) pad_x(i)]) + joint_half;
d3_x = pad_x(used_pads_rear[0]);     // the diode's anode pad
// jack screw hardware keep-out, behind each flange hole, and the front wall cut-back over it
jack_ys = jack_y0 < 0 ? fl_plate_d : jack_y0;
jack_cut_z = hole_z + jack_d / 2 + jack_clr;
// shield outline (X, Y)
sh_x0 = dx(shield_d1);  sh_x1 = dx(shield_d0);
sh_y0 = wy(shield_w / 2);  sh_y1 = wy(-shield_w / 2);
// -X end stop rib: X st_x0..st_x1, from just behind the antenna (Y st_y0) back to the XIAO rear edge
st_x1 = xa0 - pcb_clr;
st_x0 = st_x1 - stop_t;
st_y0 = wy(ant_w[0]) + stop_ant_clr;
// shield stop block behind the shield's rear face, between the D3 joint zone and the corridor
ss_x0 = rear_jx1 + 0.3;
ss_x1 = min(sh_x1, wch0 - 0.3);
// RX diode: unit axis, body centre, its anode (-) and cathode (+) ends, and the anode joint on the D3 pad
dio_u = diode_dir / norm(diode_dir);
dio_r = diode_d / 2 + diode_clr;     // keep-out radius
dio_c = diode_c[0] == -1 ? [hdr_hw + dio_r, yx1 + dio_r, zx1 + diode_lead_out + diode_clr + diode_l / 2] : diode_c;
dio_a = dio_c - dio_u * diode_l / 2;
dio_k = dio_c + dio_u * diode_l / 2;
dio_joint = [d3_x, yx1 - 0.6, zx1 + 0.4];

// ---- exterior outline (plan), CCW seen from above
side_sag = sag_frac * (y_out1 - y_cf);
function side_mid(a, b, sg) = sg <= 0 ? [] :
    let(d = b - a, n = [-d[1], d[0]] / norm(d), t1 = (1 - facet_flat) / 2, t2 = (1 + facet_flat) / 2)
    facet_flat > 0 ? [a + d * t1 + n * sg, a + d * t2 + n * sg] : [a + d * 0.5 + n * sg];
function outline() = let(X = x_out, c = corner_chamfer, y0 = y_cf, Y1 = y_out1,
        fl = [-X + c, y0], fr = [X - c, y0], r0 = [X, y0 + c], r1 = [X, Y1 - c],
        rr = [X - c, Y1], rl = [-X + c, Y1], l1 = [-X, Y1 - c], l0 = [-X, y0 + c])
    concat([fl], side_mid(fl, fr, front_sag), [fr, r0], side_mid(r0, r1, side_sag), [r1, rr],
           side_mid(rr, rl, rear_sag), [rl, l1], side_mid(l1, l0, side_sag), [l0]);
// inward offset of a polygon by d (edge lines shifted, then intersected)
function _line(p, q, d) = let(u = (q - p) / norm(q - p)) [p + [-u[1], u[0]] * d, u];
function _isect(l1, l2) = let(p = l1[0], r = l1[1], q = l2[0], v = l2[1],
        den = r[0] * v[1] - r[1] * v[0]) p + r * (((q[0] - p[0]) * v[1] - (q[1] - p[1]) * v[0]) / den);
function offset_poly(P, d) = let(n = len(P))
    [for (i = [0 : n - 1]) _isect(_line(P[(i - 1 + n) % n], P[i], d), _line(P[i], P[(i + 1) % n], d))];
// right-hand side face X at a given Y (the left side is its mirror image)
function surf_x(y) = let(a = y_cf + corner_chamfer, b = y_out1 - corner_chamfer, t = (y - a) / (b - a),
        t1 = (1 - facet_flat) / 2, t2 = (1 + facet_flat) / 2,
        sg = (t <= 0 || t >= 1) ? 0 : t < t1 ? side_sag * t / t1 : t > t2 ? side_sag * (1 - t) / (1 - t2) : side_sag)
    x_out - sg;
// lowest X of the side face over a Y span: the flat that the snap finger and pocket are cut from
function face_x(y0, y1) = let(a = y_cf + corner_chamfer, b = y_out1 - corner_chamfer,
        f0 = a + (b - a) * (1 - facet_flat) / 2, f1 = a + (b - a) * (1 + facet_flat) / 2)
    min(surf_x(y0), surf_x(y1), (y0 < f1 && y1 > f0) ? x_out - side_sag : 1e9);
function lc_face(c) = face_x(c - lc_w / 2 - clip_clr - lc_side_gap, c + lc_w / 2 + clip_clr + lc_side_gap);

// USB-C shell cross-section (stadium, in the YZ plane) and the tongue top: the tongue rises beside the cradle to
// where its edges are still tongue_tip wide
usb_a = usb_w / 2 - usb_h / 2;       // half length of the shell's flat top and bottom
tongue_w = usb_slot_w / 2 - clip_clr / 2;
tongue_r = usb_h / 2 + tongue_gap;
tongue_top = let(dd = tongue_w - tongue_tip - usb_a - tongue_side_clr)
    dd >= tongue_r ? zx1 - tongue_gap : max(zx1 - tongue_gap, usb_cz - sqrt(tongue_r * tongue_r - dd * dd));
// relief depth: the +X side face (bowed) to the receptacle mouth, at the relief's shallowest point
usb_relief_depth = min([for (y = [xcy - usb_recess_w / 2, xcy, xcy + usb_recess_w / 2]) surf_x(y)]) - x_mouth;
// logo on the floor flat (the bottom face inside the 45 degree edge chamfer)
logo_h = logo_w * logo_svg_h / logo_svg_w;
logo_cy = (y_cf + y_rear_face) / 2;

// "ZEBCASE WARNING" echoes mean the geometry is wrong for this parameter set: build.sh and controls.sh fail on any of
// them. Reminders that print on a good build use "ZEBCASE NOTE" instead.
echo(str("ZEBCASE outer X ", 2 * x_out, " Y ", y_out1 - y_cf, " Z ", z_top - z_bot,
         " (top ", z_top, " above PCB top, limit ", max_above, ")"));
echo(str("ZEBCASE style: side sag ", side_sag, ", rear sag ", rear_sag, ", front sag ", front_sag,
         ", corner chamfer ", corner_chamfer, ", corner facet ", corner_facet, ", edge chamfer ", edge_chamfer,
         ", snap faces at X ", [for (c = lc_ys) lc_face(c)]));
echo(str("ZEBCASE XIAO X ", xa0, "..", xa1, " Y ", yx0, "..", yx1, " underside z ", zx0, ", USB mouth X ", x_mouth,
         " at Y ", xcy, " z ", usb_cz, ", relief depth ", usb_relief_depth, ", lid clips at Y ", lc_ys));
if (z_top > max_above) echo(str("ZEBCASE WARNING: top ", z_top, " above the PCB top, over the ", max_above, " mm envelope"));
assert(z_top <= max_above, "case taller than the 25 mm envelope above the board");
assert(usb_relief_depth >= usb_recess_d - 1e-6, "the side face leaves the USB relief shallower than usb_recess_d");
echo(str("ZEBCASE top skin ", z_top - z_ceil, ", floor ", floor_t, logo_mode != "none" ? str(", ", floor_t - logo_depth,
         " under the logo (", logo_mode, ", ", logo_w, " x ", logo_h, " mm, floor flat margins front ",
         logo_cy - logo_h / 2 - (y_cf + edge_chamfer), " rear ", (y_rear_face - edge_chamfer) - (logo_cy + logo_h / 2),
         " sides ", (x_out - side_sag - edge_chamfer) - logo_w / 2, ")") : ""));
// 0.9: the 1.6 floor less the 0.68 inlay leaves 0.92, which the 0.16 mm profile prints as 6 solid layers
assert(logo_mode == "none" || floor_t - logo_depth >= 0.9 - 1e-6, "logo leaves less than 0.9 mm of floor skin");
assert(logo_mode == "none" || logo_mode == "inlay" || logo_mode == "deboss", "logo_mode must be inlay, deboss or none");
echo(str("ZEBCASE XIAO on the DB9 body ", body_overlap, " mm; rear edge to the Dupont housing ", hdr_y0 - yx1,
         " mm; front edge ", yx0 - y_fw1, " behind the lid front wall"));
assert(hdr_y0 - yx1 >= xiao_hdr_min - 1e-6, "the XIAO rear edge is too close to the Dupont housing");
echo(str("ZEBCASE NOTE (check): the front-edge 5V/GND", pullup_pad > 0 ? "/3V3" : "", " joints (X ", front_jx0, "..", front_jx1,
         ", Y ", yx0, ") lie over the DB9 body's +X end and the +X jack screw hardware (top z ", hole_z + jack_d / 2,
         ", ", zx0 - hole_z - jack_d / 2, " under the XIAO): top-side joints only, with nothing under the XIAO there"));
echo(str("ZEBCASE CORNER PINS at X +-", mh_x, " Y ", mh_y, ": ", mh_pin_dx, " across X, ", mh_pin_dy, " across Y, top z ",
         mh_pin_top, "; pressers from Y ", cp_y0, ", ", press_gap, " above the PCB"));
echo(str("ZEBCASE leg-tail keep-out (estimate +-", leg_keepout, "): X +-", lug_hx - leg_ko_x, "..", lug_hx + leg_ko_x,
         " Y ", leg_y - leg_ko_y, "..", leg_y + leg_ko_y, "; ledges from Y ", ledge_y0));
echo(str("ZEBCASE SUPPORT XIAO: on the DB9 body (", body_overlap, ") and the tongue (gap ", tongue_gap,
         ", top edges at z ", tongue_top, "); pads ", xpad_gap, " over the shield, USB shell and rear strip; -X stop rib X ",
         st_x0, "..", st_x1, " Y ", st_y0, "..", yx1, " (antenna to Y ", wy(ant_w[0]), "); shield stop X ", ss_x0, "..", ss_x1,
         " Y ", sh_y1 + shield_stop_clr, "..", yx1, "; 5V/GND corridor X ", wch0, "..", wch1));
echo(str("ZEBCASE PADS: front 5V X ", pad_x(1), ", GND ", pad_x(2), ", 3V3 ", pad_x(3), " (Y ", yx0, "); rear D3 X ", pad_x(4),
         ", D5 ", pad_x(6), " (Y ", yx1, "); front joints X ", front_jx0, "..", front_jx1, ", rear joints X ", rear_jx0, "..", rear_jx1));
if (diode_on) echo(str("ZEBCASE DIODE: ", diode_d, " x ", diode_l, " body, centre ", dio_c, ", axis ", dio_u, "; keep-out radius ",
                       dio_r, ", body ends ", dio_a, " (anode) / ", dio_k, " (cathode); anode joint on D3 at ", dio_joint));
echo(str("ZEBCASE LIPS: |X| <= ", lip_x, ", ", lip_proud, " proud of Y 0, gap ", lip_gap, "; top lip z ", lip_t_bot, " (D-shell top ",
         ds_top, "), bottom lip to z ", lip_b_top, " (D-shell bottom ", ds_bot, ")"));
if (lip_t_bot < ds_top + lip_ds_clr || lip_b_top > ds_bot - lip_ds_clr)
    echo("ZEBCASE WARNING: a flange lip comes within lip_ds_clr of the D-shell");
if (lip_x > fl_hole_pitch / 2 - 3.5) echo("ZEBCASE WARNING: the lips reach the jackpost area at the flange holes");
if (dupont_on) echo(str("ZEBCASE DUPONT: housing X +-", hdr_hw, " Y ", hdr_y0, "..", hdr_y1, " to z ", dupont_top,
                        ", keep-out to Y ", hdr_room, " (interior rear face ", y_in1, ", backstop ", y_bs, " below z ", bs_top,
                        "); wire-bend zone X +-", hdr_hw + bend_side, " Y ", yx1, "..", hdr_room, " from z ", dupont_top,
                        " to the ceiling at ", z_ceil));
if (p9_on) echo(str("ZEBCASE PIN 9 WIRE: pad X ", p9_x, " Y ", p9_y, " (estimate); run under the board at X ", p9_run_x,
                    " +-", wire_pass / 2, ", z ", -pcb_t - wire_pass, "..", -pcb_t, " (floor top ", z_floor,
                    "); riser behind the PCB Y ", y_pcb1, "..", y_pcb1 + wire_pass, " up to the ceiling, then +X over the ",
                    "housing to the 5V/GND corridor; ", wire_pass - wire_d, " spare round a ", wire_d, " wire"));
assert(!p9_on || wire_pass >= wire_d + 0.4 - 1e-6, "wire_pass leaves under 0.4 mm round the pin 9 wire (wire_d)");
if (jack_on) echo(str("ZEBCASE JACK HARDWARE: X +-", fl_hole_pitch / 2, " z ", hole_z, ", d ", jack_d, ", Y ", jack_ys, "..",
                      jack_ys + jack_l, "; front wall cut back beside the DB9 body up to z ", jack_cut_z));
if (!front_on_body) echo(str("ZEBCASE WARNING: the XIAO lies only ", body_overlap, " mm on the DB9 body (needs ", front_rest_min, ")"));
if (leg_pocket_on && leg_pocket_bot < z_bot + 0.8)
    echo(str("ZEBCASE WARNING: the leg-tail pocket bottom (z ", leg_pocket_bot, ") comes within 0.8 mm of the tray floor bottom"));

// ---------------------------------------------------------------- helpers
// Bounds must be in order: an inverted cube renders as nothing, silently (revision 4 lost a button ghost that way).
module box(x0, x1, y0, y1, z0, z1) {
    assert(x1 >= x0 && y1 >= y0 && z1 >= z0, str("box with inverted bounds ", [x0, x1, y0, y1, z0, z1]));
    translate([x0, y0, z0]) cube([x1 - x0, y1 - y0, z1 - z0]);
}
// box from two corners in any order (for d / w ranges, which run along -X / -Y)
module cbox(a, b) box(min(a[0], b[0]), max(a[0], b[0]), min(a[1], b[1]), max(a[1], b[1]), min(a[2], b[2]), max(a[2], b[2]));
module mirror_x_both() { children(); mirror([1, 0, 0]) children(); }
// Polygon in the XZ plane extruded along Y from y0 to y1.
module xz_prism(pts, y0, y1) {
    translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - y0) polygon(pts);
}
// Polygon in the YZ plane ([y, z] points) extruded along X from x0 to x1.
module yz_prism(pts, x0, x1) {
    translate([x0, 0, 0]) rotate([90, 0, 90]) linear_extrude(x1 - x0) polygon(pts);
}
// 2D children in the YZ plane ([y, z]) extruded along X from x0 to x1.
module yz_extrude(x0, x1) {
    translate([x0, 0, 0]) rotate([90, 0, 90]) linear_extrude(x1 - x0) children();
}
// USB-C shell cross-section (YZ), grown by c
module usb_stadium(c) {
    hull() for (s = [-1, 1]) translate([xcy + s * usb_a, usb_cz]) circle(r = usb_h / 2 + c);
}

// Faceted pincushion exterior: concave plan outline with corner chamfers, extruded with
// 45 degree chamfers on the top and bottom perimeter edges. Top and bottom stay flat.
module outer_solid() {
    P = outline();
    Q = offset_poly(P, edge_chamfer);
    n = len(P);
    e = edge_chamfer;
    pts = concat([for (p = Q) [p[0], p[1], z_bot]], [for (p = P) [p[0], p[1], z_bot + e]],
                 [for (p = P) [p[0], p[1], z_top - e]], [for (p = Q) [p[0], p[1], z_top]]);
    faces = concat([[for (i = [0 : n - 1]) i]], [[for (i = [n - 1 : -1 : 0]) 3 * n + i]],
                   [for (k = [0 : 2]) for (i = [0 : n - 1])
                       [k * n + i, (k + 1) * n + i, (k + 1) * n + (i + 1) % n, k * n + (i + 1) % n]]);
    difference() {
        polyhedron(pts, faces, convexity = 4);
        if (corner_facet > 0)
            for (sx = [-1, 1], cy = [0, 1], cz = [0, 1]) {
                B = [sx * x_out, cy ? y_out1 : y_cf, cz ? z_top : z_bot];
                d = [-sx, cy ? -1 : 1, cz ? -1 : 1];            // inward along each axis
                // tetrahedron: the facet triangle plus an apex well outside the corner
                hull() polyhedron([B - d * 5, B + [d[0] * corner_facet, 0, 0], B + [0, d[1] * corner_facet, 0],
                                   B + [0, 0, d[2] * corner_facet]], [[0, 1, 2], [0, 2, 3], [0, 3, 1], [1, 3, 2]]);
            }
    }
}

// shadow-line groove on the lid just above the seam, interrupted at the snap fingers
module reveal_cut() {
    difference() {
        box(-x_out - 5, x_out + 5, y_cf - 5, y_out1 + 5, split_z - 1, split_z + reveal_h);
        translate([0, 0, split_z - 2]) linear_extrude(reveal_h + 3) polygon(offset_poly(outline(), reveal_d));
        for (c = lc_ys) mirror_x_both()
            box(x_in, x_out + 6, c - lc_w / 2, c + lc_w / 2, split_z - 2, split_z + 2);   // groove runs into the side slots: no webs
    }
}

// ---------------------------------------------------------------- tray
module tray(logo = logo_mode) union() {
  intersection() {
  outer_solid();
    difference() {
        union() {
            difference() {
                intersection() { outer_solid(); box(-x_out - 1, x_out + 1, y_cf - 1, y_out1 + 1, z_bot - 1, split_z); }
                box(-x_in, x_in, y_in0 - 1, y_in1, z_floor, split_z + 1);
            }
            // front skin under the PCB, supports the PCB front edge (flush with the case front)
            box(-x_in, x_in, y_cf, skin_y1, z_floor - eps, -pcb_t);
            // small gusset in the floor / front skin corner: keeps min_wall square to the bottom front chamfer
            fg = max(0, edge_chamfer + min_wall * sqrt(2) - floor_t - 1.2);
            if (fg > 0) translate([x_in, 0, 0]) rotate([0, -90, 0]) linear_extrude(2 * x_in)
                polygon([[z_floor - eps, skin_y1 - eps], [z_floor - eps, skin_y1 + fg], [z_floor + fg, skin_y1 - eps]]);
            // side ledges
            mirror_x_both() box(x_in - ledge_w, x_in + eps, ledge_y0, y_pcb1 - 0.5, z_floor - eps, -pcb_t);
            // bosses around the board-lock leg tails (off for the HW-044: tails not measured)
            if (leg_pocket_on) mirror_x_both()
                box(lug_hx - leg_px - leg_boss_wall, min(lug_hx + leg_px + leg_boss_wall, x_in + 0.5),
                    leg_y - leg_py - leg_boss_wall, leg_y + leg_py + leg_boss_wall, z_floor - eps, leg_boss_top);
            // corner pins through the mounting holes, on bosses under the solder side
            mirror_x_both() corner_pin();
            // backstop ledge behind the PCB rear edge, below the PCB top, with the pin 9 wire's notch
            if (y_in1 > y_bs) difference() {
                box(-x_in - eps, x_in + eps, y_bs, y_in1 + eps, z_floor - eps, bs_top);
                if (p9_on && p9_notch) box(p9_run_x - wire_pass / 2, p9_run_x + wire_pass / 2, y_bs - 1, y_in1 + eps, z_floor - 1, bs_top + 1);
            }
            usb_tongue();
        }
        if (leg_pocket_on) mirror_x_both() translate([lug_hx, leg_y, 0]) leg_pocket();
        // lid clip pockets and catch recesses
        for (c = lc_ys) mirror_x_both() lid_clip_tray_cut(c);
        usb_recess_cut();
        // the tongue ends square under the relief: its rounded corners would leave knife-edge wedges on the tongue
        box(x_mouth + usb_mouth_inset, x_out + 1, xcy - tongue_w - 1, xcy + tongue_w + 1, usb_cz - usb_recess_h / 2, split_z + 20);
        if (logo != "none") logo_body(1);        // inlay pocket or deboss in the floor
    }
  }
  bottom_lip();
}

// Tray pin through a rear-corner mounting hole (+X; mirrored), flatted in Y, on a boss under the solder side
module corner_pin() {
    translate([mh_x, mh_y, 0]) {
        translate([0, 0, z_floor - eps]) cylinder(d = mh_boss_d, h = -pcb_t - z_floor + eps);
        // lens: inside the hole moved mh_pin_clr_y either way in Y, and mh_pin_clr inside it in X
        intersection() {
            union() {
                translate([0, 0, -pcb_t - eps]) cylinder(d = mh_pin_dx, h = mh_pin_top - mh_pin_lead + pcb_t + eps);
                translate([0, 0, mh_pin_top - mh_pin_lead])
                    cylinder(d1 = mh_pin_dx, d2 = mh_pin_dx - 2 * mh_pin_lead, h = mh_pin_lead);
            }
            translate([0, mh_pin_clr_y, -pcb_t - 1]) cylinder(d = mh_d, h = mh_pin_top + pcb_t + 2);
            translate([0, -mh_pin_clr_y, -pcb_t - 1]) cylinder(d = mh_d, h = mh_pin_top + pcb_t + 2);
        }
    }
}

// Tongue in the +X wall that fills the lid USB slot under the receptacle, with a cradle under the shell's rounded ends
module usb_tongue() difference() {
    box(x_in, x_out, xcy - tongue_w, xcy + tongue_w, split_z - eps, tongue_top);
    yz_extrude(x_in - 1, x_out + 1) hull() for (s = [-1, 1]) translate([s * tongue_side_clr, 0]) usb_stadium(tongue_gap);
}

// Bottom retention lip, in front of the PCB front edge and the flange bottom margin; it grows from the tray front
// face below the PCB, with a 45 degree root chamfer underneath (tray prints floor down)
module bottom_lip() {
    zr = -pcb_t - lip_root;           // bottom of the straight outer face
    yz_prism([[eps, zr - lip_proud], [-lip_proud, zr], [-lip_proud, lip_b_top], [-lip_gap - lip_lead, lip_b_top],
              [-lip_gap, lip_b_top - lip_lead], [-lip_gap, -pcb_t], [eps, -pcb_t]], -lip_x, lip_x);
}

// Top retention lip from the lid, in front of the flange top margin. The lid prints top down, so the 45 degree
// chamfer above the lip (in assembly) is what carries it in print.
module top_lip() {
    zr = z_slot_top + lip_root;
    yz_prism([[-lip_gap, lip_t_bot + lip_lead], [-lip_gap - lip_lead, lip_t_bot], [-lip_proud, lip_t_bot],
              [-lip_proud, zr], [eps, zr + lip_proud], [eps, z_slot_top], [-lip_gap, z_slot_top]], -lip_x, lip_x);
}

// lid-clip geometry, right side (+X); mirrored for the left
lc_tip = split_z - lc_below;
lc_hook_top = lc_tip + 1.4;
// All snap geometry is measured from xo = the lowest point of the concave side face over the
// clip's span, so the bow never thins the finger or the wall behind its pocket.
module lid_clip_tray_cut(c) {
    hw = lc_w / 2 + clip_clr;
    xo = lc_face(c);
    box(xo - pocket, x_out + 1, c - hw, c + hw, lc_tip - lc_pry, split_z + 1);        // pocket + pry gap
    // catch window, through to the inside (below the board, beside the ledge): a blind recess
    // would leave less than min_wall behind it at mid-span
    box(x_in, xo - pocket + eps, c - hw, c + hw, lc_tip - 0.2, lc_hook_top + lc_win_up);
}
module lid_clip_lid_cut(c) {
    hw = lc_w / 2;
    xo = lc_face(c);
    // slots either side and behind the finger, up into the lid wall; lc_slot_back behind it (not lc_gap) so the
    // finger cannot weld to the wall and keeps its full free length
    difference() {
        box(xo - lc_t - lc_slot_back, x_out + 1, c - hw - lc_side_gap, c + hw + lc_side_gap, split_z - 1, split_z + lc_above);
        box(xo - lc_t, x_out + 1, c - hw, c + hw, split_z - 1, split_z + lc_above + 1);
    }
}
module lid_clip_finger(c) {
    hw = lc_w / 2;
    xi = lc_face(c) - lc_t;
    box(xi, x_out + 1, c - hw, c + hw, lc_tip, split_z + eps);     // outer face trimmed by outer_solid()
    xz_prism([[xi + eps, lc_tip], [xi, lc_tip], [xi - lc_hook, lc_tip + lc_hook],
              [xi - lc_hook, lc_hook_top], [xi + eps, lc_hook_top]], c - hw, c + hw);
}

// plug overmould relief in the +X side face, its floor flush with the receptacle mouth
module usb_recess_cut() {
    r = min(usb_recess_r, usb_recess_h / 2 - 0.01);
    yz_extrude(x_mouth + usb_mouth_inset, x_out + 1) translate([xcy, usb_cz])
        offset(r = r) square([usb_recess_w - 2 * r, usb_recess_h - 2 * r], center = true);
}

// ---------------------------------------------------------------- lid
module lid(xcap = true) union() {
  intersection() {
    outer_solid();
    difference() {
        union() {
            difference() {
                intersection() { outer_solid(); box(-x_out - 1, x_out + 1, y_cf - 1, y_out1 + 1, split_z, z_top + 1); }
                box(-x_in, x_in, y_in0 - 1, y_in1, split_z - 1, z_ceil);
                for (c = lc_ys) mirror_x_both() lid_clip_lid_cut(c);
            }
            // front wall: behind the flange plate (full flange outline to Y fl_plate_d) it is the bearing face;
            // above the flange opening it fills forward to Y 0. Across the full interior width, like the tray.
            difference() {
                box(-x_in - eps, x_in + eps, fl_plate_d, y_fw1, split_z, z_top - eps);
                // lead-in on the bottom front edge
                yz_prism([[fl_plate_d - 1, split_z - 1], [fl_plate_d + fw_lead + 1, split_z - 1],
                          [fl_plate_d - 1, split_z + fw_lead + 1]], -x_in - 1, x_in + 1);
            }
            box(-x_in - eps, x_in + eps, y_cf, fl_plate_d + eps, z_slot_top, z_top - eps);
            if (lug_pin_on) mirror_x_both() lug_pin();
            // board pressers over the rear-corner pins
            mirror_x_both() corner_presser();
            // XIAO capture: pads, shield stop block, -X end stop rib
            if (xcap) xiao_pads();
            end_stop();
            for (c = lc_ys) mirror_x_both() lid_clip_finger(c);
            if (ctl_undercut) box(-4.5, -2.5, yx1 - 1, yx1 + 0.2 + eps, zx0 - 1.5, zx0 - 0.5);   // under the unused D6 pad
            if (ctl_diode_undercut) box(7.6, 8.0, 26.9, 27.9, 10.4, 11.0);
        }
        // DB9 rear body
        box(-body_cut_x, body_cut_x, y_fb - 1, y_body1 + pcb_clr, -1, body_h + pcb_clr);
        // revision 4: the front wall beside the DB9 body (revision 3's "wings") cut back over the jack screw
        // hardware, with the plate bearing above it; 45 degree lead-ins on the new bottom front edges, except behind
        // the 5V/GND joint relief (revision 5), where the wall is only 1.2 deep and a lead-in would leave 0.9
        if (jack_on && !fw_wings) {
            mirror_x_both() box(body_cut_x - eps, x_in + eps, fl_plate_d - 1, y_fw1 + 1, split_z - 1, jack_cut_z);
            difference() {
                union() {
                    mirror_x_both()
                        yz_prism([[fl_plate_d - 1, jack_cut_z - 1], [fl_plate_d + jack_lead + 1, jack_cut_z - 1],
                                  [fl_plate_d - 1, jack_cut_z + jack_lead + 1]], body_cut_x - eps, x_in + eps);
                    yz_prism([[fl_plate_d - 1, body_h + pcb_clr - 1], [fl_plate_d + jack_lead + 1, body_h + pcb_clr - 1],
                              [fl_plate_d - 1, body_h + pcb_clr + jack_lead + 1]], -body_cut_x, body_cut_x);
                }
                if (fw_relief) box(front_jx0 - 0.3, front_jx1 + 0.3, fl_plate_d - 2, y_fw1 + 2, split_z - 2, z_top);
            }
        }
        if (step_on) box(-step_wx / 2 - pcb_clr, step_wx / 2 + pcb_clr, y_fb - 1, y_fb + step_d + pcb_clr, -1, step_h + pcb_clr);
        // front wall at XIAO height: a 45 degree lead-in on its rear face, so a XIAO up to xlead too far forward is
        // pushed back as the lid closes, and one relief for the 5V/GND (and 3V3) solder joints on the XIAO front edge
        box(xa0 - pcb_clr / 2, x_in + eps, y_fw1 - xlead, y_fw1 + 1, split_z - 1, zx0);
        yz_prism([[y_fw1 - xlead, zx0 - eps], [y_fw1 + 1, zx0 - eps], [y_fw1 + 1, zx0 + xlead + 1]], xa0 - pcb_clr / 2, x_in + eps);
        if (fw_relief) box(front_jx0 - 0.3, min(front_jx1 + 0.3, x_in + eps), y_fw1 - 0.8, y_fw1 + 1, split_z - 1, zx1 + 3.3);
        // USB-C slot in the +X wall: the shell's stadium, open down to the rim so the closing lid passes over the
        // receptacle; the tray tongue fills below it
        yz_extrude(x_in - eps, x_out + 1) hull() {
            usb_stadium(usb_clr);
            translate([xcy - usb_slot_w / 2, split_z - 1]) square([usb_slot_w, usb_cz - split_z + 1]);
        }
        usb_recess_cut();
        if (seam_reveal) reveal_cut();
    }
  }
  top_lip();
}

// Mirrored so that it reads correctly seen from outside the floor (from -Z): seen from below with +Y up, +X is to
// the left. The D-shell end is towards Y 0, the WiFi arcs towards the rear.
module logo_2d() {
    // closing then opening rounds concave and convex corners alike; neither removes a stroke or gap over 2 x logo_round
    offset(r = logo_round) offset(r = -logo_round) offset(r = -logo_round) offset(r = logo_round)
        translate([0, logo_cy]) mirror([1, 0]) scale(logo_w / logo_svg_w) translate(-logo_svg_c) import(logo_file);
}
// The inlay body (extra = 0), or the same body extended `extra` below the floor as the tray cut: no clearance
module logo_body(extra = 0) {
    translate([0, 0, z_bot - extra]) linear_extrude(logo_depth + extra) logo_2d();
}

// Pin hanging from the lid front wall into a lug screw hole (off: kept for reference)
module lug_pin() {
    zb = lug_h - pin_engage;
    translate([lug_hx, lug_hole_y, 0]) {
        translate([0, 0, lug_h + 0.2]) cylinder(d = pin_boss_d, h = split_z - lug_h - 0.2 + eps);
        intersection() {
            union() {
                translate([0, 0, zb + 0.4]) cylinder(d = pin_d, h = split_z - zb);
                translate([0, 0, zb]) cylinder(d1 = pin_d - 0.8, d2 = pin_d, h = 0.4);   // lead-in
            }
            box(-pin_flat / 2, pin_flat / 2, -pin_d, pin_d, zb - 1, split_z + 1);
        }
    }
}

// Slot around a leg tail (off for the HW-044): leg_clr in Y, leg_clr + leg_slot_x in X, with a 45 degree lead-in.
module leg_pocket() {
    r = leg_py;
    hx = leg_px - (leg_t == 0 ? r : 0);
    module slot(rr, z0, h) {
        if (leg_t == 0) hull() for (sx = [-1, 1]) translate([sx * hx, 0, z0]) cylinder(r = rr, h = h);
        else translate([-hx - (rr - r), -rr, z0]) cube([2 * (hx + rr - r), 2 * rr, h]);
    }
    slot(r, leg_pocket_bot, leg_boss_top - leg_pocket_bot + 1);
    hull() {
        slot(r, leg_boss_top - leg_leadin, 0.01);
        slot(r + leg_leadin, leg_boss_top, 0.01);
    }
}

// Lid presser over a rear-corner pin (+X; mirrored): a block from the side and rear walls down to press_gap above
// the PCB around the hole, with a bore over the pin top. Below the split it stands press_wall_clr off the tray walls;
// above it, it is rooted 0.5 into the lid walls with 45 degree tops so it prints top down.
module corner_presser() {
    x0 = mh_x - cp_inner;
    zr = split_z + press_root;
    difference() {
        intersection() {
            box(x0, x_in + 0.5, cp_y0, y_in1 + 0.5, press_gap, zr + 20);
            xz_prism([[x0, press_gap - 1], [x_in + 1, press_gap - 1], [x_in + 1, zr + x_in + 1 - x0], [x0, zr]], cp_y0 - 1, y_in1 + 1);
            yz_prism([[cp_y0, press_gap - 1], [y_in1 + 1, press_gap - 1], [y_in1 + 1, zr + y_in1 + 1 - cp_y0], [cp_y0, zr]], x0 - 1, x_in + 1);
        }
        box(x_in - press_wall_clr, x_in + 1, cp_y0 - 1, y_in1 + 1, press_gap - 1, split_z);
        box(x0 - 1, x_in + 1, y_in1 - press_wall_clr, y_in1 + 1, press_gap - 1, split_z);
        if (mh_pin_top + cp_bore_clr > press_gap)
            translate([mh_x, mh_y, press_gap - 1]) cylinder(d = mh_pin_dx + 2 * cp_bore_clr, h = mh_pin_top + cp_bore_clr - press_gap + 1);
    }
}

// Rigid pads from the ceiling, xpad_gap above the XIAO: a ring on the RF shield top, a block on the USB-C shell's
// flat top (from the 5V/GND corridor to the +X wall), and the shield stop block over the rear edge strip behind the
// shield. They hold the XIAO down onto the DB9 body and the tongue.
module xiao_pads() {
    x0 = sh_x0 + xpad_inset;  x1 = sh_x1 - xpad_inset;
    y0 = sh_y0 + xpad_inset;  y1 = sh_y1 - xpad_inset;
    difference() {
        box(x0, x1, y0, y1, zx1 + shield_h + xpad_gap, z_ceil + eps);
        box(x0 + xpad_wall, x1 - xpad_wall, y0 + xpad_wall, y1 - xpad_wall, zx1, z_ceil);
    }
    // USB-C shell top, from the corridor (or, as a positive control, the shell's inner end) to the +X wall
    box(ctl_usbpad_full ? x_mouth - usb_l + xpad_inset : max(x_mouth - usb_l + xpad_inset, wch1 + 0.3), x_in + eps,
        xcy - usb_a, xcy + usb_a, zx1 + usb_h + xpad_gap, z_ceil + eps);
    // shield stop: behind the shield's rear face, between the D3 joint zone and the corridor; its bottom is a pad
    // xpad_gap over the rear edge strip, with a 45 degree lead-in on its front edge. Above the shield it joins the ring.
    sy0 = sh_y1 + shield_stop_clr;
    difference() {
        box(ss_x0, ss_x1, sy0, yx1, zx1 + xpad_gap, z_ceil + eps);
        yz_prism([[sy0 - 1, zx1 + xpad_gap - 1], [sy0 + xlead, zx1 + xpad_gap - eps], [sy0 - 1, zx1 + xpad_gap + xlead + 1]],
                 ss_x0 - 1, ss_x1 + 1);
    }
    box(ss_x0, min(ss_x1, x1), y1 - eps, sy0 + eps, zx1 + shield_h + xpad_gap, z_ceil + eps);
}

// -X end stop: a rib along the XIAO's antenna-end edge, pcb_clr off it, from just behind the antenna back to the
// rear edge, and a web at its rear end out to the -X side wall. Over the DB9 body it stops stop_z_clr above the body
// top; behind the body it reaches 0.6 below the XIAO. A 45 degree lead-in on its bottom inner edge pushes a XIAO
// that sits too far -X back as the lid closes.
function stop_bottom_front() = body_h + stop_z_clr;
module end_stop() {
    yb = y_body1 + pcb_clr;
    difference() {
        union() {
            if (st_y0 < yb) box(st_x0, st_x1, st_y0, min(yb, yx1), stop_bottom_front(), z_ceil + eps);
            if (yx1 - max(st_y0, yb) >= 1.2) box(st_x0, st_x1, max(st_y0, yb), yx1, zx0 - 0.6, z_ceil + eps);
            box(-x_in - eps, st_x1, yx1 - stop_web_t, yx1, zx0 - 0.6, z_ceil + eps);
        }
        for (s = [[stop_bottom_front(), st_y0 - 1, yb], [zx0 - 0.6, yb, yx1 + 1]]) let(zb = s[0]) if (s[2] > s[1])
            xz_prism([[st_x1 + 1, zb - 1], [st_x1 - stop_lead - 1, zb - 1], [st_x1 + 1, zb + stop_lead + 1]], s[1], s[2]);
    }
}

// ---------------------------------------------------------------- ghosts
// Colour + optional section cut, applied per object so cut faces keep their colour.
// Ghosts are cut 0.02 behind the case's cut face (as seen): coplanar cut faces of touching objects merge in the
// render and take one colour (OpenSCAD 2026 with the manifold backend), which painted the case in the wire colour.
module sec(back = 0) {
    if (section == "none") children();
    else intersection() {
        children();
        if (section == "x") box(section_pos - section_t, section_pos - back, -100, 100, -100, 100);
        else if (section == "z") box(-100, 100, -100, 100, section_pos - section_t, section_pos - back);
        else box(-100, 100, section_pos + back, section_pos + section_t, -100, 100);
    }
}
module gcol(c, a = 1, back = 0.02) color(c, a) sec(back) children();
module ghost_db9() {
    gcol("silver") {
        box(-fl_w / 2, fl_w / 2, 0, fl_t, fl_bot, fl_bot + fl_h);
        // D-shell (trapezoid)
        translate([0, 0, hole_z]) rotate([90, 0, 0]) linear_extrude(ds_proj)
            offset(r = 1) offset(delta = -1)
                polygon([[-ds_w / 2, ds_h / 2], [ds_w / 2, ds_h / 2],
                         [ds_w / 2 - 1.5, -ds_h / 2], [-ds_w / 2 + 1.5, -ds_h / 2]]);
    }
    // front plate behind the steel flange: the full flange outline back to fl_plate_d
    gcol("darkgray") box(-fl_w / 2, fl_w / 2, fl_t, ghost_plate_d < 0 ? fl_plate_d : ghost_plate_d, fl_bot, fl_bot + fl_h);
    // lugs either side of the body
    gcol("dimgray") mirror_x_both() box(body_w / 2, board_w / 2, y_fb, lug_end, 0, lug_h);
    // board-lock legs through the lugs and the PCB, with their tails below the board (position estimated), and
    // the keep-out around the tails that no tray feature may enter
    // (gcol outermost: the section slab is cut in assembly coordinates)
    gcol("silver") mirror_x_both() translate([lug_hx + ghost_lug_dx, leg_y + ghost_lug_dy, 0])
        if (leg_t == 0) translate([0, 0, leg_tip]) cylinder(d = leg_w, h = lug_h - leg_tip, $fn = 24);
        else box(-leg_w / 2, leg_w / 2, -leg_t / 2, leg_t / 2, leg_tip, lug_h);
    if (!leg_pocket_on) gcol("orange", 0.3) mirror_x_both() translate([lug_hx + ghost_lug_dx, leg_y + ghost_lug_dy, 0])
        box(-leg_ko_x, leg_ko_x, -leg_ko_y, leg_ko_y, leg_tip, -pcb_t);
    // jack screw tails / nuts behind the flange holes (revision 4), a keep-out
    if (jack_on) gcol("gold", 0.6) translate(ghost_jack_d) mirror_x_both()
        translate([fl_hole_pitch / 2, jack_ys, hole_z]) rotate([-90, 0, 0]) cylinder(d = jack_d, h = jack_l, $fn = 32);
    gcol("dimgray") {
        box(-body_w / 2, body_w / 2, y_fb, y_body1, 0, body_h);
        if (step_on) box(-step_wx / 2, step_wx / 2, y_fb, y_fb + step_d, 0, step_h);
    }
}

module ghost_rs232() {
    gcol("royalblue") difference() {
        box(-board_w / 2, board_w / 2, y_pcb0, y_pcb1, -pcb_t, 0);
        for (s = [-1, 1]) translate([s * mh_x + ghost_mh_dx, mh_y + ghost_mh_dy, -pcb_t - 1]) cylinder(d = mh_d, h = pcb_t + 2);
    }
    // through-hole pin zone under the board (kept 1.25 from the edges), cut back around the corner-pin bosses
    // (plus the board's play at the pins): no pins are assumed within the bosses
    if (show_pin_zone) gcol("red", 0.4) difference() {
        box(-board_w / 2 + 1.25, board_w / 2 - 1.25, max(y_pcb0, y_fb) + 1.25, y_pcb1 - 1.25, -pcb_t - 2.5, -pcb_t);
        mirror_x_both() translate([mh_x, mh_y, 0])
            box(-mh_boss_d / 2 - mh_pin_clr - 0.01, mh_boss_d / 2 + mh_pin_clr + 0.01,
                -mh_boss_d / 2 - mh_pin_clr_y - 0.01, mh_boss_d / 2 + mh_pin_clr_y + 0.01, -10, 0);
    }
    gcol("black") box(-hdr_hw, hdr_hw, hdr_y0, hdr_y1, 0, hdr_h);
}

// Dupont housing on the header and the wire-bend zone above it and forward to the XIAO, up to the ceiling
module ghost_dupont() if (dupont_on) translate(ghost_dupont_d) {
    gcol("dimgray") box(-hdr_hw, hdr_hw, hdr_y0, hdr_y1, 0, dupont_top);
    // the room behind the housing (revision 4): pins, housing and wires lean back here
    gcol("cyan", 0.3) box(-hdr_hw, hdr_hw, hdr_y1, hdr_room, 0, dupont_top);
    gcol("cyan", 0.3) box(-hdr_hw - bend_side, hdr_hw + bend_side, yx1, hdr_room, dupont_top, z_ceil - eps);
}

// XIAO in the case frame: d (from the USB end) along -X, w (across) along -Y
module ghost_xiao() {
    translate(ghost_offset) {
        gcol("#202020") box(xa0, xa1, yx0, yx1, zx0, zx1);
        gcol("whitesmoke") box(sh_x0, sh_x1, sh_y0, sh_y1, zx1, zx1 + shield_h);
        gcol("lightgray") yz_extrude(x_mouth - usb_l, x_mouth) usb_stadium(0);
        gcol("ivory") cbox([dx(ant_d[0]), wy(ant_w[0]), zx1], [dx(ant_d[1]), wy(ant_w[1]), zx1 + 1.1]);   // ceramic antenna
        gcol("gold") cbox([dx(ufl_d[0]), wy(ufl_w[0]), zx1], [dx(ufl_d[1]), wy(ufl_w[1]), zx1 + ufl_h]);  // U.FL + plug
        gcol("dimgray") for (s = [-1, 1]) cbox([dx(btn_d[0]), wy(s * btn_w[0]), zx1], [dx(btn_d[1]), wy(s * btn_w[1]), zx1 + 0.6]);
        // solder blobs on the used pads (edge + top)
        gcol("silver") {
            for (i = front_pads)     translate([pad_x(i), yx0 + 0.4, zx1]) scale([0.9, 1.3, 0.9]) sphere(r = 1);
            for (i = used_pads_rear) translate([pad_x(i), yx1 - 0.4, zx1]) scale([0.9, 1.3, 0.9]) sphere(r = 1);
        }
    }
}

// RX diode (assumed DO-41): body along dio_u from the anode end dio_a to the cathode end dio_k; the anode lead runs
// out of the body and bends to the D3 pad, the cathode lead runs out of the far end to meet the RXD wire
module cyl_between(p, q, d) {
    v = q - p;  L = norm(v);
    translate(p) rotate([0, acos(v[2] / L), atan2(v[1], v[0])]) cylinder(d = d, h = L, $fn = 24);
}
module capsule(pts, d) for (i = [0 : len(pts) - 2]) hull() {
    translate(pts[i]) sphere(d = d, $fn = 16);
    translate(pts[i + 1]) sphere(d = d, $fn = 16);
}
dio_ka = dio_k + dio_u * diode_lead_out;     // cathode lead end
dio_aa = dio_a - dio_u * diode_lead_out;     // anode lead bend
module ghost_diode() if (diode_on) translate(ghost_rear_d + ghost_diode_d) {
    gcol("#303030") cyl_between(dio_a, dio_k, diode_d);
    gcol("lightgray") cyl_between(dio_k - dio_u * 0.9, dio_k - dio_u * 0.4, diode_d + 0.02);   // cathode band
    gcol("silver") { capsule([dio_k, dio_ka], diode_lead_d); capsule([dio_a, dio_aa, dio_joint], diode_lead_d); }
}
// its keep-out: the body and both leads grown by diode_clr
module diode_zone() if (diode_on) translate(ghost_rear_d + ghost_diode_d) {
    cyl_between(dio_a - dio_u * diode_clr, dio_k + dio_u * diode_clr, diode_d + 2 * diode_clr);
    capsule([dio_k, dio_ka], diode_lead_d + 2 * diode_clr);
    capsule([dio_a, dio_aa, dio_joint], diode_lead_d + 2 * diode_clr);
}

module wire(pts) {
    for (i = [0 : len(pts) - 2]) hull() {
        translate(pts[i]) sphere(d = 1.6, $fn = 12);
        translate(pts[i + 1]) sphere(d = 1.6, $fn = 12);
    }
}
// One feasible route (revision 5). Header pins 0, 1 (-X) -> 5V/GND on the front edge: up over the housing, +X to the
// corridor and forward over the USB shell, down onto the pads. Pin 2 -> D5 on the rear edge, forward over the
// housing front face and down. Pin 3 -> the diode's cathode lead beside the housing. Pin 9 -> 5V: under the board to
// the rear edge, up behind it to the ceiling, +X over the housing behind the module wires and on with them.
// Which header pin carries which signal is not modelled: the wires can cross in the bend zone.
module ghost_wires() {
    function hx(i) = -hdr_hw + 1.27 + i * 2.54;
    T = dupont_on ? dupont_top : hdr_h;   // wires leave the top of the Dupont housing (or the header)
    zc = zx1 + usb_h + 1.6;               // corridor run height, over the USB shell
    gcol("limegreen") {
        for (k = [0, 1]) let(px = pad_x(used_pads_front[k]), cx = wch_cx - k * 1.6, zh = T + 3.2 + k * 1.6)
            wire([[hx(k), hdr_yc, T], [hx(k), hdr_yc, zh], [cx, hdr_yc - 0.6, zh], [cx, yx1 - 2, zc + 0.6 + k * 1.6],
                  [cx, yx0 + 4.2, zc], [px, yx0 + 1.4, zx1 + 2.2], [px, yx0 + 0.4, zx1 + 0.8]]);
        // pin 9 -> 5V: under the board to the rear edge, up behind it, over the housing, forward to the 5V pad
        if (p9_on) let(px = pad_x(used_pads_front[0]), zr = -pcb_t - wire_pass / 2, yr = y_pcb1 + wire_pass / 2,
                       zt = z_ceil - 1.0, cx = wch_cx + 1.6, ya = min(yr, hdr_room - 0.8))
            translate(ghost_p9_d) wire([[p9_x, p9_y, -pcb_t - 0.6], [p9_x, p9_y, zr], [p9_run_x, p9_y, zr],
                  [p9_run_x, yr, zr], [p9_run_x, yr, zt], [p9_run_x, ya, zt], [cx, ya, zt], [cx, yx1 - 2, zc + 1.6],
                  [cx, yx0 + 4.2, zc + 0.4], [px, yx0 + 1.4, zx1 + 2.2], [px, yx0 + 0.4, zx1 + 0.8]]);
        // TXD -> D5: forward over the housing front face and down onto the pad
        let(px = pad_x(used_pads_rear[1]))
            wire([[hx(2), hdr_yc, T], [hx(2), hdr_yc, T + 1.0], [px, yx1 + 0.3, T + 0.9], [px, yx1 - 1.0, zx1 + 3],
                  [px, yx1 - 0.4, zx1 + 0.8]]);
        // RXD -> the diode's cathode lead
        if (diode_on) let(ka = dio_ka + ghost_rear_d + ghost_diode_d)
            wire([[hx(3), hdr_yc, T], [hx(3), hdr_yc, max(T, ka[2]) + 0.8], ka]);
    }
}

// Keep-out volumes for the wires, their joints and the diode (the user's requirement): no case feature may enter them
module wire_zones() {
    gcol("magenta", 0.25) {
        // over and in front of the 5V/GND (and 3V3) pads on the front edge (the lid front wall is relieved here)
        for (i = front_pads)
            box(pad_x(i) - joint_half, pad_x(i) + joint_half, yx0 - 0.9, sh_y0, zx0, zx1 + 3);
        // the 5V/GND corridor across the XIAO top, from the front edge back to the rear of the bend zone, and the
        // bend zone's extension over it (+X) and back over the pin 9 riser (-X)
        box(wch_cx - gwch_hw, wch_cx + gwch_hw, yx0, hdr_room, zx1, z_ceil - eps);
        translate(ghost_dupont_d)    // moves with the bend zone it extends
            box(p9_on ? p9_run_x - wire_pass / 2 : -hdr_hw - bend_side, wch_cx + gwch_hw, yx1, hdr_room, dupont_top, z_ceil - eps);
        translate(ghost_rear_d) {
            // over the rear edge strip from the D5 joint to the D3 joint (D5 wire, diode anode lead), up to the ceiling
            box(rear_jx0, rear_jx1, sh_y1, yx1 + eps, zx1, z_ceil - eps);
            // the D3/D5 joints, over and under the rear edge (the XIAO overhangs the RS232 PCB there)
            for (i = used_pads_rear)
                box(pad_x(i) - joint_half, pad_x(i) + joint_half, yx1 - 2.6, yx1 + 0.9, 0.3, zx1 + 3);
        }
        diode_zone();
        // pin 9 wire (revision 4): the joint under the pad, the run under the board to the rear edge, and the
        // riser behind the PCB rear edge up to the ceiling
        if (p9_on) translate(ghost_p9_d) {
            box(p9_x - p9_joint / 2, p9_x + p9_joint / 2, p9_y - p9_joint / 2, p9_y + p9_joint / 2, -pcb_t - p9_joint_h, -pcb_t);
            box(p9_run_x - wire_pass / 2, p9_x, p9_y - wire_pass / 2, p9_y + wire_pass / 2, -pcb_t - wire_pass, -pcb_t);
            box(p9_run_x - wire_pass / 2, p9_run_x + wire_pass / 2, p9_y - wire_pass / 2, y_pcb1, -pcb_t - wire_pass, -pcb_t);
            box(p9_run_x - wire_pass / 2, p9_run_x + wire_pass / 2, y_pcb1, y_pcb1 + wire_pass - eps, -pcb_t - wire_pass, z_ceil - eps);
        }
    }
}

module ghosts(zones = true) {
    translate([ghost_board_dx, ghost_board_dy, 0]) { ghost_db9(); translate([0, 0, ghost_board_dz]) ghost_rs232(); }
    ghost_xiao();
    ghost_diode();
    ghost_dupont();
    ghost_wires();
    if (zones) wire_zones();
}

// ---------------------------------------------------------------- output
module tray_c() { gcol("steelblue", back = 0) tray(); if (logo_mode == "inlay") gcol("white", back = 0) logo_body(); }
module lid_c()  gcol("orange", back = 0) lid();

if (part == "tray") {
    translate([0, 0, -z_bot]) tray();                              // floor on the bed
} else if (part == "lid") {
    translate([0, 0, z_top]) rotate([180, 0, 0]) lid();            // top on the bed
} else if (part == "coupon_tray") {
    // the whole tray, single colour, with the middle of the floor cut out: front skin and lips, ledges, both corner
    // pins on their bosses, snap pockets, tongue
    translate([0, 0, -z_bot]) difference() {
        tray(logo = "none");
        if (coupon_floor) box(-x_in + ledge_w + 1.5, x_in - ledge_w - 1.5, skin_y1 + 3, mh_y - mh_boss_d / 2 - 1.5, z_bot - 1, z_floor + 1);
    }
} else if (part == "coupon_lid") {
    // the lid below coupon_z, printed cut face down: front wall and its plate bearing face, top lip, corner
    // pressers, snap fingers, USB slot; the ceiling and the XIAO pads are left out
    translate([0, 0, coupon_z]) rotate([180, 0, 0]) intersection() { lid(xcap = false); box(-50, 50, -50, 50, -50, coupon_z); }
} else if (part == "logo") {
    translate([0, 0, -z_bot]) logo_body();                         // inlay, same transform as the tray STL
} else if (part == "check_logo") {
    // strokes thinner than logo_min (or corners sharper than logo_min / 2) survive "shape minus its opening";
    // gaps narrower than logo_min survive "closing minus shape". Both must be empty.
    r = logo_min / 2;
    linear_extrude(1) {
        // fine facets everywhere, or the import's 48-gon arcs leave 0.01 mm slivers against the offsets
        difference() { logo_2d($fn = 96); offset(r = r, $fn = 96) offset(r = -r, $fn = 96) logo_2d($fn = 96); }
        difference() { offset(r = -r, $fn = 96) offset(r = r, $fn = 96) logo_2d($fn = 96); logo_2d($fn = 96); }
    }
} else if (part == "assembly") {
    tray_c(); if (show_lid) lid_c(); if (show_ghosts) ghosts(show_zones);
} else if (part == "plugged") {
    // as it hangs on the printer: tray floor up (case -Z to world +Z), case +Y to world +X (DB9 at the left of a top
    // view), case +X to world +Y (the parallel-port side at the top, the USB-C at the bottom)
    multmatrix([[0, 1, 0, 0], [1, 0, 0, 0], [0, 0, -1, 0], [0, 0, 0, 1]]) { tray_c(); lid_c(); if (show_ghosts) ghosts(); }
} else if (part == "exploded") {
    tray_c();
    if (show_ghosts) { ghost_db9(); ghost_rs232(); ghost_xiao(); ghost_dupont(); }
    translate([0, 0, explode]) lid_c();   // the XIAO lies in the tray before the lid closes; wires omitted
} else if (part == "ghosts") {
    ghosts();
} else if (part == "check_tray") {
    intersection() { tray(); ghosts(); }
} else if (part == "check_lid") {
    intersection() { lid(); ghosts(); }
} else if (part == "check_lid_path") {
    // The lid closes straight down over the parts already in the tray, so it collides on the way if any lid
    // material lies directly below a part: sweep the parts 30 mm downwards and intersect with the closed lid.
    // (Wires and wire zones are left out: the wires are flexible and the zones are checked by check_lid. The diode
    // body is rigid, so it is swept as well.)
    intersection() {
        lid();
        minkowski() {
            union() { translate([ghost_board_dx, ghost_board_dy, 0]) { ghost_db9(); translate([0, 0, ghost_board_dz]) ghost_rs232(); }
                      ghost_xiao(); ghost_diode(); ghost_dupont(); }
            translate([0, 0, -30]) cube([0.001, 0.001, 30]);
        }
    }
} else if (part == "check_traylid") {
    intersection() { tray(); translate([0, 0, ghost_lid_dz]) lid(); }
} else if (part == "tray_inplace") {                               // assembly coordinates, for wallprobe.py
    tray();
} else if (part == "lid_inplace") {
    lid();
}
