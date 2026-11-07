<!-- docs/briefs/2026-11-07-render-check.md -->
> **Archived job brief: render-check** (the render check both ways, orphan PNG test, Linux-only skip, save_png frees). Run on 2026-11-07 in Claude Code; this is the brief as run, with build-host-only paths replaced. Result: PR #7, merged (`947ad82`; branch head `fa6a791`).
> The commands below were for that moment (base commits, expected outputs); to redo the work, read `docs/STATUS.md` first and branch from today's `main`.
>
> **Repo rules (apply to every job):** work on the job branch named below and push only that branch, with an explicit refspec; pull requests are **drafts** only; never merge, mark ready, tag or release (Yakov merges, with merge commits); never flash, upload or reflash hardware, and never with a Teensy attached to the build host (pizzabox), since only Yakov flashes, on his own computer; storage stays read-only; no engine changes; commit as `Yakov <646086+yakovkhalinsky@users.noreply.github.com>` with no trailers; never probe credentials. See `CLAUDE.md`.

# Claude Code job tessera-render-check-1: the render check, both ways (tight brief)

You are in a fresh clone of `yakovkhalinsky/tessera-oc` (your working directory). Everything you need is in this brief. You copy three files in, run one script and compare its output; you design nothing.

## The rule for any problem (read first)
* Every check prints a marker `<NAME>_EXIT=<n>`; 0 means pass. The script's whole output must equal Appendix C byte for byte. If anything differs, or any step below fails, write the exit report (step 7) whose first line starts exactly `STOP: <reason>`, end, and do not commit or push. Never print a line starting with `STOP:` otherwise.
* Do not reason a difference away, edit the script, re-run a check differently or change a file to make it pass. The only allowed fix is a copying slip (your copy of an appendix differs from the appendix): fix it once.
* A rejected push means a `STOP:` line with the exact error, and end. No credential, token, ssh or config probing of any kind. No `gh` command at all.

## Branching
```sh
git checkout -b fix/render-check origin/main
git submodule update --init --recursive
git log -1 --format=%h    # must print 4088c02
```
Your job branch is **`fix/render-check`**. Push it only as `git push origin HEAD:fix/render-check`. No other ref, no PR.

## Scope (Acid Burn's review of PR #6: the MUST and both SHOULDs)
* `tools/build_host.sh` lines 24-31 (the render check block) become lines 24-39: clear `build/render_check` before the render; fail when a committed `docs/img/screen-*.png` has no render, when a render is not committed, or when the bytes differ, each with its own `docs/img stale: run tools/render_screens.sh (...)` message; skip the check with `render_screens: check skipped (the renderer runs on Linux only)` when `uname -s` is not Linux.
* `tools/render_screens/rs_main.cpp` in `save_png()`: lines 140-141 (`malloc` failed) and lines 172-173 (`fopen` failed) free `raw` and `z` before returning.
* `docs/GUIDE.md` lines 164-165 ("Regenerating the pictures"): the check's three cases and Linux only.
* `CHANGELOG.md` line 9: the guide bullet's last sentence.
* Nothing else: 0 lines in `.github/`, `firmware/`, `parts/`, `tests/`, `vendor/`, `docs/img/`.

## Hard budget (`git diff --numstat origin/main`; exact)
```
1	1	CHANGELOG.md
5	2	docs/GUIDE.md
10	2	tools/build_host.sh
6	2	tools/render_screens/rs_main.cpp
```

## Do (in this order, nothing else)
1. The branching commands above.
2. `mkdir -p build`. Write Appendix A with the Write tool to `build/fix.diff`, then `git apply --check build/fix.diff && git apply build/fix.diff; echo APPLY_EXIT=$?` (must print `APPLY_EXIT=0`).
3. Write Appendix B with the Write tool to `build/check.sh` and Appendix C to `build/check.expected`, each exactly. Then `md5sum build/fix.diff build/check.sh build/check.expected` must print exactly:
   ```
   c23a263ca6168d0e1fa0626720de788a  build/fix.diff
   8ad3ed555cc4f64dec1100c7c5fbff35  build/check.sh
   5b7c789fdd2f85ff2fa07fb040b927ad  build/check.expected
   ```
4. **Checks** (about 6 minutes; one command, give it a 900 s timeout): `sh build/check.sh > build/check.out 2>&1; echo CHECK_SH_EXIT=$?; diff build/check.expected build/check.out; echo DIFF_EXIT=$?`. It must print `CHECK_SH_EXIT=0`, no diff lines, and `DIFF_EXIT=0`. The script builds and runs the host tests, makes a stale, an orphan and a missing PNG and a leftover render in turn (restoring each), shims `uname` to test the off-Linux skip, and runs the device build and the read-only checks.
5. **Commit** (one): `git add CHANGELOG.md docs/GUIDE.md tools/build_host.sh tools/render_screens/rs_main.cpp && git commit -m "tools: render check, two-way name compare, orphan PNG test"; echo COMMIT_EXIT=$?`. No trailer. Do not run `git config`. Then, on the commit: `tools/build_host.sh > build/after.log 2>&1; echo AFTER_COMMIT_EXIT=$?` must print `AFTER_COMMIT_EXIT=0`, `grep -c '^render_screens: docs/img current' build/after.log` must print `1`, and `test -z "$(git status --porcelain)"; echo TREE_EXIT=$?` must print `TREE_EXIT=0`. If any differs: a `STOP:` line, no push.
6. **Push:** `git push origin HEAD:fix/render-check; echo PUSH_EXIT=$?` must print `PUSH_EXIT=0`.
7. **Exit report:** write it to `build/render-check-exit-report.md` and repeat it as your final message: the new SHA, the numstat, every marker line and the diff result, and anything that went wrong (a denial, a difference, a copying slip you fixed). On a problem its first line is `STOP: <reason>`.

## Appendix A: the edits (`build/fix.diff`, apply exactly)
````diff
diff --git a/CHANGELOG.md b/CHANGELOG.md
index f3ab804..315395f 100644
--- a/CHANGELOG.md
+++ b/CHANGELOG.md
@@ -6,7 +6,7 @@ The port's releases. The version is the one the firmware carries
 ## Unreleased
 
 ### Docs: the panel and controls guide
-- `docs/GUIDE.md` (linked from the README): the uO_C rev A panel (`docs/img/panel.svg`), the controls on each screen as they are on `main`, the power-on, the safe mode and the crash pager. The screen pictures (`docs/img/screen-*.png`) are drawn by the firmware's own screen code on the host (`tools/render_screens.sh`, `tools/render_screens/`), and CI fails if they are stale.
+- `docs/GUIDE.md` (linked from the README): the uO_C rev A panel (`docs/img/panel.svg`), the controls on each screen as they are on `main`, the power-on, the safe mode and the crash pager. The screen pictures (`docs/img/screen-*.png`) are drawn by the firmware's own screen code on the host (`tools/render_screens.sh`, `tools/render_screens/`), and CI fails if they are stale (a screen that differs, is missing or is left over; the check is skipped, with a message, off Linux).
 
 ### UI, phase 1a: the input core (no change on the module yet)
 - `parts/oc_ui_input.c`: the buttons and encoders as events (5 ms debounce, tap ≤ 400 ms, hold from 400 ms, long at 600 ms, BOTTOM as shift (each event says whether BOTTOM is held), TOP + BOTTOM chord within `UI_CHORD_MS` = 40 ms either way round, boot-latched buttons ignored until released, value acceleration) and the 1 kHz sampler's ring. Host-tested in `tests/oc_ui_test.c`; not wired into the firmware until phase 1b. Plan: `docs/UI-PLAN.md`.
diff --git a/docs/GUIDE.md b/docs/GUIDE.md
index c63456b..ef0b520 100644
--- a/docs/GUIDE.md
+++ b/docs/GUIDE.md
@@ -161,5 +161,8 @@ tools/render_screens.sh        # writes docs/img/screen-*.png
 The pictures come from the code, so a change to a screen means rendering them
 again and committing the new PNGs. `tools/build_host.sh` (and so CI's host job)
 renders them into `build/render_check` and stops with
-`docs/img stale: run tools/render_screens.sh` if one differs from `docs/img`.
-The panel drawing (`docs/img/panel.svg`) is drawn by hand.
+`docs/img stale: run tools/render_screens.sh` if a screen differs, is missing
+or is left over in `docs/img`. The renderer runs on Linux only (it maps the
+firmware's fixed debug-register addresses); elsewhere `tools/build_host.sh`
+skips the check and says so. The panel drawing (`docs/img/panel.svg`) is drawn
+by hand.
diff --git a/tools/build_host.sh b/tools/build_host.sh
index f8e9de2..bd170f9 100755
--- a/tools/build_host.sh
+++ b/tools/build_host.sh
@@ -21,10 +21,18 @@ mkdir -p "$GEN" "$OUT"
 [ -f "$GEN/x0x_drum_tables.h" ]   || "$PY" "$TESSERA/tools/gen_909_tables.py" "$GEN/x0x_drum_tables.h" >/dev/null
 [ -f "$GEN/melodee_fm6.h" ]       || "$PY" "$TESSERA/tools/gen_fm6_patches.py" "$GEN/melodee_fm6.h" >/dev/null
 
-# the guide's screens (docs/GUIDE.md): docs/img must be what the screen code draws now
-if [ "$1" != build ]; then
+# the guide's screens (docs/GUIDE.md): docs/img must be what the screen code draws now,
+# the same screen files both ways and the same bytes (the renderer runs on Linux only)
+if [ "$1" != build ] && [ "$(uname -s)" != Linux ]; then
+    echo "render_screens: check skipped (the renderer runs on Linux only)"
+elif [ "$1" != build ]; then
+    rm -rf build/render_check
     tools/render_screens.sh build/render_check >/dev/null
+    for f in docs/img/screen-*.png; do
+        [ -f "build/render_check/${f##*/}" ] || { echo "docs/img stale: run tools/render_screens.sh (${f##*/} is committed but not rendered)" >&2; exit 1; }
+    done
     for f in build/render_check/screen-*.png; do
+        [ -f "docs/img/${f##*/}" ] || { echo "docs/img stale: run tools/render_screens.sh (${f##*/} is rendered but not committed)" >&2; exit 1; }
         cmp -s "$f" "docs/img/${f##*/}" || { echo "docs/img stale: run tools/render_screens.sh (${f##*/} differs)" >&2; exit 1; }
     done
     echo "render_screens: docs/img current"
diff --git a/tools/render_screens/rs_main.cpp b/tools/render_screens/rs_main.cpp
index e1a4a11..195a80a 100644
--- a/tools/render_screens/rs_main.cpp
+++ b/tools/render_screens/rs_main.cpp
@@ -137,8 +137,10 @@ static int save_png(const char *dir, const char *name)
     char path[512];
     FILE *f;
 
-    if (!raw || !z)
+    if (!raw || !z) {
+        free(raw); free(z);
         return 1;
+    }
     for (uint32_t y = 0; y < ph; y++) {
         raw[y * stride] = 0;                          /* the filter: none */
         for (uint32_t x = 0; x < pw; x++)
@@ -169,8 +171,10 @@ static int save_png(const char *dir, const char *name)
     ihdr[10] = ihdr[11] = ihdr[12] = 0;
     snprintf(path, sizeof path, "%s/%s", dir, name);
     f = fopen(path, "wb");
-    if (!f)
+    if (!f) {
+        free(raw); free(z);
         return 1;
+    }
     fwrite(SIG, 1, 8, f);
     chunk(f, "IHDR", ihdr, 13);
     chunk(f, "IDAT", z, zn);
````

## Appendix B: `build/check.sh` (write exactly)
````sh
#!/bin/sh
# tessera-guide-fix-1 checks. Every check prints NAME_EXIT=n, 0 = pass. Run from the clone root.
cd "$(dirname "$0")/.." || exit 1
B=build/fixcheck
rm -rf "$B" && mkdir -p "$B/bin"

tools/build_host.sh > "$B/host.log" 2>&1; echo BUILD_HOST_EXIT=$?
grep -E '^(render_screens: |oc_port: [0-9]+ checks|oc_ui: [0-9]+ checks|oc_nav: [0-9]+ checks)' "$B/host.log"
md5sum docs/img/screen-*.png

# negative: a stale PNG (screen-home.png holds the reset screen)
cp docs/img/screen-home.png "$B/home.png" && cp docs/img/screen-reset.png docs/img/screen-home.png
tools/build_host.sh > "$B/stale.log" 2>&1; r=$?; cp "$B/home.png" docs/img/screen-home.png
grep '^docs/img stale' "$B/stale.log"; test $r -ne 0; echo NEG_STALE_EXIT=$?

# negative: an orphan committed PNG (no render behind it)
cp docs/img/screen-home.png docs/img/screen-orphan.png
tools/build_host.sh > "$B/orphan.log" 2>&1; r=$?; rm -f docs/img/screen-orphan.png
grep '^docs/img stale' "$B/orphan.log"; test $r -ne 0; echo NEG_ORPHAN_EXIT=$?

# negative: a rendered screen that is not committed
mv docs/img/screen-reset.png "$B/reset.png"
tools/build_host.sh > "$B/missing.log" 2>&1; r=$?; mv "$B/reset.png" docs/img/screen-reset.png
grep '^docs/img stale' "$B/missing.log"; test $r -ne 0; echo NEG_MISSING_EXIT=$?

# a leftover file in build/render_check is cleared, not trusted
cp docs/img/screen-home.png build/render_check/screen-leftover.png
tools/build_host.sh > "$B/leftover.log" 2>&1; echo LEFTOVER_CLEARED_EXIT=$?
test ! -e build/render_check/screen-leftover.png; echo LEFTOVER_GONE_EXIT=$?

# off Linux: the check is skipped with a message (uname shimmed to Darwin)
printf '#!/bin/sh\necho Darwin\n' > "$B/bin/uname" && chmod +x "$B/bin/uname"
PATH="$PWD/$B/bin:$PATH" tools/build_host.sh > "$B/darwin.log" 2>&1; echo OFF_LINUX_EXIT=$?
grep '^render_screens: ' "$B/darwin.log"

# the device build and the read-only checks
pio run -e T40 > "$B/pio.log" 2>&1; echo PIO_EXIT=$?
python3 tools/check_read_only.py > "$B/ro.log" 2>&1; echo READ_ONLY_EXIT=$?
python3 tools/check_sections.py .pio/build/T40/firmware.elf > "$B/sections.log" 2>&1; echo SECTIONS_EXIT=$?
test -z "$(grep -rnE 'flash_write|flash_erase|eeprom_write|lfs_format|lfs_file_write|lfs_remove|lfs_rename|EEPROM\.(put|write|update)|eepromemu_|LittleFS_Program|\.format\(|eraseFiles' firmware/ parts/ tools/render_screens tools/render_screens.sh tools/build_host.sh docs/GUIDE.md)"; echo NOGREP_EXIT=$?
test -z "$(grep -rli format tools/render_screens tools/render_screens.sh docs/GUIDE.md)"; echo NOFORMAT_EXIT=$?

# the tree: only the four edited files differ from main, nothing untracked
git diff --numstat origin/main
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
docs/img stale: run tools/render_screens.sh (screen-home.png differs)
NEG_STALE_EXIT=0
docs/img stale: run tools/render_screens.sh (screen-orphan.png is committed but not rendered)
NEG_ORPHAN_EXIT=0
docs/img stale: run tools/render_screens.sh (screen-reset.png is rendered but not committed)
NEG_MISSING_EXIT=0
LEFTOVER_CLEARED_EXIT=0
LEFTOVER_GONE_EXIT=0
OFF_LINUX_EXIT=0
render_screens: check skipped (the renderer runs on Linux only)
PIO_EXIT=0
READ_ONLY_EXIT=0
SECTIONS_EXIT=0
NOGREP_EXIT=0
NOFORMAT_EXIT=0
1	1	CHANGELOG.md
5	2	docs/GUIDE.md
10	2	tools/build_host.sh
6	2	tools/render_screens/rs_main.cpp
TREE_EXIT=0
````
