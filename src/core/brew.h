/*
 * Cauldrons and brewing (GDD 7.2, M4c): an empty cauldron and an
 * ingredient are dropped on one field, the wizard stands on it and casts
 * the potion spell - the cauldron fills with level+3 draughts. DRINK
 * straight from the cauldron, FILL an empty vial, throw the filled vial
 * (the bomb explodes). Dragons are summoned from a cauldron with dragon
 * herb; potion and dragon appear together (PM 21).
 */
#ifndef LOC_BREW_H
#define LOC_BREW_H

#include <stdbool.h>
#include <stdint.h>

#include "spells.h"
#include "world.h"

#include <stddef.h>   /* NULL */

/* Cauldron on (x, y), NULL when there is none. */
Cauldron *brew_cauldron_at(World *w, int16_t x, int16_t y);
/* Register cauldron objects placed by the map (call after loading). */
void brew_register_map_cauldrons(World *w);
/* Drop/replace the cauldron objects on a field (empty or full). */
void brew_set_cauldron(World *w, int16_t x, int16_t y, bool full,
                       uint8_t potion);
/* Brew: the wizard on (x, y) casts a potion spell - needs the empty
 * cauldron plus the matching ingredient object on the field. Fills the
 * cauldron with level+3 doses and burns the level. */
bool brew_cast(World *w, Spellbook *b, uint8_t wiz, uint8_t spell);
/* DRINK one draught from the cauldron under the unit (q): applies the
 * potion effect (F1 duration), one dose less. */
bool brew_drink(World *w, uint8_t unit);
/* FILL the empty vial in use from the cauldron under the unit (v): the
 * vial becomes a filled one, the cauldron loses one dose. */
bool brew_fill(World *w, uint8_t unit);
/* Drink a filled vial that is in use. */
bool brew_drink_vial(World *w, uint8_t unit);
/* Throw the filled vial in use (up to 6 fields, stops at walls and
 * units): the bomb explodes on the 3x3 around the impact, friend and
 * foe alike (D21); other potions shatter harmlessly. False when the
 * object in use is no filled vial. */
bool brew_throw_vial(World *w, Rng *rng, uint8_t unit, int8_t dx, int8_t dy);

/* The potion an ingredient belongs to (SP_*), WEAPON_NONE-like sentinel
 * NO_SPELL = 0xFF for non-ingredients. */
uint8_t brew_ingredient_potion(uint8_t object_kind);
/* Dragons need a cauldron with dragon herb under the wizard (PM 21);
 * the herb is spent with the successful summon. */
bool brew_dragon_ready(World *w, uint8_t wiz);
void brew_dragon_spend(World *w, uint8_t wiz);

#endif
