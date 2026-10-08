/* Movement of the AI: single steps, paths around walls and doors, the ring of
 * recently visited fields (K10.3 step 6). */
#include "ai_priv.h"

#include <string.h>

#include "combat.h"

bool ai_step_toward(World *w, Rng *rng, uint8_t unit, int16_t x, int16_t y)
{
    Unit *u;
    int8_t dx, dy;
    (void)rng;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    dx = x > u->x ? 1 : (x < u->x ? -1 : 0);
    dy = y > u->y ? 1 : (y < u->y ? -1 : 0);
    if (dx == 0 && dy == 0)
        return false;
    if (world_move_unit(w, unit, dx, dy))
        return true;
    if (dx != 0 && world_move_unit(w, unit, dx, 0))   /* sidestep */
        return true;
    if (dy != 0 && world_move_unit(w, unit, 0, dy))
        return true;
    return world_move_unit(w, unit, (int8_t)-dx, dy) ||
           world_move_unit(w, unit, dx, (int8_t)-dy);
}

/* Try to interact with a blocking feature straight ahead: closed doors open
 * (CF_USE), locked ones with a key or by force, chests with a key or by
 * prying. True when the way is free now. */
bool ai_clear_feature(World *w, Rng *rng, uint8_t unit, int16_t x, int16_t y)
{
    uint8_t fe = world_feature(w, x, y);
    if (fe == FE_DOOR_CLOSED)
        return world_open_door(w, unit, x, y);
    if (fe == FE_DOOR_LOCKED) {          /* C2: key, else smash it */
        bool destroyed = false;
        if (world_unlock_door(w, unit, x, y))
            return true;
        return combat_terrain(w, rng, unit, x, y, &destroyed) > 0 && destroyed;
    }
    if (fe == FE_CHEST || fe == FE_CHEST_FREE)
        return items_open_chest(w, rng, unit, x, y);
    return false;
}

/* ---------- the ring of visited fields ---------- */

void ai_visit(World *w, uint8_t unit, int16_t x, int16_t y)
{
    Unit *u = &w->units[unit];
    u->visited[u->visit_head & 7] = (uint16_t)(y * w->w + x);
    u->visit_head = (uint8_t)((u->visit_head + 1) & 7);
}

bool ai_visited(const World *w, uint8_t unit, int16_t x, int16_t y)
{
    const Unit *u = &w->units[unit];
    uint16_t packed = (uint16_t)(y * w->w + x);
    uint8_t i;
    for (i = 0; i < 8; i++)
        if (u->visited[i] == packed)
            return true;
    return false;
}

/* ---------- walking with a path (D62) ---------- */

#define PATH_R 7                  /* the search window reaches this far */
#define PATH_W (2 * PATH_R + 1)

/* Can a walker cross this field? Closed doors count: it opens them. */
static bool path_open(const World *w, int16_t x, int16_t y, uint8_t owner)
{
    uint8_t fe = world_feature(w, x, y);
    (void)owner;                      /* units move: only the terrain counts (and it is cheap) */
    return !world_blocks(w, x, y) || fe == FE_DOOR_CLOSED || fe == FE_DOOR_LOCKED;
}

/* First step towards (tx, ty): breadth-first from the unit through the
 * window around it (8 directions) to the reachable field closest to the
 * target - the target itself when it lies inside and can be reached, so
 * a far goal still finds the door out of a house. The target field may
 * be blocked (a chest to open, a door). The buffers live on the stack:
 * 1.3 KB only while the AI walks. */
bool ai_path_step(const World *w, uint8_t unit, int16_t tx, int16_t ty,
                  int8_t *sdx, int8_t *sdy)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t first[PATH_W * PATH_W];       /* 1 + direction of the first step */
    uint8_t qx[PATH_W * PATH_W], qy[PATH_W * PATH_W];
    uint16_t head = 0, tail = 0;
    const Unit *u = &w->units[unit];
    int16_t gx, gy;                       /* target relative to the unit */
    uint8_t best_first = 0;
    int16_t best_d;
    world_delta(w, u->x, u->y, tx, ty, &gx, &gy);
    best_d = (int16_t)((gx < 0 ? -gx : gx) > (gy < 0 ? -gy : gy)
                           ? (gx < 0 ? -gx : gx) : (gy < 0 ? -gy : gy));
    if (best_d == 0)
        return false;
    memset(first, 0, sizeof first);
    first[PATH_R * PATH_W + PATH_R] = 0xFF;   /* the start */
    qx[tail] = PATH_R;
    qy[tail++] = PATH_R;
    while (head < tail) {
        uint8_t cx = qx[head], cy = qy[head], d;
        uint8_t from = first[cy * PATH_W + cx];
        head++;
        for (d = 0; d < 8; d++) {
            int16_t nx = (int16_t)(cx + DX[d]), ny = (int16_t)(cy + DY[d]);
            int16_t rx, ry, dist;
            uint16_t c;
            uint8_t step;
            if (nx < 0 || ny < 0 || nx >= PATH_W || ny >= PATH_W)
                continue;
            c = (uint16_t)(ny * PATH_W + nx);
            if (first[c])
                continue;
            step = from == 0xFF ? (uint8_t)(d + 1) : from;
            rx = (int16_t)(gx - (nx - PATH_R));
            ry = (int16_t)(gy - (ny - PATH_R));
            if (rx == 0 && ry == 0) {          /* the target: done */
                *sdx = DX[step - 1];
                *sdy = DY[step - 1];
                return true;
            }
            first[c] = 0xFE;                   /* seen */
            if (!path_open(w, (int16_t)(u->x + nx - PATH_R),
                           (int16_t)(u->y + ny - PATH_R), u->owner))
                continue;
            first[c] = step;
            dist = (int16_t)((rx < 0 ? -rx : rx) > (ry < 0 ? -ry : ry)
                                 ? (rx < 0 ? -rx : rx) : (ry < 0 ? -ry : ry));
            if (dist < best_d) {
                best_d = dist;
                best_first = step;
            }
            qx[tail] = (uint8_t)nx;
            qy[tail++] = (uint8_t)ny;
        }
    }
    if (!best_first)
        return false;                     /* nothing gets closer */
    *sdx = DX[best_first - 1];
    *sdy = DY[best_first - 1];
    return true;
}

uint8_t ai_walk_to(World *w, Rng *rng, uint8_t unit, int16_t tx, int16_t ty,
                   uint8_t steps, bool beside)
{
    uint8_t id = w->units[unit].id;
    while (steps-- > 0) {
        int8_t dx, dy;
        uint8_t dist;
        if (w->units[unit].ap < 4)
            break;
        dist = world_distance(w, w->units[unit].x, w->units[unit].y, tx, ty);
        if (dist == 0 || (beside && dist <= 1))
            break;
        if (ai_path_step(w, unit, tx, ty, &dx, &dy)) {
            int16_t nx = (int16_t)(w->units[unit].x + dx);
            int16_t ny = (int16_t)(w->units[unit].y + dy);
            uint8_t fe = world_feature(w, nx, ny);
            if (fe == FE_DOOR_CLOSED || fe == FE_DOOR_LOCKED ||
                fe == FE_CHEST || fe == FE_CHEST_FREE) {   /* open it */
                if (!ai_clear_feature(w, rng, unit, nx, ny))
                    break;
                continue;
            }
            if (!world_move_unit(w, unit, dx, dy))
                break;
        } else {
            if (!ai_step_toward(w, rng, unit, tx, ty))
                break;
            unit = world_find_unit(w, id);
            if (unit == NO_UNIT)
                return NO_UNIT;
        }
    }
    return unit;
}
