<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Yakov Khalinsky -->
# Tessera-OC: the panel and the controls

This guide covers the firmware on `main`, release **v0.1.2**. It describes only
what that firmware does today. The new controls that phase 1 of the UI plan
([UI-PLAN.md](UI-PLAN.md)) brings are listed at the end as *coming*.

The screen pictures are not mock-ups. `tools/render_screens` builds the
firmware's own drawing code (`firmware/oc_display.cpp`, `oc_ui.cpp`,
`oc_diag.cpp`) and the engine for the host, and writes the display's 128 × 64
pixels out at three times the size. `tools/build_host.sh`, which CI runs on
every push, renders them again and fails if they differ from the files here
(see the end of this guide).

## The panel

![The uO_C rev A front panel, as Tessera-OC uses it](img/panel.svg)

| Control or jack | What it does on `main` |
|---|---|
| **L encoder, turn** | moves the cursor up and down the list (it wraps round) |
| **L encoder, push** | nothing yet |
| **R encoder, turn** | changes the value in the row under the cursor |
| **R encoder, push** | nothing yet |
| **TOP button** | play / stop |
| **BOTTOM button** | restart from the top. Held at power-on, it starts the **safe mode** |
| **TR1** | clock in: the rig's clock, counted on the falling edge |
| **TR2 – TR4** | read, but not used |
| **CV1 – CV4** | not used |
| **A – D** | the CV outs, tracks 1 – 4: the pitch at 1 V an octave, with the gate riding on the same CV (see the README's *The outputs*) |

The module has no gate jacks, no digital outs and no MIDI out.

## Power-on

![The splash](img/screen-splash.png)

At power-on the module shows these screens in order:

1. If the framework has a **crash report**, the crash pager comes first (see
   below). Otherwise, if the last boot stopped part of the way through, the
   module shows `RESET AT STAGE n` for five seconds.
2. The **splash**: the module's name and the firmware's version.
3. **HOME**. If nothing is saved, the module composes for itself: track 1 gets
   TURING, 2 gets EUCLID, 3 MARKOV and 4 MORPH, an octave apart, and it plays.

## HOME

![HOME after a power-on with nothing saved](img/screen-home.png)

The top line shows the page, the selected track and the tempo. The bottom line
is the footer.

| Row | R encoder turn |
|---|---|
| `TEMPO` | 1 BPM per detent, from 40 to 240 BPM |
| `TRACK 1` – `TRACK 4` | changes that track's generator (STEP, TURING, EUCLID, MARKOV, MORPH, ATLAS, BOUNCE, AWAKE, POLY, QUAD, CONSTL, SERIES, CHORD, ANSWER). It stops at the ends of the list |
| `CLOCK` | nothing: it only shows `INT` (the module's own tempo) or `EXT` (following TR1) |

When the L encoder's cursor lands on a `TRACK` row, that track becomes the
selected one (`T1` – `T4` on the top line).

The footer still says `T:PAGE`, but on `main` the TOP button plays and stops.
The other pages in the code (GEN, SCL, CV, MOD, PROJ) cannot be reached from the
panel yet.

## The safe mode

![The safe mode's screen](img/screen-safe-mode.png)

To start in safe mode, hold the **BOTTOM** button while you power on. This is
what `setup()` in `firmware/oc_platform.cpp` does:

- After the button pins get their pull-ups, it waits **25 ms** for them to settle.
- It then reads the pins **3 times, 10 ms apart**. A button counts as held only if
  its pin reads low on **all three** samples.
- The calibration, which says which button is BOTTOM, has not been read yet. So
  either of the two button pins (4 and 5) is accepted. In practice either button
  held at power-on starts the safe mode.

In the safe mode the module runs:

- the calibration;
- the CV outs at 0 V;
- the display.

The engine, the storage, the USB MIDI and the tick do not run. The screen is
redrawn four times a second. It shows the version, the stage the previous run
reached, the boot count, the longest engine block and whether the storage is
there (it is always read only).

No control does anything in the safe mode. Power the module off and on to leave it.

## The crash pager

![The crash report, page 1](img/screen-crash-1.png)
![The crash report, page 2](img/screen-crash-2.png)

If the module faulted in the run before, `oc_diag_crash()` in
`firmware/oc_diag.cpp` shows the framework's crash report at power-on, before the
splash. The first line is the stage the previous run reached. After that comes
the report, seven lines to a page.

| Button | In the crash pager |
|---|---|
| **TOP** | the next page (after the last page it goes back to the first) |
| **BOTTOM** | go on with the boot |

The pager first waits until both buttons are released, so a button already held
at power-on does not count. After that each press has to be released before the
next one counts. There is no timeout, on purpose: a module in a reboot loop
stops on a screen that says what happened. The report text in the pictures is an
example written in the framework's own layout.

![A reset part of the way through the boot, with no report](img/screen-reset.png)

With no report but a boot that stopped part of the way through, the screen shows
`RESET AT STAGE n` for five seconds and the boot goes on. These are the stages:

| Stage | Step |
|---|---|
| 10 | setup |
| 11 | the pins |
| 12 | the SPI |
| 13 | the DAC |
| 14 | the calibration |
| 15 | the display |
| 16 | the splash |
| 17 | the engine |
| 18 | the storage |
| 19 | the project |
| 20 | the UI |
| 21 | the USB MIDI |
| 22 | the timer |
| 23 | done |
| 99 | running |

## Coming: phase 1 of the UI (PR #4 / #5)

These are **not on the module yet**:

- **PR #4** (merged) adds the new input core: debounce, tap, hold and long
  presses, BOTTOM as a shift, the TOP + BOTTOM chord. It is host-tested only.
  The firmware does not use it yet.
- **PR #5** (a draft) adds the screen router. BOTTOM + L turn selects the track
  on every screen. BOTTOM + R turn sets the tempo, or the EVOLVE amount on
  PERFORM. An L long press opens the screen menu and an L tap goes back.
  TOP + BOTTOM opens and closes PERFORM. TRACK gets tabs.

When the shell is wired in (phase 1c), this guide's tables and pictures change
with it.

## Regenerating the pictures

```sh
tools/build_host.sh build      # the engine's generated tables (build/gen)
tools/render_screens.sh        # writes docs/img/screen-*.png
```

The pictures come from the code, so a change to a screen means rendering them
again and committing the new PNGs. `tools/build_host.sh` (and so CI's host job)
renders them into `build/render_check` and stops with
`docs/img stale: run tools/render_screens.sh` if one differs from `docs/img`.
The panel drawing (`docs/img/panel.svg`) is drawn by hand.
