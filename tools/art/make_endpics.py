#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Generate the end screen pictures (polish round, GDD 11.6) as PNGs.

    uv run tools/art/make_endpics.py   # -> assets/title/win.png, lose.png

96x96 each, in the style of the title (make_title.py): the game's own
tiles enlarged with Scale2x/Scale3x, only Agon 64 colours.
  win:  the wizard steps out of the glowing portal with gold and gems
  lose: a grave at night, the wizard's hat on the stone, a broken staff
tools/build_title.py compiles them to build/win.bin and build/lose.bin.
The PNGs are the source of truth; re-running overwrites them.
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_title import TILES, creature, enlarge  # noqa: E402
from palette import C, PALETTE  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "assets" / "title"
S = 96


def tile(name: str, factor: int) -> Image.Image:
    return enlarge(Image.open(TILES / f"{name}.png").convert("RGBA"), factor)


def put(im: Image.Image, x: int, y: int, col) -> None:
    if 0 <= x < S and 0 <= y < S:
        im.putpixel((x, y), (*col, 255))


def stars(im: Image.Image, rng: random.Random, n: int, ymax: int) -> None:
    for _ in range(n):
        put(im, rng.randrange(S), rng.randrange(ymax),
            rng.choice([C["dgrey"], C["grey"], C["white"]]))


def win() -> Image.Image:
    rng = random.Random(3)
    im = Image.new("RGBA", (S, S), (0, 0, 0, 255))
    stars(im, rng, 40, 60)
    # golden glow rings behind the portal
    for r, col in ((46, C["purple"]), (40, C["violet"]), (34, C["orange"])):
        for y in range(S):
            for x in range(S):
                if (x - 48) ** 2 + (y - 46) ** 2 <= r * r and (x + y) % 2 == 0:
                    put(im, x, y, col)
    for r, col in ((24, C["magenta"]), (16, C["pink"]), (9, C["white"])):
        for y in range(S):
            for x in range(S):
                if (x - 48) ** 2 + ((y - 40) * 1.3) ** 2 <= r * r and (x * 3 + y) % 3:
                    put(im, x, y, col)
    im.alpha_composite(creature("wizard", "p1", 3), (24, 22))
    for name, x in (("obj_gold", -2), ("obj_ruby", 24), ("obj_diamond", 46), ("obj_gold", 70)):
        im.alpha_composite(tile(name, 2) if name != "obj_gold" else tile(name, 2), (x, 54))
    for (x, y) in ((10, 20), (84, 14), (78, 40), (16, 46)):
        put(im, x, y, C["white"])
        for d in (1, 2):
            put(im, x + d, y, C["yellow"])
            put(im, x - d, y, C["yellow"])
            put(im, x, y + d, C["yellow"])
            put(im, x, y - d, C["yellow"])
    return im


def lose() -> Image.Image:
    rng = random.Random(5)
    im = Image.new("RGBA", (S, S), (0, 0, 0, 255))
    stars(im, rng, 30, 50)
    # moon
    for y in range(S):
        for x in range(S):
            if (x - 76) ** 2 + (y - 16) ** 2 <= 81:
                put(im, x, y, C["cream"] if (x - 79) ** 2 + (y - 13) ** 2 > 50 else C["black"])
    # ground
    for y in range(74, S):
        for x in range(S):
            put(im, x, y, C["dgreen"] if (x * 3 + y) % 5 else C["green"])
    # gravestone
    for y in range(30, 80):
        for x in range(30, 66):
            top = 30 + max(0, 12 - int(((x - 48) ** 2) ** 0.5 * 0.7))
            if y >= top - 6 and (x - 48) ** 2 + (y - 42) ** 2 <= 18 * 18 or y >= 42:
                edge = x in (30, 65) or y == 79
                put(im, x, y, C["dgrey"] if edge else (C["grey"] if (x + y) % 7 else C["dgrey"]))
    for (x0, y0, x1, y1) in ((47, 40, 49, 62), (41, 46, 55, 48)):    # cross
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                put(im, x, y, C["dgrey"])
    # the hat lies on the stone, the staff broken in the grass
    # only the hat: the rows above the face, left of the staff
    wiz = creature("wizard", "p1", 2)
    hat = wiz.crop((6, 0, 34, 15))
    im.alpha_composite(hat, (34, 16))
    for i in range(16):                   # the staff, broken in two
        for t in (0, 1):
            put(im, 6 + i, 86 - i // 3 + t, C["brown"])
            put(im, 70 + i, 81 + i // 4 + t, C["brown"])
    for (x, y, col) in ((22, 80, C["yellow"]), (23, 80, C["orange"]), (22, 81, C["orange"])):
        put(im, x, y, col)
    return im


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, im in (("win", win()), ("lose", lose())):
        rgb = im.convert("RGB")
        for col in rgb.get_flattened_data():
            if col not in PALETTE:
                raise SystemExit(f"{name}: colour {col} not in the Agon palette")
        rgb.convert("RGBA").save(OUT_DIR / f"{name}.png")
        rgb.resize((S * 4, S * 4), Image.NEAREST).save(ROOT / "build" / f"{name}_preview.png")
    print("[endpics] assets/title/win.png, lose.png (previews in build/)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
