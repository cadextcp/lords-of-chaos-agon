#include "wizard.h"

#include "items.h"

#include <stdio.h>
#include <string.h>

Wizard wizard_slots[WIZARD_SLOTS];

/* F6 start values (own design, D7): a stock wizard matches the
 * creatures.csv wizard row; costs rise linearly, caps keep the
 * designer honest. */
#define START_COM 10
#define START_DEF 12
#define START_MR 80
#define START_CON 30
#define START_STA 60
/* Creation budget: designing a fresh wizard distributes these XP (the
 * original gave a point pool at creation). Own value - the Amiga anchor
 * (F6) is still to be read; costs are 5 + value/4 per point. */
#define START_XP 20

uint8_t wizard_attr(const Wizard *w, WizardAttr a)
{
    switch (a) {
    case WA_COMBAT:
        return w->com;
    case WA_DEFENCE:
        return w->def;
    case WA_MAGIC_RES:
        return w->mr;
    case WA_CONSTITUTION:
        return w->con;
    default:
        return w->sta;
    }
}

uint8_t wizard_attr_cost(WizardAttr a, uint8_t current)
{
    (void)a;
    return (uint8_t)(5 + current / 4);   /* the stronger, the pricier */
}

uint8_t wizard_attr_max(WizardAttr a)
{
    switch (a) {
    case WA_COMBAT:
        return 30;
    case WA_DEFENCE:
        return 30;
    case WA_MAGIC_RES:
        return 100;
    case WA_CONSTITUTION:
        return 60;
    default:
        return 100;
    }
}

bool wizard_raise(Wizard *w, WizardAttr a)
{
    uint8_t cur = wizard_attr(w, a);
    uint8_t cost = wizard_attr_cost(a, cur);
    uint8_t *target = a == WA_COMBAT ? &w->com
                    : a == WA_DEFENCE ? &w->def
                    : a == WA_MAGIC_RES ? &w->mr
                    : a == WA_CONSTITUTION ? &w->con : &w->sta;
    if (cur >= wizard_attr_max(a) || w->xp < cost)
        return false;
    w->xp = (uint16_t)(w->xp - cost);
    *target = (uint8_t)(cur + 1);
    return true;
}

bool wizard_lower(Wizard *w, WizardAttr a)
{
    uint8_t cur = wizard_attr(w, a);
    uint8_t base = a == WA_COMBAT ? w->base_com
                 : a == WA_DEFENCE ? w->base_def
                 : a == WA_MAGIC_RES ? w->base_mr
                 : a == WA_CONSTITUTION ? w->base_con : w->base_sta;
    uint8_t *target = a == WA_COMBAT ? &w->com
                    : a == WA_DEFENCE ? &w->def
                    : a == WA_MAGIC_RES ? &w->mr
                    : a == WA_CONSTITUTION ? &w->con : &w->sta;
    if (cur <= base)
        return false;                    /* never below the start value */
    *target = (uint8_t)(cur - 1);
    w->xp = (uint16_t)(w->xp + wizard_attr_cost(a, *target));   /* full refund */
    return true;
}

bool wizard_valid(const Wizard *w)
{
    uint8_t i;
    bool terminated = false;
    for (i = 0; i < WIZARD_NAME_MAX; i++)
        if (w->name[i] == '\0')
            terminated = true;
    if (!terminated || w->level < 1 || w->level > 8)
        return false;
    for (i = 0; i < WA_COUNT; i++) {
        uint8_t v = wizard_attr(w, (WizardAttr)i);
        if (v == 0 || v > wizard_attr_max((WizardAttr)i))
            return false;
    }
    if (w->base_com > w->com || w->base_def > w->def || w->base_mr > w->mr ||
        w->base_con > w->con || w->base_sta > w->sta)
        return false;
    for (i = 0; i < SPELL_COUNT; i++)
        if (w->book.level[i] > SPELL_MAX_LEVEL)
            return false;
    return true;
}

void wizard_slot_reset(uint8_t slot)
{
    Wizard *w;
    if (slot >= WIZARD_SLOTS)
        return;
    w = &wizard_slots[slot];
    memset(w, 0, sizeof *w);
    strcpy(w->name, "Zauberer");
    w->level = 1;
    w->com = w->base_com = START_COM;
    w->def = w->base_def = START_DEF;
    w->mr = w->base_mr = START_MR;
    w->con = w->base_con = START_CON;
    w->sta = w->base_sta = START_STA;
    w->xp = START_XP;      /* creation budget for the designer */
    /* stock book: the original's starting levels (user anchor,
     * 2026-10-04) - no summons, they come from the scenario books */
    w->book.level[SP_MAGIC_EYE] = 4;
    w->book.level[SP_SPEED_POTION] = 6;
    w->book.level[SP_STRENGTH_POTION] = 6;
    w->book.level[SP_PROTECTION_POTION] = 6;
    w->book.level[SP_FLYING_POTION] = 6;
    w->book.level[SP_MAGIC_BOLT] = 6;
    w->book.level[SP_BOMB_POTION] = 8;
    w->book.level[SP_INVISIBILITY_POTION] = 8;
    w->book.level[SP_CURSE] = 8;
    w->book.level[SP_MAGIC_SHIELD] = 8;
    w->book.level[SP_MAGIC_LIGHTNING] = 8;
    w->book.level[SP_HEALING_POTION] = 9;
    w->book.level[SP_FLOOD] = 10;
    w->book.level[SP_TANGLE_VINE] = 10;
    w->book.level[SP_GOOEY_BLOB] = 10;
    w->book.level[SP_ENCHANT] = 10;
    w->book.level[SP_SUBVERSION] = 10;
    w->book.level[SP_TELEPORT] = 10;
}

void wizard_slot_random(uint8_t slot, uint8_t strength, Rng *rng)
{
    uint16_t pool;
    uint8_t rolls, k, spell;
    Wizard *w;
    wizard_slot_reset(slot);
    w = &wizard_slots[slot];
    snprintf(w->name, sizeof w->name, "Zufall-%u", slot + 1);
    /* strength budget: 3 points per strength step on random spells */
    rolls = (uint8_t)(3 * strength);
    for (k = 0; k < rolls; k++) {
        spell = (uint8_t)rng_range(rng, SPELL_COUNT);
        if (SPELLS[spell].category == SPC_POTION ||
            SPELLS[spell].category == SPC_AREA)
            continue;                    /* keep the starters meaningful */
        if (w->book.level[spell] < SPELL_MAX_LEVEL)
            w->book.level[spell]++;
    }
    pool = (uint16_t)(40 * strength);    /* and XP to spend */
    w->xp = pool;
}

void wizard_apply_to_world(const Wizard *w, World *world, uint8_t unit)
{
    Unit *u;
    if (unit >= world->unit_count)
        return;
    u = &world->units[unit];
    u->com = w->com;
    u->def = w->def;
    u->mr = w->mr;
    u->con = u->con_max = w->con;
    u->sta = u->sta_max = w->sta;
    u->mana = u->mana_max = 80;          /* stock mana pool (GDD 4.1) */
    u->item_count = 0;                   /* F5: the wizard arrives unarmed */
    u->in_use = NO_ITEM;
}

/* The spellbook of the slot (the frontend owns the books[] array). */
const Spellbook *wizard_book(const Wizard *w)
{
    return &w->book;
}

void wizard_campaign_result(Wizard *w, uint16_t vp, uint8_t scenario)
{
    w->xp = (uint16_t)(w->xp + vp);      /* 1:1 (GDD 9) */
    if (scenario >= 1 && scenario <= 16 && !(w->scenarios_done & (1u << (scenario - 1)))) {
        w->scenarios_done |= (uint16_t)(1u << (scenario - 1));
        if (w->level < 8)
            w->level++;                  /* first clear: one level up */
    }
}
