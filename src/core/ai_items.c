/* What AI creatures do with objects (K10.6): pick up by wish value, wield the
 * best weapon, eat when hurt or tired, drink vials and cauldrons, throw loot
 * to their wizard. */
#include "ai_priv.h"

#include <string.h>

#include "brew.h"
#include "effect.h"
#include "events.h"
#include "ride.h"

static bool potion_active(const Unit *u)
{
    return effect_active(u, EFF_STRENGTH) || effect_active(u, EFF_PROTECT) ||
           effect_active(u, EFF_SPEED) || effect_active(u, EFF_FLYING) ||
           effect_active(u, EFF_INVISIBLE);
}

static bool hurt(const Unit *u)
{
    return 3u * u->con < 2u * u->con_max;        /* 1.5 Con < ConMax */
}

bool ai_carries_loot(const Unit *u)
{
    uint8_t i;
    for (i = 0; i < u->item_count; i++)
        if (AI_ITEM[u->items[i]] & 0x80)
            return true;
    return false;
}

static bool has_item(const Unit *u, uint8_t kind)
{
    uint8_t i;
    for (i = 0; i < u->item_count; i++)
        if (u->items[i] == kind)
            return true;
    return false;
}

bool ai_pick_item(const World *w, uint8_t unit, const AiView *v, AiItem *out)
{
    const Unit *u = &w->units[unit];
    uint8_t actor = ride_actor_kind(u), i, best = 0;
    uint16_t carried = items_weight(w, unit);
    bool found = false;
    for (i = 0; i < v->it_n; i++) {
        const AiItem *it = &v->it[i];
        uint16_t val, score;
        if (it->kind == AI_CHEST) {
            if (!has_item(u, OBJ_CHEST_KEY))
                continue;
            val = 127;
        } else {
            uint8_t cat = OBJECTS[it->kind].category;
            val = AI_ITEM[it->kind] & 0x7F;
            if (val == 0)
                continue;
            if (cat == OC_WEAPON && !(CREATURES[actor].flags & CF_WEAPONS))
                continue;
            if (cat == OC_KEY && !(CREATURES[actor].flags & CF_USE))
                continue;
            if (it->kind == OBJ_CAULDRON_FULL) {          /* drink on the spot */
                if (potion_active(u))
                    continue;
            } else if (u->item_count >= UNIT_ITEMS ||
                       carried + OBJECTS[it->kind].weight > CREATURES[actor].carry)
                continue;
        }
        score = (uint16_t)(val / (it->dist + 1u));
        if (!found || score >= best) {
            best = (uint8_t)score;
            *out = *it;
            found = true;
        }
    }
    return found;
}

/* Index of the object of this kind on (x, y). */
static uint8_t object_at(const World *w, int16_t x, int16_t y, uint8_t kind)
{
    uint8_t i;
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].x == x && w->objects[i].y == y &&
            items_kind_of_tile(w->objects[i].tile) == kind)
            return i;
    return 0xFF;
}

AiAct ai_item_actions(World *w, Rng *rng, uint8_t id, const AiView *v)
{
    uint8_t ui = world_find_unit(w, id), i, best, actor;
    Unit *u;
    AiItem target;
    (void)rng;
    if (ui == NO_UNIT)
        return A_END;
    u = &w->units[ui];
    actor = ride_actor_kind(u);

    /* 1. a cauldron underfoot: drink the potion in it */
    {
        Cauldron *c = brew_cauldron_at(w, u->x, u->y);
        if (c && c->doses > 0 && c->potion != 0xFF && c->potion != SP_BOMB_POTION &&
            ((c->potion == SP_HEALING_POTION && hurt(u)) ||
             (c->potion != SP_HEALING_POTION && !potion_active(u))) &&
            world_can_pay(w, ui, ACT_DRINK)) {
            if ((u->flags & UF_FLYING) && !world_land(w, ui))
                return A_NONE;
            if (brew_drink(w, ui))
                return A_DONE;
            return A_DONE;               /* it landed: the next pass drinks */
        }
    }

    /* 2. standing on the object it wants: pick it up (landing first) */
    if (ai_pick_item(w, ui, v, &target) && target.kind != AI_CHEST &&
        target.kind != OBJ_CAULDRON_FULL && target.x == u->x && target.y == u->y) {
        uint8_t obj = object_at(w, u->x, u->y, target.kind);
        if (obj != 0xFF && world_can_pay(w, ui, ACT_PICK_UP)) {
            if ((u->flags & UF_FLYING) && !world_land(w, ui))
                return A_NONE;           /* cannot land here: leave it */
            if (items_pick_up_object(w, ui, obj))
                return A_DONE;
        }
    }

    /* 3. a vial: healing when hurt, anything else when no potion works */
    for (i = 0; i < u->item_count; i++) {
        uint8_t k = u->items[i];
        bool heal = k == OBJ_VIAL_HEALING;
        if (k != OBJ_VIAL_HEALING && k != OBJ_VIAL_STRENGTH && k != OBJ_VIAL_PROTECTION &&
            k != OBJ_VIAL_INVISIBILITY && k != OBJ_VIAL_SPEED && k != OBJ_VIAL_FLYING)
            continue;
        if ((heal && !hurt(u)) || (!heal && potion_active(u)))
            continue;
        if (k == OBJ_VIAL_FLYING)
            continue;                        /* a ground creature does not need wings */
        if (!world_can_pay(w, ui, ACT_DRINK))
            break;
        u->in_use = i;
        if (brew_drink_vial(w, ui))
            return A_DONE;
        break;
    }

    /* 4. eat: hurt or tired; the food with the biggest effect */
    if (hurt(u) || u->sta <= u->sta_max / 4) {
        uint8_t pick = 0xFF, bv = 0;
        for (i = 0; i < u->item_count; i++) {
            const ObjectDef *o = &OBJECTS[u->items[i]];
            uint8_t val;
            if (o->category != OC_FOOD)
                continue;
            val = actor == CR_WIZARD && o->eat_mana ? (uint8_t)(2 * o->eat_mana) : o->eat_con;
            if (val > bv || pick == 0xFF) {
                bv = val;
                pick = i;
            }
        }
        if (pick != 0xFF && world_can_pay(w, ui, ACT_EAT)) {
            u->in_use = pick;
            if (items_eat(w, ui))
                return A_DONE;
        }
    }

    /* 5. wield the best weapon it carries (not shields) */
    if (CREATURES[actor].flags & CF_WEAPONS) {
        uint8_t pick = 0xFF;
        best = 0;
        for (i = 0; i < u->item_count; i++) {
            const ObjectDef *o = &OBJECTS[u->items[i]];
            uint8_t val = AI_ITEM[u->items[i]] & 0x7F;
            if (o->category != OC_WEAPON || o->weapon == WEAPON_SHIELD)
                continue;
            if (val > best) {
                best = val;
                pick = i;
            }
        }
        if (pick != 0xFF && u->in_use != pick && world_can_pay(w, ui, ACT_CHANGE)) {
            world_pay(w, ui, ACT_CHANGE);
            u->in_use = pick;
            return A_DONE;
        }
    }
    return A_NONE;
}

AiAct ai_toss_loot(World *w, uint8_t id)
{
    uint8_t ui = world_find_unit(w, id), wi, i, kind, last;
    Unit *u, *wiz;
    uint16_t d;
    int16_t dx, dy;
    if (ui == NO_UNIT)
        return A_END;
    u = &w->units[ui];
    if (u->owner >= OWN_NEUTRAL || ride_actor_kind(u) == CR_WIZARD)
        return A_NONE;
    wi = ai_wizard_of(w, u->owner);
    if (wi == NO_UNIT || !ai_carries_loot(u))
        return A_NONE;
    wiz = &w->units[wi];
    for (i = 0; i < u->item_count; i++)
        if (AI_ITEM[u->items[i]] & 0x80)
            break;
    kind = u->items[i];
    d = world_range(w, u->x, u->y, wiz->x, wiz->y);
    if (d >= items_throw_range(w, ui, OBJECTS[kind].weight) || w->object_count >= MAX_OBJECTS)
        return A_NONE;
    if (!world_can_pay(w, ui, ACT_THROW))
        return A_NONE;
    world_pay(w, ui, ACT_THROW);
    world_delta(w, u->x, u->y, wiz->x, wiz->y, &dx, &dy);
    events_push(EV_PROJECTILE, u->x, u->y, PJ_THROWN, u->owner,
                (uint8_t)(int8_t)dx, (uint8_t)(int8_t)dy);
    last = (uint8_t)(u->item_count - 1);
    u->items[i] = u->items[last];
    u->item_count--;
    if (u->in_use == i)
        u->in_use = NO_ITEM;
    else if (u->in_use == last)
        u->in_use = i;
    w->objects[w->object_count].x = wiz->x;           /* lands on his field, harmless */
    w->objects[w->object_count].y = wiz->y;
    w->objects[w->object_count].tile = OBJECTS[kind].tile;
    w->object_count++;
    return A_DONE;
}
