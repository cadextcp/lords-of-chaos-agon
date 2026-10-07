/*
 * Area effects on fields (K5.3, D67 0e): Magic Fire, Gooey Blob, Tangle
 * Vine and Flood occupy fields. Fire and blob ignite by the flammability
 * of the ground at the cast, spread each round and die out by the
 * original's survival rule; vine and flood fill a 9x9 square and stay.
 * Every wizard has one area per kind whose level is the book level of the
 * last cast (a weaker later cast weakens the whole fire). Deterministic
 * through the game RNG.
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
    AREA_BLOB,       /* sticky, hits undead too, impassable */
    AREA_VINE,       /* impassable, wounds everyone */
    AREA_FLOOD       /* water: slow to cross, drowns non-water types, extinguishes fire */
} AreaKind;

#define AREAS_MAX 48
#define AREA_FIELDS_MAX 48   /* per area, performance (F4) */

/* Damage per round at the area's level (K8.3): fire 25 + 2F, blob
 * 16 + 2 (B - 1), vine 8, flood 0. Ignores Defence. */
uint8_t area_damage(AreaKind kind, uint8_t level);

/* One area: kind, owner, the level of the last cast and its fields
 * (packed 16-bit x + y * w). */
typedef struct {
    AreaKind kind;
    uint8_t strength;     /* book level of the last cast (F or B) */
    uint8_t owner;        /* whose spell (fire and blob spare their own) */
    uint8_t count;
    uint16_t fields[AREA_FIELDS_MAX];
    uint8_t power[AREA_FIELDS_MAX];   /* kept in step with strength (view, save) */
} Area;

/* Flammability / susceptibility of a field to an area kind, 0..15: the
 * feature's own fire value when set, else the ground's (K8.4). */
uint8_t area_susceptibility(const World *w, AreaKind kind, int16_t x, int16_t y);

/* Cast at book level `level` onto (x, y). Fire and blob ignite the target
 * field when RND(10 - L) < flammability; vine and flood try every field of
 * the 9x9 square with D <= L + 3 by RND(18 - 2L + 2D) < susceptibility.
 * Returns the number of fields that took hold (0: it fizzled). The mana is
 * the caller's business. */
uint8_t area_cast(World *w, Rng *rng, AreaKind kind, uint8_t level, uint8_t owner,
                  int16_t x, int16_t y);
/* Set one field alight / flooded directly (dragon fire, tests). Ignores the
 * dice but not the terrain; false when the field does not take it. */
bool area_set(World *w, AreaKind kind, uint8_t level, uint8_t owner,
              int16_t x, int16_t y);
/* Level of the owner's area of that kind (the last cast), 1 when he has none. */
uint8_t area_level(AreaKind kind, uint8_t owner);
/* Area (kind) on the field, NULL when none. */
Area *area_at(const World *w, int16_t x, int16_t y);
/* Round end: damage, spread, survival, objects. Kills go through the
 * normal kill path. Returns the number of active areas afterwards. */
uint8_t area_round_end(World *w, Rng *rng);

/* View helper: kind and per-field strength on (x, y) (for tiles). */
AreaKind area_kind_at(const World *w, int16_t x, int16_t y);
uint8_t area_power_at(const World *w, int16_t x, int16_t y);
/* Savegame (M4i): copy out up to cap active areas / replace the pool. */
uint8_t area_export(Area *dst, uint8_t cap);
void area_import(const Area *src, uint8_t n);
/* Forget all areas (new game / test). */
void area_reset(void);
/* Number of active areas (tests, bench). */
uint8_t area_active_count(void);
/* Blob and vine block movement; fire and flood only cost more. */
bool area_blocks_kind(const World *w, int16_t x, int16_t y);
/* Toughness of the blob or vine on the field for a terrain attack (vine 40,
 * blob 50), 0 when there is nothing to hit. */
uint8_t area_toughness(const World *w, int16_t x, int16_t y);
/* The attacker broke through: the field is free again. */
void area_remove_field(World *w, int16_t x, int16_t y);

#endif
