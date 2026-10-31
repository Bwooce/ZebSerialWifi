# Notes for agents working on ZebSerialWifi

ZebSerialWifi is a Wi-Fi to RS-232 bridge for Zebra label printers: a Seeed XIAO ESP32-C6 and a MAX3232 module in a
3D-printed case. README.md describes the device for its users; `openspec/config.yaml` holds the project context,
constraints and conventions that every spec and change follows. Read both before changing behaviour.

## Repository map

| Path | What |
|---|---|
| `firmware/` | ESP-IDF 6.1 project, target esp32c6. Sources in `firmware/main/`; the web pages and their JavaScript are C string literals in `web.c`. |
| `firmware/test_host/` | Host unit tests for the pure parsers (no ESP-IDF needed). |
| `tools/cable-labels.html` | Stand-alone page that prints cable flag labels through the bridge's `/pstprnt`. |
| `tools/check_page_js.py` | Runs `node --check` on the page scripts in `web.c` and `cable-labels.html`. |
| `enclosure/` | OpenSCAD case model, `build.sh` (STLs, renders and gates), `controls.sh` (controls for the gates). See `enclosure/README.md`. |
| `print/` | Bambu Studio CLI slicing for the case (Mac with Bambu Studio only). See `print/README.md`. |
| `openspec/` | `specs/` is the behaviour as built, one capability per directory; `changes/` holds proposals, `changes/archive/` finished ones. |
| `docs/` | Images for the README, the original design and plan (`superpowers/`, older than the code) and the papercuts log. |
| `.github/workflows/ci.yml` | CI: the checks below, on every push and pull request. |

## Checks before committing

Run the ones your change touches. CI runs all of them, the enclosure gates only when `enclosure/` or the workflow
changes. Report what you ran, and never claim a check you did not run.

| Check | Command | When |
|---|---|---|
| OpenSpec | `npx -y @fission-ai/openspec@1.16.0 validate --all --strict` | any change under `openspec/` |
| Host tests | `make -C firmware/test_host` (on Linux add `CC=clang`); must end "N checks, 0 failures" | any change to a parser in `firmware/main/` |
| Firmware | `cd firmware && idf.py build`, with no warnings (warnings are errors) | any change under `firmware/` |
| Page JavaScript | `python3 tools/check_page_js.py` | any change to `web.c` or `tools/cable-labels.html` |
| Enclosure | `enclosure/build.sh` and `enclosure/controls.sh`, both with zero failures | any change under `enclosure/` |

On the Mac the ESP-IDF toolchain is set up with `. ~/esp/esp-idf-v6.1/export.sh`, and OpenSCAD is
`/opt/homebrew/bin/openscad` (override with `OPENSCAD=`; it needs the manifold backend, and `python3` needs numpy).
`check_page_js.py` cannot see the `var UI` script that `send_ui_script()` builds with `snprintf`; check that one in a
browser. CI's spec drift job warns, without failing, when a pull request changes `firmware/main/`, `enclosure/` or
`tools/` and nothing under `openspec/`.

## OpenSpec, in both directions

- **Spec to code.** A planned change in behaviour starts as a change proposal under `openspec/changes/<name>/`
  (proposal, design if needed, tasks, spec deltas). Then write the code, run the checks, and archive the change into
  `openspec/changes/archive/` so that `openspec/specs/` again describes the code as built.
- **Code to spec.** Any commit that changes behaviour updates the spec that owns that behaviour in the same commit,
  even for a small fix with no proposal. One spec owns each behaviour; others refer to it by name.
- Every requirement cites its source in a `**Source**:` line (file and function, constant, parameter or route), and
  numbers come from a named define, Kconfig symbol or SCAD parameter.
- A requirement states the intended behaviour. Where the code falls short, the spec lists the gap under
  "## Notes", "### Known deviations", with file:line; a fix removes its bullet in the same commit.
- Where a spec and the code disagree, the code wins: fix the document. The full rules are in `openspec/config.yaml`
  (`context` and `rules`); do not copy them elsewhere.

## House style

- Plain, precise English in short sentences; metric units; British/Australian spelling (behaviour, colour, centre,
  licence); no exclamation marks. This applies to prose, comments, specs, page text and commit messages.
- The device is "the bridge", the Zebra is "the printer", a TCP or HTTP peer is "the client". Write "setup network"
  and "setup access point", not "set-up".
- Commit messages: one plain sentence or clause saying what changed and why, often naming the area first
  ("Cable page: ...", "POST /pstprnt: ...", "Case revision 5: ..."). No type prefix and no AI attribution lines.
- Commit with explicit pathspecs (`git commit -m "..." -- <paths>`). Push at milestones.
- Host test expected values trace to printer output or Zebra manuals, never to values this code computed.

## Safety

- Never commit build output: `firmware/build/`, `firmware/sdkconfig`, `firmware/managed_components/` and
  `firmware/test_host/build/` are gitignored and stay that way. The one exception is `enclosure/out/*.stl` and
  `enclosure/renders/*.png`, which are tracked and which `enclosure/build.sh` deletes and regenerates: run it when the
  case is meant to change, and commit the STLs and renders with the model change. The STLs and the sliced 3MFs in
  `print/` are regenerated, never hand-edited.
- There are no secrets in the repository, and none should be added. Wi-Fi credentials live only in the device's NVS.
- The partition table and bootloader change only over USB (`idf.py flash`); an app image must fit the 0x1E0000 OTA
  slot. Before testing or reporting on a device, match its running firmware version to the commit you built.
