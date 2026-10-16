#!/bin/sh
# Positive and negative controls for the interference checks: each line moves a ghost and says whether the named
# check must find real overlap ("hit": a piece of at least 0.01 mm3, thicker than 0.005 mm in every axis) or none.
# Prints PASS / FAIL per line and exits non-zero on any FAIL. An OpenSCAD error (an ERROR line such as a failed assert,
# or a non-zero exit that is not just "Current top level object is empty.") is reported as got "error" and fails.
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
  if grep -qE '^ERROR:|Assertion|Execution aborted' "$TMP/o.log" ||
     { [ "$st" -ne 0 ] && ! grep -q 'Current top level object is empty' "$TMP/o.log"; }; then
    got="error (exit $st: $(grep -m1 -E '^ERROR:|Assertion' "$TMP/o.log" | cut -c1-80))"
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
# RS232 board + DB9 fore-aft (lips 0.1 in front, lid front wall at 0 behind, tray rear wall 0.25 behind the PCB)
ctl "board fwd 0.05"               check_tray    none -D ghost_board_dy=-0.05
ctl "board fwd 0.05"               check_lid     none -D ghost_board_dy=-0.05
ctl "board fwd 0.15 (bottom lip)"  check_tray    hit  -D ghost_board_dy=-0.15
ctl "board fwd 0.15 (top lip)"     check_lid     hit  -D ghost_board_dy=-0.15
ctl "board fwd 1.2 (bottom lip)"   check_tray    hit  -D ghost_board_dy=-1.2
ctl "board back 0.2"               check_tray    none -D ghost_board_dy=0.2
ctl "board back 0.2 (lid wall)"    check_lid     hit  -D ghost_board_dy=0.2
ctl "board back 0.3 (rear wall)"   check_tray    hit  -D ghost_board_dy=0.3
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
# corner pressers: moved 0.4 forward they reach the 5V/GND joints on the XIAO rear edge
ctl "pressers 0.4 fwd (5V/GND)"    check_lid     hit  -D cp_front=1.2
# XIAO: pads 0.25 above, tongue 0.05 below, -X wall and +X stop rib 0.2, front wall 0.2 in front, shield stop 0.3
# behind the shield; the USB slot sides are 0.2 off the shell either way in Y
ctl "xiao up 0.05"                 check_lid     none -D 'ghost_offset=[0,0,0.05]'
ctl "xiao up 0.2"                  check_lid     none -D 'ghost_offset=[0,0,0.2]'
ctl "xiao up 0.3 (pads)"           check_lid     hit  -D 'ghost_offset=[0,0,0.3]'
ctl "xiao down 0.04"               check_tray    none -D 'ghost_offset=[0,0,-0.04]'
ctl "xiao down 0.06 (tongue)"      check_tray    hit  -D 'ghost_offset=[0,0,-0.06]'
ctl "xiao +X 0.15"                 check_lid     none -D 'ghost_offset=[0.15,0,0]'
ctl "xiao -X 0.15"                 check_lid     none -D 'ghost_offset=[-0.15,0,0]'
ctl "xiao +X 0.3 (stop rib)"       check_lid     hit  -D 'ghost_offset=[0.3,0,0]'
ctl "xiao -X 0.3 (-X wall)"        check_lid     hit  -D 'ghost_offset=[-0.3,0,0]'
ctl "xiao fwd 0.15"                check_lid     none -D 'ghost_offset=[0,-0.15,0]'
ctl "xiao back 0.15"               check_lid     none -D 'ghost_offset=[0,0.15,0]'
ctl "xiao back 0.15"               check_tray    none -D 'ghost_offset=[0,0.15,0]'
ctl "xiao fwd 0.3 (front wall)"    check_lid     hit  -D 'ghost_offset=[0,-0.3,0]'
ctl "xiao back 0.35 (shield stop)" check_lid     hit  -D 'ghost_offset=[0,0.35,0]'
# wire keep-outs: the shield pad ring across the D3/D5 channel, and the front wall without its joint reliefs
ctl "shield ring over the channel" check_lid     hit  -D ctl_ring_full=true
ctl "front wall without reliefs"   check_lid     hit  -D fw_relief=false
# Dupont housing + wire-bend zone (keep-out), moved 1 mm: the ones that stay clear, and what the others meet
ctl "dupont +X 1"                  check_lid      none -D 'ghost_dupont_d=[1,0,0]'
ctl "dupont -X 1"                  check_lid      none -D 'ghost_dupont_d=[-1,0,0]'
ctl "dupont down 1"                check_lid      none -D 'ghost_dupont_d=[0,0,-1]'
ctl "dupont -Y 1 (shield stop)"    check_lid      hit  -D 'ghost_dupont_d=[0,-1,0]'
ctl "dupont +Y 1 (rear wall)"      check_lid      hit  -D 'ghost_dupont_d=[0,1,0]'
ctl "dupont up 1 (ceiling)"        check_lid      hit  -D 'ghost_dupont_d=[0,0,1]'
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
