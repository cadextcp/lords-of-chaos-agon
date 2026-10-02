#include "spells.h"

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
