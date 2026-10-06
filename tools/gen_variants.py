#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Terrain variants of scenario 1, "The Many Coloured Land" (GDD D57).

    uv run tools/gen_variants.py            # build/maps/mcl_v00.map .. mcl_v15.map
    uv run tools/gen_variants.py --ascii 3  # print the floor of variant 3

The hand-made map data/maps/many_coloured_land.txt stays the source of truth
for the two houses (rooms, windows, garden with gate), the units, the objects
and the portal; they are copied into every variant unchanged, so the
wizards always start where the selftests expect them. Everything around
them is rolled per variant:

  - the river (course, width, three bridges),
  - organic biomes (Voronoi cells with warped borders on the torus), where
    the enchanted wood and the dead wood swap sides at random,
  - winding paths that run through the woods and over the bridges,
  - trees, rocks, mushrooms.

Every variant is validated (paths connected, no dead ends except at the
portal, both houses and the portal reachable, every biome present); a
variant that fails is rolled again with the next attempt number. The game
picks one at random per new game (src/agon/main.c, MCL_VARIANTS).
The generator uses floating point - it runs on the PC, never on the Agon.
"""

from __future__ import annotations

import argparse
import collections
import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_maps  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "data" / "maps" / "many_coloured_land.txt"
VARIANTS = 16          # keep in step with MCL_VARIANTS in src/agon/main.c
W = H = 36

HOUSE1 = (3, 2, 14, 16)        # house with east wing and the fenced garden
HOUSE2 = (28, 22, 33, 30)
CLEAR_BOXES = [(1, 0, 16, 18), (25, 20, 35, 33)]    # no woods or swamp here
NEAR_BOXES = [(3, 2, 14, 17), (27, 21, 34, 31)]     # no trees here
PORTAL = (26, 3)
GATE_PATH = (11, 17)           # south of the garden gate
STUBS = [(27, 25), (26, 25)]   # in front of house 2's west door
PROTECT = {(26, 4), *STUBS}    # path ends that are meant to end

# char, x, y, reach; "W" and "E" are the two slots of the enchanted/dead wood
SEED_TEMPLATE = [
    ("f", 29, 6, 9), ("f", 8, 33, 7), ("f", 23, 33, 4),
    ("W", 6, 25, 7), ("E", 31, 14, 6),
    ("u", 24, 16, 6),
    ('"', 4, 18, 6), ('"', 13, 28, 4), ('"', 20, 2, 4),
    ("r", 33, 34, 4),
]
LAT = 6


class Reject(Exception):
    """This roll failed validation."""


def tdist(x: float, y: float, sx: float, sy: float) -> float:
    dx, dy = abs(x - sx), abs(y - sy)
    return math.hypot(min(dx, W - dx), min(dy, H - dy))


def smooth(t: float) -> float:
    return t * t * (3 - 2 * t)


class Noise:
    def __init__(self, rng: random.Random):
        self.lat = {(i, j): rng.random() for i in range(W // LAT) for j in range(H // LAT)}

    def __call__(self, x: float, y: float) -> float:
        gx, gy = x / LAT, y / LAT
        i0, j0 = math.floor(gx), math.floor(gy)
        fx, fy = smooth(gx - i0), smooth(gy - j0)

        def v(i: int, j: int) -> float:
            return self.lat[(i % (W // LAT), j % (H // LAT))]
        a = v(i0, j0) * (1 - fx) + v(i0 + 1, j0) * fx
        b = v(i0, j0 + 1) * (1 - fx) + v(i0 + 1, j0 + 1) * fx
        return a * (1 - fy) + b * fy


def load_base() -> dict:
    m = gen_maps.parse(BASE)
    for key in ("floor", "feature", "decor", "roof"):
        m[key + "_g"] = [list(m[key][y * W:(y + 1) * W]) for y in range(H)]
    return m


def build(base: dict, rng: random.Random) -> dict:
    floor = [["g"] * W for _ in range(H)]
    feat = [["."] * W for _ in range(H)]
    roof = [["."] * W for _ in range(H)]
    decor = [["."] * W for _ in range(H)]
    noise = Noise(rng)

    # ---- biomes: Voronoi cells with warped borders, wood slots swap sides
    swap = rng.random() < 0.5
    seeds = []
    for c, sx, sy, r in SEED_TEMPLATE:
        if c == "W":
            c = "n" if swap else "m"
        elif c == "E":
            c = "m" if swap else "n"
        seeds.append((c, sx + rng.randint(-2, 2), sy + rng.randint(-2, 2),
                      max(3, r + rng.randint(-1, 1))))
    score_of = {}
    for y in range(H):
        for x in range(W):
            best, bc = 9.0, "g"
            for c, sx, sy, r in seeds:
                s = (tdist(x, y, sx, sy) + (noise(x + 3.7, y + 1.3) - 0.5) * 9) / r
                if s < best:
                    best, bc = s, c
            score_of[(x, y)] = (best, bc)
            floor[y][x] = bc if best < 1.0 else "g"
            if best >= 1.0 and bc in "fmn" and best < 1.25 and rng.random() < 0.45:
                floor[y][x] = '"'          # tall-grass fringe around the woods

    # ---- the river: a sine course, 2 wide, one 3-wide pool, three bridges
    c0, a1, a2 = rng.uniform(18.4, 19.8), rng.uniform(1.2, 2.2), rng.uniform(0.3, 0.9)
    p1, p2 = rng.uniform(0, 2 * math.pi), rng.uniform(0, 2 * math.pi)
    pool = rng.randrange(H)
    river: dict[int, list[int]] = {}
    for y in range(H):
        c = c0 + a1 * math.sin(2 * math.pi * y / H + p1) + a2 * math.sin(4 * math.pi * y / H + p2)
        width = 3 if (y - pool) % H < 3 else 2
        x0 = int(round(c - width / 2 + 0.5))
        river[y] = [(x0 + k) % W for k in range(width)]
    for y in range(H):
        for x in river[y]:
            floor[y][x] = "~"
    for y in range(0, 19):                 # keep the house 1 side free
        if min(river[y]) < 17:
            raise Reject("river too close to house 1")
    for y in range(20, 34):                # and the house 2 side
        if max(river[y]) > 22:
            raise Reject("river too close to house 2")

    def bridge_row(lo: int, hi: int) -> int:
        ok = [y for y in range(lo, hi + 1)
              if all(floor[y - 1][x] == "~" or floor[(y + 1) % H][x] == "~" for x in river[y])]
        if not ok:
            raise Reject("no bridge row")
        return rng.choice(ok)
    b1, b2, b3 = bridge_row(8, 11), bridge_row(19, 23), bridge_row(29, 33)
    for y in (b1, b2, b3):
        for x in river[y]:
            floor[y][x] = "b"

    # ---- clear the houses' surroundings, then copy the hand-made houses
    for (x0, y0, x1, y1) in CLEAR_BOXES:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if floor[y][x] in 'fmnu"r':
                    floor[y][x] = "g"
    for (x0, y0, x1, y1) in (HOUSE1, HOUSE2):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                floor[y][x] = base["floor_g"][y][x]
                feat[y][x] = base["feature_g"][y][x]
                roof[y][x] = base["roof_g"][y][x]
                decor[y][x] = base["decor_g"][y][x]
    for y in range(H):                      # the portal clearing
        for x in range(W):
            if tdist(x, y, *PORTAL) <= 3 and feat[y][x] == "." and floor[y][x] not in "~bsw":
                floor[y][x] = "g"

    # ---- paths
    def walk(a: tuple[int, int], b: tuple[int, int], wobble: float) -> list[tuple[int, int]]:
        x, y = a
        cells = [(x, y)]
        for _ in range(400):
            if (x, y) == b:
                break
            dx, dy = b[0] - x, b[1] - y
            dx = dx - W if dx > W // 2 else dx + W if dx < -W // 2 else dx
            dy = dy - H if dy > H // 2 else dy + H if dy < -H // 2 else dy
            if rng.random() < wobble:           # drift sideways now and then
                if abs(dx) >= abs(dy):
                    y += rng.choice((-1, 1)) if dy == 0 else (1 if dy > 0 else -1)
                else:
                    x += rng.choice((-1, 1)) if dx == 0 else (1 if dx > 0 else -1)
            elif abs(dx) > abs(dy) or (abs(dx) == abs(dy) and rng.random() < .5):
                x += 1 if dx > 0 else -1
            else:
                y += 1 if dy > 0 else -1
            x %= W
            y %= H
            cells.append((x, y))
        return cells

    def in_house(x: int, y: int) -> bool:        # the houses and the garden stay as drawn
        return any(x0 <= x <= x1 and y0 <= y <= y1 for (x0, y0, x1, y1) in (HOUSE1, HOUSE2))

    def road(points: list[tuple[int, int]], wobble: float = 0.35) -> None:
        for a, b in zip(points, points[1:]):
            for (x, y) in walk(a, b, wobble):
                if floor[y][x] in "~bsw" or feat[y][x] not in ".t" or in_house(x, y):
                    continue
                floor[y][x] = "p"
                feat[y][x] = "."

    def cross(y: int) -> None:                 # path on both banks of a bridge
        for x in range(min(river[y]) - 3, max(river[y]) + 4):
            if floor[y][x] not in "~b" and feat[y][x] == ".":
                floor[y][x] = "p"

    def wb(y: int) -> int: return min(river[y]) - 2
    def eb(y: int) -> int: return max(river[y]) + 2

    slot_w = next(s for s in seeds if s[0] in "mn" and s[1] < 18)
    slot_e = next(s for s in seeds if s[0] in "mn" and s[1] >= 18)
    sw_forest = seeds[1]
    g = GATE_PATH
    road([g, (15, 17), (15, 13), (wb(b1), b1)], .25)                 # north branch
    cross(b1)
    road([(eb(b1), b1), (24, 7), (25, 5), (26, 4)], .45)             # to the portal
    road([g, (12, 19), (wb(b2), b2)], .4)                            # centre crossing
    cross(b2)
    road([(eb(b2), b2), (24, 23), (26, 25)], .4)                     # to house 2
    road([(26, 25), (24, 28), (eb(b3), b3)], .4)                     # south crossing
    cross(b3)
    road([(wb(b3), b3), sw_forest[1:3], slot_w[1:3], (7, 21), (8, 18), g], .4)   # west woods
    road([(24, 23), (26, 19), slot_e[1:3], (35, 17), (2, 18), (7, 18)], .4)      # east woods, over the edge
    for (x, y) in STUBS:
        if feat[y][x] == ".":
            floor[y][x] = "p"

    def pathlike(x: int, y: int) -> bool:
        return floor[y % H][x % W] in "pb" or feat[y % H][x % W] in "dD"

    def path_ends() -> list[tuple[int, int]]:
        out = []
        for y in range(H):
            for x in range(W):
                if floor[y][x] == "p" and feat[y][x] == ".":
                    n = sum(pathlike(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                    if n <= 1:
                        out.append((x, y))
        return out
    for _ in range(4):                         # shave off stubs left by the wobble
        for (x, y) in path_ends():
            if (x, y) not in PROTECT:
                floor[y][x] = "g"
    left = [e for e in path_ends() if e not in PROTECT]
    if left:
        raise Reject(f"dead ends {left}")
    net = {(x, y) for y in range(H) for x in range(W) if floor[y][x] in "pb" and feat[y][x] == "."}
    net |= {(11, 10), (11, 16)}
    seen_p = {(11, 11)}
    dq = collections.deque(seen_p)
    while dq:
        x, y = dq.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            q = ((x + dx) % W, (y + dy) % H)
            if q in net and q not in seen_p:
                seen_p.add(q)
                dq.append(q)
    if net - seen_p:
        raise Reject("paths are cut apart")

    # ---- trees, rocks, mushrooms
    def near_house(x: int, y: int) -> bool:
        return any(x0 - 1 <= x <= x1 + 1 and y0 - 1 <= y <= y1 + 1 for (x0, y0, x1, y1) in NEAR_BOXES)

    def near_water(x: int, y: int) -> bool:
        return any(floor[(y + dy) % H][(x + dx) % W] == "~" for dx in (-1, 0, 1) for dy in (-1, 0, 1))
    for y in range(H):
        for x in range(W):
            if feat[y][x] != "." or near_house(x, y) or tdist(x, y, *PORTAL) <= 2.2:
                continue
            f = floor[y][x]
            s, bc = score_of[(x, y)]
            if f in 'g"':
                p = 0.012
                if bc == "f" and s < 1.7:
                    p = 0.30
                elif bc in "mn" and s < 1.5:
                    p = 0.08
                if near_water(x, y):
                    p = max(p, 0.08)
                if rng.random() < p:
                    feat[y][x] = "t"
            if f == "r" and rng.random() < 0.08:
                feat[y][x] = "R"
            if f in 'gn"' and bc == "n" and s < 1.4 and rng.random() < 0.05:
                feat[y][x] = "R"
            if f == "g" and rng.random() < 0.004:
                feat[y][x] = "R"
            if f == "u" and decor[y][x] == "." and rng.random() < 0.13:
                decor[y][x] = "o"
    for k in range(0, 8, 2):                   # standing stones round the portal
        a = k * math.pi / 4
        x, y = round(PORTAL[0] + 3 * math.cos(a)), round(PORTAL[1] + 3 * math.sin(a))
        if feat[y][x] == "." and floor[y][x] not in "~b":
            feat[y][x] = "R"

    # ---- validation
    def passable(x: int, y: int) -> bool:
        return floor[y][x] != "~" and feat[y][x] not in "#WFtRBSTMXL"
    start = next((x, y) for x, y, kind, owner in base["units"]
                 if kind == "CR_WIZARD" and owner == "OWN_P1")
    goals = {next((x, y) for x, y, kind, owner in base["units"]
                  if kind == "CR_WIZARD" and owner == "OWN_P2"), PORTAL}
    seen = {start}
    dq = collections.deque([start])
    while dq:
        x, y = dq.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            q = ((x + dx) % W, (y + dy) % H)
            if q not in seen and passable(*q):
                seen.add(q)
                dq.append(q)
    if not goals <= seen:
        raise Reject("a wizard or the portal cannot be reached")
    area = collections.Counter(c for row in floor for c in row)
    for ch, need in (("m", 25), ("n", 20), ("u", 35), ("f", 150), ('"', 40), ("p", 80)):
        if area[ch] < need:
            raise Reject(f"too little {ch!r} ({area[ch]})")
    m = {k: base[k] for k in ("w", "h", "wrap", "units", "objects", "portal")}
    m["floor"] = "".join("".join(r) for r in floor)
    m["feature"] = "".join("".join(r) for r in feat)
    m["decor"] = "".join("".join(r) for r in decor)
    m["roof"] = "".join("".join(r) for r in roof)
    return m


def variant(base: dict, n: int) -> tuple[dict, int]:
    for attempt in range(300):
        try:
            return build(base, random.Random(n * 1000 + attempt)), attempt
        except Reject:
            continue
    raise SystemExit(f"variant {n}: no valid roll in 300 attempts")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--ascii", type=int, metavar="N", help="print the floor of variant N and stop")
    a = ap.parse_args()
    base = load_base()
    if a.ascii is not None:
        m, _ = variant(base, a.ascii)
        for y in range(H):
            print(m["floor"][y * W:(y + 1) * W])
        return 0
    enums = gen_maps.c_enums(gen_maps.CORE / "world.h", gen_maps.CORE / "map_def.h",
                             gen_maps.CORE / "gen" / "tiles.h", gen_maps.CORE / "gen" / "creatures.h")
    gen_maps.OUT_DIR.mkdir(parents=True, exist_ok=True)
    tries = []
    for n in range(VARIANTS):
        m, attempt = variant(base, n)
        data = gen_maps.encode(m, enums)
        (gen_maps.OUT_DIR / f"mcl_v{n:02d}.map").write_bytes(data)
        tries.append(attempt + 1)
    print(f"[variants] {VARIANTS} x build/maps/mcl_vNN.map ({len(data)} bytes each; "
          f"rolls needed: {tries})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
