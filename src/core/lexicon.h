/*
 * Lexicon of discoveries (M5, GDD 2.4): which creatures and objects the
 * player has ever seen. Kept as bitmasks so it survives a scenario in a
 * dozen bytes (lexicon.dat on the SD card, wizards.dat pattern).
 */
#ifndef LOC_LEXICON_H
#define LOC_LEXICON_H

#include <stdbool.h>
#include <stdint.h>

#include "sight.h"
#include "world.h"

typedef struct {
    uint32_t seen_creature;    /* bit per CreatureKind */
    uint32_t seen_object[2];   /* bit per ObjectKind (40 kinds) */
} Lexicon;

void lexicon_init(Lexicon *l);
void lexicon_see_creature(Lexicon *l, uint8_t kind);
void lexicon_see_object(Lexicon *l, uint8_t kind);
bool lexicon_seen_creature(const Lexicon *l, uint8_t kind);
bool lexicon_seen_object(const Lexicon *l, uint8_t kind);
/* How many entries are discovered (end screen / tests). */
uint8_t lexicon_seen_count(const Lexicon *l);

/* Mark everything currently visible to the sight's owner: enemy and
 * neutral units (own units count as known) and objects on visible
 * fields. Cheap enough for every sight update. */
void lexicon_watch(Lexicon *l, const World *w, const Sight *s);
/* Object kind shown on a field tile (OBJECTS[].tile), -1 when no object
 * uses that tile. */
int16_t lexicon_object_kind_of_tile(uint16_t tile);

/* Serialisation for lexicon.dat: "LOCL", version, payload. Returns the
 * number of bytes written (0 when the buffer is too small). */
uint16_t lexicon_export(const Lexicon *l, uint8_t *buf, uint16_t cap);
bool lexicon_import(Lexicon *l, const uint8_t *buf, uint16_t len);

#endif
