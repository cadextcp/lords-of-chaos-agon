/* Spell rules on top of the generated table (gen/data.h, data/spells.csv). */
#ifndef LOC_SPELLS_H
#define LOC_SPELLS_H

#include <stdint.h>

#include "gen/data.h"
#include "world.h"

#define SPELL_MAX_LEVEL 8

/* Mana cost at a level (0..8): base + level * step (observation B3.5). */
uint8_t spell_mana(uint8_t spell, uint8_t level);

/* Known spells of one wizard: the level drops by one per cast, at 0 the
 * spell is used up for the game (GDD 7.1). */
typedef struct {
    uint8_t level[SPELL_COUNT];
} Spellbook;

/* Starting books until the scenarios carry them (M3g): a small test set
 * per owner. */
void spellbook_default(Spellbook *b, uint8_t owner);

/* Everything needed for a cast: a grounded wizard with a known spell,
 * mana and AP for ACT_CAST. */
bool spell_can_cast(const World *w, const Spellbook *b, uint8_t wiz, uint8_t spell);

/* Summon `level` creatures of the spell's kind on free ground fields
 * around the wizard (GDD 7.2). Pays AP and mana and burns the level;
 * without enough space the mana is lost and nothing appears. Returns
 * the number of creatures placed. */
uint8_t spell_summon(World *w, Spellbook *b, uint8_t wiz, uint8_t spell);

#endif
