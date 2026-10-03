/*
 * Full-screen screens (M5a): the end screen after a won or lost game.
 * Later parts add title, help and lexicon screens here.
 */
#ifndef LOC_SCREENS_H
#define LOC_SCREENS_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/game.h"

typedef struct {
    const char *name;        /* the player's wizard */
    const char *scenario;    /* scenario title, NULL on test maps */
    GameOutcome outcome;     /* OUT_WIN or OUT_LOSE */
    uint8_t rounds;
    uint16_t vp;             /* total victory points */
    uint16_t loot_vp;        /* of which carried treasure */
    uint8_t kills;
    bool campaign;           /* XP / level lines are shown */
    uint16_t xp_total;       /* XP of the wizard after the game */
    uint16_t xp_gain;
    uint8_t level;
    bool level_up;
} EndInfo;

/* Show the end screen and wait for a key. True: back to the main menu
 * (Enter/Space), false: quit the program (Esc). */
bool screen_end(const EndInfo *info);

#endif
