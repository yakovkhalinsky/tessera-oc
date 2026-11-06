# Changelog

The port's releases. The version is the one the firmware carries
(`firmware/oc_version.h`); the splash on the module's screen shows it.

## v0.1.2 — UNTESTED ON HARDWARE until Yakov confirms

v0.1.1 fixed the bootloop but faulted in the engine's `pattern_init()`, in a
`memset` at 0x200272A0. This release fixes that, makes the storage read only,
and adds the diagnostics that say where a boot stopped.

### The fault

The Teensy's startup arms a 32-byte NOACCESS region at `&_ebss`, to catch a
stack overflow. An MPU region of 32 bytes must start on a 32-byte boundary,
and `_ebss` was 0x200272BC — so the hardware rounded the region down to
0x200272A0, and the trap covered the tail of the engine's `.noinit` instead
of the empty space behind it. The engine's `pattern_init()` memset its pattern
bank into the trap and the module died.

* `firmware/oc_imxrt1062.ld`: the Teensy's own script now, with two deltas —
  `.bss` is NOLOAD, and the engine's `.pool` and `.noinit` are inputs of
  `.bss` (so the startup clears them and `_ebss` is the aligned end of
  everything). `_ebss` is 0x200272C0, and the hex is the same size.
* `tools/check_sections.py`: the build now checks `_ebss % 32 == 0`, that
  nothing sits in the trap or in the padding in front of it, that `.bss` is
  NOBITS, that the diagnostics word is in the AXI RAM (it has to survive the
  reset), and that the stack has its 64 KiB of headroom.

### The storage is read only

The security review (ZC, issuecomment-6677339063) asks for a fail closed
storage. v0.1.1 mounted the module's LittleFS drive and could write it.

* `firmware/oc_storage.cpp`: the drive mounts (the mount only — it is never
  formatted, not even a blank one) and is read. Its write, its erase and its
  sync all fail. A save is refused before it starts, and the screen says
  `SAVE OFF v0.1.2`. The CI greps the port's own sources for every write
  path (`flash_write`, `lfs_format`, `EEPROM.put`, …) and fails the build if
  one appears.

### The diagnostics

A module on a rack has no serial cable and no log, so it has to say what
happened to itself, on its own screen, and across a reset.

* `firmware/oc_diag.cpp`: a persistent stage word in the AXI RAM (which the
  startup does not clear), the boot's stages (10 to 23), the loop's own steps
  (30 to 34) and the tick's (50, then 99 when the module is running). The
  stage is mirrored to the framework's crash breadcrumbs.
* **The crash report is on the screen**, a page at a time, with the summary
  first (the stage it died at, the CFSR, the faulting address, the PC, the
  reset status and the tick's worst time). The bottom button goes on; there
  is no timeout, so a module in a reboot loop stops on a screen that says what
  happened.
* **The safe mode**: the bottom button held at power-on. The calibration, the
  DAC at 0 V and the display, and nothing else — no storage, no engine, no
  tick, no MIDI.
* **The timing**: the DWT's cycle counter, the longest tick and the longest
  block, in microseconds.
* **The watchdog**: WDOG1, 16 s, fed in the loop. A module that hangs resets
  itself instead of sitting there.

### The other fixes

* **The boot order** (the o_C's own): the DAC's reset is driven high and left
  there (no low pulse), then the calibration, then the CV outs at 0 V, then
  the display, then the engine, and the tick last, at the priority the o_C's
  own firmware uses (80).
* **The flipped controls**: the module's calibration carries the flags, and
  the FLIPCONTROLS bit says the module's controls are mirrored. The pinout
  and the DAC's channels follow it (A↔D, B↔C), and the FLIPSCREEN bit says
  the screen is the other way round. The remap may only touch the inputs: a
  static assert says so.
* **The display's column offset** comes from the calibration, not from a
  hard-coded 2.
* **The encoders.** A turn the other way never ended (`while (steps--)` with
  a negative count), and the right encoder's direction was tested after the
  decrement, so it turned the wrong way. The left encoder moves the row both
  ways now, and the right one turns the way the encoder turned.

### Known

* The firmware has not been on the module yet: **v0.1.2 is untested on
  hardware until Yakov confirms it.**
* The drive's geometry is the o_C's own (4 KiB blocks, 128 of them, at
  0x60170000). If the module's LittleFS was made with a different geometry,
  the mount fails and the storage stays off — which is the safe way round.
* Part C of the brief (the display's page-at-a-time scheme over the SPI
  interrupt) was not done: it does not fit the line budget, and it is a
  performance change, not a fix. It is v0.1.3's work.

## v0.1.1 — UNTESTED ON HARDWARE until Yakov confirms

v0.1.0 bootlooped on the module with a blank screen. This release fixes that,
and the other faults the port had.

### The bootloop

The engine keeps its big buffers in two sections of its own: `.pool` (the
FM-1's zero-initialised working buffers) and `.noinit`. The Teensy 4.0's
linker script knows neither of them, so the linker orphaned them: they landed
between the copied `.data` and the cleared `.bss`, in a gap that no startup
code touches. Nothing ever wrote them, so the engine started on whatever the
RAM happened to hold, and the first 60 us tick read it and faulted.

* `firmware/oc_imxrt1062.ld`: the port's own memory map (the Teensy's, with
  `.pool` and `.noinit` inside the cleared `.bss`). The startup zeroes them
  before `setup()` runs.
* `tools/check_sections.py`: the build now checks the map: no symbol between
  `_edata` and `_sbss`, no orphan RAM section, nothing outside the ranges the
  startup writes. It fails on the v0.1.0 firmware and passes on this one. It
  runs in CI (`python3 tools/check_sections.py`) and locally.

### The other faults

* **The trigger inputs are inputs.** `TR1..TR4` are the o_C's trigger *inputs*
  (the schematic has no digital out, and the o_C's own firmware reads them).
  v0.1.0 drove them as outputs. They are now inputs, pulled up, and a trigger
  is a falling edge, as the o_C's hardware and firmware have it.
* **No digital outs: the gate rides on the CV.** A track's CV carries its pitch
  while the track's out is asserted (the gate up, the trigger's pulse, the
  clock's pulse) and rests at the module's 0 V reference otherwise. The rig
  reads the gate off the CV (a comparator, an envelope follower).
* **The DAC writes a 24-bit frame**, the DAC8565's own frame (the command byte
  and the 16 data bits), not two 16-bit ones.
* **The calibration is read, properly.** v0.1.0 looked for the wrong fourcc
  ('CAL1' is 0x43414C01, not 0x314C4143), never checked the checksum and never
  read the per-channel tables, so every module fell back to the straight line.
  The newest page that checksums is read now, per channel, and the EEPROM is
  never written.
* **The straight line is straight.** The fallback (a module with no
  calibration) put MIDI 36 at −3 V instead of 0 V: three octaves too low.
* **The critical section is a critical section.** `oc_irq_off`/`oc_irq_on` were
  empty, so the engine's knob handlers could be interrupted halfway. They are
  the ARM's own `cpsid i`/`cpsie i` now.
* **The DAC never writes over the display.** The DAC is written from the tick
  (the interrupt) and the OLED from the main loop, on the same SPI bus: the
  interrupt's write waits for the display (the main loop applies it) instead of
  interleaving with it.
* **Three logic bugs.** `!oc_load` was inverted (it composed instead of
  loading), `oc_playing` and `oc_clock_pulse` were tested as function pointers
  (always true), so the transport only ever stopped and the clock's pulse never
  ended.
* **The module says something.** The crash report goes out over the USB serial
  at every boot (the USB descriptor is now MIDI *and* serial), and the screen
  shows a splash before the engine's init.

### Known

* The firmware has not been on the module yet: **v0.1.1 is untested on
  hardware until Yakov confirms it.**
* The DAC8565's SPI mode is the one the o_C's own firmware uses for the same
  chip on the same bus (the display's, mode 0); it is one constant
  (`OC_DAC_SPI_MODE` in `firmware/oc_platform.cpp`) if a module wants the
  other.
* `TR2..TR4` are read but unused: the port has no trigger input path yet.

## v0.1.0

The first release: the engine, the platform layer, the UI and the build.
It never booted on the module (the orphaned `.pool`).
