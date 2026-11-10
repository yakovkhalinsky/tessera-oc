#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the o_C port's host tests: the port's engine, the shared generator test,
# the port's own test (the pins and the encoders), and the tables the engine
# needs (generated from the vendored Tessera).
#
#   tools/build_host.sh          build and run
#   tools/build_host.sh build    build only
set -e
cd "$(dirname "$0")/.."
TESSERA=vendor/tessera
GEN=build/gen
OUT=build/host
PY="${PYTHON:-python3}"

mkdir -p "$GEN" "$OUT"

# the tables the engine reads: the sample rate and the control blocks (FS, CTL),
# the lookup tables, the DX7's patches and the 909's
[ -f "$GEN/melodee_tables.h" ]    || "$PY" "$TESSERA/tools/gen_tables.py" "$GEN/melodee_tables.h" >/dev/null
[ -f "$GEN/x0x_drum_tables.h" ]   || "$PY" "$TESSERA/tools/gen_909_tables.py" "$GEN/x0x_drum_tables.h" >/dev/null
[ -f "$GEN/melodee_fm6.h" ]       || "$PY" "$TESSERA/tools/gen_fm6_patches.py" "$GEN/melodee_fm6.h" >/dev/null

# the guide's screens (docs/GUIDE.md): docs/img must be what the screen code draws now,
# the same screen files both ways and the same bytes (the renderer runs on Linux only).
# The goldens (tests/golden/) are the same screens as text, which a reviewer can
# diff; UI_GOLDEN_UPDATE=1 rewrites them.
if [ "$1" != build ] && [ "$(uname -s)" != Linux ]; then
    echo "render_screens: check skipped (the renderer runs on Linux only)"
elif [ "$1" != build ]; then
    rm -rf build/render_check build/render_golden
    tools/render_screens.sh build/render_check build/render_golden >/dev/null
    for f in docs/img/screen-*.png; do
        [ -f "build/render_check/${f##*/}" ] || { echo "docs/img stale: run tools/render_screens.sh (${f##*/} is committed but not rendered)" >&2; exit 1; }
    done
    for f in build/render_check/screen-*.png; do
        [ -f "docs/img/${f##*/}" ] || { echo "docs/img stale: run tools/render_screens.sh (${f##*/} is rendered but not committed)" >&2; exit 1; }
        cmp -s "$f" "docs/img/${f##*/}" || { echo "docs/img stale: run tools/render_screens.sh (${f##*/} differs)" >&2; exit 1; }
    done
    echo "render_screens: docs/img current"

    for f in tests/golden/screen-*.txt; do
        [ -f "build/render_golden/${f##*/}" ] || { echo "goldens stale: run tools/render_screens.sh (${f##*/} is committed but not rendered)" >&2; exit 1; }
    done
    for f in build/render_golden/screen-*.txt; do
        [ -f "tests/golden/${f##*/}" ] || { echo "goldens stale: run tools/render_screens.sh (${f##*/} is rendered but not committed)" >&2; exit 1; }
    done
    if [ "${UI_GOLDEN_UPDATE:-}" = "1" ]; then
        mkdir -p tests/golden
        cp build/render_golden/screen-*.txt tests/golden/
        echo "goldens: updated"
    else
        for f in build/render_golden/screen-*.txt; do
            cmp -s "$f" "tests/golden/${f##*/}" || { echo "goldens stale: run tools/render_screens.sh (${f##*/} differs)" >&2; exit 1; }
        done
        echo "goldens: tests/golden current"
    fi
fi

# the port's own unit, the engine's sources (vendor/tessera/firmware/src) and the
# shared test (vendor/tessera/tests): the test finds the port's host shim
# through -Itests
cc -O2 -ffp-contract=off -w \
   -I"$GEN" -Ifirmware -I"$TESSERA/firmware/src" -Itests \
   -o "$OUT/oc_engine_test" tests/oc_engine_test.c -lm

# the port's own test: the module's pins (the o_C's own tables) and the
# encoders' quadrature (the turn the other way must end)
cc -O2 -w -Ifirmware -I"$TESSERA/firmware/src" -o "$OUT/oc_port_test" tests/oc_port_test.c

# the UI's input core (parts/oc_ui_input.c), and once more with the chord window off
cc -O2 -std=c99 -Wall -Wextra -Werror -Iparts -o "$OUT/oc_ui_test" tests/oc_ui_test.c parts/oc_ui_input.c
cc -std=c99 -Wall -Wextra -Werror -DUI_CHORD_MS=0u -Iparts -c -o "$OUT/oc_ui_input_nochord.o" parts/oc_ui_input.c
# the UI's screen router (parts/oc_ui_nav.c)
cc -O2 -std=c99 -Wall -Wextra -Werror -Iparts -o "$OUT/oc_ui_nav_test" tests/oc_ui_nav_test.c parts/oc_ui_nav.c
# the UI's drawing kit (parts/oc_ui_gfx.c) and the grid (parts/oc_ui_screen.c):
# the primitives, the two faces and the shape of a screen
cc -O2 -std=c99 -Wall -Wextra -Werror -Iparts -I"$TESSERA/firmware/src" -o "$OUT/oc_ui_gfx_test" tests/oc_ui_gfx_test.c parts/oc_ui_gfx.c parts/oc_ui_screen.c
# OVERVIEW's note name and gate history (parts/oc_ui_overview.c)
cc -O2 -std=c99 -Wall -Wextra -Werror -Iparts -I"$TESSERA/firmware/src" -o "$OUT/oc_ui_overview_test" tests/oc_ui_overview_test.c parts/oc_ui_overview.c parts/oc_ui_gfx.c

[ "$1" = build ] && exit 0

# the baseline is the FM-1's: the port must play exactly what the FM-1 plays
"$OUT/oc_engine_test" "$TESSERA/tests/tessera_baseline.txt" build/oc_log || exit 1
"$OUT/oc_port_test" && "$OUT/oc_ui_test" && "$OUT/oc_ui_nav_test" && "$OUT/oc_ui_overview_test" && exec "$OUT/oc_ui_gfx_test"
