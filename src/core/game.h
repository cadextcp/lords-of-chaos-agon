/*
 * Portal and victory points (GDD 9, PM 29): the portal opens within the
 * scenario's round span; wizards escape through it with their carried
 * treasures. Kills score - double when a wizard strikes in melee (not
 * with ranged weapons). The game ends when every wizard has escaped or
 * died; whoever does not make it scores nothing.
 */
#ifndef LOC_GAME_H
#define LOC_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"
#include "world.h"

#define VP_ESCAPE 10   /* own design (GDD 9 leaves the amount open) */

typedef struct {
    int16_t portal_x, portal_y;   /* -1 = no portal on this map */
    uint8_t portal_round;         /* the round it opens */
    bool portal_open;
    uint8_t escaped;              /* owner bitmask of wizards through */
    uint16_t vp[OWN_NEUTRAL];
} Game;

/* Portal at (x, y); opens between round rmin and rmax (deterministic
 * through the passed RNG). */
void game_init(Game *g, int16_t x, int16_t y, uint8_t rmin, uint8_t rmax,
               Rng *rng);
/* Call at the start of every round. */
void game_new_round(Game *g, uint8_t round);
/* A unit steps onto the portal field: wizards escape with their carried
 * treasures (VP_ESCAPE plus every carried OC_TREASURE), creatures
 * cannot pass. The escaped wizard is removed from the world. */
bool game_try_enter_portal(Game *g, World *w, uint8_t unit);
/* VP for one kill: the victim's value from the creature table, doubled
 * when the killer is a wizard striking in melee (no ranged weapon,
 * AMI 4). Own units score nothing, independents never score. */
void game_kill_credit(Game *g, const Kill *k);
/* Credit every logged kill (world_kill_unit) and clear the log. Call
 * after every action that may kill. */
void game_credit_kills(Game *g, World *w);
/* Over when no wizard remains on the map (escaped or dead). */
bool game_over(const Game *g, const World *w);

#endif
