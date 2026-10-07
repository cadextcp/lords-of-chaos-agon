#include "effect.h"

#include <string.h>

#include "gen/data.h"

bool effect_grant(Unit *u, uint8_t kind, uint8_t power, uint8_t rounds)
{
    uint8_t i;
    for (i = 0; i < UNIT_EFFECTS; i++)
        if (u->effects[i].rounds == 0 || u->effects[i].kind == kind) {
            u->effects[i].kind = kind;
            u->effects[i].power = power;
            u->effects[i].rounds = rounds;
            if (kind == EFF_INVISIBLE)
                u->flags |= UF_INVISIBLE;
            if (kind == EFF_MAGIC_WEAPON)
                u->flags |= UF_MAGIC_WEAPON;
            return true;
        }
    return false;
}

bool effect_active(const Unit *u, uint8_t kind)
{
    uint8_t i;
    for (i = 0; i < UNIT_EFFECTS; i++)
        if (u->effects[i].rounds > 0 && u->effects[i].kind == kind)
            return true;
    return false;
}

uint8_t effect_power(const Unit *u, uint8_t kind)
{
    uint8_t i;
    for (i = 0; i < UNIT_EFFECTS; i++)
        if (u->effects[i].rounds > 0 && u->effects[i].kind == kind)
            return u->effects[i].power;
    return 0;
}

bool effect_tick(Unit *u)
{
    bool changed = false;
    uint8_t i;
    bool invis = false, magic = false;
    for (i = 0; i < UNIT_EFFECTS; i++) {
        if (u->effects[i].rounds == 0)
            continue;
        if (u->effects[i].rounds > 1) {
            u->effects[i].rounds--;
        } else {
            u->effects[i].rounds = 0;    /* expired */
            changed = true;
        }
    }
    for (i = 0; i < UNIT_EFFECTS; i++) { /* flags follow their effects */
        if (u->effects[i].rounds == 0)
            continue;
        if (u->effects[i].kind == EFF_INVISIBLE)
            invis = true;
        if (u->effects[i].kind == EFF_MAGIC_WEAPON)
            magic = true;
    }
    if (!invis && u->kind != CR_PIXIE)
        u->flags &= (uint8_t)~UF_INVISIBLE;
    if (!magic)
        u->flags &= (uint8_t)~UF_MAGIC_WEAPON;
    return changed;
}
