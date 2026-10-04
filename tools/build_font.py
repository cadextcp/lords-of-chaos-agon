#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Build the heading font (polish round, ADR 0012: fonts from VDP buffers).

    uv run tools/build_font.py

An own 5x8 base design (row 7 = descender) is turned into an 8x16 display
face: rows doubled, stems one pixel bolder, small foot serifs.
The game draws headings with it (render_heading) and falls back to the
system font when the file is missing.

Outputs (generated, not committed):
  build/fonts/head.fnt      256 glyphs x 16 rows, 1 byte per row (bit 7 =
                            left pixel) - the VDP's font buffer layout
  build/fonts/head.png      preview sheet

Character codes follow the game's strings: ASCII plus the umlaut codes of
umfont.c (0x84 ae, 0x94 oe, 0x81 ue, 0xE1 ss, 0x8E Ae, 0x99 Oe, 0x9A Ue).
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "build" / "fonts"

# 5 columns x 8 rows; row 7 is below the baseline (descenders).
G = {
    "A": [".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X", "....."],
    "B": ["XXXX.", "X...X", "X...X", "XXXX.", "X...X", "X...X", "XXXX.", "....."],
    "C": [".XXX.", "X...X", "X....", "X....", "X....", "X...X", ".XXX.", "....."],
    "D": ["XXX..", "X..X.", "X...X", "X...X", "X...X", "X..X.", "XXX..", "....."],
    "E": ["XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "XXXXX", "....."],
    "F": ["XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "X....", "....."],
    "G": [".XXX.", "X...X", "X....", "X.XXX", "X...X", "X...X", ".XXXX", "....."],
    "H": ["X...X", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X", "....."],
    "I": [".XXX.", "..X..", "..X..", "..X..", "..X..", "..X..", ".XXX.", "....."],
    "J": ["..XXX", "...X.", "...X.", "...X.", "...X.", "X..X.", ".XX..", "....."],
    "K": ["X...X", "X..X.", "X.X..", "XX...", "X.X..", "X..X.", "X...X", "....."],
    "L": ["X....", "X....", "X....", "X....", "X....", "X....", "XXXXX", "....."],
    "M": ["X...X", "XX.XX", "X.X.X", "X.X.X", "X...X", "X...X", "X...X", "....."],
    "N": ["X...X", "XX..X", "X.X.X", "X..XX", "X...X", "X...X", "X...X", "....."],
    "O": [".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX.", "....."],
    "P": ["XXXX.", "X...X", "X...X", "XXXX.", "X....", "X....", "X....", "....."],
    "Q": [".XXX.", "X...X", "X...X", "X...X", "X.X.X", "X..X.", ".XX.X", "....."],
    "R": ["XXXX.", "X...X", "X...X", "XXXX.", "X.X..", "X..X.", "X...X", "....."],
    "S": [".XXXX", "X....", "X....", ".XXX.", "....X", "....X", "XXXX.", "....."],
    "T": ["XXXXX", "..X..", "..X..", "..X..", "..X..", "..X..", "..X..", "....."],
    "U": ["X...X", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX.", "....."],
    "V": ["X...X", "X...X", "X...X", "X...X", "X...X", ".X.X.", "..X..", "....."],
    "W": ["X...X", "X...X", "X...X", "X.X.X", "X.X.X", "X.X.X", ".X.X.", "....."],
    "X": ["X...X", "X...X", ".X.X.", "..X..", ".X.X.", "X...X", "X...X", "....."],
    "Y": ["X...X", "X...X", ".X.X.", "..X..", "..X..", "..X..", "..X..", "....."],
    "Z": ["XXXXX", "....X", "...X.", "..X..", ".X...", "X....", "XXXXX", "....."],
    "a": [".....", ".....", ".XXX.", "....X", ".XXXX", "X...X", ".XXXX", "....."],
    "b": ["X....", "X....", "XXXX.", "X...X", "X...X", "X...X", "XXXX.", "....."],
    "c": [".....", ".....", ".XXX.", "X....", "X....", "X...X", ".XXX.", "....."],
    "d": ["....X", "....X", ".XXXX", "X...X", "X...X", "X...X", ".XXXX", "....."],
    "e": [".....", ".....", ".XXX.", "X...X", "XXXXX", "X....", ".XXX.", "....."],
    "f": ["..XX.", ".X...", ".X...", "XXX..", ".X...", ".X...", ".X...", "....."],
    "g": [".....", ".....", ".XXXX", "X...X", "X...X", ".XXXX", "....X", ".XXX."],
    "h": ["X....", "X....", "XXXX.", "X...X", "X...X", "X...X", "X...X", "....."],
    "i": ["..X..", ".....", ".XX..", "..X..", "..X..", "..X..", ".XXX.", "....."],
    "j": ["...X.", ".....", "..XX.", "...X.", "...X.", "...X.", "...X.", ".XX.."],
    "k": ["X....", "X....", "X..X.", "X.X..", "XX...", "X.X..", "X..X.", "....."],
    "l": [".XX..", "..X..", "..X..", "..X..", "..X..", "..X..", ".XXX.", "....."],
    "m": [".....", ".....", "XX.X.", "X.X.X", "X.X.X", "X.X.X", "X...X", "....."],
    "n": [".....", ".....", "XXXX.", "X...X", "X...X", "X...X", "X...X", "....."],
    "o": [".....", ".....", ".XXX.", "X...X", "X...X", "X...X", ".XXX.", "....."],
    "p": [".....", ".....", "XXXX.", "X...X", "X...X", "XXXX.", "X....", "X...."],
    "q": [".....", ".....", ".XXXX", "X...X", "X...X", ".XXXX", "....X", "....X"],
    "r": [".....", ".....", "X.XX.", "XX..X", "X....", "X....", "X....", "....."],
    "s": [".....", ".....", ".XXXX", "X....", ".XXX.", "....X", "XXXX.", "....."],
    "t": [".X...", ".X...", "XXX..", ".X...", ".X...", ".X..X", "..XX.", "....."],
    "u": [".....", ".....", "X...X", "X...X", "X...X", "X..XX", ".XX.X", "....."],
    "v": [".....", ".....", "X...X", "X...X", "X...X", ".X.X.", "..X..", "....."],
    "w": [".....", ".....", "X...X", "X...X", "X.X.X", "X.X.X", ".X.X.", "....."],
    "x": [".....", ".....", "X...X", ".X.X.", "..X..", ".X.X.", "X...X", "....."],
    "y": [".....", ".....", "X...X", "X...X", "X...X", ".XXXX", "....X", ".XXX."],
    "z": [".....", ".....", "XXXXX", "...X.", "..X..", ".X...", "XXXXX", "....."],
    "0": [".XXX.", "X...X", "X..XX", "X.X.X", "XX..X", "X...X", ".XXX.", "....."],
    "1": ["..X..", ".XX..", "..X..", "..X..", "..X..", "..X..", ".XXX.", "....."],
    "2": [".XXX.", "X...X", "....X", "...X.", "..X..", ".X...", "XXXXX", "....."],
    "3": ["XXXX.", "....X", "....X", ".XXX.", "....X", "....X", "XXXX.", "....."],
    "4": ["...X.", "..XX.", ".X.X.", "X..X.", "XXXXX", "...X.", "...X.", "....."],
    "5": ["XXXXX", "X....", "XXXX.", "....X", "....X", "X...X", ".XXX.", "....."],
    "6": ["..XX.", ".X...", "X....", "XXXX.", "X...X", "X...X", ".XXX.", "....."],
    "7": ["XXXXX", "....X", "...X.", "..X..", ".X...", ".X...", ".X...", "....."],
    "8": [".XXX.", "X...X", "X...X", ".XXX.", "X...X", "X...X", ".XXX.", "....."],
    "9": [".XXX.", "X...X", "X...X", ".XXXX", "....X", "...X.", ".XX..", "....."],
    " ": ["....."] * 8,
    ".": [".....", ".....", ".....", ".....", ".....", ".XX..", ".XX..", "....."],
    ",": [".....", ".....", ".....", ".....", ".....", ".XX..", "..X..", ".X..."],
    ":": [".....", ".XX..", ".XX..", ".....", ".XX..", ".XX..", ".....", "....."],
    ";": [".....", ".XX..", ".XX..", ".....", ".XX..", "..X..", ".X...", "....."],
    "!": ["..X..", "..X..", "..X..", "..X..", "..X..", ".....", "..X..", "....."],
    "?": [".XXX.", "X...X", "....X", "...X.", "..X..", ".....", "..X..", "....."],
    "-": [".....", ".....", ".....", "XXXXX", ".....", ".....", ".....", "....."],
    "+": [".....", "..X..", "..X..", "XXXXX", "..X..", "..X..", ".....", "....."],
    "'": ["..X..", "..X..", ".X...", ".....", ".....", ".....", ".....", "....."],
    "(": ["...X.", "..X..", ".X...", ".X...", ".X...", "..X..", "...X.", "....."],
    ")": [".X...", "..X..", "...X.", "...X.", "...X.", "..X..", ".X...", "....."],
    "/": [".....", "....X", "...X.", "..X..", ".X...", "X....", ".....", "....."],
    "*": [".....", "X.X.X", ".XXX.", "XXXXX", ".XXX.", "X.X.X", ".....", "....."],
    "%": ["XX..X", "XX..X", "...X.", "..X..", ".X...", "X..XX", "X..XX", "....."],
    "=": [".....", ".....", "XXXXX", ".....", "XXXXX", ".....", ".....", "....."],
    "<": ["...X.", "..X..", ".X...", "X....", ".X...", "..X..", "...X.", "....."],
    ">": [".X...", "..X..", "...X.", "....X", "...X.", "..X..", ".X...", "....."],
    "_": [".....", ".....", ".....", ".....", ".....", ".....", "XXXXX", "....."],
}


def umlaut(base: list[str]) -> list[str]:
    """Two dots above: on lowercase they replace the empty top rows, on
    uppercase the glyph is squeezed by dropping its second row."""
    if base[0].strip(".") == "" and base[1].strip(".") == "":
        return [".X.X.", "....."] + base[2:]
    return [".X.X."] + base[:1] + base[2:]


EXTRA = {
    0x84: umlaut(G["a"]), 0x94: umlaut(G["o"]), 0x81: umlaut(G["u"]),
    0x8E: umlaut(G["A"]), 0x99: umlaut(G["O"]), 0x9A: umlaut(G["U"]),
    0xE1: [".XX..", "X..X.", "X..X.", "XXX..", "X..X.", "X..X.", "XXX..", "X...."],
}


def display_glyph(rows: list[str]) -> list[int]:
    """5x8 -> 8x16: rows doubled, stems one pixel bolder to the right
    (columns 1-6), small foot serifs in column 0, column 7 empty."""
    base = [[c == "X" for c in r] for r in rows]

    def on(x, y):
        return 0 <= y < 8 and 0 <= x < 5 and base[y][x]
    out = []
    for y in range(8):
        for half in (0, 1):
            bits = [False] * 8
            for x in range(5):
                if on(x, y):
                    bits[x + 1] = bits[x + 2] = True          # bold
            # a small foot serif left of a stem that ends on the baseline
            # (column 7 always stays empty: one pixel between letters)
            for x in range(5):
                if half == 1 and on(x, y) and on(x, y - 1) and not on(x, y + 1)                         and not on(x - 1, y) and y >= 5:
                    bits[x] = True
            v = 0
            for i, b in enumerate(bits):
                if b:
                    v |= 0x80 >> i
            out.append(v)
    return out


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    table = [[0] * 16 for _ in range(256)]
    for ch, rows in G.items():
        table[ord(ch)] = display_glyph(rows)
    for code, rows in EXTRA.items():
        table[code] = display_glyph(rows)
    blob = bytes(v for glyph in table for v in glyph)
    (OUT_DIR / "head.fnt").write_bytes(blob)

    sample = "LORDS OF CHAOS  Gl\x81ckwunsch!  Zauber erlernen 0123456789"
    im = Image.new("RGB", (len(sample) * 8, 16), (0, 0, 0))
    px = im.load()
    for i, ch in enumerate(sample):
        for y, v in enumerate(table[ord(ch)]):
            for x in range(8):
                if v & (0x80 >> x):
                    px[i * 8 + x, y] = (255, 255, 0)
    im.resize((im.width * 3, im.height * 3), Image.NEAREST).save(OUT_DIR / "head.png")
    print(f"[font] build/fonts/head.fnt ({len(blob)} bytes), preview build/fonts/head.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
