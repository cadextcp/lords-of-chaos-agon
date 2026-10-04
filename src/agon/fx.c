#include "fx.h"

#include <agon/keyboard.h>
#include <agon/vdp.h>
#include <stdio.h>
#include <string.h>

#include "../core/colors.h"
#include "../core/events.h"
#include "../core/gen/tiles.h"
#include "../core/view.h"
#include "render.h"
#include "sound.h"

#define FX_FRAME_CS 8    /* one effect frame: 80 ms */
#define FX_TOUCHED 32

static bool fx_enabled = true;

void fx_set_enabled(bool on)
{
    fx_enabled = on;
}

/* Wait n effect frames; queued keys are read and dropped (K5: no stale
 * "held" state may leak into the chord logic afterwards). */
static void wait_frames(uint8_t n)
{
    struct keyboard_event_t e;
    uint32_t until = getsysvar_time() + (uint32_t)n * FX_FRAME_CS;
    while ((int32_t)(getsysvar_time() - until) < 0)
        while (kbuf_poll_event(&e))
            ;
}

/* World -> view field of an event; false when outside the window. */
static bool event_view(const World *w, int16_t wx, int16_t wy,
                       int16_t *vx, int16_t *vy)
{
    int16_t dx = (int16_t)(wx - view_origin_x());
    int16_t dy = (int16_t)(wy - view_origin_y());
    if (w->wrap) {                        /* shortest way around the torus */
        if (dx > w->w / 2)
            dx = (int16_t)(dx - w->w);
        if (dx < -w->w / 2)
            dx = (int16_t)(dx + w->w);
        if (dy > w->h / 2)
            dy = (int16_t)(dy - w->h);
        if (dy < -w->h / 2)
            dy = (int16_t)(dy + w->h);
    }
    *vx = dx;
    *vy = dy;
    return dx >= 0 && dy >= 0 && dx < VIEW_W && dy < VIEW_H;
}

static void draw_overlay(int16_t vx, int16_t vy, uint16_t tile)
{
    render_draw_tile(tile, vx * TILE_PX, vy * TILE_PX);
}

/* Fields painted over by the show; repainted at the end. */
static int16_t touched[FX_TOUCHED][2];
static uint8_t touched_n;

static void touch(int16_t vx, int16_t vy)
{
    if (touched_n < FX_TOUCHED) {
        touched[touched_n][0] = vx;
        touched[touched_n][1] = vy;
        touched_n++;
    }
}

/* Damage text on the field's top row. It may be wider than one field
 * (3 text columns): it is shifted left to stay inside the map window
 * (columns 0..26, never the panel) and every field under it is marked. */
static void float_text(int16_t vx, int16_t vy, const char *text)
{
    uint8_t len = (uint8_t)strlen(text);
    int16_t col = (int16_t)(vx * 3), c;
    if (col + len > VIEW_W * 3)
        col = (int16_t)(VIEW_W * 3 - len);
    if (col < 0)
        col = 0;
    render_menu_text((uint8_t)col, (uint8_t)(vy * 3), C_BRIGHT_RED, text);
    for (c = (int16_t)(col / 3); c <= (col + len - 1) / 3 && c < VIEW_W; c++)
        if (c != vx)
            touch(c, vy);
}

void fx_drain_play(World *w, const Sight *s)
{
    static GameEvent ev[EVENT_RING];
    uint8_t n, i;
    char buf[12];

    n = events_drain(ev, EVENT_RING);
    if (!n)
        return;
    if (!fx_enabled)
        return;                          /* scripted run: drop the show */
    touched_n = 0;
    for (i = 0; i < n; i++) {
        const GameEvent *e = &ev[i];
        int16_t vx, vy;
        if (!event_view(w, e->x, e->y, &vx, &vy))
            continue;                      /* outside the 9x9 window */
        if (s && !sight_visible(s, w, e->x, e->y))
            continue;                      /* happens in the fog of war */
        touch(vx, vy);
        switch (e->type) {
        case EV_SWING:
            sound_play(SND_SWING);
            draw_overlay(vx, vy, T_FX_SLASH);
            wait_frames(2);
            break;
        case EV_HIT:
            sound_play(SND_HIT);
            if (e->b) {                      /* critical (D30): KRIT! */
                sound_play(SND_SMASH);
                draw_overlay(vx, vy, T_FX_HIT);
                draw_overlay(vx, vy, T_FX_SLASH);
                snprintf(buf, sizeof buf, "KRIT -%u", e->a);
                float_text(vx, vy, buf);
                wait_frames(5);
            } else {
                draw_overlay(vx, vy, T_FX_HIT);
                snprintf(buf, sizeof buf, "-%u", e->a);
                float_text(vx, vy, buf);
                wait_frames(3);
            }
            break;
        case EV_MISS:
            sound_play(SND_MISS);
            draw_overlay(vx, vy, T_FX_MISS);
            wait_frames(2);
            break;
        case EV_DEATH:
            sound_play(SND_DEATH);
            draw_overlay(vx, vy, T_FX_DEATH_0);   /* white flash */
            wait_frames(1);
            draw_overlay(vx, vy, T_FX_DEATH_1);   /* the creature fades */
            wait_frames(2);
            draw_overlay(vx, vy, T_FX_DEATH_2);
            wait_frames(2);
            draw_overlay(vx, vy, T_FX_DEATH_3);   /* dust and a cross */
            wait_frames(2);
            break;
        case EV_SPELL:
            sound_play(SND_SPELL);
            draw_overlay(vx, vy, T_FX_DEATH_0);   /* flash of magic */
            wait_frames(2);
            break;
        case EV_SMASH:
            sound_play(SND_SMASH);
            draw_overlay(vx, vy, T_FX_DEATH_3);   /* debris cloud */
            wait_frames(2);
            break;
        default:
            break;                        /* EV_WOUND rides along with EV_HIT */
        }
    }
    /* repaint every field the show painted over */
    for (i = 0; i < touched_n; i++)
        view_mark_dirty((uint8_t)touched[i][0], (uint8_t)touched[i][1]);
    if (touched_n)
        render_fields();
}
