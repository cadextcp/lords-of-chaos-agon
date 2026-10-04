#include "wizard.h"

#include "items.h"

#include <stdio.h>
#include <string.h>

Wizard wizard_slots[WIZARD_SLOTS];

/* F6 (user anchor, 2026-10-04): a fresh wizard starts at the minimum
 * distribution below and distributes 600 XP over attributes, mana, AP
 * and summon spells. Point costs: combat 2, defence 2, magic
 * resistance 4, constitution 2, stamina 4, mana 9, AP 8. */
#define START_COM 5
#define START_DEF 5
#define START_MR 70
#define START_CON 25
#define START_STA 34
#define START_MANA 90
#define START_AP 34
#define START_XP 600
static const uint8_t ATTR_COST[WA_COUNT] = {2, 2, 4, 2, 4};
#define MANA_COST 9
#define MANA_MAX 250
#define AP_COST 8
#define AP_MAX 120

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
    (void)current;
    return a < WA_COUNT ? ATTR_COST[a] : 9;   /* flat, per the anchor */
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

/* Buyable spell levels (F6 anchor, 2026-10-04): the first level costs
 * the spell's design_cost XP, every further level half of that again
 * ("jeder Level 50 % mehr"), cap 8. Only summons are buyable; the
 * starting book carries everything else. */
uint16_t wizard_spell_next_cost(const Wizard *w, uint8_t spell)
{
    uint16_t base;
    if (spell >= SPELL_COUNT || !SPELLS[spell].design_cost)
        return 0;
    base = SPELLS[spell].design_cost;
    return w->book.level[spell] == 0 ? base : (uint16_t)(base / 2 ? base / 2 : 1);
}

bool wizard_spell_raise(Wizard *w, uint8_t spell)
{
    uint16_t cost;
    if (spell >= SPELL_COUNT)
        return false;
    cost = wizard_spell_next_cost(w, spell);
    if (!cost || w->book.level[spell] >= 8 || w->xp < cost)
        return false;
    w->xp = (uint16_t)(w->xp - cost);
    w->book.level[spell]++;
    return true;
}

bool wizard_spell_lower(Wizard *w, uint8_t spell)
{
    uint16_t refund;
    if (spell >= SPELL_COUNT || w->book.level[spell] == 0)
        return false;
    w->book.level[spell]--;
    /* the level just given up cost base at level 1, half above */
    refund = w->book.level[spell] == 0 ? SPELLS[spell].design_cost
                                       : SPELLS[spell].design_cost / 2;
    if (refund == 0)
        refund = 1;
    w->xp = (uint16_t)(w->xp + refund);
    return true;
}

uint8_t wizard_mana_cost(void)
{
    return MANA_COST;
}

bool wizard_mana_raise(Wizard *w)
{
    if (w->mana_max >= MANA_MAX || w->xp < MANA_COST)
        return false;
    w->xp = (uint16_t)(w->xp - MANA_COST);
    w->mana_max++;
    return true;
}

bool wizard_mana_lower(Wizard *w)
{
    if (w->mana_max <= START_MANA)
        return false;                    /* never below the start value */
    w->mana_max--;
    w->xp = (uint16_t)(w->xp + MANA_COST);   /* full refund */
    return true;
}

uint8_t wizard_ap_cost(void)
{
    return AP_COST;
}

bool wizard_ap_raise(Wizard *w)
{
    if (w->ap >= AP_MAX || w->xp < AP_COST)
        return false;
    w->xp = (uint16_t)(w->xp - AP_COST);
    w->ap++;
    return true;
}

bool wizard_ap_lower(Wizard *w)
{
    if (w->ap <= START_AP)
        return false;                    /* never below the minimum */
    w->ap--;
    w->xp = (uint16_t)(w->xp + AP_COST);     /* full refund */
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
    if (w->mana_max < START_MANA || w->mana_max > MANA_MAX)
        return false;
    if (w->ap < START_AP || w->ap > AP_MAX)
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
    w->mana_max = START_MANA;
    w->ap = START_AP;
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
    u->mana = u->mana_max = w->mana_max;  /* raised with XP (F6) */
    u->ap = u->ap_max = w->ap;            /* designer AP, min 34 (F6) */
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
