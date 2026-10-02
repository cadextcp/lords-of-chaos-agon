#include "render.h"

#include <agon/mos.h>
#include <agon/vdp.h>
#include <stdio.h>
#include <string.h>

#include "../core/colors.h"
#include "../core/names.h"
#include "../core/view.h"

#define SCREEN_MODE 8
#define FORMAT_RGBA2222 1
#define TILE_BUFFER_BASE 0x2000
#define MAP_PX (VIEW_W * TILE_PX)   /* 216 */
#define PANEL_X MAP_PX
#define TEXT_COL_PANEL 27           /* 216 / 8 */
#define TEXT_ROW_MSG 27             /* 216 / 8 */
#define TEXT_COLS 40

#define CURSOR_SPRITE 0

static uint8_t pixels[TILE_PX * TILE_PX];

static bool load_tiles(void)
{
    uint8_t fh, head[7], sizes[2 * TILE_COUNT];
    uint16_t count, i;
    uint24_t len;

    fh = mos_fopen("tiles.bin", FA_READ);
    if (!fh) {
        printf("tiles.bin not found\r\n");
        return false;
    }
    if (mos_fread(fh, (char *)head, 7) != 7 || memcmp(head, "LOCT", 4) != 0 || head[4] != 1) {
        printf("tiles.bin: bad header\r\n");
        mos_fclose(fh);
        return false;
    }
    count = (uint16_t)(head[5] | (head[6] << 8));
    if (count != TILE_COUNT) {
        printf("tiles.bin: %u tiles, expected %u\r\n", count, TILE_COUNT);
        mos_fclose(fh);
        return false;
    }
    mos_fread(fh, (char *)sizes, 2u * count);
    for (i = 0; i < count; i++) {
        uint8_t w = sizes[2 * i], h = sizes[2 * i + 1];
        len = (uint24_t)w * h;
        if (len > sizeof pixels || mos_fread(fh, (char *)pixels, len) != len) {
            printf("tiles.bin: entry %u broken\r\n", i);
            mos_fclose(fh);
            return false;
        }
        vdp_adv_clear_buffer(TILE_BUFFER_BASE + i);
        vdp_adv_write_block_data(TILE_BUFFER_BASE + i, (int)len, (char *)pixels);
        vdp_adv_select_bitmap(TILE_BUFFER_BASE + i);
        vdp_adv_bitmap_from_buffer(w, h, FORMAT_RGBA2222);
    }
    mos_fclose(fh);
    return true;
}

static void draw_tile(uint8_t id, int x, int y)
{
    vdp_adv_select_bitmap(TILE_BUFFER_BASE + id);
    vdp_draw_bitmap(x, y);
}

bool render_init(void)
{
    vdp_mode(SCREEN_MODE);
    vdp_cursor_enable(false);
    vdp_clear_screen();
    vdp_set_pixel_coordinates();
    printf("Loading tiles...");
    if (!load_tiles())
        return false;
    vdp_clear_screen();
    view_invalidate();

    /* Cursor sprite: one frame per colour, in CursorColour order. */
    vdp_select_sprite(CURSOR_SPRITE);
    vdp_clear_sprite();
    vdp_adv_add_sprite_bitmap(TILE_BUFFER_BASE + T_CURSOR_GREEN);
    vdp_adv_add_sprite_bitmap(TILE_BUFFER_BASE + T_CURSOR_WHITE);
    vdp_adv_add_sprite_bitmap(TILE_BUFFER_BASE + T_CURSOR_YELLOW);
    vdp_adv_add_sprite_bitmap(TILE_BUFFER_BASE + T_CURSOR_RED);
    vdp_activate_sprites(1);
    vdp_hide_sprite();
    vdp_refresh_sprites();
    return true;
}

void render_cursor(int16_t vx, int16_t vy, uint8_t colour, bool visible)
{
    vdp_select_sprite(CURSOR_SPRITE);
    if (visible && vx >= 0 && vy >= 0 && vx < VIEW_W && vy < VIEW_H) {
        vdp_nth_sprite_frame(colour);
        vdp_move_sprite_to(vx * TILE_PX, vy * TILE_PX);
        vdp_show_sprite();
    } else {
        vdp_hide_sprite();
    }
    vdp_refresh_sprites();
}

uint8_t render_fields(void)
{
    uint8_t vx, vy, i, n = 0;
    for (vy = 0; vy < VIEW_H; vy++) {
        for (vx = 0; vx < VIEW_W; vx++) {
            const FieldLayers *f;
            if (!view_dirty(vx, vy))
                continue;
            f = view_field(vx, vy);
            for (i = 0; i < f->n; i++)
                draw_tile(f->id[i], vx * TILE_PX, vy * TILE_PX);
            n++;
        }
    }
    view_clean();
    if (n)
        vdp_refresh_sprites();   /* keep the cursor on top of new tiles */
    return n;
}

static void text_at(uint8_t col, uint8_t row, uint8_t colour, const char *s)
{
    vdp_cursor_tab(col, row);
    vdp_set_text_colour(colour);
    vdp_set_text_bg_colour(C_BLACK);
    printf("%s", s);
}

/* Fill colour, bar outline colour and icon per bar (Amiga order, B2.4). */
static const uint8_t BAR_FILL[6] = {C_BRIGHT_GREEN, C_BRIGHT_YELLOW, C_BRIGHT_RED,
                                    C_WHITE, C_BRIGHT_BLUE, C_BRIGHT_MAGENTA};
static const uint8_t BAR_EDGE[6] = {C_GREEN, C_YELLOW, C_RED, C_GREY, C_BLUE, C_MAGENTA};
static const uint8_t BAR_ICON[6] = {T_ICON_BOOT, T_ICON_BOLT, T_ICON_HEART,
                                    T_ICON_SWORD, T_ICON_SHIELD, T_ICON_STAR};
/* Status icons (PM 11) in UF_* bit order. */
static const uint8_t STATUS_ICON[5] = {T_ICON_ST_UNDEAD, T_ICON_ST_FLY, T_ICON_ST_MOUNT,
                                       T_ICON_ST_WOUND, T_ICON_ST_INVISIBLE};
#define COMBAT_SCALE 50   /* combat/defence bar full at 50 (creature table max) */
#define BAR_TOP 58
#define BAR_BOTTOM 168

static void black(int x0, int y0, int x1, int y1)
{
    vdp_gcol(0, C_BLACK);
    vdp_filled_rectangle(x0, y0, x1, y1);
}

static void bar(uint8_t i, uint8_t value, uint8_t max)
{
    int x = PANEL_X + 8 + i * 16;
    int h;
    if (max == 0) {                 /* e.g. mana of a non-wizard: no bar */
        black(x, BAR_TOP, x + 7, BAR_BOTTOM + 12);
        return;
    }
    if (value > max)
        value = max;
    h = (BAR_BOTTOM - BAR_TOP - 2) * value / max;
    vdp_gcol(0, BAR_EDGE[i]);
    vdp_rectangle(x, BAR_TOP, x + 7, BAR_BOTTOM);
    black(x + 1, BAR_TOP + 1, x + 6, BAR_BOTTOM - 1);
    if (h > 0) {
        vdp_gcol(0, BAR_FILL[i]);
        vdp_filled_rectangle(x + 1, BAR_BOTTOM - 1 - h, x + 6, BAR_BOTTOM - 1);
    }
    draw_tile(BAR_ICON[i], x, BAR_BOTTOM + 4);
}

/* Panel (GDD 11.1), 104 px = text columns 27..39:
 *   portrait 24x24 in a frame, right of it level + status icons,
 *   name, AP/mana figures, 6 bars with icons, "Am Boden" list. */
void render_panel(const World *w, uint8_t unit)
{
    char buf[16];
    const Unit *u = &w->units[unit];
    const char *ground[GROUND_MAX];
    uint8_t i, n;

    vdp_gcol(0, C_BRIGHT_BLUE);
    vdp_rectangle(PANEL_X + 4, 4, PANEL_X + 31, 31);
    black(PANEL_X + 5, 5, PANEL_X + 30, 30);
    draw_tile(u->kind == CR_WIZARD ? (uint8_t)(T_WIZARD_P1 + u->owner) : T_GOBLIN,
              PANEL_X + 6, 6);
    text_at(32, 1, C_GREY, u->kind == CR_WIZARD ? "Stufe 1" : "       ");
    for (i = 0; i < 5; i++) {
        int x = 256 + i * 9;
        black(x, 16, x + 7, 23);
        if (u->flags & (1u << i))
            draw_tile(STATUS_ICON[i], x, 16);
    }

    snprintf(buf, sizeof buf, "%-13.13s", name_unit(u));
    text_at(TEXT_COL_PANEL, 5, C_BRIGHT_WHITE, buf);
    snprintf(buf, sizeof buf, "AP %2u  ", u->ap);
    text_at(TEXT_COL_PANEL, 6, C_BRIGHT_GREEN, buf);
    if (u->mana_max)
        snprintf(buf, sizeof buf, "Ma%3u", u->mana);
    else
        snprintf(buf, sizeof buf, "     ");
    text_at(34, 6, C_BRIGHT_MAGENTA, buf);

    bar(0, u->ap, u->ap_max);
    bar(1, u->sta, u->sta_max);
    bar(2, u->con, u->con_max);
    bar(3, u->com, COMBAT_SCALE);
    bar(4, u->def, COMBAT_SCALE);
    bar(5, u->mana, u->mana_max);

    text_at(TEXT_COL_PANEL, 23, C_GREY, "Am Boden:");
    n = ground_names(w, u->x, u->y, ground);
    for (i = 0; i < GROUND_MAX; i++) {
        snprintf(buf, sizeof buf, "%-13.13s", i < n ? ground[i] : "");
        text_at(TEXT_COL_PANEL, (uint8_t)(24 + i), C_BRIGHT_WHITE, buf);
    }
}

void render_message(uint8_t line, uint8_t colour, const char *text)
{
    char buf[TEXT_COLS];
    /* Pad to 39 columns: writing column 39 of the last row would scroll. */
    snprintf(buf, sizeof buf, "%-39.39s", text);
    text_at(0, (uint8_t)(TEXT_ROW_MSG + line), colour, buf);
}

void render_shutdown(void)
{
    vdp_select_sprite(CURSOR_SPRITE);
    vdp_hide_sprite();
    vdp_activate_sprites(0);
    vdp_mode(0);
    vdp_cursor_enable(true);
}
