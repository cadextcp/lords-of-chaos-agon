#include "sight.h"

#include <string.h>

/* Blocking bitmap of the whole map, one bit per field, rebuilt at the
 * start of every sight_compute(): rays then cost one 8-bit load instead
 * of two indexed table reads with a row multiply (AGON-QUIRKS T2). */
static uint8_t blk[MAP_MAX_H][SIGHT_COLS];

static bool blocked(int16_t x, int16_t y)
{
    return (blk[y][(uint8_t)(x >> 3)] & (uint8_t)(0x80u >> (x & 7))) != 0;
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

void sight_compute(const World *w, Sight *s)
{
    uint8_t i, y8, b, range, bytes;
    int16_t x, y, ux, uy;

    memset(s->visible, 0, sizeof s->visible);
    bytes = (uint8_t)((w->w + 7) >> 3);
    for (y8 = 0; y8 < w->h; y8++)
        for (b = 0; b < bytes; b++)
            blk[y8][b] = world_sight_byte(w, y8, (uint8_t)(b << 3));

    for (i = 0; i < w->unit_count; i++) {
        const Unit *u = &w->units[i];
        if (u->owner != s->owner)
            continue;
        ux = u->x;
        uy = u->y;
        range = (u->flags & UF_FLYING) ? SIGHT_AIR : SIGHT_GROUND;
        set_bit(s->visible, w->w, w->h, ux, uy);
        set_bit(s->explored, w->w, w->h, ux, uy);
        for (y = (int16_t)(uy - range); y <= (int16_t)(uy + range); y++) {
            int16_t wy = y, dy;
            if (wy < 0 || wy >= w->h) {
                if (!w->wrap)
                    continue;
                while (wy < 0) wy = (int16_t)(wy + w->h);
                while (wy >= w->h) wy = (int16_t)(wy - w->h);
            }
            dy = (int16_t)(wy - uy);
            if (dy > (int16_t)(w->h >> 1)) dy = (int16_t)(dy - w->h);
            else if (dy < -(int16_t)(w->h >> 1)) dy = (int16_t)(dy + w->h);
            for (x = (int16_t)(ux - range); x <= (int16_t)(ux + range); x++) {
                int16_t wx = x, dx;
                if (wx < 0 || wx >= w->w) {
                    if (!w->wrap)
                        continue;
                    while (wx < 0) wx = (int16_t)(wx + w->w);
                    while (wx >= w->w) wx = (int16_t)(wx - w->w);
                }
                dx = (int16_t)(wx - ux);
                if (dx > (int16_t)(w->w >> 1)) dx = (int16_t)(dx - w->w);
                else if (dx < -(int16_t)(w->w >> 1)) dx = (int16_t)(dx + w->w);
                if (dx == 0 && dy == 0)
                    continue;            /* own cell, set above */
                /* Airborne sources look over everything (GDD 3.4); the
                 * covered-terrain exceptions for creatures below follow
                 * with the roof data. */
                if ((u->flags & UF_FLYING) ||
                    (!get_bit(s->visible, w->w, w->h, wx, wy) &&
                     path_clear(w, (uint8_t)ux, (uint8_t)uy, (int8_t)dx, (int8_t)dy))) {
                    set_bit(s->visible, w->w, w->h, wx, wy);
                    set_bit(s->explored, w->w, w->h, wx, wy);
                }
            }
        }
    }
}

bool sight_explored(const Sight *s, const World *w, int16_t x, int16_t y)
{
    return get_bit(s->explored, w->w, w->h, x, y);
}

bool sight_visible(const Sight *s, const World *w, int16_t x, int16_t y)
{
    return get_bit(s->visible, w->w, w->h, x, y);
}
