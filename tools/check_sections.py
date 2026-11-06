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
     it), or the framework's own .bss.dma;
  4. _ebss must be 32-aligned and the MPU's stack-overflow trap (the 32 bytes
     at _ebss) must be empty, with the stock padding in front of it - the
     v0.1.1 fault: the trap was misaligned, it covered the tail of the
     engine's .noinit, and the engine's pattern_init() memset into it;
  5. .bss must be NOBITS (it holds no initialised data, so it costs no flash);
  6. the persistent diagnostics word (oc_persist) must be in its own section
     at the end of the AXI RAM (.oc_persist, 0x2027FF40..0x2027FF80), not in
     the cleared .bss and not at the start of the AXI RAM: it has to survive
     the reset;
  7. the stack must have its headroom: _estack - _ebss of at least 64 KiB.

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
#   .bss      the startup clears it (with the engine's .pool and .noinit in it)
#   .pool     the engine's own: a map that still has it as a section of its
#   .noinit   own is caught by the MPU's trap check below, not here
#   .bss.dma  the framework's own (the USB's buffers and the heap's start)
#   .oc_persist  the port's own: the diagnostics word, at a fixed address
KNOWN_RAM_SECTIONS = (".data", ".bss", ".pool", ".noinit", ".bss.dma", ".oc_persist")

# the RAM that the framework's own DMA area holds (not cleared, not copied),
# and the port's own diagnostics word (a fixed address, written by the port)
UNMANAGED = (".bss.dma", ".oc_persist")

# the diagnostics word's own address: the end of the AXI RAM, just below the
# framework's fault info (the startup writes it at 0x2027FF80,
# cores/teensy4/startup.c:635). The start of the AXI RAM is wiped by the boot
# ROM, so a word there does not survive the reset.
PERSIST_LO = 0x2027FF40
PERSIST_HI = 0x2027FF80

# the map's own symbols: they are the addresses, not data in the RAM
MAP_SYMBOLS = frozenset(("_sdata", "_edata", "_sbss", "_ebss", "_estack"))

# the stack's headroom: the stack grows down from _estack, and the MPU traps
# a stack overflow at _ebss. Under 64 KiB the module would fault on a deep
# call, and the fault would look like a random one.
MIN_HEADROOM = 64 * 1024


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

    for name in ("_edata", "_sbss", "_ebss", "_estack"):
        if name not in symbols:
            problems.append("the ELF has no %s (is it a linked image?)" % name)
    if problems:
        fail(problems, lines)

    edata, sbss, ebss = symbols["_edata"], symbols["_sbss"], symbols["_ebss"]
    estack = symbols["_estack"]
    lines.append("  _edata 0x%08x   _sbss 0x%08x   _ebss 0x%08x   _estack 0x%08x"
                 % (edata, sbss, ebss, estack))

    # the data symbols: the ones in the RAM, and not the map's own addresses
    data = {n: a for n, a in symbols.items() if in_ram(a) and n not in MAP_SYMBOLS}

    # 1. the orphan gap: nothing may sit between the copied data and the
    #    cleared bss
    gap = sorted(
        (a, n) for n, a in data.items() if edata < a < sbss
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

    # 4. the MPU's stack-overflow trap: the startup arms a 32-byte NOACCESS
    #    region at &_ebss, and a 32-byte MPU region must start on a 32-byte
    #    boundary. If _ebss is not aligned, the hardware rounds the region
    #    down and the trap covers the RAM behind it - which is where the
    #    engine's .noinit ended, and the engine's pattern_init() memset into
    #    it (the v0.1.1 fault). So: _ebss 32-aligned, the trap empty, and
    #    the stock padding in front of it empty too.
    if ebss % 32:
        problems.append("_ebss (0x%08x) is not 32-aligned: the MPU's stack-overflow trap would cover the RAM behind it" % ebss)
    for lo, hi, what in ((ebss, ebss + 32, "the MPU's trap"), (ebss - 32, ebss, "the padding in front of it")):
        hits = sorted((a, n) for n, a in data.items() if lo <= a < hi)
        if hits:
            problems.append("%d symbol(s) in %s (0x%08x..0x%08x): %s" % (len(hits), what, lo, hi, ", ".join("%s@0x%08x" % (n, a) for a, n in hits[:12])))

    # 5. .bss holds no initialised data: it must cost no flash
    bss = [s for s in sections if s[0] == ".bss"]
    if not bss:
        problems.append("the ELF has no .bss section")
    elif bss[0][3]:
        problems.append(".bss is loaded: it must be NOBITS, for it is cleared, not copied")

    # 6. the diagnostics word: it has to survive the reset, so it must be at
    #    the end of the AXI RAM (.oc_persist), not in the cleared .bss and not
    #    at the start of the AXI RAM (which the boot ROM may wipe)
    persist = symbols.get("oc_persist")
    dma = [s for s in sections if s[0] == ".bss.dma"]
    if persist is None:
        problems.append("the ELF has no oc_persist (the persistent diagnostics word): the build is not the port's")
    elif sbss <= persist < ebss + 32:
        problems.append("oc_persist is at 0x%08x, in the cleared bss: the startup would wipe it at every reset" % persist)
    elif not (PERSIST_LO <= persist < PERSIST_HI):
        problems.append("oc_persist is at 0x%08x, not in 0x%08x..0x%08x (the end of the AXI RAM, below the "
                        "framework's fault info): it must survive the reset" % (persist, PERSIST_LO, PERSIST_HI))
    elif dma and dma[0][1] <= persist < dma[0][1] + dma[0][2]:
        problems.append("oc_persist is at 0x%08x, in .bss.dma: the start of the AXI RAM does not survive the reset"
                        % persist)

    # 7. the stack's headroom: the stack grows down from _estack
    headroom = estack - ebss
    lines.append("  the stack's headroom: %d bytes (0x%08x..0x%08x)" % (headroom, ebss, estack))
    if headroom < MIN_HEADROOM:
        problems.append("the stack has %d bytes between _ebss and _estack: under %d the module would fault on a deep call" % (headroom, MIN_HEADROOM))

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
