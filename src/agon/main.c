/*
 * Agon frontend (M2: testland with turns).
 *
 *   loc              play: move the active unit, Tab/Space unit choice,
 *                    Shift+E ends the turn (with confirmation), ESC quits
 *   loc --selftest   core self-test without VDP, exits the emulator (CI)
 *   loc --dump       additionally write map + view hash to loc.log per frame
 *   loc --bench      measure full and partial redraw times -> loc.log
 *   loc --keytest    keyboard spike: show/log every key event (issue #3)
 *   loc --free-round1  lifting of the round 1 movement lock (PM 7) for
 *                    scripted emulator runs
 */
#include <agon/keyboard.h>
#include <agon/mos.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../core/chord.h"
#include "../core/colors.h"
#include "../core/names.h"
#include "../core/selftest.h"
#include "../core/turn.h"
#include "../core/view.h"
#include "../core/world.h"
#include "emu.h"
#include "input.h"
#include "keytest.h"
#include "log.h"
#include "mapfile.h"
#include "render.h"

#define MAP_TESTLAND "maps/testland.map"   /* relative to /loc (ADR 0008) */
#define MAP_HOUSE "maps/wizard_house.map"
#define TURN_SEED 42    /* fixed: emulator runs replay like the selftest */
#define ANIM_CS 40     /* candle flicker period in centiseconds */
#define BLINK_CS 30    /* cursor blink period (Amiga: flashing cursor) */
#define WINDOW_CS 8    /* arrow chord window 80 ms (GDD 5.2, ADR 0007) */
#define DELAY_CS 35    /* held key: first repeat after 350 ms */
#define REPEAT_CS 20   /* then one step per 200 ms */

static World world;
static Turns turns;
static bool cursor_on = true;
static bool confirm_end = false;   /* Shift+E asks before ending the turn */

/* The unit the player acts with; start_phase guarantees one of the phase
 * owner's units is active. */
static uint8_t active(void)
{
    if (turns.active < world.unit_count)
        return turns.active;
    return 0;
}

static void place_cursor(void)
{
    const Unit *u = &world.units[active()];
    render_cursor((int16_t)(u->x - view_origin_x()), (int16_t)(u->y - view_origin_y()),
                  CURSOR_GREEN, cursor_on);
}

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

/* Message line 0: whose unit is active (handover M2c). */
static void show_status(void)
{
    char buf[48];
    snprintf(buf, sizeof buf, "Runde %u - %s: %s", turns.round,
             name_owner(turns.phase), name_unit(&world.units[active()]));
    render_message(0, C_BRIGHT_WHITE, buf);
}

static void frame(bool dump)
{
    const Unit *u = &world.units[active()];
    view_follow(&world, u->x, u->y);
    view_update(&world);
    render_fields();
    cursor_on = true;
    place_cursor();
    render_panel(&world, active());
    show_status();
    if (dump)
        log_frame(&world, view_hash());
}

/* Move the active unit in direction mask m (chord.h); messages on failure. */
static void step(uint8_t m, bool dump)
{
    int8_t dx, dy;
    if (!chord_to_step(m, &dx, &dy))
        return;
    if (!turn_may_move(&turns)) {
        render_message(1, C_BRIGHT_RED, "Runde 1: nur Zaubern (PM 7).");
    } else if (world_move_unit(&world, active(), dx, dy)) {
        render_message(1, C_GREY, "");
        frame(dump);
    } else if (world.units[active()].ap < world_unit_step_cost(&world, active(),
                   (int16_t)(world.units[active()].x + dx), (int16_t)(world.units[active()].y + dy),
                   dx != 0 && dy != 0)) {
        render_message(1, C_BRIGHT_RED, "Zu wenig AP - Leertaste/Tab weiter.");
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
        view_animate(i);
        render_fields();
    }
    part_cs = getsysvar_time() - t0;

    t0 = getsysvar_time();
    for (i = 0; i < n; i++)
        render_cursor(3, 4, CURSOR_GREEN, (i & 1) != 0);
    snprintf(buf, sizeof buf, "BENCH cursor blink: %lu ms",
             (unsigned long)((getsysvar_time() - t0) * 10 / n));
    log_line(buf);

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
    bool dump = false, do_bench = false, free_round1 = false, running = true;
    const char *map_path;
    uint32_t next_anim, next_blink;
    uint8_t phase = 0, m, i;
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
    map_path = (argc > 1 && strcmp(argv[1], "--house") == 0) ? MAP_HOUSE : MAP_TESTLAND;
    do_bench = argc > 1 && strcmp(argv[1], "--bench") == 0;
    for (i = 1; i < (uint8_t)argc; i++)
        if (strcmp(argv[i], "--free-round1") == 0)
            free_round1 = true;

    log_open(dump || do_bench);
    log_line("BOOT");
    {
        uint32_t t0 = getsysvar_time();
        bool ok = mapfile_load(&world, map_path);
        char buf[48];
        snprintf(buf, sizeof buf, "MAP %s %s in %lu ms", map_path, ok ? "loaded" : "FAILED",
                 (unsigned long)((getsysvar_time() - t0) * 10));
        log_line(buf);
        if (!ok) {
            printf("%s\r\n", buf);
            log_close();
            return 1;
        }
    }
    turn_init(&turns, &world, TURN_SEED, 1u << OWN_P1);
    if (free_round1)
        turns.round1_lock = false;
    if (!render_init()) {
        log_line("ERR render_init");
        log_close();
        return 1;
    }
    frame(dump);
    render_message(1, C_BRIGHT_YELLOW, "Willkommen in Testland.");
    render_message(2, C_BRIGHT_BLUE, "Tab Einheit  Leertaste fertig  E Zugende");
    if (do_bench)
        bench();

    kbuf_init(16);
    chord_init(&chord, WINDOW_CS, DELAY_CS, REPEAT_CS);
    next_anim = getsysvar_time() + ANIM_CS;
    next_blink = getsysvar_time() + BLINK_CS;
    while (running) {
        /* Drain every queued key event first: drawing a step can take longer
         * than the repeat delay, and a stale "held" state would otherwise
         * repeat keys that were already released (ADR 0007). */
        while (running && kbuf_poll_event(&e)) {
            now = (uint16_t)getsysvar_time();
            if (input_arrow(e.vkey)) {           /* movement by vkey */
                m = chord_key(&chord, input_arrow(e.vkey), e.isdown != 0, now);
                if (m)
                    step(m, dump);
                continue;
            }
            if (!e.isdown)
                continue;
            if (input_diagonal(e.vkey)) {
                step(input_diagonal(e.vkey), dump);
            } else if (e.vkey == VK_TAB) {       /* next/previous own unit */
                confirm_end = false;
                turn_next_unit(&turns, &world, (e.kmod & KMOD_SHIFT) != 0);
                render_message(1, C_GREY, "");
                if (!turn_units_left(&turns, &world))
                    render_message(1, C_BRIGHT_YELLOW,
                                   "Alle Einheiten fertig - E fuer Zugende.");
                frame(dump);
            } else if (e.vkey == VK_SPACE) {     /* unit finished */
                confirm_end = false;
                turn_finish_unit(&turns, &world);
                if (turn_units_left(&turns, &world))
                    render_message(1, C_GREY, "");
                else
                    render_message(1, C_BRIGHT_YELLOW,
                                   "Alle Einheiten fertig - E fuer Zugende.");
                frame(dump);
            } else if (e.ascii == 'E') {         /* Shift+E: turn end */
                if (!confirm_end) {
                    confirm_end = true;
                    render_message(1, C_BRIGHT_YELLOW,
                                   "Zug beenden? Nochmal E=ja, Esc=nein.");
                } else {
                    confirm_end = false;
                    turn_end_phase(&turns, &world);
                    render_message(1, C_BRIGHT_GREEN, "Neue Runde.");
                    frame(dump);
                }
            } else if (e.vkey == VK_ESC) {
                if (confirm_end) {
                    confirm_end = false;
                    render_message(1, C_GREY, "");
                } else {
                    running = false;
                }
            }
        }
        now = (uint16_t)getsysvar_time();
        m = chord_poll(&chord, now);
        if (m)
            step(m, dump);
        if (getsysvar_time() >= next_anim) {   /* candle and water animation */
            next_anim += ANIM_CS;
            view_animate(++phase);
            render_fields();
        }
        if (getsysvar_time() >= next_blink) {  /* blinking cursor sprite */
            next_blink += BLINK_CS;
            cursor_on = !cursor_on;
            place_cursor();
        }
    }
    kbuf_deinit();

    render_shutdown();
    log_line("EXIT");
    log_close();
    return 0;
}
