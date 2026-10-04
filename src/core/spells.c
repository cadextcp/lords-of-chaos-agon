#include "spells.h"

#include "area.h"
#include "brew.h"
#include "combat.h"
#include "effect.h"
#include "events.h"
#include "items.h"
#include "sight.h"

#include <string.h>

uint8_t spell_mana(uint8_t spell, uint8_t level)
{
    uint16_t m;
    if (spell >= SPELL_COUNT)
        return 0;
    if (level > SPELL_MAX_LEVEL)
        level = SPELL_MAX_LEVEL;
    m = (uint16_t)(SPELLS[spell].mana_base + (uint16_t)level * SPELLS[spell].mana_step);
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
    return u->kind == CR_WIZARD && !(u->flags & UF_FLYING) &&
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
    world_spend(w, wiz, ACTIONS[ACT_CAST].ap);
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

/* One bolt-like shot at whatever stands on (x, y) (any layer). The
 * caster comes by value: a lightning splash may kill the caster himself
 * (or reorder the unit list) before the remaining fields are rolled. */
static bool shoot_field(World *w, Rng *rng, const Unit *caster, int16_t x,
                        int16_t y, uint8_t dice_n, uint8_t die,
                        uint8_t *damage, bool *crit)
{
    uint8_t target = world_unit_at(w, x, y, UL_GROUND);
    uint8_t i;
    uint16_t d = 0;
    uint16_t roll;
    *damage = 0;
    if (crit)
        *crit = false;
    if (target == NO_UNIT)
        target = world_unit_at(w, x, y, UL_AIR);
    if (target == NO_UNIT)
        return false;
    roll = rng_range(rng, 100);
    if (roll >= combat_hit_chance(caster->com,
                                 items_defence_noshield(w, target))) {
        events_push(EV_MISS, x, y, caster->kind, caster->owner, 0, 0);
        return false;
    }
    {
        bool is_crit = roll < COMBAT_CRIT_PERCENT;
        if (is_crit)                       /* critical: dice twice (D30) */
            dice_n = (uint8_t)(dice_n * 2);
        if (crit)
            *crit = is_crit;
        /* the spell's damage dice (D28): magic outdamages a weapon swing */
        for (i = 0; i < dice_n; i++)
            d = (uint16_t)(d + rng_range(rng, die) + 1);
        if (d == 0)
            d = 1;
        *damage = (uint8_t)d;
        combat_damage(w, target, *damage, caster->kind, caster->owner, false,
                      NULL, is_crit);
    }
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
    world_spend(w, wiz, ACTIONS[ACT_CAST].ap);
    w->units[wiz].mana = (uint8_t)(w->units[wiz].mana - mana);
    b->level[spell] = (uint8_t)(level - 1);
    events_push(EV_SPELL, x, y, spell, w->units[wiz].owner, 0, 0);
    world_disturb(w, x, y, w->units[wiz].owner);   /* magic scares (D37) */
    return true;
}

static bool in_range(const World *w, const Unit *u, int16_t x, int16_t y)
{
    int16_t dx = (int16_t)(x - u->x), dy = (int16_t)(y - u->y);
    if (w->wrap) {
        if (dx > w->w / 2) dx = (int16_t)(dx - w->w);
        if (dx < -w->w / 2) dx = (int16_t)(dx + w->w);
        if (dy > w->h / 2) dy = (int16_t)(dy - w->h);
        if (dy < -w->h / 2) dy = (int16_t)(dy + w->h);
    }
    return dx >= -SPELL_RANGE && dx <= SPELL_RANGE &&
           dy >= -SPELL_RANGE && dy <= SPELL_RANGE;
}

bool spell_bolt(World *w, Spellbook *b, uint8_t wiz, uint8_t spell,
                int16_t x, int16_t y, Rng *rng, SpellShot *out)
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
    if (!in_range(w, u, x, y) || !sight_has_spell_los(w, u->x, u->y, x, y))
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
        /* D&D-style upcast (D29): the dice grow with the book level the
         * spell is cast at - the first charge of a full book hits hardest */
        out->hit = shoot_field(w, rng, &caster, x, y,
                               (uint8_t)(SPELLS[spell].dice_n + level),
                               SPELLS[spell].die, &out->damage, &out->crit);
        out->died = w->unit_count < before;
    }
    return true;
}

bool spell_lightning(World *w, Spellbook *b, uint8_t wiz,
                     int16_t x, int16_t y, Rng *rng, SpellShot *out)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t i;
    Unit caster;
    /* massive target fields are rejected (GDD 7.2) */
    if (world_wrap(w, &x, &y) && world_blocks(w, x, y) &&
        FEATURE_TOUGH[world_feature(w, x, y)] == 0)
        return false;
    if (wiz >= w->unit_count)
        return false;
    caster = w->units[wiz];              /* before the bolt reorders units */
    if (!spell_bolt(w, b, wiz, SP_MAGIC_LIGHTNING, x, y, rng, out))
        return false;
    /* smash destructible terrain at the target */
    if (world_blocks(w, x, y) && FEATURE_TOUGH[world_feature(w, x, y)] > 0) {
        events_push(EV_SMASH, x, y, world_feature(w, x, y), 0, 0, 0);
        w->feature[y][x] = FE_NONE;
        world_map_changed(w);
        out->terrain_smashed = true;
    }
    for (i = 0; i < 8; i++) {
        uint8_t dmg;
        int16_t nx = (int16_t)(x + DX[i]), ny = (int16_t)(y + DY[i]);
        uint8_t before = w->unit_count;
        if (!world_wrap(w, &nx, &ny))
            continue;
        if (shoot_field(w, rng, &caster, nx, ny, SPELLS[SP_MAGIC_LIGHTNING].splash_n,
                        SPELLS[SP_MAGIC_LIGHTNING].splash_die, &dmg, NULL))
            out->splash_hits++;
        if (w->unit_count < before)
            out->died = true;
    }
    return true;
}

/* F2: resistance chances, D16 style. */
static bool resist_roll(Rng *rng, uint8_t level, uint8_t mr, int8_t bonus)
{
    int16_t p = (int16_t)(50 + 5 * ((int16_t)(4 * level) - mr / 4) + bonus);
    if (p < 10)
        p = 10;
    if (p > 90)
        p = 90;
    return rng_range(rng, 100) < (uint16_t)p;
}

/* Targeted spells need the field within SPELL_RANGE and in sight (D17);
 * only Magic Fire will do without (GDD 7.2). */
static bool reachable(const World *w, const Unit *u, int16_t *x, int16_t *y)
{
    return world_wrap(w, x, y) && in_range(w, u, *x, *y) &&
           sight_has_spell_los(w, u->x, u->y, *x, *y);
}

/* Find a free, non-massive landing field near (x, y) for Teleport. */
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

    switch (spell) {
    case SP_MAGIC_SHIELD:              /* self: +2*level def for 2*level rounds */
        if (!world_wrap(w, &x, &y) || x != u->x || y != u->y)
            return CAST_REJECTED;      /* targets the caster only */
        level = b->level[spell];
        pay_for_spell(w, b, wiz, spell, x, y);
        if (!effect_grant(u, EFF_SHIELD, (uint8_t)(2 * level),
                          (uint8_t)(2 * level)))
            return CAST_REJECTED;      /* no effect slot free */
        return CAST_OK;

    case SP_MAGIC_EYE:                 /* sight from a point, one round */
        if (!world_wrap(w, &x, &y) || !in_range(w, u, x, y) ||
            !sight_has_spell_los(w, u->x, u->y, x, y))
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        out->allowed = true;
        out->damage = 0;
        return CAST_OK;                /* the caller reveals the area */

    case SP_TELEPORT: {                /* inaccurate jump, 0 AP after */
        int16_t dist;
        if (!world_wrap(w, &x, &y))
            return CAST_REJECTED;
        dist = world_distance(w, u->x, u->y, x, y);   /* Chebyshev (D17) */
        if (dist > SPELL_RANGE)
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        {   /* F3: scatter up to dist/4 fields, onto a free field */
            uint8_t tries = 0;
            int16_t tx = x, ty = y;
            do {
                if (tries) {            /* -s..+s, s = dist/4 but at least 1 */
                    int16_t s = (int16_t)(dist / 4 > 0 ? dist / 4 : 1);
                    tx = (int16_t)(x + (int16_t)rng_range(rng, (uint16_t)(2 * s + 1)) - s);
                    ty = (int16_t)(y + (int16_t)rng_range(rng, (uint16_t)(2 * s + 1)) - s);
                }
                tries++;
            } while (!free_field(w, tx, ty) && tries < 12);
            if (!free_field(w, tx, ty))
                return CAST_REJECTED;  /* massive or busy: fails (GDD 7.2) */
            u->x = (uint8_t)tx;
            u->y = (uint8_t)ty;
        }
        u->ap = 0;                     /* exhausted after the jump */
        out->allowed = true;
        return CAST_OK;
    }

    case SP_CURSE: {                   /* deadly wound (GDD 7.2) */
        uint8_t target;
        if (!reachable(w, u, &x, &y))
            return CAST_REJECTED;
        target = world_unit_at(w, x, y, UL_GROUND);
        if (target == NO_UNIT)
            return CAST_REJECTED;
        level = b->level[spell];
        pay_for_spell(w, b, wiz, spell, x, y);
        if (!resist_roll(rng, level, w->units[target].mr, 20)) {
            out->allowed = true;
            return CAST_NO_RES;
        }
        out->allowed = true;
        out->hit = true;
        w->units[target].flags |= UF_WOUNDED;
        return CAST_OK;
    }

    case SP_SUBVERSION: {              /* creature changes sides */
        uint8_t target;
        Unit *t;
        if (!reachable(w, u, &x, &y))
            return CAST_REJECTED;
        target = world_unit_at(w, x, y, UL_GROUND);
        if (target == NO_UNIT)
            return CAST_REJECTED;
        t = &w->units[target];
        if (t->kind == CR_WIZARD || (t->flags & UF_MOUNT))
            return CAST_REJECTED;      /* not on wizards or mounts (GDD 7.2) */
        level = b->level[spell];
        pay_for_spell(w, b, wiz, spell, x, y);
        if (!resist_roll(rng, level, t->mr, 0)) {
            out->allowed = true;
            return CAST_NO_RES;
        }
        out->allowed = true;
        out->hit = true;
        t->owner = u->owner;
        return CAST_OK;
    }

    case SP_MAGIC_ATTACK: {            /* whole kind in the area, own too */
        uint8_t i, radius = 2;
        uint8_t kind, center, caster_kind, caster_owner;
        if (!reachable(w, u, &x, &y))
            return CAST_REJECTED;
        center = world_unit_at(w, x, y, UL_GROUND);
        if (center == NO_UNIT)
            return CAST_REJECTED;
        kind = w->units[center].kind;
        level = b->level[spell];
        pay_for_spell(w, b, wiz, spell, x, y);
        caster_kind = u->kind;          /* the caster may die in the blast */
        caster_owner = u->owner;
        out->allowed = true;
        for (i = w->unit_count; i-- > 0;) {   /* removal swaps in done units */
            Unit *t = &w->units[i];
            if (t->kind != kind || world_distance(w, x, y, t->x, t->y) > radius)
                continue;
            if (resist_roll(rng, level, t->mr, -10)) {
                out->splash_hits++;
                combat_damage(w, i, t->con, caster_kind, caster_owner, false, NULL, false);
            }
        }
        return CAST_OK;
    }

    case SP_ENCHANT: {                 /* weapons on the field become magic */
        uint8_t i;
        if (!reachable(w, u, &x, &y))
            return CAST_REJECTED;
        level = b->level[spell];
        pay_for_spell(w, b, wiz, spell, x, y);
        for (i = 0; i < w->unit_count; i++) {
            Unit *t = &w->units[i];
            if (t->x != x || t->y != y)
                continue;
            effect_grant(t, EFF_MAGIC_WEAPON, level, (uint8_t)(2 * level));
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
        if (!reachable(w, u, &x, &y))
            return CAST_REJECTED;
        level = b->level[spell];
        if (!area_cast(w, kind, level, u->owner, x, y))
            return CAST_BAD_TERRAIN;     /* field refuses: nothing paid */
        pay_for_spell(w, b, wiz, spell, x, y);
        out->allowed = true;
        return CAST_OK;
    }

    default:
        return CAST_REJECTED;
    }
}
