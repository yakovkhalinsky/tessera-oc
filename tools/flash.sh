#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the firmware and flash it into the module.
#
#   tools/flash.sh
#
# The module's Teensy 4.0 has to be in the bootloader when the loader looks
# for it: press its reset button (the small one on the Teensy, on the board
# under the module's shield) when the loader says it is waiting. The -w below
# waits for the module to turn up rather than giving up at once.
#
# The same .hex goes into the GitHub releases, for flashing with the Teensy
# Loader app (tyuploader) on Windows or macOS, where no toolchain is needed.
set -e
cd "$(dirname "$0")/.."

PIO=${PIO:-../venv/bin/pio}
LOADER=${LOADER:-$HOME/.platformio/packages/tool-teensy/teensy_loader_cli}
HEX=.pio/build/T40/firmware.hex

[ -x "$PIO" ] || PIO=$(command -v pio) || { echo "no PlatformIO"; exit 2; }
[ -x "$LOADER" ] || { echo "no teensy_loader_cli (it comes with PlatformIO's teensy platform)"; exit 2; }

"$PIO" run -e T40

echo "flashing (press the module's reset button if it asks for one)"
"$LOADER" --mcu=TEENSY40 -w -v "$HEX"
