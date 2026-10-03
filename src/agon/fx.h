/*
 * Combat and spell effects (M5c, GDD 11.4): drain the core event ring
 * and play every event as a tile overlay plus a sound. Short timed
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
/* Drain the core event ring and play every event as a tile overlay plus
 * a sound. */
void fx_drain_play(World *w, const Sight *s);

#endif
