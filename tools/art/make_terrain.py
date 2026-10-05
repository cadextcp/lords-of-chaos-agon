#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Terrain variety tiles (graphics polish, D51): floor variants, flowing water,
water lilies. The PNGs written to assets/tiles/ are the source afterwards;
rerun only to regenerate.

    uv run tools/art/make_terrain.py            # everything
    uv run tools/art/make_terrain.py --only floor_grass_1 floor_water_0

Floor variants are the base tile rolled by an offset plus a few accents, so
they match the base art. Water is drawn from scratch: four frames, ripple
rows moving sideways at different speeds (a multiple of 6 px per frame, so the
24 px tile wraps seamlessly and all fields share one phase = a connected
river).
"""

from __future__ import annotations

import argparse
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent.parent
TILES = ROOT / "assets" / "tiles"
sys.path.insert(0, str(ROOT / "tools" / "art"))
from palette import PALETTE  # noqa: E402

N = 24
BLUE, MBLUE, LBLUE, NAVY, WHITE = (0, 0, 170), (0, 85, 170), (85, 170, 255), (0, 0, 85), (255, 255, 255)
DGREEN, GREEN, LGREEN = (0, 85, 0), (0, 170, 0), (85, 255, 85)
YELLOW, PINK, GREY, DGREY = (255, 255, 85), (255, 85, 255), (170, 170, 170), (85, 85, 85)
BROWN, DBROWN = (170, 85, 0), (85, 85, 0)


def load(name: str) -> Image.Image:
    return Image.open(TILES / f"{name}.png").convert("RGBA")


def roll(im: Image.Image, dx: int, dy: int) -> Image.Image:
    out = Image.new("RGBA", im.size)
    for y in range(N):
        for x in range(N):
            out.putpixel(((x + dx) % N, (y + dy) % N), im.getpixel((x, y)))
    return out


def dots(im: Image.Image, seed: int, spots: list[tuple[tuple, int]]) -> Image.Image:
    """Scatter single pixels: spots = [(colour, count), ...]."""
    rnd = random.Random(seed)
    for col, count in spots:
        for _ in range(count):
            im.putpixel((rnd.randrange(1, N - 1), rnd.randrange(1, N - 1)), (*col, 255))
    return im


def flower(im: Image.Image, x: int, y: int, petal=WHITE) -> None:
    im.putpixel((x, y), (*YELLOW, 255))
    for dx, dy in ((1, 0), (-1, 0), (0, -1)):
        im.putpixel((x + dx, y + dy), (*petal, 255))
    im.putpixel((x, y + 1), (*GREEN, 255))


def tuft(im: Image.Image, x: int, y: int, col=LGREEN) -> None:
    for dy, dx in ((0, 0), (-1, 0), (-2, 0), (-1, 1), (-2, -1)):
        im.putpixel((x + dx, y + dy), (*col, 255))


def stone(im: Image.Image, x: int, y: int) -> None:
    for dx in (0, 1, 2):
        im.putpixel((x + dx, y), (*GREY, 255))
    im.putpixel((x, y + 1), (*DGREY, 255))
    im.putpixel((x + 1, y + 1), (*DGREY, 255))
    im.putpixel((x + 1, y - 1), (*GREY, 255))


def floor_grass_1():
    im = roll(load("floor_grass"), 7, 5)
    flower(im, 5, 8)
    flower(im, 17, 17, PINK)
    return im


def floor_grass_2():
    im = roll(load("floor_grass"), 13, 11)
    tuft(im, 8, 10)
    tuft(im, 18, 19)
    tuft(im, 15, 6, GREEN)
    return im


def floor_grass_3():
    im = roll(load("floor_grass"), 5, 17)
    stone(im, 6, 17)
    flower(im, 19, 7)
    return im


def floor_path_1():
    im = roll(load("floor_path"), 9, 6)
    dots(im, 31, [(DBROWN, 6), (BROWN, 3)])
    return im


def floor_path_2():
    im = roll(load("floor_path"), 15, 13)
    tuft(im, 7, 20, GREEN)
    dots(im, 32, [(DBROWN, 5)])
    return im


def floor_tallgrass_1():
    return roll(load("floor_tallgrass"), 11, 7)


def floor_swamp_1():
    return roll(load("floor_swamp"), 12, 9)


def _ripple(im: Image.Image, x: int, y: int, crest, body) -> None:
    for dx, dy in ((0, 0), (1, 0), (2, -1), (3, 0), (4, 0)):
        im.putpixel(((x + dx) % N, y + dy), (*body, 255))
    im.putpixel(((x + 2) % N, y - 1), (*crest, 255))


# variant -> (ripple rows, start x per row, speed per row in px/frame)
_WATER = {
    "a": ((3, 9, 15, 21), (0, 6, 0, 6), (6, 12, 6, 18), BLUE, LBLUE),
    "b": ((0 + 1, 6, 12, 18), (3, 9, 15, 21), (12, 6, 18, 6), BLUE, MBLUE),
}


def water(variant: str, frame: int) -> Image.Image:
    ys, xs, speeds, base, body = _WATER[variant]
    im = Image.new("RGBA", (N, N), (*base, 255))
    for row, (y, x0, sp) in enumerate(zip(ys, xs, speeds)):
        x = x0 + sp * frame
        for k in (0, 12):
            sparkle = (row + frame) % 2 == 0
            _ripple(im, x + k, y + 1, WHITE if sparkle else body, body)
    for x, y in ((5, 6), (17, 18), (11, 13)):
        im.putpixel(((x + 6 * frame) % N, y), (*MBLUE, 255))
    return im


def lily(frame: int) -> Image.Image:
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for (cx, cy, rx, ry, bloom) in ((8, 9, 5, 3, True), (17, 16, 3, 2, False)):
        cy += frame                      # bob one pixel down in frame 1
        d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=(*GREEN, 255), outline=(*DGREEN, 255))
        d.line((cx, cy, cx + rx, cy - 1), fill=(*BLUE, 255))     # the notch
        if bloom:
            im.putpixel((cx - 1, cy - 1), (*PINK, 255))
            im.putpixel((cx - 2, cy - 1), (*WHITE, 255))
            im.putpixel((cx - 1, cy - 2), (*WHITE, 255))
            im.putpixel((cx, cy - 1), (*YELLOW, 255))
    return im


def all_tiles() -> dict[str, Image.Image]:
    t = {
        "floor_grass_1": floor_grass_1(), "floor_grass_2": floor_grass_2(),
        "floor_grass_3": floor_grass_3(), "floor_path_1": floor_path_1(),
        "floor_path_2": floor_path_2(), "floor_tallgrass_1": floor_tallgrass_1(),
        "floor_swamp_1": floor_swamp_1(),
        "decor_lily_0": lily(0), "decor_lily_1": lily(1),
    }
    for f in range(4):
        t[f"floor_water_{f}"] = water("a", f)
        t[f"floor_waterb_{f}"] = water("b", f)
    return t


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--only", nargs="*")
    args = ap.parse_args()
    tiles = all_tiles()
    for name, im in tiles.items():
        if args.only and name not in args.only:
            continue
        for y in range(N):
            for x in range(N):
                r, g, b, a = im.getpixel((x, y))
                if a and (r, g, b) not in PALETTE:
                    raise SystemExit(f"{name}: colour {(r, g, b)} not in Agon palette")
        im.save(TILES / f"{name}.png")
        print(f"[terrain] {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
