/*
 * Wizard Designer and campaign (GDD 2.3/7.3/9, M4f, D22 F5/F6): four
 * wizard slots on the SD card, XP spending with per-attribute costs,
 * VP -> XP 1:1, one level up per finished scenario. Campaign transfers
 * carry attributes, spell levels and XP only - never items (F5).
 * Start values below are own design (D7) until the WinUAE reading
 * (F6) provides anchors.
 */
#ifndef LOC_WIZARD_H
#define LOC_WIZARD_H

#include <stdbool.h>
#include <stdint.h>

#include "spells.h"
#include "world.h"

#define WIZARD_SLOTS 4
#define WIZARD_NAME_MAX 16

/* Designer attributes (GDD 7.3 lists combat, defence, magic resistance,
 * constitution, stamina as the trainable set - combat/defence/magic
 * resistance here; stamina/constitution follow their maxima). */
typedef enum {
    WA_COMBAT,
    WA_DEFENCE,
    WA_MAGIC_RES,
    WA_CONSTITUTION,
    WA_STAMINA,
    WA_COUNT
} WizardAttr;

typedef struct {
    char name[WIZARD_NAME_MAX];
    uint8_t level;        /* campaign level, scenario n needs level n */
    uint16_t xp;
    uint8_t base_com, base_def, base_mr, base_con, base_sta;   /* startvals */
    uint8_t com, def, mr, con, sta;      /* current (raised with XP) */
    uint8_t mana_max;      /* raisable with XP: 9 XP per point (F6) */
    uint8_t ap;            /* action points, 8 XP per point (F6) */
    Spellbook book;
    Spellbook base_book;    /* starting levels: lowering stops here (no
                             * refund for pre-given levels) */
    uint16_t scenarios_done;   /* bitmask of scenario numbers (1..16) */
} Wizard;

/* Attribute access for the designer table. */
uint8_t wizard_attr(const Wizard *w, WizardAttr a);
/* Cost of the NEXT point of this attribute at its current value: floor(value /
 * divisor), divisors 2, 2, 16, 10, 8 (K3.3). */
uint8_t wizard_attr_cost(WizardAttr a, uint8_t current);
uint8_t wizard_attr_max(WizardAttr a);
/* Spend XP on one point; false when not enough XP or at the cap. */
bool wizard_raise(Wizard *w, WizardAttr a);
/* Take one point back (full XP refund); never below the start value. */
bool wizard_lower(Wizard *w, WizardAttr a);
/* Buyable spells (K3.3): level L to L+1 costs xp_base + xp_step * L,
 * cap 8. Spells are picked when the wizard is built or levels up - or found
 * on scrolls (D33). */
uint16_t wizard_spell_next_cost(const Wizard *w, uint8_t spell);
bool wizard_spell_raise(Wizard *w, uint8_t spell);   /* spend XP */
bool wizard_spell_lower(Wizard *w, uint8_t spell);   /* full refund */
/* The sensible starting set, applied on request at scenario start
 * (empty books otherwise). No-op when the wizard already owns Magic
 * Bolt - never overwrites a designed book. */
void wizard_apply_standard_set(Wizard *w);
/* Mana: floor(mana / 10) XP per point up to 200, start 80; it flows into the
 * unit via apply_to_world. */
uint8_t wizard_mana_cost(const Wizard *w);
bool wizard_mana_raise(Wizard *w);
bool wizard_mana_lower(Wizard *w);
/* Action points: floor(AP / 4) XP per point, minimum 34, cap 40. */
uint8_t wizard_ap_cost(const Wizard *w);
bool wizard_ap_raise(Wizard *w);
bool wizard_ap_lower(Wizard *w);
/* Sanity check for data read from the SD card (ranges, name, book). */
bool wizard_valid(const Wizard *w);

/* The four slots (RAM mirror; the frontend persists them). */
extern Wizard wizard_slots[WIZARD_SLOTS];
/* Erase a slot to the stock designer wizard. */
void wizard_slot_reset(uint8_t slot);
/* Random wizard for the "random strength" setup (own roll order). */
void wizard_slot_random(uint8_t slot, uint8_t strength, Rng *rng);

/* Load a wizard into the world as the player's unit: kind, values and
 * book come from the designer data (F5: no items). */
void wizard_apply_to_world(const Wizard *w, World *world, uint8_t unit);
/* Collect the campaign result after a scenario: VP -> XP 1:1, level up
 * when the scenario number is new (GDD 9). */
void wizard_campaign_result(Wizard *w, uint16_t vp, uint8_t scenario);
/* The slot's spellbook (frontend copies it into its books[] array). */
const Spellbook *wizard_book(const Wizard *w);

#endif
