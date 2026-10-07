#!/usr/bin/env python3
"""Flatten a Bambu Studio system profile (follow its "inherits" chain) and apply overrides.

The Bambu Studio CLI wants complete profiles; system profiles are partial and inherit from
one another. Usage:
  resolve_profile.py <profiles_dir> <kind: machine|process|filament> <name> <overrides.json|-> <out.json>
"""
import json
import os
import sys


def load(profiles_dir, kind, name):
    path = os.path.join(profiles_dir, kind, name + ".json")
    with open(path) as f:
        return json.load(f)


def resolve(profiles_dir, kind, name):
    chain = []
    while name:
        p = load(profiles_dir, kind, name)
        chain.append(p)
        name = p.get("inherits", "")
    merged = {}
    for p in reversed(chain):
        merged.update(p)
    merged.pop("inherits", None)
    return merged, chain[0].get("name", "")


def main():
    profiles_dir, kind, name, overrides, out = sys.argv[1:6]
    cfg, system_name = resolve(profiles_dir, kind, name)
    if overrides != "-":
        with open(overrides) as f:
            for k, v in json.load(f).items():
                base = cfg.get(k)
                # H2D profiles hold one value per extruder variant; broadcast a short override
                if isinstance(base, list) and isinstance(v, list) and len(v) == 1 and len(base) > 1:
                    v = v * len(base)
                cfg[k] = v
    # mark as a user preset derived from the system one so Studio keeps the overrides visible
    cfg["from"] = "User"
    # a user preset that inherits the system preset is how Studio checks printer compatibility
    cfg["inherits"] = system_name
    cfg["is_custom_defined"] = "0"
    # the CLI matches presets by these ids, which system profiles leave empty
    final = cfg.get("name", name)
    if kind == "machine":
        cfg["printer_settings_id"] = final
    elif kind == "process":
        cfg["print_settings_id"] = final
    elif kind == "filament":
        cfg["filament_settings_id"] = [final]
    with open(out, "w") as f:
        json.dump(cfg, f, indent=2)
    print(f"{kind}: {name} -> {out} ({len(cfg)} keys)")


if __name__ == "__main__":
    main()
