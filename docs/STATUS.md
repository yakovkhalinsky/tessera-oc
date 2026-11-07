# Status

Last updated 2026-11-07. Update this file at the start and end of every job (see
`CLAUDE.md`).

## Where it stands

- **v0.1.2** boots and runs on Yakov's uO_C, and the tracks show. The left
  encoder fix is confirmed on hardware (main build `c96d1ce`).
- **UI plan:** `docs/UI-PLAN.md`. Yakov kept every default in its §8
  questions:
  - The left button (`TOP`) is play/stop. The right button (`BOTTOM`) is
    restart on a clean tap, and shift when held.
  - To open PERFORM, hold the left button, then press the right.
  - FILL sets density to 100 % while held.
  - Only the CV outputs are shown.
  - The module powers on to OVERVIEW.
  - On PERFORM, BOTTOM + R changes the EVOLVE amount; on other screens it
    changes the tempo.

## Phases

| Phase | What | State |
|---|---|---|
| 1a | the input core (debounce, tap/long/hold, shift, chord, accel) | merged, PR #4 (`c96d1ce`) |
| 1b | the screen router, and shift read at the moment of the press | merged, PR #5 (`57f4e07`) |
| guide | `docs/GUIDE.md`, the panel SVG, framebuffer PNG renders, a CI staleness check | merged, PR #6 (`4088c02`) |
| tools | render check, two-way name compare, orphan PNG test | draft, PR #7 |
| guide fix | the panel SVG shows the buttons stacked vertically between the encoders. Redraw them side by side above the encoders, and label them left (play/stop) and right (restart/shift) | to do |
| 1c | wire 1a/1b into the firmware (L/R push, control map, screen menu, existing pages re-homed). The first phase with a hex worth flashing | next |
| 2 | the drawing kit, 3x5 font, golden snapshots | to do |
| 3 | OVERVIEW lanes | to do |
| 4 | the TRACK edit screen | to do |
| 5 | PERFORM | to do |
| 6 | polish and timing | to do |
| 5b | read the CV inputs (optional, needs Yakov's OK) | not started |

## Open questions for Yakov (none of them block work)

- The module's HP width.
- Whether the Phazerville Flip and L-reversed settings are on.
- Whether to mark the v0.1.0 release as broken.
