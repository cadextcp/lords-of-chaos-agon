/*
 * Save game serialization (GDD 2.3, M4i): one binary blob holding the
 * world, the turn state, the game (portal/VP/eye), the spellbooks and
 * the loads-left counter, the area effects and the explored map. Written
 * at the round end; loading restores the exact state (verified by a
 * hash). The blob is a flat image of the structs (same build only): the
 * turn state carries function pointers that the caller re-binds after a
 * load, and the length gate refuses images of another build.
 */
#ifndef LOC_SAVE_H
#define LOC_SAVE_H

#include <stdbool.h>
#include <stdint.h>

#include "area.h"
#include "game.h"
#include "sight.h"
#include "spells.h"
#include "turn.h"
#include "world.h"

#define SAVE_AREAS 4              /* one area per kind at most */
#define SAVE_BUF_SIZE 12288       /* room for the largest blob (46x46, D64) */

typedef struct {
    World world;
    Turns turns;
    Game game;
    Spellbook books[OWN_NEUTRAL];
    uint8_t loads_left;       /* 5 charges (F8), 0xFF = unlimited */
    uint8_t explored[MAP_MAX_H][SIGHT_COLS];   /* hidden map of player 1 */
    uint8_t area_count;
    Area areas[SAVE_AREAS];
} SaveGame;

/* Serialize into out (cap bytes); returns the length, 0 on overflow. */
uint16_t save_serialize(const SaveGame *s, uint8_t *out, uint16_t cap);
/* Parse a blob back; false on any mismatch. */
bool save_deserialize(SaveGame *s, const uint8_t *in, uint16_t len);
/* May this savegame be loaded? False when its charges are used up
 * (GDD 2.3, F8: 0xFF means the rule is off). */
bool save_may_load(const SaveGame *s);
/* FNV-1a over the serialized blob (acceptance: save -> load -> same). */
uint32_t save_hash(const SaveGame *s);

#endif
