#!/usr/bin/env python3
"""Generate the ZebSerial logo SVGs.

The mark is a DB9 D-shell (the trapezoid) with three tapered zebra stripes knocked out of it and two WiFi arcs
radiating from its top edge. Everything is on a 64 x 64 grid, with the main edges on multiples of 4 so they land
on pixel boundaries at 16 px. The mark itself spans 56 units, so at the 22 mm deboss width one unit is 0.39 mm
and the 0.8 mm minimum stroke/gap for a 0.4 mm nozzle is 2.0 units.

The case debosses zebserial-mark-deboss.svg (logo_file in enclosure/zebserial_case.scad, at logo_w), and
enclosure/build.sh checks its strokes and gaps with part="check_logo". zebserial-mark-lid.svg is a bolder, simpler
variant (two wide stripes, two constant-width arcs with round caps, every corner rounded) for a two-colour inlay; the
case does not use it at present.

Usage: python3 make_logo.py [font.ttf]
After changing the mark, update LOGO_D in firmware/main/web.c and, if the outline moved, logo_svg_w / logo_svg_c in
enclosure/zebserial_case.scad (the imported bounding box, which OpenSCAD reports for import() of the deboss SVG).
The wordmark uses Liberation Sans Bold (SIL OFL), converted to paths, so the SVGs need no font.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = "/Applications/OpenSCAD.app/Contents/Resources/fonts/Liberation-2.00.1/ttf/LiberationSans-Bold.ttf"

# D-shell, wider at the top like the male connector seen from the front
TRAP_TOP, TRAP_BOT, TRAP_HW_TOP, TRAP_HW_BOT = 32, 60, 28, 20
TRAP = [(32 - TRAP_HW_TOP, TRAP_TOP), (32 + TRAP_HW_TOP, TRAP_TOP), (32 + TRAP_HW_BOT, TRAP_BOT),
        (32 - TRAP_HW_BOT, TRAP_BOT)]
TRAP_R = 5.0
# Slanted stripes knocked out of the shell: (x offset of the centre at mid height, width at top, width at bottom).
# Each leans by SLANT (x shift from bottom to top) and is inset STRIPE_INSET from the top and bottom edges.
STRIPES = [(-13.0, 8.0, 4.0), (0.0, 8.0, 4.0), (13.0, 8.0, 4.0)]
SLANT = 5.0
STRIPE_INSET = 4.0
# WiFi arcs radiating from the top of the shell: (inner radius, outer radius) at the middle, thickness at the ends
C = (32, 36)
BANDS = [(8.0, 16.0), (20.0, 28.0)]
END_T = 5.5
HALF_ANGLE = math.radians(52)

# Lid variant (units as above): every stroke and gap at least LID_MIN units, every corner rounded by >= LID_R
LID_MIN = 2.4                                   # 1.2 mm at 0.5 mm per unit (logo_w 28); more at 30 mm
LID_R = 1.5                                     # stripe corner fillet (0.6 mm opening radius needs >= 1.2 units)
LID_STRIPES = [(-8.0, 6.0), (8.0, 6.0)]         # (x offset of the centre at mid height, width)
LID_SLANT = 5.0
LID_INSET = 5.0                                 # stripes inset from the shell's top and bottom edges
LID_C = (32, 36)                                # arc centre
LID_ARCS = [13.0, 23.0]                         # arc centre-line radii
LID_ARC_T = 6.0                                 # arc thickness (round caps of radius LID_ARC_T / 2)
LID_HALF_ANGLE = math.radians(38)               # caps stay LID_ARC_T / 2 clear of the shell's top edge


def f(v):
    s = f"{v:.1f}".rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


def pt(p):
    return f"{f(p[0])} {f(p[1])}"


def rounded_poly(pts, r):
    """Closed path for a convex polygon (clockwise on screen) with every corner filleted at radius r."""
    n = len(pts)
    out = []
    for i in range(n):
        p0, p1, p2 = pts[i - 1], pts[i], pts[(i + 1) % n]
        a = (p0[0] - p1[0], p0[1] - p1[1])
        b = (p2[0] - p1[0], p2[1] - p1[1])
        la, lb = math.hypot(*a), math.hypot(*b)
        a, b = (a[0] / la, a[1] / la), (b[0] / lb, b[1] / lb)
        theta = math.acos(a[0] * b[0] + a[1] * b[1])
        d = r / math.tan(theta / 2)
        t0 = (p1[0] + a[0] * d, p1[1] + a[1] * d)
        t1 = (p1[0] + b[0] * d, p1[1] + b[1] * d)
        out.append(("L" if out else "M") + pt(t0))
        out.append(f"A{f(r)} {f(r)} 0 0 1 {pt(t1)}")
    return "".join(out) + "Z"


def band(ri, ro):
    """Tapered arc: outer edge is a circle about C; inner edge is the circle through the two inner end points
    and the inner mid point, so the band is ro-ri thick at the top and END_T at its radial-cut ends."""
    s, c = math.sin(HALF_ANGLE), math.cos(HALF_ANGLE)
    ol = (C[0] - ro * s, C[1] - ro * c)
    orr = (C[0] + ro * s, C[1] - ro * c)
    rie = ro - END_T
    il = (C[0] - rie * s, C[1] - rie * c)
    ir = (C[0] + rie * s, C[1] - rie * c)
    # circle centred on the vertical axis through (half-chord hx, y_e) and (0, y_m)
    hx, ye, ym = rie * s, C[1] - rie * c, C[1] - ri
    yc = (hx * hx + ye * ye - ym * ym) / (2 * (ye - ym))
    rin = yc - ym
    return (f"M{pt(ol)}A{f(ro)} {f(ro)} 0 0 1 {pt(orr)}L{pt(ir)}"
            f"A{f(rin)} {f(rin)} 0 0 0 {pt(il)}Z")


def stripe(xc, wt, wb):
    y0, y1 = TRAP_TOP + STRIPE_INSET, TRAP_BOT - STRIPE_INSET
    xt, xb = 32 + xc + SLANT / 2, 32 + xc - SLANT / 2
    # anticlockwise, so the stripes are holes under the nonzero fill rule as well as evenodd
    return f"M{pt((xt - wt / 2, y0))}L{pt((xb - wb / 2, y1))}L{pt((xb + wb / 2, y1))}L{pt((xt + wt / 2, y0))}Z"


def rounded_poly_any(pts, r):
    """Closed path for a convex polygon in either direction, every corner filleted at radius r."""
    n = len(pts)
    out = []
    for i in range(n):
        p0, p1, p2 = pts[i - 1], pts[i], pts[(i + 1) % n]
        a = (p0[0] - p1[0], p0[1] - p1[1])
        b = (p2[0] - p1[0], p2[1] - p1[1])
        la, lb = math.hypot(*a), math.hypot(*b)
        a, b = (a[0] / la, a[1] / la), (b[0] / lb, b[1] / lb)
        theta = math.acos(a[0] * b[0] + a[1] * b[1])
        d = r / math.tan(theta / 2)
        t0 = (p1[0] + a[0] * d, p1[1] + a[1] * d)
        t1 = (p1[0] + b[0] * d, p1[1] + b[1] * d)
        turn = (p1[0] - p0[0]) * (p2[1] - p1[1]) - (p1[1] - p0[1]) * (p2[0] - p1[0])
        out.append(("L" if out else "M") + pt(t0))
        out.append(f"A{f(r)} {f(r)} 0 0 {1 if turn > 0 else 0} {pt(t1)}")
    return "".join(out) + "Z"


def lid_stripe(xc, w):
    y0, y1 = TRAP_TOP + LID_INSET, TRAP_BOT - LID_INSET
    xt, xb = 32 + xc + LID_SLANT / 2, 32 + xc - LID_SLANT / 2
    # anticlockwise on screen: a hole under the nonzero rule too
    return rounded_poly_any([(xt - w / 2, y0), (xb - w / 2, y1), (xb + w / 2, y1), (xt + w / 2, y0)], LID_R)


def lid_arc(rc):
    """Constant-width arc about LID_C with round caps, clockwise on screen."""
    h = LID_ARC_T / 2
    s, c = math.sin(LID_HALF_ANGLE), math.cos(LID_HALF_ANGLE)
    ro, ri = rc + h, rc - h
    P = lambda r, sx: (LID_C[0] + sx * r * s, LID_C[1] - r * c)
    return (f"M{pt(P(ro, -1))}A{f(ro)} {f(ro)} 0 0 1 {pt(P(ro, 1))}A{f(h)} {f(h)} 0 0 1 {pt(P(ri, 1))}"
            f"A{f(ri)} {f(ri)} 0 0 0 {pt(P(ri, -1))}A{f(h)} {f(h)} 0 0 1 {pt(P(ro, -1))}Z")


def lid_mark_path():
    # the arcs' caps must clear the shell top by at least LID_MIN
    for rc in LID_ARCS:
        assert LID_C[1] - rc * math.cos(LID_HALF_ANGLE) + LID_ARC_T / 2 <= TRAP_TOP - LID_MIN
    assert LID_ARCS[1] - LID_ARCS[0] - LID_ARC_T >= LID_MIN and LID_ARC_T >= LID_MIN
    return (rounded_poly(TRAP, TRAP_R) + "".join(lid_stripe(*s) for s in LID_STRIPES)
            + "".join(lid_arc(r) for r in LID_ARCS))


def mark_path():
    return rounded_poly(TRAP, TRAP_R) + "".join(stripe(*s) for s in STRIPES) + "".join(band(*b) for b in BANDS)


def wordmark(font_path, x0, baseline, cap_h):
    from fontTools.pens.svgPathPen import SVGPathPen
    from fontTools.pens.transformPen import TransformPen
    from fontTools.ttLib import TTFont

    font = TTFont(font_path)
    gs = font.getGlyphSet()
    cmap = font.getBestCmap()
    scale = cap_h / font["OS/2"].sCapHeight
    pen = SVGPathPen(gs, ntos=f)
    x = x0
    for ch in "ZebSerial":
        g = cmap[ord(ch)]
        gs[g].draw(TransformPen(pen, (scale, 0, 0, -scale, x, baseline)))
        x += gs[g].width * scale
    return pen.getCommands(), x


def main():
    font = sys.argv[1] if len(sys.argv) > 1 else FONT
    d = mark_path()
    mark = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="currentColor">'
            f'<path fill-rule="evenodd" d="{d}"/></svg>\n')
    with open(os.path.join(HERE, "zebserial-mark.svg"), "w") as fh:
        fh.write(mark)
    # Deboss: same path, explicit black fill, 1 unit = 1 mm; the case model centres and scales it to logo_w.
    # The viewBox starts at 0 0 because OpenSCAD 2026.06 offsets a non-zero viewBox origin wrongly in Y.
    deboss = (f'<svg xmlns="http://www.w3.org/2000/svg" width="64mm" height="64mm" viewBox="0 0 64 64">'
              f'<path fill="#000" fill-rule="evenodd" d="{d}"/></svg>\n')
    with open(os.path.join(HERE, "zebserial-mark-deboss.svg"), "w") as fh:
        fh.write(deboss)
    # Lid inlay / deboss variant (same coordinate convention as the deboss file)
    lid = (f'<svg xmlns="http://www.w3.org/2000/svg" width="64mm" height="64mm" viewBox="0 0 64 64">'
           f'<path fill="#000" fill-rule="evenodd" d="{lid_mark_path()}"/></svg>\n')
    with open(os.path.join(HERE, "zebserial-mark-lid.svg"), "w") as fh:
        fh.write(lid)
    words, x1 = wordmark(font, 76, 46, 28)
    w = math.ceil(x1 + 2)
    full = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} 64" fill="currentColor">'
            f'<path fill-rule="evenodd" d="{d}"/><path d="{words}"/></svg>\n')
    with open(os.path.join(HERE, "zebserial-full.svg"), "w") as fh:
        fh.write(full)
    print(f"mark {len(mark)} B, deboss {len(deboss)} B, full {len(full)} B")


if __name__ == "__main__":
    main()
