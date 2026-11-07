# Tessera-OC

**Tessera's generative engine, ported to the Ornament & Crime module.**

Tessera is generative firmware for the M-Vave FM-1: thirteen generators, song forms
(EVOLVE, FORM, DEVELOP), one harmony for the whole project, and a modulation matrix of
cords that move anything from anything. It is an *instrument*: it makes the sound.

An o_C module — the Ornament & Crime badge and its Eurorack relatives, the ones that
drive a modular rig — is not an instrument. It has four CV outputs, four trigger
*inputs*, MIDI, a display and two encoders, and **no audio output at all**. It is a
sequencer for the rig, not a voice in it.

So Tessera-OC is Tessera's *generative half* on that hardware: the generators, the
harmony, the cords, the steps and the song forms stay, and the synths, the mixer, the
effects and the USB audio go. The module composes; your modular rig plays.

The music does not change: **the port plays exactly what the FM-1 plays** — the same
scenarios, the same note log, the same hash, checked against the FM-1's own baseline
(`vendor/tessera/tests/tessera_baseline.txt`). See [Status](#status).

---

## Credits

This project is a derivative work and stands on the shoulders of several people. They
are credited here and in [NOTICE](NOTICE); nothing here is my own invention.

| | |
|---|---|
| **[Tessera](https://github.com/Rastapopolix/tessera)** — the generative engine: the generators, EVOLVE and FORM, the harmony, the cords, the steps. **Copyright (C) 2026 Theo Dainis** ([Rastapopolix](https://github.com/Rastapopolix)), GPL-3.0-only. Vendored as a submodule (`vendor/tessera`), pinned to a branch that splits the engine from the audio path. |
| **[Melodee](https://github.com/keremimo/melodee)** — the FM-1 firmware Tessera grew out of: the sequencer, the parameter registry, the modulation matrix, the project format. **Copyright (C) 2026 Kerem Kilic (Ellic Studio)**, GPL-3.0-only. |
| **[Felucca](https://github.com/…)** — the upstream of Melodee. **Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments**, GPL-3.0-only. |
| **[Ornament & Crime](https://ornament-and-cri.me/)** — the hardware: the original o_C design by **mxmxmx** (CC BY-NC-SA 4.0) and the O.R.N.8 by **Paul Stoffregen**. The CV, the trigger ins, the OLED, the encoders and the DACs are theirs. |
| **[O_C-Phazerville](https://github.com/djphazer/O_C-Phazerville)** (DJ Phazer) — the o_C module firmware this port takes the platform conventions from, and the reference for the T4.0's pinout, the clock and the UI idiom. MIT, with GPLv3 parts. |
| **Mutable Instruments** — the Plaits synthesis Tessera's MACRO engine came from, and **DaisySP** — the physical models. MIT / Apache-2.0. |
| **[Chakra Petch](https://fonts.google.com/specimen/Chakra+Petch)** — the type. OFL. |

The port itself (the `firmware/` directory and the tests) is GPL-3.0-only, so that the
whole work stays under the licence of the engine it carries.

---

## Status

| Phase | What | State |
|---|---|---|
| **0** | The survey and the plan ([PORT-PLAN.md](PORT-PLAN.md)) | done |
| **1** | The engine without an audio path: the time base, the LFO and the engine constants split out of the audio files, and the o_C engine built on them | **done** |
| **2** | The platform layer: the module's tick, the CV and gate outputs, the encoders, the buttons, the storage | **done** |
| **3** | The output mapping: the notes to CV, gates and triggers; the clock out; the modulation matrix on the CV | **done** |
| **4** | The UI, in the o_C idiom: two encoders, four buttons, a 128×64 display | **done** (a first pass) |
| **5** | Packaging: the PlatformIO build, the flash, the rig | the build is done; the rig is untested |

Phase 1's acceptance criterion was that the port plays what the FM-1 plays. It does:
the shared generator test runs against the FM-1's own baseline, **375 checks, none
failing** — 342 in the shared test and 33 in the port's own (the module's pin map and
the encoders' quadrature). The two checks it skips are the ones that read the synth's
voices (the pitch accumulator, ENERGY's drone): they need a synth, and the port has none.

The current release is **v0.1.2** ([CHANGELOG.md](CHANGELOG.md)): the v0.1.0
firmware bootlooped on the module (the engine's `.pool` was an orphan section
that nothing ever wrote), v0.1.1 fixed that but faulted in the engine's
`pattern_init()` (the MPU's stack-overflow trap was misaligned and covered the
engine's RAM), and this one fixes that, makes the storage read only, and adds
the diagnostics that say where a boot stopped. **v0.1.2 is untested on
hardware until Yakov confirms it.**

---

## The outputs

The module has four CV outs (a DAC8565, then an OPA2172 buffer) and **no digital
outs**: its four trigger jacks (TR1..TR4) are inputs, as the schematic and the
o_C's own firmware have them. There is no MIDI out either: the module is a
sequencer for the rig, and the rig reads CV.

| | |
|---|---|
| **CV 1..4** | the tracks' pitches, **1 V an octave** — the o_C's own convention, so the port's CV outs are interchangeable with the module's: 0 V is MIDI 36 (C2, the T4.0's `kOctaveZero = 3`), an octave up is a volt more. The range is the module's own asymmetric Vbias, −3 V … +7 V, and the codes come from the module's own calibration (its EEPROM, per channel, per octave). |
| **the gate on the CV** | the module has no digital outs, so the gate rides on the same CV: a track's CV carries its pitch while the track's out is asserted (the **gate** up for the note, a 5 ms **trigger** pulse at its onset, the **clock**'s pulse) and rests at the module's 0 V reference otherwise. The rig reads the gate off the CV — a comparator, an envelope follower — and the pitch off the same wire. |
| **TR1..TR4** | the trigger *inputs*, read and never driven: TR1 takes the rig's clock (a trigger pulls the pin down, so the pulse is the falling edge), TR2..TR4 are read but unused. |

Four tracks, four channels, one to one: Tessera is a four-track instrument and the
o_C is a four-channel one, so every track keeps its own key, scale, transpose and
cords, and the harmony (the chord track, QNT CHRD) lands on the outputs as it
should.

Two things the module cannot say, and what happens instead:

- **Velocity has nowhere to go** — no CV spare, no MIDI. An accent is heard in the
  timing, not the level; the gate's *length* is the sequencer's own gate (the
  step's GATE, the LENGTH page) and the cords can move it.
- **There is no gate jack**, only the CV. The gate is the CV's own shape: it is at
  the note's pitch while the note sounds, and back at 0 V when it ends.

The pitch scale's other setting is 0.1 V an octave, for V/oct systems.

---

## Layout

```
firmware/            the port: the o_C engine
  oc_engine.c          the unity unit: the sequencer and the generators, no audio path
  oc_engines.c         the engine table: the port's own engine (CV and gate), the rest retired
  oc_voice.c           the note layer: there are no voices, the note goes to the rig
  oc_out.c             the output: the CV out's scale, the gate, the MIDI out
  oc_midi.c            the MIDI queue (the module's USB MIDI)
  oc_state.c           the input, the pitch bend, the inert FM-1 engine state
  oc_hal.h             the platform names the shared engine calls
  oc_platform.cpp      the module: the pins, the CV out, the calibration, the tick
  oc_display.cpp       the OLED (the SH1106, over the DAC's SPI bus)
  oc_ui.cpp            the screen and the controls
  oc_storage.cpp       the module's own flash (the projects, four slots)
  oc_port.h            the engine's and the platform's interface
  oc_spi.h             the SPI bus the DAC and the OLED share
  oc_version.h         the release the firmware carries
  oc_imxrt1062.ld      the port's memory map (the engine's .pool and .noinit in .bss)
parts/               the UI's pure-C parts (host-tested; not on the module yet)
  oc_ui_input.c        the buttons and the encoders as events
  oc_ui_nav.c          the screen router: the stack, the cursors, the control map
tests/               the port's tests
  oc_engine_test.c     the FM-1's generator test with the engine swapped for the port's
  oc_hostsim.c         the host environment (the port's engine, the block driver, the sinks)
  oc_port_test.c       the port's own: the module's pin map and the encoders' quadrature
  oc_ui_test.c         the UI's input core
  oc_ui_nav_test.c     the UI's screen router
tools/               the build scripts
  build_host.sh        the host build and the generator test
  check_sections.py    the memory map's check (no orphan RAM, nothing unwritten)
  release.sh           the firmware, the hex and the release notes
vendor/tessera/      the engine, as a submodule (a fork of Rastapopolix/tessera)
```

The engine is a **submodule**, pinned to `yakovkhalinsky/tessera` at the branch
`port/engine-split`: the FM-1's firmware with the engine split out of the audio files
(the time base into `seqtime.c`, the LFO and the voice mode into `seq_lfo.c`, the
engines' constants into headers), so a target without an audio path can use it. Those
changes are small and upstreamable; the port itself is not.

To update the engine:

```sh
git submodule update --remote vendor/tessera
git -C vendor/tessera fetch origin && git -C vendor/tessera merge origin/main
```

---

## Build and test

The host build needs only a C compiler and Python: it generates the tables the engine
reads, runs the shared generator test against the FM-1's baseline, and runs the port's
own test (the module's pin map and the encoders' quadrature).

```sh
tools/build_host.sh          # build and run
tools/build_host.sh build    # build only
```

The device build (Phase 5) is a PlatformIO project for the Teensy 4.0:

```sh
pio run -e T40                        # build
python3 tools/check_sections.py       # the memory map: no orphan RAM, nothing unwritten
```

The last one is the check that would have caught the v0.1.0 bootloop: the engine's
`.pool` was an orphan section (the Teensy's linker script knows nothing of it), so
nothing ever wrote it, and the engine ran on garbage. The port's own linker script
(`firmware/oc_imxrt1062.ld`) puts it in the range the startup clears, and the check
makes sure it stays there. It also checks the MPU's stack-overflow trap (the v0.1.1
fault: `_ebss` was not 32-aligned, so the trap covered the engine's RAM), the
persistent diagnostics word, and the stack's headroom.

### Flashing

> **⚠️ A flash erases the module's settings and its presets.**
>
> A Teensy Loader upload erases the program flash outside the EEPROM emulation.
> The o_C's own firmware keeps its settings and its presets in that flash (a
> LittleFS drive at the end of it), so **they are gone after a flash**. Export
> them over USB first, and put them back afterwards.
>
> The **calibration survives**: it lives in the EEPROM, and the flash does not
> touch the EEPROM.
>
> This release's own firmware never writes the flash: the storage is read only
> (the projects are read, never written), so flashing is the only thing that
> changes it.

```sh
pio run -e T40 -t upload          # flash it (the module must be in normal mode)
```

---

## Licence

GPL-3.0-only. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
