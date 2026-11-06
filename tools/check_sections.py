#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The port's memory-map check: no orphan RAM, nothing outside the startup's reach.

The v0.1.0 firmware bootlooped on the module because the engine's .pool (and
its .noinit) were orphans: the Teensy 4.0's own linker script knows neither of
them, so they landed between the copied .data and the cleared .bss, in a gap
that no startup code touches. The engine then ran on whatever the RAM happened
to hold, and the first timer tick faulted.

Nothing in the build notices that: the link succeeds, the sizes look fine, and
the firmware only fails on the module. So the build checks the map itself:

  1. no symbol may sit between _edata and _sbss (the orphan gap);
  2. every allocated RAM section must be one the map knows about (no orphans);
  3. every allocated RAM section must lie in a range the startup reaches: the
     copied .data, the cleared .bss (with the engine's .pool and .noinit in
     it), or the framework's own .bss.dma.

    tools/check_sections.py                        # the default ELF
    tools/check_sections.py .pio/build/T40/firmware.elf

The tools are the ARM binutils' (arm-none-eabi-nm, arm-none-eabi-objdump);
they are in the toolchain PlatformIO installs. Set NM and OBJDUMP to point
elsewhere.
"""
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_ELF = os.path.join(ROOT, ".pio", "build", "T40", "firmware.elf")


def find_tool(name):
    """the ARM binutils' tool: in the path, or in the toolchain PlatformIO
    installed (it is not on the path in CI)"""
    override = os.environ.get(name.upper())
    if override:
        return override
    from shutil import which
    found = which(name)
    if found:
        return found
    pattern = os.path.join(os.path.expanduser("~"), ".platformio", "packages",
                           "toolchain-gccarmnoneeabi-teensy", "bin", name)
    hits = sorted(glob.glob(pattern))
    return hits[0] if hits else name


NM = find_tool("arm-none-eabi-nm")
OBJDUMP = find_tool("arm-none-eabi-objdump")

# the Teensy 4.0's RAM: DTCM (the CPU's) and RAM (the AXI bus)
RAM_RANGES = ((0x20000000, 0x20080000), (0x20200000, 0x20280000))

# the RAM sections the map may hold, and who writes them:
#   .data     the startup copies it from the flash
#   .bss      the startup clears it
#   .pool     the engine's own: the startup clears it with .bss
#   .noinit   the engine's own: the startup clears it with .bss
#   .bss.dma  the framework's own (the USB's buffers and the heap's start)
KNOWN_RAM_SECTIONS = (".data", ".bss", ".pool", ".noinit", ".bss.dma")

# the RAM that the framework's own DMA area holds (not cleared, not copied)
UNMANAGED = (".bss.dma",)


def fail(problems, lines):
    for p in problems:
        lines.append("  FAIL %s" % p)
    sys.stderr.write("\n".join(lines) + "\n")
    sys.exit(1)


def read_elf_symbols(elf):
    """the defined symbols, by address: {name: address}"""
    out = subprocess.run(
        [NM, "-n", "--defined-only", elf],
        check=True, capture_output=True, text=True,
    ).stdout
    symbols = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    return symbols


def read_elf_sections(elf):
    """the sections: [(name, vma, size, has_contents)]"""
    out = subprocess.run(
        [OBJDUMP, "-h", elf],
        check=True, capture_output=True, text=True,
    ).stdout
    sections = []
    lines = out.splitlines()
    for i, line in enumerate(lines):
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+", line)
        if not m:
            continue
        name, size, vma = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
        flags = lines[i + 1] if i + 1 < len(lines) else ""
        sections.append((name, vma, size, "CONTENTS" in flags))
    return sections


def in_ram(addr):
    return any(lo <= addr < hi for lo, hi in RAM_RANGES)


def main():
    elf = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ELF
    if not os.path.isfile(elf):
        sys.stderr.write("no such ELF: %s\n" % elf)
        sys.exit(2)

    symbols = read_elf_symbols(elf)
    sections = read_elf_sections(elf)
    problems = []
    lines = ["the memory map of %s:" % elf]

    for name in ("_edata", "_sbss", "_ebss"):
        if name not in symbols:
            problems.append("the ELF has no %s (is it a linked image?)" % name)
    if problems:
        fail(problems, lines)

    edata, sbss, ebss = symbols["_edata"], symbols["_sbss"], symbols["_ebss"]
    lines.append("  _edata 0x%08x   _sbss 0x%08x   _ebss 0x%08x" % (edata, sbss, ebss))

    # 1. the orphan gap: nothing may sit between the copied data and the
    #    cleared bss
    gap = sorted(
        (a, n) for n, a in symbols.items() if edata < a < sbss
    )
    if gap:
        problems.append(
            "%d symbol(s) between _edata and _sbss (0x%08x..0x%08x): %s"
            % (len(gap), edata, sbss, ", ".join("%s@0x%08x" % (n, a) for a, n in gap[:12]))
        )

    # 2. and 3. the RAM sections: known, and within the startup's reach
    cleared = (sbss, ebss)
    copied = (symbols.get("_sdata", 0), edata)
    for name, vma, size, has_contents in sections:
        if not size or not in_ram(vma):
            continue
        if name not in KNOWN_RAM_SECTIONS:
            problems.append(
                "orphan RAM section %s at 0x%08x (%d bytes): no startup code "
                "initialises it" % (name, vma, size)
            )
            continue
        if name in UNMANAGED:
            continue
        inside = (cleared[0] <= vma and vma + size <= cleared[1]) or (
            copied[0] <= vma and vma + size <= copied[1]
        )
        if not inside:
            problems.append(
                "RAM section %s at 0x%08x (%d bytes) is outside the ranges the "
                "startup writes (the copied data 0x%08x..0x%08x, the cleared bss "
                "0x%08x..0x%08x)"
                % (name, vma, size, copied[0], copied[1], cleared[0], cleared[1])
            )

    if problems:
        lines.append("  the map is not sound:")
        fail(problems, lines)

    for name, vma, size, has_contents in sections:
        if size and in_ram(vma):
            lines.append("  %-10s 0x%08x  %6d bytes  %s"
                         % (name, vma, size, "loaded" if has_contents else "cleared"))
    lines.append("  the map is sound: nothing between _edata and _sbss, "
                 "no orphan RAM")
    print("\n".join(lines))
    sys.exit(0)


if __name__ == "__main__":
    main()
