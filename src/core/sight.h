/*
 * Line of sight and the hidden map (K11, D67 0h): every player keeps a
 * bitmap of explored fields and one of the fields currently seen by any of
 * his units. The reach is an octagon, 2 dx < R, 2 dy < R and D < R with
 * D = 2 max + min and R = 19 on the ground (9 fields straight, 6 diagonal)
 * and R = 23 in the air (11 and 7). Ground observers cast shadows; fliers
 * look over walls. Targets within D < 4 (the eight neighbours) are always
 * seen. Who may see whom across the two heights follows K11.2: fliers are
 * always seen by ground observers outside a roof, fliers see ground targets
 * only beyond a canopy-free, roof-free field (or next to them).
 */
#ifndef LOC_SIGHT_H
#define LOC_SIGHT_H

#include <stdbool.h>
#include <stdint.h>

#include "world.h"

#define SIGHT_GROUND 9
#define SIGHT_AIR 11
#define SIGHT_R_GROUND 19   /* K11.1: octagon limit on the ground ... */
#define SIGHT_R_AIR 23      /* ... and in the air */
#define SIGHT_COLS ((MAP_MAX_W + 7) / 8)

typedef struct {
    uint8_t owner;    /* whose units provide sight (own units stay visible) */
    uint8_t explored[MAP_MAX_H][SIGHT_COLS];
    uint8_t visible[MAP_MAX_H][SIGHT_COLS];    /* terrain in view (hidden map) */
    uint8_t vis_ground[MAP_MAX_H][SIGHT_COLS]; /* ground targets seen here */
    uint8_t vis_air[MAP_MAX_H][SIGHT_COLS];    /* flying targets seen here */
    uint8_t vis_obj[MAP_MAX_H][SIGHT_COLS];    /* objects seen here */
} Sight;

/* Empty map for one owner. */
void sight_init(Sight *s, uint8_t owner);
/* Recompute `visible` from all units of the owner; `explored` accumulates.
 * Deterministic, no RNG. */
void sight_compute(const World *w, Sight *s);
/* Field flags; false outside the map. */
bool sight_explored(const Sight *s, const World *w, int16_t x, int16_t y);
bool sight_visible(const Sight *s, const World *w, int16_t x, int16_t y);
/* Is this unit (not an own one, not invisible) seen by the owner: on its own
 * height and by the K11.2 rules. Own units are always seen. */
bool sight_unit_visible(const Sight *s, const World *w, const Unit *u);
/* Is an object on (x, y) seen (never one under a canopy for a flyer)? */
bool sight_object_visible(const Sight *s, const World *w, int16_t x, int16_t y);
/* Does ONE observer (a creature, ground or air) see a target at (tx, ty), K11.1
 * and K11.2: the octagon, then the rules per height pair. `target_air` is the
 * target's height, `is_object` marks things on the ground (an object, never
 * seen under a canopy by a flier). Invisibility is not checked here. */
bool sight_sees(const World *w, const Unit *observer, int16_t tx, int16_t ty,
                bool target_air, bool is_object);
/* The octagon test of K11.1 for an offset (dx, dy) and a limit R. */
bool sight_in_reach(int16_t dx, int16_t dy, uint8_t r);
/* Can a shot or throw from (x0, y0) at height `fly0` (true: in the air) reach
 * (x1, y1) at height `fly1` (K11.6)? Ground to ground needs a clear line;
 * with the air involved only a roof stops it (indoors, over a roofed target). */
bool sight_shot_clear(const World *w, int16_t x0, int16_t y0, bool fly0,
                      int16_t x1, int16_t y1, bool fly1);
/* Clear ground line between two fields (endpoints exclusive, GDD 3.4),
 * honouring wrap-around. For targeting (M3c). */
/* Like sight_has_los, but tall grass does not block (spells, D36). */
bool sight_has_spell_los(const World *w, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
bool sight_has_los(const World *w, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
/* Whether a ground figure standing at (x0, y0) sees field (x, y): the same
 * shadowcast as sight_compute, for this one figure, terrain only (units do
 * not block). Cached per position and map generation. Drives the roof
 * display (D56). False beyond SIGHT_GROUND. */
bool sight_look(const World *w, int16_t x0, int16_t y0, int16_t x, int16_t y);
/* Magic Eye (K5.3): everything within D <= `range` of (x, y) is seen and
 * explored, walls ignored, like an airborne observer; also invisible ones. */
void sight_add_eye(Sight *s, const World *w, int16_t x, int16_t y, uint8_t range);

#endif
