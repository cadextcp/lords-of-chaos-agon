#include "combat.h"

#include <string.h>

#include "gen/data.h"
#include "items.h"

uint8_t combat_hit_chance(uint8_t com, uint8_t def)
{
    int16_t p = (int16_t)(50 + 5 * ((int16_t)com - def));
    if (p < 10)
        return 10;
    if (p > 90)
        return 90;
    return (uint8_t)p;
}

/* Damage of one hit (D16): quarter of Combat plus a random quarter. */
static uint8_t roll_damage(const Unit *u, Rng *rng)
{
    uint16_t d = (uint16_t)((u->com + rng_range(rng, (uint16_t)(u->com + 1))) / 4);
    return d == 0 ? 1 : (uint8_t)d;
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

bool combat_damage(World *w, uint8_t target, uint8_t damage, uint8_t killer_kind,
                   uint8_t killer_owner, bool melee, bool *wound)
{
    Unit *u = &w->units[target];
    bool wounded = damage > u->con_max / 4;   /* fatal wound (PM 17) */
    if (wound)
        *wound = wounded;
    if (wounded)
        u->flags |= UF_WOUNDED;
    if (damage >= u->con) {
        world_kill_unit(w, target, killer_kind, killer_owner, melee);
        return true;
    }
    u->con = (uint8_t)(u->con - damage);
    return false;
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
    world_engage(w, att);                  /* melee contact binds both (GDD 6) */
    world_engage(w, def);
    /* normal weapons clank off the undead (GDD 4.2); either way the
     * defender strikes back below, hit or miss (GDD 6) */
    ok_to_hit = items_can_harm_undead(w, att, def) &&
                rng_range(rng, 100) <
                combat_hit_chance(items_combat(w, att), items_defence(w, def));
    if (ok_to_hit) {
        out->hit = true;
        out->damage = roll_damage(a, rng);   /* base value; weapon bonus in chance */
        out->died = combat_damage(w, def, out->damage, a->kind, a->owner, true,
                                  &out->wound);
        if (out->died)
            return true;                   /* the dead do not strike back */
    }

    d = &w->units[def];                    /* pointer refreshed, not removed */
    if (d->ap >= ACTIONS[ACT_RETURN_ATTACK].ap &&
        d->sta >= ACTIONS[ACT_RETURN_ATTACK].stamina) {
        out->returned = true;
        world_spend(w, def, ACTIONS[ACT_RETURN_ATTACK].ap);
        if (items_can_harm_undead(w, def, att) &&
            rng_range(rng, 100) <
            combat_hit_chance(items_combat(w, def), items_defence(w, att))) {
            out->return_hit = true;
            out->return_damage = roll_damage(d, rng);
            out->attacker_died = combat_damage(w, att, out->return_damage,
                                               d->kind, d->owner, true,
                                               &out->return_wound);
        }
    }
    return true;
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
    dmg = roll_damage(u, rng);
    if ((uint16_t)(dmg + rng_range(rng, 4)) > FEATURE_TOUGH[fe]) {
        w->feature[y][x] = FE_NONE;        /* smashed to pieces */
        world_map_changed(w);
        *destroyed = true;
    }
    return dmg;
}
