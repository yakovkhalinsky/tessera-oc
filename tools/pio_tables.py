# SPDX-License-Identifier: GPL-3.0-only
"""PlatformIO pre-build: the tables the engine reads.

The engine needs the sample rate and the control block size (FS, CTL) and the
lookup tables beside them. They are generated from the vendored Tessera's
tools, into build/gen, which the build's include path points at.

    extra_scripts = pre:tools/pio_tables.py
"""
import os
import subprocess
import sys

Import("env")  # noqa: F821  (PlatformIO's SCons pre-script)

ROOT = env["PROJECT_DIR"]  # noqa: F821
TESSERA = os.path.join(ROOT, "vendor", "tessera")
GEN = os.path.join(ROOT, "build", "gen")

# the tables the engine reads, and the tool that writes each
TABLES = [
    ("melodee_tables.h", "tools/gen_tables.py"),
    ("x0x_drum_tables.h", "tools/gen_909_tables.py"),
    ("melodee_fm6.h", "tools/gen_fm6_patches.py"),
]


def generate():
    os.makedirs(GEN, exist_ok=True)
    for name, tool in TABLES:
        out = os.path.join(GEN, name)
        if os.path.exists(out):
            continue
        src = os.path.join(TESSERA, tool)
        print("pio_tables: %s" % tool)
        subprocess.run([sys.executable, src, out], check=True)


generate()
