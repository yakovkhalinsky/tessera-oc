#!/bin/sh
# panel-layout (job panel-layout-2) checks. Every check prints NAME_EXIT=n, 0 = pass.
# Run from anywhere in the clone: sh docs/briefs/2026-11-07-panel-layout.check.sh
# Its whole output goes to build/check.out; compare it with the .check.expected file.
cd "$(git rev-parse --show-toplevel)" || exit 1
B=build/layoutcheck
rm -rf "$B" && mkdir -p "$B" || exit 1
exec > build/check.out 2>&1

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

# the tree: only the four edited files differ from the job branch's start, nothing untracked
git diff --quiet origin/docs/panel-layout-2 -- docs/briefs; echo BRIEFS_UNCHANGED_EXIT=$?
git diff --numstat origin/docs/panel-layout-2
test -z "$(git status --porcelain | grep -v '^ M ')"; echo TREE_EXIT=$?
