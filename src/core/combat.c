#include "combat.h"

#include "gen/data.h"

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

static void apply_hit(World *w, uint8_t target, uint8_t damage,
                      bool *wound, bool *died)
{
    Unit *u = &w->units[target];
    *wound = damage > u->con_max / 4;      /* fatal wound (PM 17) */
    if (*wound)
        u->flags |= UF_WOUNDED;
    if (damage >= u->con) {
        *died = true;
        world_remove_unit(w, target);
    } else {
        u->con = (uint8_t)(u->con - damage);
    }
}

bool combat_melee(World *w, Rng *rng, uint8_t att, uint8_t def, CombatResult *out)
{
    const Unit *a, *d;
    bool ok_to_hit;
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

    out->hit = out->wound = out->died = false;
    out->returned = out->return_hit = false;
    out->return_wound = out->attacker_died = false;
    out->damage = out->return_damage = 0;

    world_spend(w, att, ACTIONS[ACT_MELEE].ap);
    ok_to_hit = rng_range(rng, 100) < combat_hit_chance(a->com, d->def);
    if (!ok_to_hit)
        return true;
    out->hit = true;
    out->damage = roll_damage(a, rng);
    apply_hit(w, def, out->damage, &out->wound, &out->died);
    if (out->died)
        return true;                       /* the dead do not strike back */

    d = &w->units[def];                    /* pointer refreshed, not removed */
    if (d->ap >= ACTIONS[ACT_RETURN_ATTACK].ap &&
        d->sta >= ACTIONS[ACT_RETURN_ATTACK].stamina) {
        out->returned = true;
        world_spend(w, def, ACTIONS[ACT_RETURN_ATTACK].ap);
        if (rng_range(rng, 100) < combat_hit_chance(d->com, a->def)) {
            out->return_hit = true;
            out->return_damage = roll_damage(d, rng);
            apply_hit(w, att, out->return_damage, &out->return_wound,
                      &out->attacker_died);
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
