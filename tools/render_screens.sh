#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Yakov Khalinsky
# The guide's screens (docs/GUIDE.md): build tools/render_screens against the
# firmware's own screen code and the engine, and write the PNGs.
#
#   tools/render_screens.sh [OUTDIR]     (default docs/img)
set -e
cd "$(dirname "$0")/.."
OUT=build/render_screens
SRC=tools/render_screens
INC="-I$SRC -Ibuild/gen -Ifirmware -Ivendor/tessera/firmware/src"

mkdir -p "$OUT"
[ -f build/gen/melodee_tables.h ] || tools/build_host.sh build >/dev/null
cc -O2 -w $INC -c -o "$OUT/rs_engine.o" "$SRC/rs_engine.c"
for f in oc_display oc_ui oc_diag; do
    c++ -O2 -w -DOC_VERSION_EXTRA='"_T40"' -DOC_ENC_REV_PINS_21_22=1 $INC -c -o "$OUT/$f.o" "firmware/$f.cpp"
done
c++ -O2 -Wall -Wextra -DOC_VERSION_EXTRA='"_T40"' -DOC_ENC_REV_PINS_21_22=1 $INC -o "$OUT/render_screens" "$SRC/rs_main.cpp" \
    "$OUT/rs_engine.o" "$OUT/oc_display.o" "$OUT/oc_ui.o" "$OUT/oc_diag.o"
mkdir -p "${1:-docs/img}"
exec "$OUT/render_screens" "${1:-docs/img}"
