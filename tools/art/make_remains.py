#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Draw assets/tiles/remains.png (D70): a small skull on crossed bones, left
where a creature died. Run once; afterwards the PNG (or its .aseprite
source) is the master.

    uv run tools/art/make_remains.py
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets" / "tiles" / "remains.png"

INK = (0, 0, 0, 255)            # outline
BONE = (255, 255, 170, 255)     # light bone
SHADE = (170, 170, 85, 255)     # shaded bone

# 24 x 24, '.' transparent, '#' ink, 'W' bone, 'o' shade. The pile lies in
# the lower half so a figure standing on the field mostly covers it.
ART = [
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "...##.....######.....##.",
    "..#WW#...#WWWWWW#...#WW#",
    "..#WWo#.#WWWWWWWo#.#oWW#",
    "...##oo#WW##WW##Wo#oo##.",
    ".....#oWWW##WW##Wo#o#...",
    "......#WWWWW##WWWoo#....",
    ".......#WWWW##WWoo#.....",
    "......##oWoWoWoWo##.....",
    ".....#oo#W#W#W#W#Wo#....",
    "....#oo#..#######.#oo#..",
    "...##o#............#o##.",
    "..#WWo#............#oWW#",
    "..#WW#..............#WW#",
    "...##................##.",
    "........................",
]

COL = {".": (0, 0, 0, 0), "#": INK, "W": BONE, "o": SHADE}


def main() -> None:
    assert len(ART) == 24 and all(len(r) == 24 for r in ART)
    im = Image.new("RGBA", (24, 24))
    for y, row in enumerate(ART):
        for x, c in enumerate(row):
            im.putpixel((x, y), COL[c])
    im.save(OUT)
    print(f"wrote {OUT.relative_to(ROOT).as_posix()}")


if __name__ == "__main__":
    main()
