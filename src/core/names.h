/*
 * Display names (German UI, GDD decision pending for other languages).
 * All player-visible strings of the core live here so a later translation
 * touches one file.
 */
#ifndef LOC_NAMES_H
#define LOC_NAMES_H

#include <stdint.h>

#include "sight.h"
#include "world.h"

const char *name_unit(const Unit *u);
/* Whose phase it is: "Zauberer-1".."Zauberer-4", "" for independents. */
const char *name_owner(uint8_t owner);
const char *name_floor(uint8_t floor);
const char *name_feature(uint8_t feature);
const char *name_decor(uint8_t decor);
const char *name_object(uint16_t tile);

/* Look mode (GDD 5.1): one line about a field - the unit standing there
 * (own always, enemies only in current sight, flyers get a suffix), else
 * the topmost ground entry. Unexplored fields stay dark. buf should hold
 * 24 characters. Sight NULL shows everything (tests). */
const char *describe_field(const World *w, const Sight *s, int16_t x, int16_t y,
                           char *buf, uint8_t len);

#define GROUND_MAX 3
/* What lies under (x, y), topmost first: objects, walkable feature, decor,
 * else the floor. Returns the number of names written (1..GROUND_MAX). */
uint8_t ground_names(const World *w, int16_t x, int16_t y, const char *out[GROUND_MAX]);

#endif
