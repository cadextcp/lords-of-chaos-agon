#include "view.h"

#include <string.h>

/* Tile IDs per floor; half floors follow each floor tile in the order
 * n, s, w, e (see tools/build_tiles.py). */
static const uint8_t FLOOR_TILE[FL_COUNT] = {
    T_FLOOR_STONE, T_FLOOR_WOOD, T_FLOOR_GRASS, T_FLOOR_PATH};
static const uint8_t FLOOR_HALF[FL_COUNT] = {
    T_FLOOR_STONE_HALF_N, T_FLOOR_WOOD_HALF_N, T_FLOOR_GRASS_HALF_N, T_FLOOR_PATH_HALF_N};
enum { HALF_N, HALF_S, HALF_W, HALF_E };

static const uint8_t FEATURE_TILE[FE_COUNT] = {
    [FE_BED] = T_BED, [FE_BOOKSHELF] = T_BOOKSHELF, [FE_CANDLE] = T_CANDLE_0,
    [FE_CAULDRON] = T_CAULDRON, [FE_TABLE] = T_TABLE, [FE_CHAIR] = T_CHAIR,
    [FE_DRAWERS] = T_DRAWERS, [FE_CHEST] = T_CHEST, [FE_TREE] = T_TREE,
};

static FieldLayers fields[VIEW_H][VIEW_W];
static uint8_t dirty[VIEW_H][VIEW_W];
static bool valid;
static int16_t origin_x, origin_y;
static int16_t cursor_x, cursor_y;
static uint8_t cursor_tile = NO_CURSOR;
static uint8_t phase;

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
    if (!w->wrap) {   /* keep small maps in view */
        if (origin_x > w->w - VIEW_W) origin_x = (int16_t)(w->w - VIEW_W);
        if (origin_y > w->h - VIEW_H) origin_y = (int16_t)(w->h - VIEW_H);
        if (origin_x < 0) origin_x = 0;
        if (origin_y < 0) origin_y = 0;
    }
}

void view_set_cursor(int16_t x, int16_t y, uint8_t tile)
{
    cursor_x = x;
    cursor_y = y;
    cursor_tile = tile;
}

void view_set_phase(uint8_t p) { phase = p & 1; }

static void push(FieldLayers *f, uint8_t id)
{
    if (f->n < VIEW_MAX_LAYERS)
        f->id[f->n++] = id;
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
    fl = w->floor[wy][wx];
    fe = w->feature[wy][wx];
    push(out, FLOOR_TILE[fl]);

    /* Half floors: each side of a wall line shows the neighbour's floor. */
    if (world_is_wall_line(w, wx, wy)) {
        for (i = 0; i < 4; i++) {
            int16_t nx = (int16_t)(wx + DX[i]), ny = (int16_t)(wy + DY[i]);
            uint8_t nfl = world_floor(w, nx, ny);
            if (!world_is_wall_line(w, nx, ny) && nfl != fl)
                push(out, (uint8_t)(FLOOR_HALF[nfl] + i));
        }
    }

    if (w->decor[wy][wx] == DE_RUG)
        push(out, T_DECOR_RUG);
    else if (w->decor[wy][wx] == DE_PENTACLE)
        push(out, T_DECOR_PENTACLE);

    if (fe == FE_WALL) {
        push(out, (uint8_t)(T_WALL_00 + wall_mask(w, wx, wy)));
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

/* Per-frame layers on top: animation phase, object, unit, cursor. */
static void compose_dynamic(const World *w, int16_t wx, int16_t wy, FieldLayers *out)
{
    uint8_t i, u;

    if (phase)
        for (i = 0; i < out->n; i++)
            if (out->id[i] == T_CANDLE_0)
                out->id[i] = T_CANDLE_1;

    for (i = 0; i < w->object_count; i++) {
        if (w->objects[i].x == wx && w->objects[i].y == wy) {
            push(out, w->objects[i].tile);
            break;
        }
    }

    u = world_unit_at(w, wx, wy);
    if (u != NO_UNIT) {
        const Unit *un = &w->units[u];
        push(out, un->kind == CR_WIZARD ? (uint8_t)(T_WIZARD_P1 + un->owner) : T_GOBLIN);
    }

    if (cursor_tile != NO_CURSOR) {
        int16_t cx = cursor_x, cy = cursor_y;
        if (world_wrap(w, &cx, &cy) && cx == wx && cy == wy)
            push(out, cursor_tile);
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
}

/* Static layers of every map field, computed once per map (14 KB). */
static FieldLayers scache[MAP_MAX_H][MAP_MAX_W];
static const World *cache_world;
static uint8_t cache_gen;

void view_rebuild(const World *w)
{
    uint8_t x, y;
    for (y = 0; y < w->h; y++)
        for (x = 0; x < w->w; x++)
            compose_static(w, x, y, &scache[y][x]);
    cache_world = w;
    cache_gen = w->generation;
}

/* Fast path: cached static layers + a per-frame overlay of objects, units
 * and cursor mapped to window positions. Must equal view_compose(). */
static uint8_t over_obj[VIEW_H][VIEW_W], over_unit[VIEW_H][VIEW_W];

static bool to_view(const World *w, int16_t x, int16_t y, uint8_t *vx, uint8_t *vy)
{
    int16_t rx = (int16_t)(x - origin_x), ry = (int16_t)(y - origin_y);
    if (w->wrap) {
        rx = (int16_t)(((rx % w->w) + w->w) % w->w);
        ry = (int16_t)(((ry % w->h) + w->h) % w->h);
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
    memset(over_obj, 0xFF, sizeof over_obj);
    memset(over_unit, 0xFF, sizeof over_unit);
    for (i = w->object_count; i-- > 0;)   /* first object in the list wins */
        if (to_view(w, w->objects[i].x, w->objects[i].y, &vx, &vy))
            over_obj[vy][vx] = w->objects[i].tile;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *un = &w->units[i];
        if (to_view(w, un->x, un->y, &vx, &vy))
            over_unit[vy][vx] = un->kind == CR_WIZARD ? (uint8_t)(T_WIZARD_P1 + un->owner)
                                                      : T_GOBLIN;
    }
}

static void compose_fast(const World *w, uint8_t vx, uint8_t vy, FieldLayers *out)
{
    int16_t wx = (int16_t)(origin_x + vx), wy = (int16_t)(origin_y + vy);
    uint8_t i;
    if (!world_wrap(w, &wx, &wy)) {
        out->n = 1;
        out->id[0] = T_FLOOR_GRASS;
        return;
    }
    *out = scache[wy][wx];
    if (phase)
        for (i = 0; i < out->n; i++)
            if (out->id[i] == T_CANDLE_0)
                out->id[i] = T_CANDLE_1;
    if (over_obj[vy][vx] != 0xFF)
        push(out, over_obj[vy][vx]);
    if (over_unit[vy][vx] != 0xFF)
        push(out, over_unit[vy][vx]);
    if (cursor_tile != NO_CURSOR) {
        int16_t cx = cursor_x, cy = cursor_y;
        if (world_wrap(w, &cx, &cy) && cx == wx && cy == wy)
            push(out, cursor_tile);
    }
}

uint8_t view_update(const World *w)
{
    FieldLayers f;
    uint8_t vx, vy, n = 0;
    if (cache_world != w || cache_gen != w->generation)
        view_rebuild(w);
    build_overlay(w);
    for (vy = 0; vy < VIEW_H; vy++) {
        for (vx = 0; vx < VIEW_W; vx++) {
            compose_fast(w, vx, vy, &f);
            if (!valid || f.n != fields[vy][vx].n ||
                memcmp(f.id, fields[vy][vx].id, f.n) != 0) {
                fields[vy][vx] = f;
                dirty[vy][vx] = 1;
            }
            n = (uint8_t)(n + dirty[vy][vx]);
        }
    }
    valid = true;
    return n;
}

bool view_dirty(uint8_t vx, uint8_t vy) { return dirty[vy][vx] != 0; }

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
            for (i = 0; i < f->n; i++)
                h = (h ^ f->id[i]) * 16777619UL;
        }
    return h;
}
