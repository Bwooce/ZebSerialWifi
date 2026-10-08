#!/bin/bash
# Slice the ZebSerialWifi print projects (H2D 0.4, Bambu PETG Basic, textured PEI) with Bambu Studio's CLI.
# Runs on a Mac with Bambu Studio installed.
# Usage: run_slice.sh <workdir> [case|coupon]
#   The workdir holds this folder's files plus the STLs from enclosure/out:
#   case:   tray.stl lid.stl logo.stl -> zebserial_case.3mf. The logo is a second part of the lid object on
#           filament 2, so it prints as a flush two-colour inlay.
#   coupon: coupon_tray.stl coupon_lid.stl -> zebserial_coupon.3mf, single colour.
set -euo pipefail
W="$1"
MODE="${2:-case}"
APP=/Applications/BambuStudio.app/Contents
P="$APP/Resources/profiles/BBL"
cd "$W"
python3 resolve_profile.py "$P" machine "Bambu Lab H2D 0.4 nozzle" - machine.json
python3 resolve_profile.py "$P" process "0.16mm Balanced Quality @BBL H2D" process_overrides.json process.json
python3 resolve_profile.py "$P" filament "Bambu PETG Basic @BBL H2D 0.4 nozzle" filament_overrides.json filament.json
rm -f result.json
if [ "$MODE" = case ]; then
    # the second filament needs its own preset name and colour, or the CLI crashes
    python3 resolve_profile.py "$P" filament "Bambu PETG Basic @BBL H2D 0.4 nozzle" filament_overrides_logo.json filament_logo.json
    OUT=zebserial_case.3mf
    # objects with the same assemble_index become parts of one object; the STLs share the lid's coordinates
    cat > assemble.json <<JSON
{"plates": [{"plate_name": "zebserial", "need_arrange": true, "objects": [
  {"path": "$W/tray.stl", "count": 1, "filaments": [1], "assemble_index": [1], "pos_x": [0], "pos_y": [0], "pos_z": [0]},
  {"path": "$W/lid.stl",  "count": 1, "filaments": [1], "assemble_index": [2], "pos_x": [0], "pos_y": [0], "pos_z": [0]},
  {"path": "$W/logo.stl", "count": 1, "filaments": [2], "assemble_index": [2], "pos_x": [0], "pos_y": [0], "pos_z": [0]}
]}]}
JSON
    rm -f "$OUT"
    "$APP/MacOS/BambuStudio" --outputdir "$W" --load-settings "machine.json;process.json" \
        --load-filaments "filament.json;filament_logo.json" --load-assemble-list assemble.json \
        --slice 0 --export-3mf "$OUT" > slice.log 2>&1 || true
else
    OUT=zebserial_coupon.3mf
    rm -f "$OUT"
    "$APP/MacOS/BambuStudio" --outputdir "$W" --orient 0 --arrange 1 \
        --load-settings "machine.json;process.json" --load-filaments "filament.json" \
        --slice 0 --export-3mf "$OUT" coupon_tray.stl coupon_lid.stl > slice.log 2>&1 || true
fi
python3 -c "import json; r=json.load(open('result.json')); print('return_code', r['return_code'], r.get('error_string',''))"
ls -la "$OUT"
unzip -p "$OUT" Metadata/plate_1.gcode | grep -m2 -E 'total estimated time|total filament weight'
