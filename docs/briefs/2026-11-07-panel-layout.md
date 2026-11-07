# Job panel-layout-2: the panel layout, LEFT and RIGHT buttons (tight brief)

This brief is committed in the repo as `docs/briefs/2026-11-07-panel-layout.md`, next to the three files it uses: `docs/briefs/2026-11-07-panel-layout.patch` (the edits), `.check.sh` (the checks) and `.check.expected` (their exact output). It is enough on its own to run the job in plain Claude Code, from a fresh clone of `yakovkhalinsky/tessera-oc`. You apply one patch, run one script and compare its output. You design nothing.

## Repo rules (always)
* Pull requests are **drafts** only, and this job opens none. Never merge, mark ready, tag or release; Yakov merges, with merge commits.
* Never flash, upload or reflash hardware, and never with a Teensy attached to the build host. Only Yakov flashes, on his own computer.
* Push only the job branch, `docs/panel-layout-2`, and only as `git push origin HEAD:docs/panel-layout-2`. Never push to `main`, never force-push.
* Commits are Yakov's (the identity is set for you; do not run `git config`), with no trailer of any kind.
* Storage stays read-only and the engine is not touched; this job is docs only.
* Read only files inside this clone. Do not save memory, notes or any file outside the clone.
* `docs/briefs/` is read-only: never write, edit, move or delete anything there, and never redirect output into it.
* No credential, token, ssh or config probing of any kind. No `gh` command at all.

## The rule for any problem (read first)
* Every check prints a marker `<NAME>_EXIT=<n>`; 0 means pass. If a marker is not 0, a value differs from this brief, or a step fails, write the exit report (step 8) whose first line starts exactly `STOP: <reason>`, end, and do not commit or push. Never print a line starting with `STOP:` otherwise.
* Do not reason a difference away, re-run a check differently, rebuild the patch, or change a file to make a check pass. There is nothing to copy by hand in this job, so there is no copying slip to fix.
* A rejected push means a `STOP:` line with the exact error, and end.

## Scope (Yakov: the panel guide's button locations were wrong)
* `docs/img/panel.svg`: the real uO_C rev A panel, top to bottom: the OLED; the LEFT and RIGHT buttons side by side under it; the L and R encoders; TR1-4; CV1-4; A-D.
* `docs/GUIDE.md`: every button named LEFT/RIGHT (LEFT = play/stop, RIGHT = restart/shift, as calibrated), with one note that the code calls them TOP/BOTTOM, citing `firmware/oc_platform.cpp` and `firmware/oc_port.h` for where the calibration sets it.
* `docs/UI-PLAN.md`: the PERFORM chord and the shift wording say LEFT/RIGHT.
* `CHANGELOG.md`: one line.
* `docs/STATUS.md`: the guide fix row, at the end (step 6), in its own commit.
* Nothing else: 0 lines in `.github/`, `firmware/`, `parts/`, `tests/`, `vendor/`, `tools/`, `docs/img/*.png`, `docs/briefs/`.

## Hard budget (exact)
The layout commit, `git diff --numstat origin/docs/panel-layout-2 HEAD` after step 5:
```
1	0	CHANGELOG.md
25	17	docs/GUIDE.md
20	18	docs/UI-PLAN.md
39	34	docs/img/panel.svg
```
The STATUS commit: `1	1	docs/STATUS.md`.

## Do (in this order, nothing else)
1. **Branch** (the pinned base):
   ```sh
   git checkout -b docs/panel-layout-2 origin/docs/panel-layout-2
   git submodule update --init --recursive
   git rev-parse --short=7 HEAD~1                     # must print 947ad82
   git log -1 --format=%s                             # must print: ops: the panel-layout job files; STATUS in progress
   test "$(git rev-parse HEAD)" = "$(git rev-parse origin/docs/panel-layout-2)"; echo BASE_EXIT=$?
   ```
   `BASE_EXIT=0` is required.
2. **Pinned files first**, before anything is written: `md5sum docs/briefs/2026-11-07-panel-layout.patch docs/briefs/2026-11-07-panel-layout.check.sh docs/briefs/2026-11-07-panel-layout.check.expected` must print exactly:
   ```
   bf6831920ea82f71ad8e268ba96a5b0e  docs/briefs/2026-11-07-panel-layout.patch
   6c2064ad1ea0b24a439736125bca8a39  docs/briefs/2026-11-07-panel-layout.check.sh
   448f95f1e29b082726ddfc607d5852ce  docs/briefs/2026-11-07-panel-layout.check.expected
   ```
3. **Patch, checked then applied:** `git apply --check docs/briefs/2026-11-07-panel-layout.patch; echo APPLY_CHECK_EXIT=$?` must print `APPLY_CHECK_EXIT=0`. If it does not: STOP (no rebuild, no other way). Only then: `git apply docs/briefs/2026-11-07-panel-layout.patch; echo APPLY_EXIT=$?` must print `APPLY_EXIT=0`.
4. **Checks** (about 2 minutes; give it a 600 s timeout): `sh docs/briefs/2026-11-07-panel-layout.check.sh; echo CHECK_SH_EXIT=$?` must print `CHECK_SH_EXIT=0`. The script writes its whole output to `build/check.out`. Then `diff docs/briefs/2026-11-07-panel-layout.check.expected build/check.out; echo DIFF_EXIT=$?` must print no diff lines and `DIFF_EXIT=0`. The script builds and runs the host tests (with the render check), prints the md5 of the screens and the edited files, parses the SVG, checks the LEFT/RIGHT wording and the citations, checks that `docs/briefs/` is unchanged, and prints the numstat.
5. **Layout commit** (one): `git add CHANGELOG.md docs/GUIDE.md docs/UI-PLAN.md docs/img/panel.svg && git commit -m "docs: the panel layout, LEFT and RIGHT buttons"; echo COMMIT_EXIT=$?`. Then, on the commit: `tools/build_host.sh > build/after.log 2>&1; echo AFTER_COMMIT_EXIT=$?` must print `AFTER_COMMIT_EXIT=0`, `grep -c '^render_screens: docs/img current' build/after.log` must print `1`, and `test -z "$(git status --porcelain)"; echo TREE_EXIT=$?` must print `TREE_EXIT=0`. If any differs: a `STOP:` line, no push.
6. **STATUS row, its own commit:**
   ```sh
   S=$(git rev-parse --short=7 HEAD)
   sed -i 's/| in progress: job panel-layout-2 |/| in review: job panel-layout-2, layout commit '"$S"'; draft PR to follow |/' docs/STATUS.md
   git diff --numstat                                  # must print: 1	1	docs/STATUS.md
   grep -c "| in review: job panel-layout-2, layout commit $S; draft PR to follow |" docs/STATUS.md   # must print 1
   git add docs/STATUS.md && git commit -m "docs: STATUS, panel-layout-2 result"; echo STATUS_COMMIT_EXIT=$?
   git diff --numstat HEAD~1 HEAD                      # must print: 1	1	docs/STATUS.md
   ```
7. **Push** (both commits at once): `git push origin HEAD:docs/panel-layout-2; echo PUSH_EXIT=$?` must print `PUSH_EXIT=0`.
8. **Exit report:** write it to `build/panel-layout-2-exit-report.md` and repeat it as your final message: the two new SHAs, both numstats, every marker line and the diff result, and anything that went wrong (a denial, a difference). On a problem its first line is `STOP: <reason>`.
