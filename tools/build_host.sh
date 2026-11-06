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

# the port's own unit, the engine's sources (vendor/tessera/firmware/src) and the
# shared test (vendor/tessera/tests): the test finds the port's host shim
# through -Itests
cc -O2 -ffp-contract=off -w \
   -I"$GEN" -Ifirmware -I"$TESSERA/firmware/src" -Itests \
   -o "$OUT/oc_engine_test" tests/oc_engine_test.c -lm

# the port's own test: the module's pins (the o_C's own tables) and the
# encoders' quadrature (the turn the other way must end)
cc -O2 -w -Ifirmware -I"$TESSERA/firmware/src" -o "$OUT/oc_port_test" tests/oc_port_test.c

[ "$1" = build ] && exit 0

# the baseline is the FM-1's: the port must play exactly what the FM-1 plays
"$OUT/oc_engine_test" "$TESSERA/tests/tessera_baseline.txt" build/oc_log || exit 1
exec "$OUT/oc_port_test"
