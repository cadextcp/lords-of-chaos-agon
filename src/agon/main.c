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
 *   loc --fly        own flyers start airborne (demo; the ISO '<' key is
 *                    not sendable to the emulator, #3)
 */
#include <agon/keyboard.h>
#include <agon/mos.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../core/chord.h"
#include "../core/combat.h"
#include "../core/colors.h"
#include "../core/gen/data.h"
#include "../core/names.h"
#include "../core/selftest.h"
#include "../core/sight.h"
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
static Sight p1_sight;             /* hidden map of the human player */
static bool cursor_on = true;
static bool confirm_end = false;   /* Shift+E asks before ending the turn */
static bool look_mode = false;     /* x: examine any field (GDD 5.1) */
static int16_t look_x, look_y;
static bool spell_list = false;    /* c: pick a spell (GDD 5.1) */
static bool targeting = false;     /* aiming a spell (Enter casts, Esc ends) */
static uint8_t target_spell;
static int16_t target_x, target_y;
static Spellbook books[OWN_NEUTRAL];   /* starting books until M3g */

/* The unit the player acts with; start_phase guarantees one of the phase
 * owner's units is active. */
static uint8_t active(void)
{
    if (turns.active < world.unit_count)
        return turns.active;
    return 0;
}

/* Targeting cursor colour (GDD 5.1): yellow ground, blue air, red when
   out of range or without a line of sight. */
static uint8_t target_cursor_colour(void)
{
    const Unit *u = &world.units[active()];
    int16_t dx = (int16_t)(target_x - u->x), dy = (int16_t)(target_y - u->y);
    if (world.wrap) {
        if (dx > world.w / 2) dx = (int16_t)(dx - world.w);
        if (dx < -world.w / 2) dx = (int16_t)(dx + world.w);
        if (dy > world.h / 2) dy = (int16_t)(dy - world.h);
        if (dy < -world.h / 2) dy = (int16_t)(dy + world.h);
    }
    if (dx > SPELL_RANGE || dx < -SPELL_RANGE || dy > SPELL_RANGE || dy < -SPELL_RANGE)
        return CURSOR_RED;
    if (!sight_has_los(&world, u->x, u->y, target_x, target_y))
        return CURSOR_RED;
    if (world_unit_at(&world, target_x, target_y, UL_AIR) != NO_UNIT)
        return CURSOR_BLUE;
    return CURSOR_YELLOW;
}

static void place_cursor(void)
{
    if (targeting) {
        render_cursor((int16_t)(target_x - view_origin_x()),
                      (int16_t)(target_y - view_origin_y()),
                      target_cursor_colour(), cursor_on);
        return;
    }
    if (look_mode) {
        render_cursor((int16_t)(look_x - view_origin_x()),
                      (int16_t)(look_y - view_origin_y()), CURSOR_WHITE, cursor_on);
        return;
    }
    {
        const Unit *u = &world.units[active()];
        render_cursor((int16_t)(u->x - view_origin_x()), (int16_t)(u->y - view_origin_y()),
                      (u->flags & UF_FLYING) ? CURSOR_BLUE : CURSOR_GREEN, cursor_on);
    }
}

static void print_line(const char *line)
{
    printf("%s\r\n", line);
}

static int selftest(void)
{
    uint16_t fails;
    selftest_set_verbose(false);
    fails = core_selftest(print_line);
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
    if (look_mode || targeting) {       /* free cursor over the map */
        int16_t cx = targeting ? target_x : look_x;
        int16_t cy = targeting ? target_y : look_y;
        char buf[24];
        view_follow(&world, cx, cy);
        view_update(&world);
        render_fields();
        cursor_on = true;
        place_cursor();
        render_panel_at(&world, &p1_sight, cx, cy);
        describe_field(&world, &p1_sight, cx, cy, buf, sizeof buf);
        render_message(1, C_BRIGHT_CYAN, buf);
        if (targeting)
            render_message(2, C_GREY, "Enter wirkt, Esc bricht ab.");
        if (dump)
            log_frame(&world, view_hash());
        return;
    }
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
}

/* Sight changes with every own move and at the round boundary (enemy
 * movement enters or leaves view); recomputing is cheap enough to do
 * exactly then, not per frame. */
static void update_sight(void)
{
    sight_compute(&world, &p1_sight);
}

/* Cast the aimed spell at (target_x, target_y); messages on the outcome. */
static void cast_targeted(bool dump)
{
    SpellShot shot;
    char msg[48];
    uint8_t wiz = active();
    bool ok;
    uint8_t count_before = world.unit_count;

    if (target_spell == SP_MAGIC_LIGHTNING)
        ok = spell_lightning(&world, &books[OWN_P1], wiz, target_x, target_y,
                             &turns.rng, &shot);
    else
        ok = spell_bolt(&world, &books[OWN_P1], wiz, target_spell, target_x,
                        target_y, &turns.rng, &shot);
    if (!ok) {
        render_message(1, C_BRIGHT_RED, "Ausser Reichweite oder Sicht.");
        return;
    }
    if (shot.hit)
        snprintf(msg, sizeof msg, "Zauber trifft: %u Schaden.", shot.damage);
    else
        snprintf(msg, sizeof msg, "Zauber verpufft.");
    render_message(1, shot.hit ? C_BRIGHT_YELLOW : C_GREY, msg);
    if (shot.terrain_smashed)
        render_message(2, C_BRIGHT_YELLOW, "Blitz schlaegt das Terrain ein!");
    if (world.unit_count < count_before) {
        turn_revalidate(&turns, &world);
        render_message(2, C_BRIGHT_RED, "Mindestens eine Kreatur stirbt.");
    }
    update_sight();
    frame(dump);
}

/* Move the active unit in direction mask m (chord.h); messages on failure. */
static void step(uint8_t m, bool dump)
{
    int8_t dx, dy;
    if (!chord_to_step(m, &dx, &dy))
        return;
    if (look_mode || targeting) {       /* free cursor, no costs */
        int16_t *cx = targeting ? &target_x : &look_x;
        int16_t *cy = targeting ? &target_y : &look_y;
        *cx = (int16_t)(*cx + dx);
        *cy = (int16_t)(*cy + dy);
        if (!world_wrap(&world, cx, cy)) {
            *cx = (int16_t)(*cx - dx);
            *cy = (int16_t)(*cy - dy);
        }
        frame(dump);
        return;
    }
    if (!turn_may_move(&turns)) {
        render_message(1, C_BRIGHT_RED, "Runde 1: nur Zaubern (PM 7).");
    } else if (world_move_unit(&world, active(), dx, dy)) {
        render_message(1, C_GREY, "");
        update_sight();
        frame(dump);
    } else {
        int16_t nx = (int16_t)(world.units[active()].x + dx);
        int16_t ny = (int16_t)(world.units[active()].y + dy);
        uint8_t other;
        switch (world_bump_kind(&world, active(), dx, dy)) {
        case BUMP_DOOR:
            if (world_open_door(&world, active(), nx, ny)) {
                render_message(1, C_BRIGHT_GREEN, "Tuer geoeffnet.");
                update_sight();        /* the open door changes lines of sight */
            } else if (!(CREATURES[world.units[active()].kind].flags & CF_USE)) {
                render_message(1, C_BRIGHT_RED, "Keine Haende fuer die Tuer.");
            } else {
                render_message(1, C_BRIGHT_RED, "Zu wenig AP fuer die Tuer.");
            }
            frame(dump);
            return;
        case BUMP_NO_AP:
            render_message(1, C_BRIGHT_RED, "Zu wenig AP - Leertaste/Tab weiter.");
            return;
        case BUMP_UNIT: {
            CombatResult r;
            char msg[48], name[16], aname[16];
            uint8_t att = active();
            other = world_unit_at(&world, nx, ny,
                                  (world.units[att].flags & UF_FLYING) ? UL_AIR : UL_GROUND);
            if (other == NO_UNIT || world.units[other].owner == OWN_P1) {
                render_message(1, C_BRIGHT_RED, "Da geht es nicht weiter.");
                return;
            }
            snprintf(name, sizeof name, "%s", name_unit(&world.units[other]));
            snprintf(aname, sizeof aname, "%s", name_unit(&world.units[att]));
            if (!combat_melee(&world, &turns.rng, att, other, &r)) {
                render_message(1, C_BRIGHT_RED, "Angriff nicht moeglich.");
                return;
            }
            if (r.died)
                snprintf(msg, sizeof msg, "%s stirbt!", name);
            else if (r.wound)
                snprintf(msg, sizeof msg, "Treffer: %u. Toedliche Wunde!", r.damage);
            else if (r.hit)
                snprintf(msg, sizeof msg, "Treffer: %u Schaden.", r.damage);
            else
                snprintf(msg, sizeof msg, "Verfehlt.");
            render_message(1, r.hit || r.died ? C_BRIGHT_YELLOW : C_GREY, msg);
            if (r.died)
                turn_on_unit_removed(&turns, &world, other);
            if (r.returned) {
                if (r.attacker_died) {
                    snprintf(msg, sizeof msg, "Rueckschlag toetet %s!", aname);
                } else if (r.return_hit) {
                    snprintf(msg, sizeof msg, "Rueckschlag: %u Schaden.", r.return_damage);
                } else {
                    snprintf(msg, sizeof msg, "Rueckschlag: daneben.");
                }
                render_message(2, r.attacker_died ? C_BRIGHT_RED :
                               r.return_hit ? C_BRIGHT_RED : C_GREY, msg);
            }
            if (r.attacker_died)
                turn_on_unit_removed(&turns, &world, att);
            update_sight();
            frame(dump);
            return;
        }
        case BUMP_TERRAIN: {
            bool destroyed;
            char msg[48];
            uint8_t dmg = combat_terrain(&world, &turns.rng, active(), nx, ny, &destroyed);
            if (dmg == 0) {
                if (world_blocks(&world, nx, ny) &&
                    FEATURE_TOUGH[world_feature(&world, nx, ny)] == 0)
                    render_message(1, C_BRIGHT_RED, "Unzerstoerbar.");
                else if (world_blocks(&world, nx, ny))
                    render_message(1, C_BRIGHT_RED, "Zu wenig AP zum Zuschlagen.");
                else
                    render_message(1, C_BRIGHT_RED, "Da geht es nicht weiter.");
            } else if (destroyed) {
                snprintf(msg, sizeof msg, "%s zerstoert!", name_feature(world_feature(&world, nx, ny)));
                render_message(1, C_BRIGHT_YELLOW, msg);
                update_sight();            /* rubble changes lines of sight */
            } else {
                snprintf(msg, sizeof msg, "%u Schaden.", dmg);
                render_message(1, C_GREY, msg);
            }
            frame(dump);
            return;
        }
        default:
            render_message(1, C_BRIGHT_RED, "Da geht es nicht weiter.");
            return;
        }
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

    t0 = getsysvar_time();
    for (i = 0; i < n; i++)                  /* line of sight, both p1 units */
        sight_compute(&world, &p1_sight);
    snprintf(buf, sizeof buf, "BENCH sight compute: %lu ms",
             (unsigned long)((getsysvar_time() - t0) * 10 / n));
    log_line(buf);

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
    bool dump = false, do_bench = false, free_round1 = false, do_fly = false;
    bool running = true;
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
        else if (strcmp(argv[i], "--fly") == 0)
            do_fly = true;

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
    if (do_fly) {                    /* the ISO key is not sendable yet (#3) */
        uint8_t k;
        for (k = 0; k < world.unit_count; k++)
            if (world.units[k].owner == OWN_P1 && world.units[k].ap_fly)
                world.units[k].flags |= UF_FLYING;
    }
    {
        uint8_t o;
        for (o = 0; o < OWN_NEUTRAL; o++)
            spellbook_default(&books[o], o);
    }
    sight_init(&p1_sight, OWN_P1);
    update_sight();
    view_set_sight(&p1_sight);
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
            if (spell_list && e.isdown) {        /* letters pick (a is WASD too) */
                if (e.vkey == VK_ESC) {
                    spell_list = false;
                    view_invalidate();
                    frame(dump);
                } else if (e.ascii >= 'a' && e.ascii <= 'z') {
                    uint16_t pick = e.ascii - 'a';
                    uint16_t i, n = 0;
                    spell_list = false;
                    view_invalidate();
                    for (i = 0; i < SPELL_COUNT; i++) {
                        if (books[OWN_P1].level[i] == 0)
                            continue;
                        if (n == pick)
                            break;
                        n++;
                    }
                    if (i < SPELL_COUNT) {
                        uint8_t wiz = active();
                        if (world.units[wiz].kind != CR_WIZARD) {
                            render_message(1, C_BRIGHT_RED, "Nur Zauberer zaubern.");
                        } else if (SPELLS[i].category == SPC_SUMMON) {
                            uint8_t got = spell_summon(&world, &books[OWN_P1], wiz, (uint8_t)i);
                            if (got)
                                render_message(1, C_BRIGHT_GREEN, "Beschworen!");
                            else
                                render_message(1, C_BRIGHT_RED, "Kein Platz - Mana verloren.");
                            update_sight();
                        } else if (i == SP_MAGIC_BOLT || i == SP_MAGIC_LIGHTNING) {
                            targeting = true;
                            target_spell = (uint8_t)i;
                            target_x = world.units[wiz].x;
                            target_y = world.units[wiz].y;
                        } else {
                            render_message(1, C_BRIGHT_YELLOW, "Zauber folgt spaeter.");
                        }
                    }
                    frame(dump);
                }
                continue;
            }
            if (targeting && e.isdown && !input_arrow(e.vkey) &&
                !input_diagonal(e.vkey)) {        /* aim: Enter casts, Esc ends */
                if (e.vkey == VK_ESC) {
                    targeting = false;
                    render_message(1, C_GREY, "");
                    render_message(2, C_GREY, "");
                    frame(dump);
                } else if (e.ascii == 13 || e.vkey == VK_SPACE ||
                           e.ascii == 'c') {
                    /* aiming at the caster cancels without cost (GDD 7.1) */
                    if (target_x == world.units[active()].x &&
                        target_y == world.units[active()].y) {
                        targeting = false;
                        render_message(1, C_GREY, "Abgebrochen.");
                        render_message(2, C_GREY, "");
                        frame(dump);
                    } else {
                        targeting = false;
                        cast_targeted(dump);
                    }
                }
                continue;
            }
            if (input_arrow(e.vkey)) {           /* movement by vkey */
                if (!spell_list) {
                    m = chord_key(&chord, input_arrow(e.vkey), e.isdown != 0, now);
                    if (m)
                        step(m, dump);
                }
                continue;
            }
            if (!e.isdown)
                continue;
            if (input_diagonal(e.vkey)) {
                if (!spell_list)
                    step(input_diagonal(e.vkey), dump);
            } else if (look_mode) {                /* x: examine (GDD 5.1) */
                if (e.vkey == VK_ESC || e.ascii == 'x') {
                    look_mode = false;
                    render_message(1, C_GREY, "");
                    frame(dump);
                }
            } else if (e.ascii == 'c') {         /* spell list (GDD 5.1) */
                confirm_end = false;
                if (world.units[active()].kind == CR_WIZARD) {
                    spell_list = true;
                    render_spell_list(&books[OWN_P1]);
                } else {
                    render_message(1, C_BRIGHT_RED, "Nur Zauberer zaubern.");
                }
            } else if (e.ascii == 'x') {
                confirm_end = false;
                look_mode = true;
                look_x = world.units[active()].x;
                look_y = world.units[active()].y;
                frame(dump);
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
                    update_sight();
                    frame(dump);
                }
            } else if (e.ascii == '<') {              /* take off */
                confirm_end = false;
                if (world_take_off(&world, active()))
                    render_message(1, C_BRIGHT_GREEN, "Steigt auf.");
                else if (world.units[active()].ap_fly == 0)
                    render_message(1, C_BRIGHT_RED, "Diese Kreatur fliegt nicht.");
                else if (world.units[active()].ap < ACTIONS[ACT_TAKE_OFF].ap)
                    render_message(1, C_BRIGHT_RED, "Zu wenig AP zum Aufsteigen.");
                else
                    render_message(1, C_BRIGHT_RED, "Da fliegt schon einer.");
                update_sight();
                frame(dump);
            } else if (e.ascii == '>') {              /* land */
                confirm_end = false;
                if (world_land(&world, active()))
                    render_message(1, C_BRIGHT_GREEN, "Landet.");
                else if (!(world.units[active()].flags & UF_FLYING))
                    render_message(1, C_BRIGHT_RED, "Die Kreatur fliegt nicht.");
                else if (world.units[active()].ap < ACTIONS[ACT_LAND].ap)
                    render_message(1, C_BRIGHT_RED, "Zu wenig AP zum Landen.");
                else
                    render_message(1, C_BRIGHT_RED, "Kein Platz zum Landen.");
                update_sight();
                frame(dump);
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
