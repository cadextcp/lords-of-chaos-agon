/* The AI wizard (K10.2, K10.5, D67): scenario profile, spell choice by
 * priority, and the phase that runs him and his creatures through the same
 * decision loop (ai_creature.c). */
#include "ai_priv.h"

#include <string.h>

#include "area.h"
#include "brew.h"
#include "combat.h"
#include "effect.h"
#include "ride.h"
#include "sight.h"

/* ---------- scenario file v2: profile, routes, plans, triggers ---------- */

bool ai_scenario_load(World *w, AiProfile *profiles, const uint8_t *d, uint16_t len)
{
    uint16_t pos = 6;
    uint8_t n, i, k;
    memset(profiles, 0, sizeof(AiProfile) * OWN_NEUTRAL);
    if (len < 6 || memcmp(d, "LOCS", 4) != 0 || (d[4] != 2 && d[4] != 3))
        return false;
    n = d[5];
    for (i = 0; i < n; i++) {            /* books: spellbook_load reads them */
        if (pos + 2 > len)
            return false;
        pos = (uint16_t)(pos + 2 + 2u * d[pos + 1]);
    }
    if (pos >= len)
        return false;
    n = d[pos++];
    for (i = 0; i < n; i++) {
        AiProfile *p;
        uint8_t owner, pc;
        if (pos + 21 > len)
            return false;
        owner = d[pos++];
        if (owner >= OWN_NEUTRAL)
            return false;
        p = &profiles[owner];
        p->present = true;
        memcpy(p->name, &d[pos], 10);
        p->name[10] = 0;
        pos += 10;
        p->mana = d[pos]; p->ap = d[pos + 1]; p->sta = d[pos + 2]; p->con = d[pos + 3];
        p->com = d[pos + 4]; p->def = d[pos + 5]; p->mr = d[pos + 6]; p->carry = d[pos + 7];
        p->vp = d[pos + 8];
        pos += 9;
        pc = d[pos++];
        if (pos + 2u * pc > len)
            return false;
        for (k = 0; k < pc; k++) {
            if (d[pos] >= SPELL_COUNT)
                return false;
            p->prio[d[pos]] = d[pos + 1];
            pos += 2;
        }
    }
    if (pos + 2 > len)
        return false;
    w->route_n = d[pos++];
    w->route_summon_n = d[pos++];
    if (w->route_n > ROUTES_MAX)
        return false;
    for (i = 0; i < w->route_n; i++) {
        Route *r = &w->routes[i];
        if (pos + 2 > len)
            return false;
        r->flags = d[pos++];
        r->n = d[pos++];
        if (r->n > ROUTE_WP_MAX || pos + 2u * r->n > len)
            return false;
        for (k = 0; k < r->n; k++) {
            r->x[k] = d[pos++];
            r->y[k] = d[pos++];
        }
    }
    if (pos >= len)
        return false;
    n = d[pos++];
    if (pos + 4u * n > len)
        return false;
    for (i = 0; i < n; i++) {            /* plans of map units */
        uint8_t u = d[pos];
        if (u < w->unit_count) {
            w->units[u].plan_route = d[pos + 1];
            w->units[u].plan_step = d[pos + 2];
            w->units[u].plan_flags = d[pos + 3];
        }
        pos += 4;
    }
    if (pos >= len)
        return false;
    n = d[pos++];
    if (n > TRIGGERS_MAX || pos + 3u * n > len)
        return false;
    w->trig_n = n;
    for (i = 0; i < n; i++) {
        w->trig_x[i] = d[pos];
        w->trig_y[i] = d[pos + 1];
        w->trig_id[i] = d[pos + 2];
        pos += 3;
    }
    memset(w->trig_fired, 0, sizeof w->trig_fired);
    if (d[4] >= 3) {                     /* v3: the AP factor (D71) */
        if (pos >= len)
            return false;
        world_set_ap_scale(w, d[pos]);
    }
    return true;
}

void ai_profile_apply(const AiProfile *p, World *w, Game *g, uint8_t owner)
{
    uint8_t wi = ai_wizard_of(w, owner);
    Unit *u;
    if (wi == NO_UNIT || owner >= OWN_NEUTRAL || !p[owner].present)
        return;
    u = &w->units[wi];
    u->com = p[owner].com;
    u->def = p[owner].def;
    u->mr = p[owner].mr;
    u->con = u->con_max = p[owner].con;
    u->sta = u->sta_max = p[owner].sta;
    u->ap = u->ap_max = world_scale_ap(w, p[owner].ap);   /* D71 */
    u->mana = u->mana_max = p[owner].mana;
    if (g)
        g->wizard_vp[owner] = p[owner].vp;
}

/* ---------- spell choice (K10.5) ---------- */

static uint8_t free_neighbours(const World *w, const Unit *u)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t i, n = 0;
    for (i = 0; i < 8; i++) {
        int16_t x = (int16_t)(u->x + DX[i]), y = (int16_t)(u->y + DY[i]);
        if (world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
            world_unit_at(w, x, y, UL_GROUND) == NO_UNIT)
            n++;
    }
    return n;
}

/* An own creature (not the wizard) of `owner` closer than `d` units to (x, y)? */
static bool own_creature_near(const World *w, uint8_t owner, uint8_t wiz_id,
                              int16_t x, int16_t y, uint16_t d, bool inclusive)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *u = &w->units[i];
        uint16_t r;
        if (u->owner != owner || u->id == wiz_id)
            continue;
        r = world_range(w, x, y, u->x, u->y);
        if (inclusive ? r <= d : r < d)
            return true;
    }
    return false;
}

/* Try to cast spell `s` now; true when the mana was spent. */
static bool try_cast(World *w, Rng *rng, const AiEnv *env, uint8_t id, uint8_t s,
                     const AiView *v)
{
    uint8_t wi = world_find_unit(w, id), level, i;
    Unit *u;
    Spellbook *book = env->book;
    SpellShot shot;
    if (wi == NO_UNIT)
        return false;
    u = &w->units[wi];
    level = book->level[s];
    if (!spell_can_cast(w, book, wi, s))
        return false;
    if (SPELLS[s].category == SPC_SUMMON) {
        uint8_t kind = SUMMON_KIND[s];
        uint8_t mana_after = (uint8_t)(u->mana - spell_mana(s, level));
        if (v->en_n == 0 && mana_after < 40)
            return false;                /* keeps a reserve of 40 in peace */
        if (free_neighbours(w, u) < level)
            return false;
        if ((kind == CR_GOLD_DRAGON || kind == CR_GREEN_DRAGON || kind == CR_RED_DRAGON) &&
            !brew_dragon_ready(w, wi))
            return false;
        spell_summon(w, book, wi, s, rng);
        return true;
    }
    if (SPELLS[s].category == SPC_POTION) {
        Cauldron *c = brew_cauldron_at(w, u->x, u->y);
        if (s == SP_BOMB_POTION || !c || c->potion != 0xFF)
            return false;                /* an empty cauldron under him */
        if (!brew_cast(w, book, wi, s)) {
            for (i = 0; i < u->item_count; i++)      /* put the ingredient down first */
                if (brew_ingredient_potion(u->items[i]) == s) {
                    u->in_use = i;
                    if (items_drop(w, wi))
                        return brew_cast(w, book, wi, s);
                    break;
                }
            return false;
        }
        return true;
    }
    if (s == SP_MAGIC_SHIELD) {
        if (effect_active(u, EFF_SHIELD))
            return false;
        return spell_apply(w, book, wi, s, u->x, u->y, false, rng, &shot) == CAST_OK;
    }
    if (s == SP_MAGIC_BOLT || s == SP_MAGIC_LIGHTNING) {
        uint8_t attack = spell_attack_value(s, level);
        for (i = 0; i < v->en_n; i++) {
            uint8_t ei = world_find_unit(w, v->en[i].id);
            const Unit *e;
            if (ei == NO_UNIT)
                continue;
            e = &w->units[ei];
            if (e->flags & UF_RIDDEN)
                continue;
            if (v->en[i].dist > 2 * level + 6 || 4u * attack < 3u * v->en[i].d_eff)
                continue;
            if (s == SP_MAGIC_LIGHTNING &&
                own_creature_near(w, u->owner, u->id, e->x, e->y, 4, false))
                continue;
            {   /* aimed at the enemy's own height (CAST-A/G, F8) */
                bool air = (e->flags & UF_FLYING) != 0;
                if (s == SP_MAGIC_LIGHTNING
                        ? spell_lightning(w, book, wi, e->x, e->y, air, rng, &shot)
                        : spell_bolt(w, book, wi, s, e->x, e->y, air, rng, &shot))
                    return true;
            }
        }
        return false;
    }
    if (s == SP_MAGIC_FIRE || s == SP_GOOEY_BLOB || s == SP_TANGLE_VINE || s == SP_FLOOD) {
        AreaKind kind = s == SP_MAGIC_FIRE ? AREA_FIRE : s == SP_GOOEY_BLOB ? AREA_BLOB
                      : s == SP_TANGLE_VINE ? AREA_VINE : AREA_FLOOD;
        for (i = 0; i < v->en_n; i++) {
            uint8_t ei = world_find_unit(w, v->en[i].id);
            const Unit *e;
            if (ei == NO_UNIT)
                continue;
            e = &w->units[ei];
            if (v->en[i].dist > 2 * level + 6 ||
                area_susceptibility(w, kind, e->x, e->y) == 0 ||
                area_kind_at(w, e->x, e->y) != AREA_NONE)
                continue;
            if ((kind == AREA_VINE || kind == AREA_FLOOD) &&
                own_creature_near(w, u->owner, u->id, e->x, e->y, (uint16_t)(2 * level + 1), true))
                continue;
            if (spell_apply(w, book, wi, s, e->x, e->y, false, rng, &shot) == CAST_OK)
                return true;
        }
        return false;
    }
    return false;                        /* Enchant, Subversion, Curse, Teleport, Eye: never */
}

AiAct ai_wizard_cast(World *w, Rng *rng, const AiEnv *env, uint8_t id, const AiView *v)
{
    uint8_t wi = world_find_unit(w, id), s;
    uint8_t *pr;
    const Unit *u;
    if (wi == NO_UNIT || !env || !env->profile || !env->book)
        return A_NONE;
    u = &w->units[wi];
    if (!world_can_pay(w, wi, ACT_CAST))
        return A_NONE;
    if (v->en_n == 0 && 2u * u->ap < u->ap_max)
        return A_NONE;                   /* in peace only with half his AP */
    pr = env->profile->prio;
    for (;;) {
        uint8_t pick = 0xFF, best = 0;
        for (s = 0; s < SPELL_COUNT; s++)
            if (pr[s] > best && !(pr[s] & 1)) {
                best = pr[s];
                pick = s;
            }
        if (pick == 0xFF)
            break;
        if (try_cast(w, rng, env, id, pick, v)) {
            pr[pick] = (uint8_t)(pr[pick] >> 1);   /* halved for good (K10.5) */
            return A_DONE;
        }
        pr[pick] |= 1;                   /* checked this pass, not cast */
    }
    for (s = 0; s < SPELL_COUNT; s++)    /* a pass without a cast rounds odd values down */
        pr[s] &= (uint8_t)~1u;
    return A_NONE;
}

/* ---------- phase ---------- */

/* Without a scenario profile (tests, maps without an AI table) the wizard gets a
 * sensible priority table from his book: bolt and lightning first, then
 * potions, summons, area spells, shield. Kept per owner so the halving
 * persists. */
static AiProfile fallback[OWN_NEUTRAL];
static uint8_t seeded[OWN_NEUTRAL][(SPELL_COUNT + 7) / 8];   /* priorities given so far */

static AiProfile *profile_for(const AiCtx *ctx, uint8_t owner, const Spellbook *book)
{
    AiProfile *p;
    uint8_t s;
    if (ctx->profiles && ctx->profiles[owner].present)
        return &ctx->profiles[owner];
    p = &fallback[owner];
    p->present = true;
    for (s = 0; s < SPELL_COUNT; s++) {          /* a spell new in the book gets its default */
        uint8_t cat = SPELLS[s].category;
        if (!book->level[s] || (seeded[owner][s >> 3] & (1u << (s & 7))))
            continue;
        seeded[owner][s >> 3] |= (uint8_t)(1u << (s & 7));
        p->prio[s] = s == SP_MAGIC_LIGHTNING ? 220 : s == SP_MAGIC_BOLT ? 200
                   : cat == SPC_POTION ? 150 : cat == SPC_SUMMON ? 100
                   : cat == SPC_AREA ? 100 : s == SP_MAGIC_SHIELD ? 60 : 0;
    }
    return p;
}

/* One wizard phase: the wizard first (he is position set 1 in the original),
 * then his creatures, then the score. */
void ai_wizard_phase(Turns *t, World *w, void *ctx_ptr)
{
    AiCtx *ctx = ctx_ptr;
    uint8_t owner, wi, wiz_id = NO_UNIT;
    AiEnv env;
    if (!ctx || !ctx->books || !ctx->game)
        return;
    owner = t->phase;
    memset(&env, 0, sizeof env);
    env.game = ctx->game;
    env.round = t->round;
    env.book = &ctx->books[owner];
    env.profile = profile_for(ctx, owner, env.book);
    wi = ai_wizard_of(w, owner);
    if (wi != NO_UNIT) {
        wiz_id = w->units[wi].id;
        if (w->units[wi].plan_route == 0xFF && w->route_n)
            ai_plan_new(w, &t->rng, wi);       /* his own route at the start */
        ai_creature_turn(w, &t->rng, &env, wiz_id);
    }
    {   /* the creatures: the same env (portal), no spells */
        AiEnv cenv = env;
        cenv.book = NULL;
        cenv.profile = NULL;
        {
            uint8_t ids[MAX_UNITS], n = 0, i;
            for (i = 0; i < w->unit_count; i++)
                if (w->units[i].owner == owner && w->units[i].id != wiz_id)
                    ids[n++] = w->units[i].id;
            for (i = 0; i < n; i++)
                if (world_find_unit(w, ids[i]) != NO_UNIT)
                    ai_creature_turn(w, &t->rng, &cenv, ids[i]);
        }
    }
    game_credit_kills(ctx->game, w);
    turn_revalidate(t, w);
}
