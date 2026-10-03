/*
 * Guided tutorial (M5, GDD 2.4): a small scenario that walks a new player
 * through the game one step at a time. The engine only watches the world
 * (plus two notifications the world cannot show) and advances a step
 * counter; the frontend shows the hint line that belongs to the current
 * step. Platform-free: no RNG, no view, no side effects on the world.
 */
#ifndef LOC_TUTORIAL_H
#define LOC_TUTORIAL_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"
#include "gen/data.h"
#include "world.h"

typedef enum {
    TUT_MOVE,     /* the player wizard left its start field */
    TUT_SWITCH,   /* the player switched units (notification) */
    TUT_PICKUP,   /* the chest key left its start field */
    TUT_CHEST,    /* the chest is opened (feature gone) */
    TUT_KILL,     /* every hostile wizard unit is dead */
    TUT_SPELL,    /* a spell was cast (notification) */
    TUT_PORTAL,   /* the player wizard escaped through the portal */
    TUT_DONE,     /* every step finished */
    TUT_COUNT
} TutorialStep;

typedef struct {
    uint8_t step;               /* TutorialStep currently asked for */
    bool switch_done;           /* Tab was pressed */
    bool spell_done;            /* a spell was cast */
    uint8_t wiz_x, wiz_y;       /* player wizard at init */
    uint8_t key_x, key_y;       /* chest key at init */
    uint8_t chest_x, chest_y;   /* the chest to open */
    uint8_t enemies;            /* hostile wizard units at init */
} Tutorial;

/* Snapshot the world (wizard, chest key, chest, enemies) and start at
 * TUT_MOVE. Only meaningful on the tutorial map. */
void tutorial_init(Tutorial *t, const World *w);
/* Check the current step; advance while it holds. Returns the step that
 * became active (TUT_DONE when everything is finished). */
uint8_t tutorial_update(Tutorial *t, const World *w, const Game *g);
/* Player actions invisible in the world: pass TUT_SWITCH or TUT_SPELL.
 * Sticky: they satisfy their step whenever it comes up. */
void tutorial_notify(Tutorial *t, uint8_t event);
bool tutorial_finished(const Tutorial *t);

#endif
