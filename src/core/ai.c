#include "ai.h"

#include "combat.h"
#include "items.h"
#include "sight.h"

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

bool ai_step_toward(World *w, uint8_t unit, int16_t x, int16_t y)
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
    (void)home_range;                    /* the post itself is the anchor */
    int16_t dx, dy;
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
            if (ai_step_toward(w, unit, w->units[unit].post_x,
                               w->units[unit].post_y))
                break;
    }
}

/* Nearest treasure on the ground the unit can see (simple: on its
 * field or within sight ray, M4h). Returns the kind, NO_ITEM if none. */
static uint8_t nearest_treasure(World *w, uint8_t unit)
{
    uint8_t i, k, kind, best = NO_ITEM;
    int16_t bx = 0, by = 0;
    uint8_t best_d = 9 + 1;
    Unit *u = &w->units[unit];
    for (i = 0; i < w->object_count; i++) {
        int16_t ddx, ddy, d;
        uint8_t o;
        kind = NO_ITEM;
        for (k = 0; k < OBJ_COUNT; k++)
            if (OBJECTS[k].tile == w->objects[i].tile)
                kind = k;
        if (kind == NO_ITEM || OBJECTS[kind].category != OC_TREASURE)
            continue;
        ddx = (int16_t)(w->objects[i].x - u->x);
        ddy = (int16_t)(w->objects[i].y - u->y);
        if (w->wrap) {
            if (ddx > w->w / 2) ddx = (int16_t)(ddx - w->w);
            if (ddx < -w->w / 2) ddx = (int16_t)(ddx + w->w);
            if (ddy > w->h / 2) ddy = (int16_t)(ddy - w->h);
            if (ddy < -w->h / 2) ddy = (int16_t)(ddy + w->h);
        }
        d = (int16_t)((ddx < 0 ? -ddx : ddx) > (ddy < 0 ? -ddy : ddy)
                          ? (ddx < 0 ? -ddx : ddx) : (ddy < 0 ? -ddy : ddy));
        if (d >= best_d || d > 9)
            continue;
        o = unit;
        if (!sight_has_los(w, u->x, u->y, w->objects[i].x, w->objects[i].y))
            continue;
        best = kind;
        bx = w->objects[i].x;
        by = w->objects[i].y;
        best_d = (uint8_t)d;
        (void)o;
    }
    if (best != NO_ITEM && u->x == bx && u->y == by)
        items_pick_up(w, unit);         /* stand on it: take it */
    return best;                        /* 0xFF never used as a kind */
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
        if (!ai_step_toward(w, unit, w->units[prey].x, w->units[prey].y))
            return;                     /* stuck */
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

    {   /* M4h: cast the cheapest attack spell at a visible enemy */
        Spellbook *book = &ctx->books[owner];
        uint8_t foe = NO_UNIT, k, bolt = 0xFF;
        int16_t fx = 0, fy = 0;
        SpellShot shot;
        for (i = 0; i < w->unit_count && bolt == 0xFF; i++) {
            const Unit *f = &w->units[i];
            if (f->owner == owner || (f->flags & UF_INVISIBLE))
                continue;
            if (sight_visible(&sight, w, f->x, f->y)) {
                fx = f->x;
                fy = f->y;
                /* pick the bolt if known and affordable */
                if (book->level[SP_MAGIC_BOLT] > 0 &&
                    w->units[wiz].mana >= spell_mana(SP_MAGIC_BOLT,
                                                     book->level[SP_MAGIC_BOLT]) &&
                    w->units[wiz].ap >= ACTIONS[ACT_CAST].ap) {
                    if (spell_bolt(w, book, wiz, SP_MAGIC_BOLT, fx, fy,
                                   &t->rng, &shot) && shot.hit) {
                        wiz = world_find_unit(w, wiz_id);
                        if (wiz == NO_UNIT)
                            return;
                        break;
                    }
                    wiz = world_find_unit(w, wiz_id);
                    if (wiz == NO_UNIT)
                        return;
                }
                (void)foe;
                (void)k;
                bolt = 0xFF;              /* one target per round keeps it simple */
            }
        }
    }

    {   /* M4h: grab a treasure the wizard can see */
        uint8_t kind = nearest_treasure(w, wiz);
        int16_t tx, ty;
        uint8_t steps;
        if (kind != NO_ITEM) {
            /* walk one step toward the remembered target field */
            for (i = 0; i < w->object_count; i++) {
                uint8_t k2;
                for (k2 = 0; k2 < OBJ_COUNT; k2++)
                    if (OBJECTS[k2].tile == w->objects[i].tile &&
                        OBJECTS[k2].category == OC_TREASURE) {
                        tx = w->objects[i].x;
                        ty = w->objects[i].y;
                        for (steps = 0; steps < 2; steps++)
                            if (w->units[wiz].ap >= 4 &&
                                ai_step_toward(w, wiz, tx, ty))
                                break;
                        if (w->units[wiz].x == tx && w->units[wiz].y == ty)
                            items_pick_up(w, wiz);
                        i = w->object_count;   /* first treasure only */
                        break;
                    }
            }
            wiz = world_find_unit(w, wiz_id);
            if (wiz == NO_UNIT)
                return;
        }
    }

    own = 0;                              /* recount after the hunt */
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner)
            own++;
    while (own < 3 && w->units[wiz].ap >= ACTIONS[ACT_CAST].ap) {
        uint8_t pick = 0xFF, k, best = 0xFF;   /* summon company */
        for (k = 0; k < SPELL_COUNT; k++) {
            uint8_t cost = spell_mana(k, ctx->books[owner].level[k]);
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
            if (!ai_step_toward(w, wiz, ctx->game->portal_x, ctx->game->portal_y))
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
