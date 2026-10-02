/*
 * Display names (German UI, GDD decision pending for other languages).
 * All player-visible strings of the core live here so a later translation
 * touches one file.
 */
#ifndef LOC_NAMES_H
#define LOC_NAMES_H

#include <stdint.h>

#include "world.h"

const char *name_unit(const Unit *u);
const char *name_floor(uint8_t floor);
const char *name_feature(uint8_t feature);
const char *name_decor(uint8_t decor);
const char *name_object(uint8_t tile);

#define GROUND_MAX 3
/* What lies under (x, y), topmost first: objects, walkable feature, decor,
 * else the floor. Returns the number of names written (1..GROUND_MAX). */
uint8_t ground_names(const World *w, int16_t x, int16_t y, const char *out[GROUND_MAX]);

#endif
