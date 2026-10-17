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
run -D 'part="logo"' -o out/logo.stl     # two-colour inlay, aligned with tray.stl (logo_mode="inlay")
# fit coupons, single colour, in print orientation: the tray with the middle of the floor cut out, and the lid below
# coupon_z (front wall, top lip, corner pressers, snaps)
run -D 'part="coupon_tray"' -o out/coupon_tray.stl
run -D 'part="coupon_lid"' -o out/coupon_lid.stl

echo "== renders"
IMG="--imgsize=1400,1050 --colorscheme=Tomorrow --render"
# assembly views
run $IMG --camera=0,16,6,60,0,25,140 -D 'part="assembly"' -o renders/assembly_front.png
run $IMG --camera=0,16,6,60,0,205,140 -D 'part="assembly"' -o renders/assembly_rear.png
run $IMG --camera=0,16,14,60,0,25,180 -D 'part="exploded"' -o renders/exploded.png
run $IMG --camera=0,16,14,60,0,205,180 -D 'part="exploded"' -D show_ghosts=false -o renders/exploded_case_only.png
# as it hangs on the printer: tray floor up with the logo, DB9 (printer) at the left, parallel-port side (+X) at the
# top, USB-C at the bottom; plan view and a view from the printer side
run $IMG --camera=16,0,0,0,0,0,105 --projection=o -D 'part="plugged"' -o renders/plugged_floor_logo.png
run $IMG --camera=16,0,0,50,0,300,120 -D 'part="plugged"' -o renders/plugged_iso.png
run $IMG --camera=16,0,0,0,0,0,105 --projection=o -D 'part="plugged"' -D 'logo_mode="deboss"' -D show_ghosts=false -o renders/plugged_floor_deboss.png
# front: flange flush with the case front, D-shell out, retention lips over the flange margins
run $IMG --camera=0,0,6,82,0,12,95 -D 'part="assembly"' -o renders/front_flush.png
run $IMG --camera=0,0,6,90,0,0,105 --projection=o -D 'part="assembly"' -o renders/front_elevation.png
# x plane at X 0 (DB9 body, both lips, lid front wall, XIAO across its width, D5 channel, Dupont housing), from +X
run $IMG --camera=0,16,8,90,0,90,120 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=0.2 -o renders/section_x0.png
# close-up of the same plane at the front: lips, flange slot, lid front wall
run $IMG --camera=0,2,5,90,0,90,65 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=0.2 -o renders/section_lips.png
# x plane at X 12 (through the flange plate beside the DB9 body), seen from +X: plate, lid front wall bearing on it
run $IMG --camera=0,6,6,90,0,90,50 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=12.5 -D section_t=1 -o renders/section_plate.png
# the lid alone from the front: the opening for the full flange plate
run $IMG --camera=0,0,12,80,0,0,90 -D 'part="lid_inplace"' -o renders/lid_front.png
# plan section at the USB-C axis (z 13.25), from above: XIAO, USB slot and relief in the -X wall, +X stop rib,
# shield pad and stop, D3/D5 channel, Dupont housing, corner pressers
run $IMG --camera=0,16,0,0,0,0,105 --projection=o -D 'part="assembly"' -D 'section="z"' -D section_pos=13.25 -D section_t=30 -o renders/section_plan_usb.png
run $IMG --camera=0,16,0,0,0,0,105 --projection=o -D 'part="assembly"' -D show_ghosts=false -D 'section="z"' -D section_pos=13.25 -D section_t=30 -o renders/section_plan_usb_case_only.png
# y plane through the USB-C axis (Y 16.2), from the front: tongue cradle, slot, relief, USB pad, shield pad, stop rib
run $IMG --camera=0,16,8,90,0,0,110 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=16.2 -D section_t=2 -o renders/section_usb.png
# y plane through the corner pins (Y 27), from the front: pins, bosses, pressers, Dupont housing
run $IMG --camera=0,27,4,90,0,0,110 --projection=o -D 'part="assembly"' -D 'section="y"' -D section_pos=27 -D section_t=2 -o renders/section_pins.png
# x plane through the +X corner pin (X 13.25), seen from +X, close-up
run $IMG --camera=0,26,0,90,0,90,40 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=13.25 -D section_t=2 -o renders/section_pin_x.png
# x plane at X 1.5 (shield stop block over the rear edge strip, D5 pad relief in the front wall), from +X
run $IMG --camera=0,16,10,90,0,90,90 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=1.5 -D section_t=2 -o renders/section_shield_stop.png
# the -X side: the USB-C opening (radiused relief, stadium slot, tongue cradle)
run $IMG --camera=-20,16,12,75,0,285,70 -D 'part="assembly"' -o renders/side_usb.png
run $IMG --camera=-22,16,10,90,0,270,60 --projection=o -D 'part="assembly"' -D show_ghosts=false -o renders/side_usb_case_only.png
# y plane through the estimated leg tails (Y 7.6..9.6), from the front: the keep-out (orange) is clear of the tray
run $IMG --camera=0,16,0,90,0,0,110 --projection=o -D 'part="assembly"' -D show_pin_zone=false -D 'section="y"' -D section_pos=7.6 -D section_t=2 -o renders/section_leg_keepout.png
# sections through each lid snap finger (front pair at Y 6.3, rear pair at Y 27.4), seen from the front
run $IMG --camera=0,16,2,90,0,0,110 --projection=o -D 'part="assembly"' -D show_ghosts=false -D 'section="y"' -D section_pos=5.3 -D section_t=2 -o renders/section_snap_front.png
run $IMG --camera=0,16,2,90,0,0,110 --projection=o -D 'part="assembly"' -D show_ghosts=false -D 'section="y"' -D section_pos=26.4 -D section_t=2 -o renders/section_snap_rear.png
# revision 4: the pin 9 wire (x slab X -9..-7, seen from +X): the run under the board, the notch in the backstop
# ledge behind the PCB rear edge, and the riser up into the 5V/GND wire zone
run $IMG --camera=0,16,8,90,0,90,120 --projection=o -D 'part="assembly"' -D 'section="x"' -D section_pos=-7 -D section_t=2 -o renders/section_p9.png
# revision 4: the jack screw hardware (gold) just behind the flange plate (y slab Y 5.15..7.15, from the front):
# the lid front wall cut back over it, bearing on the plate above it and across the DB9 body top
run $IMG --camera=0,16,6,90,0,0,110 --projection=o -D 'part="assembly"' -D show_pin_zone=false -D 'section="y"' -D section_pos=5.15 -D section_t=2 -o renders/section_jack.png
# revision 4: plan section z 11.3..23.3 from above: D3/D5 channel, wire-bend zone, 5V/GND zone and the pin 9 riser
run $IMG --camera=0,16,0,0,0,0,105 --projection=o -D 'part="assembly"' -D 'section="z"' -D section_pos=23.3 -D section_t=12 -o renders/section_plan_wires.png
# exterior style, same viewpoints as the reference renders (plan from above, isometric from the front left)
run $IMG --camera=0,16,6,0,0,0,120 --projection=o -D 'part="assembly"' -o renders/style_plan.png
run $IMG --camera=0,16,6,55,0,330,135 -D 'part="assembly"' -o renders/style_iso.png
run $IMG --camera=0,16,6,90,0,90,120 --projection=o -D 'part="assembly"' -o renders/style_side.png
# print orientation
run $IMG --camera=0,0,0,55,0,25,130 --autocenter --viewall -D 'part="tray"' -o renders/print_tray.png
run $IMG --camera=0,0,0,55,0,205,130 --autocenter --viewall -D 'part="lid"' -o renders/print_lid.png

echo "== interference: volume of ghost/tray, ghost/lid and tray/lid overlaps"
echo "   (zero-volume faces are designed contacts, e.g. flange on the case front, board on the ledges)"
# Every gate below fails the build: an OpenSCAD error (ERROR / failed assert, or a non-zero exit that is not just an
# empty result), a real overlap, a logo stroke or gap under logo_min, or a wall sliver not on the whitelist.
TMP=$(mktemp -d)
fails=0
fail() { echo "FAIL: $*"; fails=$((fails + 1)); }
# osc_ok LOG STATUS: true unless the run errored. "Current top level object is empty." alone (exit 1) is an empty
# result, not an error.
osc_ok() {
  if grep -qE '^ERROR:|Assertion|Execution aborted' "$1"; then return 1; fi
  [ "$2" -eq 0 ] || grep -q 'Current top level object is empty' "$1"
}
for c in check_tray check_lid check_traylid check_lid_path; do
  fn=""; [ $c = check_lid_path ] && fn='$fn=16'   # the swept check takes about 2 min at the default $fn
  rm -f "$TMP/$c.stl"
  st=0; run ${fn:+-D "$fn"} "$@" -D "part=\"$c\"" -o "$TMP/$c.stl" > "$TMP/$c.log" 2>&1 || st=$?
  if ! osc_ok "$TMP/$c.log" $st; then
    fail "$c: openscad error (exit $st)"; grep -E '^ERROR:|Assertion' "$TMP/$c.log" | head -3
  elif [ -s "$TMP/$c.stl" ]; then
    hits=$(python3 -I stlinfo.py --hits "$TMP/$c.stl")
    echo "$(python3 -I stlinfo.py "$TMP/$c.stl" | sed "s|$TMP/||")  ->  $hits"
    case $hits in none*) ;; *) fail "$c: overlap" ;; esac
  else
    echo "$c: empty  ->  none"
  fi
done
echo "== logo: strokes and gaps under logo_min (part=check_logo), must be none"
rm -f "$TMP/check_logo.stl"
st=0; run "$@" -D 'part="check_logo"' -o "$TMP/check_logo.stl" > "$TMP/check_logo.log" 2>&1 || st=$?
if ! osc_ok "$TMP/check_logo.log" $st; then
  fail "check_logo: openscad error (exit $st)"; grep -E '^ERROR:|Assertion' "$TMP/check_logo.log" | head -3
elif [ -s "$TMP/check_logo.stl" ]; then
  hits=$(python3 -I stlinfo.py --hits "$TMP/check_logo.stl"); echo "check_logo: $hits"
  case $hits in none*) ;; *) fail "check_logo: strokes or gaps under logo_min" ;; esac
else
  echo "check_logo: none"
fi
echo "== wall thickness (wallprobe.py): every sample under 1.2 mm, grouped; anything off the whitelist fails"
# Whitelist (assembly coordinates, x0 x1 y0 y1 z0 z1 minimum), each one judged acceptable in the README:
#  tray: the tongue's top edges beside the USB cradle in the -X wall (about 0.93): they taper to tongue_tip, only
#        round the opening and carry nothing (the shell rests on the cradle bottom)
#  tray: the floor / front skin gusset (1.19): 1.2 by construction square to the 45 degree bottom front chamfer
#  tray: the floor skin under the logo pocket (0.92), over the logo footprint only: the inlay fills the pocket in the
#        two-colour print, and the 0.92 prints as six solid 0.16 mm layers
#  lid:  the snap hooks (about 0.95 and 1.1): the hook profile itself, a 0.6 mm 45 degree bump on the finger
ALLOW_tray="--allow -20.5 -15.5 10 22.5 10.5 12.5 0.9 --allow -17 17 0 14 -6.5 -1 1.18 --allow -13 13 3.3 29.4 -6.5 -4.5 0.9"
ALLOW_lid="--allow 16 20.5 3 32 -1.5 1 0.9 --allow -20.5 -16 3 32 -1.5 1 0.9"
for p in tray lid; do
  rm -f "$TMP/${p}_inplace.stl"
  st=0; run "$@" -D "part=\"${p}_inplace\"" -o "$TMP/${p}_inplace.stl" > "$TMP/${p}_inplace.log" 2>&1 || st=$?
  if ! osc_ok "$TMP/${p}_inplace.log" $st || [ ! -s "$TMP/${p}_inplace.stl" ]; then
    fail "${p}_inplace: openscad error or empty (exit $st)"; grep -E '^ERROR:|Assertion' "$TMP/${p}_inplace.log" | head -3
    continue
  fi
  eval "allow=\$ALLOW_$p"
  # shellcheck disable=SC2086
  st=0; python3 wallprobe.py --summary $allow "$TMP/${p}_inplace.stl" > "$TMP/${p}_wall.txt" 2>&1 || st=$?
  sed "s|$TMP/||" "$TMP/${p}_wall.txt"
  [ $st -eq 0 ] || fail "${p}: wall sliver(s) not on the whitelist (or wallprobe error, exit $st)"
done
rm -rf "$TMP"
python3 -I stlinfo.py out/tray.stl out/lid.stl out/logo.stl out/coupon_tray.stl out/coupon_lid.stl
if [ "$fails" -ne 0 ]; then echo "build: $fails gate(s) FAILED"; exit 1; fi
echo "build: all gates passed"
