/*
 * VDP feature spike (polish round, ADR 0012) - a separate program so that
 * it costs the game no RAM: vdptest [n] runs all tests or test n (1 audio,
 * 2 font, 3 palette, 4 sprites, 5 double buffer). Every test prints and
 * logs (vdptest.log) what the VDP answered, so the verdict needs neither
 * eyes nor ears. Built by tools/build.py, staged to /loc/vdptest.bin.
 */
#include <agon/keyboard.h>
#include <agon/mos.h>
#include <agon/vdp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../src/core/colors.h"

#define TILE_BUFFER_BASE 0x2000
#define TILE_PX 24
#define TILES 9                      /* 0 = floor, 1..8 = sprite figures */
#define FONT_BUFFER 0x5000
#define SYSFONT_BUFFER 0x5100
#define SAMPLE_BUFFER 0x6000
#define SAMPLE_LEN 8000              /* 0.5 s at 16 kHz */
#define SPRITES 8

static uint8_t logfh;

static void log_line(const char *l)
{
    if (!logfh)
        return;
    mos_fwrite(logfh, (char *)l, strlen(l));
    mos_fputc(logfh, '\n');
}

/* Test bitmaps instead of the game's tiles: a checkered floor and eight
 * coloured figures with a transparent border. */
static void make_tiles(void)
{
    static uint8_t px[TILE_PX * TILE_PX];
    uint8_t t, x, y;
    for (t = 0; t < TILES; t++) {
        for (y = 0; y < TILE_PX; y++)
            for (x = 0; x < TILE_PX; x++) {
                uint8_t v;
                if (t == 0)
                    v = (uint8_t)(0xC0 | (((x / 4 + y / 4) & 1) ? 0x08 : 0x04));
                else if (x < 4 || x > 19 || y < 2 || y > 21)
                    v = 0;                               /* transparent */
                else
                    v = (uint8_t)(0xC0 | ((t * 9) & 0x3F) | ((x == 4 || y == 2) ? 0x3F : 0));
                px[y * TILE_PX + x] = v;
            }
        vdp_adv_clear_buffer(TILE_BUFFER_BASE + t);
        vdp_adv_write_block_data(TILE_BUFFER_BASE + t, sizeof px, (char *)px);
        vdp_adv_select_bitmap(TILE_BUFFER_BASE + t);
        vdp_adv_bitmap_from_buffer(TILE_PX, TILE_PX, 1);
    }
}

static void draw_tile(uint8_t t, int x, int y)
{
    vdp_adv_select_bitmap(TILE_BUFFER_BASE + t);
    vdp_draw_bitmap(x, y);
}

static void screen_clear(void)
{
    vdp_gcol(0, C_BLACK);
    vdp_filled_rectangle(0, 0, 319, 239);
}

static char line[64];
static uint8_t row;
static bool single;                  /* one test: hold its screen longer */

static void out(uint8_t colour, const char *s)
{
    log_line(s);
    if (row < 28) {
        vdp_cursor_tab(0, row++);
        vdp_set_text_colour(colour);
        printf("%-38.38s", s);
    }
}

static void heading(const char *s)
{
    screen_clear();
    row = 0;
    out(C_BRIGHT_YELLOW, s);
    row++;
}

/* Wait for a key or until the time is up (scripted runs advance alone). */
static void pause_cs(uint16_t cs)
{
    struct keyboard_event_t e;
    uint32_t until;
    if (single && cs >= 150)
        cs = 1500;                       /* the end of a test: screenshot */
    until = getsysvar_time() + cs;
    while ((int32_t)(getsysvar_time() - until) < 0)
        while (kbuf_poll_event(&e))
            if (e.isdown)
                return;
}

static void flag_clear(uint8_t flag)
{
    mos_sysvars()[sysvar_vdp_pflags] &= (uint8_t)~flag;
}

/* Wait (max 0.5 s) for the VDP to answer; false on timeout. */
static bool flag_wait(uint8_t flag)
{
    uint32_t until = getsysvar_time() + 50;
    while (!(mos_sysvars()[sysvar_vdp_pflags] & flag))
        if ((int32_t)(getsysvar_time() - until) >= 0)
            return false;
    return true;
}

/* Play a note and return the VDP verdict: 1 queued, 0 rejected, 9 no
 * answer at all. */
static uint8_t note(uint8_t ch, uint8_t vol, uint16_t hz, uint16_t ms)
{
    flag_clear(vdp_pflag_audio);
    vdp_audio_play_note(ch, vol, hz, ms);
    if (!flag_wait(vdp_pflag_audio))
        return 9;
    return getsysvar_audioSuccess();
}

static uint24_t pixel_at(int x, int y)
{
    flag_clear(vdp_pflag_point);
    vdp_request_pixel_colour(x, y, false);
    if (!flag_wait(vdp_pflag_point))
        return 0xFFFFFF;
    return getsysvar_scrpixel();
}

/* ---------- 1: audio ---------- */

static int8_t chunk[500];             /* generated piece by piece */

static void test_audio(void)
{
    uint8_t a, b, c, i;
    uint32_t t0;
    heading("1 AUDIO");

    a = note(0, 60, 440, 400);
    b = note(0, 60, 554, 400);
    c = note(0, 60, 659, 400);
    snprintf(line, sizeof line, "A1 3 notes back to back: %u %u %u", a, b, c);
    out(C_BRIGHT_WHITE, line);
    pause_cs(50);
    a = note(0, 60, 440, 200);
    snprintf(line, sizeof line, "A2 after the first ended: %u", a);
    out(C_BRIGHT_WHITE, line);
    pause_cs(30);

    /* a sample: decaying noise burst over a 200 Hz tone, own synthesis,
     * generated and sent in 500-byte pieces (RAM is tight) */
    t0 = getsysvar_time();
    vdp_adv_clear_buffer(SAMPLE_BUFFER);
    {
        uint16_t k = 0;
        uint16_t lfsr = 0xACE1;
        for (i = 0; i < SAMPLE_LEN / 500; i++) {
            uint16_t j;
            for (j = 0; j < 500; j++, k++) {
                int16_t env = (int16_t)(127 - (k * 127L) / SAMPLE_LEN);
                int16_t tone = (int16_t)(((k / 40) & 1) ? 40 : -40);
                int16_t noise;
                lfsr = (uint16_t)((lfsr >> 1) ^ (-(int16_t)(lfsr & 1) & 0xB400));
                noise = (int16_t)((int8_t)(lfsr & 0xFF) / 2);
                chunk[j] = (int8_t)(((tone + noise) * env) / 127);
            }
            vdp_adv_write_block_data(SAMPLE_BUFFER, 500, (char *)chunk);
        }
    }
    vdp_adv_consolidate(SAMPLE_BUFFER);
    vdp_audio_create_sample_from_buffer(1, SAMPLE_BUFFER,
                                        VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED);
    vdp_audio_set_sample(1, SAMPLE_BUFFER);
    snprintf(line, sizeof line, "A3 sample make+upload 8000 B: %lu cs",
             (unsigned long)(getsysvar_time() - t0));
    out(C_BRIGHT_WHITE, line);
    t0 = getsysvar_time();               /* pure transfer, no synthesis */
    vdp_adv_clear_buffer(SAMPLE_BUFFER + 1);
    for (i = 0; i < SAMPLE_LEN / 500; i++)
        vdp_adv_write_block_data(SAMPLE_BUFFER + 1, 500, (char *)chunk);
    vdp_adv_consolidate(SAMPLE_BUFFER + 1);
    snprintf(line, sizeof line, "A3b upload only 8000 B: %lu cs",
             (unsigned long)(getsysvar_time() - t0));
    out(C_BRIGHT_WHITE, line);
    vdp_adv_clear_buffer(SAMPLE_BUFFER + 1);
    a = note(1, 100, 0, 500);
    snprintf(line, sizeof line, "A4 sample play ch1: %u", a);
    out(C_BRIGHT_WHITE, line);
    pause_cs(60);

    vdp_audio_enable_channel(4);
    vdp_audio_enable_channel(5);
    a = note(4, 50, 330, 200);
    b = note(5, 50, 392, 200);
    snprintf(line, sizeof line, "A5 extra channels 4/5: %u %u", a, b);
    out(C_BRIGHT_WHITE, line);
    pause_cs(30);

    /* interrupting: a long note, then reset the channel and play again */
    a = note(0, 50, 300, 2000);
    vdp_audio_reset_channel(0);
    b = note(0, 50, 400, 200);
    snprintf(line, sizeof line, "A6 long note, reset, new: %u %u", a, b);
    out(C_BRIGHT_WHITE, line);
    pause_cs(30);

    /* tunable sample: the same buffer at three pitches (instrument) */
    vdp_audio_create_sample_from_buffer(2, SAMPLE_BUFFER,
        VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED | VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE);
    vdp_audio_set_buffer_frequency(2, SAMPLE_BUFFER, 262);
    vdp_audio_set_sample(2, SAMPLE_BUFFER);
    a = note(2, 80, 262, 150);
    pause_cs(20);
    b = note(2, 80, 392, 150);
    pause_cs(20);
    c = note(2, 80, 523, 150);
    snprintf(line, sizeof line, "A7 tunable sample C/G/C': %u %u %u", a, b, c);
    out(C_BRIGHT_WHITE, line);
    pause_cs(30);

    /* do enabled extra channels survive a reset? */
    vdp_audio_enable_channel(9);
    a = note(9, 50, 500, 100);
    pause_cs(15);
    vdp_audio_reset_channel(9);
    b = note(9, 50, 500, 100);
    pause_cs(15);
    vdp_audio_reset_channel(4);
    c = note(4, 50, 500, 100);
    snprintf(line, sizeof line, "A8 ch9, reset ch9, reset ch4: %u %u %u", a, b, c);
    out(C_BRIGHT_WHITE, line);
    pause_cs(30);

    /* samples: is the note length honoured, and does a reset free a
     * channel that plays a sample? (the sample is 500 ms long) */
    vdp_audio_set_sample(1, SAMPLE_BUFFER);
    a = note(1, 80, 0, 100);              /* 100 ms of a 500 ms sample */
    {
        uint32_t t = getsysvar_time() + 20;   /* 200 ms later */
        while ((int32_t)(getsysvar_time() - t) < 0)
            ;
    }
    b = note(1, 80, 0, 500);              /* 1 = length honoured */
    {
        uint32_t t = getsysvar_time() + 5;
        while ((int32_t)(getsysvar_time() - t) < 0)
            ;
    }
    vdp_audio_reset_channel(1);           /* mid-sample */
    vdp_audio_set_sample(1, SAMPLE_BUFFER);
    c = note(1, 80, 0, 500);              /* right after the reset */
    snprintf(line, sizeof line, "A9 smp len kept/reset now: %u %u %u", a, b, c);
    out(C_BRIGHT_WHITE, line);
    {
        uint32_t t = getsysvar_time() + 5;
        while ((int32_t)(getsysvar_time() - t) < 0)
            ;
    }
    vdp_audio_reset_channel(1);
    {
        uint32_t t = getsysvar_time() + 2;    /* one clock step */
        while ((int32_t)(getsysvar_time() - t) < 0)
            ;
    }
    vdp_audio_set_sample(1, SAMPLE_BUFFER);
    a = note(1, 80, 0, 500);
    snprintf(line, sizeof line, "A10 reset, 20 ms, play: %u", a);
    out(C_BRIGHT_WHITE, line);
    pause_cs(60);

    /* how far can a tunable sample (base 262 Hz) be pitched up? */
    {
        static const uint16_t HZ[6] = {500, 520, 524, 600, 786, 1048};
        char *p = line;
        p += snprintf(p, 20, "A11 x");
        for (i = 0; i < 6; i++) {
            vdp_audio_reset_channel(2);
            vdp_audio_set_sample(2, SAMPLE_BUFFER);
            a = note(2, 60, HZ[i], 60);
            p += snprintf(p, 8, " %u:%u", HZ[i] / 10, a);
            pause_cs(10);
        }
        out(C_BRIGHT_WHITE, line);
    }
    pause_cs(60);
    out(C_GREY, "(1 = queued, 0 = rejected, 9 = silent)");
    pause_cs(150);
}

/* ---------- 2: fonts ---------- */


static void test_font(void)
{
    uint16_t c;
    uint8_t r, rows8, rows16;
    uint32_t t0, t_sys, t_own;
    heading("2 FONT");

    /* system font copied into a buffer and used as a font again */
    vdp_font_copy(SYSFONT_BUFFER);
    vdp_font_create(SYSFONT_BUFFER, 8, 8, 8, 0);
    vdp_font_select(SYSFONT_BUFFER, 0);
    out(C_BRIGHT_WHITE, "F1 copied system font: Hallo Welt");

    /* own 8x16 font: a frame with the code's bits inside, sent glyph by
     * glyph (the appended blocks are consolidated afterwards) */
    vdp_adv_clear_buffer(FONT_BUFFER);
    for (c = 0; c < 256; c++) {
        uint8_t g[16];
        for (r = 0; r < 16; r++) {
            uint8_t v;
            if (r == 0 || r == 15)
                v = 0;
            else if (r == 1 || r == 14)
                v = 0x7E;
            else if (r >= 4 && r < 12)
                v = (uint8_t)(0x42 | (((c >> (r - 4)) & 1) ? 0x18 : 0));
            else
                v = 0x42;
            g[r] = v;
        }
        vdp_adv_write_block_data(FONT_BUFFER, 16, (char *)g);
    }
    vdp_adv_consolidate(FONT_BUFFER);
    vdp_font_create(FONT_BUFFER, 8, 16, 14, 0);

    rows8 = getsysvar_scrRows();
    vdp_font_select(FONT_BUFFER, 0);
    flag_clear(vdp_pflag_mode);
    rows16 = getsysvar_scrRows();
    vdp_cursor_tab(0, 4);
    vdp_set_text_colour(C_BRIGHT_CYAN);
    t0 = getsysvar_time();
    for (c = 0; c < 10; c++)
        printf("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\x84\x94\x81\xE1");
    t_own = getsysvar_time() - t0;

    vdp_font_select(SYSFONT_BUFFER, 0);
    vdp_cursor_tab(0, 20);
    vdp_set_text_colour(C_GREY);
    t0 = getsysvar_time();
    for (c = 0; c < 10; c++)
        printf("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\x84\x94\x81\xE1");
    t_sys = getsysvar_time() - t0;
    vdp_font_select(0xFFFF, 0);                  /* back to the system font */

    row = 25;
    snprintf(line, sizeof line, "F2 rows sys %u own %u (sysvar)", rows8, rows16);
    out(C_BRIGHT_WHITE, line);
    snprintf(line, sizeof line, "F3 400 chars: own %lu cs, sys %lu cs",
             (unsigned long)t_own, (unsigned long)t_sys);
    out(C_BRIGHT_WHITE, line);
    pause_cs(250);
}

/* ---------- 3: palette in MODE 8 ---------- */

static void test_palette(void)
{
    uint24_t before, after;
    uint32_t t0;
    uint8_t i;
    heading("3 PALETTE");
    vdp_gcol(0, C_BRIGHT_RED);
    vdp_filled_rectangle(40, 60, 119, 139);
    vdp_gcol(0, C_BRIGHT_BLUE);
    vdp_filled_rectangle(160, 60, 239, 139);
    before = pixel_at(80, 100);
    vdp_define_colour(C_BRIGHT_RED, 255, 0, 255, 0);   /* red -> green? */
    pause_cs(10);
    after = pixel_at(80, 100);
    snprintf(line, sizeof line, "P1 VDU19 red->green: %06lX -> %06lX",
             (unsigned long)before, (unsigned long)after);
    row = 19;
    out(C_BRIGHT_WHITE, line);
    out(C_BRIGHT_WHITE, after != before ? "   palette works in MODE 8"
                                         : "   no effect in MODE 8");
    t0 = getsysvar_time();
    for (i = 0; i < 100; i++)
        vdp_define_colour(C_BRIGHT_BLUE, 255, i, 0, (uint8_t)(255 - i));
    snprintf(line, sizeof line, "P2 100 x VDU19: %lu cs",
             (unsigned long)(getsysvar_time() - t0));
    out(C_BRIGHT_WHITE, line);
    pause_cs(200);
    vdp_reset_graphics();
}

/* ---------- 4: sprites ---------- */

static void test_sprites(void)
{
    uint8_t s, x, y;
    uint16_t frame;
    uint32_t t0, dt;
    heading("4 SPRITES");
    for (y = 0; y < 8; y++)
        for (x = 0; x < 13; x++)
            draw_tile(0, x * 24, 24 + y * 24);
    for (s = 1; s <= SPRITES; s++) {
        vdp_select_sprite(s);
        vdp_clear_sprite();
        vdp_adv_add_sprite_bitmap(TILE_BUFFER_BASE + s);
        vdp_adv_add_sprite_bitmap(TILE_BUFFER_BASE + 1 + s % 8);
        vdp_move_sprite_to(0, 24 + (s - 1) * 24);
        vdp_show_sprite();
    }
    vdp_activate_sprites(SPRITES + 1);
    vdp_refresh_sprites();
    t0 = getsysvar_time();
    for (frame = 0; frame < 150; frame++) {
        for (s = 1; s <= SPRITES; s++) {
            vdp_select_sprite(s);
            vdp_move_sprite_to((int)(frame * 2 + s * 3) % 300,
                               24 + (s - 1) * 24);
            if ((frame & 7) == 0)
                vdp_next_sprite_frame();
        }
        vdp_refresh_sprites();
    }
    dt = getsysvar_time() - t0;
    snprintf(line, sizeof line, "S1 8 sprites x 150 moves: %lu cs", (unsigned long)dt);
    row = 23;
    out(C_BRIGHT_WHITE, line);
    snprintf(line, sizeof line, "   = %lu ms per frame",
             (unsigned long)(dt * 10 / 150));
    out(C_BRIGHT_WHITE, line);
    pause_cs(200);
    for (s = 1; s <= SPRITES; s++) {
        vdp_select_sprite(s);
        vdp_hide_sprite();
    }
    vdp_activate_sprites(1);
    vdp_refresh_sprites();
}

/* ---------- 5: double buffering (last: it changes the mode) ---------- */

static void test_double_buffer(void)
{
    uint16_t w, h, i;
    uint8_t colours, ok;
    uint32_t t0, dt;
    flag_clear(vdp_pflag_mode);
    vdp_mode(136);                                /* MODE 8 + 128 */
    ok = flag_wait(vdp_pflag_mode);
    w = getsysvar_scrwidth();
    h = getsysvar_scrheight();
    colours = getsysvar_scrColours();
    vdp_cursor_enable(false);
    vdp_set_pixel_coordinates();
    snprintf(line, sizeof line, "D1 MODE 136: ack %u %ux%u %u col", ok, w, h,
             colours);
    log_line(line);
    t0 = getsysvar_time();
    for (i = 0; i < 40; i++) {                   /* wipe: bar grows per frame */
        vdp_gcol(0, C_BLACK);
        vdp_filled_rectangle(0, 0, 319, 239);
        vdp_gcol(0, (i & 1) ? C_BRIGHT_BLUE : C_BLUE);
        vdp_filled_rectangle(0, 0, (int)(i * 8), 239);
        draw_tile(1, (int)(i * 7), 100);
        vdp_cursor_tab(1, 1);
        vdp_set_text_colour(C_BRIGHT_YELLOW);
        printf("5 DOUBLE BUFFER %u", i);
        vdp_swap();
    }
    dt = getsysvar_time() - t0;
    snprintf(line, sizeof line, "D2 40 full frames + swap: %lu cs", (unsigned long)dt);
    log_line(line);
    vdp_cursor_tab(1, 4);
    vdp_set_text_colour(C_BRIGHT_WHITE);
    printf("D1 ack %u %ux%u %u col", ok, w, h, colours);
    vdp_cursor_tab(1, 6);
    printf("D2 40 frames: %lu cs", (unsigned long)dt);
    vdp_swap();
    pause_cs(300);
}

int main(int argc, char **argv)
{
    int which = argc > 1 ? argv[1][0] - '0' : 0;
    logfh = mos_fopen("vdptest.log", FA_WRITE | FA_CREATE_ALWAYS);
    log_line("VDPTEST");
    vdp_mode(8);
    vdp_cursor_enable(false);
    vdp_clear_screen();
    vdp_set_pixel_coordinates();
    make_tiles();
    kbuf_init(16);
    single = which != 0;
    if (which == 0 || which == 1)
        test_audio();
    if (which == 0 || which == 2)
        test_font();
    if (which == 0 || which == 3)
        test_palette();
    if (which == 0 || which == 4)
        test_sprites();
    if (which == 0 || which == 5)
        test_double_buffer();
    log_line("VDPTEST END");
    kbuf_deinit();
    vdp_mode(0);
    vdp_cursor_enable(true);
    if (logfh)
        mos_fclose(logfh);
    return 0;
}
