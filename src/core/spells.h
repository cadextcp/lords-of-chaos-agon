/* Spell rules on top of the generated table (gen/data.h, data/spells.csv). */
#ifndef LOC_SPELLS_H
#define LOC_SPELLS_H

#include <stdint.h>

#include "gen/data.h"
#include "rng.h"
#include "world.h"

#define SPELL_MAX_LEVEL 8

/* Mana cost at a level (0..8): base + level * step (observation B3.5). */
uint8_t spell_mana(uint8_t spell, uint8_t level);

/* Known spells of one wizard: the level drops by one per cast, at 0 the
 * spell is used up for the game (GDD 7.1). */
typedef struct {
    uint8_t level[SPELL_COUNT];
} Spellbook;

/* Fill the books of all wizards from a compiled scenario file (.scn,
 * M4a): "LOCS" v1, see tools/gen_scenarios.py. False on malformed data. */
bool spellbook_load(Spellbook *books, const uint8_t *data, uint16_t len);

/* Everything needed for a cast: a grounded wizard with a known spell,
 * mana and AP for ACT_CAST. */
bool spell_can_cast(const World *w, const Spellbook *b, uint8_t wiz, uint8_t spell);

/* Summon `level` creatures of the spell's kind on free ground fields
 * around the wizard (GDD 7.2). Pays AP and mana and burns the level;
 * without enough space the mana is lost and nothing appears. Returns
 * the number of creatures placed. */
uint8_t spell_summon(World *w, Spellbook *b, uint8_t wiz, uint8_t spell);

/* Spell range in fields (own design until WinUAE says more, GDD 13). */
#define SPELL_RANGE 6

typedef struct {
    bool allowed;       /* cast went through (range, LOS, wall) */
    bool hit;
    uint8_t damage;
    bool died;          /* the target died and is removed */
    uint8_t splash_hits;/* lightning: neighbours hit */
    bool terrain_smashed;   /* lightning at the target field */
} SpellShot;

/* Magic Bolt (GDD 7.2): physical damage with line of sight and range,
 * Defence counts (same model as melee, D16). Hits ground and air
 * units; nothing else. */
bool spell_bolt(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                int16_t x, int16_t y, Rng *rng, SpellShot *out);
/* Magic Lightning: bolt plus one roll for every neighbouring field;
 * smashes destructible terrain at the target; massive target fields
 * (walls) are rejected. */
bool spell_lightning(World *w, Spellbook *b, uint8_t wiz,
                     int16_t x, int16_t y, Rng *rng, SpellShot *out);

#endif
