#include "spells.h"

#include "combat.h"
#include "effect.h"
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

bool spell_can_cast(const World *w, const Spellbook *b, uint8_t wiz, uint8_t spell)
{
    const Unit *u;
    if (wiz >= w->unit_count || spell >= SPELL_COUNT)
        return false;
    u = &w->units[wiz];
    return u->kind == CR_WIZARD && !(u->flags & UF_FLYING) &&
           b->level[spell] > 0 &&
           u->mana >= spell_mana(spell, b->level[spell]) &&
           u->ap >= ACTIONS[ACT_CAST].ap;
}

uint8_t spell_summon(World *w, Spellbook *b, uint8_t wiz, uint8_t spell)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t level, mana, want, placed = 0, i, free_count = 0, kind;
    const Unit *u;
    if (!spell_can_cast(w, b, wiz, spell) || SPELLS[spell].category != SPC_SUMMON)
        return 0;
    u = &w->units[wiz];
    level = b->level[spell];
    mana = spell_mana(spell, level);
    kind = SUMMON_KIND[spell];                /* one summon spell per kind */
    if (kind >= CR_COUNT)
        return 0;
    for (i = 0; i < 8; i++) {
        int16_t x = (int16_t)(u->x + DX[i]), y = (int16_t)(u->y + DY[i]);
        if (world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
            world_unit_at(w, x, y, UL_GROUND) == NO_UNIT)
            free_count++;
    }
    /* all or nothing: not enough room for the whole level and the mana
     * is lost (GDD 7.2) */
    want = free_count >= level ? level : 0;
    world_spend(w, wiz, ACTIONS[ACT_CAST].ap);
    w->units[wiz].mana = (uint8_t)(u->mana - mana);
    b->level[spell] = (uint8_t)(level - 1);
    for (i = 0; i < 8 && placed < want; i++) {
        int16_t x = (int16_t)(u->x + DX[i]), y = (int16_t)(u->y + DY[i]);
        if (world_wrap(w, &x, &y) && !world_blocks(w, x, y) &&
            world_unit_at(w, x, y, UL_GROUND) == NO_UNIT &&
            world_spawn_unit(w, u->owner, kind, (uint8_t)x, (uint8_t)y) != NO_UNIT) {
            placed++;
        }
    }
    return placed;
}

/* One bolt-like shot at whatever stands on (x, y) (any layer). The
 * caster comes by value: a lightning splash may kill the caster himself
 * (or reorder the unit list) before the remaining fields are rolled. */
static bool shoot_field(World *w, Rng *rng, const Unit *caster, int16_t x,
                        int16_t y, uint8_t *damage)
{
    uint8_t target = world_unit_at(w, x, y, UL_GROUND);
    *damage = 0;
    if (target == NO_UNIT)
        target = world_unit_at(w, x, y, UL_AIR);
    if (target == NO_UNIT)
        return false;
    if (rng_range(rng, 100) >= combat_hit_chance(caster->com,
                                                 items_defence(w, target)))
        return false;
    *damage = (uint8_t)((caster->com +
                         rng_range(rng, (uint16_t)(caster->com + 1))) / 4);
    if (*damage == 0)
        *damage = 1;
    combat_damage(w, target, *damage, caster->kind, caster->owner, false, NULL);
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
    if (!in_range(w, u, x, y) || !sight_has_los(w, u->x, u->y, x, y))
        return false;
    {
        uint8_t before = w->unit_count;
        Unit caster;
        if (!pay_for_spell(w, b, wiz, spell, x, y))
            return false;
        caster = w->units[wiz];
        out->allowed = true;
        out->hit = shoot_field(w, rng, &caster, x, y, &out->damage);
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
        if (shoot_field(w, rng, &caster, nx, ny, &dmg))
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
            !sight_has_los(w, u->x, u->y, x, y))
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        out->allowed = true;
        out->damage = 0;
        return CAST_OK;                /* the caller reveals the area */

    case SP_TELEPORT: {                /* inaccurate jump, 0 AP after */
        int16_t dx, dy, dist;
        if (!world_wrap(w, &x, &y))
            return CAST_REJECTED;
        dx = (int16_t)(x - u->x);
        dy = (int16_t)(y - u->y);
        if (w->wrap) {
            if (dx > w->w / 2) dx = (int16_t)(dx - w->w);
            if (dx < -w->w / 2) dx = (int16_t)(dx + w->w);
            if (dy > w->h / 2) dy = (int16_t)(dy - w->h);
            if (dy < -w->h / 2) dy = (int16_t)(dy + w->h);
        }
        dist = (int16_t)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
        if (dist > SPELL_RANGE)
            return CAST_REJECTED;
        pay_for_spell(w, b, wiz, spell, x, y);
        {   /* F3: scatter up to dist/4 fields, onto a free field */
            uint8_t tries = 0;
            int16_t tx = x, ty = y;
            do {
                if (tries) {
                    tx = (int16_t)(x + (int16_t)rng_range(rng, dist / 4 + 1) -
                                   (int16_t)(dist / 8 + 1));
                    ty = (int16_t)(y + (int16_t)rng_range(rng, dist / 4 + 1) -
                                   (int16_t)(dist / 8 + 1));
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
        uint8_t target = world_unit_at(w, x, y, UL_GROUND);
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
        uint8_t target = world_unit_at(w, x, y, UL_GROUND);
        Unit *t;
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
        uint8_t kind;
        uint8_t center = world_unit_at(w, x, y, UL_GROUND);
        if (center == NO_UNIT)
            return CAST_REJECTED;
        kind = w->units[center].kind;
        level = b->level[spell];
        pay_for_spell(w, b, wiz, spell, x, y);
        out->allowed = true;
        for (i = w->unit_count; i-- > 0;) {
            Unit *t = &w->units[i];
            int16_t ddx = (int16_t)(t->x - x), ddy = (int16_t)(t->y - y);
            if (t->kind != kind)
                continue;
            if (ddx < 0) ddx = (int16_t)(-ddx);
            if (ddy < 0) ddy = (int16_t)(-ddy);
            if (ddx > radius || ddy > radius)
                continue;
            if (resist_roll(rng, level, t->mr, -10)) {
                out->splash_hits++;
                combat_damage(w, i, t->con, u->kind, u->owner, false, NULL);
            }
        }
        return CAST_OK;
    }

    case SP_ENCHANT: {                 /* weapons on the field become magic */
        uint8_t i;
        if (!world_wrap(w, &x, &y))
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

    default:
        return CAST_REJECTED;
    }
}
