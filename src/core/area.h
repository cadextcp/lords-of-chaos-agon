/*
 * Area effects on fields (GDD 3.2/7.2, M4d): Magic Fire, Gooey Blob,
 * Tangle Vine and Flood occupy fields with a strength; the round end
 * spreads and weakens them (decision D22/F4, start values below).
 * Deterministic through the game RNG.
 */
#ifndef LOC_AREA_H
#define LOC_AREA_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"
#include "world.h"

typedef enum {
    AREA_NONE = 0xFF,
    AREA_FIRE,       /* burns, destroys objects, hits only enemies */
    AREA_BLOB,       /* sticky, hits undead too, impassable while strong */
    AREA_VINE,       /* holds units on vulnerable terrain, wounds */
    AREA_FLOOD       /* water: drowns non-water-types, extinguishes fire */
} AreaKind;

#define AREAS_MAX 48
#define AREA_FIELDS_MAX 48   /* per area, performance (F4) */

/* Start values (own design, D7 - recorded in the GDD): damage per round,
 * vulnerable terrain per kind. PASSABLE = everything walkable. */
uint8_t area_damage(AreaKind kind);            /* fire/blob/vine per round */
bool area_terrain_ok(AreaKind kind, uint8_t floor, uint8_t feature);

/* One area: kind, strength and its fields (packed 16-bit x + y * w). */
typedef struct {
    AreaKind kind;
    uint8_t strength;     /* level of the casting spell */
    uint8_t owner;        /* whose spell (fire hits enemies only) */
    uint8_t count;
    uint16_t fields[AREA_FIELDS_MAX];
    uint8_t power[AREA_FIELDS_MAX];   /* per-field strength */
} Area;

/* Cast: start a new area (strength = spell level) or refresh the
 * closest one of the same kind. True when the field took the effect. */
bool area_cast(World *w, AreaKind kind, uint8_t level, uint8_t owner,
               int16_t x, int16_t y);
/* Area (kind) on the field, NULL when none. */
Area *area_at(const World *w, int16_t x, int16_t y);
/* Round end: spread, weaken, apply damage, drown, extinguish. Kills go
 * through the normal kill path (simple VP, GDD 9). Returns the number
 * of active areas afterwards. */
uint8_t area_round_end(World *w, Rng *rng);

/* View helper: kind and per-field strength on (x, y) (for tiles). */
AreaKind area_kind_at(const World *w, int16_t x, int16_t y);
uint8_t area_power_at(const World *w, int16_t x, int16_t y);
/* Forget all areas (new game / test). */
void area_reset(void);
/* Number of active areas (tests, bench). */
uint8_t area_active_count(void);
/* Does the field hold a strong enough area of this kind? (Blob/Vine
 * block movement while power >= 2; start value.) */
bool area_blocks_kind(const World *w, int16_t x, int16_t y);

#endif
