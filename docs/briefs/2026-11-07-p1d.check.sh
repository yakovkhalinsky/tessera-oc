#!/bin/sh
# p1d (job p1d) checks. Every check prints NAME_EXIT=n, 0 = pass.
# Run from anywhere in the clone: sh docs/briefs/2026-11-07-p1d.check.sh
# Its whole output goes to build/check.out; compare it with the .check.expected file.
cd "$(git rev-parse --show-toplevel)" || exit 1
B=build/p1dcheck
rm -rf "$B" && mkdir -p "$B" || exit 1
exec > build/check.out 2>&1

# the host tests and the render check
tools/build_host.sh > "$B/host.log" 2>&1; echo BUILD_HOST_EXIT=$?
grep -E '^(render_screens: |oc_port: [0-9]+ checks|oc_ui: [0-9]+ checks|oc_nav: [0-9]+ checks)' "$B/host.log"
grep -c ' FAIL$' "$B/host.log"
grep -E '^oc_ui: ring: ' "$B/host.log"

# the device build (the toolchain on this host), its sizes and its memory map
"$HOME/.venvs/pio/bin/pio" run -e T40 > "$B/pio.log" 2>&1; echo PIO_EXIT=$?
grep -E '^teensy_size: +(FLASH|RAM1|RAM2):' "$B/pio.log"
python3 tools/check_sections.py .pio/build/T40/firmware.elf > "$B/sections.log" 2>&1; echo SECTIONS_EXIT=$?
tail -n 1 "$B/sections.log"
python3 tools/check_read_only.py; echo READ_ONLY_EXIT=$?
test -z "$(grep -rnE 'flash_write|flash_erase|eeprom_write|lfs_format|lfs_file_write|lfs_remove|lfs_rename|EEPROM\.(put|write|update)|eepromemu_|LittleFS_Program|\.format\(|eraseFiles' firmware/ parts/)"; echo WRITE_PATH_EXIT=$?

# (a) the priority comes before begin(); (b) the ring counts and INFO shows it
grep -n 'ui_timer\.priority(\|ui_timer\.begin(' firmware/oc_ui.cpp
grep -c 'r->dropped++' parts/oc_ui_input.c
grep -c 'RING %lu' firmware/oc_ui.cpp
# (c) the note in the plan
grep -c '^\*\*Note (p1d): 1 kHz polling is fine for the detented encoders.\*\*' docs/UI-PLAN.md
md5sum firmware/oc_ui.cpp parts/oc_ui_input.c parts/oc_ui_input.h tests/oc_ui_test.c docs/GUIDE.md docs/UI-PLAN.md CHANGELOG.md
md5sum docs/img/screen-*.png

# the tree: the files the patch touches and nothing else; no engine change; the pinned files untouched
git diff --quiet origin/feat/ui-p1d -- docs/briefs docs/STATUS.md .github vendor firmware/oc_engine.c firmware/oc_hal.h firmware/oc_port.h firmware/oc_platform.cpp firmware/oc_storage.cpp firmware/oc_display.cpp firmware/oc_diag.cpp; echo PINNED_UNCHANGED_EXIT=$?
git diff --numstat origin/feat/ui-p1d
git status --porcelain | LC_ALL=C sort
