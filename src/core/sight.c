#include "sight.h"

#include <string.h>

#include "area.h"
#include "gen/data.h"

/* Blocking bitmap of the whole map, one bit per field: a ray step then
 * costs one 8-bit load instead of two indexed table reads with a row
 * multiply (AGON-QUIRKS T2).
 *
 * Both bitmaps depend only on floor and feature (D56: a roof is display
 * only - walls, doors and windows decide what is seen), so they are built
 * once per map generation, not once per query. Every LOS test used to
 * rebuild all 1440 bits, and the AI asks one per candidate in a loop.
 * Invariant: whoever writes floor/feature calls world_map_changed().
 * The game code does; the selftest has to as well. */
static uint8_t blk[MAP_MAX_H][SIGHT_COLS];
static uint8_t blk_spell[MAP_MAX_H][SIGHT_COLS];     /* same, tall grass out (D36) */
static const World *blk_world;                       /* world both came from */
static uint8_t blk_gen;                              /* its generation back then */
static bool blk_spell_ready;                         /* the spell bitmap is current */

/* Which bitmap path_clear() consults; set by every entry point below. */
static const uint8_t (*blk_cur)[SIGHT_COLS] = blk;

static bool blocked(int16_t x, int16_t y)
{
    return (blk_cur[y][(uint8_t)(x >> 3)] & (uint8_t)(0x80u >> (x & 7))) != 0;
}

static void ensure_blk(const World *w)
{
    uint8_t y8, b, bytes;
    if (blk_world == w && blk_gen == w->generation)
        return;
    bytes = (uint8_t)((w->w + 7) >> 3);
    memset(blk, 0, sizeof blk);
    for (y8 = 0; y8 < w->h; y8++)
        for (b = 0; b < bytes; b++)
            blk[y8][b] = world_sight_byte(w, y8, (uint8_t)(b << 3));
    {   /* fire and blob are walls of flame and goo (K11.4) */
        uint8_t x8;
        for (y8 = 0; y8 < w->h; y8++)
            for (x8 = 0; x8 < w->w; x8++) {
                AreaKind ak = area_kind_at(w, x8, y8);
                if (ak == AREA_FIRE || ak == AREA_BLOB)
                    blk[y8][x8 >> 3] |= (uint8_t)(0x80u >> (x8 & 7));
            }
    }
    blk_world = w;
    blk_gen = w->generation;
    blk_spell_ready = false;
}

/* Spells reach through tall grass (D36): the same bitmap with tall grass
 * fields taken out that hold nothing else which blocks. */
static void ensure_blk_spell(const World *w)
{
    uint8_t x, y;
    ensure_blk(w);
    if (blk_spell_ready)
        return;
    memcpy(blk_spell, blk, sizeof blk_spell);
    for (y = 0; y < w->h; y++)
        for (x = 0; x < w->w; x++)
            if (w->floor[y][x] == FL_TALL_GRASS &&
                !world_feature_blocks_sight(w, x, y))
                blk_spell[y][x >> 3] &= (uint8_t)~(0x80u >> (x & 7));
    blk_spell_ready = true;
}

static void set_bit(uint8_t map[MAP_MAX_H][SIGHT_COLS], uint8_t w, uint8_t h,
                    int16_t x, int16_t y)
{
    if (x >= 0 && y >= 0 && x < w && y < h)
        map[y][(uint8_t)(x >> 3)] |= (uint8_t)(0x80u >> (x & 7));
}

static bool get_bit(const uint8_t map[MAP_MAX_H][SIGHT_COLS], uint8_t w,
                    uint8_t h, int16_t x, int16_t y)
{
    if (x < 0 || y < 0 || x >= w || y >= h)
        return false;
    return (map[y][(uint8_t)(x >> 3)] & (uint8_t)(0x80u >> (x & 7))) != 0;
}

void sight_init(Sight *s, uint8_t owner)
{
    memset(s, 0, sizeof *s);
    s->owner = owner;
}

/* Bresenham walk from (sx, sy) along the (already minimal) delta to the
 * target: true when no blocking field lies strictly between them - the
 * endpoints themselves never block (GDD 3.4). Runs in unwrapped
 * coordinates; a step moves by one field, so one conditional add or
 * subtract per axis covers the wrap. 8-bit math throughout (range
 * <= 11) to dodge the eZ80's 24-bit int arithmetic. */
static bool path_clear(const World *w, uint8_t sx, uint8_t sy, int8_t dx, int8_t dy)
{
    int8_t x = (int8_t)sx, y = (int8_t)sy;
    int8_t ix = (int8_t)(sx + dx), iy = (int8_t)(sy + dy);
    int8_t err, adx = (int8_t)(dx < 0 ? -dx : dx), ady = (int8_t)(dy < 0 ? -dy : dy);
    int8_t steps = adx > ady ? adx : ady;
    int8_t ex = dx < 0 ? -1 : 1, ey = dy < 0 ? -1 : 1;
    if (steps == 0)
        return true;
    err = (int8_t)(adx - ady);
    while (steps-- > 0) {
        int8_t e2 = (int8_t)(err * 2);
        int16_t nx, ny;
        if (e2 > -ady) {
            err = (int8_t)(err - ady);
            x = (int8_t)(x + ex);
        }
        if (e2 < adx) {
            err = (int8_t)(err + adx);
            y = (int8_t)(y + ey);
        }
        if (x == ix && y == iy)
            break;                       /* endpoint exclusive */
        nx = x;
        ny = y;
        if (nx < 0 || nx >= w->w || ny < 0 || ny >= w->h) {
            if (!w->wrap)
                return false;            /* outside a small map */
            if (nx < 0) nx = (int16_t)(nx + w->w);
            else if (nx >= w->w) nx = (int16_t)(nx - w->w);
            if (ny < 0) ny = (int16_t)(ny + w->h);
            else if (ny >= w->h) ny = (int16_t)(ny - w->h);
        }
        if (blocked(nx, ny))
            return false;
    }
    return true;
}

/* --- Recursive shadowcasting (D39, ADR 0009) --------------------------
 *
 * Replaces one Bresenham ray per target field (O(r^3) per unit) with eight
 * octant scans that visit every field once (O(r^2)). Slopes stay exact
 * fractions and are compared by cross multiplication - no division, no
 * floating point (ADR 0003). With col <= row <= 11 every product stays far
 * inside int16_t.
 *
 * The eight octants overlap on the axes and diagonals. Visibility is OR-ed,
 * so an overlap can only reveal a field, never hide one; that is the usual
 * trade-off of this algorithm and keeps the field of view symmetric around
 * the source.
 *
 * Reachable fields satisfy max(|dx|, |dy|) <= radius, so the Chebyshev
 * range of GDD 3.4 falls out of the octant decomposition itself. */
typedef struct {
    const World *w;
    uint8_t (*vis)[SIGHT_COLS];    /* fields seen */
    uint8_t (*vis2)[SIGHT_COLS];   /* ... also seen as ground targets, or NULL */
    uint8_t (*vis3)[SIGHT_COLS];   /* ... and as objects, or NULL */
    uint8_t reach;                 /* octagon limit R (K11.1) */
    uint8_t (*expl)[SIGHT_COLS];   /* fields explored too, NULL = not tracked */
    uint8_t ux, uy;        /* source field, already on the map */
    uint8_t radius;
} Cast;

/* (col, row) of an octant -> field offset: dx = a*col + b*row,
 * dy = c*col + d*row. The eight rows cover the whole square. */
static const int8_t OCT[8][4] = {
    { 1,  0,  0, -1}, { 0,  1, -1,  0}, { 0,  1,  1,  0}, { 1,  0,  0,  1},
    {-1,  0,  0,  1}, { 0, -1,  1,  0}, { 0, -1, -1,  0}, {-1,  0,  0, -1},
};

/* a/b < c/d for positive denominators. */
static bool slope_lt(int16_t an, int16_t ad, int16_t cn, int16_t cd)
{
    return (int16_t)(an * cd) < (int16_t)(cn * ad);
}

/* Absolute field for an offset. False when it falls off a non-wrapping
 * map; such a field also blocks, exactly as a ray leaving the map did. */
static bool cast_cell(const Cast *c, int8_t dx, int8_t dy, int16_t *ax,
                      int16_t *ay)
{
    int16_t x = (int16_t)(c->ux + dx), y = (int16_t)(c->uy + dy);
    if (!world_wrap(c->w, &x, &y))
        return false;
    *ax = x;
    *ay = y;
    return true;
}

static void cast_octant(Cast *c, uint8_t row, int16_t lo_n, int16_t lo_d,
                        int16_t hi_n, int16_t hi_d, uint8_t oct)
{
    const int8_t *m = OCT[oct];
    uint8_t col;
    int8_t prev = -1;                    /* -1 none yet, 0 clear, 1 blocked */

    if (row > c->radius || slope_lt(hi_n, hi_d, lo_n, lo_d))
        return;
    for (col = 0; col <= row; col++) {
        int16_t den = (int16_t)(2 * row);          /* both cell edges share it */
        int16_t cl_n = (int16_t)(2 * col - 1);     /* low edge of the field */
        int16_t ch_n = (int16_t)(2 * col + 1);     /* high edge */
        int16_t ax = 0, ay = 0;
        bool wall;
        if (slope_lt(ch_n, den, lo_n, lo_d))
            continue;                              /* entirely before window */
        if (slope_lt(hi_n, hi_d, cl_n, den))
            break;                                 /* entirely past window */
        {
            int8_t dx = (int8_t)(m[0] * (int8_t)col + m[1] * (int8_t)row);
            int8_t dy = (int8_t)(m[2] * (int8_t)col + m[3] * (int8_t)row);
            if (cast_cell(c, dx, dy, &ax, &ay)) {
                if (2 * (uint8_t)row + col < c->reach) {   /* inside the octagon */
                    set_bit(c->vis, c->w->w, c->w->h, ax, ay);
                    if (c->vis2)
                        set_bit(c->vis2, c->w->w, c->w->h, ax, ay);
                    if (c->vis3)
                        set_bit(c->vis3, c->w->w, c->w->h, ax, ay);
                    if (c->expl)
                        set_bit(c->expl, c->w->w, c->w->h, ax, ay);
                }
                wall = blocked(ax, ay);
            } else {
                wall = true;
            }
        }
        if (wall) {
            if (prev == 0)                         /* clear -> blocked */
                cast_octant(c, (uint8_t)(row + 1), lo_n, lo_d, cl_n, den, oct);
            prev = 1;
        } else {
            if (prev == 1) {                       /* blocked -> clear */
                lo_n = cl_n;
                lo_d = den;
            }
            prev = 0;
        }
    }
    if (prev == 0)
        cast_octant(c, (uint8_t)(row + 1), lo_n, lo_d, hi_n, hi_d, oct);
}

bool sight_in_reach(int16_t dx, int16_t dy, uint8_t r)
{
    int16_t ax = dx < 0 ? (int16_t)-dx : dx, ay = dy < 0 ? (int16_t)-dy : dy;
    int16_t big = ax > ay ? ax : ay, small = ax > ay ? ay : ax;
    return 2 * ax < r && 2 * ay < r && 2 * big + small < r;
}

/* Bits of one field for the owner. */
static void mark_field(Sight *s, const World *w, int16_t wx, int16_t wy,
                       bool terrain, bool ground, bool air, bool obj)
{
    if (terrain) {
        set_bit(s->visible, w->w, w->h, wx, wy);
        set_bit(s->explored, w->w, w->h, wx, wy);
    }
    if (ground)
        set_bit(s->vis_ground, w->w, w->h, wx, wy);
    if (air)
        set_bit(s->vis_air, w->w, w->h, wx, wy);
    if (obj)
        set_bit(s->vis_obj, w->w, w->h, wx, wy);
}

/* A field a flier cannot look into: ground under a canopy (forest, tall
 * grass, trees) or under a roof (K11.2, K11.4). */
static bool canopy(const World *w, int16_t x, int16_t y)
{
    return FLOOR_SIGHT[w->floor[y][x]] || w->feature[y][x] == FE_TREE;
}

/* Everything within the octagon R around (x, y), ignoring walls: what an
 * airborne observer and the Magic Eye see. `eye` also shows ground targets
 * under roofs and canopy (the Eye ignores the cover rules of the air). */
static void mark_octagon(Sight *s, const World *w, int16_t x, int16_t y,
                         uint8_t r, bool eye)
{
    int16_t dx, dy, lim = (int16_t)((r + 1) / 2);
    for (dy = -lim; dy <= lim; dy++)
        for (dx = -lim; dx <= lim; dx++) {
            int16_t wx = (int16_t)(x + dx), wy = (int16_t)(y + dy);
            bool near, roofed, cov;
            if (!sight_in_reach(dx, dy, r) || !world_wrap(w, &wx, &wy))
                continue;
            near = sight_in_reach(dx, dy, 4);
            roofed = world_has_roof(w, wx, wy);
            cov = canopy(w, wx, wy);
            mark_field(s, w, wx, wy, true,
                       eye || (!roofed && (near || !cov)),
                       true,
                       eye || (!roofed && !cov));
        }
}

void sight_compute(const World *w, Sight *s)
{
    uint8_t i, oct;
    int8_t nx, ny;
    Cast c;

    memset(s->visible, 0, sizeof s->visible);
    memset(s->vis_ground, 0, sizeof s->vis_ground);
    memset(s->vis_air, 0, sizeof s->vis_air);
    memset(s->vis_obj, 0, sizeof s->vis_obj);
    ensure_blk(w);
    c.w = w;
    c.vis = s->visible;
    c.expl = s->explored;

    for (i = 0; i < w->unit_count; i++) {
        const Unit *u = &w->units[i];
        if (u->owner != s->owner)
            continue;
        mark_field(s, w, u->x, u->y, true, true, true, true);
        if (u->flags & UF_FLYING) {
            mark_octagon(s, w, u->x, u->y, SIGHT_R_AIR, false);
            continue;
        }
        c.ux = (uint8_t)u->x;
        c.uy = (uint8_t)u->y;
        c.radius = SIGHT_GROUND;
        c.reach = SIGHT_R_GROUND;
        c.vis2 = s->vis_ground;
        c.vis3 = s->vis_obj;
        blk_cur = blk;
        for (oct = 0; oct < 8; oct++)
            cast_octant(&c, 1, 0, 1, 1, 1, oct);
        for (ny = -1; ny <= 1; ny++)         /* the eight neighbours: always (D < 4) */
            for (nx = -1; nx <= 1; nx++) {
                int16_t wx = (int16_t)(u->x + nx), wy = (int16_t)(u->y + ny);
                if (world_wrap(w, &wx, &wy))
                    mark_field(s, w, wx, wy, true, true, false, true);
            }
        if (!world_has_roof(w, u->x, u->y)) {
            /* flying targets are always seen within the octagon, over walls;
             * under a roof the other height is not seen at all (K11.2) */
            int16_t dx, dy;
            for (dy = -(SIGHT_R_GROUND / 2); dy <= SIGHT_R_GROUND / 2; dy++)
                for (dx = -(SIGHT_R_GROUND / 2); dx <= SIGHT_R_GROUND / 2; dx++) {
                    int16_t wx = (int16_t)(u->x + dx), wy = (int16_t)(u->y + dy);
                    if (sight_in_reach(dx, dy, SIGHT_R_GROUND) && world_wrap(w, &wx, &wy))
                        mark_field(s, w, wx, wy, false, false, true, false);
                }
        }
    }
}

bool sight_sees(const World *w, const Unit *obs, int16_t tx, int16_t ty,
                bool target_air, bool is_object)
{
    int16_t dx, dy;
    bool air = (obs->flags & UF_FLYING) != 0;
    (void)is_object;
    if (!world_wrap(w, &tx, &ty))
        return false;
    world_delta(w, obs->x, obs->y, tx, ty, &dx, &dy);
    if (!sight_in_reach(dx, dy, air ? SIGHT_R_AIR : SIGHT_R_GROUND))
        return false;
    if (air) {
        if (target_air)
            return true;                     /* fliers always see fliers */
        if (world_has_roof(w, tx, ty))
            return false;                    /* nobody looks into a house from above */
        if (sight_in_reach(dx, dy, 4))
            return true;
        return !canopy(w, tx, ty);          /* objects and units alike */
    }
    if (target_air)
        return !world_has_roof(w, obs->x, obs->y);   /* under a roof the other height is gone */
    if (sight_in_reach(dx, dy, 4))
        return true;                         /* the neighbours are always seen */
    return sight_has_los(w, obs->x, obs->y, tx, ty);
}

bool sight_unit_visible(const Sight *s, const World *w, const Unit *u)
{
    if (u->owner == s->owner)
        return true;
    return get_bit((u->flags & UF_FLYING) ? s->vis_air : s->vis_ground, w->w, w->h,
                   u->x, u->y);
}

bool sight_object_visible(const Sight *s, const World *w, int16_t x, int16_t y)
{
    return get_bit(s->vis_obj, w->w, w->h, x, y);
}

bool sight_shot_clear(const World *w, int16_t x0, int16_t y0, bool fly0,
                      int16_t x1, int16_t y1, bool fly1)
{
    if (!fly0 && !fly1)
        return sight_has_los(w, x0, y0, x1, y1);
    if (!fly0 && world_has_roof(w, x0, y0))
        return false;                    /* a ground shooter indoors cannot shoot up */
    if (!fly1 && world_has_roof(w, x1, y1))
        return false;                    /* nor can anything reach a roofed ground target */
    return true;                         /* only walls of the dungeon scenario stop it */
}

bool sight_has_los(const World *w, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx, dy;
    if (!world_wrap(w, &x0, &y0) || !world_wrap(w, &x1, &y1))
        return false;
    ensure_blk(w);                      /* independent of sight_compute */
    blk_cur = blk;
    dx = (int16_t)(x1 - x0);
    dy = (int16_t)(y1 - y0);
    if (w->wrap) {
        if (dx > w->w / 2) dx = (int16_t)(dx - w->w);
        if (dx < -w->w / 2) dx = (int16_t)(dx + w->w);
        if (dy > w->h / 2) dy = (int16_t)(dy - w->h);
        if (dy < -w->h / 2) dy = (int16_t)(dy + w->h);
    }
    return path_clear(w, (uint8_t)x0, (uint8_t)y0, (int8_t)dx, (int8_t)dy);
}

/* Spells reach through tall grass (D36): the same line test, with tall
 * grass fields that hold nothing else that blocks taken out of the map. */
bool sight_has_spell_los(const World *w, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx, dy;
    if (!world_wrap(w, &x0, &y0) || !world_wrap(w, &x1, &y1))
        return false;
    ensure_blk_spell(w);
    blk_cur = blk_spell;
    world_delta(w, x0, y0, x1, y1, &dx, &dy);
    return path_clear(w, (uint8_t)x0, (uint8_t)y0, (int8_t)dx, (int8_t)dy);
}

bool sight_explored(const Sight *s, const World *w, int16_t x, int16_t y)
{
    return get_bit(s->explored, w->w, w->h, x, y);
}

bool sight_visible(const Sight *s, const World *w, int16_t x, int16_t y)
{
    return get_bit(s->visible, w->w, w->h, x, y);
}

/* What one ground figure sees right now, cached per position and map
 * generation: the roof of a building opens on exactly these fields (D56). */
static uint8_t look[MAP_MAX_H][SIGHT_COLS];
static const World *look_world;
static uint8_t look_gen;
static int16_t look_x = -1, look_y;

bool sight_look(const World *w, int16_t x0, int16_t y0, int16_t x, int16_t y)
{
    if (!world_wrap(w, &x0, &y0) || !world_wrap(w, &x, &y))
        return false;
    ensure_blk(w);
    if (look_world != w || look_gen != w->generation || look_x != x0 || look_y != y0) {
        Cast c;
        uint8_t oct;
        memset(look, 0, sizeof look);
        set_bit(look, w->w, w->h, x0, y0);
        c.w = w;
        c.vis = look;
        c.vis2 = c.vis3 = NULL;
        c.expl = NULL;
        c.ux = (uint8_t)x0;
        c.uy = (uint8_t)y0;
        c.radius = SIGHT_GROUND;
        c.reach = SIGHT_R_GROUND;
        blk_cur = blk;
        for (oct = 0; oct < 8; oct++)
            cast_octant(&c, 1, 0, 1, 1, 1, oct);
        look_world = w;
        look_gen = w->generation;
        look_x = x0;
        look_y = y0;
    }
    return get_bit(look, w->w, w->h, x, y);
}

void sight_add_eye(Sight *s, const World *w, int16_t x, int16_t y, uint8_t range)
{
    mark_octagon(s, w, x, y, (uint8_t)(range + 1), true);   /* D <= range */
}
