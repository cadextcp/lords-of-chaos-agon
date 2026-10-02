/*
 * World state: map layers (floor, decor, feature), units and objects.
 * Platform-free; the view (view.h) turns it into tile layers for drawing.
 */
#ifndef LOC_WORLD_H
#define LOC_WORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "map_def.h"

#define MAP_MAX_W 36
#define MAP_MAX_H 36
#define MAX_UNITS 32
#define MAX_OBJECTS 64
#define NO_UNIT 0xFF

typedef enum {
    FL_STONE, FL_WOOD, FL_GRASS, FL_PATH, FL_TALL_GRASS, FL_FOREST, FL_MAGIC_WOOD,
    FL_SHADOW_WOOD, FL_SWAMP, FL_WATER, FL_RUBBLE, FL_COUNT
} Floor;
typedef enum { DE_NONE, DE_RUG, DE_PENTACLE } Decor;
typedef enum {
    FE_NONE, FE_WALL, FE_DOOR_CLOSED, FE_DOOR_OPEN, FE_BED, FE_BOOKSHELF,
    FE_CANDLE, FE_CAULDRON, FE_TABLE, FE_CHAIR, FE_DRAWERS, FE_CHEST, FE_TREE,
    FE_ROCK, FE_COUNT
} Feature;

/* Status flags shown as panel icons (PM 11). */
enum { UF_UNDEAD = 1, UF_FLYING = 2, UF_MOUNT = 4, UF_WOUNDED = 8, UF_INVISIBLE = 16 };

typedef struct {
    uint8_t x, y;
    uint8_t kind;   /* CreatureKind */
    uint8_t owner;  /* Owner */
    uint8_t flags;  /* UF_* */
    uint8_t native; /* NATIVE_* terrain type (pays floor cost there) */
    uint8_t ap, ap_max, ap_fly;
    uint8_t sta, sta_max;     /* stamina */
    uint8_t con, con_max;     /* constitution */
    uint8_t com, def;         /* combat, defence */
    uint8_t mr;               /* magic resistance */
    uint8_t mana, mana_max;   /* wizards only */
} Unit;

typedef struct {
    uint8_t x, y;
    uint8_t tile;
} Object;

typedef struct {
    uint8_t w, h, wrap;
    uint8_t generation;   /* bumped whenever the map layers change (view cache) */
    uint8_t floor[MAP_MAX_H][MAP_MAX_W];
    uint8_t decor[MAP_MAX_H][MAP_MAX_W];
    uint8_t feature[MAP_MAX_H][MAP_MAX_W];
    Unit units[MAX_UNITS];
    uint8_t unit_count;
    Object objects[MAX_OBJECTS];
    uint8_t object_count;
} World;

/* Load a binary map (.map, ADR 0008). Validates everything first; on
 * false the world is left unchanged. */
bool world_load_bin(World *w, const uint8_t *data, uint16_t len);
/* Call after changing floor/decor/feature (door opened ...). */
void world_map_changed(World *w);

/* Normalise (x, y) for wrapping maps. Returns false if outside a
 * non-wrapping map. */
bool world_wrap(const World *w, int16_t *x, int16_t *y);

/* Feature at (x, y); FE_NONE outside a non-wrapping map. */
uint8_t world_feature(const World *w, int16_t x, int16_t y);
/* Floor at (x, y); FL_GRASS outside a non-wrapping map. */
uint8_t world_floor(const World *w, int16_t x, int16_t y);
/* Wall or door: forms the connected wall line (GDD 11.2). */
bool world_is_wall_line(const World *w, int16_t x, int16_t y);
/* Blocks sight between ground positions: floor (data/costs.csv) or a
 * tall feature (GDD 3.4). */
bool world_blocks_sight(const World *w, int16_t x, int16_t y);
/* Feature blocks ground movement (GDD 3.3 furniture table). */
bool world_blocks(const World *w, int16_t x, int16_t y);

uint8_t world_unit_at(const World *w, int16_t x, int16_t y);
/* AP cost to enter (x, y); diagonal steps cost 3/2, rounded up (GDD 5.3). */
uint8_t world_step_cost(const World *w, int16_t x, int16_t y, bool diagonal);
/* Same for a unit: its terrain type (wood/water/rock) pays only the plain
 * floor cost in matching terrain (GDD 5.3). */
uint8_t world_unit_step_cost(const World *w, uint8_t unit, int16_t x, int16_t y,
                             bool diagonal);
/* Move a unit one step (8 directions); false if blocked, occupied, outside
 * or not enough AP. Spends the AP on success. */
bool world_move_unit(World *w, uint8_t unit, int8_t dx, int8_t dy);
/* Start of a turn: refill AP, recover 25 % stamina (GDD 5.3). */
void world_new_turn(World *w);

/* Character for dumps (floor/feature/unit at a glance). */
char world_char(const World *w, int16_t x, int16_t y);

#endif
