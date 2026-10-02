#include "world.h"

#include <string.h>

static uint8_t floor_from(char c)
{
    switch (c) {
    case 'w': return FL_WOOD;
    case 'g': return FL_GRASS;
    case 'p': return FL_PATH;
    default:  return FL_STONE;
    }
}

static uint8_t decor_from(char c)
{
    switch (c) {
    case 'r': return DE_RUG;
    case '*': return DE_PENTACLE;
    default:  return DE_NONE;
    }
}

static uint8_t feature_from(char c)
{
    switch (c) {
    case '#': return FE_WALL;
    case 'D': return FE_DOOR_CLOSED;
    case 'd': return FE_DOOR_OPEN;
    case 'B': return FE_BED;
    case 'S': return FE_BOOKSHELF;
    case 'K': return FE_CANDLE;
    case 'C': return FE_CAULDRON;
    case 'T': return FE_TABLE;
    case 'h': return FE_CHAIR;
    case 'M': return FE_DRAWERS;
    case 'X': return FE_CHEST;
    case 't': return FE_TREE;
    default:  return FE_NONE;
    }
}

/* Movement blocking per feature (GDD 3.3): chairs, candle stands and the
 * cauldron can be walked onto; furniture with a body blocks. */
static const bool FEATURE_BLOCKS[FE_COUNT] = {
    [FE_NONE] = false, [FE_WALL] = true, [FE_DOOR_CLOSED] = true,
    [FE_DOOR_OPEN] = false, [FE_BED] = true, [FE_BOOKSHELF] = true,
    [FE_CANDLE] = false, [FE_CAULDRON] = false, [FE_TABLE] = true,
    [FE_CHAIR] = false, [FE_DRAWERS] = true, [FE_CHEST] = true,
    [FE_TREE] = true,
};

/* Orthogonal AP cost per floor (data/costs.csv, GDD 5.3). */
static const uint8_t FLOOR_COST[FL_COUNT] = {
    [FL_STONE] = 4, [FL_WOOD] = 4, [FL_GRASS] = 4, [FL_PATH] = 3};
/* AP per turn until creature data arrives in M2 (GDD 5.3: wizard ~40). */
static const uint8_t KIND_AP[] = {[CR_WIZARD] = 40, [CR_GOBLIN] = 30};

void world_load(World *w, const MapDef *def)
{
    uint8_t x, y, i;
    uint16_t k;

    {
        uint8_t gen = (uint8_t)(w->generation + 1);
        memset(w, 0, sizeof *w);
        w->generation = gen;
    }
    w->w = def->w;
    w->h = def->h;
    w->wrap = def->wrap;
    for (y = 0; y < def->h; y++) {
        for (x = 0; x < def->w; x++) {
            k = (uint16_t)((uint16_t)y * def->w + x);
            w->floor[y][x] = floor_from(def->floor[k]);
            w->decor[y][x] = decor_from(def->decor[k]);
            w->feature[y][x] = feature_from(def->feature[k]);
        }
    }
    for (i = 0; i < def->unit_count && i < MAX_UNITS; i++) {
        w->units[i].x = def->units[i].x;
        w->units[i].y = def->units[i].y;
        w->units[i].kind = def->units[i].kind;
        w->units[i].owner = def->units[i].owner;
        w->units[i].ap_max = KIND_AP[def->units[i].kind];
        w->units[i].ap = w->units[i].ap_max;
    }
    w->unit_count = i;
    for (i = 0; i < def->object_count && i < MAX_OBJECTS; i++) {
        w->objects[i].x = def->objects[i].x;
        w->objects[i].y = def->objects[i].y;
        w->objects[i].tile = def->objects[i].tile;
    }
    w->object_count = i;
}

void world_map_changed(World *w)
{
    w->generation++;
}

bool world_wrap(const World *w, int16_t *x, int16_t *y)
{
    if (w->wrap) {
        *x = (int16_t)(((*x % w->w) + w->w) % w->w);
        *y = (int16_t)(((*y % w->h) + w->h) % w->h);
        return true;
    }
    return *x >= 0 && *y >= 0 && *x < w->w && *y < w->h;
}

uint8_t world_feature(const World *w, int16_t x, int16_t y)
{
    return world_wrap(w, &x, &y) ? w->feature[y][x] : FE_NONE;
}

uint8_t world_floor(const World *w, int16_t x, int16_t y)
{
    return world_wrap(w, &x, &y) ? w->floor[y][x] : FL_GRASS;
}

bool world_is_wall_line(const World *w, int16_t x, int16_t y)
{
    uint8_t f = world_feature(w, x, y);
    return f == FE_WALL || f == FE_DOOR_CLOSED || f == FE_DOOR_OPEN;
}

bool world_blocks(const World *w, int16_t x, int16_t y)
{
    if (!world_wrap(w, &x, &y))
        return true;
    return FEATURE_BLOCKS[w->feature[y][x]];
}

uint8_t world_unit_at(const World *w, int16_t x, int16_t y)
{
    uint8_t i;
    if (!world_wrap(w, &x, &y))
        return NO_UNIT;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].x == x && w->units[i].y == y)
            return i;
    return NO_UNIT;
}

uint8_t world_step_cost(const World *w, int16_t x, int16_t y, bool diagonal)
{
    uint8_t c = FLOOR_COST[world_floor(w, x, y)];
    return diagonal ? (uint8_t)((c * 3 + 1) / 2) : c;
}

bool world_move_unit(World *w, uint8_t unit, int8_t dx, int8_t dy)
{
    int16_t nx, ny;
    uint8_t cost;
    Unit *u;
    if (unit >= w->unit_count || (dx == 0 && dy == 0) ||
        dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return false;
    u = &w->units[unit];
    nx = (int16_t)(u->x + dx);
    ny = (int16_t)(u->y + dy);
    if (!world_wrap(w, &nx, &ny) || world_blocks(w, nx, ny) ||
        world_unit_at(w, nx, ny) != NO_UNIT)
        return false;
    cost = world_step_cost(w, nx, ny, dx != 0 && dy != 0);
    if (u->ap < cost)
        return false;
    u->ap = (uint8_t)(u->ap - cost);
    u->x = (uint8_t)nx;
    u->y = (uint8_t)ny;
    return true;
}

void world_new_turn(World *w)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        w->units[i].ap = w->units[i].ap_max;
}

char world_char(const World *w, int16_t x, int16_t y)
{
    static const char FEATURE_CHARS[FE_COUNT] = {
        0, '#', 'D', 'd', 'B', 'S', 'K', 'C', 'T', 'h', 'M', 'X', 't'};
    static const char FLOOR_CHARS[FL_COUNT] = {'.', ',', '"', ':'};
    uint8_t u, f;
    if (!world_wrap(w, &x, &y))
        return ' ';
    u = world_unit_at(w, x, y);
    if (u != NO_UNIT)
        return w->units[u].kind == CR_WIZARD ? '@' : 'g';
    f = w->feature[y][x];
    if (f != FE_NONE)
        return FEATURE_CHARS[f];
    if (w->decor[y][x] == DE_RUG)
        return '=';
    return FLOOR_CHARS[w->floor[y][x]];
}
