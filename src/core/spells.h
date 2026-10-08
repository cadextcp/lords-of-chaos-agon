/* Spell rules on top of the generated table (gen/data.h, data/spells.csv). */
#ifndef LOC_SPELLS_H
#define LOC_SPELLS_H

#include <stdint.h>

#include "gen/data.h"
#include "rng.h"
#include "world.h"

/* Book levels: casts left and power at once. Cap 10 - the original's
 * starting book carries levels up to 10 (user anchor, 2026-10-04);
 * the earlier cap 8 was our own placeholder. */
#define SPELL_MAX_LEVEL 10

/* Mana cost at a level L (K5.2): mana base * (L + 1). */
uint8_t spell_mana(uint8_t spell, uint8_t level);

/* Known spells of one wizard: the level drops by one per cast, at 0 the
 * spell is used up for the game (GDD 7.1). */
typedef struct {
    uint8_t level[SPELL_COUNT];
} Spellbook;

/* Fill the books of all wizards from a compiled scenario file (.scn,
 * M4a): "LOCS" v1, see tools/gen_scenarios.py. False on malformed data. */
bool spellbook_load(Spellbook *books, const uint8_t *data, uint16_t len);

/* Mana of a cast at book level L (K5.2): the same for every spell. */
uint8_t spell_cast_mana(uint8_t spell, uint8_t level);
/* Summon and potion spells are cast on the ground only, never from the
 * air (CAST-G, F8). */
bool spell_needs_ground(uint8_t spell);
/* Everything needed for a cast: a wizard (on foot, riding or flying) with
 * a known spell, mana and AP for ACT_CAST; a flying wizard casts no
 * spell_needs_ground spell. */
bool spell_can_cast(const World *w, const Spellbook *b, uint8_t wiz, uint8_t spell);

/* Summon L creatures of the spell's kind (K5.3): each tries up to 40 times
 * for a random free neighbour field. Pays AP and mana and burns one level;
 * without any free field the mana is lost and nothing appears. Returns the
 * number of creatures placed. */
uint8_t spell_summon(World *w, Spellbook *b, uint8_t wiz, uint8_t spell, Rng *rng);

/* Reach of a targeted spell cast at book level L, in distance units of the
 * original (K1, 2 * max + min): 2L + 7, Magic Eye 3L + 10, Teleport 2L + 30. */
uint8_t spell_range(uint8_t spell, uint8_t level);
bool spell_in_range(const World *w, const Unit *u, uint8_t spell, uint8_t level,
                    int16_t x, int16_t y);
/* Attack value of Magic Bolt (4L + 25) and Lightning (4L + 30). */
uint8_t spell_attack_value(uint8_t spell, uint8_t level);

typedef struct {
    bool allowed;       /* cast went through (range, LOS, wall) */
    bool hit;
    uint8_t damage;
    bool died;          /* the target died and is removed */
    uint8_t splash_hits;/* lightning: neighbours hit */
    bool terrain_smashed;   /* lightning at the target field */
    uint8_t eye_range;      /* Magic Eye: how far it sees, 3L + 10 units */
} SpellShot;

/* Target height (F8): every targeted spell is cast at a position on the
 * ground (CAST-G, air = false) or in the air (CAST-A, air = true) and
 * only touches what is at that height. The line to the target follows
 * the shot rules of K11.6 for the caster's and the target's height. */
bool spell_line_clear(const World *w, const Unit *caster, int16_t x, int16_t y,
                      bool air);

/* Magic Bolt (GDD 7.2): physical damage with line of sight and range,
 * Defence counts (same model as melee, D16). Hits the unit at the aimed
 * height; nothing else. */
bool spell_bolt(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                int16_t x, int16_t y, bool air, Rng *rng, SpellShot *out);
/* Magic Lightning: bolt plus one roll for every neighbouring field at the
 * aimed height; a ground cast smashes destructible terrain; massive
 * target fields (walls) are rejected for a ground cast. */
bool spell_lightning(World *w, Spellbook *b, uint8_t wiz,
                     int16_t x, int16_t y, bool air, Rng *rng, SpellShot *out);

typedef enum {
    CAST_OK,          /* spell went through */
    CAST_REJECTED,    /* range, line of sight or a rule blocked it */
    CAST_NO_RES,      /* the target resisted (Curse, Subversion) */
    CAST_BAD_TERRAIN  /* area spell: in reach, but the field refuses it */
} CastResult;

/* The other targeted spells (M4b, GDD 7.2, D22/F1-F3). Magic Shield and
 * Enchant may target the caster himself; the others need a target
 * field. `air` is the target height (F8): Curse, Subversion and the
 * Magic Attack centre take the unit there, Enchant the units there,
 * Teleport into the air needs a flying potion; Shield and Eye ignore
 * it; the area spells take the ground only. out receives hits/damage
 * where it applies. */
CastResult spell_apply(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                       int16_t x, int16_t y, bool air, Rng *rng, SpellShot *out);

#endif
