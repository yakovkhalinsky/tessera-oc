# Changelog

The port's releases. The version is the one the firmware carries
(`firmware/oc_version.h`); the splash on the module's screen shows it.

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
