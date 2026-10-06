"""
Night recolouring of the outdoor tiles (GDD D54).

The Amiga original plays at night: the ground is black and the grass is a
sprinkle of bright green tufts. MODE 8 has four levels per channel and no
palette tricks (QUIRKS S4), so the look comes from the tile colours
themselves: dark fills, saturated accents. Lights (candles, windows,
mushrooms, portal, spell effects), creatures, objects, walls and the house
interior are not touched.

Each rule is (stem prefix, {colour: colour}); the mapping is simultaneous
and applies to every tile whose name starts with the prefix. The PNGs in
assets/tiles stay the daylight source of truth.
"""

from __future__ import annotations

from PIL import Image

BLACK = (0, 0, 0)
RULES: list[tuple[str, dict[tuple[int, int, int], tuple[int, int, int]]]] = [
    # grass: black ground, dim tufts
    # tufts are dim: the bright greens would make the ground restless
    ("floor_grass", {(0, 85, 0): BLACK, (0, 170, 0): (0, 85, 0), (85, 170, 0): (0, 85, 0)}),
    ("edge_path_", {(0, 85, 0): BLACK, (0, 170, 0): (0, 85, 0), (85, 255, 85): (0, 85, 0)}),
    # trees and forest floor: one step darker, highlights become mid green
    ("tree", {(0, 85, 0): BLACK, (0, 170, 0): (0, 85, 0), (85, 255, 85): (0, 170, 0)}),
    ("floor_forest", {(0, 85, 0): BLACK, (0, 170, 0): (0, 85, 0), (85, 255, 85): (0, 170, 0)}),
    # tall grass: black ground, stalks stay
    ("floor_tallgrass", {(85, 85, 0): BLACK, (170, 170, 85): (85, 85, 0)}),
    ("edge_tall_", {(85, 255, 85): (0, 170, 0), (0, 170, 0): (0, 85, 0)}),
    # swamp: stagnant dark blue, teal and green specks stay
    ("floor_swamp", {(85, 85, 0): (0, 0, 85)}),
    # path: grey earth
    ("floor_path", {(170, 170, 85): (85, 85, 85), (85, 85, 85): BLACK, (85, 85, 0): BLACK}),
    # shore: dull sand lip
    ("edge_shore_", {(170, 170, 85): (85, 85, 0)}),
    # magic wood: the cyan glows on black
    ("floor_magicwood", {(0, 85, 0): BLACK}),
    # rubble
    ("floor_rubble", {(85, 85, 0): BLACK}),
]


def nightify(stem: str, im: Image.Image) -> Image.Image:
    for prefix, mapping in RULES:
        if stem.startswith(prefix):
            out = im.copy()
            for y in range(out.height):
                for x in range(out.width):
                    r, g, b, a = out.getpixel((x, y))
                    if a and (r, g, b) in mapping:
                        out.putpixel((x, y), (*mapping[(r, g, b)], 255))
            return out
    return im
