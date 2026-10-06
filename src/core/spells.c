#include "spells.h"

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
    if (len < 6 || memcmp(data, "LOCS", 4) != 0 || data[4] != 1)
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

/* Summons (D34): the book level is the creature's level, not a count.
 * Casting costs the level-1 mana and spends nothing; every other spell
 * pays for its level and uses a charge up. */
uint8_t spell_cast_mana(uint8_t spell, uint8_t level)
{
    if (spell < SPELL_COUNT && SPELLS[spell].category == SPC_SUMMON)
        return spell_mana(spell, 1);
    return spell_mana(spell, level);
}

void spell_scale_creature(Unit *u, uint8_t level)
{
    uint16_t pct, v;
    if (level > SPELL_SUMMON_MAX_LEVEL)
        level = SPELL_SUMMON_MAX_LEVEL;
    if (level <= 1)
        return;
    pct = (uint16_t)(100 + SUMMON_LEVEL_PERCENT * (level - 1));
    v = (uint16_t)((uint16_t)u->com * pct / 100);
    u->com = v > 255 ? 255 : (uint8_t)v;
    v = (uint16_t)((uint16_t)u->def * pct / 100);
    u->def = v > 255 ? 255 : (uint8_t)v;
    v = (uint16_t)((uint16_t)u->con_max * pct / 100);
    u->con_max = v > 255 ? 255 : (uint8_t)v;
    u->con = u->con_max;
}

bool spell_can_cast(const World *w, const Spellbook *b, uint8_t wiz, uint8_t spell)
{
    const Unit *u;
    if (wiz >= w->unit_count || spell >= SPELL_COUNT)
        return false;
    u = &w->units[wiz];
    return ride_actor_kind(u) == CR_WIZARD && !(u->flags & UF_FLYING) &&
           b->level[spell] > 0 &&
           u->mana >= spell_cast_mana(spell, b->level[spell]) &&
           u->ap >= ACTIONS[ACT_CAST].ap;
}

uint8_t spell_summon(World *w, Spellbook *b, uint8_t wiz, uint8_t spell)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t level, mana, want, placed = 0, i, free_count = 0, kind;
    bool dragon_herb_spend = false;
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
    for (i = 0; i < 8; i++) {
        int16_t x = (int16_t)(u->x + DX[i]), y = (int16_t)(u->y + DY[i]);
        if (world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
            world_unit_at(w, x, y, UL_GROUND) == NO_UNIT)
            free_count++;
    }
    /* one creature of the book's level (D34); without a free field the
     * mana is lost (GDD 7.2) */
    want = free_count >= 1 ? 1 : 0;
    if (want == 0)
        dragon_herb_spend = false;       /* failed: the herb survives */
    else if (dragon_herb_spend)
        brew_dragon_spend(w, wiz);
    world_pay(w, wiz, ACT_CAST);
    w->units[wiz].mana = (uint8_t)(u->mana - mana);
    if (want)                             /* summons spend no level (D34) */
        events_push(EV_SPELL, u->x, u->y, spell, u->owner, 0, 0);
    for (i = 0; i < 8 && placed < want; i++) {
        int16_t x = (int16_t)(u->x + DX[i]), y = (int16_t)(u->y + DY[i]);
        if (world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
            world_unit_at(w, x, y, UL_GROUND) == NO_UNIT) {
            uint8_t slot = world_spawn_unit(w, u->owner, kind, (uint8_t)x, (uint8_t)y);
            if (slot != NO_UNIT) {
                spell_scale_creature(&w->units[slot], level);
                placed++;
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

/* One bolt-like shot at whatever stands on (x, y) (any layer): damage =
 * RND(min(255, 2 (A+1))) - Defence_eff, undead included (K5.3). The caster
 * comes by value: a lightning splash may kill the caster himself (or
 * reorder the unit list) before the remaining fields are rolled. */
static bool shoot_field(World *w, Rng *rng, const Unit *caster, int16_t x,
                        int16_t y, uint8_t attack, uint8_t *damage)
{
    uint8_t target = world_unit_at(w, x, y, UL_GROUND);
    *damage = 0;
    if (target == NO_UNIT)
        target = world_unit_at(w, x, y, UL_AIR);
    if (target == NO_UNIT)
        return false;
    *damage = combat_roll(rng, attack, items_defence(w, target));
    if (*damage == 0) {
        events_push(EV_MISS, x, y, caster->kind, caster->owner, 0, 0);
        return false;
    }
    combat_damage(w, target, *damage, caster->kind, caster->owner, false, NULL, false);
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
    world_disturb(w, x, y, w->units[wiz].owner);   /* magic scares (D37) */
    return true;
}

bool spell_bolt(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                int16_t x, int16_t y, Rng *rng, SpellShot *out)
{
    const Unit *u;
    out->allowed = out->hit = out->died = out->crit = false;
    out->damage = 0;
    out->splash_hits = 0;
    out->terrain_smashed = false;
    if (spell != SP_MAGIC_BOLT && spell != SP_MAGIC_LIGHTNING)
        return false;
    if (wiz >= w->unit_count || !world_wrap(w, &x, &y))
        return false;
    u = &w->units[wiz];
    if (!spell_in_range(w, u, spell, b->level[spell], x, y) ||
        !sight_has_spell_los(w, u->x, u->y, x, y))
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
        out->hit = shoot_field(w, rng, &caster, x, y,
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
    return true;
}

bool spell_lightning(World *w, Spellbook *b, uint8_t wiz,
                     int16_t x, int16_t y, Rng *rng, SpellShot *out)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t i, attack;
    Unit caster;
    /* massive target fields are rejected (GDD 7.2) */
    if (world_wrap(w, &x, &y) && world_blocks(w, x, y) &&
        FEATURE_TOUGH[world_feature(w, x, y)] == 0)
        return false;
    if (wiz >= w->unit_count)
        return false;
    caster = w->units[wiz];              /* before the bolt reorders units */
    attack = spell_attack_value(SP_MAGIC_LIGHTNING, b->level[SP_MAGIC_LIGHTNING]);
    if (!spell_bolt(w, b, wiz, SP_MAGIC_LIGHTNING, x, y, rng, out))
        return false;
    /* the target field and its eight neighbours: creatures take a bolt
     * each, destructible terrain breaks (K5.3) */
    if (lightning_smash(w, rng, x, y, attack))
        out->terrain_smashed = true;
    for (i = 0; i < 8; i++) {
        uint8_t dmg;
        int16_t nx = (int16_t)(x + DX[i]), ny = (int16_t)(y + DY[i]);
        uint8_t before = w->unit_count;
        if (!world_wrap(w, &nx, &ny))
            continue;
        if (shoot_field(w, rng, &caster, nx, ny, attack, &dmg))
            out->splash_hits++;
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
                      uint8_t level, int16_t *x, int16_t *y)
{
    return world_wrap(w, x, y) && spell_in_range(w, u, spell, level, *x, *y) &&
           sight_has_spell_los(w, u->x, u->y, *x, *y);
}

/* A free, walkable ground field. */
static bool free_field(const World *w, int16_t x, int16_t y)
{
    return world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
           world_unit_at(w, x, y, UL_GROUND) == NO_UNIT;
}

CastResult spell_apply(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                       int16_t x, int16_t y, Rng *rng, SpellShot *out)
{
    uint8_t level;
    Unit *u;
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
        if (!reachable(w, u, spell, level, &x, &y))
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        out->allowed = true;
        out->damage = 0;
        return CAST_OK;                /* the caller reveals the area */

    case SP_TELEPORT: {                /* inaccurate jump, 0 AP after (K5.3) */
        int16_t dist, tx, ty;
        uint16_t over;
        if (!world_wrap(w, &x, &y))
            return CAST_REJECTED;
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
        if (!free_field(w, tx, ty) || !world_wrap(w, &tx, &ty))
            return CAST_REJECTED;      /* the field is taken or solid: it fails */
        u->x = (uint8_t)tx;
        u->y = (uint8_t)ty;
        u->ap = 0;                     /* exhausted after the jump */
        return CAST_OK;
    }

    case SP_CURSE: {                   /* seven wounds (K5.3) */
        uint8_t target;
        if (!reachable(w, u, spell, level, &x, &y))
            return CAST_REJECTED;
        target = world_unit_at(w, x, y, UL_GROUND);
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
        if (!reachable(w, u, spell, level, &x, &y))
            return CAST_REJECTED;
        target = world_unit_at(w, x, y, UL_GROUND);
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
        if (!reachable(w, u, spell, level, &x, &y))
            return CAST_REJECTED;
        center = world_unit_at(w, x, y, UL_GROUND);
        if (center == NO_UNIT)
            return CAST_REJECTED;
        kind = w->units[center].kind;
        pay_for_spell(w, b, wiz, spell, x, y);
        caster_kind = u->kind;          /* the caster may die in the blast */
        caster_owner = u->owner;
        out->allowed = true;
        for (i = w->unit_count; i-- > 0;) {   /* removal swaps in done units */
            Unit *t = &w->units[i];
            if (t->kind != kind || world_range(w, x, y, t->x, t->y) >= radius)
                continue;
            if (resist_roll(rng, level, t->mr, 0)) {
                out->splash_hits++;
                combat_damage(w, i, t->con, caster_kind, caster_owner, false, NULL, false);
            }
        }
        return CAST_OK;
    }

    case SP_ENCHANT: {                 /* weapons on the field become magic, L+3 rounds */
        uint8_t i;
        if (!reachable(w, u, spell, level, &x, &y))
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        for (i = 0; i < w->unit_count; i++) {
            Unit *t = &w->units[i];
            if (t->x != x || t->y != y)
                continue;
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
        /* range only, no line of sight needed (K5.3) */
        if (!world_wrap(w, &x, &y) || !spell_in_range(w, u, spell, level, x, y))
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
