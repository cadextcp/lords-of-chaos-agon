/*
 * Agon frontend (M1 demo: the wizard house).
 *
 *   loc              walk the wizard through the house (arrows/WASD,
 *                    Shift+E new turn, ESC quit)
 *   loc --selftest   core self-test without VDP, exits the emulator (CI)
 *   loc --dump       additionally write map + view hash to loc.log per frame
 *   loc --bench      measure full and partial redraw times -> loc.log
 *   loc --keytest    keyboard spike: show/log every key event (issue #3)
 */
#include <agon/keyboard.h>
#include <agon/mos.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../core/chord.h"
#include "../core/colors.h"
#include "../core/selftest.h"
#include "../core/view.h"
#include "../core/world.h"
#include "emu.h"
#include "input.h"
#include "keytest.h"
#include "log.h"
#include "render.h"

#define ANIM_CS 40     /* candle flicker period in centiseconds */
#define WINDOW_CS 8    /* arrow chord window 80 ms (GDD 5.2, ADR 0007) */
#define DELAY_CS 35    /* held key: first repeat after 350 ms */
#define REPEAT_CS 20   /* then one step per 200 ms */

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

/* Move the active unit in direction mask m (chord.h); messages on failure. */
static void step(uint8_t m, bool dump)
{
    int8_t dx, dy;
    if (!chord_to_step(m, &dx, &dy))
        return;
    if (world_move_unit(&world, ACTIVE, dx, dy)) {
        render_message(1, C_GREY, "");
        frame(dump);
    } else if (world.units[ACTIVE].ap < world_step_cost(&world,
                   (int16_t)(world.units[ACTIVE].x + dx), (int16_t)(world.units[ACTIVE].y + dy),
                   dx != 0 && dy != 0)) {
        render_message(1, C_BRIGHT_RED, "Zu wenig AP - E fuer Zugende.");
    } else {
        render_message(1, C_BRIGHT_RED, "Da geht es nicht weiter.");
    }
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
    uint8_t phase = 0, m;
    uint16_t now;
    Chord chord;

    if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
        return selftest();
    if (argc > 1 && strcmp(argv[1], "--keytest") == 0) {
        log_open(true);
        keytest_run();
        log_close();
        render_shutdown();
        return 0;
    }
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
    render_message(2, C_BRIGHT_BLUE, "Pfeile+Akkorde  Pos1/Ende/Bild diag.");
    if (do_bench)
        bench();

    kbuf_init(16);
    chord_init(&chord, WINDOW_CS, DELAY_CS, REPEAT_CS);
    next_anim = getsysvar_time() + ANIM_CS;
    while (running) {
        now = (uint16_t)getsysvar_time();
        m = chord_poll(&chord, now);
        if (m)
            step(m, dump);
        if (getsysvar_time() >= next_anim) {   /* candle flicker */
            next_anim += ANIM_CS;
            view_set_phase(++phase);
            view_update(&world);
            render_fields();
        }
        if (!kbuf_poll_event(&e))
            continue;
        if (input_arrow(e.vkey)) {           /* movement by vkey (ADR 0007) */
            m = chord_key(&chord, input_arrow(e.vkey), e.isdown != 0, now);
            if (m)
                step(m, dump);
            continue;
        }
        if (!e.isdown)
            continue;
        if (input_diagonal(e.vkey)) {
            step(input_diagonal(e.vkey), dump);
        } else if (e.vkey == VK_ESC) {
            running = false;
        } else if (e.ascii == 'E') {
            world_new_turn(&world);
            render_message(1, C_BRIGHT_GREEN, "Neue Runde: AP aufgefuellt.");
            frame(dump);
        }
    }
    kbuf_deinit();

    render_shutdown();
    log_line("EXIT");
    log_close();
    return 0;
}
