#include "world.h"

#include <string.h>

#include "gen/data.h"
#include "gen/tiles.h"

/* Movement blocking per feature (GDD 3.3): chairs, candle stands and the
 * cauldron can be walked onto; furniture with a body blocks. */
static const bool FEATURE_BLOCKS[FE_COUNT] = {
    [FE_NONE] = false, [FE_WALL] = true, [FE_DOOR_CLOSED] = true,
    [FE_DOOR_OPEN] = false, [FE_BED] = true, [FE_BOOKSHELF] = true,
    [FE_CANDLE] = false, [FE_CAULDRON] = false, [FE_TABLE] = true,
    [FE_CHAIR] = false, [FE_DRAWERS] = true, [FE_CHEST] = true,
    [FE_TREE] = true, [FE_ROCK] = true,
};

/* Tall features that block ground sight (GDD 3.4). A table rather than an
 * ||-chain: ez80 clang turns such chains into an i14 bit test it cannot
 * legalize (AGON-QUIRKS T7). */
static const bool FEATURE_SIGHT[FE_COUNT] = {
    [FE_WALL] = true, [FE_DOOR_CLOSED] = true, [FE_BOOKSHELF] = true,
    [FE_TREE] = true, [FE_ROCK] = true,
};

/* Provisional stats until data/creatures.csv arrives in M2 (GDD 4.1, 5.3):
 * ap, stamina, constitution, combat, defence, mana, flags. */
typedef struct {
    uint8_t ap, sta, con, com, def, mana, flags;
} KindStats;
static const KindStats KIND[] = {
    [CR_WIZARD] = {40, 60, 30, 10, 12, 80, 0},
    [CR_GOBLIN] = {30, 45, 32, 9, 9, 0, 0},
};

static void init_unit(Unit *u, uint8_t x, uint8_t y, uint8_t kind, uint8_t owner)
{
    const KindStats *k = &KIND[kind];
    u->x = x;
    u->y = y;
    u->kind = kind;
    u->owner = owner;
    u->flags = k->flags;
    u->ap = u->ap_max = k->ap;
    u->sta = u->sta_max = k->sta;
    u->con = u->con_max = k->con;
    u->com = k->com;
    u->def = k->def;
    u->mana = u->mana_max = k->mana;
}

bool world_load_bin(World *w, const uint8_t *b, uint16_t len)
{
    uint8_t mw, mh, x, y, i, n;
    uint16_t cells, pos, k;

    if (len < MAPBIN_HEADER || memcmp(b, "LOCM", 4) != 0 || b[4] != MAPBIN_VERSION)
        return false;
    if ((uint16_t)(b[5] | (b[6] << 8)) != TILE_COUNT)   /* stale map vs tile bank */
        return false;
    mw = b[7];
    mh = b[8];
    if (mw == 0 || mh == 0 || mw > MAP_MAX_W || mh > MAP_MAX_H)
        return false;
    cells = (uint16_t)((uint16_t)mw * mh);
    pos = (uint16_t)(MAPBIN_HEADER + 3u * cells);
    if (len < pos + 1u)
        return false;
    for (k = 0; k < cells; k++) {
        if (b[MAPBIN_HEADER + k] >= FL_COUNT ||
            b[MAPBIN_HEADER + cells + k] >= FE_COUNT ||
            b[MAPBIN_HEADER + 2u * cells + k] > DE_PENTACLE)
            return false;
    }
    n = b[pos++];
    if (n > MAX_UNITS || len < pos + 4u * n + 1u)
        return false;
    for (i = 0; i < n; i++) {
        const uint8_t *u = &b[pos + 4u * i];
        if (u[0] >= mw || u[1] >= mh || u[2] >= CR_COUNT || u[3] >= OWN_COUNT)
            return false;
    }
    {
        uint16_t opos = (uint16_t)(pos + 4u * n);
        uint8_t no = b[opos];
        if (no > MAX_OBJECTS || len < opos + 1u + 3u * no)
            return false;
        for (i = 0; i < no; i++) {
            const uint8_t *o = &b[opos + 1u + 3u * i];
            if (o[0] >= mw || o[1] >= mh || o[2] >= TILE_COUNT)
                return false;
        }
    }

    /* Validated: build the world. */
    {
        uint8_t gen = (uint8_t)(w->generation + 1);
        memset(w, 0, sizeof *w);
        w->generation = gen;
    }
    w->w = mw;
    w->h = mh;
    w->wrap = b[9] ? 1 : 0;
    for (y = 0; y < mh; y++)
        for (x = 0; x < mw; x++) {
            k = (uint16_t)((uint16_t)y * mw + x);
            w->floor[y][x] = b[MAPBIN_HEADER + k];
            w->feature[y][x] = b[MAPBIN_HEADER + cells + k];
            w->decor[y][x] = b[MAPBIN_HEADER + 2u * cells + k];
        }
    for (i = 0; i < n; i++) {
        const uint8_t *u = &b[pos + 4u * i];
        init_unit(&w->units[i], u[0], u[1], u[2], u[3]);
    }
    w->unit_count = n;
    pos = (uint16_t)(pos + 4u * n);
    w->object_count = b[pos++];
    for (i = 0; i < w->object_count; i++) {
        w->objects[i].x = b[pos++];
        w->objects[i].y = b[pos++];
        w->objects[i].tile = b[pos++];
    }
    return true;
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

bool world_blocks_sight(const World *w, int16_t x, int16_t y)
{
    uint8_t fe;
    if (!world_wrap(w, &x, &y))
        return false;
    fe = w->feature[y][x];
    return FLOOR_SIGHT[w->floor[y][x]] || FEATURE_SIGHT[fe];
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
    uint8_t c = FLOOR_AP[world_floor(w, x, y)];
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
    {   /* stamina: half the AP, rounded up (GDD 5.3) */
        uint8_t st = (uint8_t)((cost + 1) / 2);
        u->sta = u->sta > st ? (uint8_t)(u->sta - st) : 0;
    }
    u->x = (uint8_t)nx;
    u->y = (uint8_t)ny;
    return true;
}

void world_new_turn(World *w)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++) {
        Unit *u = &w->units[i];
        uint16_t sta = (uint16_t)(u->sta + u->sta_max / 4);
        u->ap = u->ap_max;
        u->sta = (uint8_t)(sta > u->sta_max ? u->sta_max : sta);
    }
}

char world_char(const World *w, int16_t x, int16_t y)
{
    static const char FEATURE_CHARS[FE_COUNT] = {
        [FE_NONE] = ' ', [FE_WALL] = '#', [FE_DOOR_CLOSED] = 'D', [FE_DOOR_OPEN] = 'd',
        [FE_BED] = 'B', [FE_BOOKSHELF] = 'S', [FE_CANDLE] = 'K', [FE_CAULDRON] = 'C',
        [FE_TABLE] = 'T', [FE_CHAIR] = 'h', [FE_DRAWERS] = 'M', [FE_CHEST] = 'X',
        [FE_TREE] = 't', [FE_ROCK] = 'R'};
    static const char FLOOR_CHARS[FL_COUNT] = {
        [FL_STONE] = '.', [FL_WOOD] = ',', [FL_GRASS] = '"', [FL_PATH] = ':',
        [FL_TALL_GRASS] = ';', [FL_FOREST] = 'f', [FL_MAGIC_WOOD] = 'm',
        [FL_SHADOW_WOOD] = 'n', [FL_SWAMP] = 'u', [FL_WATER] = '~', [FL_RUBBLE] = 'r'};
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
