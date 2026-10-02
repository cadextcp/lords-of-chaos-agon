/*
 * Melee combat (GDD 6, decision D16): Combat is rolled against Defence
 * with a random share, damage comes off the Constitution. The defender
 * strikes back while it still has AP and stamina (PM 18). Fatal wounds
 * (a single hit above 25 % of the Constitution, PM 17) bleed one point
 * per round until death or healing. Bumping into impassable terrain
 * attacks it; features have a toughness (data/features.csv), walls are
 * indestructible.
 */
#ifndef LOC_COMBAT_H
#define LOC_COMBAT_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"
#include "world.h"

typedef struct {
    bool hit;             /* attacker connected */
    uint8_t damage;       /* constitution lost by the defender */
    bool wound;           /* fatal wound opened (PM 17) */
    bool died;            /* defender died (already removed) */
    bool returned;        /* defender struck back (PM 18) */
    bool return_hit;
    uint8_t return_damage;
    bool return_wound;    /* the return blow opened the attacker's wound */
    bool attacker_died;   /* the return blow killed the attacker */
} CombatResult;

/* Hit chance in percent, for tests and the AI: 50 + 5 per point of
 * Combat over Defence, clamped to 10..90. */
uint8_t combat_hit_chance(uint8_t com, uint8_t def);

/* One melee exchange. False (nothing happens) when the attack is not
 * allowed: not adjacent, same owner, a grounded attacker against a
 * flyer, or the attacker lacks AP. A dying unit is removed immediately;
 * at most one unit dies per exchange, so unit indices shift at most
 * once (see world_remove_unit). */
bool combat_melee(World *w, Rng *rng, uint8_t att, uint8_t def, CombatResult *out);

/* Terrain attack (GDD 3.3): damage rolled against the feature's
 * toughness; a lucky hit smashes it. Returns the damage, 0 when there
 * is nothing to hit (no feature, walkable, wall) or not enough AP.
 * *destroyed tells whether the feature is gone. */
uint8_t combat_terrain(World *w, Rng *rng, uint8_t att, int16_t x, int16_t y,
                       bool *destroyed);

#endif
