/*
 * Custom 8x8 glyph set. Codes start at GLYPH_FIRST so the normal ASCII
 * font stays usable for text. Each glyph has an ASCII fallback used by
 * the host terminal frontend and by screen_dump().
 */
#ifndef LOC_GLYPHS_H
#define LOC_GLYPHS_H

#include <stdint.h>

enum {
    GLYPH_FIRST = 128,
    G_WIZARD = GLYPH_FIRST,
    G_TREE,
    G_WALL,
    G_WATER,
    G_FIRE,
    G_GRASS,
    GLYPH_END
};

typedef struct {
    uint8_t code;
    char ascii;
    uint8_t rows[8]; /* top row first, MSB = leftmost pixel */
} GlyphDef;

extern const GlyphDef glyph_defs[];
extern const uint8_t glyph_count;

char glyph_ascii(uint8_t code);

#endif
