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
    Spellbook book;
    uint16_t scenarios_done;   /* bitmask of scenario numbers (1..16) */
} Wizard;

/* Attribute access for the designer table. */
uint8_t wizard_attr(const Wizard *w, WizardAttr a);
/* Cost of the NEXT point of this attribute (F6 start values: linear). */
uint8_t wizard_attr_cost(WizardAttr a, uint8_t current);
uint8_t wizard_attr_max(WizardAttr a);
/* Spend XP on one point; false when not enough XP or at the cap. */
bool wizard_raise(Wizard *w, WizardAttr a);

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
