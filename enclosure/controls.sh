#!/bin/sh
# Positive and negative controls for the interference checks: each line moves a ghost and says whether the named
# check must find real overlap ("hit": a piece of at least 0.01 mm3, thicker than 0.005 mm in every axis) or none.
# Prints PASS / FAIL per line and exits non-zero on any FAIL. An OpenSCAD error (an ERROR or WARNING line, a
# "ZEBCASE WARNING" echo, a failed assert, or a non-zero exit that is not just "Current top level object is empty.")
# is reported as got "error" and fails.
# Usage: ./controls.sh
set -u
cd "$(dirname "$0")"
OSC=${OPENSCAD:-/opt/homebrew/bin/openscad}
TMP=$(mktemp -d)
fails=0

ctl() {   # name check expect -D ...
  name=$1; chk=$2; want=$3; shift 3
  rm -f "$TMP/o.stl"
  st=0
  "$OSC" --backend=manifold "$@" -D "part=\"$chk\"" -o "$TMP/o.stl" zebserial_case.scad > "$TMP/o.log" 2>&1 || st=$?
  if grep -qE '^ERROR:|^WARNING:|Assertion|Execution aborted|ZEBCASE WARNING' "$TMP/o.log" ||
     { [ "$st" -ne 0 ] && ! grep -q 'Current top level object is empty' "$TMP/o.log"; }; then
    got="error (exit $st: $(grep -m1 -E '^ERROR:|^WARNING:|Assertion|ZEBCASE WARNING' "$TMP/o.log" | cut -c1-80))"
  elif [ -s "$TMP/o.stl" ]; then
    got=$(python3 -I stlinfo.py --hits "$TMP/o.stl")
  else
    got=none
  fi
  case $got in "$want"*) r=PASS ;; *) r=FAIL; fails=$((fails + 1)) ;; esac
  printf '%s  %-34s %-14s want %-4s got %s\n' "$r" "$name" "$chk" "$want" "$got"
}

# nominal
ctl nominal                        check_tray    none
ctl nominal                        check_lid     none
ctl nominal                        check_traylid none
ctl nominal                        check_lid_path none -D '$fn=16'
ctl "lid undercut under the XIAO"  check_lid      none -D ctl_undercut=true
ctl "lid undercut under the XIAO"  check_lid_path hit  -D '$fn=16' -D ctl_undercut=true
# RS232 board + DB9 fore-aft (lips 0.1 in front, lid front wall at 0 behind, tray backstop ledge 0.25 behind the PCB)
ctl "board fwd 0.05"               check_tray    none -D ghost_board_dy=-0.05
ctl "board fwd 0.05"               check_lid     none -D ghost_board_dy=-0.05
ctl "board fwd 0.15 (bottom lip)"  check_tray    hit  -D ghost_board_dy=-0.15
ctl "board fwd 0.15 (top lip)"     check_lid     hit  -D ghost_board_dy=-0.15
ctl "board fwd 1.2 (bottom lip)"   check_tray    hit  -D ghost_board_dy=-1.2
ctl "board back 0.2"               check_tray    none -D ghost_board_dy=0.2
ctl "board back 0.2 (lid wall)"    check_lid     hit  -D ghost_board_dy=0.2
ctl "board back 0.3 (backstop)"    check_tray    hit  -D ghost_board_dy=0.3
# mounting holes off their nominal Y (pins 0.45 clear in Y all round the hole)
ctl "holes +0.4 Y"                 check_tray    none -D ghost_mh_dy=0.4
ctl "holes -0.4 Y"                 check_tray    none -D ghost_mh_dy=-0.4
ctl "holes +0.5 Y (corner pins)"   check_tray    hit  -D ghost_mh_dy=0.5
ctl "holes -0.5 Y (corner pins)"   check_tray    hit  -D ghost_mh_dy=-0.5
# board sideways: the side walls and the corner pins both locate it in X (0.2 per side), so the whole-board move
# hits both; the holes-only move tests the pins alone
ctl "board +X 0.15"                check_tray    none -D ghost_board_dx=0.15
ctl "board -X 0.15"                check_tray    none -D ghost_board_dx=-0.15
ctl "board +X 0.3 (walls, pins)"   check_tray    hit  -D ghost_board_dx=0.3
ctl "board -X 0.3 (walls, pins)"   check_tray    hit  -D ghost_board_dx=-0.3
ctl "holes +X 0.15"                check_tray    none -D ghost_mh_dx=0.15
ctl "holes -X 0.15"                check_tray    none -D ghost_mh_dx=-0.15
ctl "holes +X 0.3 (corner pins)"   check_tray    hit  -D ghost_mh_dx=0.3
ctl "holes -X 0.3 (corner pins)"   check_tray    hit  -D ghost_mh_dx=-0.3
# board-lock leg-tail keep-out (estimated position +-1.5): the ledges start 0.5 behind it, the side wall 0.5 outside
ctl "leg keep-out +0.4 Y"          check_tray    none -D ghost_lug_dy=0.4
ctl "leg keep-out +0.6 Y (ledge)"  check_tray    hit  -D ghost_lug_dy=0.6
ctl "leg keep-out +0.4 X"          check_tray    none -D ghost_lug_dx=0.4
ctl "leg keep-out +0.6 X (wall)"   check_tray    hit  -D ghost_lug_dx=0.6
# RS232 PCB alone up (corner pressers press_gap 0.25 above its top)
ctl "pcb up 0.2"                   check_lid     none -D ghost_board_dz=0.2
ctl "pcb up 0.3 (corner pressers)" check_lid     hit  -D ghost_board_dz=0.3
# XIAO: pads 0.25 above, tongue 0.05 below, +X wall and -X stop rib 0.2, front wall 0.2 in front, shield stop 0.3
# behind the shield; the USB slot sides are 0.2 off the shell either way in Y
ctl "xiao up 0.05"                 check_lid     none -D 'ghost_offset=[0,0,0.05]'
ctl "xiao up 0.2"                  check_lid     none -D 'ghost_offset=[0,0,0.2]'
ctl "xiao up 0.3 (pads)"           check_lid     hit  -D 'ghost_offset=[0,0,0.3]'
ctl "xiao down 0.04"               check_tray    none -D 'ghost_offset=[0,0,-0.04]'
ctl "xiao down 0.06 (tongue)"      check_tray    hit  -D 'ghost_offset=[0,0,-0.06]'
ctl "xiao +X 0.15"                 check_lid     none -D 'ghost_offset=[0.15,0,0]'
ctl "xiao -X 0.15"                 check_lid     none -D 'ghost_offset=[-0.15,0,0]'
ctl "xiao +X 0.3 (+X wall)"        check_lid     hit  -D 'ghost_offset=[0.3,0,0]'
ctl "xiao -X 0.3 (stop rib)"       check_lid     hit  -D 'ghost_offset=[-0.3,0,0]'
ctl "xiao fwd 0.15"                check_lid     none -D 'ghost_offset=[0,-0.15,0]'
ctl "xiao back 0.15"               check_lid     none -D 'ghost_offset=[0,0.15,0]'
ctl "xiao back 0.15"               check_tray    none -D 'ghost_offset=[0,0.15,0]'
ctl "xiao fwd 0.3 (front wall)"    check_lid     hit  -D 'ghost_offset=[0,-0.3,0]'
ctl "xiao back 0.35 (shield stop)" check_lid     hit  -D 'ghost_offset=[0,0.35,0]'
# wire keep-outs: the USB shell pad across the 5V/GND corridor, and the front wall without its 5V/GND/3V3 relief
ctl "USB pad over the corridor"    check_lid     hit  -D ctl_usbpad_full=true
ctl "front wall without relief"    check_lid     hit  -D fw_relief=false
# revision 5: the 5V/GND corridor, 5.0 wide, is 0.3 off the shield stop block and the USB pad; a
# corridor keep-out wider than the case's must hit
ctl "corridor zone 0.4 wider"      check_lid     hit  -D ghost_wch_hw=2.9
ctl "corridor zone 0.2 wider"      check_lid     none -D ghost_wch_hw=2.7
# revision 5: the rear-edge D3/D5 zones (joints, the strip over the rear edge, the diode) against the shield stop
# block (0.3 +X of the D3 joint zone) and the -X stop rib (3.76 -X of the D5 joint zone)
ctl "rear zones +X 0.2"            check_lid     none -D 'ghost_rear_d=[0.2,0,0]'
ctl "rear zones +X 0.4 (stop blk)" check_lid     hit  -D 'ghost_rear_d=[0.4,0,0]'
ctl "rear zones -X 3.6"            check_lid     none -D 'ghost_rear_d=[-3.6,0,0]'
ctl "rear zones -X 3.9 (stop rib)" check_lid     hit  -D 'ghost_rear_d=[-3.9,0,0]'
# revision 5: the RX diode keep-out (DO-41 body + 0.3, both leads + 0.3) beside the Dupont housing: nearest is the
# shield stop block's rear corner (0.18 radially from the keep-out, 0.48 from the body): -Y 0.2 stays clear, -Y 0.4
# must hit the block; +X has room, the rear wall is 2.6 behind the keep-out
ctl "diode -Y 0.2"                 check_lid     none -D 'ghost_diode_d=[0,-0.2,0]'
ctl "diode -Y 0.4 (stop block)"    check_lid     hit  -D 'ghost_diode_d=[0,-0.4,0]'
ctl "diode +X 1"                   check_lid     none -D 'ghost_diode_d=[1,0,0]'
ctl "diode +Y 2"                   check_lid     none -D 'ghost_diode_d=[0,2,0]'
ctl "diode +Y 2.8 (rear wall)"     check_lid     hit  -D 'ghost_diode_d=[0,2.8,0]'
ctl "diode off"                    check_lid     none -D diode_on=false
# the diode body is rigid, so check_lid_path sweeps it: a lid block under the body, outside every keep-out, must be
# clear when closed and hit on the way down
ctl "lid block under the diode"    check_lid      none -D ctl_diode_undercut=true
ctl "lid block under the diode"    check_lid_path hit  -D '$fn=16' -D ctl_diode_undercut=true
# Dupont housing + wire-bend zone (keep-out, with the wire zones' extension of it from X -9 to the corridor), moved
# 1 mm: the ones that stay clear, and what the others meet
ctl "dupont +X 1"                  check_lid      none -D 'ghost_dupont_d=[1,0,0]'
ctl "dupont -X 1"                  check_lid      none -D 'ghost_dupont_d=[-1,0,0]'
ctl "dupont down 1"                check_lid      none -D 'ghost_dupont_d=[0,0,-1]'
ctl "dupont -Y 1 (rib, stop blk)"  check_lid      hit  -D 'ghost_dupont_d=[0,-1,0]'
ctl "dupont up 1 (ceiling)"        check_lid      hit  -D 'ghost_dupont_d=[0,0,1]'
# revision 4: the housing keep-out, the bend zone and the 5V/GND zone run dupont_rear_clr behind the housing, and
# the rear wall is 0.73 behind that; the revision 3 rear face (bs_clr behind the PCB) must hit
ctl "dupont +Y 0.5"                check_lid      none -D 'ghost_dupont_d=[0,0.5,0]'
ctl "dupont +Y 0.5"                check_tray     none -D 'ghost_dupont_d=[0,0.5,0]'
ctl "dupont +Y 1 (rear wall)"      check_lid      hit  -D 'ghost_dupont_d=[0,1,0]'
ctl "dupont +Y 1 (rear wall)"      check_tray     hit  -D 'ghost_dupont_d=[0,1,0]'
ctl "rev 3 rear face (lid)"        check_lid      hit  -D ctl_rear_rev3=true
ctl "rev 3 rear face (tray)"       check_tray     hit  -D ctl_rear_rev3=true
# revision 4: pin 9 wire keep-out (joint 2.5 deep, 0.5 above the floor; run 2.0 under the board, 1.0 above the
# floor; riser in the backstop notch, exactly wire_pass wide, against the rear wall)
ctl "pin 9 wire down 0.4"          check_tray     none -D 'ghost_p9_d=[0,0,-0.4]'
ctl "pin 9 wire down 0.6 (floor)"  check_tray     hit  -D 'ghost_p9_d=[0,0,-0.6]'
ctl "pin 9 wire +X 0.1 (notch)"    check_tray     hit  -D 'ghost_p9_d=[0.1,0,0]'
ctl "pin 9 wire -X 0.1 (notch)"    check_tray     hit  -D 'ghost_p9_d=[-0.1,0,0]'
ctl "pin 9 wire +Y 0.1 (rear wall)" check_lid     hit  -D 'ghost_p9_d=[0,0.1,0]'
ctl "no backstop notch"            check_tray     hit  -D p9_notch=false
# revision 4: jack screw hardware behind the flange holes (7 mm, 5 mm behind the plate): the front wall is cut back
# jack_clr above it, the lid side walls are 0.2 outside it (the tray walls, below z 4, are further); revision 3's full-width front wall ("wings") must hit
ctl "jack hardware up 0.2"         check_lid      none -D 'ghost_jack_d=[0,0,0.2]'
ctl "jack hardware up 0.4 (wall)"  check_lid      hit  -D 'ghost_jack_d=[0,0,0.4]'
ctl "jack hardware +X 0.15"        check_lid      none -D 'ghost_jack_d=[0.15,0,0]'
ctl "jack hardware -X 0.15"        check_lid      none -D 'ghost_jack_d=[-0.15,0,0]'
ctl "jack hardware +X 0.3 (wall)"  check_lid      hit  -D 'ghost_jack_d=[0.3,0,0]'
ctl "jack hardware -X 0.3 (wall)"  check_lid      hit  -D 'ghost_jack_d=[-0.3,0,0]'
ctl "rev 3 wings vs the hardware"  check_lid      hit  -D fw_wings=true
ctl "rev 3 wings (lid path)"       check_lid_path hit  -D '$fn=16' -D fw_wings=true
# flange plate (full flange outline to fl_plate_d 5.1): 0.5 deeper must hit the lid front wall; the revision 2 lid
# (front wall from the steel flange back, fl_plate_d = fl_t) must hit the 5.1 plate: the fault the user reported
ctl "plate 0.5 deeper"             check_lid      hit  -D ghost_plate_d=5.6
ctl "plate 0.5 deeper (path)"      check_lid_path hit  -D '$fn=16' -D ghost_plate_d=5.6
ctl "rev 2 lid vs the 5.1 plate"   check_lid      hit  -D fl_plate_d=0.38 -D ghost_plate_d=5.1
# tray against lid: the check must see a too-tight snap pocket and a presser into the tray wall
ctl "snap pockets 0.3 too tight"   check_traylid hit  -D clip_clr=-0.3
ctl "presser into the tray wall"   check_traylid hit  -D press_wall_clr=-0.1
# snap engagement with the lid held high (by a pad, presser or the flange): up to 0.35 the hook top is still under
# the catch window top (lc_win_up), so the finger springs in; the old 0.1 window misses at 0.3
ctl "lid up 0.3 (snaps still catch)" check_traylid none -D ghost_lid_dz=0.3
ctl "lid up 0.4 (hook above window)" check_traylid hit -D ghost_lid_dz=0.4
ctl "lid up 0.3, old window 0.1"   check_traylid hit  -D ghost_lid_dz=0.3 -D lc_win_up=0.1
# logo: every stroke and gap >= logo_min at logo_w 25; the same mark at 20 mm breaks it (0.6 mm corner fillets)
ctl "logo at 25 mm"                check_logo     none
ctl "logo at 20 mm"                check_logo     hit  -D logo_w=20

rm -rf "$TMP"
echo "controls: $fails failed"
[ "$fails" -eq 0 ]
