#include "spells.h"

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
