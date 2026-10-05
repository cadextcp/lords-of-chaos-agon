#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Idle frames for the creatures that get drawn animation (D53).

    uv run tools/art/creature_frames.py [--sheet]

Reads assets/tiles/<name>.png and writes <name>_f1.png and <name>_f2.png:
  - wing flappers: the wing boxes move up (f1) and down (f2), behind the body
  - ghosts: the lower body sways to one side (f1) and the other (f2)
The key colours are copied, so owner variants work as for the base tile.
The step order in the game is base, f1, base, f2 (view.c FLAP_SEQ).
--sheet writes build/creature_frames.png (base, f1, f2 per creature, 4x).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent.parent
TILES = ROOT / "assets" / "tiles"
N = 24

# name -> ("wings", [boxes (x0, y0, x1, y1)], up, down)  or  ("sway", y0, y1, max_dx)
FRAMES = {
    "giant_bat": ("wings", [(0, 0, 8, 17), (15, 0, 23, 17)], 3, 2),
    "harpy": ("wings", [(0, 0, 6, 14), (17, 0, 23, 14)], 2, 2),
    "pixie": ("wings", [(2, 0, 8, 14), (15, 0, 21, 14)], 2, 2),
    "gryphon": ("wings", [(0, 0, 11, 11)], 3, 2),
    "pegasus": ("wings", [(0, 0, 9, 10)], 2, 2),
    "gold_dragon": ("wings", [(0, 0, 11, 11)], 3, 2),
    "green_dragon": ("wings", [(0, 0, 11, 11)], 3, 2),
    "red_dragon": ("wings", [(0, 0, 11, 11)], 3, 2),
    "ghost": ("sway", 9, 20, 2),
    "spectre": ("sway", 9, 20, 2),
}


def inside(boxes, x, y):
    return any(b[0] <= x <= b[2] and b[1] <= y <= b[3] for b in boxes)


def wings(im: Image.Image, boxes, dy: int) -> Image.Image:
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    moved = []
    for y in range(N):
        for x in range(N):
            px = im.getpixel((x, y))
            if px[3] == 0:
                continue
            if inside(boxes, x, y):
                moved.append((x, y + dy, px))
            else:
                out.putpixel((x, y), px)
    for x, y, px in moved:                    # behind the body: only into gaps
        if 0 <= y < N and out.getpixel((x, y))[3] == 0:
            out.putpixel((x, y), px)
    return out


def sway(im: Image.Image, y0: int, y1: int, dx: int) -> Image.Image:
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    for y in range(N):
        s = 0
        if y0 <= y <= y1:
            s = round(dx * (y - y0 + 1) / (y1 - y0 + 1))
        elif y > y1:
            s = dx
        for x in range(N):
            px = im.getpixel((x, y))
            if px[3] and 0 <= x + s < N:
                out.putpixel((x + s, y), px)
    return out


def make(name: str) -> tuple[Image.Image, Image.Image]:
    im = Image.open(TILES / f"{name}.png").convert("RGBA")
    spec = FRAMES[name]
    if spec[0] == "wings":
        return wings(im, spec[1], -spec[2]), wings(im, spec[1], spec[3])
    return sway(im, spec[1], spec[2], spec[3]), sway(im, spec[1], spec[2], -spec[3])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--sheet", action="store_true")
    args = ap.parse_args()
    sheet = Image.new("RGBA", (len(FRAMES) * 3 * N * 3, N * 3), (40, 40, 40, 255))
    for k, name in enumerate(FRAMES):
        f1, f2 = make(name)
        f1.save(TILES / f"{name}_f1.png")
        f2.save(TILES / f"{name}_f2.png")
        for j, im in enumerate((Image.open(TILES / f"{name}.png").convert("RGBA"), f1, f2)):
            big = im.resize((N * 3, N * 3), Image.NEAREST)
            sheet.paste(big, ((k * 3 + j) * N * 3, 0), big)
    if args.sheet:
        out = ROOT / "build" / "creature_frames.png"
        out.parent.mkdir(exist_ok=True)
        sheet.save(out)
        print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
