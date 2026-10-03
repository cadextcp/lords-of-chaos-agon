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
    if (a == WA_CONSTITUTION)
        w->base_con = (uint8_t)(w->base_con + 1);
    if (a == WA_STAMINA)
        w->base_sta = (uint8_t)(w->base_sta + 1);
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
    /* stock book (same set the test scenario granted p1) */
    w->book.level[SP_GIANT_BAT] = 2;
    w->book.level[SP_MAGIC_BOLT] = 1;
    w->book.level[SP_DWARF] = 1;
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
    if (scenario <= 16 && !(w->scenarios_done & (1u << (scenario - 1)))) {
        w->scenarios_done |= (uint16_t)(1u << (scenario - 1));
        if (w->level < 8)
            w->level++;                  /* first clear: one level up */
    }
}
