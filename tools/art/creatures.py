"""
The 25 creatures of Lords of Chaos as 24x24 tiles (GDD D13), 3/4 front/side
view like the Amiga. Every creature carries the owner key colours
(KEY_LIGHT / KEY_DARK: clothing, saddle cloth or collar) so the tile build
can create one variant per owner (GDD 11.2).

Built from a few templates (humanoid, quadruped, flyer) to keep the set
consistent; the PNGs in assets/tiles are the editable result.
"""

from __future__ import annotations

from make_tiles import (C, KEY_DARK, KEY_LIGHT, ellipse, line, new, outline, poly, px,
                        rect, shadow)

K1, K2 = KEY_LIGHT, KEY_DARK


# ------------------------------------------------------------ templates
def humanoid(skin, *, top=5, width=4, legs=None, hair=None, beard=None, eyes=C["black"],
             body=K1, body_dark=K2, arms=None):
    """Upright figure. top = y of the head top (smaller = taller figure)."""
    im = new()
    cx = 12
    head_h = 5
    by0 = top + head_h + 1                       # body top
    rect(im, cx - width, by0, cx + width - 1, 17, body)
    rect(im, cx, by0, cx + width - 1, 17, body_dark)
    leg = legs or skin
    rect(im, cx - width + 1, 18, cx - 2, 20, leg)  # legs
    rect(im, cx + 1, 18, cx + width - 2, 20, leg)
    arm = arms or skin
    rect(im, cx - width - 2, by0 + 1, cx - width - 1, by0 + 6, arm)
    rect(im, cx + width, by0 + 1, cx + width + 1, by0 + 6, arm)
    rect(im, cx - 3, top, cx + 2, top + head_h, skin)   # head
    px(im, cx - 2, top + 2, eyes)
    px(im, cx + 1, top + 2, eyes)
    if hair:
        rect(im, cx - 3, top - 1, cx + 2, top, hair)
    if beard:
        poly(im, [(cx - 3, top + 3), (cx + 2, top + 3), (cx, top + 8), (cx - 1, top + 8)], beard)
    return im, cx, top, by0


def finish(im, sx0=6, sx1=17, sy=22):
    outline(im)
    shadow(im, sx0, sx1, sy)
    return im


def quadruped(body, belly, *, x0=3, x1=18, y0=8, y1=16, head=None, legs=None, facing=1,
              leg_h=5):
    """Four-legged animal in side view (3/4). facing 1 = head right."""
    im = new()
    leg = legs or belly
    for lx in (x0 + 1, x0 + 4, x1 - 5, x1 - 2):
        rect(im, lx, y1 - 1, lx + 1, y1 + leg_h, leg)
    ellipse(im, (x0, y0, x1, y1), fill=body)
    ellipse(im, (x0 + 2, y0 + (y1 - y0) // 2, x1 - 2, y1), fill=belly)
    return im


def flip(im):
    from PIL import Image
    return im.transpose(Image.FLIP_LEFT_RIGHT)


# ------------------------------------------------------------ humanoids
def dwarf():
    im, cx, top, by0 = humanoid(C["skin"], top=8, width=4, legs=C["dbrown"],
                                hair=C["dgrey"], beard=C["lwood"])
    rect(im, cx - 4, top - 2, cx + 3, top - 1, C["grey"])          # helmet
    px(im, cx - 1, top - 3, C["white"])
    line(im, [(cx + 6, 6), (cx + 6, 17)], C["wood"])                # axe
    rect(im, cx + 6, 6, cx + 8, 9, C["grey"])
    return finish(im)


def goblin():
    im, cx, top, by0 = humanoid(C["green"], top=6, width=3, legs=C["green"])
    poly(im, [(cx - 3, top + 1), (cx - 7, top - 1), (cx - 3, top + 4)], C["green"])   # ears
    poly(im, [(cx + 2, top + 1), (cx + 6, top - 1), (cx + 2, top + 4)], C["green"])
    px(im, cx - 2, top + 2, C["yellow"])
    px(im, cx + 1, top + 2, C["yellow"])
    line(im, [(cx - 7, 18), (cx - 9, 9)], C["dbrown"], 2)          # club
    rect(im, cx - 10, 7, cx - 8, 10, C["wood"])
    return finish(im)


def troll():
    im, cx, top, by0 = humanoid(C["moss"], top=3, width=5, legs=C["moss"], arms=C["moss"])
    rect(im, cx - 5, by0, cx + 4, by0 + 5, C["moss"])               # bare chest
    rect(im, cx - 5, by0 + 6, cx + 4, 17, K1)                       # loincloth
    rect(im, cx, by0 + 6, cx + 4, 17, K2)
    px(im, cx - 1, top + 4, C["white"])                             # tusks
    px(im, cx + 1, top + 4, C["white"])
    rect(im, cx + 6, 3, cx + 8, 12, C["dbrown"])                    # big club
    return finish(im, 5, 18)


def giant():
    im, cx, top, by0 = humanoid(C["skin"], top=0, width=5, legs=C["dbrown"],
                                hair=C["brown"], beard=C["brown"])
    rect(im, cx - 7, by0 + 6, cx - 6, by0 + 9, C["skin"])
    line(im, [(cx + 7, 2), (cx + 7, 18)], C["wood"], 2)
    rect(im, cx + 6, 1, cx + 9, 5, C["wood"])
    return finish(im, 4, 19)


def pixie():
    im = new()
    ellipse(im, (2, 6, 10, 14), fill=C["sky"])                      # wings
    ellipse(im, (14, 6, 22, 14), fill=C["sky"])
    ellipse(im, (4, 8, 9, 12), fill=C["white"])
    ellipse(im, (15, 8, 20, 12), fill=C["white"])
    poly(im, [(9, 19), (12, 11), (15, 19)], K1)                     # dress
    poly(im, [(12, 11), (15, 19), (12, 19)], K2)
    rect(im, 10, 6, 13, 10, C["skin"])
    rect(im, 10, 5, 13, 6, C["yellow"])
    px(im, 11, 8, C["black"])
    px(im, 12, 8, C["black"])
    rect(im, 10, 20, 10, 21, C["skin"])
    rect(im, 13, 20, 13, 21, C["skin"])
    px(im, 17, 4, C["yellow"])                                      # sparkle
    return finish(im, 8, 15)


def zombie():
    im, cx, top, by0 = humanoid((85, 170, 85), top=5, width=4, legs=C["dgrey"],
                                body=K2, body_dark=C["dgrey"], eyes=C["bred"])
    rect(im, cx - 4, by0 + 2, cx - 1, by0 + 4, K1)                  # torn shirt
    rect(im, cx - 9, by0 + 1, cx - 6, by0 + 2, (85, 170, 85))       # arms forward
    rect(im, cx + 4, by0 + 1, cx + 7, by0 + 2, (85, 170, 85))
    px(im, cx - 1, top + 4, C["dred"])
    return finish(im)


def vampire():
    im = new()
    poly(im, [(4, 20), (8, 9), (16, 9), (20, 20)], C["black"])      # cape
    poly(im, [(7, 20), (10, 10), (14, 10), (17, 20)], K1)           # lining
    rect(im, 10, 10, 14, 19, C["dgrey"])
    rect(im, 9, 3, 14, 8, (255, 255, 170))                          # pale face
    rect(im, 9, 2, 14, 3, C["black"])
    poly(im, [(9, 2), (11, 4), (14, 2)], C["black"])
    px(im, 10, 5, C["bred"])
    px(im, 13, 5, C["bred"])
    px(im, 11, 8, C["white"])
    px(im, 12, 8, C["white"])
    poly(im, [(7, 9), (9, 7), (9, 10)], K2)                         # high collar
    poly(im, [(17, 9), (14, 7), (14, 10)], K2)
    rect(im, 10, 20, 11, 21, C["black"])
    rect(im, 13, 20, 14, 21, C["black"])
    return finish(im, 6, 18)


def demon():
    im = new()
    poly(im, [(1, 4), (9, 10), (5, 16)], C["dred"])                 # wings
    poly(im, [(23, 4), (15, 10), (19, 16)], C["dred"])
    im2, cx, top, by0 = humanoid(C["bred"], top=5, width=4, legs=C["red"], arms=C["bred"])
    im.alpha_composite(im2)
    rect(im, cx - 4, by0, cx + 3, by0 + 5, C["bred"])
    rect(im, cx - 4, by0 + 6, cx + 3, 17, K1)
    rect(im, cx, by0 + 6, cx + 3, 17, K2)
    poly(im, [(cx - 3, top), (cx - 5, top - 4), (cx - 2, top)], C["cream"])   # horns
    poly(im, [(cx + 2, top), (cx + 4, top - 4), (cx + 1, top)], C["cream"])
    px(im, cx - 2, top + 2, C["yellow"])
    px(im, cx + 1, top + 2, C["yellow"])
    line(im, [(cx + 5, 18), (cx + 9, 20)], C["red"])                # tail
    return finish(im, 5, 18)


def gorilla():
    im = new()
    ellipse(im, (5, 7, 18, 19), fill=C["dgrey"])                    # body
    ellipse(im, (8, 10, 15, 18), fill=C["grey"])
    rect(im, 2, 9, 5, 19, C["dgrey"])                               # long arms
    rect(im, 18, 9, 21, 19, C["dgrey"])
    rect(im, 7, 19, 10, 21, C["dgrey"])
    rect(im, 13, 19, 16, 21, C["dgrey"])
    ellipse(im, (8, 2, 15, 9), fill=C["dgrey"])                     # head
    ellipse(im, (9, 5, 14, 9), fill=C["grey"])
    px(im, 10, 5, C["black"])
    px(im, 13, 5, C["black"])
    rect(im, 8, 9, 15, 10, K1)                                      # collar
    px(im, 11, 10, K2)
    return finish(im, 4, 19)


def harpy():
    im = new()
    poly(im, [(1, 6), (9, 10), (6, 17)], C["brown"])                # wings
    poly(im, [(23, 6), (15, 10), (18, 17)], C["brown"])
    poly(im, [(1, 6), (4, 8), (3, 11)], K1)                         # coloured wing tips
    poly(im, [(23, 6), (20, 8), (21, 11)], K1)
    ellipse(im, (8, 9, 16, 18), fill=C["wood"])                     # feathered body
    ellipse(im, (10, 12, 15, 18), fill=C["lwood"])
    rect(im, 9, 3, 14, 8, C["skin"])                                # woman's head
    rect(im, 8, 2, 15, 4, C["black"])
    rect(im, 8, 4, 8, 9, C["black"])
    rect(im, 15, 4, 15, 9, C["black"])
    px(im, 10, 5, C["black"])
    px(im, 13, 5, C["black"])
    line(im, [(10, 19), (9, 21)], C["yellow"])                      # talons
    line(im, [(14, 19), (15, 21)], C["yellow"])
    rect(im, 9, 9, 14, 9, K2)
    return finish(im, 7, 16)


# ------------------------------------------------------------ quadrupeds
def unicorn():
    im = quadruped(C["white"], C["grey"], x0=3, x1=17, y0=8, y1=15)
    ellipse(im, (15, 3, 21, 9), fill=C["white"])                    # head
    rect(im, 15, 7, 17, 11, C["white"])                             # neck
    poly(im, [(19, 3), (21, -1), (20, 4)], C["gold"])               # horn
    line(im, [(14, 4), (14, 9)], C["yellow"])                       # mane
    px(im, 19, 5, C["black"])
    line(im, [(3, 10), (1, 15)], C["yellow"], 2)                    # tail
    rect(im, 7, 7, 12, 11, K1)                                      # saddle cloth
    rect(im, 7, 11, 12, 12, K2)
    return finish(im, 3, 19)


def pegasus():
    im = quadruped(C["white"], C["grey"], x0=3, x1=17, y0=9, y1=16)
    poly(im, [(6, 10), (2, 1), (12, 8)], C["sky"])                  # wing
    poly(im, [(6, 10), (4, 3), (10, 8)], C["white"])
    ellipse(im, (15, 4, 21, 10), fill=C["white"])
    rect(im, 15, 8, 17, 12, C["white"])
    line(im, [(14, 5), (14, 10)], C["lblue"])
    px(im, 19, 6, C["black"])
    line(im, [(3, 11), (1, 16)], C["lblue"], 2)
    rect(im, 8, 9, 13, 12, K1)
    rect(im, 8, 12, 13, 13, K2)
    return finish(im, 3, 19)


def centaur():
    im = quadruped(C["brown"], C["wood"], x0=2, x1=16, y0=11, y1=17, leg_h=4)
    line(im, [(2, 13), (0, 18)], C["dbrown"], 2)                    # tail
    rect(im, 13, 5, 17, 13, K1)                                     # human torso (tunic)
    rect(im, 15, 5, 17, 13, K2)
    rect(im, 13, 0, 17, 4, C["skin"])                               # head
    rect(im, 13, 0, 17, 0, C["dbrown"])
    px(im, 14, 2, C["black"])
    px(im, 16, 2, C["black"])
    line(im, [(20, 2), (21, 6), (20, 11)], C["wood"])               # bow
    line(im, [(20, 2), (20, 11)], C["cream"])
    rect(im, 18, 6, 19, 7, C["skin"])
    return finish(im, 2, 18)


def gryphon():
    im = quadruped((170, 85, 85), (255, 170, 85), x0=3, x1=17, y0=9, y1=16)
    poly(im, [(7, 10), (3, 1), (14, 8)], C["brown"])                # wing
    poly(im, [(7, 10), (5, 4), (11, 8)], C["wood"])
    ellipse(im, (14, 3, 21, 10), fill=C["white"])                   # eagle head
    poly(im, [(20, 5), (23, 7), (20, 8)], C["gold"])                # beak
    px(im, 17, 5, C["black"])
    line(im, [(3, 12), (1, 16)], (170, 85, 85), 2)
    rect(im, 14, 10, 17, 11, K1)                                    # collar
    px(im, 15, 11, K2)
    return finish(im, 3, 19)


def elephant():
    im = quadruped(C["grey"], C["dgrey"], x0=1, x1=17, y0=5, y1=16, leg_h=5)
    ellipse(im, (13, 3, 22, 13), fill=C["grey"])                    # head
    ellipse(im, (12, 4, 16, 12), fill=C["dgrey"])                   # ear
    rect(im, 19, 10, 21, 19, C["grey"])                             # trunk
    line(im, [(17, 12), (16, 15)], C["white"])                      # tusk
    px(im, 19, 6, C["black"])
    rect(im, 4, 4, 12, 9, K1)                                       # blanket
    rect(im, 4, 9, 12, 10, K2)
    px(im, 8, 6, C["gold"])
    return finish(im, 1, 21)


def lion():
    im = quadruped((255, 170, 85), C["lwood"], x0=2, x1=16, y0=10, y1=16)
    ellipse(im, (12, 4, 22, 14), fill=C["brown"])                   # mane
    ellipse(im, (14, 6, 20, 12), fill=(255, 170, 85))               # face
    px(im, 16, 8, C["black"])
    px(im, 19, 8, C["black"])
    px(im, 18, 10, C["dbrown"])
    line(im, [(2, 12), (0, 7)], (255, 170, 85))
    px(im, 0, 6, C["brown"])
    rect(im, 12, 12, 14, 14, K1)                                    # collar
    px(im, 13, 14, K2)
    return finish(im, 2, 19)


def bear():
    im = quadruped(C["brown"], C["wood"], x0=2, x1=18, y0=7, y1=16, leg_h=5)
    ellipse(im, (13, 4, 21, 12), fill=C["brown"])                   # head
    ellipse(im, (14, 3, 16, 5), fill=C["brown"])                    # ears
    ellipse(im, (18, 3, 20, 5), fill=C["brown"])
    ellipse(im, (17, 8, 22, 11), fill=C["lwood"])                   # snout
    px(im, 21, 9, C["black"])
    px(im, 16, 6, C["black"])
    rect(im, 13, 11, 15, 13, K1)                                    # collar
    px(im, 14, 13, K2)
    return finish(im, 2, 20)


def crocodile():
    im = new()
    ellipse(im, (1, 12, 17, 18), fill=C["green"])                   # body
    poly(im, [(14, 13), (23, 14), (23, 16), (14, 17)], C["green"])  # snout
    line(im, [(15, 15), (23, 15)], C["dgreen"])
    for x in (16, 18, 20, 22):                                      # teeth
        px(im, x, 14, C["white"])
    px(im, 15, 13, C["yellow"])
    poly(im, [(1, 15), (0, 14), (0, 17)], C["green"])
    for x in (3, 7, 11):                                            # back scutes
        px(im, x, 12, C["dgreen"])
    for lx in (3, 12):
        rect(im, lx, 18, lx + 2, 20, C["green"])
    rect(im, 7, 12, 10, 13, K1)                                     # band
    rect(im, 7, 14, 10, 14, K2)
    return finish(im, 2, 18)


def dragon(body, belly, wing):
    im = quadruped(body, belly, x0=2, x1=16, y0=10, y1=17, leg_h=4)
    poly(im, [(7, 11), (2, 1), (10, 4), (13, 9)], wing)             # wing
    poly(im, [(7, 11), (4, 4), (9, 6)], body)
    rect(im, 14, 6, 16, 12, body)                                   # neck
    ellipse(im, (14, 2, 22, 8), fill=body)                          # head
    poly(im, [(20, 4), (23, 6), (20, 7)], body)
    poly(im, [(15, 2), (14, -1), (17, 2)], C["cream"])              # horn
    px(im, 18, 4, C["yellow"])
    line(im, [(2, 14), (0, 10)], body, 2)                           # tail
    px(im, 0, 9, wing)
    rect(im, 13, 10, 16, 11, K1)                                    # collar
    px(im, 14, 11, K2)
    return finish(im, 2, 19)


# ------------------------------------------------------------ flyers / special
def giant_bat():
    im = new()
    poly(im, [(12, 9), (1, 3), (3, 9), (1, 13), (8, 12)], C["dbrown"])    # wings
    poly(im, [(12, 9), (23, 3), (21, 9), (23, 13), (16, 12)], C["dbrown"])
    ellipse(im, (9, 6, 15, 15), fill=C["brown"])
    poly(im, [(9, 7), (9, 3), (11, 6)], C["brown"])                       # ears
    poly(im, [(15, 7), (15, 3), (13, 6)], C["brown"])
    px(im, 10, 9, C["bred"])
    px(im, 13, 9, C["bred"])
    rect(im, 10, 13, 13, 13, K1)                                          # band
    px(im, 11, 14, K2)
    outline(im)
    shadow(im, 8, 15, 21)       # flying: shadow lower on the ground
    return im


def giant_spider():
    im = new()
    for i, (x0, y0) in enumerate(((3, 9), (2, 13), (3, 17), (5, 20))):   # legs
        line(im, [(9, 13), (x0, y0 - 3), (x0 - 1, y0)], C["dgrey"])
        line(im, [(14, 13), (23 - x0, y0 - 3), (24 - x0, y0)], C["dgrey"])
    ellipse(im, (6, 9, 17, 19), fill=C["black"])                          # abdomen
    ellipse(im, (8, 5, 15, 11), fill=C["dgrey"])                          # head
    for x in (9, 11, 12, 14):
        px(im, x, 7, C["bred"])
    poly(im, [(11, 12), (13, 12), (12, 15)], K1)                          # hourglass
    poly(im, [(11, 17), (13, 17), (12, 14)], K2)
    return finish(im, 5, 18)


def ghost():
    im = new()
    poly(im, [(5, 21), (5, 9), (8, 3), (15, 3), (18, 9), (18, 21),
              (15, 18), (12, 21), (9, 18)], C["white"])
    poly(im, [(12, 3), (15, 3), (18, 9), (18, 21), (15, 18), (13, 20)], C["sky"])
    ellipse(im, (8, 8, 10, 11), fill=C["black"])
    ellipse(im, (13, 8, 15, 11), fill=C["black"])
    ellipse(im, (10, 13, 13, 16), fill=C["dgrey"])
    rect(im, 6, 18, 17, 18, K1)                                           # coloured hem
    outline(im)
    return im


def spectre():
    im = new()
    poly(im, [(4, 21), (7, 7), (12, 1), (17, 7), (20, 21)], C["dgrey"])   # hooded robe
    poly(im, [(12, 1), (17, 7), (20, 21), (13, 21)], C["black"])
    ellipse(im, (8, 5, 16, 12), fill=C["black"])                          # hood opening
    px(im, 10, 8, K1)                                                     # glowing eyes
    px(im, 14, 8, K1)
    line(im, [(6, 15), (2, 12)], C["dgrey"], 2)                           # reaching arm
    rect(im, 1, 11, 2, 12, (255, 255, 170))
    rect(im, 8, 14, 16, 15, K2)                                           # sash
    outline(im)
    return im


def all_creatures() -> dict:
    return {
        "dwarf": dwarf(), "goblin": goblin(), "troll": troll(), "giant": giant(),
        "pixie": pixie(), "zombie": zombie(), "vampire": vampire(), "demon": demon(),
        "gorilla": gorilla(), "harpy": harpy(), "unicorn": unicorn(), "pegasus": pegasus(),
        "centaur": centaur(), "gryphon": gryphon(), "elephant": elephant(), "lion": lion(),
        "bear": bear(), "crocodile": crocodile(),
        "gold_dragon": dragon(C["gold"], C["yellow"], C["orange"]),
        "green_dragon": dragon(C["green"], C["lgreen"], C["dgreen"]),
        "red_dragon": dragon(C["red"], C["bred"], C["dred"]),
        "giant_bat": giant_bat(), "giant_spider": giant_spider(), "ghost": ghost(),
        "spectre": spectre(),
    }
