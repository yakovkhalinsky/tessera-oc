# Tessera → Ornament & Crime (Teensy 4.0) port plan

**Target:** a standalone Tessera firmware for an o_C-format module (Teensy 4.0), replacing
Phazerville. **Sound source:** the module's CV and MIDI outputs, driving the user's modular rig.
No audio synthesis on the module itself.

**Date:** 2026-11-06 · **Status:** planned, Phase 1 starting.

---

## 1. What the target hardware actually is

The user has a *generic o_C module* (Eurorack form factor, mxmxmx o_C shield) with a Teensy 4.0,
driving a modular synth. Confirmed from the schematic (`oc-phazerville/hardware/o_c_rev2e_schematic.pdf`)
and the firmware's own pin detection (`software/src/OC_gpio.cpp`):

| Function | o_C (T3.2 / **T4.0**) | O.R.N.8 (T4.1) |
|---|---|---|
| Audio out | **none** | 2ch I2S2 |
| CV out | 4× (DAC8565 + OPA2172, ±10 V, 3.5 mm jacks) | 8× |
| CV / trigger in | 4× analog, 4× digital (TR1..TR4) | 8× / 8× |
| Display | SH1106 OLED **128×64** (some revs 128×32) | 128×64 |
| Controls | **2 encoders + 4 buttons** (SW_L, SW_R, SW_T, SW_B) | 2 encoders + 6 buttons |
| Other | MIDI (DIN + USB), microSD (shared SPI), USB host | + serial MIDI |

Consequences that shape the whole port:

* **No audio.** The synth engines (FM6, ORACLE, TRIO, 808/909), the FX layer, the mixer, the
  USB-audio device and the 44.1 kHz DMA path are all **dropped**. That is roughly 35–40 k of
  Tessera's 60 k lines, and it removes the hardest constraint (the audio ISR and its deadlines).
* **The 240×240 RGB565 TFT UI cannot be ported** (see §3). The UI is rewritten in the o_C idiom.
* **The trigger outputs are 3.3 V logic**, not ±5 V Eurorack gates — a caveat for the CV/gate
  design (§5, open question Q2).

License: O_C-Phazerville is "generally MIT licensed, however some GPLv3 bits included, so the
whole thing is also subject to compliance with the GPL" → GPL-3.0-only compatible with Tessera.

---

## 2. What is kept, and why it is the interesting part

Tessera's generative core has essentially **no hardware coupling** (audit of `firmware/hal/`
references per file):

```
gen.c 0 · mod.c 0 · cord.c 0 · params.c 0 · engines.c 0 · mix.c 0
pattern.c 0 · song_chain.c 0 · step_evolve.c 0 · tessera.c 3 · seq.c 15 · fx.c 11
```

and `tests/hostsim.c` already proves the whole engine compiles and runs on the host. Kept:

* **13 generators** — TURING, EUCLID, MARKOV, MORPH, ATLAS, BOUNCE, AWAKE, POLY, QUAD,
  CONSTELLATION, SERIES, CHORD, ANSWER.
* **EVOLVE / FORM / DEVELOP / TWEAK / GRAVITY / PIECE** — the mutation and song-form layer.
* **Harmony** — one key and scale for the project, the CHORD track, QNT CHRD (the others follow).
* **The cord matrix** — 32 cords, any source to any destination, the organic sources (TIDE,
  SWARM), the live marks, MATRIX.
* **Steps** — ratchets, conditions (1:2..8:8, FILL, FIRST), the pitch accumulator, Euclidean sheets.
* **The project model** — TSP1 chunk format, 4 project slots, 64 user-preset slots.
* **MIDI out of every track and generator**, each on a channel of its own, plus **CLOCK OUT** —
  Tessera already has a complete MIDI output path (`midi_out.c`), and it is *already* called
  before the synth (`voice.c:531` → `midi_out_on`).

Dropped: `engines.c`, `voice.c`'s polyphonic machinery, `mix.c`'s audio path, `fx.c`, `eng_*.c`,
`phys_dsp.c`, `x0x/`, `third_party/mi/` (13 k lines), `usb_audio*.c`, the OLED/LED HAL, the
bootloader and the installer.

---

## 3. The UI is a rewrite, not a port

The FM-1's display is a **240×240 RGB565 ST7789V TFT** (`lcd.c:3`), not a monochrome OLED — the
"one ink on black" look in the README is a *software palette*. The UI around it assumes:

* 7 rotary encoders + 14 buttons + **27 musical note keys** (`panel.c:9-15`) — the note keys have
  no analogue on the o_C at all;
* a **4-knob-column** page model — `page_t.id[4]` (`params.c:511`), 86 pages in 11 families;
* a **240×120 cell** Mono look with ~25 generator-specific pictures (`ui_mono.c`), each drawn in
  2×2-pixel cells;
* quick-layer modifiers (`ui_layer.c`, 768 lines) built on held buttons.

The o_C offers 2 encoders + 4 buttons and 128×64 (or 128×32) of 1-bpp display. None of that
survives translation. The UI is therefore written from scratch against the o_C's own menu idiom
(the Hemisphere/Phazerville framework: `weegfx`, `ui_encoder`, `ui_button`, `ui_event_queue`),
which is proven on exactly this hardware.

**One clean asset survives: the 5×7 bitmap font** (`gfx.c:982`, `MO_FONT[93][7]`, 651 bytes, 1 bpp,
integer-scalable). It is reusable as-is; only the geometry around it changes.

---

## 4. Time: the virtual-sample clock

Tessera's sequencer counts in **audio samples at 44.1 kHz** (`FS`), with the control-rate
modulation sources running once per 32-sample block (`CTL`). `mix_block()` (`mix.c:269`) is the
per-block entry point, and it is the only thing the audio path drives:

```c
mix_block(out, n) {
    events_block(n);   /* note events */
    cord_tick();       /* modulation sources, DRIFT's clock */
    ...mixing, FX, master...
}
```

The o_C's core ISR runs at **16 666 Hz** (60 µs, `OC_CORE_ISR_FREQ`), and the CV outputs update
there, so 60 µs is the output resolution.

**Decision: keep `FS = 44100` as a *virtual* rate and drive it from the o_C tick with a fractional
accumulator.** One 60 µs tick advances the virtual clock by `44100 / 16666 = 2.646` samples; the
accumulator keeps the remainder, so the drift is bounded by one sample forever. Every existing
time semantic in the engine — humanize delays in ms, note lengths, LFO and organic-source rates,
the MIDI-clock tempo estimate — stays **numerically identical** to the FM-1 build, because the
numbers never change; only the thing that advances them does.

The block cadence is reproduced too: the accumulator crossing a 32-virtual-sample boundary runs one
control block (`events_block` + `cord_tick`), giving the same 1378.125 blocks/s the engine sees now.

Consequence: **the OC build must play exactly the notes the FM-1 build plays.** That is the
acceptance criterion for Phase 1 (§6), and `tests/tessera_baseline.txt` already holds the expected
note list to compare against.

---

## 5. Output design (the part that needs the user's taste)

### 5.1 Track → output mapping

Tessera has 4 tracks and the module has 4 CV outputs and 4 trigger outputs. The obvious mapping:

```
track 1..4  →  CV1..CV4   (pitch, 1 V/oct by default)
track 1..4  →  TRIG1..4   (gate)
track 1..4  →  MIDI ch 1..4
generators  →  MIDI ch 5..   (their own channel each, as today)
```

### 5.2 Open questions (to settle with the user)

* **Q1 — voices vs. tracks.** A modular wants more than four voices, but there are only four CV
  outputs. Options: (a) 4 tracks → 4 CVs, polyphony only over MIDI; (b) a "voice pool" mode where
  the 4 CVs are the *loudest* voices and MIDI carries all of them; (c) CVs carry *roles* (pitch,
  gate, accent, mod) instead of tracks.
* **Q2 — gate levels.** The TR outputs are 3.3 V logic, not ±5 V gates. Options: use them anyway
  (most Eurorack gate inputs are 3.5 V-threshold), or drive gates from the CV outputs (0–10 V),
  which costs a CV channel.
* **Q3 — pitch scaling.** 1 V/oct (Eurorack default) vs 0.1 V/oct (V/oct systems) vs fixed
  semitone steps. Make it a setting.

### 5.3 Re-pointing the modulation matrix

The cord matrix's destinations are currently synth parameters. Without a synth they should point
at the outputs instead: **CV pitch, CV level, gate length, accent/velocity, MIDI velocity,
transpose, octave offset**. The sources are unchanged (velocity, key, random, LFO, TIDE, SWARM,
energy, drift) — so Tessera's per-track LFO becomes a CV modulator, which is exactly what a
modular wants. This is the musically richest part of the port and needs new destination IDs in
`pid.h` (append-only, so old projects keep working).

---

## 6. Phases

| Phase | Content | Done when |
|---|---|---|
| **0** | This plan | agreed |
| **1** | Engine extraction: move the time base out of `mix.c` and the control-rate LFO out of `voice.c`; add a no-audio build of the engine | the OC engine variant emits **byte-identical notes** to the FM-1 build (compare `tests/tessera_baseline.txt`) |
| **2** | OC platform layer: clock, CV/trig out, MIDI in/out + clock, SD storage, encoders/buttons, display | a host simulation drives the engine and dumps CV/MIDI events to a file |
| **3** | The OC block driver + output mapping: events → CV/trig/MIDI | the simulated CV/MIDI stream is musically correct (gates aligned, no stuck notes) |
| **4** | The UI, in the o_C idiom: page/navigation model, generators, harmony, cords, projects | every engine parameter reachable with 2 encoders + 4 buttons |
| **5** | Packaging: PlatformIO project, `platformio.ini` for `teensy40`, flash and test with the rig | it plays on the module |

Phase 1 is small and mechanical, and it de-risks everything after it: if the no-audio engine
reproduces the FM-1's notes exactly, the rest is plumbing.

### 6.1 Phase 1, concretely

1. **New file `firmware/src/seqtime.c`** — move `beat_samples()`, `seq_beat_samples()`,
   `seq_div_samples()`, `DIV_DEN` and `MIDI_BEAT_UNITS` out of `mix.c` (which is audio-only).
   Both builds include it.
2. **New file `firmware/src/seq_lfo.c`** — move `track_lfo_tick()`, `lfo_wave()`, `lfo_rand()`,
   `lfo_sync_inc()` and `trk_vmode()` out of `voice.c`. Control-rate, not audio; needed by the
   OC build and useful there (§5.3).
3. **New directory `firmware/oc/`** — the port: `oc_main.c`, `oc_clock.c`, `oc_out.c`, `oc_cv.c`,
   `oc_midi.c`, `oc_sd.c`, `oc_ui.c`, plus `vendor/` for the o_C platform code with its licence
   headers.
4. **A no-audio voice stub** — `trk_note_on/off`, `trk_all_off`, `engine_block` become the bridge
   from the engine to the output layer. `voice.c` keeps the polyphonic machinery behind
   `#if !MELODEE_ENGINE_ONLY`, or (preferred, upstream-friendly) the OC build simply does not
   compile `voice.c` and provides its own stub.
5. **A host test** — `tests/oc_engine_test.c`: run the OC engine variant over the same scenarios
   as `tests/tessera_test.c` and compare the note list with `tests/tessera_baseline.txt`.

### 6.2 Where the code lives

The port goes **in the Tessera repo, on a branch** (`port/oa-c`), not in a separate repository:
the engine sources are shared, and Tessera's own rule is to keep upstream syncing easy. The
branch holds `firmware/oc/` plus the two small refactors of §6.1. `main` is left untouched.

The o_C platform code is vendored under `firmware/oc/vendor/` from O_C-Phazerville (MIT/GPL),
with the licence files, following Tessera's existing `firmware/third_party/` convention.

---

## 7. Risks

| Risk | Severity | Handling |
|---|---|---|
| The no-audio engine diverges from the FM-1 build (timing, note order) | high | Phase 1's baseline comparison catches it before anything else is built |
| The UI cannot present 86 pages / 4 knob columns with 2 encoders | high | the page model is redesigned around the o_C idiom; the parameter set stays, the *navigation* does not |
| The 128×32 (not 64) display variant | medium | the UI targets 128×64 and is checked against 128×32 |
| Modulation destinations need new parameter IDs | medium | `pid.h` is append-only, so old projects survive |
| Storage: no NOR flash on the module | low | microSD is *easier* than Tessera's current SPI-NOR + external-flash resource cache; `storage.c`'s `st_read/st_erase/st_prog` seam is already clean |
| Gate levels (3.3 V) vs the rig | low | Q2 in §5.2; settled with the user |
| The module's shared SPI bus (DAC + OLED) contends with SD | medium | Phazerville already does this; the o_C's own timing is the reference |

---

## 8. Environment notes

* No Teensy toolchain or PlatformIO on this machine, so the device build cannot be compiled here.
  The **host** toolchain works and is the development loop: `tools/gen_tables.py` generates the
  tables without the JieLi SDK, and the host tests compile with plain `cc`.
* The JieLi AC79 toolchain and the AC79 SDK are **not** present, so `./build.sh` cannot run.
  `tests/run_tests.sh` gates on `build/tessera.fwsc`; individual host tests can be compiled and run
  directly (as `tests/gen_test.c` was, successfully, during the survey).
* The host loop is the same one Tessera already uses: "Generators can be developed and tested on
  the PC; flash the device only to check how it plays."
