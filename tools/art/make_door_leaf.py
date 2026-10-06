#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Door leaves (D61): an open door swings into the room and stands on the
field beside the doorway. This writes the four leaf tiles and redraws the
two open door frames (the frame stays, the leaf stubs go, a shadow under
the lintel marks the opening).

    uv run tools/art/make_door_leaf.py

Leaf tiles, named after the field edge the leaf stands on:
  door_leaf_e / door_leaf_w  - door in an east-west wall: the leaf runs
                               north-south, seen at a slant from the front
  door_leaf_{n,s}{w,e}       - door in a north-south wall: the leaf runs
                               east-west, its face turned to the viewer,
                               hinged at the wall on the west/east side
Colours are the ones of door_h_closed (agon64 palette).
"""

from pathlib import Path

from PIL import Image

TILES = Path(__file__).resolve().parents[2] / "assets" / "tiles"

K = (0, 0, 0, 255)            # outline, shadow
FRAME = (85, 0, 0, 255)       # dark red frame
LIGHT = (255, 170, 85, 255)   # wood highlight
WOOD = (170, 85, 0, 255)      # wood
PLANK = (85, 85, 0, 255)      # plank seams
KNOB = (255, 170, 0, 255)     # brass knob
CLEAR = (0, 0, 0, 0)


def blank() -> Image.Image:
    return Image.new("RGBA", (24, 24), CLEAR)


def rect(im, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            im.putpixel((x, y), c)


def slant_leaf(mirror: bool) -> Image.Image:
    """Leaf standing on the east edge (mirror: west edge), turned away
    from the doorway, seen at a slant: a narrow panel whose top edge
    climbs towards the hinge, with a floor shadow beside it."""
    im = blank()
    x0, x1 = 15, 22                       # panel columns
    for x in range(x0, x1 + 1):
        top = 1 + (x1 - x) // 2           # slant: higher at the hinge side
        bottom = 22 - (x1 - x) // 3
        for y in range(top, bottom + 1):
            edge = x in (x0, x1) or y in (top, bottom)
            if edge:
                c = FRAME
            elif x == x0 + 1:
                c = LIGHT
            elif (y - top) % 6 == 5:
                c = PLANK
            else:
                c = WOOD
            im.putpixel((x, y), c)
    im.putpixel((x0 + 2, 13), KNOB)
    for y in range(18, 23):               # shadow on the floor, room side
        im.putpixel((x0 - 1, y), K)
    im.putpixel((x1 + 1, 2), FRAME)       # hinge against the wall
    im.putpixel((x1 + 1, 20), FRAME)
    return im.transpose(Image.FLIP_LEFT_RIGHT) if mirror else im


def face_leaf(top: int, wall_east: bool) -> Image.Image:
    """Leaf seen face-on, standing on the south edge (top = 7) or the
    north edge (top = 1) of its field, hinged at the wall to the west
    (or east): it reaches from the wall 17 px into the field."""
    im = blank()
    x0, x1 = (7, 23) if wall_east else (0, 16)
    y0, y1 = top, top + 15
    rect(im, x0, y0, x1, y1, FRAME)
    rect(im, x0 + 1, y0 + 1, x1 - 1, y1 - 1, WOOD)
    for y in range(y0 + 1, y1):
        im.putpixel((x0 + 1, y), LIGHT)   # lit edge
        for x in (x0 + 5, x0 + 9):        # plank seams
            im.putpixel((x, y), PLANK)
    rect(im, x0 + 1, y0 + 1, x1 - 1, y0 + 1, LIGHT)
    im.putpixel((x0 + 3 if wall_east else x1 - 3, y0 + 8), KNOB)   # far from the hinge
    rect(im, x0 + 1, y1 + 1, x1, min(y1 + 1, 23), K)   # shadow at its foot
    return im


def open_frames() -> None:
    """Strip the old leaf stubs from the open frames; the horizontal frame
    turns dark inside (the floor showed through and read as a door)."""
    h = Image.open(TILES / "door_h_open.png").convert("RGBA")
    for y in range(10, 20):
        for x in range(6, 18):
            h.putpixel((x, y), CLEAR)
    for y in range(9, 16):                # dark passage, the sill shows below
        for x in range(6, 18):
            h.putpixel((x, y), K)
    h.save(TILES / "door_h_open.png")

    v = Image.open(TILES / "door_v_open.png").convert("RGBA")
    for y in range(4, 14):
        for x in range(17, 21):
            v.putpixel((x, y), CLEAR)
    for x in range(8, 17):
        v.putpixel((x, 4), K)
    v.putpixel((16, 4), K)
    for y in range(5, 19):
        v.putpixel((16, y), K)
    v.save(TILES / "door_v_open.png")


def main() -> None:
    slant_leaf(False).save(TILES / "door_leaf_e.png")
    slant_leaf(True).save(TILES / "door_leaf_w.png")
    for edge, top in (("s", 7), ("n", 1)):
        face_leaf(top, False).save(TILES / f"door_leaf_{edge}w.png")
        face_leaf(top, True).save(TILES / f"door_leaf_{edge}e.png")
    open_frames()


if __name__ == "__main__":
    main()
