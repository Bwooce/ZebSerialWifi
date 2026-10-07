// ZebSerialWifi enclosure: YL-97 MAX3232 RS232-TTL board + Seeed XIAO ESP32-C6.
// Two parts (tray + lid) joined by cantilever snap clips, no screws.
// Material PETG, Bambu H2D, 0.4 mm nozzle. All dimensions in mm.
//
// Coordinate frame (assembly):
//   X  across the case (left/right when looking down with the DB9 towards you is -X/+X ... see README)
//      +X is the side of the XIAO 5V/GND column, -X is the header / D0..D6 column side.
//   Y  0 at the DB9 flange FRONT face, +Y towards the rear (USB-C end).
//   Z  0 at the RS232 PCB TOP surface, +Z up.
//
// Usage:
//   openscad -D 'part="tray"' -D show_ghosts=false -o out/tray.stl zebserial_case.scad
//   openscad -D 'part="lid"'  -D show_ghosts=false -o out/lid.stl  zebserial_case.scad
//   part = "tray" | "lid" (print orientation), "assembly" | "exploded" (with optional ghosts),
//          "check_tray" | "check_lid" | "check_traylid" (interference volumes, should be empty),
//          "ghosts" (ghost models only).

/* [View] */
part = "assembly";
show_ghosts = true;
// "none", or a slab: "x" keeps section_pos-section_t < x < section_pos (view from +X),
// "y" keeps section_pos < y < section_pos+section_t (view from the front)
section = "none";
section_pos = 0;
section_t = 3;           // slab thickness, so walls behind the cut do not hide it
explode = 25;
// Positive-control offset for the interference check (moves the XIAO ghost).
ghost_offset = [0, 0, 0];

/* [Given by the user] */
board_w = 31.5;          // RS232 board width
board_d = 28.0;          // RS232 board depth, flange back to rear edge, excluding the DB9
below_clear = 3.0;       // clear space under the RS232 PCB for through-hole pins
max_above = 25.0;        // maximum height envelope above the RS232 board
pcb_t = 1.0;             // RS232 PCB thickness (unknown, user estimate)
hw_face_to_end = 5.1;    // DB9 flange front face to the back of the screw-hole hardware
xiao_overhang = 5.0;     // XIAO PCB rear edge past the RS232 board rear edge (probably less)

/* [XIAO ESP32-C6, Seeed data] */
// Seeed "XIAO Series Package and PCB Design" (2022-08-18), ESP32C3 land pattern page:
// outline 21 x 17.8, 7 pads per side at 2.54 pitch centred on the 21 length.
// The ESP32-C6 shares the outline (wiki: 21 x 17.8 mm).
xiao_l = 21.0;
xiao_w = 17.8;
xiao_pcb_t = 1.0;
xiao_pitch = 2.54;
xiao_pad1 = (xiao_l - 6 * xiao_pitch) / 2;   // 2.88: first pad centre from the USB end
usb_w = 8.94;            // USB-C receptacle width (typical mid-size receptacle)
usb_h = 3.26;            // receptacle height above the XIAO PCB top
usb_l = 7.35;            // receptacle length
usb_over = 1.5;          // receptacle overhang past the XIAO PCB end
shield_h = 2.5;          // RF shield / label can height above the XIAO PCB
// Pad indices counted from the USB end, 1..7.
// Left column (-X):  D0 D1 D2 D3 D4 D5 D6
// Right column (+X): 5V GND 3V3 D10 D9 D8 D7
used_pads_left = [5, 6];     // D4, D5
used_pads_right = [1, 2];    // 5V, GND

/* [Standard right-angle DB9 male defaults, NOT measured] */
fl_w = 30.8;             // flange width
fl_h = 12.5;             // flange height
fl_t = 1.0;              // flange thickness
fl_hole_pitch = 25.0;    // screw-hole spacing
fl_bot = -2.0;           // flange bottom edge relative to the RS232 PCB top
ds_w = 16.9;             // D-shell outer width (top)
ds_h = 8.4;              // D-shell outer height
ds_proj = 6.0;           // D-shell projection forward of the flange
body_w = 20.0;           // plastic rear body width (guess; must sit between the screw hardware)
body_h = 10.5;           // rear body height above the PCB top = XIAO underside
body_d = 9.5;            // rear body depth behind the flange back
hw_d = 5.0;              // diameter of the hardware behind each flange hole (jackscrew + spring)

/* [RS232 header, guesses from photos] */
hdr_pins = 4;
hdr_h = 8.5;             // female header height above the PCB
hdr_edge = 0.3;          // header body outer edge inset from the -X PCB edge (negative = overhangs)
hdr_rear = 0.2;          // header body end inset from the PCB rear edge

/* [Print / tolerance] */
wall = 1.6;              // minimum wall (4 perimeters)
side_t = 3.2;            // side walls (room for flush snap clips)
floor_t = 1.6;
ceil_t = 1.6;            // top skin, also the only material over the XIAO antenna
pcb_clr = 0.2;           // per side on PCB pockets
clip_clr = 0.3;          // on clips
socket_clr = 0.5;        // radial, DB9 hardware sockets (jackscrew must still turn)
socket_back = 1.2;       // material behind the blind sockets
split_z = 4.0;           // tray / lid split plane (side and rear walls)
xiao_top_clear = 4.5;    // XIAO PCB top to lid ceiling (USB-C + wire blobs)
usb_clr = 0.2;
usb_recess_w = 12.0;     // plug overmould relief
usb_recess_h = 7.0;
usb_recess_d = 1.6;      // depth of the relief behind the receptacle mouth
usb_mouth_inset = 0.0;   // receptacle mouth to relief floor (0 = flush, plug pushes on the case)

/* [Logo] */
// Debossed into the lid top, centred; reads upright seen from above with the DB9 at the bottom edge.
show_logo = true;
logo_w = 22.0;           // mark width
logo_depth = 0.5;
logo_file = "../assets/logo/zebserial-mark-deboss.svg";   // from assets/logo/make_logo.py, 1 unit = 1 mm
logo_svg_w = 52.76;      // mark width in that file
logo_svg_c = [32, 30];   // mark centre once imported (OpenSCAD flips the SVG Y axis)

/* [Clips] */
// Lid-to-tray snaps (on both side walls)
lc_w = 6.0;              // finger width
lc_t = 1.4;              // finger thickness
lc_gap = 0.15;           // finger to pocket floor
lc_hook = 0.75;          // hook protrusion
lc_above = 4.0;          // finger free length above the split (slots into the lid wall)
lc_below = 6.0;          // finger length below the split
lc_side_gap = 0.5;       // slot width either side of the finger in the lid wall
lc_front_y = 16.5;       // first lid clip centre (Y)
lc_rear_from_back = 4.2; // second lid clip centre, distance from the interior rear face
// RS232 board side clips in the tray (Y centres)
bc_y_left = [9.5];
bc_y_right = [9.5, 23.2];
bc_w = 4.0;
bc_t = 1.0;
bc_gap = 0.8;
bc_hook = 0.5;
ledge_w = 1.2;           // board support ledge, from the wall inner face
ledge_front_skip = 6.0;  // keep the ledge clear of DB9 board-lock pegs
// Lid pressers that hold the RS232 board down at the edges (Y centres)
press_y_left = [16.0];
press_y_right = [16.0, 27.0];
press_w = 2.0;
press_reach = 1.0;       // over the board edge (from the wall inner face)
// XIAO C-clips hanging from the lid ceiling: [side (-1 left, +1 right), pad index centre]
xc_list = [[-1, 2], [1, 6]];
xc_w = 4.0;
xc_t = 1.2;
xc_hook = 0.6;
// Front stop struts (X centres), bearing on the XIAO front PCB edge; kept clear of the
// ceramic antenna (+X corner) and the U.FL socket (-X corner) so a pigtail can still be fitted
stop_x = [-3.0, 2.0];
stop_w = 3.0;
stop_t = 1.6;            // depth of the part of the strut that reaches below the XIAO PCB

$fn = 48;
eps = 0.01;

// ---------------------------------------------------------------- derived
y_fb = fl_t;                         // flange back face = RS232 PCB front edge
y_pcb1 = y_fb + board_d;             // RS232 PCB rear edge
yx1 = y_pcb1 + xiao_overhang;        // XIAO PCB rear (USB) edge
yx0 = yx1 - xiao_l;                  // XIAO PCB front (antenna) edge
zx0 = body_h;                        // XIAO underside
zx1 = zx0 + xiao_pcb_t;              // XIAO top
y_mouth = yx1 + usb_over;
usb_cz = zx1 + usb_h / 2;
x_in = board_w / 2 + pcb_clr;
x_out = x_in + side_t;
y_in0 = y_fb;
y_in1 = max(y_pcb1, yx1) + pcb_clr;
y_out1 = max(y_mouth + usb_mouth_inset + usb_recess_d, y_in1 + wall);
z_floor = -pcb_t - below_clear;
z_bot = z_floor - floor_t;
z_ceil = zx1 + xiao_top_clear;
z_top = max(z_ceil + ceil_t, usb_cz + usb_recess_h / 2 + wall);
hole_z = fl_bot + fl_h / 2;
socket_d = hw_d + 2 * socket_clr;
socket_end = hw_face_to_end + clip_clr;
y_fw1 = socket_end + socket_back;    // lid front wall rear face
body_cut_x = body_w / 2 + pcb_clr;
usb_slot_w = usb_w + 2 * usb_clr;
hdr_x0 = -board_w / 2 + hdr_edge;
hdr_y1 = y_pcb1 - hdr_rear;
hdr_y0 = hdr_y1 - hdr_pins * 2.54;
lc_ys = [lc_front_y, y_in1 - lc_rear_from_back];
pocket = lc_t + lc_gap;

function pad_y(i) = yx1 - xiao_pad1 - (i - 1) * xiao_pitch;
function lc_hit(y, half) = max([for (c = lc_ys) (abs(y - c) < half + lc_w / 2 + clip_clr + lc_side_gap) ? 1 : 0]) == 1;

echo(str("ZEBCASE outer X ", 2 * x_out, " Y ", y_out1 - y_fb, " Z ", z_top - z_bot,
         " (top ", z_top, " above PCB top, limit ", max_above, ")"));
echo(str("ZEBCASE XIAO Y ", yx0, "..", yx1, " underside z ", zx0, ", lid clips at Y ", lc_ys));
assert(z_top <= max_above, "case taller than the 25 mm envelope above the board");
echo(str("ZEBCASE top skin ", z_top - z_ceil, show_logo ? str(", ", z_top - z_ceil - logo_depth, " under the logo") : ""));
assert(!show_logo || z_top - z_ceil - logo_depth >= 1.0, "logo deboss leaves less than 1 mm of top skin");
if (yx0 - pcb_clr < y_fw1) echo("ZEBCASE WARNING: XIAO front edge reaches into the lid front wall");
for (y = bc_y_left)  if (lc_hit(y, bc_w / 2 + bc_gap)) echo(str("ZEBCASE NOTE: left board clip at Y ", y, " dropped (lid clip pocket)"));
for (y = bc_y_right) if (lc_hit(y, bc_w / 2 + bc_gap)) echo(str("ZEBCASE NOTE: right board clip at Y ", y, " dropped (lid clip pocket)"));
for (c = xc_list) {
    used = c[0] < 0 ? used_pads_left : used_pads_right;
    for (u = used)
        if (abs(pad_y(u) - pad_y(c[1])) < xc_w / 2 + 1.27 + 0.3)
            echo(str("ZEBCASE WARNING: XIAO clip at pad ", c[1], " is near used pad ", u));
}

// ---------------------------------------------------------------- helpers
module box(x0, x1, y0, y1, z0, z1) {
    translate([x0, y0, z0]) cube([x1 - x0, y1 - y0, z1 - z0]);
}
module mirror_x_both() { children(); mirror([1, 0, 0]) children(); }
// Polygon in the XZ plane extruded along Y from y0 to y1.
module xz_prism(pts, y0, y1) {
    translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - y0) polygon(pts);
}

// ---------------------------------------------------------------- tray
module tray() {
    difference() {
        union() {
            difference() {
                box(-x_out, x_out, y_fb, y_out1, z_bot, split_z);
                box(-x_in, x_in, y_in0 - 1, y_in1, z_floor, split_z + 1);
            }
            // front skin under the PCB, supports the PCB front edge
            box(-x_in, x_in, y_fb, y_fb + 1.2, z_floor - eps, -pcb_t);
            // side ledges
            mirror_x_both() box(x_in - ledge_w, x_in + eps, y_fb + ledge_front_skip, y_pcb1 - 0.5, z_floor - eps, -pcb_t);
            // rear tongue that fills the lid USB slot under the receptacle
            box(-usb_slot_w / 2 + clip_clr / 2, usb_slot_w / 2 - clip_clr / 2, y_in1, y_out1, split_z - eps, zx1 - usb_clr);
        }
        // ledge relief at the header pins (header side is -X)
        box(-x_in - eps, -x_in + ledge_w + 0.1, hdr_y0 - 1, hdr_y1 + 1, z_floor, -pcb_t + 0.1);
        // header body relief in the wall
        box(-x_in - 0.6, -x_in + eps, hdr_y0 - 0.5, hdr_y1 + 0.5, 0, split_z + 1);
        // lid clip pockets and catch recesses
        for (c = lc_ys) mirror_x_both() lid_clip_tray_cut(c);
        // board clip slots
        for (y = bc_y_left)  if (!lc_hit(y, bc_w / 2 + bc_gap)) mirror([1, 0, 0]) board_clip_cut(y);
        for (y = bc_y_right) if (!lc_hit(y, bc_w / 2 + bc_gap)) board_clip_cut(y);
        usb_recess_cut();
    }
    for (y = bc_y_left)  if (!lc_hit(y, bc_w / 2 + bc_gap)) mirror([1, 0, 0]) board_clip(y);
    for (y = bc_y_right) if (!lc_hit(y, bc_w / 2 + bc_gap)) board_clip(y);
}

module board_clip_cut(y) {
    box(x_in - eps, x_in + bc_t + bc_gap, y - bc_w / 2 - bc_gap, y + bc_w / 2 + bc_gap, z_bot - 1, split_z + 1);
    // ledge must not tie the finger to the floor
    box(x_in - ledge_w - 0.1, x_in + eps, y - bc_w / 2 - bc_gap, y + bc_w / 2 + bc_gap, z_floor, -pcb_t + 0.1);
}
module board_clip(y) {
    h0 = 0.15;                    // catch face above the PCB top
    top = h0 + bc_hook + 0.4;
    box(x_in, x_in + bc_t, y - bc_w / 2, y + bc_w / 2, z_bot, top);
    xz_prism([[x_in + eps, h0], [x_in - bc_hook, h0], [x_in - bc_hook, h0 + 0.2],
              [x_in, h0 + 0.2 + bc_hook], [x_in, top], [x_in + eps, top]],
             y - bc_w / 2, y + bc_w / 2);
}

// lid-clip geometry, right side (+X); mirrored for the left
lc_tip = split_z - lc_below;
lc_hook_top = lc_tip + 1.4;
module lid_clip_tray_cut(c) {
    hw = lc_w / 2 + clip_clr;
    box(x_out - pocket, x_out + 1, c - hw, c + hw, lc_tip - 1.5, split_z + 1);       // pocket + pry gap
    box(x_out - pocket - lc_hook - 0.1, x_out - pocket + eps, c - hw, c + hw,
        lc_tip - 0.2, lc_hook_top + 0.1);                                              // catch recess
}
module lid_clip_lid_cut(c) {
    hw = lc_w / 2;
    // slots either side and behind the finger, up into the lid wall
    difference() {
        box(x_out - pocket, x_out + 1, c - hw - lc_side_gap, c + hw + lc_side_gap, split_z - 1, split_z + lc_above);
        box(x_out - lc_t, x_out + 1, c - hw, c + hw, split_z - 1, split_z + lc_above + 1);
    }
}
module lid_clip_finger(c) {
    hw = lc_w / 2;
    xi = x_out - lc_t;
    box(xi, x_out, c - hw, c + hw, lc_tip, split_z + eps);
    xz_prism([[xi + eps, lc_tip], [xi, lc_tip], [xi - lc_hook, lc_tip + lc_hook],
              [xi - lc_hook, lc_hook_top], [xi + eps, lc_hook_top]], c - hw, c + hw);
}

module usb_recess_cut() {
    box(-usb_recess_w / 2, usb_recess_w / 2, y_mouth + usb_mouth_inset, y_out1 + 1,
        usb_cz - usb_recess_h / 2, usb_cz + usb_recess_h / 2);
}

// ---------------------------------------------------------------- lid
module lid() {
    difference() {
        union() {
            difference() {
                box(-x_out, x_out, y_fb, y_out1, split_z, z_top);
                box(-x_in, x_in, y_in0 - 1, y_in1, split_z - 1, z_ceil);
                for (c = lc_ys) mirror_x_both() lid_clip_lid_cut(c);
            }
            // front wall above the split
            box(-x_in - eps, x_in + eps, y_fb, y_fw1, split_z, z_top - eps);
            // stop plates behind the screw hardware, reaching down towards the PCB
            mirror_x_both() box(body_cut_x, x_in, socket_end, y_fw1, 0.3, split_z + eps);
            // board pressers
            for (y = press_y_left)  mirror([1, 0, 0]) presser(y);
            for (y = press_y_right) presser(y);
            // XIAO C-clips and front stops
            for (c = xc_list) xiao_clip(c[0], pad_y(c[1]));
            for (x = stop_x) front_stop(x);
            for (c = lc_ys) mirror_x_both() lid_clip_finger(c);
        }
        // DB9 rear body
        box(-body_cut_x, body_cut_x, y_fb - 1, y_fb + body_d + pcb_clr, -1, body_h + pcb_clr);
        // U-shaped blind sockets for the flange hardware (open downwards)
        for (s = [-1, 1]) translate([s * fl_hole_pitch / 2, 0, 0]) hull() {
            translate([0, y_fb - 1, hole_z]) rotate([-90, 0, 0]) cylinder(d = socket_d, h = socket_end - y_fb + 1);
            translate([0, y_fb - 1, -1]) rotate([-90, 0, 0]) cylinder(d = socket_d, h = socket_end - y_fb + 1);
        }
        // header relief
        box(-x_in - 0.6, -x_in + eps, hdr_y0 - 0.5, hdr_y1 + 0.5, 0, hdr_h + 1);
        // USB-C slot, open to the rim so the receptacle drops in; the tray tongue fills below it
        box(-usb_slot_w / 2, usb_slot_w / 2, y_in1 - 1, y_out1 + 1, split_z - 1, zx1 + usb_h + usb_clr);
        usb_recess_cut();
        if (show_logo) logo_cut();
    }
}

// Seen from above, +Y (away from the DB9) is up, so the mark needs no rotation or mirroring.
module logo_cut() {
    translate([0, (y_fb + y_out1) / 2, z_top - logo_depth])
        linear_extrude(logo_depth + 1) scale(logo_w / logo_svg_w) translate(-logo_svg_c) import(logo_file);
}

module presser(y) {
    box(x_in - press_reach, x_in, y - press_w / 2, y + press_w / 2, 0.1, split_z + eps);
}

module xiao_clip(s, yc) {
    xe = xiao_w / 2 + pcb_clr;          // finger inner face
    z_hook_top = zx0 - 0.1;
    z_tip = z_hook_top - xc_hook - 0.5;
    z_lip = zx1 + 0.1;
    mirror([s < 0 ? 1 : 0, 0, 0]) {
        box(xe, xe + xc_t, yc - xc_w / 2, yc + xc_w / 2, z_tip, z_ceil + eps);
        // hook under the PCB, lead-in facing down (towards the incoming board)
        xz_prism([[xe + eps, z_tip], [xe, z_tip], [xe - xc_hook, z_tip + xc_hook],
                  [xe - xc_hook, z_hook_top], [xe + eps, z_hook_top]], yc - xc_w / 2, yc + xc_w / 2);
        // lip over the (unused) pads, 45 degree top so it prints upside down
        xz_prism([[xe + eps, z_lip], [xe - xc_hook, z_lip], [xe - xc_hook, z_lip + 0.3],
                  [xe, z_lip + 0.3 + xc_hook], [xe + eps, z_lip + 0.3 + xc_hook]], yc - xc_w / 2, yc + xc_w / 2);
    }
}

// Front stop: a compression strut from the lid front wall back to the XIAO front edge, so the
// USB insertion force goes straight into the front wall and the DB9 hardware sockets rather
// than bending a thin tab across its layer lines.
module front_stop(x) {
    y1 = yx0 - pcb_clr;                         // bearing face on the XIAO PCB edge
    body_end = y_fb + body_d + pcb_clr;
    tip0 = max(y1 - stop_t, body_end);          // tip behind the DB9 body can reach below the PCB
    box(x - stop_w / 2, x + stop_w / 2, y_fw1 - eps, y1, body_h + pcb_clr, z_ceil + eps);
    if (y1 > tip0) box(x - stop_w / 2, x + stop_w / 2, tip0, y1, zx0 - 0.6, z_ceil + eps);
}

// ---------------------------------------------------------------- ghosts
// Colour + optional section cut, applied per object so cut faces keep their colour.
module sec() {
    if (section == "none") children();
    else intersection() {
        children();
        if (section == "x") box(section_pos - section_t, section_pos, -100, 100, -100, 100);
        else box(-100, 100, section_pos, section_pos + section_t, -100, 100);
    }
}
module gcol(c, a = 1) color(c, a) sec() children();
module ghost_db9() {
    gcol("silver") {
        box(-fl_w / 2, fl_w / 2, 0, fl_t, fl_bot, fl_bot + fl_h);
        // D-shell (trapezoid)
        translate([0, 0, hole_z]) rotate([90, 0, 0]) linear_extrude(ds_proj)
            offset(r = 1) offset(delta = -1)
                polygon([[-ds_w / 2, ds_h / 2], [ds_w / 2, ds_h / 2],
                         [ds_w / 2 - 1.5, -ds_h / 2], [-ds_w / 2 + 1.5, -ds_h / 2]]);
        for (s = [-1, 1]) translate([s * fl_hole_pitch / 2, fl_t - eps, hole_z])
            rotate([-90, 0, 0]) cylinder(d = hw_d, h = hw_face_to_end - fl_t);
    }
    gcol("dimgray") box(-body_w / 2, body_w / 2, y_fb, y_fb + body_d, 0, body_h);
}

module ghost_rs232() {
    gcol("royalblue") box(-board_w / 2, board_w / 2, y_fb, y_pcb1, -pcb_t, 0);
    // through-hole pin zone under the board (kept 1.2 from the edges)
    gcol("red", 0.4) box(-board_w / 2 + 1.25, board_w / 2 - 1.25, y_fb + 1.25, y_pcb1 - 1.25, -pcb_t - 2.5, -pcb_t);
    gcol("black") box(hdr_x0, hdr_x0 + 2.54, hdr_y0, hdr_y1, 0, hdr_h);
}

module ghost_xiao() {
    translate(ghost_offset) {
        gcol("#202020") box(-xiao_w / 2, xiao_w / 2, yx0, yx1, zx0, zx1);
        gcol("whitesmoke") box(-6.3, 6.3, yx0 + 2.5, yx1 - 5.0, zx1, zx1 + shield_h);
        gcol("lightgray") box(-usb_w / 2, usb_w / 2, y_mouth - usb_l, y_mouth, zx1, zx1 + usb_h);
        gcol("ivory") box(4.0, 8.0, yx0 + 0.5, yx0 + 3.0, zx1, zx1 + 1.1);        // ceramic antenna
        gcol("gold") box(-7.6, -4.6, yx0 + 0.5, yx0 + 3.5, zx1, zx1 + 1.25);      // U.FL
        // solder blobs on the used pads (edge + top)
        gcol("silver") {
            for (i = used_pads_left)  translate([-xiao_w / 2 + 0.4, pad_y(i), zx1]) scale([1.3, 0.9, 0.9]) sphere(r = 1);
            for (i = used_pads_right) translate([ xiao_w / 2 - 0.4, pad_y(i), zx1]) scale([1.3, 0.9, 0.9]) sphere(r = 1);
        }
    }
}

module wire(pts) {
    for (i = [0 : len(pts) - 2]) hull() {
        translate(pts[i]) sphere(d = 1.6, $fn = 12);
        translate(pts[i + 1]) sphere(d = 1.6, $fn = 12);
    }
}
module ghost_wires() {
    hx = hdr_x0 + 1.27;
    function hy(i) = hdr_y1 - 1.27 - i * 2.54;
    gcol("limegreen") translate(ghost_offset * 0) {
        // header pins 0,1 -> 5V/GND (under the XIAO to the +X side), 2,3 -> D4/D5
        for (k = [0, 1]) {
            py = pad_y(used_pads_right[k]);
            wire([[hx, hy(k), hdr_h], [hx + 1, hy(k), hdr_h + 0.8], [hx + 2.5, py, 6.5],
                  [xiao_w / 2 + 3, py, 6.5], [xiao_w / 2 + 2.2, py, zx1 + 1.2], [xiao_w / 2 - 0.4, py, zx1 + 0.8]]);
        }
        for (k = [0, 1]) {
            py = pad_y(used_pads_left[k]);
            wire([[hx, hy(k + 2), hdr_h], [hx + 0.5, hy(k + 2), zx1 + 1.5], [-xiao_w / 2 - 2.5, py, zx1 + 1.8],
                  [-xiao_w / 2 + 0.4, py, zx1 + 0.8]]);
        }
    }
}

module ghosts() {
    ghost_db9();
    ghost_rs232();
    ghost_xiao();
    ghost_wires();
}

// ---------------------------------------------------------------- output
module tray_c() color("steelblue") sec() tray();
module lid_c()  color("orange") sec() lid();

if (part == "tray") {
    translate([0, 0, -z_bot]) tray();                              // floor on the bed
} else if (part == "lid") {
    translate([0, 0, z_top]) rotate([180, 0, 0]) lid();            // top on the bed
} else if (part == "assembly") {
    tray_c(); lid_c(); if (show_ghosts) ghosts();
} else if (part == "exploded") {
    tray_c();
    if (show_ghosts) { ghost_db9(); ghost_rs232(); }
    translate([0, 0, explode]) { lid_c(); if (show_ghosts) ghost_xiao(); }   // wires omitted when exploded
} else if (part == "ghosts") {
    ghosts();
} else if (part == "check_tray") {
    intersection() { tray(); ghosts(); }
} else if (part == "check_lid") {
    intersection() { lid(); ghosts(); }
} else if (part == "check_traylid") {
    intersection() { tray(); lid(); }
}
