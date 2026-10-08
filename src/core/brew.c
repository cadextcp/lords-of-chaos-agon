#include "brew.h"

#include "combat.h"
#include "effect.h"
#include "events.h"
#include "gen/data.h"
#include "gen/tiles.h"
#include "items.h"
#include "ride.h"

/* Which ingredient brews which potion (GDD 7.2). Dragon herb also
 * unlocks the dragon summons. */
static const struct {
    uint8_t object;    /* OBJ_* */
    uint8_t potion;    /* SP_* */
} INGREDIENTS[] = {
    { OBJ_MISTLETOE, SP_STRENGTH_POTION },
    { OBJ_CLOVER, SP_PROTECTION_POTION },
    { OBJ_CRYSTAL, SP_INVISIBILITY_POTION },
    { OBJ_SULPH, SP_SPEED_POTION },
    { OBJ_FAIRYWING, SP_FLYING_POTION },
    { OBJ_NITRO, SP_BOMB_POTION },
    { OBJ_APPLE, SP_HEALING_POTION },    /* dragon herb is for dragons only */
};

/* A filled vial carries its brew; the vial object kind tells which. */
static const struct {
    uint8_t vial;      /* OBJ_VIAL_* */
    uint8_t potion;    /* SP_* */
} VIALS[] = {
    { OBJ_VIAL_STRENGTH, SP_STRENGTH_POTION },
    { OBJ_VIAL_PROTECTION, SP_PROTECTION_POTION },
    { OBJ_VIAL_INVISIBILITY, SP_INVISIBILITY_POTION },
    { OBJ_VIAL_SPEED, SP_SPEED_POTION },
    { OBJ_VIAL_FLYING, SP_FLYING_POTION },
    { OBJ_VIAL_HEALING, SP_HEALING_POTION },
};

static uint8_t vial_potion(uint8_t vial)
{
    uint8_t i;
    for (i = 0; i < sizeof VIALS / sizeof VIALS[0]; i++)
        if (VIALS[i].vial == vial)
            return VIALS[i].potion;
    return 0xFF;
}

/* Duration in rounds (K8.2): floor((3 (L-1) + 10) / potion consumption of
 * the drinker), at least 1. L is the level the potion was brewed at. */
static uint8_t potion_rounds(const Unit *u, uint8_t level)
{
    uint16_t pc = CREATURES[ride_actor_kind(u)].potion;
    uint16_t base = (uint16_t)(3u * (level ? level - 1 : 0) + 10u);
    uint16_t r = pc ? base / pc : base;
    return r == 0 ? 1 : (r > 255 ? 255 : (uint8_t)r);
}

static bool ground_has(const World *w, int16_t x, int16_t y, uint8_t kind)
{
    uint8_t i;
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].x == x && w->objects[i].y == y &&
            w->objects[i].tile == OBJECTS[kind].tile)
            return true;
    return false;
}

static void consume_ground(World *w, int16_t x, int16_t y, uint8_t kind)
{
    uint8_t i;
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].x == x && w->objects[i].y == y &&
            w->objects[i].tile == OBJECTS[kind].tile) {
            w->objects[i] = w->objects[w->object_count - 1];
            w->object_count--;
            return;
        }
}

void brew_register_map_cauldrons(World *w)
{
    uint8_t i;
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].tile == T_OBJ_CAULDRON_EMPTY ||
            w->objects[i].tile == T_OBJ_CAULDRON_FULL) {
            Cauldron *c = brew_cauldron_at(w, w->objects[i].x, w->objects[i].y);
            if (!c && w->cauldron_count < CAULDRONS_MAX) {
                c = &w->cauldrons[w->cauldron_count++];
                c->x = w->objects[i].x;
                c->y = w->objects[i].y;
                c->potion = 0xFF;
                c->doses = 0;
            }
        }
}

static bool cauldron_object_at(const World *w, int16_t x, int16_t y)
{
    uint8_t i;
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].x == x && w->objects[i].y == y &&
            (w->objects[i].tile == T_OBJ_CAULDRON_EMPTY ||
             w->objects[i].tile == T_OBJ_CAULDRON_FULL))
            return true;
    return false;
}

/* The cauldron object on the ground is the truth; the record only adds
 * its contents. Records whose object was carried away are dropped, an
 * (empty) cauldron set down elsewhere gets a fresh record. */
Cauldron *brew_cauldron_at(World *w, int16_t x, int16_t y)
{
    uint8_t i;
    for (i = w->cauldron_count; i-- > 0;)
        if (!cauldron_object_at(w, w->cauldrons[i].x, w->cauldrons[i].y))
            w->cauldrons[i] = w->cauldrons[--w->cauldron_count];
    if (!cauldron_object_at(w, x, y))
        return NULL;
    for (i = 0; i < w->cauldron_count; i++)
        if (w->cauldrons[i].x == x && w->cauldrons[i].y == y)
            return &w->cauldrons[i];
    if (w->cauldron_count >= CAULDRONS_MAX)
        return NULL;
    {
        Cauldron *c = &w->cauldrons[w->cauldron_count++];
        c->x = (uint8_t)x;
        c->y = (uint8_t)y;
        c->potion = 0xFF;
        c->doses = 0;
        c->level = 0;
        return c;
    }
}

void brew_set_cauldron(World *w, int16_t x, int16_t y, bool full,
                       uint8_t potion)
{
    Cauldron *c = brew_cauldron_at(w, x, y);
    uint8_t i;
    uint16_t tile = full ? T_OBJ_CAULDRON_FULL : T_OBJ_CAULDRON_EMPTY;
    if (!c) {
        if (w->cauldron_count >= CAULDRONS_MAX)
            return;
        c = &w->cauldrons[w->cauldron_count++];
        c->x = (uint8_t)x;
        c->y = (uint8_t)y;
        c->doses = 0;
    }
    c->potion = full ? potion : 0xFF;
    if (!full)
        c->doses = 0;
    for (i = 0; i < w->object_count; i++)   /* ground object mirrors it */
        if (w->objects[i].x == x && w->objects[i].y == y &&
            (w->objects[i].tile == T_OBJ_CAULDRON_EMPTY ||
             w->objects[i].tile == T_OBJ_CAULDRON_FULL)) {
            w->objects[i].tile = tile;
            return;
        }
    if (w->object_count < MAX_OBJECTS) {
        w->objects[w->object_count].x = (uint8_t)x;
        w->objects[w->object_count].y = (uint8_t)y;
        w->objects[w->object_count].tile = tile;
        w->object_count++;
    }
}

uint8_t brew_ingredient_potion(uint8_t object_kind)
{
    uint8_t i;
    for (i = 0; i < sizeof INGREDIENTS / sizeof INGREDIENTS[0]; i++)
        if (INGREDIENTS[i].object == object_kind)
            return INGREDIENTS[i].potion;
    return 0xFF;
}

/* Apply one draught (F1 durations, strength as brewed). */
static bool apply_potion(World *w, uint8_t unit, uint8_t potion, uint8_t level)
{
    Unit *u = &w->units[unit];
    switch (potion) {
    case SP_STRENGTH_POTION:
        return effect_grant(u, EFF_STRENGTH, POTION_STRENGTH_BONUS,
                            potion_rounds(u, level));
    case SP_PROTECTION_POTION:
        return effect_grant(u, EFF_PROTECT, POTION_PROTECTION_BONUS,
                            potion_rounds(u, level));
    case SP_INVISIBILITY_POTION:
        return effect_grant(u, EFF_INVISIBLE, level, potion_rounds(u, level));
    case SP_SPEED_POTION:
        return effect_grant(u, EFF_SPEED, level, potion_rounds(u, level));
    case SP_FLYING_POTION:
        return effect_grant(u, EFF_FLYING, level, potion_rounds(u, level));
    case SP_SUPER_POTION:                /* strength + protection + speed (K8.2) */
        return effect_grant(u, EFF_STRENGTH, POTION_STRENGTH_BONUS,
                            potion_rounds(u, level)) &&
               effect_grant(u, EFF_PROTECT, POTION_PROTECTION_BONUS,
                            potion_rounds(u, level)) &&
               effect_grant(u, EFF_SPEED, level, potion_rounds(u, level));
    case SP_HEALING_POTION:
        u->con = u->con_max;             /* heals wounds too (GDD 7.2) */
        u->sta = u->sta_max;
        world_set_wounds(u, 0);
        return true;
    default:
        return false;                    /* bombs are not for drinking */
    }
}

bool brew_cast(World *w, Spellbook *b, uint8_t wiz, uint8_t spell)
{
    Unit *u;
    Cauldron *c;
    uint8_t i, level;
    bool need_found = false;
    if (wiz >= w->unit_count || spell >= SPELL_COUNT)
        return false;
    if (SPELLS[spell].category != SPC_POTION)
        return false;
    if (!spell_can_cast(w, b, wiz, spell))
        return false;
    u = &w->units[wiz];
    if (!brew_cauldron_at(w, u->x, u->y))
        return false;                    /* cauldron under the wizard */
    for (i = 0; i < sizeof INGREDIENTS / sizeof INGREDIENTS[0]; i++)
        if (INGREDIENTS[i].potion == spell &&
            ground_has(w, u->x, u->y, INGREDIENTS[i].object))
            need_found = true;
    if (!need_found)
        return false;                    /* ingredient missing (GDD 7.2) */
    level = b->level[spell];
    world_pay(w, wiz, ACT_CAST);
    u->mana = (uint8_t)(u->mana - spell_mana(spell, level));
    b->level[spell] = (uint8_t)(level - 1);
    events_push(EV_SPELL, u->x, u->y, spell, u->owner, 0, 0);
    for (i = 0; i < sizeof INGREDIENTS / sizeof INGREDIENTS[0]; i++)
        if (INGREDIENTS[i].potion == spell)
            consume_ground(w, u->x, u->y, INGREDIENTS[i].object);
    brew_set_cauldron(w, u->x, u->y, true, spell);
    c = brew_cauldron_at(w, u->x, u->y);
    if (c) {
        c->doses = (uint8_t)(level + 3); /* level+3 draughts (GDD 7.2) */
        c->level = level;
    }
    return true;
}

/* Dragons need a cauldron with dragon herb under the wizard (PM 21);
 * the herb is spent with the summon. */
bool brew_dragon_ready(World *w, uint8_t wiz)
{
    const Unit *u;
    if (wiz >= w->unit_count)
        return false;
    u = &w->units[wiz];
    return brew_cauldron_at(w, u->x, u->y) != NULL &&
           ground_has(w, u->x, u->y, OBJ_DRAGON_HERB);
}

void brew_dragon_spend(World *w, uint8_t wiz)
{
    consume_ground(w, w->units[wiz].x, w->units[wiz].y, OBJ_DRAGON_HERB);
}

bool brew_drink(World *w, uint8_t unit)
{
    Cauldron *c;
    Unit *u;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    c = brew_cauldron_at(w, u->x, u->y);
    if (!c || c->doses == 0 || c->potion == 0xFF ||
        c->potion == SP_BOMB_POTION)
        return false;
    if (!world_can_pay(w, unit, ACT_DRINK))
        return false;
    world_pay(w, unit, ACT_DRINK);
    if (!apply_potion(w, unit, c->potion, c->level ? c->level : 1))
        return false;
    if (--c->doses == 0)                 /* drunk empty */
        brew_set_cauldron(w, u->x, u->y, false, 0xFF);
    return true;
}

bool brew_fill(World *w, uint8_t unit)
{
    Cauldron *c;
    Unit *u;
    uint8_t i, kind;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->in_use == NO_ITEM || u->in_use >= u->item_count ||
        u->items[u->in_use] != OBJ_VIAL_EMPTY)
        return false;
    c = brew_cauldron_at(w, u->x, u->y);
    if (!c || c->doses == 0)
        return false;
    if (!world_can_pay(w, unit, ACT_FILL))
        return false;
    world_pay(w, unit, ACT_FILL);
    kind = c->potion == SP_BOMB_POTION ? OBJ_VIAL_BOMB : 0xFF;
    for (i = 0; i < sizeof VIALS / sizeof VIALS[0]; i++)
        if (VIALS[i].potion == c->potion)
            kind = VIALS[i].vial;
    if (kind == 0xFF)
        return false;
    u->items[u->in_use] = kind;
    if (--c->doses == 0)
        brew_set_cauldron(w, u->x, u->y, false, 0xFF);
    return true;
}

bool brew_drink_vial(World *w, uint8_t unit)
{
    Unit *u;
    uint8_t potion;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (u->in_use == NO_ITEM || u->in_use >= u->item_count)
        return false;
    potion = vial_potion(u->items[u->in_use]);
    if (potion == 0xFF)
        return false;
    if (!world_can_pay(w, unit, ACT_DRINK))
        return false;
    world_pay(w, unit, ACT_DRINK);
    if (!apply_potion(w, unit, potion, 2))
        return false;
    u->items[u->in_use] = u->items[u->item_count - 1];
    u->item_count--;
    u->in_use = NO_ITEM;
    return true;
}

bool brew_throw_vial(World *w, Rng *rng, uint8_t unit, int8_t dx, int8_t dy)
{
    Unit *u;
    uint8_t kind, thrower_kind, thrower_owner;
    int16_t x, y, dist;
    (void)rng;
    if (unit >= w->unit_count || (dx == 0 && dy == 0))
        return false;
    u = &w->units[unit];
    if (u->in_use == NO_ITEM || u->in_use >= u->item_count)
        return false;
    kind = u->items[u->in_use];
    if (kind != OBJ_VIAL_BOMB && vial_potion(kind) == 0xFF)
        return false;
    if (!world_can_pay(w, unit, ACT_THROW))
        return false;
    world_pay(w, unit, ACT_THROW);
    u->items[u->in_use] = u->items[u->item_count - 1];
    u->item_count--;
    u->in_use = NO_ITEM;
    thrower_kind = ride_actor_kind(u);   /* the blast may reorder units */
    thrower_owner = u->owner;

    x = u->x;
    y = u->y;
    {
        uint16_t flown = 0, reach = items_throw_range(w, unit, OBJECTS[kind].weight);
    for (dist = 0; dist < 36; dist++) {   /* flies until wall, unit or its reach */
        int16_t nx = (int16_t)(x + dx), ny = (int16_t)(y + dy);
        flown = (uint16_t)(flown + (dx != 0 && dy != 0 ? 3 : 2));
        if (flown > reach || !world_wrap(w, &nx, &ny) || world_blocks(w, nx, ny))
            break;
        x = nx;
        y = ny;
        if (world_unit_at(w, x, y, UL_GROUND) != NO_UNIT ||
            world_unit_at(w, x, y, UL_AIR) != NO_UNIT)
            break;                       /* shatters on the target */
    }
    }
    {   /* the vial's flight (presentation) */
        int16_t fx, fy;
        world_delta(w, w->units[unit].x, w->units[unit].y, x, y, &fx, &fy);
        events_push(EV_PROJECTILE, w->units[unit].x, w->units[unit].y,
                    PJ_THROWN, thrower_owner, (uint8_t)(int8_t)fx,
                    (uint8_t)(int8_t)fy);
    }
    {   /* D59: a friend catches the vial whole, bombs included */
        uint8_t c = world_unit_at(w, x, y, UL_GROUND);
        if (c == NO_UNIT)
            c = world_unit_at(w, x, y, UL_AIR);
        if (c != NO_UNIT && w->units[c].owner == thrower_owner &&
            items_catch(w, c, kind))
            return true;
    }
    if (kind != OBJ_VIAL_BOMB)
        return true;                     /* shatters harmlessly */
    {   /* bomb: everyone around the impact takes a hit (Amiga, D21) */
        uint8_t i;
        for (i = w->unit_count; i-- > 0;)    /* removal swaps in done units */
            if (world_distance(w, x, y, w->units[i].x, w->units[i].y) <= 1)
                combat_damage(w, i, 20, thrower_kind, thrower_owner, false, NULL);
    }
    return true;
}
