#include "ride.h"

#include "items.h"

#include "gen/data.h"

uint8_t ride_rider_kind(const Unit *mounted)
{
    return (mounted->flags & UF_RIDDEN) ? mounted->rider_kind : 0xFF;
}

uint8_t ride_actor_kind(const Unit *u)
{
    return (u->flags & UF_RIDDEN) ? u->rider_kind : u->kind;
}

static const int8_t RDX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t RDY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

bool ride_mount(World *w, uint8_t rider, int16_t x, int16_t y)
{
    Unit *r, *m;
    uint8_t mi, i;
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
     * rider rides along with his pack (PM 10), his mana and his own
     * values (D60) - nothing is lost or healed by mounting */
    world_spend(w, rider, ACTIONS[ACT_RIDE].ap);
    m->flags |= UF_RIDDEN;
    m->rider_kind = r->kind;
    m->rider_con = r->con;
    m->rider_con_max = r->con_max;
    m->rider_sta = r->sta;
    m->rider_sta_max = r->sta_max;
    m->rider_com = r->com;
    m->rider_def = r->def;
    m->rider_mr = r->mr;
    m->mana = r->mana;
    m->mana_max = r->mana_max;
    m->item_count = r->item_count;
    for (i = 0; i < UNIT_ITEMS; i++)
        m->items[i] = r->items[i];
    m->in_use = r->in_use;
    world_remove_unit(w, rider);
    return true;
}

bool ride_mount_adjacent(World *w, uint8_t rider)
{
    uint8_t i;
    if (rider >= w->unit_count)
        return false;
    for (i = 0; i < 8; i++)
        if (ride_mount(w, rider, (int16_t)(w->units[rider].x + RDX[i]),
                       (int16_t)(w->units[rider].y + RDY[i])))
            return true;                 /* the rider index is gone now */
    return false;
}

/* Put the rider carried by `m` back on the map at (x, y) with his own
 * values and pack. Returns his index, NO_UNIT when the list is full. */
static uint8_t put_rider(World *w, const Unit *m, int16_t x, int16_t y)
{
    Unit *r;
    uint8_t ri, i;
    ri = world_spawn_unit(w, m->owner, m->rider_kind, (uint8_t)x, (uint8_t)y);
    if (ri == NO_UNIT)
        return NO_UNIT;
    r = &w->units[ri];
    r->con = m->rider_con;
    r->con_max = m->rider_con_max;
    r->sta = m->rider_sta;
    r->sta_max = m->rider_sta_max;
    r->com = m->rider_com;
    r->def = m->rider_def;
    r->mr = m->rider_mr;
    r->mana = m->mana;
    r->mana_max = m->mana_max;
    for (i = 0; i < UNIT_ITEMS; i++)    /* the whole pack goes with him */
        r->items[i] = m->items[i];
    r->item_count = m->item_count;
    r->in_use = m->in_use;
    return ri;
}

static bool free_ground(const World *w, int16_t x, int16_t y)
{
    return !world_blocks(w, x, y) &&
           world_unit_at(w, x, y, UL_GROUND) == NO_UNIT;
}

bool ride_dismount(World *w, uint8_t mounted)
{
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
        int16_t nx = (int16_t)(m->x + RDX[i]);
        int16_t ny = (int16_t)(m->y + RDY[i]);
        if (!world_wrap(w, &nx, &ny) || !free_ground(w, nx, ny))
            continue;
        if (put_rider(w, m, nx, ny) == NO_UNIT)
            return false;
        m = &w->units[mounted];          /* spawning appends: index holds */
        world_spend(w, mounted, ACTIONS[ACT_DISMOUNT].ap);
        m->item_count = 0;
        m->in_use = NO_ITEM;
        m->mana = m->mana_max = 0;
        m->rider_kind = 0xFF;
        m->flags &= (uint8_t)~UF_RIDDEN;
        return true;
    }
    return false;                        /* no room to get off */
}

void ride_throw_off(World *w, const Unit *mount)
{
    int16_t x = mount->x, y = mount->y;
    uint8_t i;
    if (!(mount->flags & UF_RIDDEN))
        return;
    if (!free_ground(w, x, y)) {         /* fell from the air onto someone */
        for (i = 0; i < 8; i++) {
            int16_t nx = (int16_t)(mount->x + RDX[i]);
            int16_t ny = (int16_t)(mount->y + RDY[i]);
            if (world_wrap(w, &nx, &ny) && free_ground(w, nx, ny)) {
                x = nx;
                y = ny;
                break;
            }
        }
        if (i == 8) {                    /* nowhere to land: lost with it */
            world_drop_carried(w, mount);
            return;
        }
    }
    if (put_rider(w, mount, x, y) == NO_UNIT)
        world_drop_carried(w, mount);
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
