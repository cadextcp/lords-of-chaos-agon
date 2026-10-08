/*
 * Computer opponents (GDD 10): independent creatures hunt the nearest
 * enemy they can see, wizards manage mana, summon and head for the
 * portal. The AI only knows what its units see (hidden movement) and
 * plays deterministically through the turn RNG.
 */
#ifndef LOC_AI_H
#define LOC_AI_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"
#include "sight.h"
#include "spells.h"
#include "turn.h"
#include "rng.h"
#include "world.h"

/* Nearest enemy of `unit` within `range` fields that stands in its line
 * of sight (single ray - the cheap hunter approximation). NO_UNIT if
 * none. Flyers are ignored by grounded hunters (no melee). */
uint8_t ai_nearest_enemy(const World *w, uint8_t unit, uint8_t range);

/* One greedy step of `unit` towards (x, y): diagonal first, sidesteps
 * around blocked fields. True when a step was made. */
bool ai_step_toward(World *w, Rng *rng, uint8_t unit, int16_t x, int16_t y);

/* Hunter turn of one independent creature (GDD 10): attack an adjacent
 * enemy, otherwise chase the nearest visible one, otherwise wander. */
void ai_hunter(World *w, Rng *rng, uint8_t unit);
/* Guard turn (GDD 10, M4h): like the hunter, but never strays further
 * than `home_range` fields from its spawn - it attacks intruders and
 * returns home afterwards. */
void ai_guard(World *w, Rng *rng, uint8_t unit, uint8_t home_range);
/* Mark the unit's current field as its guard post (spawn time). */
void ai_set_post(World *w, uint8_t unit);

/* ai_hunter for every unit of `owner` except the one with id skip_id
 * (NO_UNIT: none). Works on an id snapshot, so kills that reorder the
 * unit list neither skip a hunter nor let one act twice. */
void ai_run_hunters(World *w, Rng *rng, uint8_t owner, uint8_t skip_id);

/* The AI wizard of a scenario (K10.2): his values and the spell priorities. The
 * priorities are halved for good as he casts (K10.5) - reloading resets them. */
typedef struct {
    bool present;
    char name[11];
    uint8_t mana, ap, sta, con, com, def, mr, carry, vp;
    uint8_t prio[SPELL_COUNT];
} AiProfile;

/* Load the AI part of a compiled scenario (.scn v2/v3, ADR 0014): profiles, routes, plans of
 * map units, triggers. Routes, plans and triggers go into the world (load the
 * map first). */
bool ai_scenario_load(World *w, AiProfile *profiles, const uint8_t *data, uint16_t len);
/* Give the wizard of `owner` his scenario values (stats, mana, victory value). */
void ai_profile_apply(const AiProfile *profiles, World *w, Game *g, uint8_t owner);
/* A new creature gets its plan (K10.7): the bodyguard roll by its Aggressiveness
 * or a random route among the first route_summon_n that fits it; wizards take a
 * route flagged for wizards and are never aggressive. */
void ai_plan_new(World *w, Rng *rng, uint8_t unit);

/* The wizard (or a rider-wizard) of an owner: unit index, NO_UNIT if none. */
uint8_t ai_wizard_of(const World *w, uint8_t owner);

/* Context the wizard AI needs; keep it in the game state. */
typedef struct {
    Spellbook *books;   /* [OWN_NEUTRAL] per owner */
    Game *game;
    AiProfile *profiles;   /* [OWN_NEUTRAL], may be NULL: no priorities, no spells */
} AiCtx;

/* One wizard phase (owner = turn->phase): own creatures hunt, the
 * wizard melees adjacent enemies, summons while short on creatures,
 * walks to the portal (site or open) and escapes through it. Kills are
 * credited to ctx->game. Registered as turn callback. */
void ai_wizard_phase(Turns *t, World *w, void *ctx);

#endif
