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
    int16_t eye_x, eye_y;         /* Magic Eye: sight from here (M4b) */
    uint8_t eye_rounds;           /* ticks down each round */
    uint8_t kills[OWN_NEUTRAL];   /* credited kills (end screen, M5a) */
    uint16_t loot_vp[OWN_NEUTRAL];/* treasure VP carried through the portal */
    /* wizard AI (D62): where each wizard started (0xFF = not yet seen),
     * rounds of rage left and the round the rage was last rolled */
    uint8_t home_x[OWN_NEUTRAL], home_y[OWN_NEUTRAL];
    uint8_t rage[OWN_NEUTRAL], rage_round[OWN_NEUTRAL];
} Game;

typedef enum { OUT_RUNNING, OUT_WIN, OUT_LOSE } GameOutcome;

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
/* Over when no wizard remains on the map (escaped or dead). A wizard
 * riding a mount still counts (the pair is one unit, M4e). */
bool game_over(const Game *g, const World *w);
/* How the game stands for one owner: OUT_WIN once his wizard escaped
 * through the portal, OUT_LOSE when it is gone without escaping, else
 * OUT_RUNNING. Maps without a portal never end. */
GameOutcome game_outcome(const Game *g, const World *w, uint8_t owner);

#endif
