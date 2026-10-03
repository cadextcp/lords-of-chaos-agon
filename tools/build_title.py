#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Compile assets/title/title.png into the SD title bitmap (M5d, ADR 0011).

    uv run tools/build_title.py

Output (generated, not committed):
  build/title.bin   RGBA2222 image, streamed to /loc/title.bin

Format: "LOCB" | u8 version=1 | u16 w | u16 h | pixels (RGBA2222, rows
top to bottom: bits 0-1 R, 2-3 G, 4-5 B, 6-7 A). The palette and
alpha rule are the same as for tiles (build_tiles.py).
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "art"))
from palette import PALETTE  # noqa: E402

SRC = ROOT / "assets" / "title" / "title.png"
OUT = ROOT / "build" / "title.bin"


def rgba2222(r: int, g: int, b: int, a: int) -> int:
    if a == 0:
        return 0
    return (r >> 6) | ((g >> 6) << 2) | ((b >> 6) << 4) | (3 << 6)


def main() -> int:
    im = Image.open(SRC).convert("RGBA")
    if im.size != (320, 240):
        raise SystemExit(f"expected 320x240, got {im.size}")
    out = bytearray(b"LOCB")
    out += struct.pack("<BHH", 1, im.width, im.height)
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = im.getpixel((x, y))
            if a not in (0, 255):
                raise SystemExit(f"semi-transparent pixel at {x},{y}")
            if a and (r, g, b) not in PALETTE:
                raise SystemExit(f"colour {(r, g, b)} at {x},{y} not in Agon palette")
            out.append(rgba2222(r, g, b, a))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(bytes(out))
    print(f"[title] {OUT.relative_to(ROOT)} ({len(out)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
