/*
 * Render description: a grid of cells the core writes and a platform
 * frontend draws. The core never talks to the VDP; it only marks cells
 * dirty. Frontends draw dirty cells and then call screen_clean_all().
 *
 * NOTE: `int` is 24 bits on the eZ80. Core code uses explicit-width types.
 */
#ifndef LOC_SCREEN_H
#define LOC_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 40
#define SCREEN_H 30

typedef struct {
    uint8_t glyph; /* character code; >= GLYPH_FIRST are custom glyphs */
    uint8_t fg;    /* colour index (see colors.h) */
    uint8_t bg;
} Cell;

void screen_clear(uint8_t bg);
void screen_put(uint8_t x, uint8_t y, uint8_t glyph, uint8_t fg, uint8_t bg);
void screen_text(uint8_t x, uint8_t y, const char *s, uint8_t fg, uint8_t bg);
const Cell *screen_cell(uint8_t x, uint8_t y);

bool screen_is_dirty(uint8_t x, uint8_t y);
uint16_t screen_dirty_count(void);
void screen_clean_all(void);

/* FNV-1a over all cells; used by tests to compare whole frames. */
uint32_t screen_hash(void);

/* Emit the grid as SCREEN_H lines of plain ASCII (custom glyphs mapped
 * to their ASCII fallback). `line` is NUL-terminated, no newline. */
void screen_dump(void (*emit)(const char *line));

#endif
