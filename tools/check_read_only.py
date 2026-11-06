#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The port's read-only check: the drive is opened for reading and nothing else.

The drive is read only (ZC, issuecomment-6677339063): the port never writes
the program's flash. The framework's own constants are a trap here: FILE_READ
is 0 and FILE_WRITE is 1 (cores/teensy4/FS.h:37-38), and littlefs's LFS_O_RDONLY
is 1 (LittleFS/src/littlefs/lfs.h:127) - so fs.open(name, LFS_O_RDONLY) opens
for WRITE: it creates the file if it is not there and seeks to its end
(LittleFS/src/LittleFS.h:278-296). The reads are fs.open(name) and nothing else.

The comments are stripped first: the port says why in prose, and prose is not a
write path.

    tools/check_read_only.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BAD = re.compile(r"\b(LFS_O_[A-Z]+|FILE_WRITE[A-Z_]*)\b")


def main():
    """the write modes in the port's own code: {file: [line, ...]}"""
    hits = {}
    for d in ("firmware", "parts"):
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for dirpath, _, names in os.walk(base):
            for name in sorted(names):
                if not name.endswith((".c", ".cpp", ".h")):
                    continue
                path = os.path.join(dirpath, name)
                with open(path, encoding="utf-8", errors="replace") as f:
                    text = f.read()
                code = re.sub(r"//[^\n]*", " ", re.sub(r"/\*.*?\*/", " ", text, flags=re.S))
                for i, line in enumerate(code.splitlines(), 1):
                    if BAD.search(line):
                        hits.setdefault(os.path.relpath(path, ROOT), []).append((i, line.strip()))
    if not hits:
        print("the drive is opened for reading and nothing else")
        return
    sys.stderr.write("the port names a write mode:\n")
    for path in sorted(hits):
        for i, line in hits[path]:
            sys.stderr.write("  %s:%d: %s\n" % (path, i, line))
    sys.exit(1)


if __name__ == "__main__":
    main()
