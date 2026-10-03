#include "world.h"

#include <string.h>

#include "gen/data.h"
#include "items.h"
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



static void init_unit(Unit *u, uint8_t x, uint8_t y, uint8_t kind, uint8_t owner)
{
    const CreatureDef *k = &CREATURES[kind];
    u->x = x;
    u->y = y;
    u->kind = kind;
    u->owner = owner;
    u->flags = (uint8_t)(((k->flags & CF_UNDEAD) ? UF_UNDEAD : 0) |
                         ((k->flags & CF_MOUNT) ? UF_MOUNT : 0));
    u->native = k->native;
    u->ap = u->ap_max = k->ap;
    u->ap_fly = k->ap_fly;
    u->sta = u->sta_max = k->stamina;
    u->con = u->con_max = k->con;
    u->com = k->combat;
    u->def = k->defence;
    u->mr = k->magic_res;
    u->mana = u->mana_max = k->mana;
    u->item_count = 0;
    u->in_use = NO_ITEM;
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
        if (no > MAX_OBJECTS || len < opos + 1u + 4u * no)
            return false;
        for (i = 0; i < no; i++) {
            const uint8_t *o = &b[opos + 1u + 4u * i];
            if (o[0] >= mw || o[1] >= mh || (uint16_t)(o[2] | (o[3] << 8)) >= TILE_COUNT)
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
        w->objects[i].tile = (uint16_t)(b[pos] | (b[pos + 1] << 8));
        pos += 2;
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

/* Blocking at already-normalised in-map coordinates: no wrapping, no
 * bounds check - the fast path for sight rays (M2d), which normalise
 * incrementally instead of paying a division per field (AGON-QUIRKS T2). */
bool world_blocks_sight_at(const World *w, uint8_t x, uint8_t y)
{
    return FLOOR_SIGHT[w->floor[y][x]] || FEATURE_SIGHT[w->feature[y][x]];
}

bool world_blocks_sight(const World *w, int16_t x, int16_t y)
{
    if (!world_wrap(w, &x, &y))
        return false;
    return world_blocks_sight_at(w, (uint8_t)x, (uint8_t)y);
}

/* Eight blocking flags of one row packed into a byte (x -> MSB); fields
 * beyond the map read as clear. Lets callers build bitmaps a byte at a
 * time instead of paying per-field indexing (sight.c, M2d). */
uint8_t world_sight_byte(const World *w, uint8_t y, uint8_t x)
{
    const uint8_t *fl = w->floor[y], *fe = w->feature[y];
    uint8_t bits = 0, i;
    for (i = 0; i < 8; i++) {
        uint8_t xi = (uint8_t)(x + i);
        bits <<= 1;
        if (xi < w->w && (FLOOR_SIGHT[fl[xi]] || FEATURE_SIGHT[fe[xi]]))
            bits |= 1;
    }
    return bits;
}

bool world_blocks(const World *w, int16_t x, int16_t y)
{
    if (!world_wrap(w, &x, &y))
        return true;
    return FEATURE_BLOCKS[w->feature[y][x]];
}

uint8_t world_unit_at(const World *w, int16_t x, int16_t y, UnitLayer layer)
{
    uint8_t i;
    bool air = layer == UL_AIR;
    if (!world_wrap(w, &x, &y))
        return NO_UNIT;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].x == x && w->units[i].y == y &&
            ((w->units[i].flags & UF_FLYING) != 0) == air)
            return i;
    return NO_UNIT;
}

uint8_t world_step_cost(const World *w, int16_t x, int16_t y, bool diagonal)
{
    uint8_t c = FLOOR_AP[world_floor(w, x, y)];
    return diagonal ? (uint8_t)((c * 3 + 1) / 2) : c;
}

uint8_t world_unit_step_cost(const World *w, uint8_t unit, int16_t x, int16_t y,
                             bool diagonal)
{
    uint8_t c;
    if (unit < w->unit_count && (FLOOR_NATIVE[world_floor(w, x, y)] & w->units[unit].native))
        c = FLOOR_AP[FL_STONE];   /* at home in this terrain: plain floor cost */
    else
        c = FLOOR_AP[world_floor(w, x, y)];
    return diagonal ? (uint8_t)((c * 3 + 1) / 2) : c;
}

uint8_t world_air_step_cost(bool diagonal)
{
    return diagonal ? AIR_AP_DIAG : AIR_AP_ORTH;
}

void world_spend(World *w, uint8_t unit, uint8_t ap)
{
    uint8_t st = (uint8_t)((ap + 1) / 2);   /* half the AP, rounded up */
    Unit *u = &w->units[unit];
    u->ap = (uint8_t)(u->ap - ap);
    u->sta = u->sta > st ? (uint8_t)(u->sta - st) : 0;
}

/* Add a freshly initialised unit (summons); returns its index. */
uint8_t world_spawn_unit(World *w, uint8_t owner, uint8_t kind, uint8_t x, uint8_t y)
{
    if (w->unit_count >= MAX_UNITS || kind >= CR_COUNT)
        return NO_UNIT;
    init_unit(&w->units[w->unit_count], x, y, kind, owner);
    return w->unit_count++;
}

void world_remove_unit(World *w, uint8_t unit)
{
    if (unit >= w->unit_count)
        return;
    w->units[unit] = w->units[w->unit_count - 1];   /* swap with the last */
    w->unit_count--;
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
    if (!world_wrap(w, &nx, &ny))
        return false;
    if (u->flags & UF_FLYING) {
        /* Flyers cross anything, they only respect the air layer. */
        if (world_unit_at(w, nx, ny, UL_AIR) != NO_UNIT)
            return false;
        cost = world_air_step_cost(dx != 0 && dy != 0);
    } else {
        if (world_blocks(w, nx, ny) ||
            world_unit_at(w, nx, ny, UL_GROUND) != NO_UNIT)
            return false;
        if (world_engaged(w, unit))
            return false;                  /* bound, only the attack remains */
        cost = world_unit_step_cost(w, unit, nx, ny, dx != 0 && dy != 0);
    }
    if (u->ap < cost)
        return false;
    world_spend(w, unit, cost);
    u->x = (uint8_t)nx;
    u->y = (uint8_t)ny;
    return true;
}

bool world_take_off(World *w, uint8_t unit)
{
    Unit *u;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->flags & UF_FLYING)
        return false;
    if (u->ap_fly == 0)                       /* creature cannot fly */
        return false;
    if (world_unit_at(w, u->x, u->y, UL_AIR) != NO_UNIT)
        return false;                         /* air slot taken */
    if (u->ap < ACTIONS[ACT_TAKE_OFF].ap)
        return false;
    world_spend(w, unit, ACTIONS[ACT_TAKE_OFF].ap);
    u->flags |= UF_FLYING;
    return true;
}

bool world_land(World *w, uint8_t unit)
{
    Unit *u;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (!(u->flags & UF_FLYING))
        return false;
    if (world_unit_at(w, u->x, u->y, UL_GROUND) != NO_UNIT)
        return false;                         /* no free ground slot */
    if (FLOOR_DROWN[w->floor[u->y][u->x]])
        return false;                         /* drowning floor (own rule) */
    if (u->ap < ACTIONS[ACT_LAND].ap)
        return false;
    world_spend(w, unit, ACTIONS[ACT_LAND].ap);
    u->flags &= (uint8_t)~UF_FLYING;
    return true;
}

/* Round end (GDD 2.1.4): refill AP - the layer budget while flying
 * (ap_fly) -, recover 25 % stamina (GDD 5.3), regenerate 4 % mana.
 * Exhausted creatures (stamina under 25 % of the maximum) get only half
 * AP next round (PM 12) - tested before the recovery, so one quiet
 * round cures the exhaustion. */
void world_new_turn(World *w)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++) {
        Unit *u = &w->units[i];
        uint8_t full = (u->flags & UF_FLYING) ? u->ap_fly : u->ap_max;
        if (u->flags & UF_WOUNDED)         /* bleeds until death (PM 17) */
            u->con = u->con > 0 ? (uint8_t)(u->con - 1) : 0;
        uint16_t sta = (uint16_t)(u->sta + u->sta_max / 4);
        u->ap = u->sta < u->sta_max / 4 ? (uint8_t)(full / 2) : full;
        u->sta = (uint8_t)(sta > u->sta_max ? u->sta_max : sta);
        if (u->mana_max) {
            uint8_t mana = (uint8_t)(u->mana + u->mana_max / 25);
            u->mana = mana > u->mana_max || mana < u->mana ? u->mana_max : mana;
        }
    }
    for (i = w->unit_count; i-- > 0;)      /* bleeders that died */
        if (w->units[i].con == 0)
            world_remove_unit(w, i);
}

bool world_engaged(const World *w, uint8_t unit)
{
    const Unit *u;
    int8_t dx, dy;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->flags & UF_FLYING)
        return false;                      /* flyers are never bound */
    for (dx = -1; dx <= 1; dx++)
        for (dy = -1; dy <= 1; dy++) {
            uint8_t o = world_unit_at(w, (int16_t)(u->x + dx),
                                      (int16_t)(u->y + dy), UL_GROUND);
            if (o != NO_UNIT && w->units[o].owner != u->owner)
                return true;               /* enemy at the sleeve (GDD 6) */
        }
    return false;
}

BumpKind world_bump_kind(const World *w, uint8_t unit, int8_t dx, int8_t dy)
{
    int16_t nx, ny;
    uint8_t cost;
    const Unit *u;
    if (unit >= w->unit_count || (dx == 0 && dy == 0) ||
        dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return BUMP_OUTSIDE;
    u = &w->units[unit];
    nx = (int16_t)(u->x + dx);
    ny = (int16_t)(u->y + dy);
    if (!world_wrap(w, &nx, &ny))
        return BUMP_OUTSIDE;
    if (u->flags & UF_FLYING) {
        if (world_unit_at(w, nx, ny, UL_AIR) != NO_UNIT)
            return BUMP_UNIT;
        cost = world_air_step_cost(dx != 0 && dy != 0);
    } else {
        if (w->feature[ny][nx] == FE_DOOR_CLOSED)
            return BUMP_DOOR;
        if (world_blocks(w, nx, ny))
            return BUMP_TERRAIN;
        if (world_unit_at(w, nx, ny, UL_GROUND) != NO_UNIT)
            return BUMP_UNIT;
        cost = world_unit_step_cost(w, unit, nx, ny, dx != 0 && dy != 0);
    }
    return u->ap >= cost ? BUMP_OK : BUMP_NO_AP;
}

bool world_open_door(World *w, uint8_t unit, int16_t x, int16_t y)
{
    Unit *u;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (!world_wrap(w, &x, &y) || w->feature[y][x] != FE_DOOR_CLOSED)
        return false;
    if (!(CREATURES[u->kind].flags & CF_USE))
        return false;                    /* creature without hands */
    if (u->ap < ACTIONS[ACT_OPEN_DOOR].ap)
        return false;
    world_spend(w, unit, ACTIONS[ACT_OPEN_DOOR].ap);
    w->feature[y][x] = FE_DOOR_OPEN;
    world_map_changed(w);                /* static view layers change */
    return true;
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
    u = world_unit_at(w, x, y, UL_GROUND);
    if (u == NO_UNIT)
        u = world_unit_at(w, x, y, UL_AIR);
    if (u != NO_UNIT)
        return w->units[u].kind == CR_WIZARD ? '@' : 'g';
    f = w->feature[y][x];
    if (f != FE_NONE)
        return FEATURE_CHARS[f];
    if (w->decor[y][x] == DE_RUG)
        return '=';
    return FLOOR_CHARS[w->floor[y][x]];
}
