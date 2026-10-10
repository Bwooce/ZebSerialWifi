#!/bin/sh
# Rebuild STLs, renders and the interference checks for the ZebSerialWifi case.
# Usage: ./build.sh [extra -D overrides passed to every openscad call]
set -eu
cd "$(dirname "$0")"
OSC=${OPENSCAD:-/opt/homebrew/bin/openscad}
SCAD=zebserial_case.scad
mkdir -p out renders
rm -f out/*.stl renders/*.png

run() { "$OSC" --backend=manifold "$@" "$SCAD"; }

echo "== STL export"
run -D 'part="tray"' -D show_ghosts=false -o out/tray.stl
run -D 'part="lid"' -D show_ghosts=false -o out/lid.stl
run -D 'part="logo"' -o out/logo.stl     # two-colour inlay, aligned with lid.stl (logo_mode="inlay")
# fit coupons: the front coupon_y1 of each part, single colour, in print orientation
run -D 'part="coupon_tray"' -o out/coupon_tray.stl
run -D 'part="coupon_lid"' -o out/coupon_lid.stl

echo "== renders"
IMG="--imgsize=1400,1050 --colorscheme=Tomorrow --render"
# assembly views
run $IMG --camera=0,18,6,60,0,25,140 -D 'part="assembly"' -o renders/assembly_front.png
run $IMG --camera=0,18,6,60,0,205,140 -D 'part="assembly"' -o renders/assembly_rear.png
run $IMG --camera=0,18,14,60,0,25,180 -D 'part="exploded"' -o renders/exploded.png
run $IMG --camera=0,18,14,60,0,205,180 -D 'part="exploded"' -D show_ghosts=false -o renders/exploded_case_only.png
# front: flange flush with the case front, D-shell out, retention lips over the flange margins
run $IMG --camera=0,0,6,82,0,12,95 -D 'part="assembly"' -o renders/front_flush.png
run $IMG --camera=0,0,6,90,0,0,105 --projection=o -D 'part="assembly"' -o renders/front_elevation.png
# sections: x=0 plane through the XIAO length, the DB9 body, both lips, the strut and the pads, seen from +X
run $IMG --camera=0,18,6,90,0,90,120 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=0.2 -o renders/section_xiao_length.png
# close-up of the same plane at the front: lips, flange slot, lid front wall
run $IMG --camera=0,2,5,90,0,90,65 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=0.2 -o renders/section_lips.png
# x plane through the +X screw-hole hardware axis (X 12.5) and its lid pocket, seen from +X
run $IMG --camera=0,4,6,90,0,90,45 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=13 -D section_t=1 -o renders/section_hw.png
# y plane just behind the flange (Y 0.5..2.5) through both hardware pockets, seen from the front
run $IMG --camera=0,1,8,90,0,0,85 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=0.5 -D section_t=2 -o renders/section_hw_front.png
# x plane through the backstop rib and the rear of the board (X -3), seen from +X
run $IMG --camera=0,24,0,90,0,90,50 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=-3 -o renders/section_backstop.png
# y planes across the XIAO: +X guide + shield pad (Y 15..17), -X guide + shield pad (Y 23.5..25.5), USB pad (Y 28..30)
run $IMG --camera=0,18,6,90,0,0,110 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=15 -D section_t=2 -o renders/section_xiao_guide_plusx.png
run $IMG --camera=0,18,6,90,0,0,110 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=23.5 -D section_t=2 -o renders/section_xiao_guide_minusx.png
run $IMG --camera=0,18,6,90,0,0,110 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=28 -D section_t=2 -o renders/section_usb_pad.png
# rear: the USB-C opening (radiused relief, stadium slot, tongue cradle)
run $IMG --camera=0,34,12,75,0,195,70 -D 'part="assembly"' -o renders/rear_usb.png
run $IMG --camera=0,36,12,90,0,180,60 --projection=o -D 'part="assembly"' -D show_ghosts=false -o renders/rear_usb_case_only.png
# y plane through the board-lock leg-tail pockets (Y 8.9), seen from the front
run $IMG --camera=0,18,2,90,0,0,110 --projection=o -D 'part="assembly"' -D leg_pocket_on=true -D 'section="y"' -D section_pos=7.9 -D section_t=2 -o renders/section_leg_pockets.png
# x plane through the +X leg-tail pocket (X 12.5), seen from +X
run $IMG --camera=0,14,2,90,0,90,90 --projection=o -D 'part="assembly"' -D leg_pocket_on=true -D 'section="x"' -D section_pos=13.5 -D section_t=2 -o renders/section_leg_pocket_length.png
# lid top with the logo inlay, DB9 at the bottom edge (how the logo reads), and the single-colour deboss fallback
run $IMG --camera=0,17,8,0,0,0,105 --projection=o -D 'part="assembly"' -o renders/lid_top_logo.png
run $IMG --camera=0,17,8,25,0,0,115 -D 'part="assembly"' -D 'logo_mode="deboss"' -D show_ghosts=false -o renders/lid_top_logo_deboss.png
# sections through each lid snap finger (front pair at Y 7, rear pair at Y 27.5 by default), seen from the front
run $IMG --camera=0,18,2,90,0,0,110 --projection=o -D 'part="assembly"' -D show_ghosts=false -D 'section="y"' -D section_pos=6 -D section_t=2 -o renders/section_snap_front.png
run $IMG --camera=0,18,2,90,0,0,110 --projection=o -D 'part="assembly"' -D show_ghosts=false -D 'section="y"' -D section_pos=26.4 -D section_t=2 -o renders/section_snap_rear.png
# exterior style, same viewpoints as the reference renders (plan from above, isometric from the front left)
run $IMG --camera=0,19,6,0,0,0,120 --projection=o -D 'part="assembly"' -o renders/style_plan.png
run $IMG --camera=0,19,6,55,0,330,135 -D 'part="assembly"' -o renders/style_iso.png
run $IMG --camera=0,19,6,90,0,90,120 --projection=o -D 'part="assembly"' -o renders/style_side.png
# print orientation
run $IMG --camera=0,0,0,55,0,25,130 --autocenter --viewall -D 'part="tray"' -o renders/print_tray.png
run $IMG --camera=0,0,0,55,0,205,130 --autocenter --viewall -D 'part="lid"' -o renders/print_lid.png

echo "== interference: volume of ghost/tray, ghost/lid and tray/lid overlaps"
echo "   (zero-volume faces are designed contacts, e.g. flange on the case front, board on the ledges)"
TMP=$(mktemp -d)
for c in check_tray check_lid check_traylid check_lid_path; do
  fn=""; [ $c = check_lid_path ] && fn='$fn=16'   # the swept check takes about 2 min at the default $fn
  if run ${fn:+-D "$fn"} "$@" -D "part=\"$c\"" -o "$TMP/$c.stl" > "$TMP/$c.log" 2>&1 && [ -s "$TMP/$c.stl" ]; then
    echo "$(python3 -I stlinfo.py "$TMP/$c.stl" | sed "s|$TMP/||")  ->  $(python3 -I stlinfo.py --hits "$TMP/$c.stl")"
  else
    echo "$c: empty"
  fi
done
echo "== logo: strokes and gaps under logo_min (part=check_logo), must be none"
run "$@" -D 'part="check_logo"' -o "$TMP/check_logo.stl" > "$TMP/check_logo.log" 2>&1 || true
if [ -s "$TMP/check_logo.stl" ]; then echo "check_logo: $(python3 -I stlinfo.py --hits "$TMP/check_logo.stl")"; else echo "check_logo: none"; fi
echo "== wall thickness (wallprobe.py): every sample under 1.2 mm, grouped"
echo "   expected: lid snap hooks, lead-in tips, logo skin 1.1 (see README)"
for p in tray lid; do
  run "$@" -D "part=\"${p}_inplace\"" -o "$TMP/${p}_inplace.stl" > /dev/null 2>&1
  python3 wallprobe.py --summary "$TMP/${p}_inplace.stl" | sed "s|$TMP/||"
done
rm -rf "$TMP"
python3 -I stlinfo.py out/tray.stl out/lid.stl out/logo.stl out/coupon_tray.stl out/coupon_lid.stl
