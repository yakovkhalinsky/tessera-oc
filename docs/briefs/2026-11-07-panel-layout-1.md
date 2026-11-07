<!-- docs/briefs/2026-11-07-panel-layout-1.md -->
> **Archived job brief: panel-layout-1** (run 1 of the panel layout job: the panel drawing and guide with LEFT/RIGHT buttons, stacked on fix/render-check). Run on 2026-11-07 in Claude Code; this is the brief as run, with build-host-only paths replaced. Result: failed policy: two blocked commands during the run (it read a file outside the clone, and tried to save a note outside it), so branch `docs/panel-layout` at `4d95268` is not shipped (CI was green); relaunch pending. Note: kept for the record; the relaunch is `2026-11-07-panel-layout.md`.
> The commands below were for that moment (base commits, expected outputs); to redo the work, read `docs/STATUS.md` first and branch from today's `main`.
>
> **Repo rules (apply to every job):** work on the job branch named below and push only that branch, with an explicit refspec; pull requests are **drafts** only; never merge, mark ready, tag or release (Yakov merges, with merge commits); never flash, upload or reflash hardware, and never with a Teensy attached to the build host, since only Yakov flashes, on his own computer; storage stays read-only; no engine changes; commit as `Yakov <646086+yakovkhalinsky@users.noreply.github.com>` with no trailers; never probe credentials. See `CLAUDE.md`.

# Claude Code job tessera-panel-layout-1: the panel layout, left and right buttons (tight brief)

You are in a fresh clone of `yakovkhalinsky/tessera-oc` (your working directory). Everything you need is in this brief. You copy three files in, run one script and compare its output; you design nothing.

## The rule for any problem (read first)
* Every check prints a marker `<NAME>_EXIT=<n>`; 0 means pass. The script's whole output must equal Appendix C byte for byte. If anything differs, or any step below fails, write the exit report (step 7) whose first line starts exactly `STOP: <reason>`, end, and do not commit or push. Never print a line starting with `STOP:` otherwise.
* Do not reason a difference away, edit the script, re-run a check differently or change a file to make it pass. The only allowed fix is a copying slip (your copy of an appendix differs from the appendix): fix it once.
* A rejected push means a `STOP:` line with the exact error, and end. No credential, token, ssh or config probing of any kind. No `gh` command at all.

## Branching (stacked on fix/render-check, PR #7)
```sh
git checkout -b docs/panel-layout origin/fix/render-check
git submodule update --init --recursive
git log -1 --format=%h    # must print fa6a791
```
Your job branch is **`docs/panel-layout`**. Push it only as `git push origin HEAD:docs/panel-layout`. No other ref, no PR.

## Scope (Yakov: the panel guide's button locations are wrong)
* `docs/img/panel.svg`: redrawn as the real uO_C rev A panel, top to bottom: the OLED; the LEFT and RIGHT buttons side by side under it; the L and R encoders; TR1-4; CV1-4; A-D.
* `docs/GUIDE.md`: every button named LEFT/RIGHT (LEFT = play/stop, RIGHT = restart/shift, as calibrated), one note that the code calls them TOP/BOTTOM, citing `firmware/oc_platform.cpp` and `firmware/oc_port.h` for where the calibration sets it.
* `docs/UI-PLAN.md`: the PERFORM chord and the shift wording say LEFT/RIGHT.
* `CHANGELOG.md`: one line.
* Nothing else: 0 lines in `.github/`, `firmware/`, `parts/`, `tests/`, `vendor/`, `tools/`, `docs/img/*.png`.

## Hard budget (`git diff --numstat origin/fix/render-check`; exact)
```
1	0	CHANGELOG.md
25	17	docs/GUIDE.md
20	18	docs/UI-PLAN.md
39	34	docs/img/panel.svg
```

## Do (in this order, nothing else)
1. The branching commands above.
2. `mkdir -p build`. Write Appendix A with the Write tool to `build/layout.diff`. Then always run `sed -i 's/^$/ /' build/layout.diff` (a blank context line in a diff is one space; the Write tool may drop it, and the diff has no truly empty lines). Then `git apply --check build/layout.diff && git apply build/layout.diff; echo APPLY_EXIT=$?` (must print `APPLY_EXIT=0`).
3. Write Appendix B with the Write tool to `build/check.sh` and Appendix C to `build/check.expected`, each exactly. Then `md5sum build/layout.diff build/check.sh build/check.expected` must print exactly:
   ```
   bf6831920ea82f71ad8e268ba96a5b0e  build/layout.diff
   3b167c49bd7850f3abc53c474edcab5a  build/check.sh
   cf951e05f9f6f9a801c648152c36fefc  build/check.expected
   ```
4. **Checks** (about 2 minutes; one command, give it a 600 s timeout): `sh build/check.sh > build/check.out 2>&1; echo CHECK_SH_EXIT=$?; diff build/check.expected build/check.out; echo DIFF_EXIT=$?`. It must print `CHECK_SH_EXIT=0`, no diff lines, and `DIFF_EXIT=0`. The script builds and runs the host tests (with the render check), prints the md5 of the screens and the edited files, parses the SVG, and checks the LEFT/RIGHT wording and the citations.
5. **Commit** (one): `git add CHANGELOG.md docs/GUIDE.md docs/UI-PLAN.md docs/img/panel.svg && git commit -m "docs: the panel layout, LEFT and RIGHT buttons"; echo COMMIT_EXIT=$?`. No trailer. Do not run `git config`. Then, on the commit: `tools/build_host.sh > build/after.log 2>&1; echo AFTER_COMMIT_EXIT=$?` must print `AFTER_COMMIT_EXIT=0`, `grep -c '^render_screens: docs/img current' build/after.log` must print `1`, and `test -z "$(git status --porcelain)"; echo TREE_EXIT=$?` must print `TREE_EXIT=0`. If any differs: a `STOP:` line, no push.
6. **Push:** `git push origin HEAD:docs/panel-layout; echo PUSH_EXIT=$?` must print `PUSH_EXIT=0`.
7. **Exit report:** write it to `build/panel-layout-exit-report.md` and repeat it as your final message: the new SHA, the numstat, every marker line and the diff result, and anything that went wrong (a denial, a difference, a copying slip you fixed). On a problem its first line is `STOP: <reason>`.

## Appendix A: the edits (`build/layout.diff`, apply exactly)
````diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
index 315395f..a7053d8 100644
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -7,6 +7,7 @@ The port's releases. The version is the one the firmware carries
 
 ### Docs: the panel and controls guide
 - `docs/GUIDE.md` (linked from the README): the uO_C rev A panel (`docs/img/panel.svg`), the controls on each screen as they are on `main`, the power-on, the safe mode and the crash pager. The screen pictures (`docs/img/screen-*.png`) are drawn by the firmware's own screen code on the host (`tools/render_screens.sh`, `tools/render_screens/`), and CI fails if they are stale (a screen that differs, is missing or is left over; the check is skipped, with a message, off Linux).
+- The panel drawing and the controls tables follow the real panel: the LEFT and RIGHT buttons side by side under the display, above the encoders. The code calls them TOP and BOTTOM; LEFT is play / stop as calibrated (the FLIPCONTROLS flag picks the pinout).
 
 ### UI, phase 1a: the input core (no change on the module yet)
 - `parts/oc_ui_input.c`: the buttons and encoders as events (5 ms debounce, tap ≤ 400 ms, hold from 400 ms, long at 600 ms, BOTTOM as shift (each event says whether BOTTOM is held), TOP + BOTTOM chord within `UI_CHORD_MS` = 40 ms either way round, boot-latched buttons ignored until released, value acceleration) and the 1 kHz sampler's ring. Host-tested in `tests/oc_ui_test.c`; not wired into the firmware until phase 1b. Plan: `docs/UI-PLAN.md`.
diff --git a/docs/GUIDE.md b/docs/GUIDE.md
index ef0b520..198f10c 100644
--- a/docs/GUIDE.md
+++ b/docs/GUIDE.md
@@ -17,14 +17,22 @@ every push, renders them again and fails if they differ from the files here
 
 ![The uO_C rev A front panel, as Tessera-OC uses it](img/panel.svg)
 
+From the top: the display; the **LEFT** and **RIGHT** buttons side by side under
+it; the **L** and **R** encoders; then the trigger inputs, the CV inputs and the
+outputs. The code calls the LEFT button **TOP** and the RIGHT button **BOTTOM**.
+Which pin each one is comes from the calibration, not from a fixed pin: the
+`FLIPCONTROLS` flag picks the plain or the flipped pinout
+(`firmware/oc_platform.cpp:567`, the tables at `firmware/oc_port.h:137-138`).
+LEFT is play / stop as calibrated.
+
 | Control or jack | What it does on `main` |
 |---|---|
 | **L encoder, turn** | moves the cursor up and down the list (it wraps round) |
 | **L encoder, push** | nothing yet |
 | **R encoder, turn** | changes the value in the row under the cursor |
 | **R encoder, push** | nothing yet |
-| **TOP button** | play / stop |
-| **BOTTOM button** | restart from the top. Held at power-on, it starts the **safe mode** |
+| **LEFT button** (TOP in the code) | play / stop |
+| **RIGHT button** (BOTTOM in the code) | restart from the top. Held at power-on, it starts the **safe mode** |
 | **TR1** | clock in: the rig's clock, counted on the falling edge |
 | **TR2 – TR4** | read, but not used |
 | **CV1 – CV4** | not used |
@@ -61,7 +69,7 @@ is the footer.
 When the L encoder's cursor lands on a `TRACK` row, that track becomes the
 selected one (`T1` – `T4` on the top line).
 
-The footer still says `T:PAGE`, but on `main` the TOP button plays and stops.
+The footer still says `T:PAGE`, but on `main` the LEFT button plays and stops.
 The other pages in the code (GEN, SCL, CV, MOD, PROJ) cannot be reached from the
 panel yet.
 
@@ -69,15 +77,15 @@ panel yet.
 
 ![The safe mode's screen](img/screen-safe-mode.png)
 
-To start in safe mode, hold the **BOTTOM** button while you power on. This is
+To start in safe mode, hold the **RIGHT** button while you power on. This is
 what `setup()` in `firmware/oc_platform.cpp` does:
 
 - After the button pins get their pull-ups, it waits **25 ms** for them to settle.
 - It then reads the pins **3 times, 10 ms apart**. A button counts as held only if
   its pin reads low on **all three** samples.
-- The calibration, which says which button is BOTTOM, has not been read yet. So
-  either of the two button pins (4 and 5) is accepted. In practice either button
-  held at power-on starts the safe mode.
+- The calibration, which says which pin is the RIGHT (BOTTOM) button, has not
+  been read yet. So either of the two button pins (4 and 5) is accepted. In
+  practice either button held at power-on starts the safe mode.
 
 In the safe mode the module runs:
 
@@ -104,8 +112,8 @@ the report, seven lines to a page.
 
 | Button | In the crash pager |
 |---|---|
-| **TOP** | the next page (after the last page it goes back to the first) |
-| **BOTTOM** | go on with the boot |
+| **LEFT** (TOP) | the next page (after the last page it goes back to the first) |
+| **RIGHT** (BOTTOM) | go on with the boot |
 
 The pager first waits until both buttons are released, so a button already held
 at power-on does not count. After that each press has to be released before the
@@ -138,15 +146,15 @@ With no report but a boot that stopped part of the way through, the screen shows
 
 ## Coming: phase 1 of the UI (PR #4 / #5)
 
-These are **not on the module yet**:
+These are merged but **not on the module yet** (host-tested only; the firmware
+does not use them until phase 1c):
 
-- **PR #4** (merged) adds the new input core: debounce, tap, hold and long
-  presses, BOTTOM as a shift, the TOP + BOTTOM chord. It is host-tested only.
-  The firmware does not use it yet.
-- **PR #5** (a draft) adds the screen router. BOTTOM + L turn selects the track
-  on every screen. BOTTOM + R turn sets the tempo, or the EVOLVE amount on
-  PERFORM. An L long press opens the screen menu and an L tap goes back.
-  TOP + BOTTOM opens and closes PERFORM. TRACK gets tabs.
+- **PR #4**, the new input core: debounce, tap, hold and long presses, the
+  RIGHT button as a shift, the LEFT + RIGHT chord.
+- **PR #5**, the screen router. RIGHT + L turn selects the track on every
+  screen. RIGHT + R turn sets the tempo, or the EVOLVE amount on PERFORM. An L
+  long press opens the screen menu and an L tap goes back. LEFT + RIGHT opens
+  and closes PERFORM. TRACK gets tabs.
 
 When the shell is wired in (phase 1c), this guide's tables and pictures change
 with it.
diff --git a/docs/UI-PLAN.md b/docs/UI-PLAN.md
index 033a2ed..b2b8be1 100644
--- a/docs/UI-PLAN.md
+++ b/docs/UI-PLAN.md
@@ -15,13 +15,15 @@ line-by-line, or vendored. Every line in `firmware/` and `parts/` is written fre
 
 ---
 
+> **Buttons, left and right (2026-11-07):** on the uO_C the two buttons sit side by side under the display. The code calls the **LEFT** button **TOP** (play/stop, as calibrated) and the **RIGHT** button **BOTTOM** (restart, and the shift in phase 1). Which pin each is comes from the calibration's FLIPCONTROLS flag (`firmware/oc_platform.cpp:567`, tables `firmware/oc_port.h:137-138`). This plan says LEFT/RIGHT; code names stay `BTN_TOP`/`BTN_BOT`.
+
 ## 0. Constraints (all phases)
 
 | | |
 |---|---|
 | Storage | **Read only.** `OcFlashRO` / `oc_storage.cpp` stays as it is. No flash or EEPROM writes, and no new persistence of UI state (last screen, macros, and so on). The CI grep in `.github/workflows/host-tests.yml` must stay green. **Do not name any new function or method `format`** (the grep matches `\.format\(`), and avoid the other grep tokens (`eraseFiles`, `lfs_remove`, …) even in comments. |
 | Pins | **No change to any pin's direction or mode.** No new `pinMode` calls on existing pins. `OC_INPUT_PINS`, `OC_OUTPUT_PIN_LIST`, `OC_PINOUT_*`, the DAC reset (pin 9) and the OLED/SPI pins stay untouched. The UI only *reads* the button and encoder pins that are already `INPUT_PULLUP`. |
-| Safe mode | Bottom button (pin 4 **or** 5) held at power-on means safe mode: 25 ms settle, then 3 samples 10 ms apart, all LOW (`setup()` in `oc_platform.cpp`). **Unchanged.** The crash-report pager in `oc_diag_crash()` (TOP = next page, BOTTOM = continue, fresh-press logic) is **unchanged**. The new input framework must also ignore any button already held when `ui_init()` runs, until it is released (the "boot-latched" rule), so a button held through boot never fires an action. |
+| Safe mode | Right button (BOTTOM in the code; pin 4 **or** 5) held at power-on means safe mode: 25 ms settle, then 3 samples 10 ms apart, all LOW (`setup()` in `oc_platform.cpp`). **Unchanged.** The crash-report pager in `oc_diag_crash()` (TOP = next page, BOTTOM = continue, fresh-press logic) is **unchanged**. The new input framework must also ignore any button already held when `ui_init()` runs, until it is released (the "boot-latched" rule), so a button held through boot never fires an action. |
 | ISR | The 60 µs core tick (`oc_tick`, `IntervalTimer`, priority 80) runs the engine. Nothing in the UI may lengthen it. The UI never blocks with interrupts off for more than one short critical section (one parameter apply). |
 | SPI | DAC and OLED share LPSPI4. The UI's flush takes `oc_spi_lock()`. While the lock is held, the tick's DAC writes are deferred (`oc_spi_pending`) and `oc_cv_service()` applies them afterwards. A longer flush means later CV and gate edges (see §5.4). |
 | Engine | No changes to `vendor/tessera` (submodule pin stays at `4e4af47`). The host engine test against `tessera_baseline.txt` must keep passing byte-for-byte. The UI talks to the engine only through `oc_port.h` / `parts/oc_view.c` accessors. New accessors are added to `parts/oc_view.c` (and declared in `oc_port.h`). |
@@ -84,10 +86,10 @@ A long press on the left encoder opens a small **screen menu** from anywhere.
 | **Right encoder** turn | change value under cursor (accelerated) | — | — |
 | **L push** | **BACK** (up one level; on OVERVIEW: no-op) | **MENU** (screen picker) | — |
 | **R push** | **ENTER / ACT** (open, toggle, confirm) | **CONTEXT** (reset value to default in TRACK; ROLL in PERFORM) | **R held + R turn = fine** (×1 when accel is on) |
-| **TOP button** | **PLAY / STOP** (unchanged), on press, at most 40 ms late if the chord window is used (`UI_CHORD_MS`, compile-time, so either §8 Q1 answer is a one-line change) | — | — |
-| **BOTTOM button** | **RESTART** on release (unchanged behaviour), only if it wasn't used as shift | — | **SHIFT**: BOTTOM + L turn = select track (any screen); BOTTOM + R turn = tempo (any screen; ignored when the clock is EXT); BOTTOM + R push = screen-specific secondary action |
-| **TOP + BOTTOM** chord | toggle **PERFORM** ⇄ previous screen. No transport action: TOP's play/stop waits ≤ 40 ms for a chord (§3.2), or see the alternative in §8 Q1 | | |
-| Boot | BOTTOM held at power-on = **SAFE MODE** (unchanged, before the UI exists) | | |
+| **LEFT button** (TOP in the code) | **PLAY / STOP** (unchanged), on press, at most 40 ms late if the chord window is used (`UI_CHORD_MS`, compile-time, so either §8 Q1 answer is a one-line change) | — | — |
+| **RIGHT button** (BOTTOM in the code) | **RESTART** on release (unchanged behaviour), only if it wasn't used as shift | — | **SHIFT**: RIGHT + L turn = select track (any screen); RIGHT + R turn = tempo (any screen; ignored when the clock is EXT); RIGHT + R push = screen-specific secondary action |
+| **LEFT + RIGHT** chord (TOP + BOTTOM in the code) | toggle **PERFORM** ⇄ previous screen. No transport action: LEFT's play/stop waits ≤ 40 ms for a chord (§3.2), or see the alternative in §8 Q1 | | |
+| Boot | RIGHT held at power-on = **SAFE MODE** (unchanged, before the UI exists) | | |
 
 Rationale (o_C idiom, design level): the left hand navigates and the right hand edits. Push to act,
 long press for the menu. A *physical* button doubles as shift and only fires its own action when it
@@ -127,11 +129,11 @@ HOLD_END, SHIFT_TURN(n)}` plus `CHORD_TB`. Screens implement
 
 * **Debounce:** 5 ms stable (5 consecutive 1 ms samples).
 * **Tap** = released < 400 ms with no turn and no other button. **Long** fires once at 600 ms while still held (not on release), so the menu appears without letting go. **Hold** start/end are also emitted (PERFORM's FILL uses them).
-* **Shift-consumed:** if an encoder turns or another control is pressed while BOTTOM is down, BOTTOM's release does **not** emit TAP (no restart).
-* **Chord TOP+BOTTOM:** both down within 40 ms. TOP's play/stop is deferred by up to 40 ms; if BOTTOM arrives in the window it's a chord (no transport action), otherwise TOP fires at 40 ms. *Alternative if 40 ms feels late:* fire on press and treat the chord as "TOP held ≥ 600 ms + BOTTOM". Yakov's call (see the questions in §8).
+* **Shift-consumed:** if an encoder turns or another control is pressed while RIGHT (BOTTOM) is down, its release does **not** emit TAP (no restart).
+* **Chord LEFT + RIGHT (TOP + BOTTOM in the code):** both down within 40 ms. LEFT's play/stop is deferred by up to 40 ms; if RIGHT arrives in the window it's a chord (no transport action), otherwise LEFT fires at 40 ms. *Alternative if 40 ms feels late:* fire on press and treat the chord as "TOP held ≥ 600 ms + BOTTOM". Yakov's call (see the questions in §8).
 * **Boot-latch:** controls that are down at `ui_init()` are latched and emit nothing until released.
 * **Accel:** detent interval < 20 ms → ×8, < 50 ms → ×4, < 100 ms → ×2, else ×1. It only applies when the value's range is over 24 steps (from `param_desc_t` min/max). Enum/name params are never accelerated. Holding R while turning forces ×1.
-* **Selection is explicit:** OVERVIEW's cursor is the selected track (moving it selects; that *is* the intent there). In TRACK, the left encoder never changes track. BOTTOM + L turn does, everywhere.
+* **Selection is explicit:** OVERVIEW's cursor is the selected track (moving it selects; that *is* the intent there). In TRACK, the left encoder never changes track. RIGHT + L turn does, everywhere.
 * **Reverse:** keep the `OC_ENC_REV_PINS_21_22` fix exactly as is. It moves into the shell's sampler, applied before the core sees steps.
 
 ### 3.3 Mockup — screen menu (L long press from anywhere)
@@ -163,7 +165,7 @@ explicitly.)
 ### 3.5 Acceptance criteria (phase 1)
 
 * All existing pages (HOME, GEN, SCL, CV, MOD, PROJ, re-homed as OVERVIEW / TRACK tabs / PROJECT) are reachable from the menu. The footer hint matches the real mapping on every screen.
-* TOP still plays/stops (with ≤ 40 ms chord window or immediate, per the decision). BOTTOM tap still restarts. BOTTOM used as shift never restarts.
+* LEFT (TOP) still plays/stops (with ≤ 40 ms chord window or immediate, per the decision). RIGHT (BOTTOM) tap still restarts. RIGHT used as shift never restarts.
 * A button held through boot does nothing until released. Safe mode and the crash pager behave exactly as on `9dc2ca4`.
 * Fast encoder spins during screen updates lose no detents (host test with a synthetic ring; on hardware, 20 fast detents give 20 steps).
 * No regressions in `oc_engine_test` (baseline) or `oc_port_test` (41 checks).
@@ -173,7 +175,7 @@ explicitly.)
 
 * The input state machine against scripted pin timelines: debounce (bounces < 5 ms ignored), tap vs long boundary (399/400/600 ms), long fires once, shift-consumed suppresses TAP, chord window (39 ms → chord, 41 ms → TOP then BOTTOM), boot-latch.
 * Accel table, and that enum params aren't accelerated.
-* Router: menu open/close, BACK at root is a no-op, stack depth bound, BOTTOM + L turn selects the track on every screen, and L turn in TRACK never changes `song.sel` (stubbed accessors).
+* Router: menu open/close, BACK at root is a no-op, stack depth bound, RIGHT + L turn selects the track on every screen, and L turn in TRACK never changes `song.sel` (stubbed accessors).
 * Reverse flag applied exactly once (keep the existing `check_enc_rev_21_22` cases passing).
 
 ---
@@ -222,7 +224,7 @@ tempo, the clock source and the selected track.
 * The note shown comes from a new accessor `oc_track_last_note(tr)` (volatile int16, set in `oc_note_on`; `-1` = none). It's rendered as a note name in the track's own octave space (the same mapping `oc_cv_write` uses: 0 = C2).
 
 **Input:** L turn = select track (cursor = `song.sel`). R tap = open TRACK for that track. R turn = change
-generator (as today's ROW_TRACK). R long = toggle mute (same as PERFORM). BOTTOM + R turn = tempo.
+generator (as today's ROW_TRACK). R long = toggle mute (same as PERFORM). RIGHT + R turn = tempo.
 L long = menu.
 
 **Acceptance:** all 4 lanes update at frame rate; a muted track shows `M` and an empty strip; ROLL pending
@@ -270,14 +272,14 @@ field is inverted:
 |4 ---  > ---         0          |
 ```
 
-* **Tabs:** with the cursor on row 0 ("tab row"), L turn moves the cursor into the rows. On the header, R turn switches tabs. *Simpler alternative, recommended:* **R tap on the header cycles the tab, and BOTTOM + R push cycles the tab from any row.**
+* **Tabs:** with the cursor on row 0 ("tab row"), L turn moves the cursor into the rows. On the header, R turn switches tabs. *Simpler alternative, recommended:* **R tap on the header cycles the tab, and RIGHT + R push cycles the tab from any row.**
 * **Range bar:** drawn for `F_INT`/`F_PCT` params with a range above 8. Enums show only the name.
 * **R long = reset to default** (`param_desc_t.def`), with a 1 s toast `"DEFAULT"`.
 * Generator change (MODE row) rebuilds the GEN rows immediately. The cursor clamps.
 * Add `oc_gen_rows` entries for the knobs the FM-1 exposes on the GEN page that are missing (e.g. `TX_POL_DIV3/4`, `TX_EUC_*` already present). That's an `oc_view.c` table change only, no engine change. Optional per §8.
 
 **Input:** L turn = row (or field on MOD); R turn = value (accel per §3.2); R hold + turn = fine; R long =
-default; R tap = act (ROLL row, enum next); L tap = back to OVERVIEW; BOTTOM + L turn = other track (stays
+default; R tap = act (ROLL row, enum next); L tap = back to OVERVIEW; RIGHT + L turn = other track (stays
 on the same tab and row if it exists).
 
 **Acceptance:** every row that `oc_*_rows` returns is editable from TRACK; tabs cycle; the default reset
@@ -326,9 +328,9 @@ ROLL exists (`tessera_seed_turn`). There's no fill concept on the port. No CV in
   | SERIES, CHORD, ANSWER, STEP | `TX_SUB_CHANCE` | verify `TX_SUB_CHANCE` applies on these tracks; if not, show `--` and disable |
 
   Values are written through `oc_tx_set` (the engine's clamp and IRQ-off section). The bar reads back with `oc_tx_get` normalised by the descriptor's range, so the display is always the truth.
-* **Probability macro (BOTTOM + R turn on PERFORM):** `TX_EV_AMOUNT` (EVOLVE amount) on generator tracks. *Note: this overrides the global "BOTTOM + R = tempo" on this one screen. Alternatively, keep tempo and drop this macro. See §8.*
+* **Probability macro (RIGHT + R turn on PERFORM):** `TX_EV_AMOUNT` (EVOLVE amount) on generator tracks. *Note: this overrides the global "RIGHT + R = tempo" on this one screen. Alternatively, keep tempo and drop this macro. See §8.*
 * **Fill (R held ≥ 300 ms, momentary):** while held, the selected track's density macro is pushed to 100 % (and `TX_SUB_RATCH` ×2 if Yakov wants ratchets). On release the exact previous values are restored. This is a pure UI override held in RAM. Restore must survive a screen change mid-hold (release is always delivered; the router forwards HOLD_END to the screen that got HOLD_START).
-* **Regenerate:** BOTTOM + R push = ROLL the selected track (`tessera_seed_turn(tr, +1)`). While playing it lands on the next bar (engine behaviour), and `~` blinks until `tessera.roll[tr]` clears. Needs a read-only accessor `oc_track_roll_pending(tr)`.
+* **Regenerate:** RIGHT + R push = ROLL the selected track (`tessera_seed_turn(tr, +1)`). While playing it lands on the next bar (engine behaviour), and `~` blinks until `tessera.roll[tr]` clears. Needs a read-only accessor `oc_track_roll_pending(tr)`.
 * **CV readouts:** outputs only in this phase. They come from a new `oc_cv_last_pitch(ch)` (volatile, written in `oc_cv_write`) and are shown as volts = pitch / 1536 (1 V/oct) or /15360 (0.1 V/oct), plus a note name. **CV inputs are not read by the firmware today.** Adding them needs ADC code, the T4.0 o_C CV-input pin map verified against hardware and Phazerville's documentation, and Acid Burn/ZC review. That's out of scope here (an optional phase 5b after Yakov's OK). Input pins would only be *read* (analog), but the pin assignment must be verified first.
 
 **Acceptance:** mute toggles within one frame and leaves no stuck gate (host test: note on → mute → gate 0
@@ -403,7 +405,7 @@ except the program.
 | Golden snapshots churn | One snapshot per screen state; the update flag is explicit; reviewers diff ASCII |
 | CI write-path grep false positives | Naming rule in §0 |
 | Flash/size growth | `pio run -t size` in CI; fonts are small (3×5 ≈ 200 B) |
-| TOP chord window feels laggy | 40 ms max; or the immediate-TOP alternative (§8 Q1) |
+| LEFT (TOP) chord window feels laggy | 40 ms max; or the immediate-TOP alternative (§8 Q1) |
 
 ---
 
@@ -416,8 +418,8 @@ storage PR. MIDI out. Engine changes. VOR/Plum variants. Pin remaps. Calibration
 
 ## 8. Open questions for Yakov
 
-1. **Buttons:** keep TOP = play/stop and BOTTOM = restart (and BOTTOM doubling as SHIFT, firing restart only on a clean tap)? And for the TOP + BOTTOM → PERFORM chord: delay TOP by ≤ 40 ms, or keep TOP instant and make the chord "hold TOP, press BOTTOM"?
+1. **Buttons:** keep LEFT (TOP) = play/stop and RIGHT (BOTTOM) = restart (and RIGHT doubling as SHIFT, firing restart only on a clean tap)? And for the LEFT + RIGHT → PERFORM chord: delay LEFT by ≤ 40 ms, or keep LEFT instant and make the chord "hold TOP, press BOTTOM"?
 2. **Fill:** what should FILL mean? Proposed: while held, density to 100 % (optionally ratchets ×2), then restore. Alternatives: ENERGY LEVEL up, or a one-bar fill.
 3. **CV inputs:** the firmware doesn't read the 4 CV ins at all today. Is outputs-only readout fine for now, or do you want phase 5b (reading CV ins: needs a pin-map check and review)?
 4. **Power-on screen:** OVERVIEW (proposed) or PERFORM?
-5. *(minor)* On PERFORM, should BOTTOM + R turn be tempo (global) or the probability/EVOLVE macro?
+5. *(minor)* On PERFORM, should RIGHT + R turn be tempo (global) or the probability/EVOLVE macro?
diff --git a/docs/img/panel.svg b/docs/img/panel.svg
index 9924dcb..5229546 100644
--- a/docs/img/panel.svg
+++ b/docs/img/panel.svg
@@ -2,13 +2,15 @@
 <!-- SPDX-License-Identifier: GPL-3.0-only -->
 <!-- Copyright (C) 2026 Yakov Khalinsky -->
 <!-- The uO_C rev A front panel (mxmxmx, 8HP), as Tessera-OC uses it: a schematic of the
-     controls and the jacks, not a to-scale drawing. docs/GUIDE.md explains each one. -->
-<svg xmlns="http://www.w3.org/2000/svg" width="240" height="680" viewBox="0 0 240 680"
+     controls and the jacks, top to bottom as on the panel, not a to-scale drawing.
+     docs/GUIDE.md explains each one. The firmware calls the LEFT button TOP and the
+     RIGHT button BOTTOM (as calibrated, firmware/oc_platform.cpp). -->
+<svg xmlns="http://www.w3.org/2000/svg" width="240" height="700" viewBox="0 0 240 700"
      font-family="DejaVu Sans Mono, Menlo, Consolas, monospace" text-anchor="middle">
   <title>Tessera-OC on the uO_C rev A: the front panel</title>
-  <rect x="20" y="10" width="200" height="660" rx="6" fill="#2b2b2b" stroke="#111" stroke-width="2"/>
+  <rect x="20" y="10" width="200" height="680" rx="6" fill="#2b2b2b" stroke="#111" stroke-width="2"/>
   <circle cx="40" cy="26" r="5" fill="#111"/>
-  <circle cx="200" cy="654" r="5" fill="#111"/>
+  <circle cx="200" cy="674" r="5" fill="#111"/>
   <text x="120" y="44" font-size="13" fill="#eee">TESSERA-OC</text>
   <text x="120" y="60" font-size="9" fill="#aaa">uO_C rev A</text>
 
@@ -17,41 +19,44 @@
   <text x="120" y="118" font-size="11" fill="#9cf">OLED</text>
   <text x="120" y="134" font-size="9" fill="#9cf">128 x 64</text>
 
-  <!-- the encoders: each turns and pushes -->
-  <circle cx="62" cy="226" r="26" fill="#555" stroke="#ccc" stroke-width="2"/>
-  <circle cx="62" cy="226" r="11" fill="#777"/>
-  <text x="62" y="272" font-size="10" fill="#eee">L ENC</text>
-  <text x="62" y="285" font-size="8" fill="#aaa">turn + push</text>
-  <circle cx="178" cy="226" r="26" fill="#555" stroke="#ccc" stroke-width="2"/>
-  <circle cx="178" cy="226" r="11" fill="#777"/>
-  <text x="178" y="272" font-size="10" fill="#eee">R ENC</text>
-  <text x="178" y="285" font-size="8" fill="#aaa">turn + push</text>
+  <!-- the two buttons, side by side under the display -->
+  <circle cx="70" cy="190" r="10" fill="#ddd" stroke="#888"/>
+  <text x="70" y="216" font-size="10" fill="#eee">LEFT</text>
+  <text x="70" y="229" font-size="8" fill="#aaa">play / stop</text>
+  <circle cx="170" cy="190" r="10" fill="#ddd" stroke="#888"/>
+  <text x="170" y="216" font-size="10" fill="#eee">RIGHT</text>
+  <text x="170" y="229" font-size="8" fill="#aaa">restart / shift*</text>
 
-  <!-- the two buttons between the encoders -->
-  <circle cx="120" cy="204" r="9" fill="#ddd" stroke="#888"/>
-  <text x="120" y="190" font-size="9" fill="#eee">TOP</text>
-  <circle cx="120" cy="250" r="9" fill="#ddd" stroke="#888"/>
-  <text x="120" y="273" font-size="9" fill="#eee">BOT</text>
+  <!-- the encoders: each turns and pushes -->
+  <circle cx="70" cy="276" r="26" fill="#555" stroke="#ccc" stroke-width="2"/>
+  <circle cx="70" cy="276" r="11" fill="#777"/>
+  <text x="70" y="322" font-size="10" fill="#eee">L ENC</text>
+  <text x="70" y="335" font-size="8" fill="#aaa">turn + push</text>
+  <circle cx="170" cy="276" r="26" fill="#555" stroke="#ccc" stroke-width="2"/>
+  <circle cx="170" cy="276" r="11" fill="#777"/>
+  <text x="170" y="322" font-size="10" fill="#eee">R ENC</text>
+  <text x="170" y="335" font-size="8" fill="#aaa">turn + push</text>
 
   <!-- the jacks: three rows of four -->
   <g font-size="10" fill="#eee">
-    <text x="120" y="330" font-size="9" fill="#aaa">TRIGGER IN</text>
-    <circle cx="48" cy="360" r="14" fill="#999"/><circle cx="48" cy="360" r="6" fill="#111"/><text x="48" y="390">TR1</text>
-    <circle cx="96" cy="360" r="14" fill="#999"/><circle cx="96" cy="360" r="6" fill="#111"/><text x="96" y="390">TR2</text>
-    <circle cx="144" cy="360" r="14" fill="#999"/><circle cx="144" cy="360" r="6" fill="#111"/><text x="144" y="390">TR3</text>
-    <circle cx="192" cy="360" r="14" fill="#999"/><circle cx="192" cy="360" r="6" fill="#111"/><text x="192" y="390">TR4</text>
+    <text x="120" y="370" font-size="9" fill="#aaa">TRIGGER IN</text>
+    <circle cx="48" cy="398" r="14" fill="#999"/><circle cx="48" cy="398" r="6" fill="#111"/><text x="48" y="428">TR1</text>
+    <circle cx="96" cy="398" r="14" fill="#999"/><circle cx="96" cy="398" r="6" fill="#111"/><text x="96" y="428">TR2</text>
+    <circle cx="144" cy="398" r="14" fill="#999"/><circle cx="144" cy="398" r="6" fill="#111"/><text x="144" y="428">TR3</text>
+    <circle cx="192" cy="398" r="14" fill="#999"/><circle cx="192" cy="398" r="6" fill="#111"/><text x="192" y="428">TR4</text>
 
-    <text x="120" y="430" font-size="9" fill="#aaa">CV IN</text>
-    <circle cx="48" cy="460" r="14" fill="#999"/><circle cx="48" cy="460" r="6" fill="#111"/><text x="48" y="490">CV1</text>
-    <circle cx="96" cy="460" r="14" fill="#999"/><circle cx="96" cy="460" r="6" fill="#111"/><text x="96" y="490">CV2</text>
-    <circle cx="144" cy="460" r="14" fill="#999"/><circle cx="144" cy="460" r="6" fill="#111"/><text x="144" y="490">CV3</text>
-    <circle cx="192" cy="460" r="14" fill="#999"/><circle cx="192" cy="460" r="6" fill="#111"/><text x="192" y="490">CV4</text>
+    <text x="120" y="466" font-size="9" fill="#aaa">CV IN</text>
+    <circle cx="48" cy="494" r="14" fill="#999"/><circle cx="48" cy="494" r="6" fill="#111"/><text x="48" y="524">CV1</text>
+    <circle cx="96" cy="494" r="14" fill="#999"/><circle cx="96" cy="494" r="6" fill="#111"/><text x="96" y="524">CV2</text>
+    <circle cx="144" cy="494" r="14" fill="#999"/><circle cx="144" cy="494" r="6" fill="#111"/><text x="144" y="524">CV3</text>
+    <circle cx="192" cy="494" r="14" fill="#999"/><circle cx="192" cy="494" r="6" fill="#111"/><text x="192" y="524">CV4</text>
 
-    <text x="120" y="530" font-size="9" fill="#aaa">CV OUT (TRACKS 1-4)</text>
-    <circle cx="48" cy="560" r="14" fill="#c93"/><circle cx="48" cy="560" r="6" fill="#111"/><text x="48" y="590">A</text>
-    <circle cx="96" cy="560" r="14" fill="#c93"/><circle cx="96" cy="560" r="6" fill="#111"/><text x="96" y="590">B</text>
-    <circle cx="144" cy="560" r="14" fill="#c93"/><circle cx="144" cy="560" r="6" fill="#111"/><text x="144" y="590">C</text>
-    <circle cx="192" cy="560" r="14" fill="#c93"/><circle cx="192" cy="560" r="6" fill="#111"/><text x="192" y="590">D</text>
+    <text x="120" y="562" font-size="9" fill="#aaa">CV OUT (TRACKS 1-4)</text>
+    <circle cx="48" cy="590" r="14" fill="#c93"/><circle cx="48" cy="590" r="6" fill="#111"/><text x="48" y="620">A</text>
+    <circle cx="96" cy="590" r="14" fill="#c93"/><circle cx="96" cy="590" r="6" fill="#111"/><text x="96" y="620">B</text>
+    <circle cx="144" cy="590" r="14" fill="#c93"/><circle cx="144" cy="590" r="6" fill="#111"/><text x="144" y="620">C</text>
+    <circle cx="192" cy="590" r="14" fill="#c93"/><circle cx="192" cy="590" r="6" fill="#111"/><text x="192" y="620">D</text>
   </g>
-  <text x="120" y="630" font-size="8" fill="#aaa">schematic, not to scale</text>
+  <text x="120" y="646" font-size="7" fill="#aaa">* shift: once the phase 1 UI is wired in</text>
+  <text x="120" y="660" font-size="8" fill="#aaa">schematic, not to scale</text>
 </svg>
````

## Appendix B: `build/check.sh` (write exactly)
````sh
#!/bin/sh
# tessera-panel-layout-1 checks (rev 2: UI-PLAN in scope). Every check prints NAME_EXIT=n, 0 = pass. Run from the clone root.
cd "$(dirname "$0")/.." || exit 1
B=build/layoutcheck
rm -rf "$B" && mkdir -p "$B"

tools/build_host.sh > "$B/host.log" 2>&1; echo BUILD_HOST_EXIT=$?
grep -E '^(render_screens: |oc_port: [0-9]+ checks|oc_ui: [0-9]+ checks|oc_nav: [0-9]+ checks)' "$B/host.log"
md5sum docs/img/screen-*.png docs/img/panel.svg docs/GUIDE.md docs/UI-PLAN.md CHANGELOG.md
python3 -c "import xml.dom.minidom; xml.dom.minidom.parse('docs/img/panel.svg')"; echo SVG_EXIT=$?
grep -c -E 'LEFT|RIGHT' docs/img/panel.svg
test -z "$(grep -nE '\*\*TOP button|\*\*BOTTOM button|\| \*\*TOP\*\* \||\| \*\*BOTTOM\*\* \|' docs/GUIDE.md)"; echo NO_TOP_BOTTOM_LABELS_EXIT=$?
grep -c 'TOP in the code\|BOTTOM in the code' docs/GUIDE.md
grep -c 'firmware/oc_platform.cpp:567' docs/GUIDE.md
grep -c 'firmware/oc_port.h:137-138' docs/GUIDE.md
test -z "$(grep -nE 'BOTTOM \+ [LR] |\*\*TOP button|\*\*BOTTOM button' docs/UI-PLAN.md)"; echo PLAN_LEFT_RIGHT_EXIT=$?
test -z "$(grep -rli format docs/GUIDE.md docs/img/panel.svg)"; echo NOFORMAT_EXIT=$?

# the tree: only the four edited files differ from fix/render-check, nothing untracked
git diff --numstat origin/fix/render-check
test -z "$(git status --porcelain | grep -v '^ M ')"; echo TREE_EXIT=$?
````

## Appendix C: `build/check.expected` (write exactly; it is the script's whole output)
````text
BUILD_HOST_EXIT=0
render_screens: docs/img current
oc_port: 41 checks, 0 failing
oc_ui: 51 checks, 0 failing
oc_nav: 42 checks, 0 failing
153c57c2144eb17598036b986a923eef  docs/img/screen-crash-1.png
ce96c4d3977b3220b24b7b6d5853ae4d  docs/img/screen-crash-2.png
09e67e82abada246410a8fbd1c92bf91  docs/img/screen-home.png
ed414c32883f49ef3bcfebbcc0db7de3  docs/img/screen-reset.png
d8de9abb18daa439509001af647fbb54  docs/img/screen-safe-mode.png
69c299e94492cb9f9818ba6fd6b54ed4  docs/img/screen-splash.png
ae2417dd83b40bb6b3a09889d311bd19  docs/img/panel.svg
f2f709149362cc2078d3632dfd8655e0  docs/GUIDE.md
a07f8a0988e6c023dfb4ff213625c126  docs/UI-PLAN.md
45a9ba90ab1d617c2a0a6d9ff6a8a2ba  CHANGELOG.md
SVG_EXIT=0
4
NO_TOP_BOTTOM_LABELS_EXIT=0
2
1
1
PLAN_LEFT_RIGHT_EXIT=0
NOFORMAT_EXIT=0
1	0	CHANGELOG.md
25	17	docs/GUIDE.md
20	18	docs/UI-PLAN.md
39	34	docs/img/panel.svg
TREE_EXIT=0
````
A denied command means stop, write what you needed and why in the exit report, and end. Never retry through another binary, interpreter or flag.
A failed check, or a result that does not match an expected value in the brief, means: write the exit report and stop BEFORE committing. Do not explain it away and carry on.
Never flash, upload to or reflash any hardware; that needs Yakov's own go. Never merge, mark a PR ready, tag or release. Pull requests are drafts only (gh pr create --draft), and only when the brief asks.
Push only the job branch the brief names, with an explicit refspec (git push origin HEAD:<job-branch>); never push to main or master. Work only in this clone: no git worktree, no git clone, no git -C outside it.
Do not add a Co-Authored-By trailer (or any other trailer) to commits. The commit identity is set for you; do not change git config.
On any stop, for any reason, print one line that starts exactly `STOP: <reason>` (in the exit report and your final message); never print such a line otherwise. A STOP line always means human review.
Print check markers as `<NAME>_EXIT=$?` with 0 meaning pass (for a grep that must find nothing, use test -z "$(grep ...)"). A non-zero marker before a commit means stop, not commit.
Never probe credentials of any kind: no gh auth (status, token or any other), no git credential, no git config reads of credential or list/regexp, no ssh to github.com, no ~/.ssh, ~/.git-credentials, ~/.config/gh or *_TOKEN. A rejected push means STOP and report the exact error.
