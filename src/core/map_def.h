/*
 * Static map definitions, generated from data/maps/<name>.txt by tools/gen_maps.py
 * into src/core/gen/maps.c. Grids are row-major strings using the character
 * legend documented in data/maps/wizard_house.txt.
 */
#ifndef LOC_MAP_DEF_H
#define LOC_MAP_DEF_H

#include <stdint.h>

typedef enum { CR_WIZARD, CR_GOBLIN } CreatureKind;
typedef enum { OWN_P1, OWN_P2, OWN_P3, OWN_P4, OWN_NEUTRAL } Owner;

typedef struct {
    uint8_t x, y;
    uint8_t kind;   /* CreatureKind */
    uint8_t owner;  /* Owner */
} MapUnit;

typedef struct {
    uint8_t x, y;
    uint8_t tile;   /* TileId of the object icon */
} MapObject;

typedef struct {
    uint8_t w, h;
    uint8_t wrap;   /* 1 = world wraps around at the edges (classic maps) */
    const char *floor;
    const char *feature;
    const char *decor;
    const MapUnit *units;
    uint8_t unit_count;
    const MapObject *objects;
    uint8_t object_count;
} MapDef;

extern const MapDef MAP_WIZARD_HOUSE;

#endif
