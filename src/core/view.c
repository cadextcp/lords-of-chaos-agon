#include "view.h"

#include "gen/data.h"
#include "area.h"
#include "ride.h"
#include "sight.h"

#include <string.h>

/* Tile IDs per floor; half floors follow each floor tile in the order
 * n, s, w, e (see tools/build_tiles.py). */
static const uint16_t FLOOR_TILE[FL_COUNT] = {
    [FL_STONE] = T_FLOOR_STONE, [FL_WOOD] = T_FLOOR_WOOD, [FL_GRASS] = T_FLOOR_GRASS,
    [FL_PATH] = T_FLOOR_PATH, [FL_TALL_GRASS] = T_FLOOR_TALLGRASS,
    [FL_FOREST] = T_FLOOR_FOREST, [FL_MAGIC_WOOD] = T_FLOOR_MAGICWOOD,
    [FL_SHADOW_WOOD] = T_FLOOR_SHADOWWOOD, [FL_SWAMP] = T_FLOOR_SWAMP,
    [FL_WATER] = T_FLOOR_WATER_0, [FL_RUBBLE] = T_FLOOR_RUBBLE};
static const uint16_t FLOOR_HALF[FL_COUNT] = {
    [FL_STONE] = T_FLOOR_STONE_HALF_N, [FL_WOOD] = T_FLOOR_WOOD_HALF_N,
    [FL_GRASS] = T_FLOOR_GRASS_HALF_N, [FL_PATH] = T_FLOOR_PATH_HALF_N,
    [FL_TALL_GRASS] = T_FLOOR_TALLGRASS_HALF_N, [FL_FOREST] = T_FLOOR_FOREST_HALF_N,
    [FL_MAGIC_WOOD] = T_FLOOR_MAGICWOOD_HALF_N,
    [FL_SHADOW_WOOD] = T_FLOOR_SHADOWWOOD_HALF_N, [FL_SWAMP] = T_FLOOR_SWAMP_HALF_N,
    [FL_WATER] = T_FLOOR_WATER_0_HALF_N, [FL_RUBBLE] = T_FLOOR_RUBBLE_HALF_N};
enum { HALF_N, HALF_S, HALF_W, HALF_E };

static const uint16_t FEATURE_TILE[FE_COUNT] = {
    [FE_BED] = T_BED, [FE_BOOKSHELF] = T_BOOKSHELF, [FE_CANDLE] = T_CANDLE_0,
    [FE_CAULDRON] = T_CAULDRON, [FE_TABLE] = T_TABLE, [FE_CHAIR] = T_CHAIR,
    [FE_DRAWERS] = T_DRAWERS, [FE_CHEST] = T_CHEST, [FE_TREE] = T_TREE,
    [FE_ROCK] = T_ROCK,
};

/* Animated tiles: frame 0 <-> frame 1 (candles, water). */
static const uint16_t ANIM_A[] = {T_CANDLE_0, T_FLOOR_WATER_0, T_PORTAL_0,
                                  T_AREA_FIRE_0, T_AREA_BLOB_0, T_AREA_VINE_0,
                                  T_AREA_FLOOD_0};
static const uint16_t ANIM_B[] = {T_CANDLE_1, T_FLOOR_WATER_1, T_PORTAL_1,
                                  T_AREA_FIRE_1, T_AREA_BLOB_1, T_AREA_VINE_1,
                                  T_AREA_FLOOD_1};
#define ANIM_N (sizeof ANIM_A / sizeof ANIM_A[0])

/* Which pair a tile belongs to: 1 + index into ANIM_A/ANIM_B, 0 = not
 * animated. anim_swap() used to search the pair list linearly, and
 * view_update() calls it for every layer of every field - roughly 13 600
 * comparisons per compose on a 9x9 window.
 *
 * Kept in step with ANIM_A/ANIM_B by hand; selftest_view() checks every
 * tile id against the lists, so drift cannot pass unnoticed. */
static const uint8_t anim_pair[TILE_COUNT] = {
    [T_CANDLE_0] = 1,     [T_CANDLE_1] = 1,
    [T_FLOOR_WATER_0] = 2, [T_FLOOR_WATER_1] = 2,
    [T_PORTAL_0] = 3,     [T_PORTAL_1] = 3,
    [T_AREA_FIRE_0] = 4,  [T_AREA_FIRE_1] = 4,
    [T_AREA_BLOB_0] = 5,  [T_AREA_BLOB_1] = 5,
    [T_AREA_VINE_0] = 6,  [T_AREA_VINE_1] = 6,
    [T_AREA_FLOOD_0] = 7, [T_AREA_FLOOD_1] = 7,
};

static uint16_t anim_swap(uint16_t id, uint8_t ph)
{
    uint8_t k;
    if (id >= TILE_COUNT)
        return id;
    k = anim_pair[id];
    if (k == 0)
        return id;
    return ph ? ANIM_B[k - 1] : ANIM_A[k - 1];
}

static bool is_animated(uint16_t id)
{
    return id < TILE_COUNT && anim_pair[id] != 0;
}

bool view_anim_table_ok(void)
{
    uint16_t id;
    for (id = 0; id < TILE_COUNT; id++) {
        uint8_t k, want = 0;
        for (k = 0; k < ANIM_N; k++)
            if (id == ANIM_A[k] || id == ANIM_B[k])
                want = (uint8_t)(k + 1);
        if (anim_pair[id] != want)
            return false;
    }
    return true;
}

static FieldLayers fields[VIEW_H][VIEW_W];
static uint8_t dirty[VIEW_H][VIEW_W];
static uint8_t animated[VIEW_H][VIEW_W];   /* field holds an animated tile */
static bool valid;
static int16_t origin_x, origin_y;
static int16_t cursor_x, cursor_y;
static uint16_t cursor_tile = NO_CURSOR;
static uint8_t phase;
static const Sight *sight_map;   /* NULL: omniscient (tests, mockups) */
static int16_t portal_x = -1, portal_y = -1;   /* open portal, if any */

void view_invalidate(void) { valid = false; }

void view_set_origin(int16_t x, int16_t y)
{
    origin_x = x;
    origin_y = y;
}

int16_t view_origin_x(void) { return origin_x; }
int16_t view_origin_y(void) { return origin_y; }

void view_follow(const World *w, int16_t x, int16_t y)
{
    const int16_t margin = 2;
    int16_t rx = (int16_t)(x - origin_x), ry = (int16_t)(y - origin_y);
    if (rx < margin)
        origin_x = (int16_t)(x - margin);
    else if (rx > VIEW_W - 1 - margin)
        origin_x = (int16_t)(x - (VIEW_W - 1 - margin));
    if (ry < margin)
        origin_y = (int16_t)(y - margin);
    else if (ry > VIEW_H - 1 - margin)
        origin_y = (int16_t)(y - (VIEW_H - 1 - margin));
    if (w->wrap) {    /* keep the origin inside the world */
        origin_x = (int16_t)(((origin_x % w->w) + w->w) % w->w);
        origin_y = (int16_t)(((origin_y % w->h) + w->h) % w->h);
    } else {          /* keep small maps in view */
        if (origin_x > w->w - VIEW_W) origin_x = (int16_t)(w->w - VIEW_W);
        if (origin_y > w->h - VIEW_H) origin_y = (int16_t)(w->h - VIEW_H);
        if (origin_x < 0) origin_x = 0;
        if (origin_y < 0) origin_y = 0;
    }
}

void view_set_cursor(int16_t x, int16_t y, uint16_t tile)
{
    cursor_x = x;
    cursor_y = y;
    cursor_tile = tile;
}

void view_set_sight(const Sight *s)
{
    sight_map = s;
}

void view_set_portal(int16_t x, int16_t y)
{
    portal_x = x;
    portal_y = y;
}

void view_set_phase(uint8_t p) { phase = p & 1; }

static void push(FieldLayers *f, uint16_t id)
{
    if (f->n < VIEW_MAX_LAYERS)
        f->id[f->n++] = id;
}

/* push() for an airborne unit: also flags the layer, the renderer draws
 * it a few pixels higher (GDD 11.3). */
static void push_air(FieldLayers *f, uint16_t id)
{
    if (f->n < VIEW_MAX_LAYERS) {
        f->air |= (uint16_t)(1u << f->n);
        f->id[f->n++] = id;
    }
}

static uint8_t wall_mask(const World *w, int16_t x, int16_t y)
{
    return (uint8_t)((world_is_wall_line(w, x, (int16_t)(y - 1)) ? 1 : 0) |
                     (world_is_wall_line(w, (int16_t)(x + 1), y) ? 2 : 0) |
                     (world_is_wall_line(w, x, (int16_t)(y + 1)) ? 4 : 0) |
                     (world_is_wall_line(w, (int16_t)(x - 1), y) ? 8 : 0));
}

/* Layers that only change when the map changes: floor, half floors, decor,
 * feature (candles in phase 0). (wx, wy) must be inside the map. */
static void compose_static(const World *w, int16_t wx, int16_t wy, FieldLayers *out)
{
    static const int8_t DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};
    uint8_t fl, fe, i;

    out->n = 0;
    out->air = 0;
    out->ride = 0;
    out->foe = 0;
    fl = w->floor[wy][wx];
    fe = w->feature[wy][wx];
    push(out, FLOOR_TILE[fl]);

    /* Half floors: each side of a wall line shows the neighbour's floor. */
    if (world_is_wall_line(w, wx, wy)) {
        for (i = 0; i < 4; i++) {
            int16_t nx = (int16_t)(wx + DX[i]), ny = (int16_t)(wy + DY[i]);
            uint8_t nfl = world_floor(w, nx, ny);
            if (!world_is_wall_line(w, nx, ny) && nfl != fl)
                push(out, (uint16_t)(FLOOR_HALF[nfl] + i));
        }
    }

    if (w->decor[wy][wx] == DE_RUG)
        push(out, T_DECOR_RUG);
    else if (w->decor[wy][wx] == DE_PENTACLE)
        push(out, T_DECOR_PENTACLE);

    if (fe == FE_WALL) {
        push(out, (uint16_t)(T_WALL_00 + wall_mask(w, wx, wy)));
    } else if (fe == FE_DOOR_CLOSED || fe == FE_DOOR_OPEN) {
        bool vertical = world_is_wall_line(w, wx, (int16_t)(wy - 1)) ||
                        world_is_wall_line(w, wx, (int16_t)(wy + 1));
        push(out, vertical ? (fe == FE_DOOR_OPEN ? T_DOOR_V_OPEN : T_DOOR_V_CLOSED)
                           : (fe == FE_DOOR_OPEN ? T_DOOR_H_OPEN : T_DOOR_H_CLOSED));
    } else if (fe == FE_CANDLE) {
        push(out, T_CANDLE_0);
    } else if (fe != FE_NONE) {
        push(out, FEATURE_TILE[fe]);
    }
}

/* F7 (M4e): a building loses its whole roof while an OWN ground unit
 * stands under it (the player sees his building from within). The
 * building is the 4-connected region of roofed fields; it is flooded
 * once per change of the own units under roofs, not once per field. */
/* Whose eyes decide whether a roof is lifted: only the currently active
 * figure, and only along its line of sight (playtest 2026-10-05, D41).
 * Standing outside therefore shows nothing of the inside, a figure in a
 * room uncovers that room, and an open door gives a glimpse - exactly as
 * far as the line of sight reaches. Negative x switches it off.
 *
 * Replaces the old flood fill, which lifted the whole connected roof as
 * soon as one own unit stood anywhere under it (F7, M4e) - and with it
 * two buffers worth 3.9 KB of eZ80 RAM. */
static int16_t roof_vx = -1, roof_vy;

void view_set_roof_viewer(int16_t x, int16_t y)
{
    roof_vx = x;
    roof_vy = y;
}

static bool roof_lifted(const World *w, int16_t wx, int16_t wy)
{
    int16_t dx, dy;
    if (roof_vx < 0)
        return false;
    world_delta(w, roof_vx, roof_vy, wx, wy, &dx, &dy);
    if (dx > SIGHT_GROUND || dx < -SIGHT_GROUND ||
        dy > SIGHT_GROUND || dy < -SIGHT_GROUND)
        return false;
    return sight_has_los(w, roof_vx, roof_vy, wx, wy);
}

/* The roof goes on LAST, not with the static layers (D44): drawn early it
 * sat below the units, and the renderer paints bottom-up - a figure under
 * a closed roof appeared to stand on it. Pushed here it covers whatever is
 * underneath, and the sight overlay and cursor still come after. */
static void apply_roof_rule(const World *w, int16_t wx, int16_t wy,
                            FieldLayers *out)
{
    if (!world_wrap(w, &wx, &wy) || !world_has_roof(w, wx, wy))
        return;
    if (roof_lifted(w, wx, wy))
        return;
    push(out, T_ROOF);
}


/* A unit the frontend animates itself (gliding sprite): left out of the
 * composition meanwhile. NO_UNIT = none. Presentation only. */
static uint8_t hidden_unit_id = NO_UNIT;

void view_hide_unit(uint8_t id)
{
    hidden_unit_id = id;
}

/* Hidden movement (GDD 3.4, AMI 4): enemy units are only drawn when the
 * viewer currently sees their field; invisible enemies never. */
static void push_unit(const World *w, const Unit *un, FieldLayers *out, bool air)
{
    uint8_t first = out->n;
    if (hidden_unit_id != NO_UNIT && un->id == hidden_unit_id)
        return;
    if (sight_map && un->owner != sight_map->owner &&
        (!sight_visible(sight_map, w, un->x, un->y) || (un->flags & UF_INVISIBLE)))
        return;
    {   /* a rider sits behind its mount, lifted by the renderer (M4k): the
         * mount's body hides the rider's legs, no extra artwork needed */
        uint8_t rk = ride_rider_kind(un);
        if (rk < CR_COUNT && out->n + 2 <= VIEW_MAX_LAYERS) {
            if (air)
                out->air |= (uint16_t)(1u << out->n);
            out->ride |= (uint16_t)(1u << out->n);
            out->id[out->n++] = (uint16_t)(CREATURE_TILE[rk] + un->owner);
        }
    }
    if (air)
        push_air(out, (uint16_t)(CREATURE_TILE[un->kind] + un->owner));
    else
        push(out, (uint16_t)(CREATURE_TILE[un->kind] + un->owner));
    /* Mark what belongs to an enemy wizard so the renderer can frame it
     * (B4). Wild animals stay unmarked - they are nobody's troops. */
    if (sight_map && un->owner != sight_map->owner && un->owner != OWN_NEUTRAL)
        while (first < out->n)
            out->foe |= (uint16_t)(1u << first++);
}

/* Hidden map (GDD 11.2): unexplored fields are a black tile, explored but
 * out of sight get the raster overlay on top. */
static void apply_sight(const World *w, int16_t wx, int16_t wy, FieldLayers *out)
{
    if (!sight_map)
        return;
    if (!sight_explored(sight_map, w, wx, wy)) {
        out->n = 0;
        out->air = 0;
        out->ride = 0;
        out->foe = 0;
        push(out, T_UNEXPLORED);
    } else if (!sight_visible(sight_map, w, wx, wy)) {
        push(out, T_OVERLAY_REMEMBERED);
    }
}

static void compose_cursor(const World *w, int16_t wx, int16_t wy, FieldLayers *out)
{
    if (cursor_tile != NO_CURSOR) {
        int16_t cx = cursor_x, cy = cursor_y;
        if (world_wrap(w, &cx, &cy) && cx == wx && cy == wy)
            push(out, cursor_tile);
    }
}

/* Per-frame layers on top: animation phase, object, unit. */
static void compose_dynamic(const World *w, int16_t wx, int16_t wy, FieldLayers *out)
{
    uint8_t i, u;

    if (phase)
        for (i = 0; i < out->n; i++)
            out->id[i] = anim_swap(out->id[i], phase);

    for (i = 0; i < w->object_count; i++) {
        if (w->objects[i].x == wx && w->objects[i].y == wy) {
            push(out, w->objects[i].tile);
            break;
        }
    }

    if (portal_x == wx && portal_y == wy)
        push(out, phase ? T_PORTAL_1 : T_PORTAL_0);

    u = world_unit_at(w, wx, wy, UL_GROUND);
    if (u != NO_UNIT)
        push_unit(w, &w->units[u], out, false);
    u = world_unit_at(w, wx, wy, UL_AIR);
    if (u != NO_UNIT) {
        push(out, T_AIR_SHADOW);      /* ground shadow below the flyer */
        push_unit(w, &w->units[u], out, true);
    }
    apply_roof_rule(w, wx, wy, out);
    {   /* area effect overlay (M4d, layer 7) */
        AreaKind k = area_kind_at(w, wx, wy);
        if (k != AREA_NONE) {
            uint8_t p2 = area_power_at(w, wx, wy);
            uint16_t base = k == AREA_FIRE ? T_AREA_FIRE_0
                          : k == AREA_BLOB ? T_AREA_BLOB_0
                          : k == AREA_VINE ? T_AREA_VINE_0 : T_AREA_FLOOD_0;
            (void)p2;
            push(out, phase ? (uint16_t)(base + 1) : base);
        }
    }
}

/* Reference implementation (slow, used by tests and the panel). */
void view_compose(const World *w, int16_t x, int16_t y, FieldLayers *out)
{
    int16_t wx = x, wy = y;
    if (!world_wrap(w, &wx, &wy)) {
        out->n = 0;
        push(out, T_FLOOR_GRASS);   /* outside a small map */
        return;
    }
    compose_static(w, wx, wy, out);
    compose_dynamic(w, wx, wy, out);
    apply_sight(w, wx, wy, out);
    compose_cursor(w, wx, wy, out);
}

/* Static layers of every map field, computed once per map
 * (MAP_MAX_W * MAP_MAX_H * sizeof(FieldLayers) = 39 KB). Platforms with
 * little RAM (the Mega Drive port, repo lords-of-chaos-md: 64 KB) build with
 * VIEW_STATIC_CACHE=0 and compose the static layers on demand instead;
 * the result is the same, only slower. */
#ifndef VIEW_STATIC_CACHE
#define VIEW_STATIC_CACHE 1
#endif

#if VIEW_STATIC_CACHE
static FieldLayers scache[MAP_MAX_H][MAP_MAX_W];
#endif
static const World *cache_world;
static uint8_t cache_gen;

void view_rebuild(const World *w)
{
#if VIEW_STATIC_CACHE
    uint8_t x, y;
    for (y = 0; y < w->h; y++)
        for (x = 0; x < w->w; x++)
            compose_static(w, x, y, &scache[y][x]);
#endif
    cache_world = w;
    cache_gen = w->generation;
}

/* Fast path: cached static layers + a per-frame overlay of objects, units
 * and cursor mapped to window positions. Must equal view_compose(). */
static uint16_t over_obj[VIEW_H][VIEW_W];
static uint8_t over_unit[VIEW_H][VIEW_W];   /* ground unit index + 1 */
static uint8_t over_air[VIEW_H][VIEW_W];    /* air unit index + 1, 0 = none */
#define NO_TILE 0xFFFF

static bool to_view(const World *w, int16_t x, int16_t y, uint8_t *vx, uint8_t *vy)
{
    int16_t rx = (int16_t)(x - origin_x), ry = (int16_t)(y - origin_y);
    if (w->wrap) {   /* origin and (x, y) are inside the world: no division */
        if (rx < 0) rx = (int16_t)(rx + w->w);
        if (ry < 0) ry = (int16_t)(ry + w->h);
    }
    if (rx < 0 || ry < 0 || rx >= VIEW_W || ry >= VIEW_H)
        return false;
    *vx = (uint8_t)rx;
    *vy = (uint8_t)ry;
    return true;
}

static void build_overlay(const World *w)
{
    uint8_t i, vx, vy;
    memset(over_obj, 0xFF, sizeof over_obj);     /* NO_TILE */
    memset(over_unit, 0, sizeof over_unit);
    memset(over_air, 0, sizeof over_air);
    for (i = w->object_count; i-- > 0;)   /* first object in the list wins */
        if (to_view(w, w->objects[i].x, w->objects[i].y, &vx, &vy))
            over_obj[vy][vx] = w->objects[i].tile;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *un = &w->units[i];
        if (to_view(w, un->x, un->y, &vx, &vy)) {
            if (un->flags & UF_FLYING)
                over_air[vy][vx] = (uint8_t)(i + 1);
            else
                over_unit[vy][vx] = (uint8_t)(i + 1);
        }
    }
}

static void compose_fast(const World *w, uint8_t vx, uint8_t vy, FieldLayers *out)
{
    int16_t wx = (int16_t)(origin_x + vx), wy = (int16_t)(origin_y + vy);
    uint8_t i;
    if (w->wrap) {   /* origin is normalised in view_update(): one subtraction */
        if (wx >= w->w) wx = (int16_t)(wx - w->w);
        if (wy >= w->h) wy = (int16_t)(wy - w->h);
    } else if (wx < 0 || wy < 0 || wx >= w->w || wy >= w->h) {
        out->n = 1;
        out->id[0] = T_FLOOR_GRASS;
        return;
    }
#if VIEW_STATIC_CACHE
    *out = scache[wy][wx];
#else
    compose_static(w, wx, wy, out);
#endif
    if (phase)
        for (i = 0; i < out->n; i++)
            out->id[i] = anim_swap(out->id[i], phase);
    if (over_obj[vy][vx] != NO_TILE)
        push(out, over_obj[vy][vx]);
    if (portal_x == wx && portal_y == wy)
        push(out, phase ? T_PORTAL_1 : T_PORTAL_0);
    if (over_unit[vy][vx])
        push_unit(w, &w->units[over_unit[vy][vx] - 1], out, false);
    if (over_air[vy][vx]) {
        push(out, T_AIR_SHADOW);
        push_unit(w, &w->units[over_air[vy][vx] - 1], out, true);
    }
    apply_roof_rule(w, wx, wy, out);
    {   /* area effect overlay (M4d, layer 7) */
        AreaKind k = area_kind_at(w, wx, wy);
        if (k != AREA_NONE) {
            uint16_t base = k == AREA_FIRE ? T_AREA_FIRE_0
                          : k == AREA_BLOB ? T_AREA_BLOB_0
                          : k == AREA_VINE ? T_AREA_VINE_0 : T_AREA_FLOOD_0;
            push(out, phase ? (uint16_t)(base + 1) : base);
        }
    }
    apply_sight(w, wx, wy, out);
    compose_cursor(w, wx, wy, out);
}

uint8_t view_update(const World *w)
{
    static uint8_t had_air[VIEW_H][VIEW_W];
    FieldLayers f;
    uint8_t vx, vy, n = 0;
    if (cache_world != w || cache_gen != w->generation)
        view_rebuild(w);
    if (w->wrap) {   /* once per frame, so the per-field code needs no division */
        origin_x = (int16_t)(((origin_x % w->w) + w->w) % w->w);
        origin_y = (int16_t)(((origin_y % w->h) + w->h) % w->h);
    }
    build_overlay(w);
    for (vy = 0; vy < VIEW_H; vy++) {
        for (vx = 0; vx < VIEW_W; vx++) {
            compose_fast(w, vx, vy, &f);
            animated[vy][vx] = 0;
            {
                uint8_t i;
                for (i = 0; i < f.n; i++)
                    if (is_animated(f.id[i]))
                        animated[vy][vx] = 1;
            }
            had_air[vy][vx] = (fields[vy][vx].air | fields[vy][vx].ride) != 0;
            if (!valid || f.n != fields[vy][vx].n || f.air != fields[vy][vx].air ||
                f.ride != fields[vy][vx].ride || f.foe != fields[vy][vx].foe ||
                memcmp(f.id, fields[vy][vx].id, f.n * sizeof f.id[0]) != 0) {
                fields[vy][vx] = f;
                dirty[vy][vx] = 1;
            }
        }
    }
    valid = true;
    /* Airborne units are drawn a few pixels into the field above (GDD
     * 11.3): a field whose air layer changed - old or new - must repaint
     * the field above (stale overhang), and a repainted field must be
     * followed by the air field below it, since fields draw top-down. */
    for (vy = 0; vy < VIEW_H; vy++) {
        for (vx = 0; vx < VIEW_W; vx++) {
            if (!dirty[vy][vx])
                continue;
            if (vy > 0 && ((fields[vy][vx].air | fields[vy][vx].ride) ||
                           had_air[vy][vx]))
                dirty[vy - 1][vx] = 1;
            if (vy + 1 < VIEW_H &&
                (fields[vy + 1][vx].air | fields[vy + 1][vx].ride))
                dirty[vy + 1][vx] = 1;
        }
    }
    for (vy = 0; vy < VIEW_H; vy++)
        for (vx = 0; vx < VIEW_W; vx++)
            n = (uint8_t)(n + dirty[vy][vx]);
    return n;
}

uint8_t view_animate(uint8_t p)
{
    uint8_t vx, vy, i, n = 0;
    phase = p & 1;
    for (vy = 0; vy < VIEW_H; vy++)
        for (vx = 0; vx < VIEW_W; vx++) {
            FieldLayers *f = &fields[vy][vx];
            if (!animated[vy][vx])
                continue;
            for (i = 0; i < f->n; i++) {
                uint16_t to = anim_swap(f->id[i], phase);
                if (to != f->id[i]) {
                    f->id[i] = to;
                    dirty[vy][vx] = 1;
                }
            }
            n = (uint8_t)(n + dirty[vy][vx]);
        }
    return n;
}

bool view_dirty(uint8_t vx, uint8_t vy) { return dirty[vy][vx] != 0; }

/* Force one field to repaint on the next render_fields() (fx overlays,
 * M5c). The cached layers stay valid - only the screen pixels are stale. */
void view_mark_dirty(uint8_t vx, uint8_t vy)
{
    if (vx < VIEW_W && vy < VIEW_H)
        dirty[vy][vx] = 1;
}

const FieldLayers *view_field(uint8_t vx, uint8_t vy) { return &fields[vy][vx]; }

void view_clean(void) { memset(dirty, 0, sizeof dirty); }

uint32_t view_hash(void)
{
    uint32_t h = 2166136261UL;
    uint8_t vx, vy, i;
    for (vy = 0; vy < VIEW_H; vy++)
        for (vx = 0; vx < VIEW_W; vx++) {
            const FieldLayers *f = &fields[vy][vx];
            h = (h ^ f->n) * 16777619UL;
            for (i = 0; i < f->n; i++) {
                h = (h ^ (f->id[i] & 0xFF)) * 16777619UL;
                h = (h ^ (f->id[i] >> 8)) * 16777619UL;
            }
        }
    return h;
}
