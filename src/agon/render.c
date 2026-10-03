#include "render.h"

#include <agon/mos.h>
#include <agon/vdp.h>
#include <stdio.h>
#include <string.h>

#include "../core/colors.h"
#include "../core/gen/data.h"
#include "../core/names.h"
#include "../core/ride.h"
#include "../core/sight.h"
#include "../core/spells.h"
#include "../core/view.h"

#define SCREEN_MODE 8
#define FORMAT_RGBA2222 1
#define TILE_BUFFER_BASE 0x2000
/* MAP_PX comes from render.h */
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

static void draw_tile(uint16_t id, int x, int y)
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
    vdp_adv_add_sprite_bitmap(TILE_BUFFER_BASE + T_CURSOR_BLUE);
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

/* Rider drawn behind its mount (M4k): lifted so that torso and head show
 * over the mount's back while the mount's body hides legs and feet. The
 * offset depends on the mount, read from the mount tile that follows. */
#define RIDE_LIFT 7
static void ride_offset(uint16_t mount_tile, int *dx, int *dy)
{
    static const struct { uint8_t kind; int8_t dx, dy; } MOUNTS[] = {
        { CR_UNICORN, -2, RIDE_LIFT },
        { CR_PEGASUS, -2, RIDE_LIFT },
        { CR_GRYPHON, -1, RIDE_LIFT },
        { CR_ELEPHANT, -1, RIDE_LIFT },
    };
    uint8_t i;
    *dx = 0;
    *dy = RIDE_LIFT;
    for (i = 0; i < sizeof MOUNTS / sizeof MOUNTS[0]; i++) {
        uint16_t base = CREATURE_TILE[MOUNTS[i].kind];
        if (mount_tile >= base && mount_tile - base <= OWN_NEUTRAL) {
            *dx = MOUNTS[i].dx;
            *dy = MOUNTS[i].dy;
            return;
        }
    }
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
            for (i = 0; i < f->n; i++) {
                int x = vx * TILE_PX;
                int y = vy * TILE_PX;
                if (f->air & (1u << i))         /* flyer, slightly higher */
                    y -= 3;
                if ((f->ride & (1u << i)) && i + 1 < f->n) {
                    int dx, dy;                 /* rider behind the mount */
                    ride_offset(f->id[i + 1], &dx, &dy);
                    x += dx;
                    y -= dy;
                }
                if (y < 0)
                    y = 0;
                if (x < 0)
                    x = 0;
                draw_tile(f->id[i], x, y);
            }
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
static const uint16_t BAR_ICON[6] = {T_ICON_BOOT, T_ICON_BOLT, T_ICON_HEART,
                                    T_ICON_SWORD, T_ICON_SHIELD, T_ICON_STAR};
/* Status icons (PM 11) in UF_* bit order. */
static const uint16_t STATUS_ICON[5] = {T_ICON_ST_UNDEAD, T_ICON_ST_FLY, T_ICON_ST_MOUNT,
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
    {   /* a rider shows behind his mount, lifted a little (M4k) */
        uint8_t rk = ride_rider_kind(u);
        int dx = 0, dy = 0;
        uint16_t mount_tile = (uint16_t)(CREATURE_TILE[u->kind] + u->owner);
        if (rk < CR_COUNT) {
            ride_offset(mount_tile, &dx, &dy);
            draw_tile((uint16_t)(CREATURE_TILE[rk] + u->owner), PANEL_X + 6 + dx,
                      6 - (dy > 2 ? 2 : dy));
        }
        draw_tile(mount_tile, PANEL_X + 6, 6);
    }
    text_at(32, 1, C_GREY, u->kind == CR_WIZARD ? "Stufe 1" : "       ");
    for (i = 0; i < 5; i++) {
        int x = 256 + i * 9;
        black(x, 16, x + 7, 23);
        if (u->flags & (1u << i))
            draw_tile(STATUS_ICON[i], x, 16);
    }
    {   /* timed effects (M4b): shield, strength, speed */
        uint8_t k, slot = 5;
        for (k = 0; k < UNIT_EFFECTS; k++) {
            int x;
            if (u->effects[k].rounds == 0)
                continue;
            x = 256 + slot * 9;
            if (slot >= 8)
                break;
            if (u->effects[k].kind == EFF_SHIELD || u->effects[k].kind == EFF_PROTECT)
                draw_tile(T_ICON_SHIELD, x, 16);
            else if (u->effects[k].kind == EFF_STRENGTH)
                draw_tile(T_ICON_SWORD, x, 16);
            else if (u->effects[k].kind == EFF_SPEED)
                draw_tile(T_ICON_STAR, x, 16);
            else if (u->effects[k].kind == EFF_MAGIC_WEAPON)
                draw_tile(T_ICON_BOLT, x, 16);
            else
                draw_tile(T_ICON_ST_INVISIBLE, x, 16);
            slot++;
        }
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

    bar(0, u->ap, (u->flags & UF_FLYING) ? u->ap_fly : u->ap_max);
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

/* Look mode (GDD 5.1): the examined field instead of a unit. With a
 * (visible) unit it degenerates to the normal unit panel. */
void render_panel_at(const World *w, const Sight *s, int16_t x, int16_t y)
{
    char buf[24], desc[24];
    const char *ground[GROUND_MAX];
    uint8_t i, n, u;
    int16_t wx = x, wy = y;

    if (!world_wrap(w, &wx, &wy))
        return;                          /* outside: keep the last panel */
    u = world_unit_at(w, wx, wy, UL_GROUND);
    if (u == NO_UNIT)
        u = world_unit_at(w, wx, wy, UL_AIR);
    if (u != NO_UNIT) {
        const Unit *un = &w->units[u];
        if (!s || un->owner == s->owner ||
            (sight_visible(s, w, wx, wy) && !(un->flags & UF_INVISIBLE))) {
            render_panel(w, u);
            return;
        }
    }

    vdp_gcol(0, C_BRIGHT_BLUE);
    vdp_rectangle(PANEL_X + 4, 4, PANEL_X + 31, 31);
    black(PANEL_X + 5, 5, PANEL_X + 30, 30);
    text_at(32, 1, C_GREY, "       ");
    for (i = 0; i < 5; i++) {
        int px = 256 + i * 9;
        black(px, 16, px + 7, 23);
    }
    describe_field(w, s, wx, wy, desc, sizeof desc);
    snprintf(buf, sizeof buf, "%-13.13s", desc);
    text_at(TEXT_COL_PANEL, 5, C_BRIGHT_WHITE, buf);
    text_at(TEXT_COL_PANEL, 6, C_GREY, "             ");
    for (i = 0; i < 6; i++)
        bar(i, 0, 1);
    text_at(TEXT_COL_PANEL, 23, C_GREY, "Am Boden:");
    n = ground_names(w, wx, wy, ground);
    for (i = 0; i < GROUND_MAX; i++) {
        snprintf(buf, sizeof buf, "%-13.13s", i < n ? ground[i] : "");
        text_at(TEXT_COL_PANEL, (uint8_t)(24 + i), C_BRIGHT_WHITE, buf);
    }
}

void render_spell_list(const Spellbook *book)
{
    uint8_t row = 0, letter = 'a';
    uint16_t i;
    black(0, 0, MAP_PX - 1, MAP_PX - 1);
    /* 27 columns fit left of the stat panel: letter, 15 name, level, mana */
    text_at(0, 0, C_BRIGHT_YELLOW, "  Zauber          St Mana");
    for (i = 0; i < SPELL_COUNT && letter <= 'z'; i++) {
        char line[28];
        if (book->level[i] == 0)
            continue;
        snprintf(line, sizeof line, "%c %-15.15s %2u %4u", letter,
                 SPELLS[i].name, book->level[i], spell_mana((uint8_t)i, book->level[i]));
        text_at(0, (uint8_t)(2 + row), C_BRIGHT_WHITE, line);
        row++;
        letter++;
    }
    text_at(0, 22, C_GREY, "Esc bricht ab.");
}

void render_menu_clear(void)
{
    black(0, 0, MAP_PX - 1, MAP_PX - 1);
}

void render_menu_text(uint8_t col, uint8_t row, uint8_t colour,
                      const char *text)
{
    text_at(col, row, colour, text);
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
