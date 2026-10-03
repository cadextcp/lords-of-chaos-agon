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
#include "world.h"

/* Nearest enemy of `unit` within `range` fields that stands in its line
 * of sight (single ray - the cheap hunter approximation). NO_UNIT if
 * none. Flyers are ignored by grounded hunters (no melee). */
uint8_t ai_nearest_enemy(const World *w, uint8_t unit, uint8_t range);

/* One greedy step of `unit` towards (x, y): diagonal first, sidesteps
 * around blocked fields. True when a step was made. */
bool ai_step_toward(World *w, uint8_t unit, int16_t x, int16_t y);

/* Hunter turn of one independent creature (GDD 10): attack an adjacent
 * enemy, otherwise chase the nearest visible one, otherwise wander. */
void ai_hunter(World *w, Rng *rng, uint8_t unit);

/* ai_hunter for every unit of `owner` except the one with id skip_id
 * (NO_UNIT: none). Works on an id snapshot, so kills that reorder the
 * unit list neither skip a hunter nor let one act twice. */
void ai_run_hunters(World *w, Rng *rng, uint8_t owner, uint8_t skip_id);

/* Context the wizard AI needs; keep it in the game state. */
typedef struct {
    Spellbook *books;   /* [OWN_NEUTRAL] per owner */
    Game *game;
} AiCtx;

/* One wizard phase (owner = turn->phase): own creatures hunt, the
 * wizard melees adjacent enemies, summons while short on creatures,
 * walks to the portal (site or open) and escapes through it. Kills are
 * credited to ctx->game. Registered as turn callback. */
void ai_wizard_phase(Turns *t, World *w, void *ctx);

#endif
