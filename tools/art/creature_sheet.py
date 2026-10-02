#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""Review sheet: every creature on grass in owner colours p1 and neutral
-> docs/design/mockups/creatures.png (x3)."""

import csv
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "art"))
from make_tiles import owner_variant  # noqa: E402

TILES = ROOT / "assets" / "tiles"
rows = [r for r in csv.DictReader(l for l in open(ROOT / "data" / "creatures.csv", encoding="utf-8")
                                  if not l.startswith("#"))]
cols, cell_w, cell_h = 6, 70, 40
sheet = Image.new("RGBA", (cols * cell_w, ((len(rows) + cols - 1) // cols) * cell_h), (24, 24, 24, 255))
grass = Image.open(TILES / "floor_grass.png").convert("RGBA")
d = ImageDraw.Draw(sheet)
try:
    font = ImageFont.truetype("arial.ttf", 8)
except OSError:
    font = ImageFont.load_default()
for i, r in enumerate(rows):
    x, y = (i % cols) * cell_w + 4, (i // cols) * cell_h + 2
    tile = Image.open(TILES / f"{r['id']}.png").convert("RGBA")
    for j, owner in enumerate(("p1", "neutral")):
        sheet.alpha_composite(grass, (x + j * 26, y))
        sheet.alpha_composite(owner_variant(tile, owner), (x + j * 26, y))
    d.text((x, y + 26), r["name"], fill=(230, 230, 230), font=font)
out = ROOT / "docs" / "design" / "mockups" / "creatures.png"
sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(out)
print(f"[sheet] {out.relative_to(ROOT)}")
