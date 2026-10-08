/*
 * A fresh scenario world (D35): wild animals, chests and loose finds at
 * random places, and herds that now and then cross the map. Everything
 * draws from the game RNG, so a seed replays the same world.
 */
#ifndef LOC_POPULATE_H
#define LOC_POPULATE_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"
#include "world.h"

#define POP_ANIMALS_MIN 5      /* + up to 3 more */
#define POP_CHESTS_MIN 5       /* + up to 2 more */
#define POP_FINDS_MIN 6        /* + up to 3 more */
#define POP_KEYS 2             /* chest keys among the loose finds */
#define POP_WIZARD_GAP 8       /* animals keep this far from wizards */
#define POP_CHEST_GAP 4        /* chests and finds: not on the doorstep */
#define HERD_FIRST_ROUND 4     /* no herd before this round */
#define HERD_CHANCE 15         /* percent per round, while none is out */
#define HERD_ROOM 8            /* unit slots kept free for summons */

/* Place animals (territorial ones keep their spawn field as home),
 * chests and loose food/ingredients/keys on fitting free fields. */
void populate_scenario(World *w, Rng *rng);
/* Round hook: maybe let a herd of 3-4 enter at a map edge, walking one
 * way. True when one came. */
bool populate_herd(World *w, Rng *rng, uint8_t round);

#endif
