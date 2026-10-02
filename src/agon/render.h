/*
 * Agon renderer (GDD 11, ADR 0005): MODE 8, 24x24 tiles as VDP bitmaps,
 * 9x9 map window drawn field by field (only dirty fields), info panel and
 * message lines.
 */
#ifndef LOC_RENDER_H
#define LOC_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/world.h"

/* Screen mode, cursor off, upload tiles.bin to the VDP. False on error
 * (message already printed). */
bool render_init(void);
/* Draw all dirty view fields (layer by layer) and clean them. Returns the
 * number of fields drawn. */
uint8_t render_fields(void);
/* Info panel for one unit: portrait, name, 6 bars, ground info. */
void render_panel(const World *w, uint8_t unit);
/* One of the three message lines (0..2) below the map. */
void render_message(uint8_t line, uint8_t colour, const char *text);
void render_shutdown(void);

#endif
