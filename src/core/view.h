/*
 * View composition (GDD 11.3): turns the world into a 9x9 window of fields,
 * each a bottom-to-top list of tile IDs (floor, half floors, decor, feature,
 * object, unit, cursor). Fields whose list changed since the last frame are
 * marked dirty; the frontend redraws only those.
 */
#ifndef LOC_VIEW_H
#define LOC_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#include "gen/tiles.h"
#include "world.h"

#define VIEW_W 9
#define VIEW_H 9
#define VIEW_MAX_LAYERS 10
#define NO_CURSOR 0xFF

typedef struct {
    uint8_t n;
    uint8_t id[VIEW_MAX_LAYERS];
} FieldLayers;

/* Forget the last frame: next view_update() marks every field dirty. */
void view_invalidate(void);
/* World position shown in the top-left field of the window. */
void view_set_origin(int16_t x, int16_t y);
int16_t view_origin_x(void);
int16_t view_origin_y(void);
/* Scroll so that (x, y) keeps a margin of 2 fields to the window edge. */
void view_follow(const World *w, int16_t x, int16_t y);
/* Cursor frame at a world position (tile e.g. T_CURSOR_GREEN), or none. */
void view_set_cursor(int16_t x, int16_t y, uint8_t tile);
/* Animation phase (e.g. candle flicker), 0 or 1. */
void view_set_phase(uint8_t phase);

/* Recompute the cached static layers (floor, walls, decor, furniture).
 * view_update() does it automatically when the World or its generation
 * (world_map_changed) changed. */
void view_rebuild(const World *w);

/* Recompute all fields; returns the number of dirty fields. */
uint8_t view_update(const World *w);
bool view_dirty(uint8_t vx, uint8_t vy);
const FieldLayers *view_field(uint8_t vx, uint8_t vy);
void view_clean(void);

/* Layers of one world position (also used by tests and the panel). */
void view_compose(const World *w, int16_t x, int16_t y, FieldLayers *out);

/* FNV-1a over all fields of the current frame (cross-platform test). */
uint32_t view_hash(void);

#endif
