#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Generate the first set of 24x24 tiles (own art, GDD D10) as editable PNGs.

    uv run tools/art/make_tiles.py      # -> assets/tiles/*.png, assets/icons/*.png

The PNGs are the source of truth from now on: edit them in any pixel editor
with assets/palette/agon64.gpl. Re-running this script OVERWRITES them, so
only re-run it for tiles that have not been hand-edited (use --only NAME).
"""

from __future__ import annotations

import argparse
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from palette import C, KEY_DARK, KEY_LIGHT, OWNERS, PALETTE, write_gpl  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
TILES = ROOT / "assets" / "tiles"
ICONS = ROOT / "assets" / "icons"
N = 24


# ---------------------------------------------------------------- helpers
def new(size: int = N) -> Image.Image:
    return Image.new("RGBA", (size, size), (0, 0, 0, 0))


def rgba(c):
    return (*c, 255)


def rect(im, x0, y0, x1, y1, col):
    ImageDraw.Draw(im).rectangle((x0, y0, x1, y1), fill=rgba(col))


def frame(im, x0, y0, x1, y1, col):
    ImageDraw.Draw(im).rectangle((x0, y0, x1, y1), outline=rgba(col))


def line(im, pts, col, w=1):
    ImageDraw.Draw(im).line(pts, fill=rgba(col), width=w)


def ellipse(im, box, fill=None, outline=None):
    ImageDraw.Draw(im).ellipse(box, fill=rgba(fill) if fill else None,
                               outline=rgba(outline) if outline else None)


def poly(im, pts, col):
    ImageDraw.Draw(im).polygon(pts, fill=rgba(col))


def px(im, x, y, col):
    if 0 <= x < im.width and 0 <= y < im.height:
        im.putpixel((x, y), rgba(col))


def opaque(im, x, y):
    return 0 <= x < im.width and 0 <= y < im.height and im.getpixel((x, y))[3] > 0


def outline(im, col=C["black"]):
    """1px outline around all opaque pixels (sprite look, Amiga-style)."""
    src = im.copy()
    for y in range(im.height):
        for x in range(im.width):
            if src.getpixel((x, y))[3] == 0 and any(
                    opaque(src, x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                im.putpixel((x, y), rgba(col))
    return im


def from_map(rows: list[str], legend: dict[str, tuple]) -> Image.Image:
    im = new(len(rows))
    for y, row in enumerate(rows):
        assert len(row) == len(rows), (row, len(row))
        for x, ch in enumerate(row):
            if ch != ".":
                im.putpixel((x, y), rgba(legend[ch]))
    return im


# ---------------------------------------------------------------- floors
def floor_stone():
    """Blue stone tiles like the Amiga wizard house (B1.3)."""
    im = new()
    rect(im, 0, 0, 23, 23, C["navy"])
    for y in range(N):
        row = y // 6
        for x in range(N):
            off = 6 if row % 2 else 0
            if y % 6 == 5 or (x + off) % 12 == 11:
                px(im, x, y, (0, 85, 255))
            elif y % 6 == 0 and (x + off) % 12 < 10:
                px(im, x, y, C["blue"])
    return im


def floor_wood():
    im = new()
    rect(im, 0, 0, 23, 23, C["wood"])
    for x in range(5, N, 6):
        line(im, [(x, 0), (x, 23)], C["dbrown"])
    for i, x0 in enumerate(range(0, N, 6)):
        y = (i * 9 + 4) % N
        line(im, [(x0, y), (x0 + 4, y)], C["dbrown"])
        px(im, x0 + 2, (y + 12) % N, C["lwood"])
        px(im, x0 + 1, (y + 5) % N, C["olive"])
    return im


def floor_grass(seed=7):
    im = new()
    rect(im, 0, 0, 23, 23, C["dgreen"])
    rnd = random.Random(seed)
    for _ in range(26):
        x, y = rnd.randrange(N), rnd.randrange(1, N)
        c = rnd.choice([C["green"], C["green"], C["moss"]])
        px(im, x, y, c)
        px(im, x, y - 1, c)
    for _ in range(6):
        px(im, rnd.randrange(N), rnd.randrange(N), C["lgreen"])
    return im


def floor_path(seed=3):
    im = new()
    rect(im, 0, 0, 23, 23, (170, 170, 85))
    rnd = random.Random(seed)
    for _ in range(18):
        x, y = rnd.randrange(N), rnd.randrange(N)
        px(im, x, y, rnd.choice([C["olive"], (170, 85, 85), C["grey"]]))
    for _ in range(4):
        x, y = rnd.randrange(1, N - 2), rnd.randrange(1, N - 2)
        rect(im, x, y, x + 1, y + 1, C["dgrey"])
        px(im, x, y, C["grey"])
    return im


def floor_tallgrass(seed=21):
    """Tall grass: dense golden-green blades (blocks ground sight, GDD 3.4)."""
    im = new()
    rect(im, 0, 0, 23, 23, C["olive"])
    rnd = random.Random(seed)
    for _ in range(70):
        x, y = rnd.randrange(N), rnd.randrange(3, N)
        h = rnd.randrange(3, 7)
        col = rnd.choice([(170, 170, 0), C["moss"], (170, 170, 0)])
        line(im, [(x, y), (x, y - h)], col)
        px(im, x, y - h, (255, 255, 85))
    return im


def _forest(base, crown, shade, light, trunk, seed, sparkle=None):
    im = new()
    rect(im, 0, 0, 23, 23, base)
    rnd = random.Random(seed)
    for cx, cy, r in ((6, 8, 6), (17, 6, 6), (12, 16, 7)):
        rect(im, cx - 1, cy + r - 2, cx, cy + r + 2, trunk)
        ellipse(im, (cx - r, cy - r, cx + r, cy + r - 2), fill=shade)
        ellipse(im, (cx - r, cy - r, cx + r - 2, cy + r - 4), fill=crown)
        for _ in range(4):
            px(im, cx - rnd.randrange(1, r), cy - rnd.randrange(1, r), light)
    if sparkle:
        for _ in range(5):
            px(im, rnd.randrange(N), rnd.randrange(N), sparkle)
    return im


def floor_forest():
    return _forest(C["dgreen"], C["green"], (0, 85, 0), C["lgreen"], C["dbrown"], 31)


def floor_magicwood():
    """Magic Wood: teal trees with sparkles (inspired by the Spectrum map)."""
    return _forest(C["dgreen"], (0, 170, 170), (0, 85, 85), C["cyan"], C["dbrown"], 41,
                   sparkle=C["white"])


def floor_shadowwood():
    return _forest(C["black"], C["purple"], C["navy"], C["lviolet"], C["dbrown"], 51)


def floor_swamp(seed=61):
    im = new()
    rect(im, 0, 0, 23, 23, C["olive"])
    rnd = random.Random(seed)
    for _ in range(4):
        x, y = rnd.randrange(2, 18), rnd.randrange(2, 20)
        ellipse(im, (x, y, x + rnd.randrange(4, 7), y + 2), fill=(0, 85, 85))
        px(im, x + 1, y, C["mblue"])
    for _ in range(18):
        x, y = rnd.randrange(N), rnd.randrange(4, N)
        line(im, [(x, y), (x, y - rnd.randrange(2, 5))], C["green"])
    for _ in range(10):
        px(im, rnd.randrange(N), rnd.randrange(N), C["dgreen"])
    return im


def floor_water(phase: int):
    im = new()
    rect(im, 0, 0, 23, 23, C["blue"])
    off = 3 * phase
    for row, y in enumerate((3, 9, 15, 21)):
        for x0 in range(-6, N, 12):
            x = x0 + off + (6 if row % 2 else 0)
            line(im, [(x, y), (x + 2, y - 1), (x + 4, y)], C["lblue"])
            px(im, x + 2, y - 1, C["white"] if (row + phase) % 2 == 0 else C["lblue"])
    for x, y in ((5 + off, 6), (17 - off, 18)):
        px(im, x % N, y, C["mblue"])
    return im


def floor_rubble(seed=71):
    im = new()
    rect(im, 0, 0, 23, 23, (85, 85, 0))
    rnd = random.Random(seed)
    for _ in range(14):
        x, y = rnd.randrange(1, N - 3), rnd.randrange(1, N - 3)
        w, h = rnd.randrange(2, 4), rnd.randrange(1, 3)
        rect(im, x, y, x + w, y + h, C["grey"])
        line(im, [(x, y + h + 1), (x + w, y + h + 1)], C["dgrey"])
        px(im, x, y, C["white"])
    return im


# ---------------------------------------------------------------- decor
def pentacle():
    im = new()
    pts = [(12, 2), (19, 20), (3, 9), (21, 9), (5, 20), (12, 2)]
    line(im, pts, C["white"])
    ellipse(im, (1, 1, 22, 22), outline=C["grey"])
    return im


# ---------------------------------------------------------------- walls/doors
# 3/4 front view (Amiga style, GDD 11.2): walls have a light top "cap"
# through the tile centre and a brick front face wherever no wall continues
# to the south. Vertical runs show only the narrow cap.
CAP_X = (8, 15)      # cap width of N-S runs
CAP_Y = (3, 8)       # cap height of E-W runs (foreshortened top)
FACE_BOTTOM = 20     # front face spans CAP_Y[1]+1 .. FACE_BOTTOM


def _cap_cells(mask: int) -> set:
    x0, x1 = CAP_X
    y0, y1 = CAP_Y
    cells = {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)}
    if mask & 1:
        cells |= {(x, y) for x in range(x0, x1 + 1) for y in range(0, y0)}
    if mask & 4:
        cells |= {(x, y) for x in range(x0, x1 + 1) for y in range(y1 + 1, N)}
    if mask & 2:
        cells |= {(x, y) for x in range(x1 + 1, N) for y in range(y0, y1 + 1)}
    if mask & 8:
        cells |= {(x, y) for x in range(0, x0) for y in range(y0, y1 + 1)}
    return cells


def _face_cells(mask: int) -> set:
    """Front face below the E-W cap parts that have no wall to the south."""
    x0, x1 = CAP_X
    y1 = CAP_Y[1]
    xs = set()
    if not mask & 4:
        xs |= set(range(x0, x1 + 1))
    if mask & 2:
        xs |= set(range(x1 + 1, N))
    if mask & 8:
        xs |= set(range(0, x0))
    return {(x, y) for x in xs for y in range(y1 + 1, FACE_BOTTOM + 1)}


def _draw_cap(im, cells):
    for (x, y) in cells:
        col = C["grey"]
        if (x + 2 * y) % 7 == 0:
            col = C["white"]
        im.putpixel((x, y), rgba(col))
    for (x, y) in cells:   # lit top/left rim
        if ((x, y - 1) not in cells and y > 0) or ((x - 1, y) not in cells and x > 0):
            im.putpixel((x, y), rgba(C["white"]))


def _draw_face(im, cells):
    y_top = CAP_Y[1] + 1
    for (x, y) in cells:
        r = (y - y_top) // 4
        off = 3 if r % 2 else 0
        col = C["grey"] if y < y_top + 1 else C["dgrey"]
        if (y - y_top) % 4 == 3 or (x + off) % 6 == 5:
            col = C["dgrey"]
        if y == FACE_BOTTOM:
            col = C["black"]
        if col == C["dgrey"] and (y - y_top) % 4 != 3 and (x + off) % 6 != 5:
            col = C["grey"] if (x + y) % 5 else C["dgrey"]
        im.putpixel((x, y), rgba(col))


def _outline_cells(im, cells):
    for y in range(N):
        for x in range(N):
            if (x, y) not in cells and any(
                    (x + dx, y + dy) in cells for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                im.putpixel((x, y), rgba(C["black"]))


def wall(mask: int):
    """Auto-tiled 3/4 wall. mask bits: 1=N 2=E 4=S 8=W (wall/door neighbour)."""
    im = new()
    cap, face = _cap_cells(mask), _face_cells(mask)
    face -= cap
    _draw_face(im, face)
    _draw_cap(im, cap)
    _outline_cells(im, cap | face)
    return im


def door(vertical: bool, open_: bool):
    if not vertical:
        # E-W wall: door seen from the front, set into the brick face.
        im = wall(2 | 8)
        x0, x1, y0, y1 = 6, 17, CAP_Y[1] + 1, FACE_BOTTOM
        rect(im, x0 - 1, y0 - 1, x1 + 1, y1, C["dbrown"])      # frame
        if not open_:
            rect(im, x0, y0, x1, y1 - 1, C["wood"])
            line(im, [(11, y0), (11, y1 - 1)], C["dbrown"])    # double door
            for x in (8, 14):
                line(im, [(x, y0 + 1), (x, y1 - 2)], C["olive"])
            line(im, [(x0, y0), (x1, y0)], C["lwood"])
            px(im, 10, 15, C["gold"])
            px(im, 13, 15, C["gold"])
        else:
            for y in range(y0, y1):
                for x in range(x0, x1 + 1):
                    im.putpixel((x, y), (0, 0, 0, 0))           # floor shows through
            rect(im, x0, y0, x0 + 1, y1 - 1, C["wood"])          # leaves folded back
            rect(im, x1 - 1, y0, x1, y1 - 1, C["wood"])
        return im
    # N-S wall: only the cap is visible; the door is a wooden strip in it.
    im = wall(1 | 4)
    x0, x1 = CAP_X
    y0, y1 = 5, 18
    if not open_:
        rect(im, x0, y0, x1, y1, C["wood"])
        for y in (8, 12, 16):
            line(im, [(x0, y), (x1, y)], C["dbrown"])
        line(im, [(x0, y0), (x0, y1)], C["lwood"])
        frame(im, x0 - 1, y0 - 1, x1 + 1, y1 + 1, C["black"])
        px(im, x1 - 1, 12, C["gold"])
    else:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                im.putpixel((x, y), (0, 0, 0, 0))
        line(im, [(x0 - 1, y0 - 1), (x1 + 1, y0 - 1)], C["black"])
        line(im, [(x0 - 1, y1 + 1), (x1 + 1, y1 + 1)], C["black"])
        rect(im, x1 + 1, y0, x1 + 3, y1 - 6, C["wood"])          # leaf swung open
        frame(im, x1 + 1, y0 - 1, x1 + 4, y1 - 5, C["black"])
    return im


# ---------------------------------------------------------------- furniture
# 3/4 view: light top surface, darker front face, legs/feet below.
def shadow(im, x0, x1, y):
    """Dithered drop shadow under standing objects/creatures."""
    for x in range(x0, x1 + 1):
        for yy in (y, y + 1):
            if (x + yy) % 2 == 0 and im.getpixel((x, yy))[3] == 0:
                im.putpixel((x, yy), (0, 0, 0, 255))


def bed():
    im = new()
    rect(im, 3, 0, 20, 5, C["dbrown"])            # headboard (front face)
    line(im, [(3, 0), (20, 0)], C["wood"])
    rect(im, 4, 6, 19, 17, C["white"])            # mattress top
    rect(im, 6, 6, 17, 9, C["sky"])               # pillow
    rect(im, 4, 11, 19, 17, C["mblue"])           # blanket
    line(im, [(4, 11), (19, 11)], C["lblue"])
    line(im, [(4, 14), (19, 14)], C["blue"])
    rect(im, 3, 18, 20, 20, C["wood"])            # footboard front
    line(im, [(3, 18), (20, 18)], C["lwood"])
    rect(im, 3, 21, 4, 22, C["dbrown"])
    rect(im, 19, 21, 20, 22, C["dbrown"])
    outline(im)
    shadow(im, 5, 18, 22)
    return im


def table():
    im = new()
    rect(im, 2, 6, 21, 12, C["lwood"])            # top surface
    for y in (8, 10):
        line(im, [(3, y), (20, y)], C["wood"])
    rect(im, 2, 13, 21, 15, C["wood"])            # front edge
    line(im, [(2, 15), (21, 15)], C["dbrown"])
    for x in (3, 19):
        rect(im, x, 16, x + 1, 21, C["dbrown"])   # legs
    rect(im, 6, 7, 10, 10, C["red"])              # book
    line(im, [(6, 10), (10, 10)], C["cream"])
    rect(im, 15, 4, 16, 9, C["cream"])            # candle
    px(im, 15, 3, C["yellow"])
    px(im, 16, 2, C["orange"])
    outline(im)
    shadow(im, 4, 19, 22)
    return im


def chair():
    im = new()
    rect(im, 7, 2, 16, 10, C["wood"])             # backrest (front)
    rect(im, 9, 4, 14, 8, C["dbrown"])
    line(im, [(7, 2), (16, 2)], C["lwood"])
    rect(im, 6, 11, 17, 14, C["lwood"])           # seat top
    rect(im, 6, 15, 17, 16, C["wood"])            # seat front
    for x in (6, 16):
        rect(im, x, 17, x + 1, 21, C["dbrown"])
    outline(im)
    shadow(im, 7, 16, 22)
    return im


def bookshelf():
    im = new()
    rect(im, 2, 0, 21, 2, C["lwood"])             # top
    rect(im, 2, 3, 21, 22, C["dbrown"])           # front frame
    rnd = random.Random(11)
    cols = [C["red"], C["blue"], C["green"], C["gold"], C["violet"], C["mblue"], C["cream"]]
    for top in (4, 10, 16):
        rect(im, 3, top, 20, top + 4, C["black"])
        x = 4
        while x < 20:
            w = rnd.choice((1, 2, 2))
            h = rnd.choice((3, 4, 4, 5))
            rect(im, x, top + 5 - h, min(x + w - 1, 19), top + 4, rnd.choice(cols))
            x += w + (1 if rnd.random() < 0.2 else 0)
        line(im, [(3, top + 5), (20, top + 5)], C["wood"])
    return outline(im)


def drawers():
    """Kommode: chest of drawers (container, GDD 3.3)."""
    im = new()
    rect(im, 3, 4, 20, 7, C["lwood"])             # top surface
    rect(im, 3, 8, 20, 20, C["wood"])             # front
    for y in (9, 13, 17):
        frame(im, 4, y, 19, y + 2, C["dbrown"])
        px(im, 9, y + 1, C["gold"])
        px(im, 14, y + 1, C["gold"])
    rect(im, 4, 21, 5, 22, C["dbrown"])
    rect(im, 18, 21, 19, 22, C["dbrown"])
    rect(im, 6, 5, 8, 6, C["gold"])               # a small box on top
    outline(im)
    return im


def chest():
    im = new()
    rect(im, 4, 7, 19, 11, C["lwood"])            # lid top (rounded)
    line(im, [(5, 6), (18, 6)], C["lwood"])
    rect(im, 4, 12, 19, 19, C["wood"])            # front
    line(im, [(4, 12), (19, 12)], C["dbrown"])
    for x in (7, 16):
        line(im, [(x, 6), (x, 19)], C["gold"])
    rect(im, 11, 12, 12, 15, C["gold"])
    px(im, 11, 14, C["black"])
    outline(im)
    shadow(im, 5, 18, 21)
    return im


def cauldron():
    im = new()
    for x in (6, 16):
        rect(im, x, 18, x + 1, 21, C["dgrey"])
    ellipse(im, (3, 8, 20, 20), fill=C["dgrey"])  # pot body (front)
    ellipse(im, (5, 10, 18, 19), fill=C["black"])
    line(im, [(6, 15), (8, 18)], C["grey"])       # highlight
    ellipse(im, (3, 5, 20, 11), fill=C["grey"])   # rim
    ellipse(im, (5, 6, 18, 10), fill=C["mblue"])  # liquid
    ellipse(im, (7, 7, 12, 8), fill=C["lblue"])
    px(im, 15, 7, C["sky"])
    outline(im)
    shadow(im, 5, 18, 22)
    return im


def candle(phase: int):
    im = new()
    rect(im, 8, 19, 15, 20, C["grey"])            # foot
    line(im, [(8, 19), (15, 19)], C["white"])
    rect(im, 11, 11, 12, 18, C["dgrey"])          # shaft
    rect(im, 8, 9, 15, 10, C["grey"])             # dish
    rect(im, 10, 4, 13, 8, C["red"])              # candle
    line(im, [(10, 4), (10, 8)], C["bred"])
    dx = 0 if phase == 0 else 1
    rect(im, 11 + dx, 1, 12 + dx, 3, C["orange"])
    px(im, 11 + dx, 0, C["yellow"])
    px(im, 12 - dx, 2, C["yellow"])
    outline(im)
    shadow(im, 8, 15, 21)
    return im


def rock():
    """Boulder (blocking feature, 3/4: light top, darker front)."""
    im = new()
    ellipse(im, (3, 7, 20, 21), fill=C["dgrey"])
    ellipse(im, (4, 5, 19, 16), fill=C["grey"])
    ellipse(im, (6, 6, 12, 10), fill=C["white"])
    line(im, [(9, 13), (13, 18)], C["dgrey"])
    line(im, [(14, 9), (16, 12)], C["dgrey"])
    outline(im)
    shadow(im, 4, 19, 22)
    return im


def tree():
    im = new()
    rect(im, 10, 14, 13, 21, C["brown"])
    line(im, [(10, 14), (10, 21)], C["dbrown"])
    ellipse(im, (3, 0, 20, 16), fill=C["green"])
    ellipse(im, (8, 6, 20, 16), fill=C["dgreen"])
    ellipse(im, (5, 2, 13, 9), fill=C["moss"])
    for x, y in ((7, 4), (10, 3), (6, 7), (15, 5)):
        px(im, x, y, C["lgreen"])
    outline(im)
    shadow(im, 6, 17, 22)
    return im


def rug():
    """Flat floor decor, slightly foreshortened like the Amiga cushions."""
    im = new()
    rect(im, 2, 4, 21, 19, C["red"])
    frame(im, 2, 4, 21, 19, C["gold"])
    frame(im, 4, 6, 19, 17, C["gold"])
    poly(im, [(12, 7), (17, 11), (12, 16), (7, 11)], C["pink"])
    poly(im, [(12, 9), (14, 11), (12, 13), (10, 11)], C["gold"])
    for x in range(3, 21, 2):
        px(im, x, 3, C["cream"])
        px(im, x, 20, C["cream"])
    return im


# ---------------------------------------------------------------- creatures
def wizard():
    im = new()
    poly(im, [(5, 20), (8, 11), (15, 11), (18, 20)], KEY_LIGHT)      # robe
    poly(im, [(12, 11), (15, 11), (18, 20), (13, 20)], KEY_DARK)
    rect(im, 9, 7, 14, 11, C["skin"])                                # face
    poly(im, [(9, 10), (14, 10), (12, 15), (11, 15)], C["white"])    # beard
    px(im, 10, 8, C["black"])
    px(im, 13, 8, C["black"])
    poly(im, [(11, 0), (16, 7), (7, 7)], KEY_LIGHT)                  # hat
    poly(im, [(12, 1), (16, 7), (13, 7)], KEY_DARK)
    rect(im, 6, 7, 17, 7, KEY_DARK)
    px(im, 11, 3, C["yellow"])
    rect(im, 6, 13, 7, 15, C["skin"])
    rect(im, 8, 20, 10, 20, C["dbrown"])                             # feet
    rect(im, 13, 20, 15, 20, C["dbrown"])
    line(im, [(19, 4), (19, 21)], C["wood"])                         # staff
    rect(im, 18, 2, 20, 4, C["yellow"])
    px(im, 19, 3, C["white"])
    rect(im, 17, 13, 18, 14, C["skin"])
    outline(im)
    shadow(im, 6, 18, 22)
    return im


def goblin():
    im = new()
    rect(im, 8, 13, 15, 18, C["brown"])
    line(im, [(8, 16), (15, 16)], C["dbrown"])
    rect(im, 9, 19, 10, 21, C["green"])
    rect(im, 13, 19, 14, 21, C["green"])
    ellipse(im, (7, 4, 16, 13), fill=C["green"])
    poly(im, [(7, 7), (3, 5), (7, 10)], C["green"])
    poly(im, [(16, 7), (20, 5), (16, 10)], C["green"])
    px(im, 10, 8, C["yellow"])
    px(im, 13, 8, C["yellow"])
    line(im, [(10, 11), (13, 11)], C["dgreen"])
    rect(im, 5, 14, 6, 17, C["green"])
    line(im, [(4, 18), (2, 10)], C["dbrown"], 2)
    rect(im, 1, 8, 3, 11, C["wood"])
    outline(im)
    shadow(im, 6, 17, 22)
    return im


# ---------------------------------------------------------------- objects/ui
def scroll():
    im = new()
    rect(im, 7, 14, 16, 18, C["cream"])
    rect(im, 6, 13, 7, 19, C["lwood"])
    rect(im, 16, 13, 17, 19, C["lwood"])
    line(im, [(9, 16), (14, 16)], C["tan"])
    return outline(im)


def remembered():
    im = new()
    for y in range(N):
        for x in range(N):
            if (x + y) % 2 == 0:
                im.putpixel((x, y), (0, 0, 0, 255))
    return im


def cursor(col):
    im = new()
    for (x0, y0, sx, sy) in ((0, 0, 1, 1), (23, 0, -1, 1), (0, 23, 1, -1), (23, 23, -1, -1)):
        for i in range(6):
            for t in range(2):
                px(im, x0 + sx * i, y0 + sy * t, col)
                px(im, x0 + sx * t, y0 + sy * i, col)
    return im


ICON_MAPS = {
    "boot":   ["..kkk...", "..kgk...", "..kgk...", "..kgk...", ".kggkkk.", ".kggggk.", ".kkkkkk.", "........"],
    "bolt":   ["....kyk.", "...kyk..", "..kyyyk.", ".kyyyk..", "...kyk..", "..kyk...", "..kk....", "........"],
    "heart":  [".kk.kk..", "krrkrrk.", "krrrrrk.", "krrrrrk.", ".krrrk..", "..krk...", "...k....", "........"],
    "sword":  [".....kk.", "....kwk.", "...kwk..", "k.kwk...", "kkwk....", ".kgk....", "kk.k....", "........"],
    "shield": ["kkkkkkk.", "kbbwbbk.", "kbbwbbk.", "kwwwwwk.", ".kbwbk..", "..kbk...", "...k....", "........"],
    "star":   ["...k....", "..kpk...", "kkkpkkk.", ".kpppk..", "..kpk...", ".kp.pk..", ".k...k..", "........"],
    # status icons (PM 11): undead, flying, mount, fatal wound, invisible
    "st_undead":    [".kkkkk..", "kwwwwwk.", "kwkwkwk.", "kwwwwwk.", ".kwkwk..", ".kwwwk..", "..kkk...", "........"],
    "st_fly":       ["k.......", "kck.....", "kcck....", ".kccck..", "..kccck.", "...kkkk.", "........", "........"],
    "st_mount":     ["....kk..", "...kyyk.", "kkkyyyk.", "kyyyyk..", "kykkyk..", "kk..kk..", "........", "........"],
    "st_wound":     ["...k....", "..krk...", "..krk...", ".krrrk..", "krrrrrk.", ".krrrk..", "..kkk...", "........"],
    "st_invisible": ["........", ".kkkkk..", "kgwwwgk.", "kwgkgwk.", "kgwwwgk.", ".kkkkk..", "k.....k.", "........"],
}
ICON_LEGEND = {"k": C["black"], "g": C["green"], "y": C["yellow"], "r": C["bred"],
               "w": C["white"], "b": C["blue"], "p": C["pink"], "c": C["lblue"]}


def icon(rows):
    im = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != ".":
                im.putpixel((x, y), rgba(ICON_LEGEND[ch]))
    return im


def owner_variant(im, owner):
    light, dark = OWNERS[owner]
    out = im.copy()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = out.getpixel((x, y))
            if a and (r, g, b) == KEY_LIGHT:
                out.putpixel((x, y), rgba(light))
            elif a and (r, g, b) == KEY_DARK:
                out.putpixel((x, y), rgba(dark))
    return out


# ---------------------------------------------------------------- main
def all_tiles() -> dict[str, Image.Image]:
    t = {
        "floor_stone": floor_stone(), "floor_wood": floor_wood(),
        "floor_grass": floor_grass(), "floor_path": floor_path(),
        "floor_tallgrass": floor_tallgrass(), "floor_forest": floor_forest(),
        "floor_magicwood": floor_magicwood(), "floor_shadowwood": floor_shadowwood(),
        "floor_swamp": floor_swamp(), "floor_water_0": floor_water(0),
        "floor_water_1": floor_water(1), "floor_rubble": floor_rubble(), "rock": rock(),
        "decor_rug": rug(), "decor_pentacle": pentacle(),
        "door_h_closed": door(False, False), "door_h_open": door(False, True),
        "door_v_closed": door(True, False), "door_v_open": door(True, True),
        "bed": bed(), "table": table(), "chair": chair(), "bookshelf": bookshelf(),
        "drawers": drawers(), "chest": chest(), "cauldron": cauldron(),
        "candle_0": candle(0), "candle_1": candle(1), "tree": tree(),
        "wizard": wizard(), "goblin": goblin(), "obj_scroll": scroll(),
        "overlay_remembered": remembered(),
        "cursor_white": cursor(C["white"]), "cursor_green": cursor(C["lgreen"]),
        "cursor_yellow": cursor(C["yellow"]), "cursor_red": cursor(C["bred"]),
    }
    for m in range(16):
        t[f"wall_{m:02d}"] = wall(m)
    from creatures import all_creatures   # 25 creatures (D13), separate module
    t.update(all_creatures())
    return t


def check_palette(name, im):
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = im.getpixel((x, y))
            if a and (r, g, b) not in PALETTE:
                raise SystemExit(f"{name}: colour {(r, g, b)} not in Agon palette")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--only", nargs="*", help="regenerate only these tile names")
    args = ap.parse_args()
    TILES.mkdir(parents=True, exist_ok=True)
    ICONS.mkdir(parents=True, exist_ok=True)
    (ROOT / "assets" / "palette").mkdir(parents=True, exist_ok=True)
    write_gpl(ROOT / "assets" / "palette" / "agon64.gpl")
    n = 0
    for name, im in all_tiles().items():
        if args.only and name not in args.only:
            continue
        check_palette(name, im)
        im.save(TILES / f"{name}.png")
        n += 1
    for name, rows in ICON_MAPS.items():
        icon(rows).save(ICONS / f"{name}.png")
    print(f"[art] {n} tiles -> {TILES.relative_to(ROOT)}, {len(ICON_MAPS)} icons -> {ICONS.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
