/*
 * Carried objects (GDD 8): every unit has a small inventory with one
 * object "in use"; weapons give combat bonuses (the shield always, when
 * carried), treasures carry victory points through the portal (GDD 9).
 * Action costs come from data/actions.csv.
 */
#ifndef LOC_ITEMS_H
#define LOC_ITEMS_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"
#include "world.h"

#define UNIT_ITEMS 6
#define NO_ITEM 0xFF

/* Total weight carried (against the creature's carry limit). */
uint8_t items_weight(const World *w, uint8_t unit);
/* Kind of the topmost ground object at (x, y), NO_ITEM when none. */
uint8_t items_kind_at(const World *w, int16_t x, int16_t y);
/* Pick up the object under the unit (ACT_PICK_UP): weight limit applies. */
bool items_pick_up(World *w, uint8_t unit);
/* Drop the object in use onto the unit's field (ACT_DROP). */
bool items_drop(World *w, uint8_t unit);
/* Wield the next carried object (ACT_CHANGE); empty hands are allowed. */
bool items_cycle(World *w, uint8_t unit);
/* Throw the object in use along a direction: it flies up to 6 fields,
 * stops at terrain or a unit (thrown damage, GDD 6.1) and lands on the
 * last free field. ACT_THROW. */
bool items_throw(World *w, Rng *rng, uint8_t unit, int8_t dx, int8_t dy);
/* Fire the bow in use at a field (ACT_FIRE): range 6, line of sight,
 * ground and air targets, Defence counts. */
bool items_fire(World *w, Rng *rng, uint8_t unit, int16_t tx, int16_t ty,
                uint8_t *damage);
/* Effective values with weapon bonuses (D16/D18/D21): in-use weapon
 * Combat; Defence plus one carried shield (in use or not, never more
 * than one). Every attack - melee, throw, bow, bolt - uses these. */
uint8_t items_combat(const World *w, uint8_t unit);
uint8_t items_defence(const World *w, uint8_t unit);

#endif
