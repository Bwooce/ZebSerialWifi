#!/bin/bash
# Build zebserial_case.3mf (H2D 0.4, Bambu PETG Basic, textured PEI) with Bambu Studio's CLI.
# Runs on a Mac with Bambu Studio installed. Usage: run_slice.sh <workdir with tray.stl lid.stl and this folder's files>
set -euo pipefail
W="$1"
APP=/Applications/BambuStudio.app/Contents
P="$APP/Resources/profiles/BBL"
cd "$W"
python3 resolve_profile.py "$P" machine "Bambu Lab H2D 0.4 nozzle" - machine.json
python3 resolve_profile.py "$P" process "0.16mm Balanced Quality @BBL H2D" process_overrides.json process.json
python3 resolve_profile.py "$P" filament "Bambu PETG Basic @BBL H2D 0.4 nozzle" filament_overrides.json filament.json
rm -f zebserial_case.3mf result.json
"$APP/MacOS/BambuStudio" --outputdir "$W" --orient 0 --arrange 1 \
    --load-settings "machine.json;process.json" --load-filaments "filament.json" \
    --slice 0 --export-3mf zebserial_case.3mf tray.stl lid.stl > slice.log 2>&1 || true
python3 -c "import json; r=json.load(open('result.json')); print('return_code', r['return_code'], r.get('error_string',''))"
ls -la zebserial_case.3mf
