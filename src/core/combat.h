/*
 * Melee combat (D67, K6): damage = RND(min(255, 2 (Combat_eff + 1))) -
 * Defence_eff. The defender answers with a return blow every time it has
 * 4 AP and 4 stamina (it pays them). A single hit above a quarter of the
 * Constitution opens a wound (2 con per round). Bumping into impassable
 * terrain attacks it; features have a toughness (data/features.csv),
 * walls are indestructible.
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
    uint8_t damage;       /* constitution lost by the defender */
    bool wound;           /* fatal wound opened (PM 17) */
    bool died;            /* defender died (already removed) */
    bool returned;        /* defender struck back (free reaction, D27/D29) */
    bool return_hit;
    uint8_t return_damage;
    bool return_wound;    /* the return blow opened the attacker's wound */
    bool attacker_died;   /* the return blow killed the attacker */
} CombatResult;

/* One attack roll (K6.2): RND(min(255, 2 (A + 1))) - Defence_eff, never
 * below 0; 0 means a miss. Used by melee, return blows, throws, bow. */
uint8_t combat_roll(Rng *rng, uint8_t attack, uint8_t defence);
/* Chance in percent that combat_roll yields damage (for the AI, tests). */
uint8_t combat_hit_chance(uint8_t attack, uint8_t defence);

/* Apply `damage` to a unit (all damage sources share it): a single blow
 * above a quarter of the Constitution opens a fatal wound (PM 17), a
 * lethal one kills through world_kill_unit with the given killer.
 * *wound (optional) tells about the wound. Returns true on death. */
bool combat_damage(World *w, uint8_t target, uint8_t damage, uint8_t killer_kind,
                   uint8_t killer_owner, bool melee, bool *wound);

/* One melee exchange. False (nothing happens, *out zeroed) when the
 * attack is not allowed: not adjacent, same owner, a grounded attacker
 * against a flyer, or the attacker lacks AP. A dying unit is removed
 * immediately; at most one unit dies per exchange. Removal reorders the
 * unit list - re-find units by id afterwards. */
bool combat_melee(World *w, Rng *rng, uint8_t att, uint8_t def, CombatResult *out);
/* A territorial animal defends this far around its home field (D35). */
#define TERRITORY 3
/* Does unit `e` take a swing at `owner`'s figures that pass by (D59)?
 * Wizards' creatures and neutral monsters always; a wild animal only
 * when it bears a grudge against that owner, is charging at him (D37)
 * or - territorial - the figure at (x, y) stands in its territory. */
bool combat_hostile_to(const World *w, const Unit *e, uint8_t owner,
                       int16_t x, int16_t y);
/* Terrain attack (GDD 3.3): damage rolled against the feature's
 * toughness; a lucky hit smashes it. Returns the damage, 0 when there
 * is nothing to hit (no feature, walkable, wall) or not enough AP.
 * *destroyed tells whether the feature is gone. */
uint8_t combat_terrain(World *w, Rng *rng, uint8_t att, int16_t x, int16_t y,
                       bool *destroyed);

#endif
