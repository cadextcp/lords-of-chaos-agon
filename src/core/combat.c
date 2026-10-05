#include "combat.h"

#include "sight.h"

#include <string.h>

#include "events.h"
#include "gen/data.h"
#include "items.h"

uint8_t combat_spell_hit_chance(uint8_t magic_res)
{
    int16_t p = (int16_t)(100 - (int16_t)magic_res);
    if (p < COMBAT_CRIT_PERCENT)
        return COMBAT_CRIT_PERCENT;
    if (p > 100 - COMBAT_CRIT_PERCENT)
        return 100 - COMBAT_CRIT_PERCENT;
    return (uint8_t)p;
}

uint8_t combat_hit_chance(uint8_t com, uint8_t def)
{
    int16_t p = (int16_t)(50 + 5 * ((int16_t)com - def));
    if (p < 10)
        return 10;
    if (p > 90)
        return 90;
    return (uint8_t)p;
}

/* Damage of one hit (D28/D30): the weapon in hand rolls its dice plus the
 * fighter's com/5 - a critical hit rolls the dice twice. */
static uint8_t roll_damage(const World *w, const Unit *u, Rng *rng, bool crit)
{
    return items_attack_damage(w, (uint8_t)(u - w->units), rng, crit);
}

bool combat_damage(World *w, uint8_t target, uint8_t damage, uint8_t killer_kind,
                   uint8_t killer_owner, bool melee, bool *wound, bool crit)
{
    Unit *u = &w->units[target];
    bool wounded = damage > u->con_max / 4;   /* fatal wound (PM 17) */
    world_provoke(w, target, killer_owner);
    world_disturb(w, u->x, u->y, killer_owner);    /* D37 */
    if (wound)
        *wound = wounded;
    events_push(EV_HIT, u->x, u->y, u->kind, u->owner, damage, crit ? 1 : 0);
    if (wounded) {
        u->flags |= UF_WOUNDED;
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
    bool ok_to_hit;
    memset(out, 0, sizeof *out);
    if (att >= w->unit_count || def >= w->unit_count || att == def)
        return false;
    a = &w->units[att];
    d = &w->units[def];
    if (a->owner == d->owner || !adjacent(w, a, d))
        return false;
    if ((d->flags & UF_FLYING) && !(a->flags & UF_FLYING))
        return false;                      /* no melee against flyers from the ground */
    if (a->ap < ACTIONS[ACT_MELEE].ap)
        return false;

    world_spend(w, att, ACTIONS[ACT_MELEE].ap);
    world_provoke(w, def, a->owner);       /* even a miss angers an animal */
    world_disturb(w, d->x, d->y, a->owner);
    world_engage(w, att);                  /* melee contact binds both (GDD 6) */
    world_engage(w, def);
    events_push(EV_SWING, d->x, d->y, a->kind, a->owner, 0, 0);
    /* normal weapons clank off the undead (GDD 4.2); either way the
     * defender strikes back below, hit or miss (GDD 6). A very low
     * attack roll is a critical hit: the damage dice double (D30). */
    {
        bool can_harm = items_can_harm_undead(w, att, def);
        uint16_t roll = can_harm ? rng_range(rng, 100) : 100;
        ok_to_hit = roll <
                    combat_hit_chance(items_combat(w, att), items_defence(w, def));
        out->crit = roll < COMBAT_CRIT_PERCENT;
    }
    if (ok_to_hit) {
        out->hit = true;
        out->damage = roll_damage(w, a, rng, out->crit);
        out->died = combat_damage(w, def, out->damage, a->kind, a->owner, true,
                                  &out->wound, out->crit);
        if (out->died)
            return true;                   /* the dead do not strike back */
    } else
        events_push(EV_MISS, d->x, d->y, a->kind, a->owner, 0, 0);

    d = &w->units[def];                    /* pointer refreshed, not removed */
    /* The return blow is a free defensive reaction (D27) - but every unit
     * gets ONE reaction per round (D29, D&D style): it is spent here, and
     * further attacks in the same round land unanswered. Being attacked
     * still costs no AP and no stamina. */
    if (!d->reacted) {
        bool rcan_harm, rcrit = false;
        uint16_t rroll = 100;
        out->returned = true;
        w->units[def].reacted = true;
        events_push(EV_SWING, a->x, a->y, d->kind, d->owner, 0, 0);
        rcan_harm = items_can_harm_undead(w, def, att);
        if (rcan_harm)
            rroll = rng_range(rng, 100);
        if (rroll <
            combat_hit_chance(items_combat(w, def), items_defence(w, att))) {
            rcrit = rroll < COMBAT_CRIT_PERCENT;
            out->return_hit = true;
            out->return_crit = rcrit;
            out->return_damage = roll_damage(w, d, rng, rcrit);
            out->attacker_died = combat_damage(w, att, out->return_damage,
                                               d->kind, d->owner, true,
                                               &out->return_wound, rcrit);
        } else
            events_push(EV_MISS, a->x, a->y, d->kind, d->owner, 0, 0);
    }
    return true;
}

bool combat_free_swing(World *w, Rng *rng, uint8_t att, uint8_t def,
                       CombatResult *out)
{
    const Unit *a, *d;
    bool ok_to_hit;
    memset(out, 0, sizeof *out);
    if (att >= w->unit_count || def >= w->unit_count || att == def)
        return false;
    a = &w->units[att];
    d = &w->units[def];
    if (a->owner == d->owner || !adjacent(w, a, d))
        return false;
    if ((d->flags & UF_FLYING) && !(a->flags & UF_FLYING))
        return false;
    if (!items_can_harm_undead(w, att, def))
        return false;                      /* clanks off harmlessly (GDD 4.2) */
    if (a->reacted)
        return false;                      /* reaction spent this round (D29) */
    w->units[att].reacted = true;          /* the swing uses it up */
    events_push(EV_SWING, d->x, d->y, a->kind, a->owner, 0, 0);

    {
        uint16_t roll = rng_range(rng, 100);
        ok_to_hit = roll <
                    combat_hit_chance(items_combat(w, att), items_defence(w, def));
        out->crit = roll < COMBAT_CRIT_PERCENT;   /* D30 */
    }
    if (!ok_to_hit) {
        events_push(EV_MISS, d->x, d->y, a->kind, a->owner, 0, 0);
        return true;
    }
    out->hit = true;
    out->damage = roll_damage(w, a, rng, out->crit);
    out->died = combat_damage(w, def, out->damage, a->kind, a->owner, true,
                              &out->wound, out->crit);
    return true;
}

uint8_t combat_disengage_swings(World *w, Rng *rng, uint8_t unit,
                                const Sight *seen, CombatResult *out)
{
    uint8_t i;
    if (unit >= w->unit_count)
        return 0;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *e = &w->units[i];
        if (e->owner == w->units[unit].owner || (e->flags & UF_INVISIBLE))
            continue;
        if (seen ? !sight_visible(seen, w, e->x, e->y)
                 : !sight_has_los(w, w->units[unit].x, w->units[unit].y,
                                  e->x, e->y))
            continue;                  /* unseen: no swing out of nowhere */
        if (!adjacent(w, e, &w->units[unit]))
            continue;
        if (!combat_free_swing(w, rng, i, unit, out))
            continue;
        return 1;
    }
    return 0;
}

uint8_t combat_terrain(World *w, Rng *rng, uint8_t att, int16_t x, int16_t y,
                       bool *destroyed)
{
    Unit *u;
    uint8_t fe, dmg;
    *destroyed = false;
    if (att >= w->unit_count)
        return 0;
    u = &w->units[att];
    if (!world_wrap(w, &x, &y))
        return 0;
    fe = w->feature[y][x];
    if (!world_blocks(w, x, y) || FEATURE_TOUGH[fe] == 0)
        return 0;                          /* nothing destructible to hit */
    if (u->ap < ACTIONS[ACT_MELEE].ap)
        return 0;
    world_spend(w, att, ACTIONS[ACT_MELEE].ap);
    events_push(EV_SWING, x, y, u->kind, u->owner, 0, 1);
    dmg = roll_damage(w, u, rng, false);   /* no crits against furniture */
    if ((uint16_t)(dmg + rng_range(rng, 4)) > FEATURE_TOUGH[fe]) {
        w->feature[y][x] = FE_NONE;        /* smashed to pieces */
        world_map_changed(w);
        *destroyed = true;
        events_push(EV_SMASH, x, y, fe, 0, dmg, 0);
    }
    return dmg;
}
