/*
 * Internals shared by the AI files (D67 KI): ai.c (wild animals, dispatch),
 * ai_creature.c (the decision loop of K10.3), ai_items.c (what creatures
 * pick up, wield, eat, drink and throw), ai_nav.c (steps, paths, doors) and
 * ai_wizard.c (the AI wizard). Not part of the public API.
 */
#ifndef LOC_AI_PRIV_H
#define LOC_AI_PRIV_H

#include "ai.h"
#include "items.h"

/* What an AI creature knows about its surroundings (K11.5): the visible
 * foes and the visible objects, nearest first, for ONE creature. */
#define AI_ENEMY_MAX 20
#define AI_ITEM_MAX 24

typedef struct {
    uint8_t id;          /* unit id */
    uint8_t dist;        /* distance of the original, 2 max + min, capped */
    uint8_t c_eff, d_eff;
} AiEnemy;

typedef struct {
    int16_t x, y;
    uint8_t kind;        /* ObjectKind, or AI_CHEST for a chest on the map */
    uint8_t dist;
} AiItem;

#define AI_CHEST 0xFE

typedef struct {
    AiEnemy en[AI_ENEMY_MAX];
    uint8_t en_n;
    AiItem it[AI_ITEM_MAX];
    uint8_t it_n;
} AiView;

/* ai_nav.c */
bool ai_clear_feature(World *w, Rng *rng, uint8_t unit, int16_t x, int16_t y);
bool ai_path_step(const World *w, uint8_t unit, int16_t tx, int16_t ty,
                  int8_t *sdx, int8_t *sdy);
/* Walk up to `steps` steps towards (tx, ty); stops on the target or beside it
 * with `beside`. Returns the unit's index, NO_UNIT when it died. */
uint8_t ai_walk_to(World *w, Rng *rng, uint8_t unit, int16_t tx, int16_t ty,
                   uint8_t steps, bool beside);
/* Remember the field the unit just left (the ring of the last 8 fields). */
void ai_visit(World *w, uint8_t unit, int16_t x, int16_t y);
bool ai_visited(const World *w, uint8_t unit, int16_t x, int16_t y);

/* ai_creature.c */
typedef struct {
    Game *game;          /* portal state, NULL for creatures without a wizard */
    uint8_t round;       /* the current round */
    Spellbook *book;     /* the wizard's book, NULL for creatures */
    AiProfile *profile;  /* his priorities, NULL: he casts nothing */
} AiEnv;
/* The decision loop of one creature (K10.3): at most 50 passes. `id` is the
 * unit id; it may die on the way. */
void ai_creature_turn(World *w, Rng *rng, const AiEnv *env, uint8_t id);
/* Does `e` count as a foe of `me` (wild animals only when hostile)? */
bool ai_is_foe(const World *w, const Unit *me, const Unit *e);
/* Build the view of one creature. */
void ai_build_view(const World *w, uint8_t unit, AiView *v);
/* The AI wizard of an owner (index) or NO_UNIT. */
uint8_t ai_wizard_of(const World *w, uint8_t owner);

/* ai_items.c */
typedef enum { A_NONE, A_DONE, A_END } AiAct;
/* One of: drink from a cauldron underfoot, pick up the target object underfoot,
 * drink a vial, eat, change weapon (K10.3 step 3). A_NONE when nothing applies. */
AiAct ai_item_actions(World *w, Rng *rng, uint8_t id, const AiView *v);
/* Best object to go for (K10.6): the largest floor(value / (D + 1)). false if none. */
bool ai_pick_item(const World *w, uint8_t unit, const AiView *v, AiItem *out);
/* A creature carrying loot throws it to its own wizard when in range. */
AiAct ai_toss_loot(World *w, uint8_t id);
bool ai_carries_loot(const Unit *u);

/* ai_wizard.c */
void ai_run_creatures(World *w, Rng *rng, Game *g, uint8_t owner, uint8_t skip_id);
/* One pass of the spell choice (K10.5): A_DONE when a spell was cast. */
AiAct ai_wizard_cast(World *w, Rng *rng, const AiEnv *env, uint8_t id, const AiView *v);

#endif
