#include "spells.h"

#include "combat.h"
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

void spellbook_default(Spellbook *b, uint8_t owner)
{
    memset(b, 0, sizeof *b);
    if (owner == OWN_P1) {
        b->level[SP_GIANT_BAT] = 2;
        b->level[SP_MAGIC_BOLT] = 1;
        b->level[SP_DWARF] = 1;
    } else if (owner == OWN_P2) {
        b->level[SP_GOBLIN] = 2;
        b->level[SP_MAGIC_BOLT] = 1;
    }
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
