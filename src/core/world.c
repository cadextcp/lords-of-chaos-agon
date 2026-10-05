#include "world.h"

#include <string.h>

#include "area.h"
#include "effect.h"
#include "events.h"
#include "gen/data.h"
#include "items.h"
#include "ride.h"
#include "gen/tiles.h"

/* Movement blocking per feature (GDD 3.3): chairs, candle stands and the
 * cauldron can be walked onto; furniture with a body blocks. */
static const bool FEATURE_BLOCKS[FE_COUNT] = {
    [FE_NONE] = false, [FE_WALL] = true, [FE_DOOR_CLOSED] = true,
    [FE_DOOR_OPEN] = false, [FE_BED] = true, [FE_BOOKSHELF] = true,
    [FE_CANDLE] = false, [FE_CAULDRON] = false, [FE_TABLE] = true,
    [FE_CHAIR] = false, [FE_DRAWERS] = true, [FE_CHEST] = true,
    [FE_TREE] = true, [FE_ROCK] = true, [FE_DOOR_LOCKED] = true,
    [FE_CHEST_FREE] = true,
};

/* Tall features that block ground sight (GDD 3.4). A table rather than an
 * ||-chain: ez80 clang turns such chains into an i14 bit test it cannot
 * legalize (AGON-QUIRKS T7). */
static const bool FEATURE_SIGHT[FE_COUNT] = {
    [FE_WALL] = true, [FE_DOOR_CLOSED] = true, [FE_BOOKSHELF] = true,
    [FE_TREE] = true, [FE_ROCK] = true, [FE_DOOR_LOCKED] = true,
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
    u->rider_kind = 0xFF;
    u->post_x = u->post_y = 0xFF;
    u->grudge = 0;
    u->herd_dir = 0;
    u->travel = 0;
    u->group = 0;
    u->reacted = false;
    u->alarm = 0;
    u->alarm_charge = 0;
    u->alarm_x = u->alarm_y = 0;
    u->alarm_owner = OWN_NEUTRAL;
}

bool world_load_bin(World *w, const uint8_t *b, uint16_t len)
{
    uint8_t mw, mh, x, y, i, n;
    uint16_t cells, pos, k;

    if (len < MAPBIN_HEADER || memcmp(b, "LOCM", 4) != 0 ||
        (b[4] != 2 && b[4] != MAPBIN_VERSION))
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
    w->disturb_n = 0;                    /* a new map: no old trouble */
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
        w->units[i].id = i;
    }
    w->unit_count = n;
    w->next_id = n;
    pos = (uint16_t)(pos + 4u * n);
    w->object_count = b[pos++];
    for (i = 0; i < w->object_count; i++) {
        w->objects[i].x = b[pos++];
        w->objects[i].y = b[pos++];
        w->objects[i].tile = (uint16_t)(b[pos] | (b[pos + 1] << 8));
        pos += 2;
    }
    /* v4 maps: undead guards stand their ground (M4h Wächter) */
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == OWN_NEUTRAL &&
            (w->units[i].flags & UF_UNDEAD))
            w->units[i].post_x = w->units[i].x,
            w->units[i].post_y = w->units[i].y;
    w->portal_x = w->portal_y = -1;      /* v2 maps carry no portal */
    w->portal_rmin = w->portal_rmax = 0;
    if (b[4] >= 3 && pos + 4 <= len) {   /* v3: portal x y rmin rmax */
        if (b[pos] != 0xFF) {            /* 0xFF = no portal (v4 filler) */
            w->portal_x = b[pos];
            w->portal_y = b[pos + 1];
            w->portal_rmin = b[pos + 2];
            w->portal_rmax = b[pos + 3];
        }
        pos += 4;
    }
    memset(w->roof, 0, sizeof w->roof);
    if (b[4] >= 4) {                     /* v4: one roof bit per field */
        uint16_t k2;
        if (len < pos + cells)
            return false;
        for (k2 = 0; k2 < cells; k2++)
            if (b[pos + k2])
                w->roof[k2 >> 3] |= (uint8_t)(0x80u >> (k2 & 7));
    }
    return true;
}

bool world_has_roof(const World *w, int16_t x, int16_t y)
{
    uint16_t cell;
    if (!world_wrap(w, &x, &y))
        return false;
    cell = (uint16_t)(y * w->w + x);
    return (w->roof[cell >> 3] & (uint8_t)(0x80u >> (cell & 7))) != 0;
}

void world_map_changed(World *w)
{
    w->generation++;
}

bool world_wrap(const World *w, int16_t *x, int16_t *y)
{
    if (w->wrap) {
        /* Hot path: callers are at most one map size off. The eZ80 and
         * the 68000 (Mega Drive port) have no fast 32-bit modulo, so only
         * far coordinates pay for the division. */
        if (*x < 0)
            *x = (int16_t)(*x + w->w);
        else if (*x >= w->w)
            *x = (int16_t)(*x - w->w);
        if (*y < 0)
            *y = (int16_t)(*y + w->h);
        else if (*y >= w->h)
            *y = (int16_t)(*y - w->h);
        if (*x < 0 || *x >= w->w)
            *x = (int16_t)(((*x % w->w) + w->w) % w->w);
        if (*y < 0 || *y >= w->h)
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
    static const bool WALL_LINE[FE_COUNT] = {   /* table, not ||: AGON-QUIRKS T7 */
        [FE_WALL] = true, [FE_DOOR_CLOSED] = true, [FE_DOOR_OPEN] = true,
        [FE_DOOR_LOCKED] = true,
    };
    return f < FE_COUNT && WALL_LINE[f];
}


/* Roofs count here, and since D44 they count in world_sight_byte() too.
 * Until then the two disagreed: this one saw the roof, the bitmap that
 * sight.c actually walks did not. */
bool world_blocks_sight(const World *w, int16_t x, int16_t y)
{
    if (!world_wrap(w, &x, &y))
        return false;
    return FLOOR_SIGHT[w->floor[y][x]] || FEATURE_SIGHT[w->feature[y][x]] ||
           world_has_roof(w, x, y);
}

bool world_feature_blocks_sight(const World *w, uint8_t x, uint8_t y)
{
    return FEATURE_SIGHT[w->feature[y][x]];
}

/* Eight sight-blocking bits of one row, MSB first. `with_roof` adds the
 * roof to what blocks (D44): a viewer out in the open cannot see under a
 * roof, a viewer standing under one is given the roof-free variant so it
 * does not blind itself. world_blocks_sight_at() had counted the roof
 * since M2d, but the bitmap that replaced it (ADR 0009, step 3) dropped
 * it and left that function unused. */
uint8_t world_sight_byte(const World *w, uint8_t y, uint8_t x, bool with_roof)
{
    const uint8_t *fl = w->floor[y], *fe = w->feature[y];
    uint8_t bits = 0, i;
    for (i = 0; i < 8; i++) {
        uint8_t xi = (uint8_t)(x + i);
        bits <<= 1;
        if (xi < w->w && (FLOOR_SIGHT[fl[xi]] || FEATURE_SIGHT[fe[xi]] ||
                          (with_roof && world_has_roof(w, xi, y))))
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

uint8_t world_blocking_unit_at(const World *w, int16_t x, int16_t y,
                               UnitLayer layer, uint8_t owner)
{
    uint8_t i;
    bool air = layer == UL_AIR;
    if (!world_wrap(w, &x, &y))
        return NO_UNIT;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].x == x && w->units[i].y == y &&
            ((w->units[i].flags & UF_FLYING) != 0) == air &&
            (owner == OWN_NEUTRAL || w->units[i].owner != owner))
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

void world_delta(const World *w, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 int16_t *dx, int16_t *dy)
{
    *dx = (int16_t)(x1 - x0);
    *dy = (int16_t)(y1 - y0);
    if (w->wrap) {
        if (*dx > w->w / 2) *dx = (int16_t)(*dx - w->w);
        if (*dx < -w->w / 2) *dx = (int16_t)(*dx + w->w);
        if (*dy > w->h / 2) *dy = (int16_t)(*dy - w->h);
        if (*dy < -w->h / 2) *dy = (int16_t)(*dy + w->h);
    }
}

uint8_t world_distance(const World *w, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx, dy;
    world_delta(w, x0, y0, x1, y1, &dx, &dy);
    if (dx < 0) dx = (int16_t)-dx;
    if (dy < 0) dy = (int16_t)-dy;
    return (uint8_t)(dx > dy ? dx : dy);
}

uint8_t world_find_unit(const World *w, uint8_t id)
{
    uint8_t i;
    if (id == NO_UNIT)
        return NO_UNIT;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].id == id)
            return i;
    return NO_UNIT;
}

/* Add a freshly initialised unit (summons); returns its index. The id
 * counter wraps after 255 spawns, so ids still in use are skipped. */
uint8_t world_spawn_unit(World *w, uint8_t owner, uint8_t kind, uint8_t x, uint8_t y)
{
    Unit *u;
    if (w->unit_count >= MAX_UNITS || kind >= CR_COUNT)
        return NO_UNIT;
    while (w->next_id == NO_UNIT || world_find_unit(w, w->next_id) != NO_UNIT)
        w->next_id++;
    u = &w->units[w->unit_count];
    init_unit(u, x, y, kind, owner);
    u->id = w->next_id++;
    u->done = false;
    return w->unit_count++;
}

void world_provoke(World *w, uint8_t unit, uint8_t attacker_owner)
{
    if (unit < w->unit_count && attacker_owner < OWN_NEUTRAL &&
        w->units[unit].owner == OWN_NEUTRAL)
        w->units[unit].grudge |= (uint8_t)(1u << attacker_owner);
}

void world_disturb(World *w, int16_t x, int16_t y, uint8_t owner)
{
    uint8_t i;
    if (!world_wrap(w, &x, &y))
        return;
    for (i = 0; i < w->disturb_n; i++)    /* one entry per field is enough */
        if (w->disturb[i][0] == x && w->disturb[i][1] == y)
            return;
    if (w->disturb_n >= WORLD_DISTURB)
        return;
    w->disturb[w->disturb_n][0] = (uint8_t)x;
    w->disturb[w->disturb_n][1] = (uint8_t)y;
    w->disturb[w->disturb_n][2] = owner;
    w->disturb_n++;
}

void world_remove_unit(World *w, uint8_t unit)
{
    if (unit >= w->unit_count)
        return;
    w->units[unit] = w->units[w->unit_count - 1];   /* swap with the last */
    w->unit_count--;
}

/* The dead drop everything they carried on their field (D21) - flyers
 * onto the ground below. A full object list swallows the rest. */
static void drop_carried(World *w, const Unit *u)
{
    uint8_t i;
    for (i = 0; i < u->item_count && w->object_count < MAX_OBJECTS; i++) {
        Object *o = &w->objects[w->object_count++];
        o->x = u->x;
        o->y = u->y;
        o->tile = OBJECTS[u->items[i]].tile;
    }
}

void world_kill_unit(World *w, uint8_t victim, uint8_t killer_kind,
                     uint8_t killer_owner, bool melee)
{
    if (victim >= w->unit_count)
        return;
    events_push(EV_DEATH, w->units[victim].x, w->units[victim].y,
                w->units[victim].kind, w->units[victim].owner, 0, 0);
    drop_carried(w, &w->units[victim]);
    if (killer_owner < OWN_NEUTRAL && w->kill_count < MAX_KILLS) {
        Kill *k = &w->kills[w->kill_count++];
        k->victim_kind = w->units[victim].kind;
        k->victim_owner = w->units[victim].owner;
        k->killer_kind = killer_kind;
        k->killer_owner = killer_owner;
        k->melee = melee;
    }
    world_remove_unit(w, victim);
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
        if (world_blocking_unit_at(w, nx, ny, UL_AIR, u->owner) != NO_UNIT)
            return false;
        cost = world_air_step_cost(dx != 0 && dy != 0);
    } else {
        /* D43: ghost and spectre drift through walls and furniture. Other
         * units on the field still stop them - a body is a body. */
        if ((world_blocks(w, nx, ny) &&
             !(CREATURES[u->kind].flags & CF_PHASE)) ||
            world_blocking_unit_at(w, nx, ny, UL_GROUND, u->owner) != NO_UNIT)
            return false;
        if (area_blocks_kind(w, nx, ny))
            return false;                  /* stuck in blob or vine (M4d) */
        cost = world_unit_step_cost(w, unit, nx, ny, dx != 0 && dy != 0);
    }
    if (u->ap < cost)
        return false;
    world_spend(w, unit, cost);
    u->x = (uint8_t)nx;
    u->y = (uint8_t)ny;
    if (!(u->flags & UF_FLYING))
        world_engage(w, unit);             /* arriving next to an enemy binds both */
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
    if (u->ap_fly == 0 && !effect_active(u, EFF_FLYING))
        return false;                         /* no wings, no flying potion */
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
    if (world_has_roof(w, u->x, u->y))
        return false;                         /* landing under a roof (GDD 3.2) */
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
        /* airborne on a flying potion (no wings): the ground budget */
        uint8_t full = (u->flags & UF_FLYING) && u->ap_fly ? u->ap_fly : u->ap_max;
        u->reacted = false;                 /* new round, new reaction (D29) */
        if (u->flags & UF_WOUNDED)         /* bleeds until death (PM 17) */
            u->con = u->con > 0 ? (uint8_t)(u->con - 1) : 0;
        uint16_t sta;
        if (effect_active(u, EFF_SPEED))
            sta = (uint16_t)(u->sta + 3 * (u->sta_max / 4));
        else
            sta = (uint16_t)(u->sta + u->sta_max / 4);
        u->ap = u->sta < u->sta_max / 4 ? (uint8_t)(full / 2) : full;
        if (u->con < u->con_max / 2)      /* badly hurt (GDD 4.1) */
            u->ap = (uint8_t)(u->ap / 2);
        if (effect_active(u, EFF_SPEED))  /* Speed (potion): AP x2 */
            u->ap = (uint8_t)(u->ap * 2);
        u->sta = (uint8_t)(sta > u->sta_max ? u->sta_max : sta);
        if (FLOOR_DROWN[w->floor[u->y][u->x]] &&   /* treading water (C5) */
            !(u->native & NATIVE_WATER) && !(u->flags & UF_FLYING)) {
            uint8_t cost = (uint8_t)(u->sta_max / 2);   /* net -25 % a round */
            uint8_t hurt = (uint8_t)(u->con_max / 5 > 1 ? u->con_max / 5 : 1);
            u->sta = u->sta > cost ? (uint8_t)(u->sta - cost) : 0;
            if (u->sta == 0)               /* spent: drowning, like bleeding */
                u->con = u->con > hurt ? (uint8_t)(u->con - hurt) : 0;
        }
        effect_tick(u);                   /* durations run down (GDD 2.1) */
        if ((u->flags & UF_FLYING) && u->ap_fly == 0 &&
            !effect_active(u, EFF_FLYING)) {  /* the potion wore off */
            if (world_unit_at(w, u->x, u->y, UL_GROUND) == NO_UNIT &&
                !FLOOR_DROWN[w->floor[u->y][u->x]])
                u->flags &= (uint8_t)~UF_FLYING;  /* sinks to the ground */
            else
                effect_grant(u, EFF_FLYING, 1, 1);  /* hovers on, no room */
        }
        if (u->mana_max) {
            uint8_t mana = (uint8_t)(u->mana + u->mana_max / 25);
            u->mana = mana > u->mana_max || mana < u->mana ? u->mana_max : mana;
        }
    }
    for (i = w->unit_count; i-- > 0;)      /* bleeders that died */
        if (w->units[i].con == 0) {
            events_push(EV_DEATH, w->units[i].x, w->units[i].y,
                        w->units[i].kind, w->units[i].owner, 1, 0);
            drop_carried(w, &w->units[i]);
            world_remove_unit(w, i);
        }
}

bool world_engaged(const World *w, uint8_t unit)
{
    const Unit *u;
    int8_t dx, dy;
    if (ride_may_attack_from(w, unit))
        return false;                    /* riders attack from anywhere (D21) */
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->flags & UF_FLYING)
        return false;                      /* flyers are never bound */
    if (!(u->flags & UF_ENGAGED))
        return false;                      /* free again since its last phase */
    for (dx = -1; dx <= 1; dx++)
        for (dy = -1; dy <= 1; dy++) {
            uint8_t o = world_unit_at(w, (int16_t)(u->x + dx),
                                      (int16_t)(u->y + dy), UL_GROUND);
            if (o != NO_UNIT && w->units[o].owner != u->owner)
                return true;               /* enemy at the sleeve (GDD 6) */
        }
    return false;
}

bool world_enemy_adjacent(const World *w, uint8_t unit)
{
    const Unit *u;
    int8_t dx, dy;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->flags & UF_FLYING)
        return false;
    for (dx = -1; dx <= 1; dx++)
        for (dy = -1; dy <= 1; dy++) {
            uint8_t o = world_unit_at(w, (int16_t)(u->x + dx),
                                      (int16_t)(u->y + dy), UL_GROUND);
            if (o != NO_UNIT && w->units[o].owner != u->owner)
                return true;
        }
    return false;
}

void world_engage(World *w, uint8_t unit)
{
    Unit *u;
    int8_t dx, dy;
    if (unit >= w->unit_count)
        return;
    u = &w->units[unit];
    if (u->flags & UF_FLYING)
        return;
    for (dx = -1; dx <= 1; dx++)
        for (dy = -1; dy <= 1; dy++) {
            uint8_t o = world_unit_at(w, (int16_t)(u->x + dx),
                                      (int16_t)(u->y + dy), UL_GROUND);
            if (o != NO_UNIT && w->units[o].owner != u->owner) {
                u->flags |= UF_ENGAGED;
                w->units[o].flags |= UF_ENGAGED;
            }
        }
}

void world_release(World *w, uint8_t owner)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner)
            w->units[i].flags &= (uint8_t)~UF_ENGAGED;
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
        if (world_blocking_unit_at(w, nx, ny, UL_AIR, u->owner) != NO_UNIT)
            return BUMP_UNIT;
        cost = world_air_step_cost(dx != 0 && dy != 0);
    } else {
        if (w->feature[ny][nx] == FE_DOOR_CLOSED)
            return BUMP_DOOR;
        if (world_blocks(w, nx, ny))
            return BUMP_TERRAIN;
        if (world_blocking_unit_at(w, nx, ny, UL_GROUND, u->owner) != NO_UNIT)
            return BUMP_UNIT;
        if (area_blocks_kind(w, nx, ny))
            return BUMP_HELD;
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

bool world_has_key(const World *w, uint8_t unit)
{
    const Unit *u;
    uint8_t i;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    for (i = 0; i < u->item_count; i++)
        if (u->items[i] == OBJ_CHEST_KEY)
            return true;
    return false;
}

/* Shared by close / lock / unlock: unit has hands and the AP, the field
 * holds `from`; it becomes `to`. */
static bool door_change(World *w, uint8_t unit, int16_t x, int16_t y,
                        uint8_t from, uint8_t to, uint8_t action)
{
    if (unit >= w->unit_count)
        return false;
    if (!world_wrap(w, &x, &y) || w->feature[y][x] != from)
        return false;
    if (!(CREATURES[w->units[unit].kind].flags & CF_USE))
        return false;
    if (w->units[unit].ap < ACTIONS[action].ap)
        return false;
    world_spend(w, unit, ACTIONS[action].ap);
    w->feature[y][x] = to;
    world_map_changed(w);
    return true;
}

bool world_close_door(World *w, uint8_t unit, int16_t x, int16_t y)
{
    int16_t cx = x, cy = y;
    if (!world_wrap(w, &cx, &cy) ||
        world_unit_at(w, cx, cy, UL_GROUND) != NO_UNIT ||
        world_unit_at(w, cx, cy, UL_AIR) != NO_UNIT)
        return false;                    /* somebody stands in the doorway */
    return door_change(w, unit, x, y, FE_DOOR_OPEN, FE_DOOR_CLOSED,
                       ACT_OPEN_DOOR);
}

bool world_lock_door(World *w, uint8_t unit, int16_t x, int16_t y)
{
    return world_has_key(w, unit) &&
           door_change(w, unit, x, y, FE_DOOR_CLOSED, FE_DOOR_LOCKED,
                       ACT_UNLOCK);
}

bool world_unlock_door(World *w, uint8_t unit, int16_t x, int16_t y)
{
    return world_has_key(w, unit) &&
           door_change(w, unit, x, y, FE_DOOR_LOCKED, FE_DOOR_CLOSED,
                       ACT_UNLOCK);
}

char world_char(const World *w, int16_t x, int16_t y)
{
    static const char FEATURE_CHARS[FE_COUNT] = {
        [FE_NONE] = ' ', [FE_WALL] = '#', [FE_DOOR_CLOSED] = 'D', [FE_DOOR_OPEN] = 'd',
        [FE_BED] = 'B', [FE_BOOKSHELF] = 'S', [FE_CANDLE] = 'K', [FE_CAULDRON] = 'C',
        [FE_TABLE] = 'T', [FE_CHAIR] = 'h', [FE_DRAWERS] = 'M', [FE_CHEST] = 'X',
        [FE_TREE] = 't', [FE_ROCK] = 'R', [FE_DOOR_LOCKED] = 'L',
        [FE_CHEST_FREE] = 'x'};
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
