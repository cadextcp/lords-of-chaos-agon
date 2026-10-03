#include "ai.h"

#include "combat.h"
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
        if (!unit_is_enemy(w, unit, i) || !melee_reachable(u, &w->units[i]))
            continue;
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
        if (u != NO_UNIT)                 /* not killed meanwhile */
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
            if (f->owner == owner || (f->flags & UF_FLYING))
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
