#include "spells.h"

#include "ai.h"
#include "area.h"
#include "brew.h"
#include "combat.h"
#include "effect.h"
#include "events.h"
#include "items.h"
#include "ride.h"
#include "sight.h"

#include <string.h>

uint8_t spell_mana(uint8_t spell, uint8_t level)
{
    uint16_t m;
    if (spell >= SPELL_COUNT)
        return 0;
    if (level > SPELL_MAX_LEVEL)
        level = SPELL_MAX_LEVEL;
    m = (uint16_t)(SPELLS[spell].mana * (uint16_t)(level + 1));
    return m > 255 ? 255 : (uint8_t)m;
}

bool spellbook_load(Spellbook *books, const uint8_t *data, uint16_t len)
{
    uint8_t books_n, i, pos;
    memset(books, 0, sizeof(Spellbook) * OWN_NEUTRAL);
    if (len < 6 || memcmp(data, "LOCS", 4) != 0 || (data[4] < 1 || data[4] > 3))
        return false;
    books_n = data[5];
    pos = 6;
    for (i = 0; i < books_n; i++) {
        uint8_t who, entries, k;
        if (pos + 2 > len)
            return false;
        who = data[pos++];
        entries = data[pos++];
        if (who >= OWN_NEUTRAL)
            return false;
        for (k = 0; k < entries; k++) {
            uint8_t spell, level;
            if (pos + 2 > len)
                return false;
            spell = data[pos++];
            level = data[pos++];
            if (spell >= SPELL_COUNT || level > SPELL_MAX_LEVEL)
                return false;
            books[who].level[spell] = level;
        }
    }
    return true;
}

uint8_t spell_cast_mana(uint8_t spell, uint8_t level)
{
    return spell_mana(spell, level);
}

bool spell_needs_ground(uint8_t spell)
{
    return spell < SPELL_COUNT && (SPELLS[spell].category == SPC_SUMMON ||
                                   SPELLS[spell].category == SPC_POTION);
}

bool spell_can_cast(const World *w, const Spellbook *b, uint8_t wiz, uint8_t spell)
{
    const Unit *u;
    if (wiz >= w->unit_count || spell >= SPELL_COUNT)
        return false;
    u = &w->units[wiz];
    if ((u->flags & UF_FLYING) && spell_needs_ground(spell))
        return false;                    /* not from the air (F8) */
    return ride_actor_kind(u) == CR_WIZARD &&
           b->level[spell] > 0 &&
           u->mana >= spell_cast_mana(spell, b->level[spell]) &&
           world_can_pay(w, wiz, ACT_CAST);
}

uint8_t spell_summon(World *w, Spellbook *b, uint8_t wiz, uint8_t spell, Rng *rng)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t level, mana, placed = 0, i, kind, n;
    bool dragon_herb_spend = false, any_free = false;
    const Unit *u;
    if (!spell_can_cast(w, b, wiz, spell) || SPELLS[spell].category != SPC_SUMMON)
        return 0;
    u = &w->units[wiz];
    level = b->level[spell];
    mana = spell_cast_mana(spell, level);
    kind = SUMMON_KIND[spell];                /* one summon spell per kind */
    if (kind >= CR_COUNT)
        return 0;
    {   /* dragons need a cauldron with dragon herb (PM 21, M4c) */
        if (kind == CR_GOLD_DRAGON || kind == CR_GREEN_DRAGON ||
            kind == CR_RED_DRAGON) {
            if (!brew_dragon_ready(w, wiz))
                return 0;
            dragon_herb_spend = true;
        }
    }
    for (i = 0; i < 8 && !any_free; i++) {
        int16_t x = (int16_t)(u->x + DX[i]), y = (int16_t)(u->y + DY[i]);
        if (world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
            world_unit_at(w, x, y, UL_GROUND) == NO_UNIT)
            any_free = true;
    }
    if (!any_free)
        dragon_herb_spend = false;       /* failed: the herb survives */
    else if (dragon_herb_spend)
        brew_dragon_spend(w, wiz);
    world_pay(w, wiz, ACT_CAST);
    w->units[wiz].mana = (uint8_t)(u->mana - mana);
    b->level[spell] = (uint8_t)(level - 1);   /* the level is used up (K5.1) */
    events_push(EV_SPELL, u->x, u->y, spell, u->owner, 0, 0);
    world_noise(w, u->x, u->y, NOISE_SPELL, u->owner);   /* D69 */
    /* L creatures, each on a random free neighbour: up to 40 tries (K5.3) */
    for (n = 0; n < level && any_free; n++) {
        uint8_t tries;
        for (tries = 0; tries < 40; tries++) {
            uint8_t d = (uint8_t)rng_range(rng, 8);
            int16_t x = (int16_t)(w->units[wiz].x + DX[d]);
            int16_t y = (int16_t)(w->units[wiz].y + DY[d]);
            if (world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
                world_unit_at(w, x, y, UL_GROUND) == NO_UNIT) {
                uint8_t slot = world_spawn_unit(w, w->units[wiz].owner, kind, (uint8_t)x,
                                                (uint8_t)y);
                if (slot != NO_UNIT) {
                    ai_plan_new(w, rng, slot);
                    placed++;
                }
                break;
            }
        }
    }
    return placed;
}

uint8_t spell_attack_value(uint8_t spell, uint8_t level)
{
    return (uint8_t)(4 * level + (spell == SP_MAGIC_LIGHTNING ? 30 : 25));
}

uint8_t spell_range(uint8_t spell, uint8_t level)
{
    if (spell == SP_MAGIC_EYE)
        return (uint8_t)(3 * level + 10);
    if (spell == SP_TELEPORT)
        return (uint8_t)(2 * level + 30);
    return (uint8_t)(2 * level + 7);
}

bool spell_in_range(const World *w, const Unit *u, uint8_t spell, uint8_t level,
                    int16_t x, int16_t y)
{
    return world_range(w, u->x, u->y, x, y) <= spell_range(spell, level);
}

bool spell_line_clear(const World *w, const Unit *caster, int16_t x, int16_t y,
                      bool air)
{
    bool fly = (caster->flags & UF_FLYING) != 0;
    if (!fly && !air)                    /* tall grass does not block (D36) */
        return sight_has_spell_los(w, caster->x, caster->y, x, y);
    return sight_shot_clear(w, caster->x, caster->y, fly, x, y, air);
}

/* One bolt-like shot at the unit on (x, y) at the aimed height: damage =
 * RND(min(255, 2 (A+1))) - Defence_eff, undead included (K5.3). The caster
 * comes by value: a lightning splash may kill the caster himself (or
 * reorder the unit list) before the remaining fields are rolled. */
static bool shoot_field(World *w, Rng *rng, const Unit *caster, int16_t x,
                        int16_t y, bool air, uint8_t attack, uint8_t *damage)
{
    uint8_t target = world_unit_at(w, x, y, air ? UL_AIR : UL_GROUND);
    *damage = 0;
    if (target == NO_UNIT)
        return false;
    *damage = combat_roll(rng, attack, items_defence(w, target));
    if (*damage == 0) {
        events_push(EV_MISS, x, y, caster->kind, caster->owner, 0, 0);
        return false;
    }
    combat_damage(w, target, *damage, ride_actor_kind(caster), caster->owner, false, NULL);
    return true;
}

static bool pay_for_spell(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                          int16_t x, int16_t y)
{
    uint8_t level, mana;
    if (!spell_can_cast(w, b, wiz, spell))
        return false;
    if (!world_wrap(w, &x, &y))
        return false;
    level = b->level[spell];
    mana = spell_mana(spell, level);
    world_pay(w, wiz, ACT_CAST);
    w->units[wiz].mana = (uint8_t)(w->units[wiz].mana - mana);
    b->level[spell] = (uint8_t)(level - 1);
    events_push(EV_SPELL, x, y, spell, w->units[wiz].owner, 0, 0);
    world_noise(w, w->units[wiz].x, w->units[wiz].y, NOISE_SPELL,
                w->units[wiz].owner);              /* the caster is heard (D69) */
    world_disturb(w, x, y, w->units[wiz].owner);   /* magic scares (D37) */
    return true;
}

bool spell_bolt(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                int16_t x, int16_t y, bool air, Rng *rng, SpellShot *out)
{
    const Unit *u;
    out->allowed = out->hit = out->died = false;
    out->damage = 0;
    out->splash_hits = 0;
    out->terrain_smashed = false;
    if (spell != SP_MAGIC_BOLT && spell != SP_MAGIC_LIGHTNING)
        return false;
    if (wiz >= w->unit_count || !world_wrap(w, &x, &y))
        return false;
    u = &w->units[wiz];
    if (!spell_in_range(w, u, spell, b->level[spell], x, y) ||
        !spell_line_clear(w, u, x, y, air))
        return false;
    {
        uint8_t before = w->unit_count;
        uint8_t level = b->level[spell];   /* before the cast spends it */
        Unit caster;
        if (!pay_for_spell(w, b, wiz, spell, x, y))
            return false;
        caster = w->units[wiz];
        out->allowed = true;
        {   /* the projectile flies before it hits (presentation) */
            int16_t dx, dy;
            world_delta(w, caster.x, caster.y, x, y, &dx, &dy);
            events_push(EV_PROJECTILE, caster.x, caster.y,
                        spell == SP_MAGIC_LIGHTNING ? PJ_LIGHTNING : PJ_BOLT,
                        caster.owner, (uint8_t)(int8_t)dx, (uint8_t)(int8_t)dy);
        }
        out->hit = shoot_field(w, rng, &caster, x, y, air,
                               spell_attack_value(spell, level), &out->damage);
        out->died = w->unit_count < before;
    }
    return true;
}

/* A destructible feature on (x, y) breaks when RND(2A) >= toughness (K5.3). */
static bool lightning_smash(World *w, Rng *rng, int16_t x, int16_t y, uint8_t attack)
{
    uint8_t fe;
    if (!world_blocks(w, x, y))
        return false;
    fe = world_feature(w, x, y);
    if (FEATURE_TOUGH[fe] == 0 ||
        rng_range(rng, (uint16_t)(2 * (uint16_t)attack)) < FEATURE_TOUGH[fe])
        return false;
    events_push(EV_SMASH, x, y, fe, 0, 0, 0);
    w->feature[y][x] = FE_NONE;
    world_map_changed(w);
    world_poke(w, x, y);
    return true;
}

bool spell_lightning(World *w, Spellbook *b, uint8_t wiz,
                     int16_t x, int16_t y, bool air, Rng *rng, SpellShot *out)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t i, attack;
    Unit caster;
    /* massive target fields are rejected on the ground (GDD 7.2) */
    if (!air && world_wrap(w, &x, &y) && world_blocks(w, x, y) &&
        FEATURE_TOUGH[world_feature(w, x, y)] == 0)
        return false;
    if (wiz >= w->unit_count)
        return false;
    caster = w->units[wiz];              /* before the bolt reorders units */
    attack = spell_attack_value(SP_MAGIC_LIGHTNING, b->level[SP_MAGIC_LIGHTNING]);
    if (!spell_bolt(w, b, wiz, SP_MAGIC_LIGHTNING, x, y, air, rng, out))
        return false;
    /* the target field and its eight neighbours: creatures at the aimed
     * height take a bolt each; a ground cast breaks terrain (K5.3) */
    if (!air && lightning_smash(w, rng, x, y, attack))
        out->terrain_smashed = true;
    for (i = 0; i < 8; i++) {
        uint8_t dmg;
        int16_t nx = (int16_t)(x + DX[i]), ny = (int16_t)(y + DY[i]);
        uint8_t before = w->unit_count;
        if (!world_wrap(w, &nx, &ny))
            continue;
        if (shoot_field(w, rng, &caster, nx, ny, air, attack, &dmg))
            out->splash_hits++;
        if (!air)
            lightning_smash(w, rng, nx, ny, attack);
        if (w->unit_count < before)
            out->died = true;
    }
    return true;
}

/* Resistance roll of Curse, Subversion and Magic Attack (K5.3):
 * RND(8L + 55) + bonus >= MR. */
static bool resist_roll(Rng *rng, uint8_t level, uint8_t mr, uint8_t bonus)
{
    uint16_t n = (uint16_t)(8u * level + 55u);
    return rng_range(rng, n) + bonus >= mr;
}

/* Targeted spells need the field within reach and in sight (D17). */
static bool reachable(const World *w, const Unit *u, uint8_t spell,
                      uint8_t level, int16_t *x, int16_t *y, bool air)
{
    return world_wrap(w, x, y) && spell_in_range(w, u, spell, level, *x, *y) &&
           spell_line_clear(w, u, *x, *y, air);
}

/* A free, walkable ground field. */
static bool free_field(const World *w, int16_t x, int16_t y)
{
    return world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
           world_unit_at(w, x, y, UL_GROUND) == NO_UNIT;
}

/* A free air slot under the open sky (no roof, nobody flying there). */
static bool free_air(const World *w, int16_t x, int16_t y)
{
    return world_wrap(w, &x, &y) && !world_has_roof(w, x, y) &&
           world_unit_at(w, x, y, UL_AIR) == NO_UNIT;
}

CastResult spell_apply(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                       int16_t x, int16_t y, bool air, Rng *rng, SpellShot *out)
{
    uint8_t level;
    Unit *u;
    UnitLayer layer = air ? UL_AIR : UL_GROUND;
    memset(out, 0, sizeof *out);
    if (wiz >= w->unit_count)
        return CAST_REJECTED;
    u = &w->units[wiz];
    if (!spell_can_cast(w, b, wiz, spell))
        return CAST_REJECTED;
    level = b->level[spell];

    switch (spell) {
    case SP_MAGIC_SHIELD: {            /* self: Defence + 4 (L+1) + 12 for L+1 rounds */
        uint16_t bonus = (uint16_t)(4 * (level + 1) + 12);
        if (!world_wrap(w, &x, &y) || x != u->x || y != u->y)
            return CAST_REJECTED;      /* targets the caster only */
        pay_for_spell(w, b, wiz, spell, x, y);
        if (!effect_grant(u, EFF_SHIELD, bonus > 255 ? 255 : (uint8_t)bonus,
                          (uint8_t)(level + 1)))
            return CAST_REJECTED;      /* no effect slot free */
        return CAST_OK;
    }

    case SP_MAGIC_EYE:                 /* sight from a point, one round */
        /* the eye sees like a flyer from either height (sight_add_eye) */
        if (!reachable(w, u, spell, level, &x, &y, air))
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        out->allowed = true;
        out->damage = 0;
        out->eye_range = spell_range(SP_MAGIC_EYE, level);
        return CAST_OK;                /* the caller reveals the area */

    case SP_TELEPORT: {                /* inaccurate jump, 0 AP after (K5.3) */
        int16_t dist, tx, ty;
        uint16_t over;
        if (!world_wrap(w, &x, &y))
            return CAST_REJECTED;
        if (air && !effect_active(u, EFF_FLYING))
            return CAST_REJECTED;      /* into the air only on the potion (K5.3) */
        dist = (int16_t)world_range(w, u->x, u->y, x, y);
        if (dist > spell_range(spell, level))
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        tx = x;
        ty = y;
        over = dist >= 2 * level ? (uint16_t)((dist - 2 * level) / 2 + 1) : 0;
        if (over > 1) {                /* each axis slips by RND(n) - n/2 */
            tx = (int16_t)(tx + (int16_t)rng_range(rng, over) - (int16_t)(over / 2));
            ty = (int16_t)(ty + (int16_t)rng_range(rng, over) - (int16_t)(over / 2));
        }
        out->allowed = true;
        if (!world_wrap(w, &tx, &ty) ||
            (air ? !free_air(w, tx, ty) : !free_field(w, tx, ty)))
            return CAST_REJECTED;      /* the field is taken or solid: it fails */
        u->x = (uint8_t)tx;
        u->y = (uint8_t)ty;
        if (air)
            u->flags |= UF_FLYING;     /* arrives hovering */
        else
            u->flags &= (uint8_t)~UF_FLYING;
        u->ap = 0;                     /* exhausted after the jump */
        return CAST_OK;
    }

    case SP_CURSE: {                   /* seven wounds (K5.3) */
        uint8_t target;
        if (!reachable(w, u, spell, level, &x, &y, air))
            return CAST_REJECTED;
        target = world_unit_at(w, x, y, layer);
        if (target == NO_UNIT)
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        if (!resist_roll(rng, level, w->units[target].mr, 10)) {
            out->allowed = true;
            return CAST_NO_RES;
        }
        out->allowed = true;
        out->hit = true;
        world_set_wounds(&w->units[target], 7);
        return CAST_OK;
    }

    case SP_SUBVERSION: {              /* creature changes sides (K5.3) */
        uint8_t target, mr;
        Unit *t;
        if (!reachable(w, u, spell, level, &x, &y, air))
            return CAST_REJECTED;
        target = world_unit_at(w, x, y, layer);
        if (target == NO_UNIT)
            return CAST_REJECTED;
        t = &w->units[target];
        if (t->kind == CR_WIZARD || t->rider_kind == CR_WIZARD)
            return CAST_REJECTED;      /* wizards are immune, mounted ones too */
        mr = t->mr;
        if (t->rider_kind != 0xFF && t->rider_mr > mr)
            mr = t->rider_mr;          /* the higher of mount and rider counts */
        pay_for_spell(w, b, wiz, spell, x, y);
        if (!resist_roll(rng, level, mr, 0)) {
            out->allowed = true;
            return CAST_NO_RES;
        }
        out->allowed = true;
        out->hit = true;
        t->owner = u->owner;
        return CAST_OK;
    }

    case SP_MAGIC_ATTACK: {            /* every creature of the kind around the target, own too */
        uint8_t i;
        uint8_t kind, center, caster_kind, caster_owner;
        uint16_t radius = (uint16_t)(2 * level + 1);
        if (!reachable(w, u, spell, level, &x, &y, air))
            return CAST_REJECTED;
        center = world_unit_at(w, x, y, layer);   /* picks the kind */
        if (center == NO_UNIT)
            return CAST_REJECTED;
        kind = w->units[center].kind;
        pay_for_spell(w, b, wiz, spell, x, y);
        caster_kind = ride_actor_kind(u);   /* the caster may die in the blast */
        caster_owner = u->owner;
        out->allowed = true;
        for (i = w->unit_count; i-- > 0;) {   /* removal swaps in done units */
            Unit *t = &w->units[i];
            if (t->kind != kind || world_range(w, x, y, t->x, t->y) >= radius)
                continue;
            if (resist_roll(rng, level, t->mr, 0)) {
                out->splash_hits++;
                combat_damage(w, i, t->con, caster_kind, caster_owner, false, NULL);
            }
        }
        return CAST_OK;
    }

    case SP_ENCHANT: {                 /* weapons on the field become magic, L+3 rounds */
        uint8_t i;
        if (!reachable(w, u, spell, level, &x, &y, air))
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        for (i = 0; i < w->unit_count; i++) {
            Unit *t = &w->units[i];
            if (t->x != x || t->y != y || ((t->flags & UF_FLYING) != 0) != air)
                continue;              /* only the aimed height */
            effect_grant(t, EFF_MAGIC_WEAPON, level, (uint8_t)(level + 3));
        }
        out->allowed = true;
        return CAST_OK;
    }

    case SP_MAGIC_FIRE:                /* area effects (M4d): see area.c */
    case SP_GOOEY_BLOB:
    case SP_TANGLE_VINE:
    case SP_FLOOD: {
        AreaKind kind = spell == SP_MAGIC_FIRE ? AREA_FIRE
                      : spell == SP_GOOEY_BLOB ? AREA_BLOB
                      : spell == SP_TANGLE_VINE ? AREA_VINE : AREA_FLOOD;
        /* range only, no line of sight needed (K5.3); terrain: ground only */
        if (air || !world_wrap(w, &x, &y) ||
            !spell_in_range(w, u, spell, level, x, y))
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        out->splash_hits = area_cast(w, rng, kind, level, u->owner, x, y);
        out->allowed = true;
        out->hit = out->splash_hits > 0;
        return CAST_OK;
    }

    default:
        return CAST_REJECTED;
    }
}
