/* The AI wizard (D62, D67): his house, his loot, his spells, his walk. The
 * creatures of an AI wizard run through ai_creature_turn. */
#include "ai_priv.h"

#include <stdio.h>
#include <string.h>

#include "combat.h"
#include "ride.h"
#include "sight.h"

#define HOME_RANGE 6        /* the house: roofed fields this close to home */
#define AI_SUMMON_MAX 5     /* creatures he summons up to */
#define AI_RAGE_CHANCE 12   /* one round in 12 a rage starts ... */
#define AI_RAGE_ROUNDS 3    /* ... and lasts this long */

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
    ai_run_creatures(w, &t->rng, ctx->game, owner, wiz_id);   /* creatures first */
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
                sight_unit_visible(&sight, w, f)) {
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
                !sight_unit_visible(&sight, w, f))
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
            world_can_pay(w, wiz, ACT_CAST)) {
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
        while (n < AI_SUMMON_MAX && world_can_pay(w, wiz, ACT_CAST)) {
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
            n = (uint8_t)(n + spell_summon(w, &ctx->books[owner], wiz, pick, &t->rng));   /* spawning appends: wiz stays */
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
                wiz = ai_walk_to(w, &t->rng, wiz, g->portal_x, g->portal_y, 1, false);
                if (wiz == NO_UNIT)
                    return;
            }
            return;
        }
        if (!looted) {                    /* his own house first */
            wiz = ai_walk_to(w, &t->rng, wiz, tx, ty, 8, chest);
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
                ai_walk_to(w, &t->rng, wiz, g->home_x[owner], g->home_y[owner], 8, false);
            return;
        }
        /* out: treasure in sight, else on to the rival's house */
        if (nearest_treasure(w, wiz, &tx, &ty)) {
            wiz = ai_walk_to(w, &t->rng, wiz, tx, ty, 8, false);
            if (wiz != NO_UNIT && w->units[wiz].x == tx && w->units[wiz].y == ty)
                items_pick_up(w, wiz);
            return;
        }
        if (rival_home(w, g, owner, wiz, &tx, &ty))
            ai_walk_to(w, &t->rng, wiz, tx, ty, 8, true);
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

/* Every creature of an AI wizard (all units of `owner` but the skipped one):
 * the decision loop of K10.3. Works on an id snapshot, so kills that reorder
 * the unit list neither skip a creature nor let one act twice. */
void ai_run_creatures(World *w, Rng *rng, Game *g, uint8_t owner, uint8_t skip_id)
{
    AiEnv env;
    uint8_t ids[MAX_UNITS], n = 0, i;
    env.game = g;
    env.round = 0;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner && w->units[i].id != skip_id)
            ids[n++] = w->units[i].id;
    for (i = 0; i < n; i++)
        if (world_find_unit(w, ids[i]) != NO_UNIT)
            ai_creature_turn(w, rng, &env, ids[i]);
}
