#!/bin/sh
# p1c (job p1c) checks. Every check prints NAME_EXIT=n, 0 = pass.
# Run from anywhere in the clone: sh docs/briefs/2026-11-07-p1c.check.sh
# Its whole output goes to build/check.out; compare it with the .check.expected file.
cd "$(git rev-parse --show-toplevel)" || exit 1
B=build/p1ccheck
rm -rf "$B" && mkdir -p "$B" || exit 1
exec > build/check.out 2>&1

# the host tests and the render check
tools/build_host.sh > "$B/host.log" 2>&1; echo BUILD_HOST_EXIT=$?
grep -E '^(render_screens: |oc_port: [0-9]+ checks|oc_ui: [0-9]+ checks|oc_nav: [0-9]+ checks)' "$B/host.log"
grep -c ' FAIL$' "$B/host.log"

# the device build (the toolchain on this host), its sizes and its memory map
"$HOME/.venvs/pio/bin/pio" run -e T40 > "$B/pio.log" 2>&1; echo PIO_EXIT=$?
grep -E '^teensy_size: +(FLASH|RAM1|RAM2):' "$B/pio.log"
python3 tools/check_sections.py .pio/build/T40/firmware.elf > "$B/sections.log" 2>&1; echo SECTIONS_EXIT=$?
tail -n 1 "$B/sections.log"
python3 tools/check_read_only.py; echo READ_ONLY_EXIT=$?
test -z "$(grep -rnE 'flash_write|flash_erase|eeprom_write|lfs_format|lfs_file_write|lfs_remove|lfs_rename|EEPROM\.(put|write|update)|eepromemu_|LittleFS_Program|\.format\(|eraseFiles' firmware/ parts/)"; echo WRITE_PATH_EXIT=$?

# the shell is wired: the old button edges and pages are gone, the sampler starts in normal mode only
grep -cw 'oc_buttons_edges\|ui_page_next\|ui_button\|ui_input' firmware/oc_platform.cpp firmware/oc_ui.cpp
grep -c 'ui_start();\|ui_service();' firmware/oc_platform.cpp
grep -c 'ui_timer.priority(UI_SAMPLE_PRIORITY)' firmware/oc_ui.cpp
md5sum firmware/oc_ui.cpp firmware/oc_platform.cpp firmware/oc_diag.cpp firmware/oc_ui_parts.c parts/oc_ui_nav.c parts/oc_ui_nav.h tests/oc_ui_nav_test.c tools/render_screens.sh tools/render_screens/Arduino.h tools/render_screens/rs_main.cpp docs/GUIDE.md README.md CHANGELOG.md
md5sum docs/img/screen-*.png

# the tree: the files the patch touches and nothing else; the pinned files untouched
git diff --quiet origin/feat/ui-p1c -- docs/briefs docs/STATUS.md .github vendor; echo PINNED_UNCHANGED_EXIT=$?
git diff --numstat origin/feat/ui-p1c
git status --porcelain | LC_ALL=C sort
