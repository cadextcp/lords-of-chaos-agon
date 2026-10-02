/* Spell rules on top of the generated table (gen/data.h, data/spells.csv). */
#ifndef LOC_SPELLS_H
#define LOC_SPELLS_H

#include <stdint.h>

#include "gen/data.h"

#define SPELL_MAX_LEVEL 8

/* Mana cost at a level (0..8): base + level * step (observation B3.5). */
uint8_t spell_mana(uint8_t spell, uint8_t level);

#endif
