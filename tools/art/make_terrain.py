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
import math
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


# ---- terrain transitions (D52) --------------------------------------------
# Transparent overlays drawn on the field that gives way: the shore lip on
# water fields, grass tufts on path fields, tall-grass blades on meadows.
# Edge tiles are named <family>_m<mask> (mask bits N=1 E=2 S=4 W=8, 1..15);
# corner tiles <family>_c<k> (0 NW, 1 NE, 2 SE, 3 SW) cover a lone diagonal
# neighbour. The depth profile along a side is periodic and the same in every
# tile, so a straight bank is continuous across field borders.
SAND, DSAND = (170, 170, 85), (170, 85, 0)
FAMILIES = {
    # name: (base depth, wobble, extra rows of foam beyond the depth)
    "shore": (3.0, 1.0, 2),
    "path": (3.0, 1.5, 0),
    "tall": (2.5, 1.5, 0),
}
SIDES = ("n", "e", "s", "w")        # mask bits 1, 2, 4, 8


def profile(family: str, side: str) -> list[float]:
    rnd = random.Random(f"{family}{side}")
    base, amp, _ = FAMILIES[family]
    p1, p2 = rnd.uniform(0, 6.28), rnd.uniform(0, 6.28)
    return [base + amp * (0.7 * math.sin(2 * math.pi * 2 * i / N + p1)
                          + 0.5 * math.sin(2 * math.pi * 3 * i / N + p2))
            for i in range(N)]


def edge_colour(family: str, dist: int, depth: float, rnd: random.Random):
    if family == "shore":
        if dist < depth:
            return DSAND if rnd.random() < 0.07 else SAND
        if dist < depth + 1:
            return WHITE if rnd.random() < 0.6 else LBLUE
        return LBLUE if rnd.random() < 0.3 else None
    if dist >= depth or rnd.random() > 1.15 - dist / (depth + 0.5) * 0.9:
        return None
    if family == "path":
        return rnd.choice((DGREEN, DGREEN, GREEN, GREEN, LGREEN))
    return rnd.choice((LGREEN, LGREEN, GREEN, YELLOW))


def edge_tile(family: str, mask: int = 0, corner: int | None = None) -> Image.Image:
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    rnd = random.Random(f"{family}{mask}{corner}")
    reach = FAMILIES[family][2]
    prof = {s: profile(family, s) for s in SIDES}
    for y in range(N):
        for x in range(N):
            best = None            # (dist, depth) of the closest painting side
            for bit, side in enumerate(SIDES):
                if not mask & (1 << bit):
                    continue
                dist, i = {"n": (y, x), "s": (N - 1 - y, x),
                           "w": (x, y), "e": (N - 1 - x, y)}[side]
                d = prof[side][i]
                if dist < d + reach and (best is None or dist < best[0]):
                    best = (dist, d)
            if corner is not None:
                cx, cy = ((0, 0), (N - 1, 0), (N - 1, N - 1), (0, N - 1))[corner]
                dist = round(math.hypot(x - cx, y - cy) * 0.65)
                d = FAMILIES[family][0] + 1.0
                if dist < d + reach and (best is None or dist < best[0]):
                    best = (dist, d)
            if best:
                col = edge_colour(family, best[0], best[1], rnd)
                if col:
                    im.putpixel((x, y), (*col, 255))
    return im


def edge_tiles() -> dict[str, Image.Image]:
    t = {}
    for fam in FAMILIES:
        for mask in range(1, 16):
            t[f"edge_{fam}_m{mask:02d}"] = edge_tile(fam, mask)
        for k in range(4):
            t[f"edge_{fam}_c{k}"] = edge_tile(fam, 0, k)
    return t


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
    t.update(edge_tiles())
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
