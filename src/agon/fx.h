/*
 * Combat and spell effects (M5c, GDD 11.4): drain the core event ring
 * and play every event as a tile overlay or as VDP sprites (projectiles,
 * spell effects, rising damage numbers - ADR 0012) plus a sound. Short timed
 * frames; keys that arrive during the show are swallowed so nothing
 * ghosts into the next input (K5).
 */
#ifndef LOC_FX_H
#define LOC_FX_H

#include "../core/sight.h"
#include "../core/world.h"

/* Scripted runs (dump/bench) disable the show: the waits would swallow
 * the scripted keys. Enabled by default. */
void fx_set_enabled(bool on);
/* Effect sprites 1..8 next to the cursor sprite; after render_init. */
void fx_init(void);
/* Slide a unit tile as a sprite from one view field to the next (the
 * caller hides the unit in the view meanwhile, view_hide_unit). */
void fx_glide(uint16_t tile, int16_t vx0, int16_t vy0, int16_t vx1, int16_t vy1);
extern bool fx_glide_on;                 /* setup switch */
/* Drain the core event ring and play every event as a tile overlay plus
 * a sound. */
void fx_drain_play(World *w, const Sight *s);

#endif
