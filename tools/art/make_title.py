#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Generate the title screen (GDD 11.6) as an editable PNG.

    uv run tools/art/make_title.py      # -> assets/title/title.png

A crowded wizard battle on black, in the spirit of the 8-bit loading
screens of the time - an own composition (D7: nothing traced or copied).
The figures are the game's own 24x24 creature tiles, enlarged with the
Scale2x/Scale3x pixel-art scalers (smooth diagonals, no blur, palette
kept): a huge wizard casts a bolt at a demon, a troll, a centaur, a
zombie and a dwarf fight in front, a bat and a dragon circle the magic
vortex. The logo uses a bevelled block font. Only Agon 64 colours.
tools/build_title.py compiles the PNG to build/title.bin for the SD card.
The PNG is the source of truth; re-running overwrites it.
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from palette import C, KEY_DARK, KEY_LIGHT, OWNERS, PALETTE  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
TILES = ROOT / "assets" / "tiles"
OUT = ROOT / "assets" / "title" / "title.png"
W, H = 320, 240
CLEAR = (0, 0, 0, 0)


# ---------- pixel-art scalers ----------

def scale2x(im: Image.Image) -> Image.Image:
    w, h = im.size
    src = im.load()
    out = Image.new("RGBA", (w * 2, h * 2))
    dst = out.load()

    def px(x, y):
        return src[min(max(x, 0), w - 1), min(max(y, 0), h - 1)]
    for y in range(h):
        for x in range(w):
            p = px(x, y)
            a, b, c, d = px(x, y - 1), px(x + 1, y), px(x - 1, y), px(x, y + 1)
            e0 = a if (c == a and c != d and a != b) else p
            e1 = b if (a == b and a != c and b != d) else p
            e2 = c if (d == c and d != b and c != a) else p
            e3 = d if (b == d and b != a and d != c) else p
            dst[2 * x, 2 * y], dst[2 * x + 1, 2 * y] = e0, e1
            dst[2 * x, 2 * y + 1], dst[2 * x + 1, 2 * y + 1] = e2, e3
    return out


def scale3x(im: Image.Image) -> Image.Image:
    w, h = im.size
    src = im.load()
    out = Image.new("RGBA", (w * 3, h * 3))
    dst = out.load()

    def px(x, y):
        return src[min(max(x, 0), w - 1), min(max(y, 0), h - 1)]
    for y in range(h):
        for x in range(w):
            A, B, Cc = px(x - 1, y - 1), px(x, y - 1), px(x + 1, y - 1)
            D, E, F = px(x - 1, y), px(x, y), px(x + 1, y)
            G, Hh, I = px(x - 1, y + 1), px(x, y + 1), px(x + 1, y + 1)
            e = [E] * 9
            if B != Hh and D != F:
                e[0] = D if D == B else E
                e[1] = B if (D == B and E != Cc) or (B == F and E != A) else E
                e[2] = F if B == F else E
                e[3] = D if (D == B and E != G) or (D == Hh and E != A) else E
                e[5] = F if (B == F and E != I) or (Hh == F and E != Cc) else E
                e[6] = D if D == Hh else E
                e[7] = Hh if (D == Hh and E != I) or (Hh == F and E != G) else E
                e[8] = F if Hh == F else E
            for k in range(9):
                dst[3 * x + k % 3, 3 * y + k // 3] = e[k]
    return out


def enlarge(im: Image.Image, factor: int) -> Image.Image:
    steps = {2: [2], 3: [3], 4: [2, 2], 6: [2, 3]}[factor]
    for s in steps:
        im = scale2x(im) if s == 2 else scale3x(im)
    return im


def creature(name: str, owner: str | None, factor: int, flip: bool = False) -> Image.Image:
    im = Image.open(TILES / f"{name}.png").convert("RGBA")
    if owner:
        light, dark = OWNERS[owner]
        px = im.load()
        for y in range(im.height):
            for x in range(im.width):
                r, g, b, a = px[x, y]
                if a and (r, g, b) == KEY_LIGHT:
                    px[x, y] = (*light, 255)
                elif a and (r, g, b) == KEY_DARK:
                    px[x, y] = (*dark, 255)
    if flip:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    return enlarge(im, factor)


# ---------- drawing helpers ----------

class Canvas:
    def __init__(self):
        self.im = Image.new("RGBA", (W, H), (0, 0, 0, 255))
        self.px = self.im.load()

    def put(self, x: int, y: int, rgb) -> None:
        if 0 <= x < W and 0 <= y < H:
            self.px[x, y] = (*rgb, 255)

    def paste(self, sprite: Image.Image, x: int, y: int) -> None:
        self.im.alpha_composite(sprite, (x, y)) if x >= 0 and y >= 0 else self._paste_clip(sprite, x, y)

    def _paste_clip(self, sprite, x, y):
        sp = sprite.load()
        for sy in range(sprite.height):
            for sx in range(sprite.width):
                if sp[sx, sy][3]:
                    self.put(x + sx, y + sy, sp[sx, sy][:3])


def stars(cv: Canvas, rng: random.Random) -> None:
    for _ in range(140):
        x, y = rng.randrange(W), rng.randrange(150)
        cv.put(x, y, rng.choice([C["dgrey"], C["grey"], C["white"], C["lblue"]]))


def vortex(cv: Canvas, cx: int, cy: int, rng: random.Random) -> None:
    """Swirling magic cloud: dithered rings, densest in the middle."""
    cols = [C["navy"], C["purple"], C["blue"], C["violet"], C["lviolet"], C["magenta"], C["pink"], C["white"]]
    for y in range(max(0, cy - 70), min(H, cy + 70)):
        for x in range(max(0, cx - 110), min(W, cx + 110)):
            dx, dy = (x - cx) / 105, (y - cy) / 62
            r = math.hypot(dx, dy)
            if r >= 1:
                continue
            ang = math.atan2(dy, dx)
            swirl = 0.5 + 0.5 * math.sin(ang * 3 + r * 9)
            level = (1 - r) * 0.85 + swirl * 0.35 * (1 - r)
            idx = level * (len(cols) - 1)
            base = int(idx)
            frac = idx - base
            # 2x2 ordered dither between neighbouring colours
            thr = [[0.2, 0.7], [0.95, 0.45]][y % 2][x % 2]
            k = min(len(cols) - 1, base + (1 if frac > thr else 0))
            if k == 0 and rng.random() < 0.5:
                continue                      # ragged edge
            cv.put(x, y, cols[k])


def bolt(cv: Canvas, x0, y0, x1, y1, rng: random.Random) -> None:
    pts = [(x0, y0)]
    n = 9
    for i in range(1, n):
        t = i / n
        pts.append((x0 + (x1 - x0) * t + rng.randint(-9, 9), y0 + (y1 - y0) * t + rng.randint(-7, 7)))
    pts.append((x1, y1))
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        steps = int(max(abs(bx - ax), abs(by - ay))) + 1
        for s in range(steps):
            x = round(ax + (bx - ax) * s / steps)
            y = round(ay + (by - ay) * s / steps)
            for ox in (-2, -1, 0, 1, 2):
                for oy in (-2, -1, 0, 1, 2):
                    d = abs(ox) + abs(oy)
                    if d >= 3:
                        continue
                    col = C["white"] if d == 0 else (C["sky"] if d == 1 else C["lblue"])
                    cur = cv.px[x + ox, y + oy][:3] if 0 <= x + ox < W and 0 <= y + oy < H else None
                    if cur is not None and (d == 0 or cur in ((0, 0, 0), C["navy"], C["purple"], C["blue"])):
                        cv.put(x + ox, y + oy, col)


def sparkle(cv: Canvas, x: int, y: int, size: int, col) -> None:
    for i in range(-size, size + 1):
        cv.put(x + i, y, col if abs(i) < size else C["dgrey"])
        cv.put(x, y + i, col if abs(i) < size else C["dgrey"])
    cv.put(x, y, C["white"])


# ---------- logo ----------

FONT = {
    "L": ["X....", "X....", "X....", "X....", "X....", "X....", "XXXXX"],
    "O": [".XXX.", "XX.XX", "X...X", "X...X", "X...X", "XX.XX", ".XXX."],
    "R": ["XXXX.", "X..XX", "X...X", "XXXX.", "X.XX.", "X..XX", "X...X"],
    "D": ["XXXX.", "X..XX", "X...X", "X...X", "X...X", "X..XX", "XXXX."],
    "S": [".XXXX", "XX...", "XX...", ".XXX.", "...XX", "...XX", "XXXX."],
    "F": ["XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "X...."],
    "C": [".XXXX", "XX...", "X....", "X....", "X....", "XX...", ".XXXX"],
    "H": ["X...X", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"],
    "A": ["..X..", ".XXX.", "XX.XX", "X...X", "XXXXX", "X...X", "X...X"],
}


def logo_word(cv: Canvas, word: str, x: int, y: int, s: int) -> int:
    """Bevelled gold letters: light top edge, orange lower half, dark red
    outline and a drop shadow. Returns the x after the word."""
    mask = set()
    cx = x
    for ch in word:
        if ch == " ":
            cx += 3 * s
            continue
        for ry, row in enumerate(FONT[ch]):
            for rx, v in enumerate(row):
                if v == "X":
                    for yy in range(s):
                        for xx in range(s):
                            mask.add((cx + rx * s + xx, y + ry * s + yy))
        cx += 6 * s
    top, bot = y, y + 7 * s
    for (mx, my) in mask:                                   # shadow
        for d in (1, 2, 3):
            if (mx - d, my - d) not in mask:
                cv.put(mx + d, my + d, C["dred"] if d < 3 else C["purple"])
    for (mx, my) in mask:                                   # outline
        for ox, oy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if (mx + ox, my + oy) not in mask:
                cv.put(mx + ox, my + oy, C["red"])
    for (mx, my) in mask:                                   # fill
        t = (my - top) / max(1, bot - top)
        if (mx, my - 1) not in mask:
            col = C["cream"]
        elif t < 0.45:
            col = C["yellow"]
        elif t < 0.75:
            col = C["orange"] if (mx + my) % 2 else C["yellow"]
        else:
            col = C["orange"]
        cv.put(mx, my, col)
    return cx


def main() -> int:
    rng = random.Random(7)
    cv = Canvas()
    stars(cv, rng)
    vortex(cv, 236, 52, rng)

    # back to front
    cv.paste(creature("red_dragon", None, 4), 196, 2)
    cv.paste(creature("giant_bat", "p2", 3, flip=True), 268, 70)
    cv.paste(creature("troll", "p2", 4), 6, 92)
    cv.paste(creature("zombie", "p2", 3), 58, 150)
    cv.paste(creature("demon", "p2", 4, flip=True), 96, 82)
    cv.paste(creature("centaur", "p1", 4), -26, 150)
    cv.paste(creature("dwarf", "p1", 3), 112, 168)
    wizard = creature("wizard", "p1", 6, flip=True)
    cv.paste(wizard, 182, 96)
    # the bolt leaves the staff tip (flipped tile: left side) at the demon,
    # with two side branches and sparks where it strikes
    bolt(cv, 209, 110, 146, 134, rng)
    bolt(cv, 182, 118, 160, 160, rng)
    bolt(cv, 170, 124, 128, 104, rng)
    for (x, y, s) in [(146, 134, 5), (130, 124, 3), (156, 146, 3), (138, 112, 2),
                      (209, 108, 3), (214, 84, 2), (120, 70, 2), (300, 140, 3),
                      (40, 80, 2), (88, 140, 2), (110, 40 + 120, 2)]:
        sparkle(cv, x, y, s, C["sky"])

    end = logo_word(cv, "LORDS OF", 8, 6, 3)
    logo_word(cv, "CHAOS", 8, 32, 5)
    del end

    # the bottom row stays dark for "- Taste druecken -"
    for y in range(222, H):
        for x in range(W):
            if cv.px[x, y][:3] != (0, 0, 0) and y > 225:
                cv.px[x, y] = (0, 0, 0, 255)

    out = cv.im.convert("RGB")
    for (r, g, b) in out.get_flattened_data():
        if (r, g, b) not in PALETTE:
            raise SystemExit(f"colour {(r, g, b)} not in the Agon palette")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    out.convert("RGBA").save(OUT)
    out.resize((W * 3, H * 3), Image.NEAREST).save(ROOT / "build" / "title_preview.png")
    print(f"[title] {OUT.relative_to(ROOT).as_posix()} (preview: build/title_preview.png)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
