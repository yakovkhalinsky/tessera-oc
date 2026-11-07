# CLAUDE.md: start here

This file and the docs it points to are enough to keep building tessera-oc
standalone in Claude Code, with no outside agents. Read them in this order:

1. `docs/STATUS.md`: where the work is right now, what's next, and what's open.
2. `docs/UI-PLAN.md`: the UI plan (phases, design, acceptance criteria, risks).
3. `docs/GUIDE.md`: the panel and controls guide for users.
4. `PORT-PLAN.md`, `README.md`, `CHANGELOG.md`: the port's background and history.
5. `docs/briefs/`: the brief for each job that has been run, newest last.

## The rule: write it down first

Before you start any job, save its brief as `docs/briefs/YYYY-MM-DD-<phase>.md`
and update `docs/STATUS.md` (what you're about to do, and on which branch). When
the job finishes, update `docs/STATUS.md` again with the result, the PR, the
commit and the next step. If a session dies halfway, the next one picks up from
these files.

## Hardware (Yakov's module)

- An original **mxmxmx uO_C rev A**. The PCB reads "origin uo_c by mxmxmx". It
  has a Teensy 4.0 and no VOR jack or switch. The panel reads µO_C.
- The panel, top to bottom:
  - the OLED (SH1106, 128x64)
  - two buttons side by side
  - two encoders, L and R
  - trigger inputs 1–4
  - CV inputs 1–4
  - CV outputs A–D
- The **left button** is play/stop; the code calls it `TOP`. The **right
  button** is restart and shift; the code calls it `BOTTOM`.
- The left encoder's shaft (pins 21/22) reads reversed. The build corrects it
  with `-DOC_ENC_REV_PINS_21_22=1`. The right encoder is fine.
- Pin 9 is the DAC reset on this board, which is safe.
- The button pins 4/5 need about 11 ms to settle after their pull-ups switch on.
  Boot waits 25 ms, then reads 3 samples, because a shorter wait gave false safe
  modes.
- The known-good fallback firmware is Phazerville T40. Calibration survives a
  reflash.

## Build, test, flash

```sh
git submodule update --init --recursive   # vendor/tessera is a submodule; the build fails without it
tools/build_host.sh                        # host tests
pip install platformio && pio run -e T40   # device build -> .pio/build/T40/firmware.hex
pio run -e T40 -t size
python3 tools/check_sections.py .pio/build/T40/firmware.elf
```

`[env:T40]` already sets `-DOC_HW_UOC_A`. Flash the hex with the Teensy Loader.
Only Yakov flashes, on his own computer: never with a Teensy attached to a build
host.

## Working rules

- Work on a fresh branch from `main` and open a **draft** PR with CI green. The
  PR body has a summary, screenshots, test counts, hardware test steps for Yakov
  (what to press, what to see) and the risks.
- Merge only after Yakov OKs it, and use **merge commits** (no squash, no
  rebase-merge). Never force-push. `main` is protected.
- Commit as `Yakov <646086+yakovkhalinsky@users.noreply.github.com>`.
- Storage stays read-only: nothing saves to the module until a separate,
  reviewed storage PR.
- No engine changes: the port has to play exactly what the FM-1 plays (see
  `vendor/tessera/tests/tessera_baseline.txt`).
- The engine runs in the 60 µs ISR. UI writes go only through
  `tessera_param_set` or single `int16` stores.
- A display redraw holds SPI for about 1–1.5 ms, which delays the DAC. Keep
  flushes sliced.
