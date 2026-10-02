"""
Agon 64-colour palette (RGB 2-2-2) and named colours used by the art tools.

Each channel has 4 levels: 0, 85, 170, 255. Tiles may only use these 64
colours plus full transparency (alpha 0).
"""

from __future__ import annotations

from pathlib import Path

LEVELS = (0, 85, 170, 255)
PALETTE = [(r, g, b) for r in LEVELS for g in LEVELS for b in LEVELS]
TRANSPARENT = (0, 0, 0, 0)

# Named colours (all members of PALETTE).
C = {
    "black": (0, 0, 0),
    "white": (255, 255, 255),
    "grey": (170, 170, 170),
    "dgrey": (85, 85, 85),
    "navy": (0, 0, 85),
    "blue": (0, 0, 170),
    "mblue": (0, 85, 170),
    "lblue": (85, 170, 255),
    "sky": (170, 255, 255),
    "cyan": (0, 255, 255),
    "dred": (85, 0, 0),
    "red": (170, 0, 0),
    "bred": (255, 0, 0),
    "pink": (255, 85, 170),
    "rose": (255, 170, 170),
    "brown": (170, 85, 0),
    "dbrown": (85, 0, 0),
    "tan": (170, 85, 85),
    "wood": (170, 85, 0),
    "lwood": (255, 170, 85),
    "olive": (85, 85, 0),
    "orange": (255, 170, 0),
    "yellow": (255, 255, 0),
    "cream": (255, 255, 170),
    "skin": (255, 170, 85),
    "dgreen": (0, 85, 0),
    "green": (0, 170, 0),
    "lgreen": (85, 255, 85),
    "moss": (85, 170, 0),
    "purple": (85, 0, 85),
    "violet": (170, 0, 170),
    "lviolet": (170, 85, 255),
    "magenta": (255, 0, 255),
    "gold": (255, 170, 0),
}

# Owner key colours: tiles draw robes/markings with KEY_LIGHT/KEY_DARK; the
# build replaces them per owner (GDD 11.2).
KEY_LIGHT = C["magenta"]
KEY_DARK = C["violet"]
OWNERS = {
    "p1": ((170, 85, 255), (85, 0, 170)),   # violet wizard
    "p2": ((255, 85, 85), (170, 0, 0)),     # red
    "p3": ((85, 255, 85), (0, 170, 0)),     # green
    "p4": ((255, 255, 85), (170, 170, 0)),  # yellow
    "neutral": ((170, 170, 170), (85, 85, 85)),
}

for _name, _rgb in C.items():
    assert _rgb in PALETTE, _name


def write_gpl(path: Path) -> None:
    """GIMP/Aseprite/LibreSprite palette file."""
    lines = ["GIMP Palette", "Name: Agon 64 (RGB222)", "Columns: 16", "#"]
    for r, g, b in PALETTE:
        lines.append(f"{r:3d} {g:3d} {b:3d}\t#{r:02X}{g:02X}{b:02X}")
    path.write_text("\n".join(lines) + "\n", newline="\n")
