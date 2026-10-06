#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Door leaves (D61, D65): an open door swings into the room.

Door in an east-west wall (the wall seen from the front, door_h_*): the leaf
stands IN the open frame, hinged at one jamb, so it is attached to the wall
by construction. Four frame tiles, named after the hinge side and the swing:
  door_h_open_e / door_h_open_w - leaf swung towards the viewer (room south
                                  of the wall), hinge east / west
  door_h_far_e / door_h_far_w   - leaf swung away (room north of the wall):
                                  seen through the opening, smaller, higher
The field beside the doorway only reserves the room the leaf needs (D61);
nothing is drawn there.

Door in a north-south wall (door_v_*): the leaf runs east-west and cannot
fit into the narrow frame, so it stays on the diagonal field:
  door_leaf_{n,s}{w,e}       - its face turned to the viewer,
                               hinged at the wall on the west/east side

    uv run tools/art/make_door_leaf.py

Colours are the ones of door_h_closed (agon64 palette).
"""

from pathlib import Path

from PIL import Image, ImageDraw

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


def frame_leaf(away: bool, hinge_east: bool) -> Image.Image:
    """The leaf alone, on a clear tile, to lie over door_h_open. East hinge
    first, mirrored for the west one: only the leaf is mirrored, the brick
    pattern of the wall stays."""
    im = blank()
    d = ImageDraw.Draw(im)
    if away:
        # seen through the opening: free edge farther away = smaller, higher
        poly = [(17, 9), (11, 10), (11, 17), (17, 19)]
    else:
        # swung out of the frame: free edge nearer = taller, lower
        for x in range(10, 17):                 # its shadow on the floor
            d.point((x, 23), K)
        poly = [(17, 8), (9, 10), (9, 22), (17, 20)]
    top, bottom = poly[0][1], poly[3][1]
    d.polygon(poly, fill=WOOD)
    d.line(poly + [poly[0]], fill=FRAME)
    d.line([(16, top + 1), (16, bottom - 1)], fill=LIGHT)     # lit hinge edge
    fx = poly[1][0]
    d.line([(fx + 1, poly[1][1] + 1), (fx + 1, poly[2][1] - 1)], fill=LIGHT)
    d.line([(fx + 4, poly[1][1] + 1), (fx + 4, poly[2][1] - 1)], fill=PLANK)
    im.putpixel((fx + 2, (poly[1][1] + poly[2][1]) // 2 + 1), KNOB)
    for y in (top + 2, bottom - 2):             # iron straps over the jamb
        d.line([(15, y), (18, y)], fill=K)
        im.putpixel((19, y), KNOB)
    return im if hinge_east else im.transpose(Image.FLIP_LEFT_RIGHT)


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
    for edge, top in (("s", 7), ("n", 1)):
        face_leaf(top, False).save(TILES / f"door_leaf_{edge}w.png")
        face_leaf(top, True).save(TILES / f"door_leaf_{edge}e.png")
    open_frames()
    base = Image.open(TILES / "door_h_open.png").convert("RGBA")
    for stem, away in (("door_h_open", False), ("door_h_far", True)):
        for side, east in (("e", True), ("w", False)):
            t = base.copy()
            t.alpha_composite(frame_leaf(away, east))
            t.save(TILES / f"{stem}_{side}.png")


if __name__ == "__main__":
    main()
