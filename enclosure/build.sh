#!/bin/sh
# Rebuild STLs, renders and the interference checks for the ZebSerialWifi case.
# Usage: ./build.sh [extra -D overrides passed to every openscad call]
set -eu
cd "$(dirname "$0")"
OSC=${OPENSCAD:-/opt/homebrew/bin/openscad}
SCAD=zebserial_case.scad
mkdir -p out renders
rm -f out/tray.stl out/lid.stl renders/*.png

run() { "$OSC" --backend=manifold "$@" "$SCAD"; }

echo "== STL export"
run -D 'part="tray"' -D show_ghosts=false -o out/tray.stl
run -D 'part="lid"' -D show_ghosts=false -o out/lid.stl

echo "== renders"
IMG="--imgsize=1400,1050 --colorscheme=Tomorrow --render"
# assembly views
run $IMG --camera=0,18,6,60,0,25,140 -D 'part="assembly"' -o renders/assembly_front.png
run $IMG --camera=0,18,6,60,0,205,140 -D 'part="assembly"' -o renders/assembly_rear.png
run $IMG --camera=0,18,14,60,0,25,180 -D 'part="exploded"' -o renders/exploded.png
run $IMG --camera=0,18,14,60,0,205,180 -D 'part="exploded"' -D show_ghosts=false -o renders/exploded_case_only.png
# sections: x=0 plane through the XIAO length and DB9 body, seen from +X
run $IMG --camera=0,18,6,90,0,90,120 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=1.5 -o renders/section_xiao_length.png
# y plane through the XIAO clips (right clip), seen from the front
run $IMG --camera=0,18,6,90,0,0,110 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=17.4 -D section_t=2 -o renders/section_xiao_clip_right.png
run $IMG --camera=0,18,6,90,0,0,110 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=27.6 -D section_t=2 -o renders/section_xiao_clip_left.png
# y plane through the DB9 hardware sockets, seen from the front
run $IMG --camera=0,18,6,90,0,0,110 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=2.5 -D section_t=2 -o renders/section_db9_sockets.png
# lid top with the logo deboss, DB9 at the bottom edge (how the logo reads)
run $IMG --camera=0,15,8,35,0,0,125 -D 'part="assembly"' -o renders/lid_top_logo.png
# sections through each lid snap finger (front and rear pair), seen from the front
run $IMG --camera=0,18,2,90,0,0,110 --projection=o -D 'part="assembly"' -D show_ghosts=false -D 'section="y"' -D section_pos=15.5 -D section_t=2 -o renders/section_snap_front.png
run $IMG --camera=0,18,2,90,0,0,110 --projection=o -D 'part="assembly"' -D show_ghosts=false -D 'section="y"' -D section_pos=29 -D section_t=2 -o renders/section_snap_rear.png
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
for c in check_tray check_lid check_traylid; do
  if run "$@" -D "part=\"$c\"" -o "$TMP/$c.stl" > "$TMP/$c.log" 2>&1 && [ -s "$TMP/$c.stl" ]; then
    python3 -I stlinfo.py "$TMP/$c.stl" | sed "s|$TMP/||"
  else
    echo "$c: empty"
  fi
done
echo "== wall thickness (wallprobe.py): every sample under 1.2 mm, grouped"
echo "   expected: board clip fingers 1.0 and hooks, XIAO clip hooks/lips, lid snap hooks, logo skin 1.1"
for p in tray lid; do
  run "$@" -D "part=\"${p}_inplace\"" -o "$TMP/${p}_inplace.stl" > /dev/null 2>&1
  python3 wallprobe.py --summary "$TMP/${p}_inplace.stl" | sed "s|$TMP/||"
done
rm -rf "$TMP"
python3 -I stlinfo.py out/tray.stl out/lid.stl
