#include "items.h"

#include <stddef.h>

#include "combat.h"
#include "gen/data.h"
#include "sight.h"

/* Weapon of the object in use, WEAPON_NONE without one. */
static uint8_t in_use_weapon(const Unit *u)
{
    if (u->in_use == NO_ITEM || u->in_use >= u->item_count)
        return WEAPON_NONE;
    return OBJECTS[u->items[u->in_use]].weapon;
}

uint8_t items_weight(const World *w, uint8_t unit)
{
    uint8_t i, sum = 0;
    const Unit *u;
    if (unit >= w->unit_count)
        return 0;
    u = &w->units[unit];
    for (i = 0; i < u->item_count; i++)
        sum = (uint8_t)(sum + OBJECTS[u->items[i]].weight);
    return sum;
}

uint8_t items_kind_at(const World *w, int16_t x, int16_t y)
{
    uint8_t i;
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].x == x && w->objects[i].y == y) {
            uint8_t k;
            for (k = 0; k < OBJ_COUNT; k++)
                if (OBJECTS[k].tile == w->objects[i].tile)
                    return k;
        }
    return NO_ITEM;
}

static void remove_ground_object(World *w, uint8_t i)
{
    w->objects[i] = w->objects[w->object_count - 1];
    w->object_count--;
}

bool items_pick_up(World *w, uint8_t unit)
{
    Unit *u;
    uint8_t kind, i;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    kind = items_kind_at(w, u->x, u->y);
    if (kind == NO_ITEM || u->item_count >= UNIT_ITEMS)
        return false;
    if ((uint16_t)items_weight(w, unit) + OBJECTS[kind].weight >
        CREATURES[u->kind].carry)
        return false;                    /* too heavy (GDD 8) */
    if (u->ap < ACTIONS[ACT_PICK_UP].ap)
        return false;
    world_spend(w, unit, ACTIONS[ACT_PICK_UP].ap);
    u->items[u->item_count++] = kind;
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].x == u->x && w->objects[i].y == u->y &&
            w->objects[i].tile == OBJECTS[kind].tile) {
            remove_ground_object(w, i);
            break;
        }
    return true;
}

bool items_drop(World *w, uint8_t unit)
{
    Unit *u;
    uint8_t kind;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->in_use == NO_ITEM || u->in_use >= u->item_count ||
        w->object_count >= MAX_OBJECTS)
        return false;
    if (u->ap < ACTIONS[ACT_DROP].ap)
        return false;
    world_spend(w, unit, ACTIONS[ACT_DROP].ap);
    kind = u->items[u->in_use];
    u->items[u->in_use] = u->items[u->item_count - 1];
    u->item_count--;
    u->in_use = NO_ITEM;
    w->objects[w->object_count].x = u->x;
    w->objects[w->object_count].y = u->y;
    w->objects[w->object_count].tile = OBJECTS[kind].tile;
    w->object_count++;
    return true;
}

bool items_cycle(World *w, uint8_t unit)
{
    Unit *u;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->ap < ACTIONS[ACT_CHANGE].ap)
        return false;
    world_spend(w, unit, ACTIONS[ACT_CHANGE].ap);
    if (u->item_count == 0) {
        u->in_use = NO_ITEM;
    } else if (u->in_use == NO_ITEM || u->in_use + 1 >= u->item_count) {
        u->in_use = 0;
    } else {
        u->in_use = (uint8_t)(u->in_use + 1);
    }
    return true;
}

/* Damage off a base value (throwing/firing, D16 style). */
static uint8_t roll(uint8_t base, Rng *rng)
{
    uint16_t d = (uint16_t)((base + rng_range(rng, (uint16_t)(base + 1))) / 2);
    return d == 0 ? 1 : (uint8_t)d;
}

bool items_throw(World *w, Rng *rng, uint8_t unit, int8_t dx, int8_t dy)
{
    Unit *u;
    uint8_t kind, weapon, dist;
    int16_t x, y;
    if (unit >= w->unit_count || (dx == 0 && dy == 0) ||
        dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return false;
    u = &w->units[unit];
    if (u->in_use == NO_ITEM || u->in_use >= u->item_count)
        return false;
    if (u->ap < ACTIONS[ACT_THROW].ap)
        return false;
    kind = u->items[u->in_use];
    weapon = OBJECTS[kind].weapon;
    world_spend(w, unit, ACTIONS[ACT_THROW].ap);
    u->items[u->in_use] = u->items[u->item_count - 1];
    u->item_count--;
    u->in_use = NO_ITEM;

    x = u->x;
    y = u->y;
    for (dist = 0; dist < 6; dist++) {
        int16_t nx = (int16_t)(x + dx), ny = (int16_t)(y + dy);
        uint8_t target;
        if (!world_wrap(w, &nx, &ny) || world_blocks(w, nx, ny))
            break;
        target = world_unit_at(w, nx, ny, UL_GROUND);
        if (target == NO_UNIT)
            target = world_unit_at(w, nx, ny, UL_AIR);
        if (target != NO_UNIT) {        /* thrown weapons hit flyers too */
            if (rng_range(rng, 100) < combat_hit_chance(items_combat(w, unit),
                                                       items_defence(w, target)))
                combat_damage(w, target,
                              roll(weapon != WEAPON_NONE ? WEAPONS[weapon].thrown : 1, rng),
                              u->kind, u->owner, false, NULL);
            x = (int16_t)(nx - dx);     /* lands in front of the target */
            y = (int16_t)(ny - dy);
            world_wrap(w, &x, &y);
            goto land;
        }
        x = nx;
        y = ny;
    }
land:
    if (w->object_count < MAX_OBJECTS) {
        w->objects[w->object_count].x = (uint8_t)x;
        w->objects[w->object_count].y = (uint8_t)y;
        w->objects[w->object_count].tile = OBJECTS[kind].tile;
        w->object_count++;
    }
    return true;
}

bool items_fire(World *w, Rng *rng, uint8_t unit, int16_t tx, int16_t ty,
                uint8_t *damage)
{
    Unit *u;
    uint8_t weapon, target;
    int16_t dx, dy;
    if (damage)
        *damage = 0;
    if (unit >= w->unit_count || !world_wrap(w, &tx, &ty))
        return false;
    u = &w->units[unit];
    weapon = in_use_weapon(u);
    if (weapon == WEAPON_NONE || WEAPONS[weapon].ranged == 0)
        return false;                    /* no bow in hand */
    if (u->ap < ACTIONS[ACT_FIRE].ap)
        return false;
    dx = (int16_t)(tx - u->x);
    dy = (int16_t)(ty - u->y);
    if (w->wrap) {
        if (dx > w->w / 2) dx = (int16_t)(dx - w->w);
        if (dx < -w->w / 2) dx = (int16_t)(dx + w->w);
        if (dy > w->h / 2) dy = (int16_t)(dy - w->h);
        if (dy < -w->h / 2) dy = (int16_t)(dy + w->h);
    }
    if (dx > 6 || dx < -6 || dy > 6 || dy < -6)
        return false;
    if (!sight_has_los(w, u->x, u->y, tx, ty))
        return false;
    target = world_unit_at(w, tx, ty, UL_GROUND);
    if (target == NO_UNIT)
        target = world_unit_at(w, tx, ty, UL_AIR);
    if (target == NO_UNIT)
        return false;
    world_spend(w, unit, ACTIONS[ACT_FIRE].ap);
    if (rng_range(rng, 100) < combat_hit_chance(items_combat(w, unit),
                                                items_defence(w, target))) {
        uint8_t dmg = roll(WEAPONS[weapon].ranged, rng);
        if (damage)
            *damage = dmg;
        combat_damage(w, target, dmg, u->kind, u->owner, false, NULL);
    }
    return true;
}

uint8_t items_combat(const World *w, uint8_t unit)
{
    const Unit *u;
    uint8_t weapon, com;
    if (unit >= w->unit_count)
        return 0;
    u = &w->units[unit];
    com = u->com;
    weapon = in_use_weapon(u);
    if (weapon != WEAPON_NONE)
        com = (uint8_t)(com + WEAPONS[weapon].combat);
    return com;
}

uint8_t items_defence(const World *w, uint8_t unit)
{
    const Unit *u;
    uint8_t i, def;
    if (unit >= w->unit_count)
        return 0;
    u = &w->units[unit];
    def = u->def;
    for (i = 0; i < u->item_count; i++)     /* one carried shield counts (D21) */
        if (OBJECTS[u->items[i]].weapon == WEAPON_SHIELD)
            return (uint8_t)(def + WEAPONS[WEAPON_SHIELD].defence);
    return def;
}
