#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Render a 320x240 mockup of the game screen from assets/tiles (GDD 11.1/11.3).

    uv run tools/mockup.py            # -> docs/design/mockups/wizard-house.png (x3)
    uv run tools/mockup.py --sheet    # also a tile overview sheet

Layers per field: floor, decor, feature, object, ground unit, overlay; the
cursor is drawn last (a hardware sprite on the Agon). Text uses a stand-in
pixel font; the Agon draws its own 8x8 system font.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
TILES = ROOT / "assets" / "tiles"
ICONS = ROOT / "assets" / "icons"
OUT = ROOT / "docs" / "design" / "mockups"
T = 24
W, H = 320, 240
SCALE = 3

# ---- scene: same source as the game (data/maps/wizard_house.txt) ------
sys.path.insert(0, str(ROOT / "tools"))
from gen_maps import parse  # noqa: E402

_MAP = parse(ROOT / "data" / "maps" / "wizard_house.txt")
FLOOR = [_MAP["floor"][i * 9:(i + 1) * 9] for i in range(9)]
FEATURE = [_MAP["feature"][i * 9:(i + 1) * 9] for i in range(9)]
DECOR = [_MAP["decor"][i * 9:(i + 1) * 9] for i in range(9)]
_OWN = {"OWN_P1": "_p1", "OWN_P2": "_p2", "OWN_P3": "_p3", "OWN_P4": "_p4", "OWN_NEUTRAL": "_neutral"}
UNITS = {(x, y): k[3:].lower() + _OWN[o] for x, y, k, o in _MAP["units"]}
OBJECTS = {(x, y): t[2:].lower() for x, y, t in _MAP["objects"]}
REMEMBERED = {(x, y) for x in (6, 7, 8) for y in (6, 7, 8)}
CURSOR = (next(iter(UNITS)), "cursor_green")

FEATURE_TILES = {"B": "bed", "S": "bookshelf", "K": "candle_0", "C": "cauldron",
                 "T": "table", "h": "chair", "M": "drawers", "X": "chest", "t": "tree"}
FLOOR_TILES = {"s": "floor_stone", "w": "floor_wood", "g": "floor_grass", "p": "floor_path"}

PANEL_BG = (0, 0, 0)
BARS = [  # (icon, colour, fill 0..1) - Amiga order/colours (B2.4)
    ("boot", (85, 170, 0), 0.60), ("bolt", (255, 170, 0), 0.85),
    ("heart", (255, 0, 0), 0.70), ("sword", (170, 170, 170), 0.25),
    ("shield", (0, 85, 255), 0.30), ("star", (255, 85, 170), 0.90),
]


def tile(name: str) -> Image.Image:
    if name.endswith(("_p1", "_p2", "_p3", "_p4", "_neutral")):
        base, owner = name.rsplit("_", 1)
        sys.path.insert(0, str(ROOT / "tools" / "art"))
        from make_tiles import owner_variant  # noqa: E402
        return owner_variant(Image.open(TILES / f"{base}.png").convert("RGBA"), owner)
    return Image.open(TILES / f"{name}.png").convert("RGBA")


def wall_mask(x: int, y: int) -> int:
    def solid(xx, yy):
        return 0 <= yy < 9 and 0 <= xx < 9 and FEATURE[yy][xx] in "#Dd"
    return (1 if solid(x, y - 1) else 0) | (2 if solid(x + 1, y) else 0) | \
           (4 if solid(x, y + 1) else 0) | (8 if solid(x - 1, y) else 0)


def font():
    try:
        return ImageFont.truetype("cour.ttf", 9)
    except OSError:
        return ImageFont.load_default()


def text(draw, xy, s, col):
    draw.fontmode = "1"
    draw.text(xy, s, fill=col, font=font())


def floor_at(x: int, y: int) -> str:
    """Floor of a field; outside the mockup area counts as grass."""
    if 0 <= x < 9 and 0 <= y < 9:
        return FLOOR_TILES[FLOOR[y][x]]
    return "floor_grass"


def is_wall(x: int, y: int) -> bool:
    return 0 <= x < 9 and 0 <= y < 9 and FEATURE[y][x] in "#Dd"


# Wall tiles are split like on the Amiga (B2): each side of the wall line
# shows the floor of the neighbour on that side ("half floors", GDD 11.3).
HALF = {(0, -1): (0, 0, 24, 3), (0, 1): (0, 9, 24, 24),
        (-1, 0): (0, 0, 8, 24), (1, 0): (16, 0, 24, 24)}


def draw_floor(img: Image.Image, x: int, y: int) -> None:
    ox, oy = x * T, y * T
    img.alpha_composite(tile(floor_at(x, y)), (ox, oy))
    if not is_wall(x, y):
        return
    for (dx, dy), box in HALF.items():
        nx, ny = x + dx, y + dy
        if not is_wall(nx, ny):
            part = tile(floor_at(nx, ny)).crop(box)
            img.alpha_composite(part, (ox + box[0], oy + box[1]))


def render_map(img: Image.Image) -> None:
    for y in range(9):
        for x in range(9):
            ox, oy = x * T, y * T
            draw_floor(img, x, y)
            if DECOR[y][x] == "r":
                img.alpha_composite(tile("decor_rug"), (ox, oy))
            f = FEATURE[y][x]
            if f == "#":
                img.alpha_composite(tile(f"wall_{wall_mask(x, y):02d}"), (ox, oy))
            elif f in "Dd":
                vertical = is_wall(x, y - 1) or is_wall(x, y + 1)
                name = f"door_{'v' if vertical else 'h'}_{'open' if f == 'd' else 'closed'}"
                img.alpha_composite(tile(name), (ox, oy))
            elif f in FEATURE_TILES:
                img.alpha_composite(tile(FEATURE_TILES[f]), (ox, oy))
            if (x, y) in OBJECTS:
                img.alpha_composite(tile(OBJECTS[(x, y)]), (ox, oy))
            if (x, y) in UNITS:
                img.alpha_composite(tile(UNITS[(x, y)]), (ox, oy))
            if (x, y) in REMEMBERED:
                img.alpha_composite(tile("overlay_remembered"), (ox, oy))
    (cx, cy), cname = CURSOR
    img.alpha_composite(tile(cname), (cx * T, cy * T))


def render_panel(img: Image.Image) -> None:
    """Same layout as src/agon/render.c render_panel() (8x8 text grid)."""
    d = ImageDraw.Draw(img)
    x0 = 9 * T
    d.rectangle((x0, 0, W - 1, 215), fill=PANEL_BG)
    d.rectangle((x0 + 4, 4, x0 + 31, 31), outline=(85, 85, 255))
    img.alpha_composite(tile("wizard_p1"), (x0 + 6, 6))
    text(d, (256, 8), "Stufe 1", (170, 170, 170))
    text(d, (216, 40), "Zauberer-1", (255, 255, 255))
    text(d, (216, 48), "AP 24", (85, 255, 85))
    text(d, (272, 48), "Ma 80", (255, 85, 255))
    top, bottom = 58, 168
    for i, (ic, col, fill) in enumerate(BARS):
        bx = x0 + 8 + i * 16
        d.rectangle((bx, top, bx + 7, bottom), outline=tuple(c // 2 for c in col))
        fy = bottom - int((bottom - top - 2) * fill)
        d.rectangle((bx + 1, fy, bx + 6, bottom - 1), fill=col)
        img.alpha_composite(Image.open(ICONS / f"{ic}.png").convert("RGBA"), (bx, bottom + 4))
    text(d, (216, 184), "Am Boden:", (170, 170, 170))
    text(d, (216, 192), "Teppich", (255, 255, 255))


def render_messages(img: Image.Image) -> None:
    d = ImageDraw.Draw(img)
    d.rectangle((0, 216, W - 1, H - 1), fill=(0, 0, 0))
    text(d, (2, 216), "Zauberer-1 tritt auf den Teppich.", (255, 255, 170))
    text(d, (2, 224), "Goblin gesichtet!", (255, 85, 85))
    text(d, (2, 232), "c Zaubern  g Aufheben  a Oeffnen  E Zugende", (85, 170, 255))


def tile_sheet() -> Image.Image:
    names = sorted(p.stem for p in TILES.glob("*.png"))
    cols = 10
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * (T + 4), rows * (T + 4)), (40, 40, 40, 255))
    for i, n in enumerate(names):
        sheet.alpha_composite(tile(n), ((i % cols) * (T + 4) + 2, (i // cols) * (T + 4) + 2))
    return sheet


def main() -> int:
    ap = argparse.ArgumentParser(description="Render the game screen mockup")
    ap.add_argument("--sheet", action="store_true", help="also write tiles-overview.png")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    render_map(img)
    render_panel(img)
    render_messages(img)
    out = OUT / "wizard-house.png"
    img.resize((W * SCALE, H * SCALE), Image.NEAREST).save(out)
    print(f"[mockup] {out.relative_to(ROOT)} ({W}x{H} x{SCALE})")
    if args.sheet:
        sheet = tile_sheet()
        sp = OUT / "tiles-overview.png"
        sheet.resize((sheet.width * SCALE, sheet.height * SCALE), Image.NEAREST).save(sp)
        print(f"[mockup] {sp.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
