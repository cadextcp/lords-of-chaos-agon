/*
 * Timed effects on units (GDD 2.1, M4b): potions and spells grant
 * effects with a strength and a remaining number of rounds; the round
 * end ticks them down. Strength and duration follow decision D22/F1.
 * Effect itself lives in world.h (unit state).
 */
#ifndef LOC_EFFECT_H
#define LOC_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

#include "world.h"

/* Grant (or refresh) an effect; false when all slots are taken. */
bool effect_grant(Unit *u, uint8_t kind, uint8_t power, uint8_t rounds);
/* Does the unit carry this effect? */
bool effect_active(const Unit *u, uint8_t kind);
/* Strength of the active effect, 0 when absent. */
uint8_t effect_power(const Unit *u, uint8_t kind);
/* Round end: one round off every effect, UF_INVISIBLE goes with the
 * effect. Returns true when something expired. */
bool effect_tick(Unit *u);

#endif
