#!/bin/sh
# p3 (job p3) checks. Every check prints NAME_EXIT=n, 0 = pass.
# Run from anywhere in the clone: sh docs/briefs/2026-11-10-p3.check.sh
# Its whole output goes to build/check.out; compare it with the .check.expected file.
cd "$(git rev-parse --show-toplevel)" || exit 1
export LC_ALL=C
B=build/p3check
rm -rf "$B" && mkdir -p "$B" || exit 1
exec > build/check.out 2>&1

# the host tests, the pictures and the goldens
tools/build_host.sh > "$B/host.log" 2>&1; echo BUILD_HOST_EXIT=$?
grep -E '^(render_screens: |goldens: |oc_port: [0-9]+ checks|oc_ui: [0-9]+ checks|oc_nav: [0-9]+ checks|oc_overview: [0-9]+ checks|oc_gfx: [0-9]+ checks)' "$B/host.log"
grep -c ' FAIL$' "$B/host.log"

# the device build (the toolchain on this host), its sizes and its memory map
"$HOME/.venvs/pio/bin/pio" run -e T40 > "$B/pio.log" 2>&1; echo PIO_EXIT=$?
grep -E '^teensy_size: +(FLASH|RAM1|RAM2):' "$B/pio.log"
python3 tools/check_sections.py .pio/build/T40/firmware.elf > "$B/sections.log" 2>&1; echo SECTIONS_EXIT=$?
tail -n 1 "$B/sections.log"
python3 tools/check_read_only.py; echo READ_ONLY_EXIT=$?
test -z "$(grep -rnE 'flash_write|flash_erase|eeprom_write|lfs_format|lfs_file_write|lfs_remove|lfs_rename|EEPROM\.(put|write|update)|eepromemu_|LittleFS_Program|\.format\(|eraseFiles' firmware/ parts/)"; echo WRITE_PATH_EXIT=$?

# OVERVIEW is drawn from ui_draw; the sampler only queues input; GEN_NAME is gone
grep -n 'ui_overview_draw\|ui_hist_shift\|oc_track_gate_seen' firmware/oc_ui.cpp
grep -c 'static void ui_sample(void)' firmware/oc_ui.cpp
grep -c 'ui_hist\[' firmware/oc_ui.cpp
grep -c GEN_NAME firmware/oc_ui.cpp || true
grep -c 'oc_gen_label' parts/oc_view.c
grep -c 'oc_track_last_note' parts/oc_out.c
grep -c 'oc_track_gate_seen' parts/oc_out.c
grep -c 'oc_track_roll_pending' parts/oc_view.c
grep -c 'void ui_note_text' parts/oc_ui_overview.c
grep -c 'uint16_t ui_hist_shift' parts/oc_ui_overview.c
md5sum CHANGELOG.md docs/GUIDE.md firmware/oc_port.h firmware/oc_ui.cpp firmware/oc_ui_parts.c \
  parts/oc_out.c parts/oc_ui_overview.c parts/oc_ui_overview.h parts/oc_view.c \
  tests/oc_ui_overview_test.c tools/build_host.sh tools/render_screens/rs_main.cpp
md5sum docs/img/screen-*.png
md5sum tests/golden/screen-*.txt

# the tree: the files the patch touches and nothing else; no engine change; the pinned files untouched
git diff --quiet origin/feat/ui-p3 -- docs/briefs docs/STATUS.md .github vendor firmware/oc_engine.c firmware/oc_hal.h firmware/oc_platform.cpp firmware/oc_storage.cpp firmware/oc_display.cpp firmware/oc_diag.cpp; echo PINNED_UNCHANGED_EXIT=$?
git diff --numstat origin/feat/ui-p3
git status --porcelain | LC_ALL=C sort
