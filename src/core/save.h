/*
 * Save game serialization (GDD 2.3, M4i): one binary blob holding the
 * world, the turn state, the game (portal/VP/eye), the spellbooks and
 * the loads-left counter. Written at the round end; loading restores
 * the exact state (verified by a hash). LOCS v1, little-endian u8
 * fields only.
 */
#ifndef LOC_SAVE_H
#define LOC_SAVE_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"
#include "spells.h"
#include "turn.h"
#include "world.h"

typedef struct {
    World world;
    Turns turns;
    Game game;
    Spellbook books[OWN_NEUTRAL];
    uint8_t loads_left;       /* 5 charges (F8), 0xFF = unlimited */
} SaveGame;

/* Serialize into out (cap bytes); returns the length, 0 on overflow. */
uint16_t save_serialize(const SaveGame *s, uint8_t *out, uint16_t cap);
/* Parse a blob back; false on any mismatch. */
bool save_deserialize(SaveGame *s, const uint8_t *in, uint16_t len);
/* FNV-1a over the serialized blob (acceptance: save -> load -> same). */
uint32_t save_hash(const SaveGame *s);

#endif
