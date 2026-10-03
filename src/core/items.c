#include "items.h"

#include <stddef.h>

#include "combat.h"
#include "effect.h"
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
    if (kind == OBJ_CAULDRON_FULL)
        return false;                    /* it would spill (GDD 7.2) */
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
            if (items_can_harm_undead(w, unit, target) &&
                rng_range(rng, 100) < combat_hit_chance(items_combat(w, unit),
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
    if (items_can_harm_undead(w, unit, target) &&
        rng_range(rng, 100) < combat_hit_chance(items_combat(w, unit),
                                                items_defence(w, target))) {
        uint8_t dmg = roll(WEAPONS[weapon].ranged, rng);
        if (damage)
            *damage = dmg;
        combat_damage(w, target, dmg, u->kind, u->owner, false, NULL);
    }
    return true;
}

/* Below half Constitution every fighter suffers (GDD 4.1). */
static uint8_t con_malus(const Unit *u)
{
    return u->con < u->con_max / 2 ? 2 : 0;
}

/* Can this ATTACKER wound an UNDEAD defender (GDD 4.2)? Undead
 * attackers, the Magic Slayer and enchanted weapons (M4b) do; normal
 * weapons and bare hands do not. Callers pass the defender. */
bool items_can_harm_undead(const World *w, uint8_t attacker, uint8_t defender)
{
    const Unit *a = &w->units[attacker];
    uint8_t weapon;
    if (!(w->units[defender].flags & UF_UNDEAD))
        return true;                     /* the living are always woundable */
    if (a->flags & UF_UNDEAD)
        return true;
    weapon = in_use_weapon(a);
    return weapon != WEAPON_NONE &&
           (weapon == WEAPON_MAGIC_SLAYER || a->flags & UF_MAGIC_WEAPON);
}

uint8_t items_combat(const World *w, uint8_t unit)
{
    const Unit *u;
    uint8_t weapon, com, malus;
    if (unit >= w->unit_count)
        return 0;
    u = &w->units[unit];
    com = u->com;
    weapon = in_use_weapon(u);
    if (weapon != WEAPON_NONE) {         /* enchanted: double values (GDD 6.1) */
        uint8_t bonus = WEAPONS[weapon].combat;
        com = (uint8_t)(com + ((u->flags & UF_MAGIC_WEAPON) ? 2 * bonus : bonus));
    }
    if (effect_active(u, EFF_STRENGTH))
        com = (uint8_t)(com + effect_power(u, EFF_STRENGTH));
    malus = con_malus(u);               /* below 50 % Con (GDD 4.1) */
    return com > malus ? (uint8_t)(com - malus) : 0;
}

uint8_t items_defence(const World *w, uint8_t unit)
{
    const Unit *u;
    uint8_t i, def, malus;
    if (unit >= w->unit_count)
        return 0;
    u = &w->units[unit];
    def = u->def;
    for (i = 0; i < u->item_count; i++) {   /* ONE carried shield counts (D21) */
        if (OBJECTS[u->items[i]].weapon == WEAPON_SHIELD) {
            uint8_t bonus = WEAPONS[WEAPON_SHIELD].defence;
            def = (uint8_t)(def + ((u->flags & UF_MAGIC_WEAPON) ? 2 * bonus : bonus));
            break;
        }
    }
    if (effect_active(u, EFF_SHIELD))
        def = (uint8_t)(def + effect_power(u, EFF_SHIELD));
    if (effect_active(u, EFF_PROTECT))
        def = (uint8_t)(def + effect_power(u, EFF_PROTECT));
    malus = con_malus(u);
    return def > malus ? (uint8_t)(def - malus) : 0;
}


bool items_eat(World *w, uint8_t unit)
{
    Unit *u;
    uint8_t kind;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->in_use == NO_ITEM || u->in_use >= u->item_count)
        return false;
    kind = u->items[u->in_use];
    if (OBJECTS[kind].category != OC_FOOD)
        return false;
    if (u->ap < ACTIONS[ACT_EAT].ap)
        return false;
    world_spend(w, unit, ACTIONS[ACT_EAT].ap);
    {
        uint8_t heal = OBJECTS[kind].eat_con;
        uint8_t mana = OBJECTS[kind].eat_mana;
        if (heal && u->con < u->con_max)
            u->con = (uint8_t)(u->con + heal > u->con_max ? u->con_max
                                                          : u->con + heal);
        if (mana && u->mana < u->mana_max)
            u->mana = (uint8_t)(u->mana + mana > u->mana_max ? u->mana_max
                                                             : u->mana + mana);
    }
    u->items[u->in_use] = u->items[u->item_count - 1];   /* consumed */
    u->item_count--;
    u->in_use = NO_ITEM;
    return true;
}

const char *items_read(World *w, uint8_t unit)
{
    Unit *u;
    uint8_t kind;
    if (unit >= w->unit_count)
        return NULL;
    u = &w->units[unit];
    if (u->in_use == NO_ITEM || u->in_use >= u->item_count)
        return NULL;
    kind = u->items[u->in_use];
    if (OBJECTS[kind].category != OC_SCROLL)
        return NULL;
    if (u->ap < ACTIONS[ACT_READ].ap)
        return NULL;
    world_spend(w, unit, ACTIONS[ACT_READ].ap);
    u->items[u->in_use] = u->items[u->item_count - 1];   /* read away */
    u->item_count--;
    u->in_use = NO_ITEM;
    return "Gelesen: Das Portal kommt erst spaet.";   /* until scenarios carry texts */
}

/* Chest loot table (own values, D7): every chest holds one treasure. */
static const uint8_t CHEST_LOOT[] = {
    OBJ_GOLD, OBJ_GOLD, OBJ_EMERALD, OBJ_EMERALD, OBJ_RUBY,
    OBJ_WAND, OBJ_RUNE_STONE, OBJ_DIAMOND,
};

bool items_open_chest(World *w, Rng *rng, uint8_t unit, int16_t x, int16_t y)
{
    Unit *u;
    uint8_t i, ap, kind;
    if (unit >= w->unit_count || !world_wrap(w, &x, &y))
        return false;
    if (w->feature[y][x] != FE_CHEST)
        return false;
    u = &w->units[unit];
    kind = NO_ITEM;
    for (i = 0; i < u->item_count; i++)     /* a key unlocks cheaply */
        if (u->items[i] == OBJ_CHEST_KEY)
            kind = i;
    ap = kind != NO_ITEM ? ACTIONS[ACT_UNLOCK].ap
                         : (uint8_t)(ACTIONS[ACT_OPEN_CHEST].ap * 3);
    if (u->ap < ap)
        return false;
    if (!(CREATURES[u->kind].flags & CF_USE))
        return false;                    /* hands needed */
    world_spend(w, unit, ap);
    if (kind != NO_ITEM) {               /* keys vanish after use (GDD 8) */
        u->items[kind] = u->items[u->item_count - 1];
        u->item_count--;
    }
    w->feature[y][x] = FE_NONE;          /* empty box stays as rubble-less */
    world_map_changed(w);
    if (w->object_count < MAX_OBJECTS) { /* the loot drops */
        uint8_t loot = CHEST_LOOT[rng_range(rng, (uint16_t)(sizeof CHEST_LOOT))];
        w->objects[w->object_count].x = (uint8_t)x;
        w->objects[w->object_count].y = (uint8_t)y;
        w->objects[w->object_count].tile = OBJECTS[loot].tile;
        w->object_count++;
    }
    return true;
}
