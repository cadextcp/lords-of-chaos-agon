/*
 * Line of sight and the hidden map (GDD 3.4, 11.2): every player keeps a
 * bitmap of explored fields and one of the fields currently seen by any of
 * his units. Sight range is 9 fields on the ground, 11 in the air
 * (Chebyshev distance); terrain in between blocks, the endpoints do not.
 */
#ifndef LOC_SIGHT_H
#define LOC_SIGHT_H

#include <stdbool.h>
#include <stdint.h>

#include "world.h"

#define SIGHT_GROUND 9
#define SIGHT_AIR 11
#define SIGHT_COLS ((MAP_MAX_W + 7) / 8)

typedef struct {
    uint8_t owner;    /* whose units provide sight (own units stay visible) */
    uint8_t explored[MAP_MAX_H][SIGHT_COLS];
    uint8_t visible[MAP_MAX_H][SIGHT_COLS];
} Sight;

/* Empty map for one owner. */
void sight_init(Sight *s, uint8_t owner);
/* Recompute `visible` from all units of the owner; `explored` accumulates.
 * Deterministic, no RNG. */
void sight_compute(const World *w, Sight *s);
/* Field flags; false outside the map. */
bool sight_explored(const Sight *s, const World *w, int16_t x, int16_t y);
bool sight_visible(const Sight *s, const World *w, int16_t x, int16_t y);
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
/* Magic Eye (M4b): mark the fields around (x, y) visible and explored,
 * ignoring walls, like an airborne observer. */
void sight_add_eye(Sight *s, const World *w, int16_t x, int16_t y);

#endif
