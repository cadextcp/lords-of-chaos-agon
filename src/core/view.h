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
#include "sight.h"
#include "world.h"

/* Hidden map for composition: unexplored = black, remembered = raster
 * overlay, enemy units only in sight. NULL (default) shows everything. */
void view_set_sight(const Sight *s);
/* Open portal at a world position (-1 = none); drawn as an animated
 * layer above the static terrain (GDD 9). */
void view_set_portal(int16_t x, int16_t y);

#define VIEW_W 9
#define VIEW_H 9
#define VIEW_MAX_LAYERS 12   /* floor, 4 half floors, decor, feature, object, unit (+ rider), sight overlay, cursor */
#define NO_CURSOR 0xFF

typedef struct {
    uint8_t n;
    uint16_t air;                   /* bit i: layer i is an airborne unit */
    uint16_t ride;                  /* bit i: layer i is a rider behind its mount (drawn higher) */
    uint16_t id[VIEW_MAX_LAYERS];   /* TileId, bottom to top */
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
void view_set_cursor(int16_t x, int16_t y, uint16_t tile);
/* Animation phase (e.g. candle flicker), 0 or 1. */
void view_set_phase(uint8_t phase);

/* Recompute the cached static layers (floor, walls, decor, furniture).
 * view_update() does it automatically when the World or its generation
 * (world_map_changed) changed. */
void view_rebuild(const World *w);

/* Recompute all fields; returns the number of dirty fields. */
uint8_t view_update(const World *w);
/* Switch the animation phase of the current frame without recomposing:
 * only fields with animated tiles (candles) change and become dirty.
 * Returns the number of dirty fields. Equivalent to view_set_phase() +
 * view_update() when nothing else changed. */
uint8_t view_animate(uint8_t phase);
bool view_dirty(uint8_t vx, uint8_t vy);
/* Force one view field to repaint (fx overlays, M5c). */
void view_mark_dirty(uint8_t vx, uint8_t vy);
/* Leave the unit with this stable id out of the composition while the
 * frontend animates it (NO_UNIT = none). Presentation only. */
void view_hide_unit(uint8_t id);
const FieldLayers *view_field(uint8_t vx, uint8_t vy);
void view_clean(void);

/* Layers of one world position (also used by tests and the panel). */
void view_compose(const World *w, int16_t x, int16_t y, FieldLayers *out);

/* FNV-1a over all fields of the current frame (cross-platform test). */
uint32_t view_hash(void);

/* The animation partner table (anim_pair) is maintained by hand next to
 * the ANIM_A/ANIM_B lists. True when the two agree for every tile id.
 * For the selftest - drift would silently freeze an animation. */
bool view_anim_table_ok(void);

#endif
