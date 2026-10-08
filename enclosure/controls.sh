#!/bin/sh
# Positive and negative controls for the interference checks: each line moves a ghost and says whether the named
# check must find real overlap ("hit": a piece of at least 0.01 mm3, thicker than 0.005 mm in every axis) or none.
# Prints PASS / FAIL per line and exits non-zero on any FAIL.
# Usage: ./controls.sh
set -u
cd "$(dirname "$0")"
OSC=${OPENSCAD:-/opt/homebrew/bin/openscad}
TMP=$(mktemp -d)
fails=0

ctl() {   # name check expect -D ...
  name=$1; chk=$2; want=$3; shift 3
  rm -f "$TMP/o.stl"
  "$OSC" --backend=manifold "$@" -D "part=\"$chk\"" -o "$TMP/o.stl" zebserial_case.scad > "$TMP/o.log" 2>&1
  if [ -s "$TMP/o.stl" ]; then
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
# RS232 board + DB9 fore-aft (lips 0.1 in front, lid front wall at 0 behind, backstop 0.25 behind the PCB)
ctl "board fwd 0.05"               check_tray    none -D ghost_board_dy=-0.05
ctl "board fwd 0.05"               check_lid     none -D ghost_board_dy=-0.05
ctl "board fwd 0.15 (bottom lip)"  check_tray    hit  -D ghost_board_dy=-0.15
ctl "board fwd 0.15 (top lip)"     check_lid     hit  -D ghost_board_dy=-0.15
ctl "board fwd 1.2 (bottom lip)"   check_tray    hit  -D ghost_board_dy=-1.2
ctl "board back 0.2"               check_tray    none -D ghost_board_dy=0.2
ctl "board back 0.2 (lid wall)"    check_lid     hit  -D ghost_board_dy=0.2
ctl "board back 0.3 (backstop)"    check_tray    hit  -D ghost_board_dy=0.3
ctl "board back 0.5 (backstop)"    check_tray    hit  -D ghost_board_dy=0.5
# board-lock leg tails in their loose pockets (0.6 per side in Y, 1.3 in X)
ctl "legs +0.5 Y"                  check_tray    none -D ghost_lug_dy=0.5
ctl "legs -0.5 Y"                  check_tray    none -D ghost_lug_dy=-0.5
ctl "legs +0.7 Y"                  check_tray    hit  -D ghost_lug_dy=0.7
ctl "legs -0.7 Y"                  check_tray    hit  -D ghost_lug_dy=-0.7
ctl "legs +0.9 X"                  check_tray    none -D ghost_lug_dx=0.9
# XIAO: pads 0.1 above, tongue 0.05 below, guides 0.2 either side, strut 0.2 in front, rear wall 0.2 behind
ctl "xiao up 0.05"                 check_lid     none -D 'ghost_offset=[0,0,0.05]'
ctl "xiao up 0.15 (pads)"          check_lid     hit  -D 'ghost_offset=[0,0,0.15]'
ctl "xiao up 0.3 (pads)"           check_lid     hit  -D 'ghost_offset=[0,0,0.3]'
ctl "xiao down 0.04"               check_tray    none -D 'ghost_offset=[0,0,-0.04]'
ctl "xiao down 0.06 (tongue)"      check_tray    hit  -D 'ghost_offset=[0,0,-0.06]'
ctl "xiao +X 0.15"                 check_lid     none -D 'ghost_offset=[0.15,0,0]'
ctl "xiao +X 0.15"                 check_tray    none -D 'ghost_offset=[0.15,0,0]'
ctl "xiao +X 0.3 (guide)"          check_lid     hit  -D 'ghost_offset=[0.3,0,0]'
ctl "xiao -X 0.3 (guide)"          check_lid     hit  -D 'ghost_offset=[-0.3,0,0]'
ctl "xiao fwd 0.3 (strut)"         check_lid     hit  -D 'ghost_offset=[0,-0.3,0]'
ctl "xiao back 0.3 (rear wall)"    check_lid     hit  -D 'ghost_offset=[0,0.3,0]'
# wire keep-out: a guide pushed into the D4/D5 zone
ctl "-X guide at d 13..17 (wires)" check_lid     hit  -D dupont_on=false -D 'xguides=[[-1,13,17],[1,11.8,18.8]]'
# Dupont housing + wire-bend zone (keep-out), moved 1 mm: + X and down stay clear; the rest show what they meet
ctl "dupont +X 1"                  check_lid      none -D 'ghost_dupont_d=[1,0,0]'
ctl "dupont down 1"                check_lid      none -D 'ghost_dupont_d=[0,0,-1]'
ctl "dupont -X 1 (side wall)"      check_lid      hit  -D 'ghost_dupont_d=[-1,0,0]'
ctl "dupont -Y 1 (-X presser)"     check_lid      hit  -D 'ghost_dupont_d=[0,-1,0]'
ctl "dupont +Y 1 (-X guide)"       check_lid      hit  -D 'ghost_dupont_d=[0,1,0]'
ctl "dupont up 1 (ceiling)"        check_lid      hit  -D 'ghost_dupont_d=[0,0,1]'
ctl "-X guide untrimmed (zone)"    check_lid      hit  -D dupont_clr=-20
# RS232 PCB rear-edge keep-out (resistors): a presser at Y 24 (the revision 2 position) must hit it
ctl "+X presser at Y 24 (rear edge)" check_lid    hit  -D 'press_y_right=[13.8,17,24]'
# logo: every stroke and gap >= logo_min at logo_w 30; the same mark at 20 mm breaks it (0.6 mm corner fillets)
ctl "logo at 30 mm"                check_logo     none
ctl "logo at 20 mm"                check_logo     hit  -D logo_w=20

rm -rf "$TMP"
echo "controls: $fails failed"
[ "$fails" -eq 0 ]
