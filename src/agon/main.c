/*
 * Agon frontend (M1 demo: the wizard house).
 *
 *   loc              walk the wizard through the house (arrows/WASD,
 *                    Shift+E new turn, ESC quit)
 *   loc --selftest   core self-test without VDP, exits the emulator (CI)
 *   loc --dump       additionally write map + view hash to loc.log per frame
 *   loc --bench      measure full and partial redraw times -> loc.log
 */
#include <agon/keyboard.h>
#include <agon/mos.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../core/colors.h"
#include "../core/selftest.h"
#include "../core/view.h"
#include "../core/world.h"
#include "emu.h"
#include "log.h"
#include "render.h"

#define KEY_ESC 27
/* MOS ASCII codes for the cursor keys */
#define KEY_LEFT 8
#define KEY_RIGHT 21
#define KEY_UP 11
#define KEY_DOWN 10
#define ANIM_CS 40   /* candle flicker period in centiseconds */

static World world;
static const uint8_t ACTIVE = 0;   /* the wizard */

static void print_line(const char *line)
{
    printf("%s\r\n", line);
}

static int selftest(void)
{
    uint16_t fails = core_selftest(print_line);
    printf(fails ? "=== TEST FAIL ===\r\n" : "=== TEST PASS ===\r\n");
    emu_exit(fails ? 1 : 0);
    return fails ? 1 : 0;
}

static bool key_to_step(uint8_t ascii, int8_t *dx, int8_t *dy)
{
    *dx = 0;
    *dy = 0;
    switch (ascii) {
    case KEY_UP:    case 'w': case 'W': *dy = -1; break;
    case KEY_DOWN:  case 's': case 'S': *dy = 1; break;
    case KEY_LEFT:  case 'a': case 'A': *dx = -1; break;
    case KEY_RIGHT: case 'd': case 'D': *dx = 1; break;
    default: return false;
    }
    return true;
}

static void frame(bool dump)
{
    const Unit *u = &world.units[ACTIVE];
    view_follow(&world, u->x, u->y);
    view_set_cursor(u->x, u->y, T_CURSOR_GREEN);
    view_update(&world);
    render_fields();
    render_panel(&world, ACTIVE);
    if (dump)
        log_frame(&world, view_hash());
}

/* Times N full redraws and N single-field redraws (candle flicker). */
static void bench(void)
{
    char buf[64];
    uint32_t t0, full_cs, part_cs;
    uint8_t i, fields = 0;
    const uint8_t n = 10;

    t0 = getsysvar_time();
    for (i = 0; i < n; i++) {
        view_invalidate();
        view_update(&world);
        fields = render_fields();
    }
    full_cs = getsysvar_time() - t0;

    t0 = getsysvar_time();
    for (i = 0; i < n; i++) {
        view_set_phase(i);
        view_update(&world);
        render_fields();
    }
    part_cs = getsysvar_time() - t0;

    t0 = getsysvar_time();
    for (i = 0; i < n; i++)
        view_update(&world);                 /* compose only, nothing changes */
    snprintf(buf, sizeof buf, "BENCH compose 81 fields: %lu ms",
             (unsigned long)((getsysvar_time() - t0) * 10 / n));
    log_line(buf);
    render_message(2, C_BRIGHT_YELLOW, buf);

    snprintf(buf, sizeof buf, "BENCH full %u fields: %lu ms/frame",
             fields, (unsigned long)(full_cs * 10 / n));
    log_line(buf);
    render_message(0, C_BRIGHT_YELLOW, buf);
    snprintf(buf, sizeof buf, "BENCH candles only: %lu ms/frame",
             (unsigned long)(part_cs * 10 / n));
    log_line(buf);
    render_message(1, C_BRIGHT_YELLOW, buf);
}

int main(int argc, char **argv)
{
    struct keyboard_event_t e;
    bool dump = false, do_bench = false, running = true;
    uint32_t next_anim;
    uint8_t phase = 0;
    int8_t dx, dy;

    if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
        return selftest();
    dump = argc > 1 && strcmp(argv[1], "--dump") == 0;
    do_bench = argc > 1 && strcmp(argv[1], "--bench") == 0;

    log_open(dump || do_bench);
    log_line("BOOT");
    world_load(&world, &MAP_WIZARD_HOUSE);
    if (!render_init()) {
        log_line("ERR render_init");
        log_close();
        return 1;
    }
    view_set_origin(0, 0);
    frame(dump);
    render_message(0, C_BRIGHT_YELLOW, "Willkommen im Zauberer-Haus.");
    render_message(2, C_BRIGHT_BLUE, "Pfeile/WASD gehen  E Zugende  ESC");
    if (do_bench)
        bench();

    kbuf_init(16);
    next_anim = getsysvar_time() + ANIM_CS;
    while (running) {
        if (getsysvar_time() >= next_anim) {   /* candle flicker */
            next_anim += ANIM_CS;
            view_set_phase(++phase);
            view_update(&world);
            render_fields();
        }
        if (!kbuf_poll_event(&e) || !e.isdown)
            continue;
        if (e.ascii == KEY_ESC) {
            running = false;
        } else if (e.ascii == 'E') {
            world_new_turn(&world);
            render_message(1, C_BRIGHT_GREEN, "Neue Runde: AP aufgefuellt.");
            frame(dump);
        } else if (key_to_step(e.ascii, &dx, &dy)) {
            if (world_move_unit(&world, ACTIVE, dx, dy)) {
                render_message(1, C_GREY, "");
                frame(dump);
            } else if (world.units[ACTIVE].ap < 4) {
                render_message(1, C_BRIGHT_RED, "Keine AP mehr - E fuer Zugende.");
            } else {
                render_message(1, C_BRIGHT_RED, "Da geht es nicht weiter.");
            }
        }
    }
    kbuf_deinit();

    render_shutdown();
    log_line("EXIT");
    log_close();
    return 0;
}
