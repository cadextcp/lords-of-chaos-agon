#include "keytest.h"

#include <agon/keyboard.h>
#include <agon/mos.h>
#include <agon/vdp.h>
#include <stdio.h>
#include <string.h>

#include "../core/chord.h"
#include "../core/colors.h"
#include "input.h"
#include "log.h"

#define LOG_TOP 3
#define LOG_ROWS 19
#define WINDOW_CS 8    /* chord window 80 ms (GDD 5.2) */
#define DELAY_CS 35    /* held key: first repeat after 350 ms */
#define REPEAT_CS 20   /* then one step per 200 ms */

static char rows[LOG_ROWS][40];
static uint8_t row_count;

static const char *dir_name(uint8_t m)
{
    switch (m) {
    case ARROW_UP: return "N";
    case ARROW_DOWN: return "S";
    case ARROW_LEFT: return "W";
    case ARROW_RIGHT: return "O";
    case ARROW_UP | ARROW_RIGHT: return "NO";
    case ARROW_UP | ARROW_LEFT: return "NW";
    case ARROW_DOWN | ARROW_RIGHT: return "SO";
    case ARROW_DOWN | ARROW_LEFT: return "SW";
    default: return "?";
    }
}

static void at(uint8_t col, uint8_t row, uint8_t colour, const char *s)
{
    vdp_cursor_tab(col, row);
    vdp_set_text_colour(colour);
    printf("%-39.39s", s);
}

static void push_row(const char *s)
{
    uint8_t i;
    if (row_count < LOG_ROWS) {
        strcpy(rows[row_count++], s);
    } else {
        for (i = 1; i < LOG_ROWS; i++)
            strcpy(rows[i - 1], rows[i]);
        strcpy(rows[LOG_ROWS - 1], s);
    }
    for (i = 0; i < row_count; i++)
        at(0, (uint8_t)(LOG_TOP + i), i + 1 == row_count ? C_BRIGHT_WHITE : C_GREY, rows[i]);
}

static void show_dir(uint8_t m, uint16_t now)
{
    char buf[40];
    snprintf(buf, sizeof buf, "RICHTUNG %-2s  (t=%u)", dir_name(m), now);
    at(0, 24, C_BRIGHT_GREEN, buf);
    log_line(buf);
}

void keytest_run(void)
{
    struct keyboard_event_t e;
    Chord chord;
    char buf[40];
    uint8_t esc = 0, m;
    uint16_t now;

    vdp_mode(8);
    vdp_cursor_enable(false);
    vdp_clear_screen();
    at(0, 0, C_BRIGHT_YELLOW, "KEYTEST (#3)   ESC 2x = Ende");
    at(0, 1, C_BRIGHT_CYAN, "DN/UP ascii vkey mod  t(cs)  Pfeil");
    at(0, 26, C_GREY, "Pfeil-Akkord: 2 Pfeile < 80 ms = diag.");
    at(0, 27, C_GREY, "Bitte testen: Pfeile, Akkorde, Pos1,");
    at(0, 28, C_GREY, "Ende, Bild, < >, Fn-Ziffern, halten.");
    log_line("KEYTEST start");
    chord_init(&chord, WINDOW_CS, DELAY_CS, REPEAT_CS);
    kbuf_init(32);

    while (esc < 2) {
        now = (uint16_t)getsysvar_time();
        m = chord_poll(&chord, now);
        if (m)
            show_dir(m, now);
        if (!kbuf_poll_event(&e))
            continue;
        if (e.isdown && e.vkey == VK_ESC)
            esc++;
        else if (e.isdown)
            esc = 0;
        {
            uint8_t a = input_arrow(e.vkey), d = input_diagonal(e.vkey);
            snprintf(buf, sizeof buf, "%s %02X '%c' %02X %02X %5u %s",
                     e.isdown ? "DN" : "UP", e.ascii,
                     (e.ascii >= 32 && e.ascii < 127) ? e.ascii : ' ',
                     e.vkey, e.kmod, now, a ? dir_name(a) : d ? dir_name(d) : "");
            push_row(buf);
            log_line(buf);
            if (a) {
                m = chord_key(&chord, a, e.isdown != 0, now);
                if (m)
                    show_dir(m, now);
            } else if (d && e.isdown) {
                show_dir(d, now);
            }
        }
    }
    kbuf_deinit();
    log_line("KEYTEST end");
}
