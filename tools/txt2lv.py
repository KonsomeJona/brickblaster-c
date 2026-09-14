#!/usr/bin/env python3
"""Compile a text world (tools/levels/*.txt) into a 31,200-byte .lv file.

Text format: `LEVEL n: name` headers followed by 26 rows `NN t t ... t`
(13 tokens). Token `..` is empty, otherwise a brick letter and a colour digit:
N normal, M multi (4 hits), I indestructible, T transparent, L teleporter.
Brick bytes are the EDITOR.ASM brush codes (0x21, 0x24, 0x08, 0x11, 0x18)
with the colour in bits 7-6. Unused slots are filled with 0xFF, the sentinel
search_level_number stops on (MAIN.ASM:5025-5041).

usage: txt2lv.py tools/levels/atoll.txt assets/levels/Blaster.lv3
"""
import re
import sys

BRUSH = {"N": 0x21, "M": 0x24, "I": 0x08, "T": 0x11, "L": 0x18}
COLS, ROWS, EDIT_ROWS, SLOTS = 13, 30, 26, 80


def parse(text):
    levels, grid = [], None
    for line in text.splitlines():
        if re.match(r"^LEVEL \d+:", line):
            grid = []
            levels.append(grid)
            continue
        m = re.match(r"^(\d\d) (.*)$", line)
        if not m or grid is None:
            continue
        row, tokens = int(m.group(1)), m.group(2).split()
        if row != len(grid) or len(tokens) != COLS:
            sys.exit(f"level {len(levels)} row {row}: expected row {len(grid)} with {COLS} tokens")
        cells = []
        for t in tokens:
            if t == "..":
                cells.append(0)
            elif len(t) == 2 and t[0] in BRUSH and t[1] in "0123":
                cells.append(BRUSH[t[0]] | int(t[1]) << 6)
            else:
                sys.exit(f"level {len(levels)} row {row}: bad token {t!r}")
        grid.append(cells)
    for i, g in enumerate(levels, 1):
        if len(g) != EDIT_ROWS:
            sys.exit(f"level {i}: {len(g)} rows, expected {EDIT_ROWS}")
    return levels


def main():
    src, dst = sys.argv[1], sys.argv[2]
    levels = parse(open(src, encoding="utf-8").read())
    if not 1 <= len(levels) < SLOTS:
        sys.exit("need between 1 and 79 levels")
    out = bytearray()
    for g in levels:
        block = bytearray(COLS * ROWS)
        for r, cells in enumerate(g):
            block[r * COLS:(r + 1) * COLS] = bytes(cells)
        if block[0] == 0xFF:
            sys.exit("a level may not start with the 0xFF sentinel")
        out += block
    out += b"\xff" * (COLS * ROWS * (SLOTS - len(levels)))
    open(dst, "wb").write(out)
    print(f"{dst}: {len(levels)} levels, {len(out)} bytes")


if __name__ == "__main__":
    main()
