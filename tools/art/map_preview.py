#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Draw a whole map from the host build's tile layers (graphics review aid).

    uv run tools/art/map_preview.py [map 0-4 | file.map] [x0 y0 w h] [--viewer X Y]   -> build/map_preview.png

Needs build/host/loc_host and build/tiles.bin (uv run tools/build.py --all).
"""

from __future__ import annotations

import subprocess
import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent.parent
args = sys.argv[1:]
viewer: list[str] = []
if "--viewer" in args:                      # --viewer X Y: the roof opens on what that figure sees
    k = args.index("--viewer")
    viewer = args[k + 1:k + 3]
    del args[k:k + 3]
sys.argv = [sys.argv[0], *args]
which = sys.argv[1] if len(sys.argv) > 1 else "3"
flag = "--map-file" if which.endswith(".map") else "--map-layers"   # a path or a compiled-in index
if sys.platform == "win32":
    cmd = ["wsl.exe", "-e", "bash", "-c",
           f"cd /mnt/c/{ROOT.as_posix()[3:]} && build/host/loc_host {flag} {which} {' '.join(viewer)}"]
else:
    cmd = [str(ROOT / "build" / "host" / "loc_host"), flag, which, *viewer]
out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout.splitlines()
mw, mh = map(int, out[0].split())
x0, y0, w, h = (map(int, sys.argv[2:6]) if len(sys.argv) >= 6 else (0, 0, mw, mh))

data = (ROOT / "build" / "tiles.bin").read_bytes()
count = struct.unpack_from("<H", data, 5)[0]
off = 7 + 2 * count
tiles = []
for i in range(count):
    px = data[off + i * 576: off + (i + 1) * 576]
    im = Image.new("RGBA", (24, 24))
    for k, b in enumerate(px):
        r, g, bl, a = (b & 3) * 85, ((b >> 2) & 3) * 85, ((b >> 4) & 3) * 85, 255 if b >> 6 else 0
        im.putpixel((k % 24, k // 24), (r, g, bl, a))
    tiles.append(im)

sheet = Image.new("RGBA", (w * 24, h * 24), (0, 0, 0, 255))
for y in range(y0, y0 + h):
    for x in range(x0, x0 + w):
        for t in out[1 + y * mw + x].split():
            sheet.alpha_composite(tiles[int(t)], ((x - x0) * 24, (y - y0) * 24))
(ROOT / "build" / "map_preview.png").parent.mkdir(exist_ok=True)
sheet.save(ROOT / "build" / "map_preview.png")
print("build/map_preview.png", sheet.size)
