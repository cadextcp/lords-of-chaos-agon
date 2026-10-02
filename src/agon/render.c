#include "render.h"

#include <agon/mos.h>
#include <agon/vdp.h>
#include <stdbool.h>
#include <stdint.h>

#include "../core/glyphs.h"
#include "../core/screen.h"

#define SCREEN_MODE 8

/* Per dirty cell: VDU 31,x,y  17,fg  17,128+bg  ch  = 8 bytes. */
static char buf[256];
static uint24_t len;

static void flush_buf(void)
{
    if (len) {
        mos_puts(buf, len, 0);
        len = 0;
    }
}

static void emit(uint8_t b)
{
    buf[len++] = (char)b;
    if (len == sizeof buf)
        flush_buf();
}

void render_init(void)
{
    uint8_t i;
    vdp_mode(SCREEN_MODE);
    vdp_cursor_enable(false);
    vdp_clear_screen();
    for (i = 0; i < glyph_count; i++) {
        const uint8_t *r = glyph_defs[i].rows;
        vdp_redefine_character(glyph_defs[i].code, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
    }
}

void render_flush(void)
{
    uint8_t x, y;
    int16_t fg = -1, bg = -1;
    const Cell *c;

    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            if (!screen_is_dirty(x, y))
                continue;
            /* The bottom-right cell would scroll the screen in text mode. */
            if (x == SCREEN_W - 1 && y == SCREEN_H - 1)
                continue;
            c = screen_cell(x, y);
            emit(31);
            emit(x);
            emit(y);
            if (c->fg != fg) {
                emit(17);
                emit(c->fg);
                fg = c->fg;
            }
            if (c->bg != bg) {
                emit(17);
                emit((uint8_t)(128 + c->bg));
                bg = c->bg;
            }
            emit(c->glyph);
        }
    }
    flush_buf();
    screen_clean_all();
}

void render_shutdown(void)
{
    vdp_mode(0);
    vdp_cursor_enable(true);
}
