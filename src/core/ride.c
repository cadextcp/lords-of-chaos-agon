#include "ride.h"

#include "items.h"

#include "gen/data.h"

uint8_t ride_rider_kind(const Unit *mounted)
{
    return (mounted->flags & UF_RIDDEN) ? mounted->rider_kind : 0xFF;
}

bool ride_mount(World *w, uint8_t rider, int16_t x, int16_t y)
{
    Unit *r, *m;
    uint8_t mi;
    if (rider >= w->unit_count || !world_wrap(w, &x, &y))
        return false;
    r = &w->units[rider];
    if (!(CREATURES[r->kind].flags & CF_RIDE))
        return false;                    /* this creature cannot ride */
    mi = world_unit_at(w, x, y, UL_GROUND);
    if (mi == NO_UNIT || mi == rider)
        return false;
    m = &w->units[mi];
    if (m->owner != r->owner || !(CREATURES[m->kind].flags & CF_MOUNT))
        return false;                    /* friendly mount only */
    if (m->flags & UF_RIDDEN)
        return false;                    /* already carrying someone */
    if (r->ap < ACTIONS[ACT_RIDE].ap)
        return false;
    /* the mount absorbs the rider: it keeps its own AP/stamina, the
     * rider kind rides along (its items travel with it - PM 10) */
    world_spend(w, rider, ACTIONS[ACT_RIDE].ap);
    m->flags |= UF_RIDDEN;
    m->rider_kind = r->kind;
    m->item_count = r->item_count;       /* the rider carries the loot */
    {
        uint8_t i;
        for (i = 0; i < UNIT_ITEMS; i++)
            m->items[i] = r->items[i];
        m->in_use = r->in_use;
    }
    world_remove_unit(w, rider);
    return true;
}

bool ride_dismount(World *w, uint8_t mounted)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    Unit *m;
    uint8_t i;
    if (mounted >= w->unit_count)
        return false;
    m = &w->units[mounted];
    if (!(m->flags & UF_RIDDEN))
        return false;
    if (m->ap < ACTIONS[ACT_DISMOUNT].ap)
        return false;
    for (i = 0; i < 8; i++) {            /* a free field next to the mount */
        int16_t nx = (int16_t)(m->x + DX[i]);
        int16_t ny = (int16_t)(m->y + DY[i]);
        if (!world_wrap(w, &nx, &ny) || world_blocks(w, nx, ny) ||
            world_unit_at(w, nx, ny, UL_GROUND) != NO_UNIT)
            continue;
        {
            Unit *r;
            uint8_t ri;
            world_spend(w, mounted, ACTIONS[ACT_DISMOUNT].ap);
            ri = world_spawn_unit(w, m->owner, m->rider_kind, (uint8_t)nx,
                                  (uint8_t)ny);
            if (ri == NO_UNIT)
                return false;
            r = &w->units[ri];
            r->items[0] = m->items[0];   /* the loot goes with the rider */
            r->item_count = m->item_count;
            r->in_use = m->in_use;
            m->item_count = 0;
            m->in_use = NO_ITEM;
            m->flags &= (uint8_t)~UF_RIDDEN;
            return true;
        }
    }
    return false;                        /* no room to get off */
}

bool ride_may_attack_from(const World *w, uint8_t attacker)
{
    if (attacker >= w->unit_count)
        return false;
    return (w->units[attacker].flags & UF_RIDDEN) != 0;
}

/* The engaged rule exempts riders (D21: attack from a friendly field). */
bool ride_engaged_exception(const World *w, uint8_t unit)
{
    (void)w;
    return ride_may_attack_from(w, unit);
}
