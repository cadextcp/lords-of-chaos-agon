#include "combat.h"

#include "sight.h"

#include <string.h>

#include "area.h"
#include "events.h"
#include "gen/data.h"
#include "items.h"
#include "ride.h"

uint8_t combat_roll(Rng *rng, uint8_t attack, uint8_t defence)
{
    uint16_t n = (uint16_t)(2 * ((uint16_t)attack + 1));
    uint16_t r;
    if (n > 255)
        n = 255;
    r = rng_range(rng, n);
    return r > defence ? (uint8_t)(r - defence) : 0;
}

uint8_t combat_hit_chance(uint8_t attack, uint8_t defence)
{
    uint16_t n = (uint16_t)(2 * ((uint16_t)attack + 1));
    if (n > 255)
        n = 255;
    if (defence + 1u >= n)
        return 0;
    return (uint8_t)(((uint16_t)(n - 1 - defence) * 100u) / n);
}

bool combat_damage(World *w, uint8_t target, uint8_t damage, uint8_t killer_kind,
                   uint8_t killer_owner, bool melee, bool *wound, bool crit)
{
    Unit *u = &w->units[target];
    bool wounded = damage > u->con_max / 4 && u->wounds < 7;   /* K6.3 */
    world_provoke(w, target, killer_owner);
    world_disturb(w, u->x, u->y, killer_owner);    /* D37 */
    if (wound)
        *wound = wounded;
    events_push(EV_HIT, u->x, u->y, u->kind, u->owner, damage, crit ? 1 : 0);
    if (wounded) {
        world_set_wounds(u, (uint8_t)(u->wounds + 1));
        events_push(EV_WOUND, u->x, u->y, u->kind, u->owner, 0, 0);
    }
    if (damage >= u->con) {
        world_kill_unit(w, target, killer_kind, killer_owner, melee);
        return true;
    }
    u->con = (uint8_t)(u->con - damage);
    return false;
}

/* Chebyshev distance of two units, honouring wrap-around. */
static bool adjacent(const World *w, const Unit *a, const Unit *b)
{
    int16_t dx = (int16_t)(b->x - a->x), dy = (int16_t)(b->y - a->y);
    if (w->wrap) {
        if (dx > w->w / 2) dx = (int16_t)(dx - w->w);
        if (dx < -w->w / 2) dx = (int16_t)(dx + w->w);
        if (dy > w->h / 2) dy = (int16_t)(dy - w->h);
        if (dy < -w->h / 2) dy = (int16_t)(dy + w->h);
    }
    return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}

bool combat_melee(World *w, Rng *rng, uint8_t att, uint8_t def, CombatResult *out)
{
    const Unit *a, *d;
    uint8_t dmg = 0;
    memset(out, 0, sizeof *out);
    if (att >= w->unit_count || def >= w->unit_count || att == def)
        return false;
    a = &w->units[att];
    d = &w->units[def];
    if (a->owner == d->owner || !adjacent(w, a, d))
        return false;
    if ((d->flags & UF_FLYING) && !(a->flags & UF_FLYING))
        return false;                      /* no melee against flyers from the ground */
    if (!world_can_pay(w, att, ACT_MELEE))
        return false;                      /* 8 AP and 8 stamina (K6.2) */

    world_pay(w, att, ACT_MELEE);
    world_provoke(w, def, a->owner);       /* even a miss angers an animal */
    world_disturb(w, d->x, d->y, a->owner);
    world_engage(w, att);                  /* an attack binds the attacker (K11.7) */
    events_push(EV_SWING, d->x, d->y, a->kind, a->owner, 0, 0);
    /* normal weapons clank off the undead; damage = RND(2 (C+1)) - Def (K6.2) */
    if (items_can_harm_undead(w, att, def))
        dmg = combat_roll(rng, items_combat(w, att), items_defence(w, def));
    if (dmg) {
        out->hit = true;
        out->damage = dmg;
        out->died = combat_damage(w, def, dmg, ride_actor_kind(a), a->owner, true,
                                  &out->wound, false);
        if (out->died)
            return true;                   /* the dead do not strike back */
    } else
        events_push(EV_MISS, d->x, d->y, a->kind, a->owner, 0, 0);

    /* The return blow (K6.2): every time, if the defender has 4 AP and 4
     * stamina left; it pays them and rolls with the same formula. */
    d = &w->units[def];                    /* pointer refreshed, not removed */
    if (world_can_pay(w, def, ACT_RETURN_ATTACK)) {
        uint8_t rdmg = 0;
        world_pay(w, def, ACT_RETURN_ATTACK);
        out->returned = true;
        world_engage(w, def);              /* the answer binds him too */
        events_push(EV_SWING, a->x, a->y, d->kind, d->owner, 0, 0);
        if (items_can_harm_undead(w, def, att))
            rdmg = combat_roll(rng, items_combat(w, def), items_defence(w, att));
        if (rdmg) {
            out->return_hit = true;
            out->return_damage = rdmg;
            out->attacker_died = combat_damage(w, att, rdmg, ride_actor_kind(d), d->owner,
                                               true, &out->return_wound, false);
        } else
            events_push(EV_MISS, a->x, a->y, d->kind, d->owner, 0, 0);
    }
    return true;
}

bool combat_hostile_to(const World *w, const Unit *e, uint8_t owner,
                       int16_t x, int16_t y)
{
    uint8_t wild;
    if (e->owner == owner)
        return false;
    if (e->owner != OWN_NEUTRAL)
        return true;
    wild = CREATURES[e->kind].wild;
    if (wild == WILD_NONE && !e->herd_dir)
        return true;                       /* monsters hunt everyone */
    if (owner < 8 && (e->grudge & (uint8_t)(1u << owner)))
        return true;                       /* it was attacked by them */
    if (e->alarm && e->alarm_charge && e->alarm_owner == owner)
        return true;                       /* charging the disturber (D37) */
    return wild == WILD_TERRITORIAL && e->post_x != 0xFF &&
           world_distance(w, x, y, e->post_x, e->post_y) <= TERRITORY;
}

uint8_t combat_terrain(World *w, Rng *rng, uint8_t att, int16_t x, int16_t y,
                       bool *destroyed)
{
    Unit *u;
    uint8_t fe, c;
    *destroyed = false;
    if (att >= w->unit_count)
        return 0;
    u = &w->units[att];
    if (!world_wrap(w, &x, &y))
        return 0;
    c = items_combat(w, att);
    {   /* blob and vine are terrain too (toughness 50 / 40, K8.4) */
        uint8_t at = area_toughness(w, x, y);
        if (at && area_blocks_kind(w, x, y)) {
            if ((uint16_t)(c + c / 2) < at || !world_can_pay(w, att, ACT_ATTACK_TERRAIN))
                return 0;
            world_pay(w, att, ACT_ATTACK_TERRAIN);
            world_engage(w, att);
            events_push(EV_SWING, x, y, u->kind, u->owner, 0, 1);
            if (rng_range(rng, (uint16_t)(2 * (uint16_t)c)) >= at) {
                area_remove_field(w, x, y);
                *destroyed = true;
            }
            return c;
        }
    }
    fe = w->feature[y][x];
    if (!world_blocks(w, x, y) || FEATURE_TOUGH[fe] == 0)
        return 0;                          /* nothing destructible to hit */
    if ((uint16_t)(c + c / 2) < FEATURE_TOUGH[fe])
        return 0;                          /* 1.5 C >= toughness to try (K6.4) */
    if (!world_can_pay(w, att, ACT_ATTACK_TERRAIN))
        return 0;
    world_pay(w, att, ACT_ATTACK_TERRAIN);
    world_engage(w, att);
    events_push(EV_SWING, x, y, u->kind, u->owner, 0, 1);
    if (rng_range(rng, (uint16_t)(2 * (uint16_t)c)) >= FEATURE_TOUGH[fe]) {
        w->feature[y][x] = FE_NONE;        /* smashed to pieces */
        world_map_changed(w);
        world_poke(w, x, y);
        *destroyed = true;
        events_push(EV_SMASH, x, y, fe, 0, c, 0);
    }
    return c;
}
