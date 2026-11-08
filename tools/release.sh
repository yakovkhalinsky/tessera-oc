#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the firmware and put it in a GitHub release, so it can be flashed with
# the Teensy Loader app (or the command-line loader) without a toolchain.
#
#   tools/release.sh v0.1.0         build and publish a release
#   tools/release.sh v0.1.0 --dry   build and stage the files, publish nothing
#
# The asset is a .hex: the Teensy Loader app takes it as it is, and so does
# the command-line loader (teensy_loader_cli --mcu=TEENSY40 file.hex).
set -e
cd "$(dirname "$0")/.."
VERSION=${1:-}
DRY=${2:-}
[ -n "$VERSION" ] || { echo "usage: tools/release.sh <version> [--dry]"; exit 2; }

PIO=${PIO:-../venv/bin/pio}
[ -x "$PIO" ] || PIO=$(command -v pio) || { echo "no PlatformIO: run tools/build_host.sh's venv or install it"; exit 2; }

echo "building"
"$PIO" run -e T40

STAGE=build/release
mkdir -p "$STAGE"
cp .pio/build/T40/firmware.hex "$STAGE/tessera-oc-T40.hex"
cp .pio/build/T40/firmware.elf "$STAGE/tessera-oc-T40.elf"     # for a debugger
ls -la "$STAGE"

if [ "$DRY" = "--dry" ]; then
    echo "dry run: nothing published"
    exit 0
fi

cat > "$STAGE/notes.md" <<EOF
Tessera-OC on a Teensy 4.0 (an Ornament & Crime module).

Flash it with the Teensy Loader app: open \`tessera-oc-T40.hex\`, then press
the module's reset button when the app asks for it. Or from a shell:

    teensy_loader_cli --mcu=TEENSY40 -w tessera-oc-T40.hex

What it does: Tessera's generators, song forms, harmony and modulation
matrix, driving the module's four CV outs (1 V an octave, the o_C's own
convention). The module has no digital outs - its four trigger jacks are
inputs - so the gate rides on the CV: a CV carries its pitch while the note
sounds and rests at 0 V otherwise. No audio: the module composes, the rig
plays.

The screen is the same shape on every page: a lit bar at the top with the
page's title cut out of it, six rows of text with the cursor's row lit, and
the page's controls along the bottom in a small face. The left encoder
moves the cursor down the rows, the right one turns the value, and the two
buttons play, stop, restart and change the page.

$VERSION: UNTESTED ON HARDWARE until Yakov confirms it. The changelog is in
the repository (CHANGELOG.md).
EOF

echo "publishing $VERSION"
gh release create "$VERSION" \
    "$STAGE/tessera-oc-T40.hex" "$STAGE/tessera-oc-T40.elf" \
    --repo "${GH_REPO:-yakovkhalinsky/tessera-oc}" \
    --title "Tessera-OC $VERSION" \
    --notes-file "$STAGE/notes.md" \
    || gh release upload "$VERSION" "$STAGE/tessera-oc-T40.hex" "$STAGE/tessera-oc-T40.elf" \
         --repo "${GH_REPO:-yakovkhalinsky/tessera-oc}" --clobber
echo "done: https://github.com/yakovkhalinsky/tessera-oc/releases/tag/$VERSION"
