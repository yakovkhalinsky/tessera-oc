# Tessera-OC

**Tessera's generative engine, ported to the Ornament & Crime module.**

Tessera is generative firmware for the M-Vave FM-1: thirteen generators, song forms
(EVOLVE, FORM, DEVELOP), one harmony for the whole project, and a modulation matrix of
cords that move anything from anything. It is an *instrument*: it makes the sound.

An o_C module — the Ornament & Crime badge and its Eurorack relatives, the ones that
drive a modular rig — is not an instrument. It has four CV outputs, four trigger
outputs, MIDI, a display and two encoders, and **no audio output at all**. It is a
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
| **[Ornament & Crime](https://ornament-and-cri.me/)** — the hardware: the original o_C design by **mxmxmx** (CC BY-NC-SA 4.0) and the O.R.N.8 by **Paul Stoffregen**. The CV, the trigger outs, the OLED, the encoders and the DACs are theirs. |
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
the shared generator test runs against the FM-1's own baseline, **342 checks, none
failing**. The two checks it skips are the ones that read the synth's voices (the pitch
accumulator, ENERGY's drone): they need a synth, and the port has none.

---

## The outputs

The module has four CV outs (a DAC8565, then an OPA2172 buffer) and four digital
outs. There is no MIDI out: the module is a sequencer for the rig, and the rig
reads CV and gates.

| | |
|---|---|
| **CV 1..4** | the tracks' pitches, **1 V an octave** — the o_C's own convention, so the port's CV outs are interchangeable with the module's: 0 V is MIDI 36 (C2, the T4.0's `kOctaveZero = 3`), an octave up is a volt more. The range is the module's own asymmetric Vbias, −3 V … +7 V. |
| **DIGITAL 1..4** | each track's **gate** (up for the note, down at its end), **trigger** (a 5 ms pulse at the onset) or **clock** (the tempo, for a rig the module is the master of). A gate is the default: a gate is what a voice wants, a trigger is what a drum voice wants. |
| **DIGITAL in** | an external clock, so the rig is usually the master and the module follows it. |

Four tracks, four channels, one to one: Tessera is a four-track instrument and the
o_C is a four-channel one, so every track keeps its own key, scale, transpose and
cords, and the harmony (the chord track, QNT CHRD) lands on the outputs as it
should.

Two things the module cannot say, and what happens instead:

- **Velocity has nowhere to go** — no CV spare, no MIDI. An accent is heard in the
  timing, not the level; the gate's *length* is the sequencer's own gate (the
  step's GATE, the LENGTH page) and the cords can move it.
- **The digital outs are 3.3 V logic**, where a Eurorack gate is +5 V. Most gate
  inputs take 3.3 V; the rest want a level shifter.

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
tests/               the port's tests
  oc_engine_test.c     the FM-1's generator test with the engine swapped for the port's
  oc_hostsim.c         the host environment (the port's engine, the block driver, the sinks)
tools/               the build scripts
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
reads and runs the shared generator test against the FM-1's baseline.

```sh
tools/build_host.sh          # build and run
tools/build_host.sh build    # build only
```

The device build (Phase 5) will be a PlatformIO project for the Teensy 4.0.

---

## Licence

GPL-3.0-only. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
