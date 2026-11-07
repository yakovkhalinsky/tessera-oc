# Tessera-OC — UI improvement plan

*Base: `main` @ `9dc2ca4` (PR #2 merged). Hardware: original mxmxmx uO_C rev A, no VOR, Teensy 4.0,
build env `T40` with `-DOC_HW_UOC_A` and `-DOC_ENC_REV_PINS_21_22=1`.*

This plan covers the five areas Yakov chose: (1) track overview, (2) track parameter editing,
(3) navigation and controls, (4) live performance view, (5) visual polish. It is split into
**six phases, one draft PR each**. Phase 1 is the input and navigation framework that the rest
builds on.

Phazerville (O_C-Phazerville) is a **design reference only**. We take its *conventions* (two
encoders as two "hands", encoder push to act, long press for menus, a button as shift, the
split-screen idea) and **no code**. Nothing GPL from Phazerville is copied, paraphrased
line-by-line, or vendored. Every line in `firmware/` and `parts/` is written fresh.

---

## 0. Constraints (all phases)

| | |
|---|---|
| Storage | **Read only.** `OcFlashRO` / `oc_storage.cpp` stays as it is. No flash or EEPROM writes, and no new persistence of UI state (last screen, macros, and so on). The CI grep in `.github/workflows/host-tests.yml` must stay green. **Do not name any new function or method `format`** (the grep matches `\.format\(`), and avoid the other grep tokens (`eraseFiles`, `lfs_remove`, …) even in comments. |
| Pins | **No change to any pin's direction or mode.** No new `pinMode` calls on existing pins. `OC_INPUT_PINS`, `OC_OUTPUT_PIN_LIST`, `OC_PINOUT_*`, the DAC reset (pin 9) and the OLED/SPI pins stay untouched. The UI only *reads* the button and encoder pins that are already `INPUT_PULLUP`. |
| Safe mode | Bottom button (pin 4 **or** 5) held at power-on means safe mode: 25 ms settle, then 3 samples 10 ms apart, all LOW (`setup()` in `oc_platform.cpp`). **Unchanged.** The crash-report pager in `oc_diag_crash()` (TOP = next page, BOTTOM = continue, fresh-press logic) is **unchanged**. The new input framework must also ignore any button already held when `ui_init()` runs, until it is released (the "boot-latched" rule), so a button held through boot never fires an action. |
| ISR | The 60 µs core tick (`oc_tick`, `IntervalTimer`, priority 80) runs the engine. Nothing in the UI may lengthen it. The UI never blocks with interrupts off for more than one short critical section (one parameter apply). |
| SPI | DAC and OLED share LPSPI4. The UI's flush takes `oc_spi_lock()`. While the lock is held, the tick's DAC writes are deferred (`oc_spi_pending`) and `oc_cv_service()` applies them afterwards. A longer flush means later CV and gate edges (see §5.4). |
| Engine | No changes to `vendor/tessera` (submodule pin stays at `4e4af47`). The host engine test against `tessera_baseline.txt` must keep passing byte-for-byte. The UI talks to the engine only through `oc_port.h` / `parts/oc_view.c` accessors. New accessors are added to `parts/oc_view.c` (and declared in `oc_port.h`). |
| CI | Both jobs (`host` = `tools/build_host.sh`; `device` = write-path grep, `check_read_only.py`, `pio run -e T40`, size, `check_sections.py`) green on every PR. New host tests go into `tools/build_host.sh`, so CI runs them automatically. |
| Git | Fresh branch from `main` per phase. **Draft PRs only. Never merge, never force-push, never squash or rebase pushed work.** Author and committer `Yakov <646086+yakovkhalinsky@users.noreply.github.com>`. Update `CHANGELOG.md` (under an "Unreleased" heading) and README's UI section in each PR. No version bump or tag. |
| Licence | New files carry `SPDX-License-Identifier: GPL-3.0-only` and `Copyright (C) 2026 Yakov Khalinsky`. Any new font is original or from a licence already in `LICENSES/`. |

---

## 1. Current state (what the code does today)

Files: `firmware/oc_ui.cpp` (365 lines), `firmware/oc_display.cpp` (187), `firmware/oc_platform.cpp`
`loop()`/`setup()`, `parts/oc_view.c` (engine accessors), `firmware/oc_diag.cpp` (crash and safe-mode screens).

**Input**

* `loop()` (`oc_platform.cpp`) calls `oc_buttons_edges()`. It reads all four buttons (`digitalRead`, active low) and returns press edges. **Only `BUT_TOP` (play/stop) and `BUT_BOT` (restart) are used. `BUT_L` and `BUT_R` edges are computed and thrown away.**
* So `ui_button()` and `ui_page_next()` in `oc_ui.cpp` are **never called**. Only the HOME page is reachable today. GEN, SCL, CV, MOD and PROJ exist in `PAGES[]` but can't be opened. The footer `"L:ROW R:VALUE T:PAGE"` is wrong: T is play/stop.
* Encoders are polled once per `loop()` pass in `ui_input()` → `encoder_step()` → `oc_enc_step()` (quadrature table in `oc_port.h`, 4 states per detent). Nothing samples them while `ui_draw()` + `oc_display_flush()` run, so a fast turn during a flush can lose detents.
* The left encoder moves the row (`oc_enc_row`, wraps). The right encoder adds ±1 per detent to the row's value (`ui_row_turn`). There's no acceleration, so TEMPO 40..240 or SEED 1..128 take many turns. The 21/22 shaft reversal (`OC_ENC_REV_PINS_21_22`) is applied in `ui_input()`.
* Side effect: moving the cursor onto a `ROW_TRACK` row **selects that track** (`oc_select_track`). Navigation changes state.
* There's no debounce on buttons (single read per loop pass, edge only), no long press, and no combos.

**Display**

* The driver is **SH1106** (132-column RAM, column offset from calibration `oc_cal_display_offset`, default 2), with a u8glib-style SSD1306 init sequence (`OLED_INIT`). 128×64, 1 bpp, 1 KiB framebuffer `display_fb`.
* The primitives are `oc_display_pixel`, `oc_display_rect` (outline only) and `oc_display_text` (Tessera's 5×7 `MO_FONT`, 6 px advance, lower case drawn as upper case). There's no fill, no inverse and no clipping beyond bounds.
* `oc_display_flush()` sends all 8 pages every time: 3 command bytes + 128 data bytes per page, byte-by-byte `SPI.transfer` at 12 MHz, with the SPI lock held **for the whole frame**.
* `ui_draw()` runs every **100 ms (10 fps)** from `loop()`. It's a text list: a header (`"HOME T1 120 BPM"`), 6 rows (`>LABEL value`) and a footer.

**Engine accessors available** (`parts/oc_view.c`)

* `oc_selected_track`/`oc_select_track` (`song.sel`), `oc_track_generator` (`tessera.mode[]`), `oc_tempo`/`oc_set_tempo` (40..240).
* Descriptors: `oc_tx_desc`, `oc_track_desc`, `oc_engine_desc`, `oc_param_format`. Values: `oc_tx_get/set` (→ `tessera_param_set`, which takes its own IRQ-off section), `oc_track_param_get/set` (direct `int16` write, clamped).
* Row lists: `oc_gen_rows` (MODE + 3 knobs per generator), `oc_scl_rows` (ROOT, SCALE, TRANS, QUANT, OCT), `oc_cv_rows` (OCT, PITCH, ACCENT, V/OCT, OUT), `oc_mod_rows` (4 × SRC/DST/AMT).
* Engine features we can drive: `P_MUTE` (honoured in `parts/oc_voice.c` `trk_note_on_work`), ROLL/SEED (`tessera_seed_turn`; a new seed waits for the next bar while playing, and `tessera.roll[tr] != 0` means a roll is pending), per-generator density/probability params (see §4.3), `TX_SUB_CHANCE` and `TX_EN_LEVEL` (ENERGY).
* **No CV input is read anywhere** (no ADC code in `firmware/` or `parts/`). TR2..TR4 are configured but unused. TR1 is the external clock.

**Timing that matters** (from the code)

| | |
|---|---|
| Core tick | `OC_TICK_US` = **60 µs** (`IntervalTimer`, priority 80). Each tick: `oc_clock_watch` (TR1 edge), `oc_clock_advance(n≈2.646 samples)`, `oc_out_tick`, `oc_clock_out`. |
| Engine block | FS 44100, CTL 32 → one `oc_block()` every 32 samples = **725.6 µs** (1378.125/s), **run inside the tick ISR** (`oc_clock_advance` → `oc_block`). Its worst-case cost is tracked in `oc_persist.blk_max_cyc` and shown as "BLK n US" on the safe-mode screen. *Note: the comment in `oc_hal.h` says the sequencer runs in the main loop, but it runs in the ISR. The engine's own `TESS_IRQ_OFF` sections protect UI-side knob writes.* |
| Frame | `ui_draw()` every 100 ms. Flush is 8 × 131 bytes ≈ 1048 bytes; that's ≈0.70 ms of wire time at 12 MHz, and realistically **~1–1.5 ms** with per-byte `SPI.transfer` overhead. **This hasn't been measured on hardware**; phase 1 adds the measurement. During that window every DAC update from the tick is deferred, so gate and CV edges can be up to one flush late. |
| Input | Polled per `loop()` pass, so not at all during draw + flush. |

---

## 2. Target model (summary)

**Screens:** `OVERVIEW` (home) → `TRACK` (edit, with tabs GEN · SCL · CV · MOD) · `PERFORM` · `PROJECT` · `INFO`.
A long press on the left encoder opens a small **screen menu** from anywhere.

**Global control map** (set in phase 1, the same on every screen):

| Control | Tap (< 400 ms, no turn while held) | Long (≥ 600 ms) | Held + turn / chord |
|---|---|---|---|
| **Left encoder** turn | move cursor (row / track / tab) | — | — |
| **Right encoder** turn | change value under cursor (accelerated) | — | — |
| **L push** | **BACK** (up one level; on OVERVIEW: no-op) | **MENU** (screen picker) | — |
| **R push** | **ENTER / ACT** (open, toggle, confirm) | **CONTEXT** (reset value to default in TRACK; ROLL in PERFORM) | **R held + R turn = fine** (×1 when accel is on) |
| **TOP button** | **PLAY / STOP** (unchanged), on press, at most 40 ms late if the chord window is used (`UI_CHORD_MS`, compile-time, so either §8 Q1 answer is a one-line change) | — | — |
| **BOTTOM button** | **RESTART** on release (unchanged behaviour), only if it wasn't used as shift | — | **SHIFT**: BOTTOM + L turn = select track (any screen); BOTTOM + R turn = tempo (any screen; ignored when the clock is EXT); BOTTOM + R push = screen-specific secondary action |
| **TOP + BOTTOM** chord | toggle **PERFORM** ⇄ previous screen. No transport action: TOP's play/stop waits ≤ 40 ms for a chord (§3.2), or see the alternative in §8 Q1 | | |
| Boot | BOTTOM held at power-on = **SAFE MODE** (unchanged, before the UI exists) | | |

Rationale (o_C idiom, design level): the left hand navigates and the right hand edits. Push to act,
long press for the menu. A *physical* button doubles as shift and only fires its own action when it
wasn't used as a modifier. Transport stays on the dedicated buttons because it's the thing you hit
mid-performance.

---

## 3. Area 3 — Navigation and control mapping (PHASE 1)

### 3.1 Problems

1. Four of six pages can't be reached (BUT_L/BUT_R are dropped in `loop()`).
2. There are no press semantics: no debounce, no long press, no hold, no combos, no shift.
3. Encoder polling stops during draw + flush, which loses detents.
4. Navigation has side effects (cursor on a track row changes `song.sel`).
5. The footer hint is wrong, and nothing on screen is discoverable.
6. There's no value acceleration, and wide ranges are tedious.
7. All UI logic is in an Arduino `.cpp`, so none of it is host-testable.

### 3.2 Design

Split the UI into a **pure-C core** (host-tested) and a **thin Arduino shell**:

```
parts/oc_ui_input.c     button state machine (debounce, tap/long/hold, shift-consumed, chord, boot-latch),
                        encoder accel; input: raw pin snapshot + ms; output: ui_event_t queue
parts/oc_ui_nav.c       screen router: stack (depth ≤ 4), per-screen cursor state, menu, global map dispatch
parts/oc_ui_rows.c      the row model (today's UiRow/ROW_* + ui_row_text/ui_row_turn, moved out of oc_ui.cpp)
firmware/oc_ui.cpp      shell: samples pins, feeds the core, calls draw, owns timing stats
firmware/oc_input_isr   (inside oc_ui.cpp) 1 kHz sampler, see §3.4
```

`ui_event_t` = `{ENC_L, ENC_R, PUSH_L, PUSH_R, BTN_TOP, BTN_BOT} × {TURN(n), DOWN, TAP, LONG, HOLD_START,
HOLD_END, SHIFT_TURN(n)}` plus `CHORD_TB`. Screens implement
`on_event(screen*, const ui_event_t*)` and `draw(screen*, fb*)`.

* **Debounce:** 5 ms stable (5 consecutive 1 ms samples).
* **Tap** = released < 400 ms with no turn and no other button. **Long** fires once at 600 ms while still held (not on release), so the menu appears without letting go. **Hold** start/end are also emitted (PERFORM's FILL uses them).
* **Shift-consumed:** if an encoder turns or another control is pressed while BOTTOM is down, BOTTOM's release does **not** emit TAP (no restart).
* **Chord TOP+BOTTOM:** both down within 40 ms. TOP's play/stop is deferred by up to 40 ms; if BOTTOM arrives in the window it's a chord (no transport action), otherwise TOP fires at 40 ms. *Alternative if 40 ms feels late:* fire on press and treat the chord as "TOP held ≥ 600 ms + BOTTOM". Yakov's call (see the questions in §8).
* **Boot-latch:** controls that are down at `ui_init()` are latched and emit nothing until released.
* **Accel:** detent interval < 20 ms → ×8, < 50 ms → ×4, < 100 ms → ×2, else ×1. It only applies when the value's range is over 24 steps (from `param_desc_t` min/max). Enum/name params are never accelerated. Holding R while turning forces ×1.
* **Selection is explicit:** OVERVIEW's cursor is the selected track (moving it selects; that *is* the intent there). In TRACK, the left encoder never changes track. BOTTOM + L turn does, everywhere.
* **Reverse:** keep the `OC_ENC_REV_PINS_21_22` fix exactly as is. It moves into the shell's sampler, applied before the core sees steps.

### 3.3 Mockup — screen menu (L long press from anywhere)

```
+--------------------------------+   128x64, 5x7 font, 6px advance, 8px lines
|▓MENU▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓|   header bar inverted (y 0..8)
|  OVERVIEW                      |
|▓▓PERFORM▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓|   cursor row inverted
|  TRACK EDIT                    |
|  PROJECT                       |
|  INFO                          |
|                                |
|L back  R open                  |   footer hint, 3x5 font (phase 2)
+--------------------------------+
```

### 3.4 Input sampling (frame-safe)

Add a **second `IntervalTimer` at 1 kHz with a lower priority than the core tick**
(numerically higher than 80, e.g. 160), so it can never delay `oc_tick`. It reads the 6 control
pins with `digitalReadFast`, runs `oc_enc_step` for both encoders, and pushes raw samples (or
accumulated detent counts and button levels) into a single-producer/single-consumer ring that
`loop()` drains. Cost is < 1 µs per ms. If a reviewer prefers zero new ISRs, the fallback is to keep
loop polling but make the flush page-sliced (§5.4) so a loop pass is never longer than ~0.2 ms.
**Pin modes untouched. Only reads.** (This touches interrupt configuration, so Acid Burn reviews it
explicitly.)

### 3.5 Acceptance criteria (phase 1)

* All existing pages (HOME, GEN, SCL, CV, MOD, PROJ, re-homed as OVERVIEW / TRACK tabs / PROJECT) are reachable from the menu. The footer hint matches the real mapping on every screen.
* TOP still plays/stops (with ≤ 40 ms chord window or immediate, per the decision). BOTTOM tap still restarts. BOTTOM used as shift never restarts.
* A button held through boot does nothing until released. Safe mode and the crash pager behave exactly as on `9dc2ca4`.
* Fast encoder spins during screen updates lose no detents (host test with a synthetic ring; on hardware, 20 fast detents give 20 steps).
* No regressions in `oc_engine_test` (baseline) or `oc_port_test` (41 checks).
* INFO screen (minimal text version) shows version, `BLK max µs` (from `oc_persist.blk_max_cyc`, read only), **draw µs / flush µs (max and last)**, loop max µs and storage `RO`. Frame stats live in plain RAM, **not** in `oc_persist`, whose layout and address `check_sections.py` checks.

### 3.6 Host tests (new `tests/oc_ui_test.c`, wired into `tools/build_host.sh`)

* The input state machine against scripted pin timelines: debounce (bounces < 5 ms ignored), tap vs long boundary (399/400/600 ms), long fires once, shift-consumed suppresses TAP, chord window (39 ms → chord, 41 ms → TOP then BOTTOM), boot-latch.
* Accel table, and that enum params aren't accelerated.
* Router: menu open/close, BACK at root is a no-op, stack depth bound, BOTTOM + L turn selects the track on every screen, and L turn in TRACK never changes `song.sel` (stubbed accessors).
* Reverse flag applied exactly once (keep the existing `check_enc_rev_21_22` cases passing).

---

## 4. Areas 1, 2, 4 and 5 — design

Screen geometry used throughout (phase 2 defines it in `parts/oc_ui_gfx.h`):

```
y 0..8    header   : inverted bar, 5x7 text at y=1, 21 cols
y 9..55   body     : 47 px (e.g. 4 lanes x 11 px + gaps, or 5 text rows x 9 px)
y 56..63  footer   : hints in 3x5 font (4 px advance → 32 cols) or status
columns  : 4 x 32 px for per-track layouts; 21 x 6 px for text
```

Legend in the mockups: `▓` = inverted pixels; `▮▯` = gate history cells (filled/empty); `»` = playhead;
`M` = muted; `~` = roll pending (blinks).

### 4.1 Area 1 — Track overview (PHASE 3)

**Current state:** HOME is a text list (`PAGE_HOME` in `oc_ui.cpp`): TEMPO, TRACK 1..4 (generator name
from a hand-copied `GEN_NAME[]`, which duplicates the engine's names: GRIDS shows as "ATLAS") and CLOCK
(read-only, turning does nothing). There's no sense of what is playing: no gates, notes or mute state.

**Problems:** you can't see activity, the notes on the outputs, mutes, or a pending roll. Tempo and
clock take up rows. The track list duplicates engine names.

**Design:** four lanes, one per track/CV out. Each lane shows: the track number, the generator (from
`oc_tx_desc(TX_MODE)` names via `oc_param_format`, so `GEN_NAME[]` is deleted), the current note on the
CV (name + octave), a 16-cell **gate history strip** and state flags. The header carries transport,
tempo, the clock source and the selected track.

```
+--------------------------------+
|▓▶ 120 INT        T2   OVERVIEW▓|  header (EXT shows "EXT" and the tempo greyed as "---")
|1 TURNG  C2  ▮▯▮▮▯▮▯▮▮▯▮▯▯▮▯▮   |  lane 1
|▓2 EUCLD D#3 ▮▯▯▮▯▯▮▯▮▯▯▮▯▯▮▯ ▓|  selected lane inverted
|3 MARKV --  M▯▯▯▯▯▯▯▯▯▯▯▯▯▯▯   |  muted: M, strip empty
|4 MORPH G3 ~▮▮▯▮▯▮▮▯▮▮▯▮▯▮▮▯   |  ~ blinking: ROLL pending (next bar)
|                                |
|L trk  R open  ⇧R tempo         |  footer 3x5
+--------------------------------+
```

* The gate history is sampled **in the UI** at each frame (no ISR work): a per-track `uint16_t` shift register gets one bit = "gate on now, or a note started since the last frame". It needs one new read-only accessor `oc_track_gate_seen(tr)`: a volatile flag set in `oc_note_on` (ISR, one store) and cleared by the UI.
* The note shown comes from a new accessor `oc_track_last_note(tr)` (volatile int16, set in `oc_note_on`; `-1` = none). It's rendered as a note name in the track's own octave space (the same mapping `oc_cv_write` uses: 0 = C2).

**Input:** L turn = select track (cursor = `song.sel`). R tap = open TRACK for that track. R turn = change
generator (as today's ROW_TRACK). R long = toggle mute (same as PERFORM). BOTTOM + R turn = tempo.
L long = menu.

**Acceptance:** all 4 lanes update at frame rate; a muted track shows `M` and an empty strip; ROLL pending
shows `~`; EXT clock shows `EXT` and tempo `---`; the generator names match the engine's descriptor names
(no hand-copied table remains).

**Host tests:** golden framebuffer snapshots (phase 2 harness) for: default power-on state; track 2
selected; track 3 muted; EXT clock; roll pending. Note-name formatter (`C2`, `D#3`, `-` none, out of range
clamps). History shift register.

### 4.2 Area 2 — Editing a track's parameters (PHASE 4)

**Current state:** GEN/SCL/CV/MOD are separate, unreachable pages built by `ui_build_rows()` from
`oc_gen_rows` / `oc_scl_rows` / `oc_cv_rows` / `oc_mod_rows`. Rows are `>LABEL value`, the cursor is a `>`,
there's no range indication and no default reset. MOD shows 12 flat rows (SRC/DST/AMT × 4). The value
writes go through `oc_tx_set` / `oc_track_param_set`.

**Problems:** you can't reach it, there's no context (which track, which tab), and no sense of a value's
position in its range. MOD as 12 rows is hard to read. GEN shows only 3 knobs per generator.

**Design:** one TRACK screen with **tabs**: `GEN · SCL · CV · MOD` (+ `EVO` if `TX_EV_*` / `TX_SUB_*`
rows are added; that's optional, see §8).

```
+--------------------------------+
|▓T2 EUCLID ▓GEN▓ SCL  CV  MOD  ▓|  header: track + generator; active tab inverted (double inverted)
|  MODE              EUCLID      |
|▓▓STEPS             16        ▓▓|  cursor row inverted
|  PULSE              5   ▬▬▬▭▭▭ |  numeric rows: right-aligned value + 24px range bar
|  ROT                0   ▭▭▭▭▭▭ |
|  SEED              37   ~      |  (ROLL row: R tap = roll at next bar)
|                                |
|L row R val ⇧L trk Rlong dflt   |
+--------------------------------+
```

MOD tab: one line per slot, 3 fields on the line. L moves field by field (12 fields), and the active
field is inverted:

```
|▓T2 EUCLID  GEN SCL CV ▓MOD▓   ▓|
|1 LFO  > PITCH      +40         |
|2 RAND > GATE     ▓ +12 ▓       |
|3 ---  > ---         0          |
|4 ---  > ---         0          |
```

* **Tabs:** with the cursor on row 0 ("tab row"), L turn moves the cursor into the rows. On the header, R turn switches tabs. *Simpler alternative, recommended:* **R tap on the header cycles the tab, and BOTTOM + R push cycles the tab from any row.**
* **Range bar:** drawn for `F_INT`/`F_PCT` params with a range above 8. Enums show only the name.
* **R long = reset to default** (`param_desc_t.def`), with a 1 s toast `"DEFAULT"`.
* Generator change (MODE row) rebuilds the GEN rows immediately. The cursor clamps.
* Add `oc_gen_rows` entries for the knobs the FM-1 exposes on the GEN page that are missing (e.g. `TX_POL_DIV3/4`, `TX_EUC_*` already present). That's an `oc_view.c` table change only, no engine change. Optional per §8.

**Input:** L turn = row (or field on MOD); R turn = value (accel per §3.2); R hold + turn = fine; R long =
default; R tap = act (ROLL row, enum next); L tap = back to OVERVIEW; BOTTOM + L turn = other track (stays
on the same tab and row if it exists).

**Acceptance:** every row that `oc_*_rows` returns is editable from TRACK; tabs cycle; the default reset
works; a generator change rebuilds rows without the cursor going out of range; switching track keeps the
tab.

**Host tests:** row builder for all 14 generators (row count, ids valid, `oc_tx_desc` non-null); MOD field
navigation (12 fields, wrap); reset-to-default sets `def`; range-bar width maths (min, max, mid, negative
ranges such as OCT −3..3 and LEVEL −100..100); golden snapshots for GEN (EUCLID), SCL, CV and MOD.

### 4.3 Area 4 — Live performance view (PHASE 5)

**Current state:** none. Mute exists in the engine (`P_MUTE`, honoured in `oc_voice.c`) but isn't exposed.
ROLL exists (`tessera_seed_turn`). There's no fill concept on the port. No CV inputs are read.

**Design:** four columns, one per track, built for hitting while it plays.

```
+--------------------------------+
|▓▶120  PERFORM          T2 ▓▓▓▓▓|
| 1    ▓▓2▓▓    3      4         |  selected column number inverted
| █    ▓  █ ▓   ▯      █         |  density macro, vertical bar (24 px tall)
| █    ▓  █ ▓   ▯      █         |
| ▯    ▓  █ ▓   ▯      ▯         |
| C2   ▓D#3 ▓   M      G3~       |  current note / M muted / ~ roll pending
|-1.0V ▓+1.3▓  ----   +0.6V      |  CV out volts (from last pitch), 3x5 font
|R mute Rhold fill ⇧R roll       |
+--------------------------------+
```

* **Mute:** R tap toggles `P_MUTE` on the selected track. **On mute, the track's sounding note is released immediately** (call the port's `trk_all_off` for that track inside the engine's critical section, via a new `oc_track_mute(tr, on)` in `oc_view.c`), so no gate is left high on the CV.
* **Density macro (R turn):** one 0..100 % knob per track, mapped to the generator's own density-like parameter. The table lives in `oc_view.c`, `oc_density_param(gen)`:

  | Generator | Macro drives | Notes |
  |---|---|---|
  | TURING | `TX_SUB_CHANCE` | no native density |
  | EUCLID | `TX_EUC_PULSES` scaled to 0..`STEPS` | |
  | MARKOV | `TX_MKV_DENS` (5..100) | |
  | MORPH | `TX_SUB_CHANCE` | |
  | GRIDS/ATLAS | `TX_GRD_DENS` | |
  | BOUNCE | `TX_SUB_CHANCE` | |
  | AWAKE | `TX_AWK_PROB` | |
  | POLY | `TX_SUB_CHANCE` | |
  | QUAD | `TX_QD_PROB` | |
  | CONSTEL | `TX_CS_DENS` | |
  | SERIES, CHORD, ANSWER, STEP | `TX_SUB_CHANCE` | verify `TX_SUB_CHANCE` applies on these tracks; if not, show `--` and disable |

  Values are written through `oc_tx_set` (the engine's clamp and IRQ-off section). The bar reads back with `oc_tx_get` normalised by the descriptor's range, so the display is always the truth.
* **Probability macro (BOTTOM + R turn on PERFORM):** `TX_EV_AMOUNT` (EVOLVE amount) on generator tracks. *Note: this overrides the global "BOTTOM + R = tempo" on this one screen. Alternatively, keep tempo and drop this macro. See §8.*
* **Fill (R held ≥ 300 ms, momentary):** while held, the selected track's density macro is pushed to 100 % (and `TX_SUB_RATCH` ×2 if Yakov wants ratchets). On release the exact previous values are restored. This is a pure UI override held in RAM. Restore must survive a screen change mid-hold (release is always delivered; the router forwards HOLD_END to the screen that got HOLD_START).
* **Regenerate:** BOTTOM + R push = ROLL the selected track (`tessera_seed_turn(tr, +1)`). While playing it lands on the next bar (engine behaviour), and `~` blinks until `tessera.roll[tr]` clears. Needs a read-only accessor `oc_track_roll_pending(tr)`.
* **CV readouts:** outputs only in this phase. They come from a new `oc_cv_last_pitch(ch)` (volatile, written in `oc_cv_write`) and are shown as volts = pitch / 1536 (1 V/oct) or /15360 (0.1 V/oct), plus a note name. **CV inputs are not read by the firmware today.** Adding them needs ADC code, the T4.0 o_C CV-input pin map verified against hardware and Phazerville's documentation, and Acid Burn/ZC review. That's out of scope here (an optional phase 5b after Yakov's OK). Input pins would only be *read* (analog), but the pin assignment must be verified first.

**Acceptance:** mute toggles within one frame and leaves no stuck gate (host test: note on → mute → gate 0
within one block); the density macro moves the right param for all 14 generators; fill restores exact
values on release (including when the screen changes mid-hold); roll pending shows and clears; volts
formatting is correct at −3 V, 0 V, +7 V and in 0.1 V/oct mode.

**Host tests:** `oc_density_param` table completeness (every `TESS_ENG_*`); macro ↔ param normalisation
round trip; fill override/restore state machine; mute releases the gate (engine host sim:
`oc_gate_state[]` from `tests/oc_hostsim.c`); volts/note formatter; golden snapshot of PERFORM.

### 4.4 Area 5 — Visual polish (PHASE 2 foundation + PHASE 6 finish)

**Current state:** one font (5×7 `MO_FONT`, upper case only); `>` as the cursor; no fills, inversion or
clipping; full 1 KiB flush at 10 fps with the SPI lock held for the whole frame; no frame timing measured.

**Phase 2: drawing kit (no screen changes yet beyond adopting it):**

* `parts/oc_ui_gfx.c` (pure C, operates on a `uint8_t fb[1024]` passed in, so it's host-testable): `px`, `hline`, `vline`, `fill_rect`, `invert_rect` (XOR), `text5x7`, `text3x5` (new **original** 3×5 font, digits/upper case/`+-.:/>~`), `text5x7_x2` (2× scaled for big numbers), clip rect, `text_right` (right-align), `ellipsize` (truncate with `.`).
* Layout constants (`UI_HDR_H 9`, `UI_FTR_Y 56`, `UI_COL_W 32`, `UI_CH_W 6`, …) in `parts/oc_ui_gfx.h`.
* `firmware/oc_display.cpp` keeps the SPI side only, plus `oc_display_fb()` returning the buffer. The drawing code moves to `oc_ui_gfx.c`. Existing callers (`oc_diag.cpp`, splash) keep working through thin wrappers, so the crash and safe-mode screens are byte-identical.
* **Golden snapshot harness:** `tests/oc_ui_test.c` renders a screen into a buffer and compares it with `tests/golden/<name>.txt` (64 lines of 128 `#`/`.`). The `UI_GOLDEN_UPDATE=1` env var rewrites the files. Also emit `.pbm` into `build/` (not committed) for README screenshots.

**Phase 6: polish and frame budget:**

* **Dirty-page flush:** keep a shadow copy of the last-sent 1 KiB. Send only the 128-byte pages that changed. The idle frame cost drops to ~0.
* **Page-sliced flush:** take and release the SPI lock **per page** and call `oc_cv_service()` between pages. A deferred DAC write then waits at most one page (~0.12–0.2 ms) instead of a whole frame. This is the main way the UI stops delaying CV and gate edges.
* **Frame rate:** 20 fps target (50 ms), adaptive. If the measured draw + flush p-max > 4 ms, fall back to 10 fps. Never redraw faster than the content changes (dirty flag from events, plus a 250 ms tick for blink/history).
* **Animations** (cheap, frame-driven, no timers): roll-pending `~` blink at 2 Hz; a playhead dot advancing on the gate strip; a 1 s **toast** line in the footer for actions (`MUTED T3`, `ROLL T2 @BAR`, `DEFAULT`, `SAVE OFF (RO)`); an 80 ms invert flash on the lane when a mute is toggled. No scrolling text and no per-pixel tweening.
* Header and footer consistent on every screen; footer hint text comes from the screen's own action table, so it can't drift from the real mapping (fixes the current `T:PAGE` lie).
* README "Controls" section with an ASCII control map and the PBM screenshots converted to PNG.

**Frame budget (the rule):** the UI runs only in `loop()`, never in an ISR (except the 1 kHz sampler of
§3.4, < 1 µs). Draw ≤ 2 ms, flush ≤ 1.5 ms full / ≤ 0.2 ms per page, **SPI lock held ≤ one page at a time
after phase 6**. INFO shows the measured maxima so we can prove it on hardware. The engine block (in the
60 µs ISR) is unaffected: the UI adds no work to `oc_tick` beyond two single-store flags in `oc_note_on`
(phase 3) and one store in `oc_cv_write` (phase 5).

**Acceptance (phase 6):** with no UI changes, a dirty flush sends 0 pages; INFO shows flush max ≤ 0.2 ms
per page on hardware; the golden snapshots are updated and reviewed; `BLK max` on INFO isn't higher than
`9dc2ca4`'s value on the same patch (Yakov reads it off INFO; within noise).

---

## 5. Phases (one draft PR each)

| # | Branch | Scope | Depends on | Host tests added |
|---|---|---|---|---|
| **1** | `ui/p1-input-nav` | Pure-C input state machine + router + row model (moved out of `oc_ui.cpp`); 1 kHz input sampler (or the fallback); wire L/R push; global control map; screen menu; existing pages re-homed (OVERVIEW = old HOME list for now, TRACK tabs = old GEN/SCL/CV/MOD lists, PROJECT, INFO text); accel; boot-latch; frame and loop timing stats on INFO | `main` | `oc_ui_test`: input, accel, router, reverse |
| **2** | `ui/p2-gfx` | Drawing kit (`oc_ui_gfx.c`), 3×5 font, inverted selection, header/footer grid applied to the phase 1 screens; golden snapshot harness; display driver reduced to SPI + buffer | 1 | gfx primitives, fonts, golden snapshots of every phase 1 screen, crash/safe screens unchanged |
| **3** | `ui/p3-overview` | OVERVIEW lanes: gate history, current note, mute/roll flags; accessors `oc_track_gate_seen`, `oc_track_last_note`, `oc_track_roll_pending`; delete `GEN_NAME[]` | 2 | note names, history, snapshots |
| **4** | `ui/p4-track-edit` | TRACK screen with tabs, range bars, default reset, MOD as 4×3 fields, track switch keeps tab | 2 (3 for back-nav target) | row builder ×14 gens, MOD fields, default, bar maths, snapshots |
| **5** | `ui/p5-perform` | PERFORM: mute (with release), density macro table, fill override, roll, CV-out readouts; `oc_track_mute`, `oc_density_param`, `oc_cv_last_pitch` | 3 | macro table, fill state machine, mute releases gate (hostsim), volts, snapshot |
| **6** | `ui/p6-polish` | Dirty + page-sliced flush, adaptive 20 fps, toasts, blink/playhead, flash on mute, footer from the action table, README controls and screenshots | 5 | dirty-page diff, toast timing, footer = action table |
| 5b *(optional, needs Yakov + review)* | `ui/p5b-cv-in` | Read CV inputs (ADC) for readouts only | 5 + verified pin map | — |

Each phase: fresh branch from the then-current `main` (or stacked on the previous phase's branch if it isn't
merged yet; say so in the PR body), draft PR, CI green, PR body = summary, screenshots (golden PBM → PNG),
test counts, **hardware test steps for Yakov** (what to press, what to see), and the risk notes.

Hardware test after each phase uses the read-only build. There's no save, so nothing on the module changes
except the program.

---

## 6. Risks

| Risk | Mitigation |
|---|---|
| A new 1 kHz timer interacts with the 60 µs core tick | Lower priority; < 1 µs body; reviewer sign-off; fallback = loop polling + page-sliced flush |
| UI writes race the ISR engine | Only through `tessera_param_set` (IRQ-off inside) or single `int16` stores; mute/all-off inside `TESS_IRQ_OFF`/`fm1_irq_off` sections, kept short |
| Golden snapshots churn | One snapshot per screen state; the update flag is explicit; reviewers diff ASCII |
| CI write-path grep false positives | Naming rule in §0 |
| Flash/size growth | `pio run -t size` in CI; fonts are small (3×5 ≈ 200 B) |
| TOP chord window feels laggy | 40 ms max; or the immediate-TOP alternative (§8 Q1) |

---

## 7. Out of scope

Saving anything (UI state, macros, projects): storage stays read-only until a separate, reviewed
storage PR. MIDI out. Engine changes. VOR/Plum variants. Pin remaps. Calibration UI.

---

## 8. Open questions for Yakov

1. **Buttons:** keep TOP = play/stop and BOTTOM = restart (and BOTTOM doubling as SHIFT, firing restart only on a clean tap)? And for the TOP + BOTTOM → PERFORM chord: delay TOP by ≤ 40 ms, or keep TOP instant and make the chord "hold TOP, press BOTTOM"?
2. **Fill:** what should FILL mean? Proposed: while held, density to 100 % (optionally ratchets ×2), then restore. Alternatives: ENERGY LEVEL up, or a one-bar fill.
3. **CV inputs:** the firmware doesn't read the 4 CV ins at all today. Is outputs-only readout fine for now, or do you want phase 5b (reading CV ins: needs a pin-map check and review)?
4. **Power-on screen:** OVERVIEW (proposed) or PERFORM?
5. *(minor)* On PERFORM, should BOTTOM + R turn be tempo (global) or the probability/EVOLVE macro?
