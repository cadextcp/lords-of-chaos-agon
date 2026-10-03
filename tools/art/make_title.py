#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Generate the title screen (M5d, GDD 11.6) as an editable PNG.

    uv run tools/art/make_title.py      # -> assets/title/title.png

Own pixel art in the spirit of the original box art (3/4 view, wizard,
tower, portal - D7/D10: nothing copied): night sky with moon and stars,
a wizard with his staff before a glowing portal, a tower on the hills.
Only colours from the Agon 64 palette; gradients are dithered bands.
tools/build_title.py compiles the PNG to build/title.bin for the SD card.
The PNG is the source of truth; re-running overwrites it.
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from palette import C, PALETTE  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets" / "title" / "title.png"
W, H = 320, 240

# 5x7 pixel font for the letters we need (LORDS OF CHAOS).
FONT = {
    "L": ["X....", "X....", "X....", "X....", "X....", "X....", "XXXXX"],
    "O": [".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX."],
    "R": ["XXXX.", "X...X", "X...X", "XXXX.", "X.X..", "X..X.", "X...X"],
    "D": ["XXXX.", "X...X", "X...X", "X...X", "X...X", "X...X", "XXXX."],
    "S": [".XXXX", "X....", "X....", ".XXX.", "....X", "....X", "XXXX."],
    "F": ["XXXXX", "X....", "X....", "XXX..", "X....", "X....", "X...."],
    "C": [".XXXX", "X....", "X....", "X....", "X....", "X....", ".XXXX"],
    "H": ["X...X", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"],
    "A": [".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"],
    " ": [".....", ".....", ".....", ".....", ".....", ".....", "....."],
}


def px(im, x, y, col):
    if 0 <= x < W and 0 <= y < H:
        im.putpixel((x, y), (*col, 255))


def rect(im, x0, y0, x1, y1, col):
    ImageDraw.Draw(im).rectangle((x0, y0, x1, y1), fill=(*col, 255))


def ellipse(im, box, fill=None, outline=None):
    ImageDraw.Draw(im).ellipse(box, fill=(*fill, 255) if fill else None,
                               outline=(*outline, 255) if outline else None)


def poly(im, pts, col):
    ImageDraw.Draw(im).polygon(pts, fill=(*col, 255))


def text(im, s, x0, y0, scale, col, shadow=None):
    x = x0
    for ch in s:
        rows = FONT[ch]
        for ry, row in enumerate(rows):
            for rx, c in enumerate(row):
                if c == "X":
                    for dy in range(scale):
                        for dx in range(scale):
                            if shadow:
                                px(im, x + rx * scale + dx + 2,
                                   y0 + ry * scale + dy + 2, shadow)
                            px(im, x + rx * scale + dx, y0 + ry * scale + dy, col)
        x += 6 * scale


def dither(im, y0, y1, a, b, ratio_fn):
    """Two-colour dithered band between palette colours a and b."""
    for y in range(y0, y1):
        r = ratio_fn(y)
        for x in range(W):
            if ((x * 7 + y * 13) % 16) / 15 < r:
                px(im, x, y, b)
            else:
                px(im, x, y, a)


def main() -> int:
    im = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    rnd = random.Random(5)

    # night sky: three dithered bands, darker at the top
    dither(im, 0, 40, C["black"], C["navy"], lambda y: 0.15)
    dither(im, 40, 85, C["navy"], C["blue"], lambda y: (y - 40) / 45 * 0.5)
    dither(im, 85, 130, C["blue"], C["mblue"], lambda y: (y - 85) / 45 * 0.7)

    # stars
    for _ in range(90):
        x, y = rnd.randrange(W), rnd.randrange(0, 120)
        col = rnd.choice([C["white"], C["sky"], C["cream"]])
        px(im, x, y, col)
        if rnd.random() < 0.2:
            px(im, x + 1, y, col)

    # moon with a halo
    for r, col in ((34, C["navy"]), (29, C["mblue"]), (25, C["blue"])):
        ellipse(im, (250 - r, 38 - r, 250 + r, 38 + r), fill=col)
    ellipse(im, (228, 16, 272, 60), fill=C["cream"])
    ellipse(im, (234, 22, 252, 34), fill=C["white"])
    ellipse(im, (256, 40, 266, 50), fill=C["grey"])
    ellipse(im, (242, 46, 250, 54), fill=C["grey"])

    # far hills
    poly(im, [(0, 128), (60, 104), (130, 122), (200, 100), (320, 126),
              (320, 150), (0, 150)], C["dgreen"])
    poly(im, [(0, 140), (80, 120), (180, 138), (260, 118), (320, 140),
              (320, 170), (0, 170)], C["black"])

    # the tower on the right hill
    rect(im, 236, 60, 268, 130, C["dgrey"])
    for y in range(62, 130, 6):
        for x in range(236, 268, 8):
            rect(im, x + (3 if (y // 6) % 2 else 0), y, x + 6, y + 4, C["grey"])
    for (wx, wy) in ((242, 76), (256, 90), (244, 104)):
        rect(im, wx, wy, wx + 4, wy + 6, C["yellow"])
        rect(im, wx + 1, wy + 1, wx + 2, wy + 2, C["orange"])
    poly(im, [(232, 60), (252, 34), (272, 60)], C["purple"])   # roof
    poly(im, [(232, 60), (252, 34), (252, 60)], C["violet"])
    rect(im, 250, 26, 254, 36, C["dgrey"])                     # spire
    ellipse(im, (248, 20, 256, 28), fill=C["yellow"])

    # ground ledge
    poly(im, [(0, 240), (0, 168), (70, 152), (180, 160), (320, 172), (320, 240)],
         C["dgreen"])
    for _ in range(140):                     # grass specks
        x = rnd.randrange(W)
        y = rnd.randrange(165, 240)
        px(im, x, y, C["green"])
    poly(im, [(0, 240), (0, 200), (110, 186), (240, 196), (320, 206), (320, 240)],
         C["black"])

    # glowing portal behind the wizard
    for r, col in ((40, C["navy"]), (33, C["purple"]), (26, C["violet"]),
                   (18, C["lviolet"])):
        ellipse(im, (158 - r, 130 - r // 2 - r // 2, 158 + r,
                     130 + r + r // 3), fill=col)
    ellipse(im, (146, 112, 170, 152), fill=C["lviolet"])
    ellipse(im, (152, 120, 164, 144), fill=C["magenta"])
    for (x, y) in ((150, 118), (166, 132), (156, 142), (162, 116)):
        px(im, x, y, C["white"])

    # the wizard: big version of the in-game sprite (violet robe)
    wx, wy = 84, 168                          # feet position
    poly(im, [(wx - 26, wy), (wx - 14, wy - 52), (wx + 14, wy - 52),
              (wx + 26, wy)], C["magenta"])   # robe
    poly(im, [(wx + 2, wy - 52), (wx + 14, wy - 52), (wx + 26, wy),
              (wx + 10, wy)], C["violet"])
    rect(im, wx - 10, wy - 74, wx + 10, wy - 52, C["skin"])          # head
    poly(im, [(wx - 10, wy - 58), (wx + 10, wy - 58), (wx, wy - 44)], C["white"])
    px(im, wx - 4, wy - 68, C["black"])
    px(im, wx + 4, wy - 68, C["black"])
    poly(im, [(wx - 2, wy - 96), (wx + 22, wy - 74), (wx - 22, wy - 74)],
         C["magenta"])                        # hat
    poly(im, [(wx - 2, wy - 96), (wx + 22, wy - 74), (wx + 4, wy - 74)],
         C["violet"])
    rect(im, wx - 24, wy - 76, wx + 24, wy - 74, C["violet"])         # brim
    px(im, wx + 6, wy - 90, C["yellow"])
    rect(im, wx + 28, wy - 64, wx + 31, wy - 50, C["skin"])           # hand
    rect(im, wx + 30, wy - 120, wx + 34, wy - 46, C["wood"])          # staff
    ellipse(im, (wx + 24, wy - 132, wx + 40, wy - 116), fill=C["lviolet"])
    ellipse(im, (wx + 28, wy - 128, wx + 36, wy - 120), fill=C["white"])
    rect(im, wx - 20, wy - 4, wx - 12, wy, C["dbrown"])               # feet
    rect(im, wx + 10, wy - 4, wx + 18, wy, C["dbrown"])
    # staff light spills onto the robe
    for i in range(6):
        px(im, wx + 26 - i, wy - 60 + i * 4, C["lviolet"])

    # title block
    text(im, "LORDS OF CHAOS", 32, 12, 3, C["gold"], C["black"])
    text(im, "LORDS OF CHAOS", 34, 14, 3, C["yellow"], None)
    for x in range(16, 304, 8):               # separator
        px(im, x, 42, C["gold"])
        px(im, x + 1, 42, C["dbrown"])

    # frame
    ImageDraw.Draw(im).rectangle((0, 0, W - 1, H - 1), outline=(*C["black"], 255))
    ImageDraw.Draw(im).rectangle((2, 2, W - 3, H - 3), outline=(*C["dgreen"], 255))

    # palette guard (build_title checks again, fail early here)
    for y in range(H):
        for x in range(W):
            r, g, b, a = im.getpixel((x, y))
            if a not in (0, 255) or (a and (r, g, b) not in PALETTE):
                raise SystemExit(f"colour {(r, g, b, a)} at {x},{y} not allowed")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    im.save(OUT)
    print(f"[title] {OUT.relative_to(ROOT)} ({W}x{H})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
