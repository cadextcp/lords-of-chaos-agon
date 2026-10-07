/* The decision loop of an AI creature (K10.3, D67): sleep, immediate actions,
 * ranged attack, flight, melee target, object target, route or hunt, step.
 * One creature at a time, at most 50 passes. */
#include "ai_priv.h"

#include <string.h>

#include "area.h"
#include "combat.h"
#include "effect.h"
#include "ride.h"
#include "sight.h"

#define AI_PASSES 50

/* The eight neighbours in the original's order: W, SW, S, SE, E, NE, N, NW. */
static const int8_t ND[8][2] = {{-1, 0}, {-1, 1}, {0, 1}, {1, 1},
                                {1, 0}, {1, -1}, {0, -1}, {-1, -1}};

typedef enum { GT_NONE, GT_MELEE, GT_ITEM, GT_PORTAL, GT_FOLLOW, GT_POST } GoalKind;

static uint8_t clamp8(uint16_t v)
{
    return v > 255 ? 255 : (uint8_t)v;
}

uint8_t ai_wizard_of(const World *w, uint8_t owner)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner && ride_actor_kind(&w->units[i]) == CR_WIZARD)
            return i;
    return NO_UNIT;
}

bool ai_is_foe(const World *w, const Unit *me, const Unit *e)
{
    if (e->owner == me->owner)
        return false;
    if (e->owner < OWN_NEUTRAL)
        return true;
    if (me->owner == OWN_NEUTRAL)
        return false;                    /* the wild do not fight each other */
    return combat_hostile_to(w, e, me->owner, me->x, me->y);
}

void ai_build_view(const World *w, uint8_t unit, AiView *v)
{
    const Unit *me = &w->units[unit];
    uint8_t i;
    v->en_n = v->it_n = 0;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *e = &w->units[i];
        AiEnemy ent;
        uint8_t k, pos;
        if (i == unit || !ai_is_foe(w, me, e) || (e->flags & UF_INVISIBLE))
            continue;
        if (!sight_sees(w, me, e->x, e->y, (e->flags & UF_FLYING) != 0, false))
            continue;
        ent.id = e->id;
        ent.dist = clamp8(world_range(w, me->x, me->y, e->x, e->y));
        ent.c_eff = items_combat(w, i);
        ent.d_eff = items_defence(w, i);
        for (pos = 0; pos < v->en_n && v->en[pos].dist <= ent.dist; pos++)
            ;
        if (pos >= AI_ENEMY_MAX)
            continue;
        if (v->en_n < AI_ENEMY_MAX)
            v->en_n++;
        for (k = (uint8_t)(v->en_n - 1); k > pos; k--)
            v->en[k] = v->en[k - 1];
        v->en[pos] = ent;
    }
    for (i = 0; i < w->object_count; i++) {
        const Object *o = &w->objects[i];
        AiItem it;
        uint8_t kind = items_kind_of_tile(o->tile), k, pos;
        if (kind == NO_ITEM || (AI_ITEM[kind] & 0x7F) == 0 ||
            !sight_sees(w, me, o->x, o->y, false, true))
            continue;
        it.x = o->x;
        it.y = o->y;
        it.kind = kind;
        it.dist = clamp8(world_range(w, me->x, me->y, o->x, o->y));
        for (pos = 0; pos < v->it_n && v->it[pos].dist <= it.dist; pos++)
            ;
        if (pos >= AI_ITEM_MAX)
            continue;
        if (v->it_n < AI_ITEM_MAX)
            v->it_n++;
        for (k = (uint8_t)(v->it_n - 1); k > pos; k--)
            v->it[k] = v->it[k - 1];
        v->it[pos] = it;
    }
    if (CREATURES[ride_actor_kind(me)].flags & CF_USE) {   /* chests: those that can open them */
        int16_t dx, dy;
        for (dy = -9; dy <= 9; dy++)
            for (dx = -9; dx <= 9; dx++) {
                int16_t cx = (int16_t)(me->x + dx), cy = (int16_t)(me->y + dy);
                uint8_t fe;
                if (!sight_in_reach(dx, dy, SIGHT_R_GROUND) || !world_wrap(w, &cx, &cy))
                    continue;
                fe = w->feature[cy][cx];
                if ((fe == FE_CHEST || fe == FE_CHEST_FREE) && v->it_n < AI_ITEM_MAX &&
                    sight_sees(w, me, cx, cy, false, true)) {
                    v->it[v->it_n].x = cx;
                    v->it[v->it_n].y = cy;
                    v->it[v->it_n].kind = AI_CHEST;
                    v->it[v->it_n].dist = clamp8(world_range(w, me->x, me->y, cx, cy));
                    v->it_n++;
                }
            }
    }
}

static Unit *by_id(World *w, uint8_t id, uint8_t *idx)
{
    *idx = world_find_unit(w, id);
    return *idx == NO_UNIT ? NULL : &w->units[*idx];
}

static bool aggressive(const Unit *u)
{
    return u->owner < OWN_NEUTRAL && (u->plan_route == 0xFF || (u->plan_flags & 0x80));
}

static bool is_wizard(const Unit *u)
{
    return ride_actor_kind(u) == CR_WIZARD;
}

/* ---------- targets (K10.4) ---------- */

/* Can `me` hurt `e` in melee, and is it worth it: 2 C_me >= 1.5 Def_e. */
static bool melee_worth(const World *w, uint8_t ui, const AiEnemy *e, uint8_t ei)
{
    const Unit *me = &w->units[ui], *en = &w->units[ei];
    if (4u * items_combat(w, ui) < 3u * e->d_eff)
        return false;
    if ((en->flags & UF_FLYING) && !(me->flags & UF_FLYING))
        return false;                    /* nothing reaches a flier from the ground */
    if (!items_can_harm_undead(w, ui, ei))
        return false;
    return true;
}

static bool is_threat(const World *w, uint8_t ui, const AiEnemy *e, uint8_t ei)
{
    const Unit *me = &w->units[ui], *en = &w->units[ei];
    if (4u * items_defence(w, ui) >= 3u * e->c_eff)
        return false;
    if ((me->flags & UF_FLYING) && !(en->flags & UF_FLYING))
        return false;                    /* a flier ignores those on the ground */
    if ((me->flags & UF_UNDEAD) && !(en->flags & UF_UNDEAD) &&
        !items_can_harm_undead(w, ei, ui))
        return false;                    /* the undead fear only the undead and magic */
    return true;
}

static bool field_free_for(const World *w, const Unit *me, int16_t x, int16_t y)
{
    if (!world_wrap(w, &x, &y))
        return false;
    if (world_blocks(w, x, y) && !(CREATURES[me->kind].flags & CF_PHASE))
        return false;
    if (area_blocks_kind(w, x, y))
        return false;
    return world_blocking_unit_at(w, x, y, (me->flags & UF_FLYING) ? UL_AIR : UL_GROUND,
                                  me->owner) == NO_UNIT;
}

static bool endangered(const World *w, const Unit *me, int16_t x, int16_t y, const AiView *v)
{
    uint8_t i;
    if (!field_free_for(w, me, x, y))
        return true;
    for (i = 0; i < v->en_n; i++) {
        uint8_t ei = world_find_unit(w, v->en[i].id);
        const Unit *e;
        if (ei == NO_UNIT)
            continue;
        e = &w->units[ei];
        if (sight_shot_clear(w, e->x, e->y, (e->flags & UF_FLYING) != 0, x, y,
                             (me->flags & UF_FLYING) != 0))
            return true;
    }
    return false;
}

/* Flee (K10.4): the first neighbour out of every enemy's line, else the one
 * farthest from all threats. Returns A_END after the step, A_NONE if there is
 * nothing to flee from or nowhere to go. */
static AiAct do_flee(World *w, uint8_t id, const AiView *v)
{
    uint8_t ui, i, d, threats = 0;
    Unit *u = by_id(w, id, &ui);
    int8_t bx = 0, by = 0;
    uint16_t best = 0;
    bool safe = false;
    if (!u || aggressive(u) || world_engaged(w, ui))
        return A_NONE;
    for (i = 0; i < v->en_n; i++) {
        uint8_t ei = world_find_unit(w, v->en[i].id);
        if (ei != NO_UNIT && is_threat(w, ui, &v->en[i], ei))
            threats++;
    }
    if (!threats)
        return A_NONE;
    for (d = 0; d < 8 && !safe; d++) {
        int16_t nx = (int16_t)(u->x + ND[d][0]), ny = (int16_t)(u->y + ND[d][1]);
        if (!world_wrap(w, &nx, &ny) || endangered(w, u, nx, ny, v))
            continue;
        bx = ND[d][0];
        by = ND[d][1];
        safe = true;
    }
    if (!safe)
        for (d = 0; d < 8; d++) {
            int16_t nx = (int16_t)(u->x + ND[d][0]), ny = (int16_t)(u->y + ND[d][1]);
            uint16_t sum = 0;
            if (!world_wrap(w, &nx, &ny) || !field_free_for(w, u, nx, ny))
                continue;
            for (i = 0; i < v->en_n; i++) {
                uint8_t ei = world_find_unit(w, v->en[i].id);
                if (ei != NO_UNIT && is_threat(w, ui, &v->en[i], ei))
                    sum = (uint16_t)(sum + world_range(w, nx, ny, w->units[ei].x, w->units[ei].y));
            }
            if (sum > 255)
                sum = 255;
            if (sum >= best) {
                best = sum;
                bx = ND[d][0];
                by = ND[d][1];
                safe = true;
            }
        }
    if (!safe)
        return A_NONE;
    {
        int16_t ox = u->x, oy = u->y;
        if (!world_move_unit(w, ui, bx, by))
            return A_NONE;
        ai_visit(w, ui, ox, oy);
    }
    return A_END;                        /* arrived: its turn is over */
}

/* Fire at the nearest worthwhile target in reach (K10.4). */
static AiAct do_ranged(World *w, Rng *rng, uint8_t id, const AiView *v)
{
    uint8_t ui, i, attack;
    Unit *u = by_id(w, id, &ui);
    bool dragon;
    if (!u || !items_can_fire(w, ui) || !world_can_pay(w, ui, ACT_FIRE))
        return A_NONE;
    dragon = ride_actor_kind(u) == CR_GOLD_DRAGON || ride_actor_kind(u) == CR_GREEN_DRAGON ||
             ride_actor_kind(u) == CR_RED_DRAGON;
    attack = dragon ? 35 : ((u->flags & UF_MAGIC_WEAPON) ? 2 * BOW_ATTACK : BOW_ATTACK);
    for (i = 0; i < v->en_n; i++) {
        uint8_t ei = world_find_unit(w, v->en[i].id);
        const Unit *e;
        bool air;
        if (ei == NO_UNIT)
            continue;
        e = &w->units[ei];
        air = (e->flags & UF_FLYING) != 0;
        if (4u * attack < 3u * v->en[i].d_eff || v->en[i].dist >= items_fire_range(w, ui))
            continue;
        if (e->flags & UF_UNDEAD) {
            if (dragon) {
                if (air || area_susceptibility(w, AREA_FIRE, e->x, e->y) == 0)
                    continue;
            } else if (!items_can_harm_undead(w, ui, ei))
                continue;
        }
        if (!sight_shot_clear(w, u->x, u->y, (u->flags & UF_FLYING) != 0, e->x, e->y, air))
            continue;
        if (items_fire(w, rng, ui, e->x, e->y, NULL))
            return A_DONE;
    }
    return A_NONE;
}

/* ---------- stepping (K10.3 step 6) ---------- */

static bool friend_here(const World *w, uint8_t ui)
{
    const Unit *u = &w->units[ui];
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (i != ui && w->units[i].x == u->x && w->units[i].y == u->y &&
            w->units[i].owner == u->owner &&
            ((w->units[i].flags & UF_FLYING) == (u->flags & UF_FLYING)))
            return true;
    return false;
}

/* One try at a neighbour field: attack, move, open a door, break a wall. */
static AiAct try_field(World *w, Rng *rng, uint8_t id, int8_t dx, int8_t dy, bool visit_check)
{
    uint8_t ui;
    Unit *u = by_id(w, id, &ui);
    int16_t nx, ny;
    uint8_t other, fe;
    if (!u)
        return A_END;
    nx = (int16_t)(u->x + dx);
    ny = (int16_t)(u->y + dy);
    if (!world_wrap(w, &nx, &ny))
        return A_NONE;
    other = world_unit_at(w, nx, ny, (u->flags & UF_FLYING) ? UL_AIR : UL_GROUND);
    if (other != NO_UNIT && other != ui) {
        CombatResult r;
        if (!ai_is_foe(w, u, &w->units[other]))
            return A_NONE;
        if (!items_can_harm_undead(w, ui, other))
            return A_NONE;
        if (!combat_melee(w, rng, ui, other, &r))
            return A_END;                /* no AP or stamina left */
        return (r.attacker_died) ? A_END : A_DONE;
    }
    fe = world_feature(w, nx, ny);
    if (fe == FE_DOOR_CLOSED || fe == FE_DOOR_LOCKED) {
        if (!(CREATURES[ride_actor_kind(u)].flags & CF_USE))
            return A_NONE;
        return ai_clear_feature(w, rng, ui, nx, ny) ? A_DONE : A_NONE;
    }
    if (world_blocks(w, nx, ny) && !(CREATURES[u->kind].flags & CF_PHASE) &&
        !(u->flags & UF_FLYING)) {
        bool destroyed = false;
        if (fe == FE_CHEST || fe == FE_CHEST_FREE)
            return A_NONE;
        return combat_terrain(w, rng, ui, nx, ny, &destroyed) > 0 ? A_DONE : A_NONE;
    }
    if (visit_check && ai_visited(w, ui, nx, ny))
        return A_NONE;
    {
        int16_t ox = u->x, oy = u->y;
        if (world_engaged(w, ui) || !world_move_unit(w, ui, dx, dy))
            return A_NONE;
        ai_visit(w, ui, ox, oy);
    }
    return A_DONE;
}

/* Move towards (gx, gy): a path first, the sorted neighbours as the fallback. */
static AiAct step_toward(World *w, Rng *rng, uint8_t id, int16_t gx, int16_t gy)
{
    uint8_t ui, d, order[8], i, j;
    uint16_t dist[8];
    Unit *u = by_id(w, id, &ui);
    int8_t sdx, sdy;
    if (!u)
        return A_END;
    if (ai_path_step(w, ui, gx, gy, &sdx, &sdy)) {
        AiAct r = try_field(w, rng, id, sdx, sdy, false);
        if (r != A_NONE)
            return r;
        u = by_id(w, id, &ui);
        if (!u)
            return A_END;
    }
    for (d = 0; d < 8; d++) {
        int16_t nx = (int16_t)(u->x + ND[d][0]), ny = (int16_t)(u->y + ND[d][1]);
        order[d] = d;
        dist[d] = world_wrap(w, &nx, &ny) ? world_range(w, nx, ny, gx, gy) : 0xFFFF;
    }
    for (i = 1; i < 8; i++) {            /* insertion sort, stable in the W..NW order */
        uint8_t o = order[i];
        for (j = i; j > 0 && dist[order[j - 1]] > dist[o]; j--)
            order[j] = order[j - 1];
        order[j] = o;
    }
    for (i = 0; i < 8; i++) {
        AiAct r = try_field(w, rng, id, ND[order[i]][0], ND[order[i]][1], true);
        if (r != A_NONE)
            return r;
    }
    return A_END;
}

/* ---------- the loop ---------- */

void ai_creature_turn(World *w, Rng *rng, const AiEnv *env, uint8_t id)
{
    uint8_t pass;
    AiView v;
    for (pass = 0; pass < AI_PASSES; pass++) {
        uint8_t ui, i, best_i = 0xFF;
        Unit *u = by_id(w, id, &ui);
        GoalKind gk = GT_NONE;
        int16_t gx = 0, gy = 0;
        AiAct act;
        AiItem item;
        if (!u || u->ap < 3)
            return;
        ai_build_view(w, ui, &v);
        if (u->plan_flags & 0x40) {          /* asleep: wakes when it sees a foe */
            if (v.en_n == 0)
                return;
            u->plan_flags = (uint8_t)(u->plan_flags & ~0x40);
        }
        if (!friend_here(w, ui)) {           /* K10.3 step 3 */
            act = ai_item_actions(w, rng, id, &v);
            if (act == A_DONE)
                continue;
            if (act == A_END)
                return;
            act = ai_toss_loot(w, id);
            if (act == A_DONE)
                continue;
            if (act == A_END)
                return;
            if (CREATURES[u->kind].ap_fly && !(u->flags & UF_FLYING)) {
                if (world_take_off(w, ui))
                    continue;
                if (u->ap < ACTIONS[ACT_TAKE_OFF].ap)
                    return;
            }
        }
        act = do_ranged(w, rng, id, &v);
        if (act == A_DONE)
            continue;
        act = do_flee(w, id, &v);
        if (act == A_END)
            return;
        u = by_id(w, id, &ui);
        if (!u)
            return;
        for (i = 0; i < v.en_n; i++) {       /* the nearest worthwhile melee target */
            uint8_t ei = world_find_unit(w, v.en[i].id);
            if (ei != NO_UNIT && melee_worth(w, ui, &v.en[i], ei)) {
                best_i = ei;
                break;
            }
        }
        if (best_i != 0xFF) {
            gk = GT_MELEE;
            gx = w->units[best_i].x;
            gy = w->units[best_i].y;
        } else if (ai_pick_item(w, ui, &v, &item) &&
                   !(item.x == u->x && item.y == u->y && item.kind != AI_CHEST)) {
            gk = GT_ITEM;
            gx = item.x;
            gy = item.y;
            if (item.kind == AI_CHEST && item.dist < 4) {   /* open it from beside */
                if (u->flags & UF_FLYING) {
                    if (world_land(w, ui))
                        continue;
                    return;
                }
                if (items_open_chest(w, rng, ui, gx, gy))
                    continue;
                return;
            }
        } else if (env && env->game && env->game->portal_open && env->game->portal_x >= 0 &&
                   u->owner < OWN_NEUTRAL && !game_over(env->game, w)) {
            gk = GT_PORTAL;                  /* from the portal round on, all go there */
            gx = env->game->portal_x;
            gy = env->game->portal_y;
            if (u->x == gx && u->y == gy)
                return;
        } else if (aggressive(u) && ai_wizard_of(w, u->owner) != NO_UNIT) {
            uint8_t wi = ai_wizard_of(w, u->owner);
            if (wi != ui) {
                if (world_range(w, u->x, u->y, w->units[wi].x, w->units[wi].y) < 5)
                    return;                  /* near enough: stand guard */
                gk = GT_FOLLOW;
                gx = w->units[wi].x;
                gy = w->units[wi].y;
            }
        } else if (ai_carries_loot(u) && u->owner < OWN_NEUTRAL &&
                   ai_wizard_of(w, u->owner) != NO_UNIT && !is_wizard(u)) {
            uint8_t wi = ai_wizard_of(w, u->owner);
            gk = GT_FOLLOW;
            gx = w->units[wi].x;
            gy = w->units[wi].y;
        } else if (u->post_x != 0xFF && (u->x != u->post_x || u->y != u->post_y)) {
            gk = GT_POST;
            gx = u->post_x;
            gy = u->post_y;
        }
        if (gk == GT_NONE) {                 /* nothing to do: monsters roam */
            if (u->owner == OWN_NEUTRAL && u->post_x == 0xFF) {
                static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
                static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
                uint8_t s, k;
                for (s = 0; s < 2; s++)
                    for (k = 0; k < 3; k++)
                        if (world_move_unit(w, ui, DX[rng_range(rng, 8)], DY[rng_range(rng, 8)]))
                            break;
            }
            return;
        }
        /* a flier lands to fight or to pick up what is beside it */
        if ((u->flags & UF_FLYING) && (gk == GT_MELEE || gk == GT_ITEM) &&
            world_range(w, u->x, u->y, gx, gy) < 4) {
            bool target_air = gk == GT_MELEE && (w->units[best_i].flags & UF_FLYING);
            if (!target_air && !world_land(w, ui) && gk == GT_MELEE)
                return;                      /* lands and fights in the same breath */
        }
        act = step_toward(w, rng, id, gx, gy);
        if (act == A_END)
            return;
    }
}

void ai_hunter(World *w, Rng *rng, uint8_t unit)
{
    if (unit < w->unit_count)
        ai_creature_turn(w, rng, NULL, w->units[unit].id);
}

void ai_guard(World *w, Rng *rng, uint8_t unit, uint8_t home_range)
{
    (void)home_range;                    /* the post itself is the anchor */
    if (unit < w->unit_count)
        ai_creature_turn(w, rng, NULL, w->units[unit].id);
}
