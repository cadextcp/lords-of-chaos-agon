#include "screen.h"

#include <string.h>

#include "glyphs.h"

#define CELLS (SCREEN_W * SCREEN_H)

static Cell cells[CELLS];
static uint8_t dirty[(CELLS + 7) / 8];

static uint16_t idx(uint8_t x, uint8_t y)
{
    return (uint16_t)((uint16_t)y * SCREEN_W + x);
}

static void mark(uint16_t i)
{
    dirty[i >> 3] |= (uint8_t)(1u << (i & 7));
}

void screen_clear(uint8_t bg)
{
    uint16_t i;
    for (i = 0; i < CELLS; i++) {
        cells[i].glyph = ' ';
        cells[i].fg = bg;
        cells[i].bg = bg;
    }
    memset(dirty, 0xFF, sizeof dirty);
}

void screen_put(uint8_t x, uint8_t y, uint8_t glyph, uint8_t fg, uint8_t bg)
{
    uint16_t i;
    Cell *c;
    if (x >= SCREEN_W || y >= SCREEN_H)
        return;
    i = idx(x, y);
    c = &cells[i];
    if (c->glyph == glyph && c->fg == fg && c->bg == bg)
        return;
    c->glyph = glyph;
    c->fg = fg;
    c->bg = bg;
    mark(i);
}

void screen_text(uint8_t x, uint8_t y, const char *s, uint8_t fg, uint8_t bg)
{
    while (*s && x < SCREEN_W)
        screen_put(x++, y, (uint8_t)*s++, fg, bg);
}

const Cell *screen_cell(uint8_t x, uint8_t y)
{
    return &cells[idx(x, y)];
}

bool screen_is_dirty(uint8_t x, uint8_t y)
{
    uint16_t i = idx(x, y);
    return (dirty[i >> 3] >> (i & 7)) & 1u;
}

uint16_t screen_dirty_count(void)
{
    uint16_t i, n = 0;
    for (i = 0; i < CELLS; i++)
        if ((dirty[i >> 3] >> (i & 7)) & 1u)
            n++;
    return n;
}

void screen_clean_all(void)
{
    memset(dirty, 0, sizeof dirty);
}

uint32_t screen_hash(void)
{
    uint32_t h = 2166136261UL;
    uint16_t i;
    for (i = 0; i < CELLS; i++) {
        h = (h ^ cells[i].glyph) * 16777619UL;
        h = (h ^ cells[i].fg) * 16777619UL;
        h = (h ^ cells[i].bg) * 16777619UL;
    }
    return h;
}

void screen_dump(void (*emit)(const char *line))
{
    char line[SCREEN_W + 1];
    uint8_t x, y;
    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < SCREEN_W; x++)
            line[x] = glyph_ascii(cells[idx(x, y)].glyph);
        line[SCREEN_W] = '\0';
        emit(line);
    }
}
