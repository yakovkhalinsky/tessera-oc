# Changelog

The port's releases. The version is the one the firmware carries
(`firmware/oc_version.h`); the splash on the module's screen shows it.

## Unreleased

### UI, phase 2: the drawing kit, the 3x5 face, the grid

- `parts/oc_ui_gfx.c`: the drawing kit, pure C and host-testable. It draws
  into a framebuffer the caller owns, in the panel's own layout, so the
  display's buffer is handed over as it is and nothing is converted. The
  primitives: `px`, `hline`, `vline`, `fill_rect`, `invert_rect` (a rectangle
  flipped), a clip rectangle, `text5x7`, `text3x5`, `text5x7_x2` (twice the
  size, for the big numbers of phase 3), `text_right` (a string ending at a
  column) and `ellipsize` (a string cut to fit, with a dot for the rest).
- `parts/oc_ui_font3x5.h`: a new 3x5 face for the footer, drawn for this
  port: the space, the digits, A to Z and `+ - . : / > ~`. Four pixels a glyph,
  so 32 columns fit across the panel.
- `parts/oc_ui_screen.c`: the grid every screen obeys. The header (the top
  nine rows) is a lit bar with the title's letters cut out of it; the body
  (47 rows) is six rows of text, the cursor's row lit with its letters cut
  out of it; the footer (eight rows) is the screen's hints in the 3x5 face.
  A row's label is on the left and its value on the right.
- `firmware/oc_display.cpp`: the SPI side and the panel's pixels, and
  nothing else. The drawing is the kit's. The crash pager and the safe mode
  draw with the plain text of the 5x7 face, and they come out byte for byte
  as before.
- `firmware/oc_ui.cpp`: the shell draws the grid. The screens themselves are
  unchanged in what they say: OVERVIEW still lists the tempo, the four tracks
  and the clock; the screen menu still lists the five screens; INFO still
  reports the version and the timings.
- `tests/golden/`: every screen as text, 64 lines of 128 characters. A picture
  cannot be diffed; text can, so a reviewer sees exactly which pixels a
  change moved. `tools/build_host.sh` checks them the same way it checks the
  PNGs, and `UI_GOLDEN_UPDATE=1` rewrites them.
- `tests/oc_ui_gfx_test.c`: the kit's own test: the primitives, the clip, the
  two faces, the right-aligned text and the ellipsis.

### Docs: the panel and controls guide
- `docs/GUIDE.md` (linked from the README): the uO_C rev A panel (`docs/img/panel.svg`), the controls on each screen as they are on `main`, the power-on, the safe mode and the crash pager. The screen pictures (`docs/img/screen-*.png`) are drawn by the firmware's own screen code on the host (`tools/render_screens.sh`, `tools/render_screens/`), and CI fails if they are stale (a screen that differs, is missing or is left over; the check is skipped, with a message, off Linux).
- The panel drawing and the controls tables follow the real panel: the LEFT and RIGHT buttons side by side under the display, above the encoders. The code calls them TOP and BOTTOM; LEFT is play / stop as calibrated (the FLIPCONTROLS flag picks the pinout).

### UI, phase 1a: the input core (no change on the module yet)
- `parts/oc_ui_input.c`: the buttons and encoders as events (5 ms debounce, tap ≤ 400 ms, hold from 400 ms, long at 600 ms, BOTTOM as shift (each event says whether BOTTOM is held), TOP + BOTTOM chord within `UI_CHORD_MS` = 40 ms either way round, boot-latched buttons ignored until released, value acceleration) and the 1 kHz sampler's ring. Host-tested in `tests/oc_ui_test.c`; not wired into the firmware until phase 1b. Plan: `docs/UI-PLAN.md`.

### UI, phase 1c: the shell
- `firmware/oc_ui.cpp` is the UI's shell. A 1 kHz sampler (an IntervalTimer of its own, priority 160, below the tick's 80; started in normal mode only, next to the tick) reads the four buttons and the two encoders into the input core's ring; `ui_service()` in `loop()` drains it through the input core (`parts/oc_ui_input.c`) into the screen router (`parts/oc_ui_nav.c`) and applies the actions: play / stop, restart, the track, the tempo (not while following TR1), the value (with the acceleration), enter, the default.
- The screens: OVERVIEW (the old HOME; the cursor on a `TRACK` row selects it, an R tap opens TRACK), TRACK with the GEN, SCL, CV and MOD tabs, PERFORM (the four tracks; its controls come in phase 5), PROJECT (load only, the storage is read only), INFO (the version, the longest engine block, the draw, flush and loop timings, the storage, the dropped events) and the screen menu (an L long press). The footers name the controls each screen answers.
- `firmware/oc_platform.cpp`: the old button edges are gone (the shell has the buttons now); the safe mode and the crash pager read the buttons as before. `firmware/oc_ui_parts.c` builds `parts/` into the firmware; `ui_nav_open()` is new in the router (host-tested).
- `docs/GUIDE.md` describes the new controls and screens, with five new pictures (`screen-menu`, `screen-track`, `screen-perform`, `screen-project`, `screen-info`).

### UI, phase 1b: the screen router (no change on the module yet)
- `parts/oc_ui_nav.c`: the screen stack (four deep), the cursors, the screen menu, TRACK's tabs and the global controls (BOTTOM + L selects the track and BOTTOM + R sets the tempo, except on PERFORM where it sets the EVOLVE amount, TOP plays and stops, a BOTTOM tap restarts, TOP + BOTTOM opens and closes PERFORM); it changes nothing in the engine, it tells the shell what to do. The L encoder moves the cursor and never changes the track.
- `parts/oc_ui_input.c`: a button's tap, hold and long carry the shift as it was when the button went down (BOTTOM released first no longer drops it). Host-tested in `tests/oc_ui_test.c` and `tests/oc_ui_nav_test.c`; stacked on phase 1a, wired into the firmware in phase 1c.

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
* **The drive's geometry is the framework's own** on the T4.0
  (framework-arduinoteensy 1.162.0, `LittleFS.cpp:911-957`): `FLASH_SIZE`
  0x1F0000, `SECTOR_SIZE` 32768, the base at `0x60000000 + 0x1F0000 - 512K` =
  0x60170000, the blocks 32768 bytes and **16** of them, the read, the program
  and the cache 128, the block cycles 800. The o_C's own firmware mounts the
  same drive the same way (`PhzConfig.cpp:26`: `1024 * 512`), so the port
  reads the filesystem the module's own firmware wrote — the v0.1.1 geometry
  (4 KiB blocks, 128 of them) could not mount it. Two static asserts hold the
  geometry: the drive is 512 KiB, and its base is 0x60170000.

### The diagnostics

A module on a rack has no serial cable and no log, so it has to say what
happened to itself, on its own screen, and across a reset.

* `firmware/oc_diag.cpp`: a persistent stage word in the AXI RAM (which the
  startup does not clear), the boot's stages (10 to 23) and 99 when the module
  is running. The stage is mirrored to the framework's crash breadcrumbs.
* **The crash report is on the screen**, a page at a time: the framework's
  own report (it has the fault, the CFSR, the faulting address, the PC and
  the breadcrumbs), captured into a buffer of our own because the framework's
  print clears the report as it prints it, with the stage the last run died
  at first. The top button is the next page and the bottom one goes on;
  there is no timeout, so a module in a reboot loop stops on a screen that
  says what happened.
* **The safe mode**: a button held at power-on. Either of the two buttons
  that can be the bottom one (pins 4 and 5) is accepted, so the module's
  orientation does not matter: the flag that says which is which is in the
  calibration, and the calibration is read after the pins. The calibration,
  the DAC at 0 V and the display, and nothing else — no storage, no engine,
  no tick, no MIDI.
* **The timing**: the DWT's cycle counter, the longest block, in
  microseconds, on the safe mode's screen.

### The other fixes

* **The boot order** (the o_C's own): the DAC's reset is driven high and left
  there (no low pulse), then the calibration, then the CV outs at 0 V, then
  the display, then the engine, and the tick last, at the priority the o_C's
  own firmware uses (80).
* **The pin map was the wrong way round**, and the module's buttons and
  encoders were on each other's pins. The tables are the o_C's own firmware's
  (Phazerville PSv2.0.2 `OC_gpio.cpp`: the default orientation at `:128-136`,
  the flipped one at `:82-94`):

  | | top | bottom | left enc. | left btn. | right enc. | right btn. | the rig's clock |
  |---|---|---|---|---|---|---|---|
  | the default | 5 | 4 | 22, 21 | 23 | 16, 15 | 14 | TR1 (pin 0) |
  | the flipped | 4 | 5 | 15, 16 | 14 | 21, 22 | 23 | TR1 (pin 3) |

  The flip is the calibration's FLIPCONTROLS bit: the o_C's own
  `CALIBRATION_FLAG_FLIPCONTROLS` is `1u << 3` (`OC_calibration.h:130`), and
  `Main.cpp:349` hands it to `OC::SetFlipMode`, which swaps the pins exactly
  as the tables above do. The pinout and the DAC's channels follow it
  (A↔D, B↔C), and the FLIPSCREEN bit says the screen is the other way
  round. The remap may only touch the inputs: `tests/oc_port_test.c` says so
  (the OLED's pins 6–8, the DAC's 9–10 and the SPI's are the module's own).
* **The display's column offset** comes from the calibration, not from a
  hard-coded 2.
* **The encoders.** A turn the other way never ended (`while (steps--)` with
  a negative count), and the right encoder's direction was tested after the
  decrement, so it turned the wrong way. The left encoder moves the row both
  ways now, and the right one turns the way the encoder turned. The
  quadrature and the row it moves are in `oc_port.h` (portable on purpose),
  and `tests/oc_port_test.c` drives them: an anticlockwise turn is a negative
  count, it ends, and it goes the other way — on both encoders, in both
  orientations.

### Known

* The firmware has not been on the module yet: **v0.1.2 is untested on
  hardware until Yakov confirms it.**
* If the module's LittleFS was made with a different geometry, the mount
  fails and the storage stays off — which is the safe way round. The
  geometry is the framework's own (32 KiB blocks, 16 of them, at
  0x60170000), the same one the o_C's own firmware uses.
* Part C of the brief (the display's page-at-a-time scheme over the SPI
  interrupt) was not done: it does not fit the line budget, and it is a
  performance change, not a fix. It is v0.1.3's work.
* **The watchdog** (WDOG1, 16 s, fed in the loop) is not in this release: it
  is not cross-checked against the o_C's own firmware yet. It is v0.1.3's
  work.
* The loop's own steps (the stages 30 to 34) are not in this release either:
  they cost lines and only say something if the module hangs, which is the
  watchdog's business. The boot's stages (10 to 23) are what is left, and
  99 says the module is running.

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
