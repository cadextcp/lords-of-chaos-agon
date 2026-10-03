/*
 * Full-screen screens (M5a): the end screen after a won or lost game.
 * M5 adds the help page viewer, the lexicon and (later) the title screen.
 */
#ifndef LOC_SCREENS_H
#define LOC_SCREENS_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/game.h"
#include "../core/lexicon.h"

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

/* Paged help from /loc/help/<name>.hlp: Left/Right turn pages, Esc (or
 * Enter) returns. False when the file is missing or invalid (the caller
 * may fall back to the built-in key list). The screen is cleared; redraw
 * the game with view_invalidate() afterwards. */
bool screen_help(const char *file);

/* The tutorial hint line for the current step (first body line of the
 * step's page in /loc/help/tutorial.hlp). "" while unset. Load the file
 * once with tutorial_hints_load() before the scenario starts. */
bool tutorial_hints_load(const char *file);
const char *tutorial_hint_line(uint8_t step);

/* The lexicon of discoveries: list of creatures and objects, Enter shows
 * the detail page (portrait, values, description). Blocking, Esc leaves. */
void screen_lexicon(const Lexicon *lex);

/* Title screen (M5d): the streamed title bitmap (/loc/title.bin) and the
 * title music (/loc/title.bin's neighbour music/title.bin). Any key
 * stops the music and returns. */
bool screen_title(void);

#endif
