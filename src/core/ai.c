#include "ai.h"

#include <stdio.h>
#include <string.h>

#include "combat.h"
#include "items.h"
#include "ride.h"
#include "sight.h"

/* Wizard AI (D62): the wizard keeps to his house until it is looted and
 * his creatures are mostly gone (or in a rare rage); the creatures guard
 * the house or march on the rival's. */
#define HOME_RANGE 6        /* the house: roofed fields this close to home */
#define AI_SUMMON_MAX 5     /* creatures he summons up to */
#define AI_GUARDS 2         /* the first creatures stay home */
#define AI_RAGE_CHANCE 12   /* one round in 12 a rage starts ... */
#define AI_RAGE_ROUNDS 3    /* ... and lasts this long */

static bool ai_clear_feature(World *w, Rng *rng, uint8_t unit,
                             int16_t x, int16_t y);

static bool unit_is_enemy(const World *w, uint8_t a, uint8_t b)
{
    return a != b && w->units[a].owner != w->units[b].owner;
}

/* Melee reach: grounded attackers cannot reach flyers. */
static bool melee_reachable(const Unit *hunter, const Unit *prey)
{
    if (prey->flags & UF_FLYING)
        return (hunter->flags & UF_FLYING) != 0;
    return true;
}

static uint8_t chebyshev(const World *w, const Unit *a, const Unit *b)
{
    int16_t dx = (int16_t)(b->x - a->x), dy = (int16_t)(b->y - a->y);
    if (w->wrap) {
        if (dx > w->w / 2) dx = (int16_t)(dx - w->w);
        if (dx < -w->w / 2) dx = (int16_t)(dx + w->w);
        if (dy > w->h / 2) dy = (int16_t)(dy - w->h);
        if (dy < -w->h / 2) dy = (int16_t)(dy + w->h);
    }
    return (uint8_t)((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)
                         ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy));
}

uint8_t ai_nearest_enemy(const World *w, uint8_t unit, uint8_t range)
{
    const Unit *u;
    uint8_t best = NO_UNIT, best_d = range + 1, i;
    if (unit >= w->unit_count)
        return NO_UNIT;
    u = &w->units[unit];
    for (i = 0; i < w->unit_count; i++) {
        uint8_t d;
        if (!unit_is_enemy(w, unit, i) || !melee_reachable(u, &w->units[i]) ||
            (w->units[i].flags & UF_INVISIBLE))
            continue;                    /* invisible: unseen (GDD 7.2) */
        d = chebyshev(w, u, &w->units[i]);
        if (d > range || d >= best_d)
            continue;
        if (!sight_has_los(w, u->x, u->y, w->units[i].x, w->units[i].y))
            continue;                    /* hidden movement: no cheating */
        best = i;
        best_d = d;
    }
    return best;
}

bool ai_step_toward(World *w, Rng *rng, uint8_t unit, int16_t x, int16_t y)
{
    Unit *u;
    (void)rng;
    int8_t dx, dy;
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

void ai_set_post(World *w, uint8_t unit)
{
    if (unit >= w->unit_count)
        return;
    w->units[unit].post_x = w->units[unit].x;
    w->units[unit].post_y = w->units[unit].y;
}

void ai_guard(World *w, Rng *rng, uint8_t unit, uint8_t home_range)
{
    uint8_t prey;
    int16_t dx, dy;
    (void)home_range;                    /* the post itself is the anchor */
    if (unit >= w->unit_count)
        return;
    prey = ai_nearest_enemy(w, unit, SIGHT_GROUND);
    if (prey != NO_UNIT) {              /* intruder: fight like a hunter */
        ai_hunter(w, rng, unit);
        return;
    }
    /* drift home when out of the post range */
    if (w->units[unit].post_x == 0xFF)
        return;
    dx = (int16_t)(w->units[unit].x - w->units[unit].post_x);
    dy = (int16_t)(w->units[unit].y - w->units[unit].post_y);
    if (dx < 0) dx = (int16_t)(-dx);
    if (dy < 0) dy = (int16_t)(-dy);
    if (dx > 0 || dy > 0) {             /* not home yet: drift back */
        uint8_t steps;
        for (steps = 0; steps < 2; steps++)
            if (ai_step_toward(w, rng, unit, w->units[unit].post_x,
                                    w->units[unit].post_y))
                break;
    }
}

/* Nearest treasure in the unit's line of sight within 9 fields (it only
 * knows what it sees - no cheating, GDD 10). On success the field is in
 * (*tx, *ty); standing on it, the unit picks it up. */
static bool nearest_treasure(World *w, uint8_t unit, int16_t *tx, int16_t *ty)
{
    uint8_t i, k;
    uint8_t best_d = 10;
    bool found = false;
    Unit *u = &w->units[unit];
    for (i = 0; i < w->object_count; i++) {
        int16_t ddx, ddy, d;
        bool treasure = false;
        for (k = 0; k < OBJ_COUNT; k++)
            if (OBJECTS[k].tile == w->objects[i].tile &&
                OBJECTS[k].category == OC_TREASURE)
                treasure = true;
        if (!treasure)
            continue;
        ddx = (int16_t)(w->objects[i].x - u->x);
        ddy = (int16_t)(w->objects[i].y - u->y);
        if (w->wrap) {
            if (ddx > w->w / 2) ddx = (int16_t)(ddx - w->w);
            if (ddx < -w->w / 2) ddx = (int16_t)(ddx + w->w);
            if (ddy > w->h / 2) ddy = (int16_t)(ddy - w->h);
            if (ddy < -w->h / 2) ddy = (int16_t)(ddy + w->h);
        }
        if (ddx < 0) ddx = (int16_t)(-ddx);
        if (ddy < 0) ddy = (int16_t)(-ddy);
        d = ddx > ddy ? ddx : ddy;
        if (d >= best_d)
            continue;
        if (!sight_has_los(w, u->x, u->y, w->objects[i].x, w->objects[i].y))
            continue;
        *tx = w->objects[i].x;
        *ty = w->objects[i].y;
        best_d = (uint8_t)d;
        found = true;
    }
    return found;
}

/* Try to interact with a blocking feature straight ahead (M4h/i
 * follow-up, issue 78): closed doors open (CF_USE), chests open with a
 * carried key or by prying. True when the way is free now. */
static bool ai_clear_feature(World *w, Rng *rng, uint8_t unit,
                             int16_t x, int16_t y)
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

void ai_hunter(World *w, Rng *rng, uint8_t unit)
{
    uint8_t prey, steps;
    if (unit >= w->unit_count)
        return;
    prey = ai_nearest_enemy(w, unit, SIGHT_GROUND);
    if (prey == NO_UNIT) {              /* nothing visible: wander */
        static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        uint8_t s, k;
        for (s = 0; s < 2; s++)
            for (k = 0; k < 3; k++)
                if (world_move_unit(w, unit, DX[rng_range(rng, 8)],
                                    DY[rng_range(rng, 8)]))
                    break;
        return;
    }
    for (steps = 0; steps < 3; steps++) {
        uint8_t foe;
        CombatResult r;
        int16_t px, py;
        if (unit >= w->unit_count)      /* died on a return blow */
            return;
        foe = ai_nearest_enemy(w, unit, 1);
        if (foe != NO_UNIT) {
            if (!combat_melee(w, rng, unit, foe, &r))
                return;
            if (r.died || r.attacker_died)
                return;
            continue;
        }
        if (w->units[unit].ap < 4)
            return;
        px = w->units[unit].x;
        py = w->units[unit].y;
        if (!ai_step_toward(w, rng, unit, w->units[prey].x,
                            w->units[prey].y)) {
            /* stuck: try to clear a door/chest in the direction of the prey */
            int8_t dx = w->units[prey].x > px ? 1 : (w->units[prey].x < px ? -1 : 0);
            int8_t dy = w->units[prey].y > py ? 1 : (w->units[prey].y < py ? -1 : 0);
            if (!ai_clear_feature(w, rng, unit,
                                  (int16_t)(px + dx), (int16_t)(py + dy)))
                return;                 /* really stuck */
        }
    }
}

/* ---------- wild animals (D35) ---------- */

#define WILD_DIRS 8
static const int8_t WDX[WILD_DIRS] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t WDY[WILD_DIRS] = {-1, -1, 0, 1, 1, 1, 0, -1};

/* Nearest visible unit of an owner in mask within range; with hr != 0xFF
 * only targets within hr of (hx, hy) count (the territory). */
static uint8_t nearest_masked(const World *w, uint8_t unit, uint8_t range,
                              uint8_t mask, int16_t hx, int16_t hy, uint8_t hr)
{
    const Unit *u = &w->units[unit];
    uint8_t best = NO_UNIT, best_d = (uint8_t)(range + 1), i;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *t = &w->units[i];
        uint8_t d;
        if (i == unit || t->owner >= OWN_NEUTRAL ||
            !(mask & (1u << t->owner)) || !melee_reachable(u, t) ||
            (t->flags & UF_INVISIBLE))
            continue;
        if (hr != 0xFF && world_distance(w, hx, hy, t->x, t->y) > hr)
            continue;
        d = chebyshev(w, u, t);
        if (d > range || d >= best_d)
            continue;
        if (!sight_has_los(w, u->x, u->y, t->x, t->y))
            continue;
        best = i;
        best_d = d;
    }
    return best;
}

/* Fight units of the owners in mask: strike an adjacent one, else close
 * in on the nearest visible (three actions, like a hunter). False when
 * there was nobody to fight. */
static bool fight_masked(World *w, Rng *rng, uint8_t unit, uint8_t mask,
                         int16_t hx, int16_t hy, uint8_t hr)
{
    uint8_t steps, id = w->units[unit].id;
    bool acted = false;
    for (steps = 0; steps < 3; steps++) {
        uint8_t foe, prey;
        CombatResult r;
        unit = world_find_unit(w, id);
        if (unit == NO_UNIT)
            return true;                  /* died on a return blow */
        foe = nearest_masked(w, unit, 1, mask, hx, hy, hr);
        if (foe != NO_UNIT) {
            if (!combat_melee(w, rng, unit, foe, &r))
                return acted;
            acted = true;
            if (r.died || r.attacker_died)
                return true;
            continue;
        }
        prey = nearest_masked(w, unit, SIGHT_GROUND, mask, hx, hy, hr);
        if (prey == NO_UNIT || w->units[unit].ap < 4)
            return acted;
        if (!ai_step_toward(w, rng, unit, w->units[prey].x, w->units[prey].y))
            return acted;
        acted = true;
    }
    return acted;
}

/* One quiet step in a random direction, sometimes none (grazing). */
static void graze(World *w, Rng *rng, uint8_t unit)
{
    uint8_t k;
    if (rng_range(rng, 2))
        return;
    for (k = 0; k < 3; k++) {
        uint8_t d = (uint8_t)rng_range(rng, WILD_DIRS);
        if (world_move_unit(w, unit, WDX[d], WDY[d]))
            return;
    }
}

/* Peaceful animals roam and only fight whoever attacked them. */
static void wild_peaceful(World *w, Rng *rng, uint8_t unit)
{
    if (w->units[unit].grudge &&
        fight_masked(w, rng, unit, w->units[unit].grudge, 0, 0, 0xFF))
        return;
    graze(w, rng, unit);
}

/* Territorial animals attack anyone inside their territory (and whoever
 * attacked them), otherwise they stay near home. */
static void wild_territorial(World *w, Rng *rng, uint8_t unit)
{
    Unit *u = &w->units[unit];
    int16_t hx, hy;
    if (u->post_x == 0xFF) {
        u->post_x = u->x;
        u->post_y = u->y;
    }
    hx = u->post_x;
    hy = u->post_y;
    if (u->grudge && fight_masked(w, rng, unit, u->grudge, 0, 0, 0xFF))
        return;
    if (fight_masked(w, rng, unit, 0x0F, hx, hy, TERRITORY))
        return;
    unit = world_find_unit(w, u->id);
    if (unit == NO_UNIT)
        return;
    if (world_distance(w, w->units[unit].x, w->units[unit].y, hx, hy) > 2)
        ai_step_toward(w, rng, unit, hx, hy);
    else
        graze(w, rng, unit);
}

/* A crossing herd walks its way (two steps, sidestepping), fights back
 * when attacked, and leaves the map once across. */
static void wild_herd(World *w, Rng *rng, uint8_t unit)
{
    uint8_t d, steps, id = w->units[unit].id;
    if (w->units[unit].grudge &&
        fight_masked(w, rng, unit, w->units[unit].grudge, 0, 0, 0xFF))
        return;                           /* fought back this round */
    unit = world_find_unit(w, id);
    if (unit == NO_UNIT)
        return;
    d = (uint8_t)(w->units[unit].herd_dir - 1);
    for (steps = 0; steps < 2; steps++) {
        Unit *u = &w->units[unit];
        int16_t nx = (int16_t)(u->x + WDX[d]), ny = (int16_t)(u->y + WDY[d]);
        uint8_t across = (WDX[d] != 0) ? w->w : w->h;
        if (u->travel >= across - 1 ||
            (!w->wrap && (nx < 0 || ny < 0 || nx >= w->w || ny >= w->h))) {
            world_remove_unit(w, unit);   /* off the map: gone, no kill */
            return;
        }
        /* straight on, slanting, or along an obstacle (a river) - the
         * walk counts even when blocked, so no herd is stuck for good */
        if (!world_move_unit(w, unit, WDX[d], WDY[d]) &&
            !world_move_unit(w, unit, WDX[(d + 1) % WILD_DIRS], WDY[(d + 1) % WILD_DIRS]) &&
            !world_move_unit(w, unit, WDX[(d + 7) % WILD_DIRS], WDY[(d + 7) % WILD_DIRS])) {
            uint8_t side = (uint8_t)((d + 2 + 4 * rng_range(rng, 2)) % WILD_DIRS);
            world_move_unit(w, unit, WDX[side], WDY[side]);
        }
        w->units[unit].travel++;
    }
}

/* ---------- scared animals (D37) ---------- */

#define ALARM_RADIUS 4     /* an aggressive act scares animals this close */
#define ALARM_ROUNDS 3     /* they stay alarmed this long */
#define CHARGE_PERCENT 20  /* else they flee */

static uint8_t group_of(const Unit *u)
{
    return u->group ? u->group : u->id;
}

/* Every disturbance of the round scares the peaceful and herd animals
 * nearby. One roll per group - the herd follows its leader: all charge
 * the disturber or all flee. */
static void resolve_disturbances(World *w, Rng *rng)
{
    uint8_t d, i, k, nd = 0;
    uint8_t decided[MAX_UNITS];
    for (d = 0; d < w->disturb_n; d++) {
        uint8_t dx = w->disturb[d][0], dy = w->disturb[d][1], who = w->disturb[d][2];
        for (i = 0; i < w->unit_count; i++) {
            const Unit *u = &w->units[i];
            uint8_t g, wild, j;
            bool seen = false, charge;
            if (u->owner != OWN_NEUTRAL || u->alarm)
                continue;
            wild = CREATURES[u->kind].wild;
            if (wild != WILD_PEACEFUL && wild != WILD_HERD)
                continue;                 /* monsters and territorials: own rules */
            if (world_distance(w, u->x, u->y, dx, dy) > ALARM_RADIUS)
                continue;
            g = group_of(u);
            for (j = 0; j < nd; j++)
                if (decided[j] == g)
                    seen = true;
            if (seen)
                continue;
            decided[nd++] = g;
            /* attacked themselves, they defend (D35); otherwise one roll */
            charge = who < OWN_NEUTRAL &&
                     ((u->grudge & (1u << who)) ||
                      rng_range(rng, 100) < CHARGE_PERCENT);
            for (k = 0; k < w->unit_count; k++) {
                Unit *m = &w->units[k];
                if (m->owner != OWN_NEUTRAL || group_of(m) != g)
                    continue;
                m->alarm = ALARM_ROUNDS;
                m->alarm_charge = charge ? 1 : 0;
                m->alarm_x = dx;
                m->alarm_y = dy;
                m->alarm_owner = who;
            }
        }
    }
    w->disturb_n = 0;
}

/* One rushing step. An elephant tramples: whoever stands in the way (not
 * another elephant) takes 2w6 and, if he falls, the elephant moves on;
 * tall grass under its feet is flattened. Returns the unit's new index,
 * NO_UNIT when it did not move. */
static uint8_t rush_step(World *w, Rng *rng, uint8_t unit, int8_t dx, int8_t dy)
{
    uint8_t id = w->units[unit].id;
    bool elephant = w->units[unit].kind == CR_ELEPHANT;
    if (!world_move_unit(w, unit, dx, dy)) {
        int16_t nx = (int16_t)(w->units[unit].x + dx), ny = (int16_t)(w->units[unit].y + dy);
        uint8_t victim;
        if (!elephant || !world_wrap(w, &nx, &ny))
            return NO_UNIT;
        victim = world_unit_at(w, nx, ny, UL_GROUND);
        if (victim == NO_UNIT || w->units[victim].kind == CR_ELEPHANT)
            return NO_UNIT;
        combat_damage(w, victim,
                      (uint8_t)(2 + rng_range(rng, 6) + rng_range(rng, 6)),
                      CR_ELEPHANT, OWN_NEUTRAL, false, NULL, false);
        unit = world_find_unit(w, id);
        if (unit == NO_UNIT || !world_move_unit(w, unit, dx, dy))
            return NO_UNIT;
    }
    unit = world_find_unit(w, id);
    if (unit != NO_UNIT && elephant) {
        uint8_t x = w->units[unit].x, y = w->units[unit].y;
        if (w->floor[y][x] == FL_TALL_GRASS) {   /* trampled flat */
            w->floor[y][x] = FL_GRASS;
            world_map_changed(w);
        }
    }
    return unit;
}

/* Flee: three steps straight away from the trouble, or else to the
 * neighbour farthest from it. */
static void flee(World *w, Rng *rng, uint8_t unit)
{
    uint8_t steps, id = w->units[unit].id;
    for (steps = 0; steps < 3; steps++) {
        uint8_t d, best = 0xFF, best_dist = 0, start = (uint8_t)rng_range(rng, WILD_DIRS);
        Unit *u;
        unit = world_find_unit(w, id);
        if (unit == NO_UNIT)
            return;
        u = &w->units[unit];
        {   /* straight away first - a stampede runs through */
            int16_t ax, ay;
            int8_t sx, sy;
            uint8_t moved;
            world_delta(w, u->alarm_x, u->alarm_y, u->x, u->y, &ax, &ay);
            sx = (int8_t)(ax > 0 ? 1 : (ax < 0 ? -1 : 0));
            sy = (int8_t)(ay > 0 ? 1 : (ay < 0 ? -1 : 0));
            if (sx == 0 && sy == 0)
                sx = (int8_t)(rng_range(rng, 2) ? 1 : -1);
            moved = rush_step(w, rng, unit, sx, sy);
            if (moved != NO_UNIT)
                continue;
            unit = world_find_unit(w, id);
            if (unit == NO_UNIT)
                return;
            u = &w->units[unit];
        }
        for (d = 0; d < WILD_DIRS; d++) {
            uint8_t dd = (uint8_t)((start + d) % WILD_DIRS);
            int16_t nx = (int16_t)(u->x + WDX[dd]), ny = (int16_t)(u->y + WDY[dd]);
            uint8_t dist;
            if (!world_wrap(w, &nx, &ny))
                continue;
            dist = world_distance(w, nx, ny, u->alarm_x, u->alarm_y);
            if (dist > best_dist) {
                best_dist = dist;
                best = dd;
            }
        }
        if (best == 0xFF || rush_step(w, rng, unit, WDX[best], WDY[best]) == NO_UNIT)
            return;
    }
}

/* Charge: run at the nearest unit of the disturber and attack it. */
static void charge(World *w, Rng *rng, uint8_t unit)
{
    uint8_t steps, id = w->units[unit].id;
    uint8_t mask = (uint8_t)(1u << w->units[unit].alarm_owner);
    for (steps = 0; steps < 3; steps++) {
        uint8_t foe, prey;
        CombatResult r;
        int8_t dx, dy;
        unit = world_find_unit(w, id);
        if (unit == NO_UNIT)
            return;
        foe = nearest_masked(w, unit, 1, mask, 0, 0, 0xFF);
        if (foe != NO_UNIT) {
            if (!combat_melee(w, rng, unit, foe, &r) || r.died || r.attacker_died)
                return;
            continue;
        }
        prey = nearest_masked(w, unit, SIGHT_GROUND, mask, 0, 0, 0xFF);
        if (prey == NO_UNIT) {
            flee(w, rng, unit);           /* lost sight of him: run off */
            return;
        }
        dx = w->units[prey].x > w->units[unit].x ? 1 : (w->units[prey].x < w->units[unit].x ? -1 : 0);
        dy = w->units[prey].y > w->units[unit].y ? 1 : (w->units[prey].y < w->units[unit].y ? -1 : 0);
        if (rush_step(w, rng, unit, dx, dy) == NO_UNIT &&
            !ai_step_toward(w, rng, unit, w->units[prey].x, w->units[prey].y))
            return;
    }
}

static void ai_wild(World *w, Rng *rng, uint8_t unit)
{
    if (w->units[unit].alarm) {           /* scared (D37) */
        w->units[unit].alarm--;
        if (w->units[unit].alarm_charge && w->units[unit].alarm_owner < OWN_NEUTRAL)
            charge(w, rng, unit);
        else
            flee(w, rng, unit);
        return;
    }
    if (w->units[unit].herd_dir) {
        wild_herd(w, rng, unit);
        return;
    }
    switch (CREATURES[w->units[unit].kind].wild) {
    case WILD_TERRITORIAL:
        wild_territorial(w, rng, unit);
        break;
    default:                              /* peaceful, herd animals at rest */
        wild_peaceful(w, rng, unit);
        break;
    }
}

void ai_run_hunters(World *w, Rng *rng, uint8_t owner, uint8_t skip_id)
{
    uint8_t ids[MAX_UNITS], n = 0, i;
    if (owner == OWN_NEUTRAL)             /* the round's trouble scares (D37) */
        resolve_disturbances(w, rng);
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner && w->units[i].id != skip_id)
            ids[n++] = w->units[i].id;
    for (i = 0; i < n; i++) {
        uint8_t u = world_find_unit(w, ids[i]);
        if (u == NO_UNIT)                 /* killed meanwhile */
            continue;
        if (owner == OWN_NEUTRAL &&
            (CREATURES[w->units[u].kind].wild != WILD_NONE || w->units[u].herd_dir))
            ai_wild(w, rng, u);           /* wild animals (D35) */
        else if (w->units[u].post_x != 0xFF)
            ai_guard(w, rng, u, 3);       /* map-defined guards hold (M4h) */
        else
            ai_hunter(w, rng, u);
    }
}


/* ---------- walking with a path (D62) ---------- */

#define PATH_R 10                 /* the search window reaches this far */
#define PATH_W (2 * PATH_R + 1)

/* Can a walker cross this field? Closed doors count: it opens them. */
static bool path_open(const World *w, int16_t x, int16_t y, uint8_t owner)
{
    uint8_t fe = world_feature(w, x, y);
    if (world_blocks(w, x, y) && fe != FE_DOOR_CLOSED && fe != FE_DOOR_LOCKED)
        return false;
    return world_blocking_unit_at(w, x, y, UL_GROUND, owner) == NO_UNIT;
}

/* First step towards (tx, ty): breadth-first from the unit through the
 * window around it (8 directions) to the reachable field closest to the
 * target - the target itself when it lies inside and can be reached, so
 * a far goal still finds the door out of a house. The target field may
 * be blocked (a chest to open, a door). The buffers live on the stack:
 * 1.3 KB only while the AI walks. */
static bool path_step(const World *w, uint8_t unit, int16_t tx, int16_t ty,
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

/* One step in a fixed direction. Returns the unit's index afterwards,
 * 0xFE when the step was blocked (also when the unit is bound, K11.7). */
#define AI_BLOCKED 0xFE
static uint8_t ai_move(World *w, Rng *rng, uint8_t unit, int8_t dx, int8_t dy)
{
    (void)rng;
    if (!world_move_unit(w, unit, dx, dy))
        return AI_BLOCKED;
    return unit;
}

/* Walk up to `steps` steps towards (tx, ty) - along a path when it is
 * near, greedily when far - opening closed doors on the way. Stops on
 * the target, or next to it with `beside`. Returns the unit's index,
 * NO_UNIT when it died. */
static uint8_t walk_to(World *w, Rng *rng, uint8_t unit, int16_t tx, int16_t ty,
                       uint8_t steps, bool beside)
{
    uint8_t id = w->units[unit].id;
    while (steps-- > 0) {
        int8_t dx, dy;
        uint8_t dist, r;
        if (w->units[unit].ap < 4)
            break;
        dist = world_distance(w, w->units[unit].x, w->units[unit].y, tx, ty);
        if (dist == 0 || (beside && dist <= 1))
            break;
        if (path_step(w, unit, tx, ty, &dx, &dy)) {
            int16_t nx = (int16_t)(w->units[unit].x + dx);
            int16_t ny = (int16_t)(w->units[unit].y + dy);
            uint8_t fe = world_feature(w, nx, ny);
            if (fe == FE_DOOR_CLOSED || fe == FE_DOOR_LOCKED ||
                fe == FE_CHEST || fe == FE_CHEST_FREE) {   /* open it */
                if (!ai_clear_feature(w, rng, unit, nx, ny))
                    break;
                continue;
            }
            r = ai_move(w, rng, unit, dx, dy);
            if (r == AI_BLOCKED)
                break;
            if (r == NO_UNIT)
                return NO_UNIT;
            unit = r;
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

/* ---------- the wizard's house (D62) ---------- */

static bool at_home(const World *w, const Game *g, uint8_t owner, int16_t x, int16_t y)
{
    return g->home_x[owner] != 0xFF && world_has_roof(w, x, y) &&
           world_distance(w, x, y, g->home_x[owner], g->home_y[owner]) <= HOME_RANGE;
}

/* What the wizard takes for himself: treasure, scrolls, keys, vials.
 * Weapons and shields are left to his creatures. */
static bool wizard_wants(uint8_t kind)
{
    uint8_t cat = OBJECTS[kind].category;
    return cat == OC_TREASURE || cat == OC_SCROLL || cat == OC_KEY ||
           (cat == OC_POTION && kind != OBJ_CAULDRON_EMPTY &&
            kind != OBJ_CAULDRON_FULL);
}

static bool can_carry(const World *w, uint8_t unit, uint8_t kind)
{
    const Unit *u = &w->units[unit];
    return u->item_count < UNIT_ITEMS &&
           (uint16_t)items_weight(w, unit) + OBJECTS[kind].weight <=
               CREATURES[ride_actor_kind(u)].carry;
}

/* Nearest thing left to loot in the wizard's house: an object he wants
 * and can carry, or an unopened chest. He knows his own house - no sight
 * needed there. */
static bool home_loot(const World *w, const Game *g, uint8_t owner, uint8_t wiz,
                      int16_t *tx, int16_t *ty, bool *chest)
{
    const Unit *u = &w->units[wiz];
    uint8_t best = 0xFF, i;
    int16_t x, y;
    for (i = 0; i < w->object_count; i++) {
        const Object *o = &w->objects[i];
        uint8_t kind = items_kind_of_tile(o->tile), d;
        if (kind == NO_ITEM || !wizard_wants(kind) || !can_carry(w, wiz, kind) ||
            !at_home(w, g, owner, o->x, o->y))
            continue;
        d = world_distance(w, u->x, u->y, o->x, o->y);
        if (d < best) {
            best = d;
            *tx = o->x;
            *ty = o->y;
            *chest = false;
        }
    }
    if (g->home_x[owner] == 0xFF)
        return best != 0xFF;
    for (y = (int16_t)(g->home_y[owner] - HOME_RANGE); y <= g->home_y[owner] + HOME_RANGE; y++)
        for (x = (int16_t)(g->home_x[owner] - HOME_RANGE); x <= g->home_x[owner] + HOME_RANGE; x++) {
            int16_t cx = x, cy = y;
            uint8_t fe, d;
            if (!world_wrap(w, &cx, &cy))
                continue;
            fe = w->feature[cy][cx];
            if ((fe != FE_CHEST && fe != FE_CHEST_FREE) || !at_home(w, g, owner, cx, cy))
                continue;
            d = world_distance(w, u->x, u->y, cx, cy);
            if (d < best) {
                best = d;
                *tx = cx;
                *ty = cy;
                *chest = true;
            }
        }
    return best != 0xFF;
}

/* The nearest home of another wizard (where the creatures march). */
static bool rival_home(const World *w, const Game *g, uint8_t owner, uint8_t unit,
                       int16_t *tx, int16_t *ty)
{
    uint8_t o, best = 0xFF;
    for (o = 0; o < OWN_NEUTRAL; o++) {
        uint8_t d;
        if (o == owner || g->home_x[o] == 0xFF)
            continue;
        d = world_distance(w, w->units[unit].x, w->units[unit].y,
                           g->home_x[o], g->home_y[o]);
        if (d < best) {
            best = d;
            *tx = g->home_x[o];
            *ty = g->home_y[o];
        }
    }
    return best != 0xFF;
}

/* Remember where every wizard on the map stands the first time the AI
 * looks - that is his house (the map is no secret, the units are). */
static void note_homes(const World *w, Game *g)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *u = &w->units[i];
        if (u->owner < OWN_NEUTRAL && ride_actor_kind(u) == CR_WIZARD &&
            g->home_x[u->owner] == 0xFF) {
            g->home_x[u->owner] = u->x;
            g->home_y[u->owner] = u->y;
        }
    }
}

/* ---------- the wizard's creatures (D62) ---------- */

static bool weapon_in_hand(const Unit *u)
{
    return u->in_use != NO_ITEM && u->in_use < u->item_count &&
           OBJECTS[u->items[u->in_use]].category == OC_WEAPON;
}

/* A creature with weapon hands and none in hand takes up a weapon or
 * shield from the house or one it sees within 8 fields. True when it
 * did something about it this phase. */
static bool equip(World *w, Rng *rng, uint8_t *unit, const Game *g, uint8_t owner)
{
    Unit *u = &w->units[*unit];
    uint8_t i, best = 0xFF, pick = 0xFF;
    if (!(CREATURES[ride_actor_kind(u)].flags & CF_WEAPONS) || weapon_in_hand(u))
        return false;
    for (i = 0; i < u->item_count; i++)       /* carried but not held */
        if (OBJECTS[u->items[i]].category == OC_WEAPON) {
            u->in_use = i;
            return false;
        }
    for (i = 0; i < w->object_count; i++) {
        const Object *o = &w->objects[i];
        uint8_t kind = items_kind_of_tile(o->tile), d;
        if (kind == NO_ITEM || OBJECTS[kind].category != OC_WEAPON ||
            !can_carry(w, *unit, kind))
            continue;
        d = world_distance(w, u->x, u->y, o->x, o->y);
        if (d >= best || (!at_home(w, g, owner, o->x, o->y) &&
                          (d > 8 || !sight_has_los(w, u->x, u->y, o->x, o->y))))
            continue;
        best = d;
        pick = i;
    }
    if (pick == 0xFF)
        return false;
    if (best > 1) {
        *unit = walk_to(w, rng, *unit, w->objects[pick].x, w->objects[pick].y, 3, true);
        if (*unit == NO_UNIT)
            return true;
        if (world_distance(w, w->units[*unit].x, w->units[*unit].y,
                           w->objects[pick].x, w->objects[pick].y) > 1)
            return true;                  /* on its way */
    }
    if (items_pick_up_object(w, *unit, pick)) {
        u = &w->units[*unit];
        u->in_use = (uint8_t)(u->item_count - 1);
    }
    return true;
}

/* One creature of an AI wizard: fight what it sees, arm itself, then
 * guard the house or march on the rival, picking up treasure in sight. */
static void ai_minion(World *w, Rng *rng, uint8_t unit, const Game *g,
                      uint8_t owner, bool guard)
{
    int16_t tx, ty;
    uint8_t prey = ai_nearest_enemy(w, unit, guard ? 5 : SIGHT_GROUND);
    if (prey != NO_UNIT) {
        ai_hunter(w, rng, unit);
        return;
    }
    if (equip(w, rng, &unit, g, owner) || unit == NO_UNIT)
        return;
    if (guard) {
        if (g->home_x[owner] != 0xFF &&
            world_distance(w, w->units[unit].x, w->units[unit].y,
                           g->home_x[owner], g->home_y[owner]) > 2)
            walk_to(w, rng, unit, g->home_x[owner], g->home_y[owner], 3, true);
        return;
    }
    if (nearest_treasure(w, unit, &tx, &ty)) {
        unit = walk_to(w, rng, unit, tx, ty, 3, false);
        if (unit != NO_UNIT && w->units[unit].x == tx && w->units[unit].y == ty)
            items_pick_up(w, unit);
        return;
    }
    if (rival_home(w, g, owner, unit, &tx, &ty) &&
        world_distance(w, w->units[unit].x, w->units[unit].y, tx, ty) > 2) {
        walk_to(w, rng, unit, tx, ty, 3, true);
        return;
    }
    ai_hunter(w, rng, unit);              /* arrived: roam and hunt */
}

/* Every creature of the AI wizard (all units of `owner` but him); the
 * AI_GUARDS oldest (lowest id) stay home. Id snapshot as in
 * ai_run_hunters. */
static void ai_run_minions(World *w, Rng *rng, const Game *g, uint8_t owner,
                           uint8_t skip_id)
{
    uint8_t ids[MAX_UNITS], n = 0, i, k;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner && w->units[i].id != skip_id)
            ids[n++] = w->units[i].id;
    for (i = 0; i < n; i++) {
        uint8_t u = world_find_unit(w, ids[i]), older = 0;
        if (u == NO_UNIT)
            continue;
        for (k = 0; k < n; k++)
            if (ids[k] < ids[i])
                older++;
        ai_minion(w, rng, u, g, owner, older < AI_GUARDS);
    }
}

/* The wizard's own actions; returns early once he is gone. Unit indices
 * change with every death, so the wizard is re-found by id after each
 * step that may kill. */
static void wizard_actions(Turns *t, World *w, AiCtx *ctx, uint8_t owner)
{
    static Sight sight;
    uint8_t wiz = NO_UNIT, wiz_id, i;

    for (i = 0; i < w->unit_count; i++) {
        if (w->units[i].owner != owner)
            continue;
        if (ride_actor_kind(&w->units[i]) == CR_WIZARD)
            wiz = i;
    }
    if (wiz == NO_UNIT) {                 /* leaderless creatures still hunt */
        ai_run_hunters(w, &t->rng, owner, NO_UNIT);
        return;
    }
    wiz_id = w->units[wiz].id;
    note_homes(w, ctx->game);
    if (ctx->game->rage_round[owner] != t->round) {   /* once a round */
        ctx->game->rage_round[owner] = t->round;
        if (ctx->game->rage[owner])
            ctx->game->rage[owner]--;
        else if (rng_range(&t->rng, AI_RAGE_CHANCE) == 0)
            ctx->game->rage[owner] = AI_RAGE_ROUNDS;
    }
    ai_run_minions(w, &t->rng, ctx->game, owner, wiz_id);   /* creatures first */
    wiz = world_find_unit(w, wiz_id);
    if (wiz == NO_UNIT)
        return;

    sight_init(&sight, owner);
    sight_compute(w, &sight);

    {   /* melee a visible enemy standing next to the wizard */
        CombatResult r;
        uint8_t foe = NO_UNIT;
        for (i = 0; i < w->unit_count; i++) {
            const Unit *f = &w->units[i];
            if (f->owner == owner || (f->flags & (UF_FLYING | UF_INVISIBLE)))
                continue;
            if (chebyshev(w, &w->units[wiz], f) <= 1 &&
                sight_visible(&sight, w, f->x, f->y)) {
                foe = i;
                break;
            }
        }
        if (foe != NO_UNIT && combat_melee(w, &t->rng, wiz, foe, &r)) {
            wiz = world_find_unit(w, wiz_id);
            if (wiz == NO_UNIT)
                return;                   /* fell to the return blow */
        }
    }

    {   /* M4h: Magic Bolt at the NEAREST visible enemy (one cast) */
        Spellbook *book = &ctx->books[owner];
        uint8_t foe = NO_UNIT, best_d = 0xFF;
        SpellShot shot;
        for (i = 0; i < w->unit_count; i++) {
            const Unit *f = &w->units[i];
            uint8_t d;
            if (f->owner == owner || (f->flags & UF_INVISIBLE) ||
                !sight_visible(&sight, w, f->x, f->y))
                continue;
            d = chebyshev(w, &w->units[wiz], f);
            if (d < best_d) {
                best_d = d;
                foe = i;
            }
        }
        if (foe != NO_UNIT && book->level[SP_MAGIC_BOLT] > 0 &&
            w->units[wiz].mana >= spell_mana(SP_MAGIC_BOLT,
                                             book->level[SP_MAGIC_BOLT]) &&
            w->units[wiz].ap >= ACTIONS[ACT_CAST].ap) {
            spell_bolt(w, book, wiz, SP_MAGIC_BOLT, w->units[foe].x,
                       w->units[foe].y, &t->rng, &shot);
            wiz = world_find_unit(w, wiz_id);
            if (wiz == NO_UNIT)
                return;
        }
    }

    {   /* D62: summon first - up to AI_SUMMON_MAX creatures, the dearest
         * he can afford while a quarter of his mana stays in reserve */
        uint8_t tried[(SPELL_COUNT + 7) / 8];
        uint8_t n = 0;
        memset(tried, 0, sizeof tried);
        for (i = 0; i < w->unit_count; i++)
            if (w->units[i].owner == owner && w->units[i].id != wiz_id)
                n++;
        while (n < AI_SUMMON_MAX && w->units[wiz].ap >= ACTIONS[ACT_CAST].ap) {
            uint8_t pick = 0xFF, k, best = 0, reserve = w->units[wiz].mana_max / 4;
            for (k = 0; k < SPELL_COUNT; k++) {
                uint8_t cost = spell_cast_mana(k, ctx->books[owner].level[k]);
                if (ctx->books[owner].level[k] == 0 ||
                    SPELLS[k].category != SPC_SUMMON ||
                    (tried[k >> 3] & (1u << (k & 7))) ||
                    w->units[wiz].mana < cost + reserve || cost <= best)
                    continue;
                best = cost;
                pick = k;
            }
            if (pick == 0xFF)
                break;
            tried[pick >> 3] |= (uint8_t)(1u << (pick & 7));
            if (spell_summon(w, &ctx->books[owner], wiz, pick) > 0)
                n++;                      /* spawning appends: wiz stays */
        }
    }

    {
        Game *g = ctx->game;
        uint8_t creatures = 0;
        int16_t tx = 0, ty = 0;
        bool chest = false, looted, leave;
        for (i = 0; i < w->unit_count; i++)
            if (w->units[i].owner == owner && w->units[i].id != wiz_id)
                creatures++;
        looted = !home_loot(w, g, owner, wiz, &tx, &ty, &chest);
        leave = looted && (creatures <= 1 || g->rage[owner]);

        if (g->portal_open && g->portal_x >= 0) {   /* escape beats all */
            uint8_t k;
            for (k = 0; k < 8; k++) {
                if (game_try_enter_portal(g, w, wiz))
                    return;
                if (w->units[wiz].ap < 4)
                    break;
                wiz = walk_to(w, &t->rng, wiz, g->portal_x, g->portal_y, 1, false);
                if (wiz == NO_UNIT)
                    return;
            }
            return;
        }
        if (!looted) {                    /* his own house first */
            wiz = walk_to(w, &t->rng, wiz, tx, ty, 8, chest);
            if (wiz == NO_UNIT)
                return;
            if (chest && world_distance(w, w->units[wiz].x, w->units[wiz].y, tx, ty) <= 1)
                ai_clear_feature(w, &t->rng, wiz, tx, ty);
            else if (!chest && w->units[wiz].x == tx && w->units[wiz].y == ty)
                items_pick_up(w, wiz);
            return;
        }
        if (!leave) {                     /* stay in: back home if outside */
            if (!at_home(w, g, owner, w->units[wiz].x, w->units[wiz].y) &&
                g->home_x[owner] != 0xFF)
                walk_to(w, &t->rng, wiz, g->home_x[owner], g->home_y[owner], 8, false);
            return;
        }
        /* out: treasure in sight, else on to the rival's house */
        if (nearest_treasure(w, wiz, &tx, &ty)) {
            wiz = walk_to(w, &t->rng, wiz, tx, ty, 8, false);
            if (wiz != NO_UNIT && w->units[wiz].x == tx && w->units[wiz].y == ty)
                items_pick_up(w, wiz);
            return;
        }
        if (rival_home(w, g, owner, wiz, &tx, &ty))
            walk_to(w, &t->rng, wiz, tx, ty, 8, true);
    }
}

/* One wizard phase (GDD 10, D62): own creatures guard or march, the
 * wizard melees an adjacent enemy, bolts the nearest one, summons, loots
 * his house and stays in until his creatures are mostly gone or a rage
 * takes him; an open portal always calls him. */
void ai_wizard_phase(Turns *t, World *w, void *ctx_ptr)
{
    AiCtx *ctx = ctx_ptr;
    if (!ctx || !ctx->books || !ctx->game)
        return;
    wizard_actions(t, w, ctx, t->phase);
    game_credit_kills(ctx->game, w);      /* the AI scores its kills too */
    turn_revalidate(t, w);
}
