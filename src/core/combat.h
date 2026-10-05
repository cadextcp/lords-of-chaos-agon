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
#include "sight.h"
#include "world.h"

typedef struct {
    bool hit;             /* attacker connected */
    bool crit;            /* attack roll <= 5 %: damage dice doubled (D30) */
    uint8_t damage;       /* constitution lost by the defender */
    bool wound;           /* fatal wound opened (PM 17) */
    bool died;            /* defender died (already removed) */
    bool returned;        /* defender struck back (free reaction, D27/D29) */
    bool return_hit;
    bool return_crit;
    uint8_t return_damage;
    bool return_wound;    /* the return blow opened the attacker's wound */
    bool attacker_died;   /* the return blow killed the attacker */
} CombatResult;

/* Attack rolls at or below this percentile are critical (D30): the
 * damage dice count twice (flat bonuses do not - D&D style). */
#define COMBAT_CRIT_PERCENT 5
/* Spell hit chance (D40): magic ignores armour and defence completely -
 * only magic resistance counts. The clamp keeps a critical failure
 * possible against the weakest resistance and a hit possible against the
 * strongest, so nothing is ever immune or automatic. */
uint8_t combat_spell_hit_chance(uint8_t magic_res);

/* Hit chance in percent, for tests and the AI: 50 + 5 per point of
 * Combat over Defence, clamped to 10..90. */
uint8_t combat_hit_chance(uint8_t com, uint8_t def);

/* Apply `damage` to a unit (all damage sources share it): a single blow
 * above a quarter of the Constitution opens a fatal wound (PM 17), a
 * lethal one kills through world_kill_unit with the given killer.
 * *wound (optional) tells about the wound; crit marks a critical hit
 * for the presentation event. Returns true on death. */
bool combat_damage(World *w, uint8_t target, uint8_t damage, uint8_t killer_kind,
                   uint8_t killer_owner, bool melee, bool *wound, bool crit);

/* One melee exchange. False (nothing happens, *out zeroed) when the
 * attack is not allowed: not adjacent, same owner, a grounded attacker
 * against a flyer, or the attacker lacks AP. A dying unit is removed
 * immediately; at most one unit dies per exchange. Removal reorders the
 * unit list - re-find units by id afterwards. */
bool combat_melee(World *w, Rng *rng, uint8_t att, uint8_t def, CombatResult *out);
/* Free swing (D26): like melee but without AP cost or return attack -
 * the swing a defender gets when its enemy moves out of contact.
 * False when the swing is not possible (not adjacent, same owner,
 * grounded attacker against a flyer, undead immunity). */
bool combat_free_swing(World *w, Rng *rng, uint8_t att, uint8_t def,
                       CombatResult *out);
/* After `unit` moved out of melee contact (an enemy was adjacent
 * before AND an enemy is adjacent now): one adjacent living enemy gets
 * a free swing. Returns the number of swings (0/1), out filled.
 * An enemy the moving side cannot see does not get the swing (playtest
 * 2026-10-05). `seen` is that side's field of view; without one - the AI
 * keeps no per-turn map - a direct line of sight between the two decides
 * instead. Since D44 that line counts roofs, so a creature inside a closed
 * house no longer swings at someone walking past outside. */
uint8_t combat_disengage_swings(World *w, Rng *rng, uint8_t unit,
                                const Sight *seen, CombatResult *out);

/* Terrain attack (GDD 3.3): damage rolled against the feature's
 * toughness; a lucky hit smashes it. Returns the damage, 0 when there
 * is nothing to hit (no feature, walkable, wall) or not enough AP.
 * *destroyed tells whether the feature is gone. */
uint8_t combat_terrain(World *w, Rng *rng, uint8_t att, int16_t x, int16_t y,
                       bool *destroyed);

#endif
