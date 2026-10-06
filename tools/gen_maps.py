#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Compile data/maps/<name>.txt into binary maps (ADR 0008).

    uv run tools/gen_maps.py

Outputs (generated, not committed):
  build/maps/<name>.map     loaded by the game from /loc/maps on the SD card
  src/core/gen/maps.c       the same bytes as C arrays (selftest, host build)

.map format v2 (all u8 unless noted):
  "LOCM" | version=1 | tile_count u16 LE | w | h | wrap
  | floor[w*h] | feature[w*h] | decor[w*h]          (enum values, row-major)
  | unit_count | units: x y kind owner
  | object_count | objects: x y tile_lo tile_hi   (v2: 16-bit tile ids)

Enum values are read from src/core/world.h / map_def.h / gen/tiles.h, so
the C side and this compiler cannot drift apart. The text format is
documented at the top of data/maps/wizard_house.txt.
"""

from __future__ import annotations

import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAPS = ROOT / "data" / "maps"
OUT_DIR = ROOT / "build" / "maps"
OUT_C = ROOT / "src" / "core" / "gen" / "maps.c"
OUT_H = ROOT / "src" / "core" / "gen" / "maps.h"
CORE = ROOT / "src" / "core"

# Character legends -> C enum names (values come from the headers).
FLOOR = {"s": "FL_STONE", "w": "FL_WOOD", "g": "FL_GRASS", "p": "FL_PATH",
         '"': "FL_TALL_GRASS", "f": "FL_FOREST", "m": "FL_MAGIC_WOOD",
         "n": "FL_SHADOW_WOOD", "u": "FL_SWAMP", "~": "FL_WATER", "r": "FL_RUBBLE",
         "b": "FL_BRIDGE"}
FEATURE = {".": "FE_NONE", "#": "FE_WALL", "D": "FE_DOOR_CLOSED", "d": "FE_DOOR_OPEN",
           "B": "FE_BED", "S": "FE_BOOKSHELF", "K": "FE_CANDLE", "C": "FE_CAULDRON",
           "T": "FE_TABLE", "h": "FE_CHAIR", "M": "FE_DRAWERS", "X": "FE_CHEST",
           "t": "FE_TREE", "R": "FE_ROCK", "L": "FE_DOOR_LOCKED", "x": "FE_CHEST_FREE",
           "W": "FE_WINDOW", "F": "FE_FENCE"}
DECOR = {".": "DE_NONE", "r": "DE_RUG", "*": "DE_PENTACLE", "f": "DE_FLOWERS",
         "o": "DE_MUSHROOMS"}
ROOF = {".": "0", "R": "1"}                # R = roof tile (blocks sight+landing)
OWNERS = {"p1": "OWN_P1", "p2": "OWN_P2", "p3": "OWN_P3", "p4": "OWN_P4",
          "neutral": "OWN_NEUTRAL"}


def c_enums(*headers: Path) -> dict[str, int]:
    """Values of all enumerators in the given headers (simple C enums)."""
    values: dict[str, int] = {}
    for h in headers:
        text = re.sub(r"/\*.*?\*/", "", h.read_text(encoding="utf-8"), flags=re.S)
        for body in re.findall(r"enum\s*\{(.*?)\}", text, flags=re.S):
            nxt = 0
            for item in body.split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    name, val = (x.strip() for x in item.split("=", 1))
                    nxt = int(val, 0) if re.fullmatch(r"-?(0x)?[0-9a-fA-F]+", val) else values[val]
                else:
                    name = item
                values[name] = nxt
                nxt += 1
    return values


def parse(path: Path) -> dict:
    """Text map -> dict with char grids (also used by tools/mockup.py)."""
    m = {"wrap": 0, "units": [], "objects": [], "portal": None, "roof": None}
    section = None
    grids: dict[str, list[str]] = {"floor": [], "feature": [], "decor": [],
                                   "roof": []}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.rstrip()
        if section not in grids and line.startswith("#"):
            continue
        if not line.strip():
            if section in grids and grids[section]:
                section = None
            continue
        word = line.split()[0]
        if word == "size":
            m["w"], m["h"] = map(int, line.split()[1:3])
        elif word == "wrap":
            m["wrap"] = int(line.split()[1])
        elif word == "portal":
            vals = list(map(int, line.split()[1:6]))
            if len(vals) == 4:
                vals.append(0)               # span 0: the portal stays open
            m["portal"] = tuple(vals)
        elif word in ("floor", "feature", "decor", "roof", "units", "objects") and len(line.split()) == 1:
            section = word
        elif section in grids:
            grids[section].append(line)
        elif section == "units":
            x, y, kind, owner = line.split()
            m["units"].append((int(x), int(y), "CR_" + kind.upper(), OWNERS[owner]))
        elif section == "objects":
            x, y, name = line.split()
            m["objects"].append((int(x), int(y), "T_" + name.upper()))
        else:
            raise SystemExit(f"{path.name}: unexpected line: {raw!r}")
    for key, legend in (("floor", FLOOR), ("feature", FEATURE), ("decor", DECOR)):
        rows = grids[key]
        if len(rows) != m["h"] or any(len(r) != m["w"] for r in rows):
            raise SystemExit(f"{path.name}: {key} must be {m['w']}x{m['h']}")
        bad = set("".join(rows)) - set(legend)
        if bad:
            raise SystemExit(f"{path.name}: {key} has unknown characters {sorted(bad)}")
        m[key] = "".join(rows)
    rows = grids["roof"]                   # optional v4 layer: '.' = none
    if rows:
        if len(rows) != m["h"] or any(len(r) != m["w"] for r in rows):
            raise SystemExit(f"{path.name}: roof must be {m['w']}x{m['h']}")
        bad = set("".join(rows)) - set(ROOF)
        if bad:
            raise SystemExit(f"{path.name}: roof has unknown characters {sorted(bad)}")
        m["roof"] = "".join(rows)
    return m


def encode(m: dict, enums: dict[str, int]) -> bytes:
    if not (1 <= m["w"] <= 46 and 1 <= m["h"] <= 46):
        raise SystemExit("map size must be 1..46 (MAP_MAX_W/H, D64)")
    out = bytearray(b"LOCM")
    out += struct.pack("<BHBBB", 2, enums["TILE_COUNT"], m["w"], m["h"], m["wrap"])
    for key, legend in (("floor", FLOOR), ("feature", FEATURE), ("decor", DECOR)):
        out += bytes(enums[legend[c]] for c in m[key])
    out.append(len(m["units"]))
    for x, y, kind, owner in m["units"]:
        if kind not in enums:
            raise SystemExit(f"unknown creature {kind} (see data/creatures.csv)")
        out += bytes((x, y, enums[kind], enums[owner]))
    out.append(len(m["objects"]))
    for x, y, tile in m["objects"]:
        if tile not in enums:
            raise SystemExit(f"unknown object tile {tile}")
        out += bytes((x, y)) + struct.pack("<H", enums[tile])
    if m["portal"] is not None or m["roof"] is not None:   # v5
        out[4] = 5
        out += bytes(m["portal"]) if m["portal"] is not None             else bytes((0xFF, 0xFF, 0, 0, 0))    # x = 0xFF: no portal
        if m["roof"] is not None:
            out += bytes(1 if c == "R" else 0 for c in m["roof"])
    return bytes(out)


def main() -> int:
    enums = c_enums(CORE / "world.h", CORE / "map_def.h", CORE / "gen" / "tiles.h",
                    CORE / "gen" / "creatures.h")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    OUT_C.parent.mkdir(parents=True, exist_ok=True)
    c = ["/* GENERATED by tools/gen_maps.py from data/maps/<name>.txt - do not edit. */",
         '#include "maps.h"', ""]
    h = ["/* GENERATED by tools/gen_maps.py - do not edit. */",
         "#ifndef LOC_GEN_MAPS_H", "#define LOC_GEN_MAPS_H", "",
         '#include "../map_def.h"', ""]
    for path in sorted(MAPS.glob("*.txt")):
        data = encode(parse(path), enums)
        (OUT_DIR / f"{path.stem}.map").write_bytes(data)
        name = "MAPBIN_" + re.sub(r"\W", "_", path.stem).upper()
        rows = [", ".join(f"0x{b:02X}" for b in data[i:i + 16]) for i in range(0, len(data), 16)]
        c += [f"const uint8_t {name}[] = {{", *(f"    {r}," for r in rows), "};",
              f"const uint16_t {name}_LEN = {len(data)};", ""]
        h += [f"extern const uint8_t {name}[];", f"extern const uint16_t {name}_LEN;"]
        print(f"[maps] {path.name} -> build/maps/{path.stem}.map ({len(data)} bytes)")
    OUT_C.write_text("\n".join(c), newline="\n")
    OUT_H.write_text("\n".join(h + ["", "#endif", ""]), newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
