/*
 * Agon renderer (GDD 11, ADR 0005): MODE 8, 24x24 tiles as VDP bitmaps,
 * 9x9 map window drawn field by field (only dirty fields), info panel and
 * message lines.
 */
#ifndef LOC_RENDER_H
#define LOC_RENDER_H

#define MAP_PX 216   /* 9 fields x 24 px (matches render.c) */

#include <stdbool.h>
#include <stdint.h>

#include "../core/sight.h"
#include "../core/spells.h"
#include "../core/world.h"

/* Screen mode, cursor off, upload tiles.bin to the VDP. False on error
 * (message already printed). */
bool render_init(void);
/* Draw all dirty view fields (layer by layer) and clean them. Returns the
 * number of fields drawn. */
uint8_t render_fields(void);
/* Info panel for one unit: portrait, name, 6 bars, ground info. */
void render_panel(const World *w, uint8_t unit);
/* Look mode: the examined field (unit panel when one is visible). */
void render_panel_at(const World *w, const Sight *s, int16_t x, int16_t y);
/* Cursor frame as a VDP sprite (GDD 11.2): drawn over the map, so moving
 * or blinking it redraws no fields. Colours follow the Amiga code. */
/* Frame order = upload order in render_init(); blue = unit in the air
 * (GDD 11.2). */
typedef enum { CURSOR_GREEN, CURSOR_WHITE, CURSOR_YELLOW, CURSOR_RED,
               CURSOR_BLUE } CursorColour;
void render_cursor(int16_t vx, int16_t vy, uint8_t colour, bool visible);

/* Spell list overlay (GDD 5.1: lists over the map window). Only spells
 * with a level left; letters a.. pick, Esc closes. Redraw via
 * view_invalidate + render_fields afterwards. */
void render_spell_list(const Spellbook *book);
/* One of the three message lines (0..2) below the map. */
void render_message(uint8_t line, uint8_t colour, const char *text);
/* Menu helpers (M4f): black out the map window, write one text cell. */
void render_menu_clear(void);
void render_menu_text(uint8_t col, uint8_t row, uint8_t colour,
                      const char *text);
void render_shutdown(void);

#endif
