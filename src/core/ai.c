#include "ai.h"

#include <stdio.h>

#include "combat.h"
#include "items.h"
#include "sight.h"

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
    int8_t dx, dy;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    dx = x > u->x ? 1 : (x < u->x ? -1 : 0);
    dy = y > u->y ? 1 : (y < u->y ? -1 : 0);
    if (dx == 0 && dy == 0)
        return false;
    {
        bool was_adjacent = world_enemy_adjacent(w, unit);
        uint8_t my_id = w->units[unit].id;
        if (world_move_unit(w, unit, dx, dy)) {
            CombatResult fs;               /* D26: the player swings back */
            if (was_adjacent &&
                combat_disengage_swings(w, rng, unit, &fs) && fs.hit) {
                unit = world_find_unit(w, my_id);
                if (unit == NO_UNIT)
                    return true;           /* died on the free swing */
            }
            return true;
        }
    }
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
    if (fe == FE_CHEST)
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

void ai_run_hunters(World *w, Rng *rng, uint8_t owner, uint8_t skip_id)
{
    uint8_t ids[MAX_UNITS], n = 0, i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner && w->units[i].id != skip_id)
            ids[n++] = w->units[i].id;
    for (i = 0; i < n; i++) {
        uint8_t u = world_find_unit(w, ids[i]);
        if (u == NO_UNIT)                 /* killed meanwhile */
            continue;
        if (w->units[u].post_x != 0xFF)
            ai_guard(w, rng, u, 3);       /* map-defined guards hold (M4h) */
        else
            ai_hunter(w, rng, u);
    }
}

/* The wizard's own actions; returns early once he is gone. Unit indices
 * change with every death, so the wizard is re-found by id after each
 * step that may kill. */
static void wizard_actions(Turns *t, World *w, AiCtx *ctx, uint8_t owner)
{
    static Sight sight;
    uint8_t wiz = NO_UNIT, wiz_id, i, own = 0;

    for (i = 0; i < w->unit_count; i++) {
        if (w->units[i].owner != owner)
            continue;
        own++;
        if (w->units[i].kind == CR_WIZARD)
            wiz = i;
    }
    if (wiz == NO_UNIT) {                 /* leaderless creatures still hunt */
        ai_run_hunters(w, &t->rng, owner, NO_UNIT);
        return;
    }
    wiz_id = w->units[wiz].id;
    ai_run_hunters(w, &t->rng, owner, wiz_id);   /* own creatures first */
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

    {   /* M4k: open an adjacent chest - the loot lies on the field and
         * is picked up by the treasure walk below (or next round) */
        static const int8_t DX2[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int8_t DY2[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        uint8_t d;
        for (d = 0; d < 8; d++)
            if (world_feature(w, (int16_t)(w->units[wiz].x + DX2[d]),
                              (int16_t)(w->units[wiz].y + DY2[d])) == FE_CHEST) {
                                ai_clear_feature(w, &t->rng, wiz,
                                 (int16_t)(w->units[wiz].x + DX2[d]),
                                 (int16_t)(w->units[wiz].y + DY2[d]));
                break;
            }
    }

    {   /* M4h: walk to the nearest treasure in sight and take it */
        int16_t tx, ty;
        uint8_t steps;
        if (nearest_treasure(w, wiz, &tx, &ty)) {
            for (steps = 0; steps < 2; steps++)
                if (w->units[wiz].ap >= 4 &&
                    ai_step_toward(w, &t->rng, wiz, tx, ty))
                    break;
            if (w->units[wiz].x != tx || w->units[wiz].y != ty) {
                /* stuck: open a door/chest between wizard and treasure */
                int8_t dx = tx > w->units[wiz].x ? 1 : (tx < w->units[wiz].x ? -1 : 0);
                int8_t dy = ty > w->units[wiz].y ? 1 : (ty < w->units[wiz].y ? -1 : 0);
                ai_clear_feature(w, &t->rng, wiz,
                                 (int16_t)(w->units[wiz].x + dx),
                                 (int16_t)(w->units[wiz].y + dy));
            }
            if (w->units[wiz].x == tx && w->units[wiz].y == ty)
                items_pick_up(w, wiz);
        }
    }

    own = 0;                              /* recount after the hunt */
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner)
            own++;
    while (own < 3 && w->units[wiz].ap >= ACTIONS[ACT_CAST].ap) {
        uint8_t pick = 0xFF, k, best = 0xFF;   /* summon company */
        for (k = 0; k < SPELL_COUNT; k++) {
            uint8_t cost = spell_cast_mana(k, ctx->books[owner].level[k]);
            if (ctx->books[owner].level[k] == 0 ||
                SPELLS[k].category != SPC_SUMMON ||
                w->units[wiz].mana < cost || cost >= best)
                continue;
            best = cost;
            pick = k;
        }
        if (pick == 0xFF || spell_summon(w, &ctx->books[owner], wiz, pick) == 0)
            break;                        /* spawning appends: wiz stays */
        own++;
    }

    if (ctx->game->portal_x >= 0) {
        uint8_t k;
        for (k = 0; k < 8; k++) {         /* walk, and step through */
            if (w->units[wiz].ap < 4)
                break;
            if (game_try_enter_portal(ctx->game, w, wiz))
                return;
            if (!ai_step_toward(w, &t->rng, wiz, ctx->game->portal_x,
                                     ctx->game->portal_y))
                break;
        }
    }
}

/* One wizard phase (GDD 10): own creatures hunt, then the wizard melees
 * an adjacent enemy, summons while under company and walks to the
 * portal - through it as soon as it is open. */
void ai_wizard_phase(Turns *t, World *w, void *ctx_ptr)
{
    AiCtx *ctx = ctx_ptr;
    if (!ctx || !ctx->books || !ctx->game)
        return;
    wizard_actions(t, w, ctx, t->phase);
    game_credit_kills(ctx->game, w);      /* the AI scores its kills too */
    turn_revalidate(t, w);
}
