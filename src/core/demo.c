#include "demo.h"

#include "colors.h"
#include "glyphs.h"
#include "rng.h"
#include "screen.h"
#include "version.h"

#define MAP_TOP 2
#define MAP_BOTTOM (SCREEN_H - 2)

static uint8_t terrain[SCREEN_H][SCREEN_W];
static uint8_t wiz_x, wiz_y;

static void draw_terrain(uint8_t x, uint8_t y)
{
    uint8_t g = terrain[y][x];
    uint8_t fg = C_GREEN;
    switch (g) {
    case G_TREE:  fg = C_BRIGHT_GREEN; break;
    case G_WALL:  fg = C_GREY; break;
    case G_WATER: fg = C_BRIGHT_BLUE; break;
    case G_FIRE:  fg = C_BRIGHT_RED; break;
    default:      break;
    }
    screen_put(x, y, g, fg, C_BLACK);
}

static uint8_t walkable(uint8_t x, uint8_t y)
{
    uint8_t g;
    if (x >= SCREEN_W || y < MAP_TOP || y >= MAP_BOTTOM)
        return 0;
    g = terrain[y][x];
    return g == G_GRASS || g == ' ';
}

void demo_init(uint32_t seed)
{
    Rng rng;
    uint8_t x, y;
    uint16_t r;

    rng_seed(&rng, seed);
    screen_clear(C_BLACK);
    screen_text(1, 0, LOC_TITLE, C_BRIGHT_YELLOW, C_BLACK);

    for (y = MAP_TOP; y < MAP_BOTTOM; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            r = rng_range(&rng, 100);
            if (x == 0 || x == SCREEN_W - 1 || y == MAP_TOP || y == MAP_BOTTOM - 1)
                terrain[y][x] = G_WALL;
            else if (r < 10)
                terrain[y][x] = G_TREE;
            else if (r < 13)
                terrain[y][x] = G_WATER;
            else if (r < 14)
                terrain[y][x] = G_FIRE;
            else if (r < 50)
                terrain[y][x] = G_GRASS;
            else
                terrain[y][x] = ' ';
            draw_terrain(x, y);
        }
    }

    wiz_x = SCREEN_W / 2;
    wiz_y = (MAP_TOP + MAP_BOTTOM) / 2;
    terrain[wiz_y][wiz_x] = ' ';
    screen_put(wiz_x, wiz_y, G_WIZARD, C_BRIGHT_MAGENTA, C_BLACK);
    screen_text(1, SCREEN_H - 1, "Arrows/WASD: move  ESC: quit", C_GREY, C_BLACK);
}

uint8_t demo_move(Dir d)
{
    uint8_t nx = wiz_x, ny = wiz_y;
    switch (d) {
    case DIR_N: ny--; break;
    case DIR_S: ny++; break;
    case DIR_W: nx--; break;
    case DIR_E: nx++; break;
    default: return 0;
    }
    if (!walkable(nx, ny))
        return 0;
    draw_terrain(wiz_x, wiz_y);
    wiz_x = nx;
    wiz_y = ny;
    screen_put(wiz_x, wiz_y, G_WIZARD, C_BRIGHT_MAGENTA, C_BLACK);
    return 1;
}

uint8_t demo_wizard_x(void) { return wiz_x; }
uint8_t demo_wizard_y(void) { return wiz_y; }
