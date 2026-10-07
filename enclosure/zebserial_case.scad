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
//          "ghosts" (ghost models only), "tray_inplace" | "lid_inplace" (assembly coordinates, for wallprobe.py).

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
ghost_lug_dy = 0;        // positive control: moves only the ghost lug holes in Y

/* [Measured by the user (calipers)] */
board_w = 31.5;          // RS232 board width
board_d = 26.0;          // RS232 PCB depth, its front edge to its rear edge
pcb_front_y = 0;         // DB9 flange FRONT face to the RS232 PCB front edge: flush, so the PCB runs under the flange
below_clear = 3.0;       // clear space under the RS232 PCB for through-hole pins
max_above = 25.0;        // maximum height envelope above the RS232 board
pcb_t = 1.6;             // RS232 PCB thickness
hw_face_to_end = 5.1;    // old note: flange front face to the back of the screw-hole hardware (used only as a cross-check)
// XIAO fore-aft position, chosen (the user can place it +-1.5 mm): flange FRONT face to the XIAO front (antenna)
// PCB edge. 9.1 keeps the D4/D5 underside solder-joint zone behind the DB9 body and still leaves 3.3 mm of the
// XIAO resting on the body; 7.5 would make the case 1.6 mm shorter but only if D5 is soldered on top.
xiao_front_y = 9.1;

/* [XIAO ESP32-C6, Seeed data] */
// Seeed XIAO ESP32-C6 v1.0 KiCad PCB (enclosure/reference/xiao_esp32c6) and its STEP model.
// d = distance from the USB end of the PCB, X as in the case frame (+X = 5V/GND column).
xiao_l = 20.955;
xiao_w = 17.78;
xiao_pcb_t = 1.2;        // measured
xiao_pitch = 2.54;
xiao_pad1 = 2.921;       // pad 1 (5V / D0) centre from the USB end; the pads are not centred on the length
usb_w = 8.94;            // GCT USB4105 receptacle (drawing in reference/)
usb_h = 3.3;             // measured (4.5 overall at the USB-C); shell bottom sits on the PCB top
usb_l = 7.35;
usb_over = 1.5;          // receptacle mouth past the XIAO PCB end
shield_w = 12.6;         // RF shield / label can
shield_d0 = 6.71;        // shield spans d 6.71 .. 17.31
shield_d1 = 17.31;
shield_h = 2.2;          // measured (3.4 overall over the shield)
ant_x = [0.52, 5.72];    // ceramic antenna ANT1, X range and d range
ant_d = [18.8, 20.8];
ufl_x = [-5.6, -3.0];    // U.FL socket ANT2
ufl_d = [18.25, 20.85];
ufl_h = 2.5;             // with a mated plug
btn_x = [5.3, 7.9];      // BOOT / RESET buttons, both sides (|X|)
btn_d = [0.3, 1.9];
// Pad indices counted from the USB end, 1..7.
// Left column (-X):  D0 D1 D2 D3 D4 D5 D6
// Right column (+X): 5V GND 3V3 D10 D9 D8 D7
used_pads_left = [5, 6];     // D4, D5
used_pads_right = [1, 2];    // 5V, GND

/* [Right-angle DB9 male, from Norcomp 182-YYY-113RYY1, Connfly DS1037-09M, PDI p.177] */
fl_w = 30.8;             // flange width
fl_h = 12.5;             // flange height
fl_t = 0.38;             // steel flange thickness; plastic behind it counts as body
fl_hole_pitch = 25.0;    // screw-hole spacing
fl_bot = 0;              // flange bottom relative to the RS232 PCB top: flush with the body bottom (seating plane)
ds_w = 16.9;             // D-shell outer width (top)
ds_h = 8.4;              // D-shell outer height
ds_proj = 6.0;           // D-shell projection forward of the flange
body_w = 17.2;           // measured: main rear body width
body_h = 10.4;           // measured: rear body height above the PCB top = XIAO underside
body_end = 12.42;        // flange FRONT face to the back of the rear body (drawing value, NOT measured)
body_d = body_end - fl_t;
step_on = false;         // taller step right behind the flange (Norcomp); no information for this part
step_h = 12.2;
step_d = 2.9;
step_w = -1;             // -1 = body_w
// Lugs: plastic each side of the body, 3.4 high, out to the full PCB width, with a vertical screw hole
lug_h = 3.4;             // measured
lug_end = 6.5;           // flange FRONT face to the lug rear faces (NOT measured)
lug_hole_y = 4.6;        // measured: hole centre behind the flange front face
lug_hole_x = -1;         // hole centre |X|; -1 = fl_hole_pitch / 2 (NOT measured)
lug_hole_d = 2.5;        // NOT measured, print-blocking for the pins
lug_hole_depth = 3.4;    // ghost only
lug_pin_on = true;       // lid pins drop into the lug holes and stop the case sliding rearwards
pin_d = 2.2;
pin_flat = 1.6;          // pin width across X (flats), so an X error in the hole position cannot jam the lid
pin_engage = 1.5;        // pin length inside the hole
pin_boss_d = 4.0;        // boss around the pin root, down to 0.2 above the lug top

/* [RS232 header, guesses from photos] */
hdr_pins = 4;
hdr_h = 8.5;             // female header height above the PCB
hdr_edge = 0.3;          // header body outer edge inset from the -X PCB edge (negative = overhangs)
hdr_rear = 0.2;          // header body end inset from the PCB rear edge

/* [Print / tolerance] */
wall = 1.6;              // minimum wall (4 perimeters)
side_t = 4.0;            // side walls at the corners; the pincushion sag thins them towards mid-span
floor_t = 1.6;
ceil_t = 1.6;            // top skin, also the only material over the XIAO antenna
pcb_clr = 0.2;           // per side on PCB pockets
clip_clr = 0.3;          // on clips
split_z = 4.0;           // tray / lid split plane (side and rear walls)
xiao_top_clear = 4.5;    // XIAO PCB top to lid ceiling (USB-C + wire blobs)
usb_clr = 0.2;
usb_recess_w = 12.0;     // plug overmould relief
usb_recess_h = 7.0;
usb_recess_d = 1.6;      // depth of the relief behind the receptacle mouth
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
lc_pry = 0.9;            // gap under the finger tip for a screwdriver (keeps min_wall above the bottom chamfer)
lc_front_y = 16.5;       // first lid clip centre (Y)
lc_rear_from_back = 4.2; // second lid clip centre, distance from the interior rear face
// RS232 board side clips in the tray (Y centres)
bc_y_left = [9.5];
bc_y_right = [9.5, 23.25];
bc_w = 3.5;
bc_side_gap = 0.5;       // slot either side of the finger (Y)
bc_t = 1.0;
bc_gap = 0.8;
bc_hook = 0.5;
ledge_w = 1.2;           // board support ledge, from the wall inner face
ledge_front_skip = 7.0;  // keep the ledge clear of DB9 board-lock pegs (and of the first board clip slot)
// Lid pressers that hold the RS232 board down at the edges (Y centres)
press_y_left = [12.5];   // in front of the header (header starts at Y 15.6)
press_y_right = [16.0, 24.0];
press_w = 2.0;
press_reach = 1.2;       // over the board edge (from the wall inner face)
// XIAO side clamps hanging from the lid: [side (-1 / +1), d0, d1] along the unused-pad stretches.
// -X: D0-D3 (clear of the -X button at d < 1.9 and of D4/D5); +X: D9-D7 (clear of 5V/GND).
xclamps = [[-1, 2.2, 10.5], [1, 11.8, 18.8]];
xrest = [-1, 17.4, 19.0];      // extra -X clamp on D6, added when the XIAO overlaps the DB9 body by < front_rest_min
front_rest_min = 2.0;
clamp_t = 1.2;           // flexing finger thickness
clamp_ledge = 1.0;       // ledge under the PCB edge (0.8 mm of it under the PCB)
clamp_lip = 1.0;         // lip over the PCB edge
clamp_gap = 0.1;         // ledge below / lip above the PCB: the DB9 body and the tongue are the datums
tongue_gap = 0.05;       // tray tongue top to the USB shell bottom (rear support)
// Front stop strut (X centres), bearing on the XIAO front PCB edge in the gap between the U.FL
// socket (X -5.6..-3.0) and the ceramic antenna (X 0.5..5.7)
stop_x = [-1.25];
stop_w = 3.0;
stop_t = 1.6;            // depth of the part of the strut that can reach below the XIAO PCB (behind the body)
stop_tip_min = 0.8;
stop_z_clr = 0.1;        // strut bottom above the DB9 body top where the XIAO rests on the body

$fn = 48;
eps = 0.01;

// ---------------------------------------------------------------- derived
y_fb = fl_t;                         // flange back face = case front face
y_pcb0 = pcb_front_y < 0 ? fl_t : pcb_front_y;   // RS232 PCB front edge
y_pcb1 = y_pcb0 + board_d;           // RS232 PCB rear edge
yx0 = xiao_front_y;                  // XIAO PCB front (antenna) edge
yx1 = yx0 + xiao_l;                  // XIAO PCB rear (USB) edge
xiao_overhang = yx1 - y_pcb1;        // derived: past the RS232 PCB rear edge
y_body1 = y_fb + body_d;             // back of the DB9 rear body
body_overlap = y_body1 - yx0;        // how far the XIAO front end lies on the body top
front_on_body = body_overlap >= front_rest_min && body_w >= xiao_w - 2;   // the body carries nearly the full XIAO width
step_wx = step_w < 0 ? body_w : step_w;
lug_hx = lug_hole_x < 0 ? fl_hole_pitch / 2 : lug_hole_x;
// D4/D5 underside solder-joint zone (half a pitch plus a blob either side of the pads)
joint_half = xiao_pitch / 2 + 0.5;
zx0 = body_h;                        // XIAO underside
zx1 = zx0 + xiao_pcb_t;              // XIAO top
y_mouth = yx1 + usb_over;
usb_cz = zx1 + usb_h / 2;
x_in = board_w / 2 + pcb_clr;
x_out = x_in + side_t;               // outermost X (at the corner ends of the side faces)
y_in0 = y_fb;
y_in1 = max(y_pcb1, yx1) + pcb_clr;
y_rear_face = max(y_mouth + usb_mouth_inset + usb_recess_d, y_in1 + wall);   // rear face at its mid-span
rear_sag = sag_frac * 2 * x_out;
y_out1 = y_rear_face + rear_sag;     // outermost Y; the concave rear face comes back to y_rear_face at mid-span
z_floor = -pcb_t - below_clear;
z_bot = z_floor - floor_t;
// The top also has to clear the USB overmould relief by min_wall measured square to the 45 degree top chamfer.
z_top = max(zx1 + xiao_top_clear + ceil_t, usb_cz + usb_recess_h / 2 + wall,
            usb_cz + usb_recess_h / 2 + edge_chamfer + min_wall * sqrt(2));
z_ceil = z_top - ceil_t;             // the top stays a single ceil_t skin over the XIAO antenna
hole_z = fl_bot + fl_h / 2;
y_fw1 = lug_hole_y + max(lug_hole_d, pin_boss_d) / 2 + min_wall;   // lid front wall rear face (holds the pins)
body_cut_x = body_w / 2 + pcb_clr;
usb_slot_w = usb_w + 2 * usb_clr;
hdr_x0 = -board_w / 2 + hdr_edge;
hdr_relief = hdr_edge >= pcb_clr ? 0 : pcb_clr - hdr_edge + 0.2;   // wall relief only if the header overhangs
hdr_y1 = y_pcb1 - hdr_rear;
skin_y1 = max(y_fb, y_pcb0) + 1.2;   // front skin under the PCB front edge
hdr_y0 = hdr_y1 - hdr_pins * 2.54;
lc_ys = [lc_front_y, y_in1 - lc_rear_from_back];
pocket = lc_t + lc_gap;

// ---- exterior outline (plan), CCW seen from above
side_sag = sag_frac * (y_out1 - y_fb);
function side_mid(a, b, sg) = sg <= 0 ? [] :
    let(d = b - a, n = [-d[1], d[0]] / norm(d), t1 = (1 - facet_flat) / 2, t2 = (1 + facet_flat) / 2)
    facet_flat > 0 ? [a + d * t1 + n * sg, a + d * t2 + n * sg] : [a + d * 0.5 + n * sg];
function outline() = let(X = x_out, c = corner_chamfer, y0 = y_fb, Y1 = y_out1,
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
function surf_x(y) = let(a = y_fb + corner_chamfer, b = y_out1 - corner_chamfer, t = (y - a) / (b - a),
        t1 = (1 - facet_flat) / 2, t2 = (1 + facet_flat) / 2,
        sg = (t <= 0 || t >= 1) ? 0 : t < t1 ? side_sag * t / t1 : t > t2 ? side_sag * (1 - t) / (1 - t2) : side_sag)
    x_out - sg;
// lowest X of the side face over a Y span: the flat that the snap finger and pocket are cut from
function face_x(y0, y1) = let(a = y_fb + corner_chamfer, b = y_out1 - corner_chamfer,
        f0 = a + (b - a) * (1 - facet_flat) / 2, f1 = a + (b - a) * (1 + facet_flat) / 2)
    min(surf_x(y0), surf_x(y1), (y0 < f1 && y1 > f0) ? x_out - side_sag : 1e9);
function lc_face(c) = face_x(c - lc_w / 2 - clip_clr - lc_side_gap, c + lc_w / 2 + clip_clr + lc_side_gap);

function pad_y(i) = yx1 - xiao_pad1 - (i - 1) * xiao_pitch;
// true if a board clip slot of half-width `half` at Y would leave less than min_wall to a lid clip pocket
function lc_hit(y, half) = max([for (c = lc_ys) (abs(y - c) < half + lc_w / 2 + clip_clr + min_wall) ? 1 : 0]) == 1;
bc_half = bc_w / 2 + bc_side_gap;

echo(str("ZEBCASE outer X ", 2 * x_out, " Y ", y_out1 - y_fb, " Z ", z_top - z_bot,
         " (top ", z_top, " above PCB top, limit ", max_above, ")"));
echo(str("ZEBCASE style: side sag ", side_sag, ", rear sag ", rear_sag, ", front sag ", front_sag,
         ", corner chamfer ", corner_chamfer, ", corner facet ", corner_facet, ", edge chamfer ", edge_chamfer,
         ", snap faces at X ", [for (c = lc_ys) lc_face(c)]));
echo(str("ZEBCASE XIAO Y ", yx0, "..", yx1, " underside z ", zx0, ", lid clips at Y ", lc_ys));
assert(z_top <= max_above, "case taller than the 25 mm envelope above the board");
echo(str("ZEBCASE top skin ", z_top - z_ceil, show_logo ? str(", ", z_top - z_ceil - logo_depth, " under the logo") : ""));
assert(!show_logo || z_top - z_ceil - logo_depth >= 1.0, "logo deboss leaves less than 1 mm of top skin");
if (yx0 - pcb_clr < y_fw1) echo("ZEBCASE WARNING: XIAO front edge reaches into the lid front wall");
echo(str("ZEBCASE XIAO front Y ", yx0, ", rear Y ", yx1, ", overhang ", xiao_overhang, " past the RS232 PCB; on the body ",
         body_overlap, " mm; strut depth ", yx0 - pcb_clr - y_fw1, " mm"));
d5_front = pad_y(max(used_pads_left)) - joint_half;     // front of the D4/D5 underside joint zone
if (d5_front < y_body1 + pcb_clr)
    echo(str("ZEBCASE WARNING: the D4/D5 underside solder-joint zone starts at Y ", d5_front,
             ", over the DB9 body (ends ", y_body1, "): only OK if those joints are on the top side"));
else echo(str("ZEBCASE wire zone: D4/D5 underside joints start ", d5_front - y_body1, " mm behind the DB9 body"));
echo(lug_pin_on ? str("ZEBCASE ANTI-SLIDE: lid pins (", pin_d, " x ", pin_flat, " flatted) in the lug holes at Y ", lug_hole_y,
                      ", X +-", lug_hx, " (hole ", lug_hole_d, ")")
     : "ZEBCASE ANTI-SLIDE: pins off - only friction stops the case sliding rearwards off the connector");
if (lug_pin_on && pin_d > lug_hole_d - 0.2) echo("ZEBCASE WARNING: pin_d is too big for lug_hole_d");
if (abs(lug_hole_y + lug_hole_d / 2 - hw_face_to_end) > 0.2)
    echo(str("ZEBCASE NOTE: lug hole back edge at Y ", lug_hole_y + lug_hole_d / 2, " disagrees with the old 5.1 note: measure lug_hole_d"));
echo(str("ZEBCASE SUPPORT front: ", front_on_body ? str("rests on the DB9 body top, overlap ", body_overlap, " mm")
         : str(body_overlap > 0 ? str("only ", body_overlap, " mm on the DB9 body, so ") : "not on the DB9 body, so ",
               "the lid adds a front rest (-X clamp on D6) beside the front of the +X clamp"),
         "; rear: tray tongue ", tongue_gap, " mm under the USB shell; sides: clamps ", xclamps));
for (x = stop_x) let(b = stop_bottom(x))
    echo(str("ZEBCASE SUPPORT front stop at X ", x, ": bears on ", zx1 - max(b, zx0), " of ", xiao_pcb_t,
             " mm of the PCB edge", b < zx0 ? " (full edge)" : " (stops above the body top)"));
for (y = bc_y_left)  if (lc_hit(y, bc_half)) echo(str("ZEBCASE NOTE: left board clip at Y ", y, " dropped (lid clip pocket)"));
for (y = bc_y_right) if (lc_hit(y, bc_half)) echo(str("ZEBCASE NOTE: right board clip at Y ", y, " dropped (lid clip pocket)"));
// clamps must stay 1.27 + 0.4 mm (half a pad plus the solder blob) away from every used pad
for (c = concat(xclamps, [xrest])) {
    used = c[0] < 0 ? used_pads_left : used_pads_right;
    for (u = used) let(dp = xiao_pad1 + (u - 1) * xiao_pitch)
        if (dp > c[1] - 1.67 && dp < c[2] + 1.67)
            echo(str("ZEBCASE WARNING: XIAO clamp ", c, " is over or next to used pad ", u));
}
echo(str("ZEBCASE clamp finger strain ", clamp_strain(z_ceil) * 100, " %"));

// ---------------------------------------------------------------- helpers
module box(x0, x1, y0, y1, z0, z1) {
    translate([x0, y0, z0]) cube([x1 - x0, y1 - y0, z1 - z0]);
}
module mirror_x_both() { children(); mirror([1, 0, 0]) children(); }
// Polygon in the XZ plane extruded along Y from y0 to y1.
module xz_prism(pts, y0, y1) {
    translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - y0) polygon(pts);
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
                B = [sx * x_out, cy ? y_out1 : y_fb, cz ? z_top : z_bot];
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
        box(-x_out - 5, x_out + 5, y_fb - 5, y_out1 + 5, split_z - 1, split_z + reveal_h);
        translate([0, 0, split_z - 2]) linear_extrude(reveal_h + 3) polygon(offset_poly(outline(), reveal_d));
        for (c = lc_ys) mirror_x_both()
            box(x_in, x_out + 6, c - lc_w / 2, c + lc_w / 2, split_z - 2, split_z + 2);   // groove runs into the side slots: no webs
    }
}

// ---------------------------------------------------------------- tray
module tray() intersection() {
  outer_solid();
  union() {
    difference() {
        union() {
            difference() {
                intersection() { outer_solid(); box(-x_out - 1, x_out + 1, y_fb - 1, y_out1 + 1, z_bot - 1, split_z); }
                box(-x_in, x_in, y_in0 - 1, y_in1, z_floor, split_z + 1);
            }
            // front skin under the PCB, supports the PCB front edge
            box(-x_in, x_in, y_fb, skin_y1, z_floor - eps, -pcb_t);
            // small gusset in the floor / front skin corner: keeps min_wall square to the bottom front chamfer
            fg = max(0, edge_chamfer + min_wall * sqrt(2) - floor_t - 1.2);
            if (fg > 0) translate([x_in, 0, 0]) rotate([0, -90, 0]) linear_extrude(2 * x_in)
                polygon([[z_floor - eps, skin_y1 - eps], [z_floor - eps, skin_y1 + fg], [z_floor + fg, skin_y1 - eps]]);
            // side ledges
            mirror_x_both() box(x_in - ledge_w, x_in + eps, y_pcb0 + ledge_front_skip, y_pcb1 - 0.5, z_floor - eps, -pcb_t);
            // rear tongue that fills the lid USB slot under the receptacle
            box(-usb_slot_w / 2 + clip_clr / 2, usb_slot_w / 2 - clip_clr / 2, y_in1, y_out1, split_z - eps, zx1 - tongue_gap);
        }
        // ledge relief at the header pins (header side is -X)
        box(-x_in, -x_in + ledge_w + 0.1, hdr_y0 - 1, hdr_y1 + 1, z_floor, -pcb_t + 0.1);
        // header body relief in the wall (only when the header overhangs the PCB edge)
        if (hdr_relief > 0) box(-x_in - hdr_relief, -x_in + eps, hdr_y0 - 0.5, hdr_y1 + 0.5, 0, split_z + 1);
        // lid clip pockets and catch recesses
        for (c = lc_ys) mirror_x_both() lid_clip_tray_cut(c);
        // board clip slots
        for (y = bc_y_left)  if (!lc_hit(y, bc_half)) mirror([1, 0, 0]) board_clip_cut(y);
        for (y = bc_y_right) if (!lc_hit(y, bc_half)) board_clip_cut(y);
        usb_recess_cut();
    }
    for (y = bc_y_left)  if (!lc_hit(y, bc_half)) mirror([1, 0, 0]) board_clip(y);
    for (y = bc_y_right) if (!lc_hit(y, bc_half)) board_clip(y);
  }
}

module board_clip_cut(y) {
    box(x_in - eps, x_in + bc_t + bc_gap, y - bc_half, y + bc_half, z_floor, split_z + 1);
    // ledge must not tie the finger to the floor
    box(x_in - ledge_w - 0.1, x_in, y - bc_half - 0.5, y + bc_half + 0.5, z_floor, -pcb_t + 0.1);   // no ledge slivers
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
// All snap geometry is measured from xo = the lowest point of the concave side face over the
// clip's span, so the bow never thins the finger or the wall behind its pocket.
module lid_clip_tray_cut(c) {
    hw = lc_w / 2 + clip_clr;
    xo = lc_face(c);
    box(xo - pocket, x_out + 1, c - hw, c + hw, lc_tip - lc_pry, split_z + 1);        // pocket + pry gap
    // catch window, through to the inside (below the board, beside the ledge): a blind recess
    // would leave less than min_wall behind it at mid-span
    box(x_in, xo - pocket + eps, c - hw, c + hw, lc_tip - 0.2, lc_hook_top + 0.1);
}
module lid_clip_lid_cut(c) {
    hw = lc_w / 2;
    xo = lc_face(c);
    // slots either side and behind the finger, up into the lid wall
    difference() {
        box(xo - pocket, x_out + 1, c - hw - lc_side_gap, c + hw + lc_side_gap, split_z - 1, split_z + lc_above);
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

module usb_recess_cut() {
    box(-usb_recess_w / 2, usb_recess_w / 2, y_mouth + usb_mouth_inset, y_out1 + 1,
        usb_cz - usb_recess_h / 2, usb_cz + usb_recess_h / 2);
}

// ---------------------------------------------------------------- lid
module lid() intersection() {
  outer_solid();
    difference() {
        union() {
            difference() {
                intersection() { outer_solid(); box(-x_out - 1, x_out + 1, y_fb - 1, y_out1 + 1, split_z, z_top + 1); }
                box(-x_in, x_in, y_in0 - 1, y_in1, split_z - 1, z_ceil);
                for (c = lc_ys) mirror_x_both() lid_clip_lid_cut(c);
            }
            // front wall above the split
            box(-x_in - eps, x_in + eps, y_fb, y_fw1, split_z, z_top - eps);
            if (lug_pin_on) mirror_x_both() lug_pin();
            // board pressers
            for (y = press_y_left)  mirror([1, 0, 0]) presser(y);
            for (y = press_y_right) presser(y);
            // XIAO C-clips and front stops
            for (c = xclamps) xiao_clamp(c[0], c[1], c[2]);
            if (!front_on_body) xiao_clamp(xrest[0], xrest[1], xrest[2]);
            for (x = stop_x) front_stop(x);
            for (c = lc_ys) mirror_x_both() lid_clip_finger(c);
        }
        // DB9 rear body
        box(-body_cut_x, body_cut_x, y_fb - 1, y_body1 + pcb_clr, -1, body_h + pcb_clr);
        if (step_on) box(-step_wx / 2 - pcb_clr, step_wx / 2 + pcb_clr, y_fb - 1, y_fb + step_d + pcb_clr, -1, step_h + pcb_clr);
        // header relief (only when the header overhangs the PCB edge)
        if (hdr_relief > 0) box(-x_in - hdr_relief, -x_in + eps, hdr_y0 - 0.5, hdr_y1 + 0.5, 0, hdr_h + 1);
        // USB-C slot, open to the rim so the receptacle drops in; the tray tongue fills below it
        box(-usb_slot_w / 2, usb_slot_w / 2, y_in1 - 1, y_out1 + 1, split_z - 1, zx1 + usb_h + usb_clr);
        usb_recess_cut();
        if (show_logo) logo_cut();
        if (seam_reveal) reveal_cut();
    }
}

// Seen from above, +Y (away from the DB9) is up, so the mark needs no rotation or mirroring.
module logo_cut() {
    translate([0, (y_fb + y_rear_face) / 2, z_top - logo_depth])
        linear_extrude(logo_depth + 1) scale(logo_w / logo_svg_w) translate(-logo_svg_c) import(logo_file);
}

// Pin hanging from the lid front wall into a lug screw hole, flatted on the X sides; a short boss
// down to 0.2 above the lug top shortens its lever
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

module presser(y) {
    box(x_in - press_reach, x_in, y - press_w / 2, y + press_w / 2, 0.1, split_z + eps);
}

// Side clamp: a finger hanging from the ceiling with a ledge under the XIAO PCB edge and a lip over it,
// running along one unused-pad stretch (d0..d1 from the USB end). It flexes outwards as the XIAO is
// pressed in (lid upside down), then the PCB sits between ledge and lip.
module xiao_clamp(s, d0, d1) {
    xe = xiao_w / 2 + pcb_clr;          // finger inner face
    // where the XIAO lies on the DB9 body the body carries it: the clamp starts behind the body
    over_body = xe - clamp_ledge < body_w / 2 + pcb_clr;
    y0 = over_body ? max(yx1 - d1, y_body1 + pcb_clr) : yx1 - d1;
    y1 = yx1 - d0;
    z_ledge = zx0 - clamp_gap;          // ledge top
    z_tip = z_ledge - clamp_ledge - 0.4;
    z_lip = zx1 + clamp_gap;
    if (y1 - y0 >= min_wall) mirror([s < 0 ? 1 : 0, 0, 0]) {
        box(xe, xe + clamp_t, y0, y1, z_tip, z_ceil + eps);
        // ledge, 45 degree lead-in facing the incoming board
        xz_prism([[xe + eps, z_tip], [xe, z_tip], [xe - clamp_ledge, z_tip + clamp_ledge],
                  [xe - clamp_ledge, z_ledge], [xe + eps, z_ledge]], y0, y1);
        // lip, 45 degree top so it prints upside down
        xz_prism([[xe + eps, z_lip], [xe - clamp_lip, z_lip], [xe - clamp_lip, z_lip + 0.3],
                  [xe, z_lip + 0.3 + clamp_lip], [xe + eps, z_lip + 0.3 + clamp_lip]], y0, y1);
    }
}
function clamp_strain(z_root) = 1.5 * clamp_t * (clamp_ledge - pcb_clr) / pow(z_root - (zx0 - clamp_gap - clamp_ledge - 0.4), 2);

// strut bottom: on the body top + stop_z_clr where the XIAO front end lies over the body, else below the PCB
function stop_over_body(x) = (yx0 - pcb_clr - stop_t < y_body1 + pcb_clr) && abs(x) - stop_w / 2 < body_w / 2;
function stop_bottom(x) = let(y1 = yx0 - pcb_clr, tip0 = max(y1 - stop_t, y_body1 + pcb_clr))
    (y1 - tip0 >= stop_tip_min) ? zx0 - 0.6 : body_h + stop_z_clr;
module front_stop(x) {
    y1 = yx0 - pcb_clr;                         // bearing face on the XIAO PCB edge
    tip0 = max(y1 - stop_t, y_body1 + pcb_clr); // behind the body the tip can reach below the PCB
    box(x - stop_w / 2, x + stop_w / 2, y_fw1 - eps, y1, body_h + stop_z_clr, z_ceil + eps);
    if (y1 - tip0 >= stop_tip_min) box(x - stop_w / 2, x + stop_w / 2, tip0, y1, zx0 - 0.6, z_ceil + eps);
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
    }
    // lugs either side of the body with their vertical screw holes
    gcol("dimgray") mirror_x_both() difference() {
        box(body_w / 2, board_w / 2, y_fb, lug_end, 0, lug_h);
        translate([lug_hx, lug_hole_y + ghost_lug_dy, lug_h - lug_hole_depth - eps]) cylinder(d = lug_hole_d, h = lug_hole_depth + 1);
    }
    gcol("dimgray") {
        box(-body_w / 2, body_w / 2, y_fb, y_body1, 0, body_h);
        if (step_on) box(-step_wx / 2, step_wx / 2, y_fb, y_fb + step_d, 0, step_h);
    }
}

module ghost_rs232() {
    gcol("royalblue") box(-board_w / 2, board_w / 2, y_pcb0, y_pcb1, -pcb_t, 0);
    // through-hole pin zone under the board (kept 1.2 from the edges)
    gcol("red", 0.4) box(-board_w / 2 + 1.25, board_w / 2 - 1.25, max(y_pcb0, y_fb) + 1.25, y_pcb1 - 1.25, -pcb_t - 2.5, -pcb_t);
    gcol("black") box(hdr_x0, hdr_x0 + 2.54, hdr_y0, hdr_y1, 0, hdr_h);
}

module ghost_xiao() {
    translate(ghost_offset) {
        gcol("#202020") box(-xiao_w / 2, xiao_w / 2, yx0, yx1, zx0, zx1);
        gcol("whitesmoke") box(-shield_w / 2, shield_w / 2, yx1 - shield_d1, yx1 - shield_d0, zx1, zx1 + shield_h);
        gcol("lightgray") box(-usb_w / 2, usb_w / 2, y_mouth - usb_l, y_mouth, zx1, zx1 + usb_h);
        gcol("ivory") box(ant_x[0], ant_x[1], yx1 - ant_d[1], yx1 - ant_d[0], zx1, zx1 + 1.1);   // ceramic antenna
        gcol("gold") box(ufl_x[0], ufl_x[1], yx1 - ufl_d[1], yx1 - ufl_d[0], zx1, zx1 + ufl_h);  // U.FL + plug
        gcol("dimgray") mirror_x_both() box(btn_x[0], btn_x[1], yx1 - btn_d[1], yx1 - btn_d[0], zx1, zx1 + 0.6);
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

// Keep-out volumes for the four wires and their joints (the user's requirement): no case feature may enter them
module wire_zones() {
    yl0 = pad_y(max(used_pads_left)) - joint_half;  yl1 = pad_y(min(used_pads_left)) + joint_half;
    yr0 = pad_y(max(used_pads_right)) - joint_half; yr1 = pad_y(min(used_pads_right)) + joint_half;
    gcol("magenta", 0.25) {
        box(-11.5, -6.0, yl0, yl1, 0.3, zx0 + eps);                    // under the -X D4/D5 pads
        box(6.0, 11.5, yr0, yr1, 0.3, zx0 + eps);                      // under the +X 5V/GND pads
        box(-15.6, -11.0, yl0, hdr_y1, hdr_h, zx1 + 2);                // header top to the D4/D5 pads
        box(-11.5, 11.5, max(yr0, hdr_y0), min(yr1, hdr_y1), 3, 8);    // header across under the XIAO to 5V/GND
    }
}

module ghosts() {
    ghost_db9();
    ghost_rs232();
    ghost_xiao();
    ghost_wires();
    wire_zones();
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
} else if (part == "tray_inplace") {                               // assembly coordinates, for wallprobe.py
    tray();
} else if (part == "lid_inplace") {
    lid();
}
