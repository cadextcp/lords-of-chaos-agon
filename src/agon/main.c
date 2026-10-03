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

#include "../core/ai.h"
#include "../core/area.h"
#include "../core/ride.h"
#include "../core/brew.h"
#include "../core/chord.h"
#include "../core/combat.h"
#include "../core/colors.h"
#include "../core/game.h"
#include "../core/gen/data.h"
#include "../core/items.h"
#include "../core/names.h"
#include "../core/selftest.h"
#include "../core/sight.h"
#include "../core/turn.h"
#include "../core/wizard.h"
#include "../core/view.h"
#include "../core/world.h"
#include "emu.h"
#include "input.h"
#include "keytest.h"
#include "log.h"
#include "mapfile.h"
#include "render.h"

#define MAP_SCENARIO "maps/many_coloured_land.map"   /* scenario 1 (GDD 9.1) */
#define MAP_TESTLAND "maps/testland.map"      /* dev map (ADR 0008) */
#define MAP_HOUSE "maps/wizard_house.map"
#define TURN_SEED 42    /* fixed: emulator runs replay like the selftest */
#define ANIM_CS 40     /* candle flicker period in centiseconds */
#define BLINK_CS 30    /* cursor blink period (Amiga: flashing cursor) */
#define WINDOW_CS 8    /* arrow chord window 80 ms (GDD 5.2, ADR 0007) */
#define DELAY_CS 35    /* held key: first repeat after 350 ms */
#define REPEAT_CS 20   /* then one step per 200 ms */

static World world;
static Turns turns;
static Game game;                  /* portal and victory points (M3e) */
static Sight p1_sight;             /* hidden map of the human player */
static bool cursor_on = true;
static bool confirm_end = false;   /* Shift+E asks before ending the turn */
static bool look_mode = false;     /* x: examine any field (GDD 5.1) */
static int16_t look_x, look_y;
static bool spell_list = false;    /* c: pick a spell (GDD 5.1) */
static bool targeting = false;     /* aiming (Enter casts/throws/fires) */
static bool game_ended = false;    /* final score shown, only Esc left */
typedef enum { TA_SPELL, TA_THROW, TA_FIRE } TargetKind;
static TargetKind target_kind;
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
    selftest_set_verbose(false);         /* emulator console loses long tails */
    fails = core_selftest(print_line);
    printf(fails ? "=== TEST FAIL ===\r\n" : "=== TEST PASS ===\r\n");
    /* The fake VDP drops the first console bytes after the boot banner
     * (packet desync); a sacrificial line keeps the verdict readable. */
    printf("\r\n.");
    fflush(stdout);                      /* reach the UART before the exit */
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
    if (game.eye_rounds > 0)
        sight_add_eye(&p1_sight, &world, game.eye_x, game.eye_y);
}

/* After any action that may kill: credit the logged kills (M3e) and
 * find the active unit again - deaths reorder the unit list. */
static void settle(void)
{
    game_credit_kills(&game, &world);
    turn_revalidate(&turns, &world);
}

/* Round hook for turn_end_phase: the portal opens on its round, also
 * in rounds the AI plays on its own. */
static void on_round(Turns *t, World *w, void *ctx)
{
    area_round_end(w, &t->rng);          /* area effects tick (M4d) */
    (void)w;
    game_new_round((Game *)ctx, t->round);
}

/* Throw or fire at the aimed field (direction = first step towards it). */
static void throw_or_fire(bool dump)
{
    const Unit *u = &world.units[active()];
    int16_t dx = (int16_t)(target_x - u->x), dy = (int16_t)(target_y - u->y);
    int8_t sx, sy;
    if (dx > 0) sx = 1; else if (dx < 0) sx = -1; else sx = 0;
    if (dy > 0) sy = 1; else if (dy < 0) sy = -1; else sy = 0;
    if (target_kind == TA_THROW) {
        if (brew_throw_vial(&world, &turns.rng, active(), sx, sy) ||
            items_throw(&world, &turns.rng, active(), sx, sy))
            render_message(1, C_BRIGHT_YELLOW, "Geworfen!");
        else
            render_message(1, C_BRIGHT_RED, "Nichts zu werfen.");
        settle();
    } else {
        uint8_t dmg = 0;
        if (items_fire(&world, &turns.rng, active(), target_x, target_y, &dmg)) {
            if (dmg)
                render_message(1, C_BRIGHT_YELLOW, "Schuss trifft!");
            else
                render_message(1, C_GREY, "Schuss daneben.");
            settle();
        } else {
            render_message(1, C_BRIGHT_RED, "Kein Ziel in Reichweite.");
        }
    }
    update_sight();
    frame(dump);
}

/* Cast the aimed spell at (target_x, target_y); messages on the outcome. */
static void cast_targeted(bool dump)
{
    SpellShot shot;
    char msg[48];
    uint8_t wiz = active();
    bool ok;
    uint8_t count_before = world.unit_count;

    if (target_spell == SP_MAGIC_BOLT || target_spell == SP_MAGIC_LIGHTNING) {
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
    } else {
        CastResult cr = spell_apply(&world, &books[OWN_P1], wiz, target_spell,
                                    target_x, target_y, &turns.rng, &shot);
        if (cr == CAST_REJECTED) {
            render_message(1, C_BRIGHT_RED, "Ausser Reichweite oder Sicht.");
            return;
        }
        if (cr == CAST_BAD_TERRAIN) {
            render_message(1, C_BRIGHT_RED, "Das Ziel nimmt das nicht an.");
            return;
        }
        if (cr == CAST_NO_RES) {
            render_message(1, C_GREY, "Das Ziel widersteht.");
            settle();
            update_sight();
            frame(dump);
            return;
        }
        if (target_spell == SP_MAGIC_FIRE || target_spell == SP_GOOEY_BLOB ||
            target_spell == SP_TANGLE_VINE || target_spell == SP_FLOOD) {
            AreaKind kind = target_spell == SP_MAGIC_FIRE ? AREA_FIRE
                          : target_spell == SP_GOOEY_BLOB ? AREA_BLOB
                          : target_spell == SP_TANGLE_VINE ? AREA_VINE
                          : AREA_FLOOD;
            /* range, sight, terrain and payment: spell_apply */
            render_message(1, C_BRIGHT_MAGENTA,
                           kind == AREA_FIRE ? "Es brennt!"
                           : kind == AREA_BLOB ? "Klebriger Brei!"
                           : kind == AREA_VINE ? "Ranken wachsen!"
                           : "Die Flut steigt!");
            update_sight();
            frame(dump);
            return;
        }
        if (target_spell == SP_MAGIC_EYE) {
            game.eye_x = target_x;      /* reveal from there (GDD 7.2) */
            game.eye_y = target_y;
            game.eye_rounds = 1;
            render_message(1, C_BRIGHT_CYAN, "Auge eroeffnet.");
            settle();
            update_sight();
            frame(dump);
            return;
        }
        if (target_spell == SP_MAGIC_SHIELD) {
            render_message(1, C_BRIGHT_CYAN, "Schild aktiv.");
            settle();
            update_sight();
            frame(dump);
            return;
        }
        if (target_spell == SP_TELEPORT) {
            render_message(1, C_BRIGHT_CYAN, "Teleportiert!");
            update_sight();
            frame(dump);
            return;
        }
        if (target_spell == SP_SUBVERSION) {
            render_message(1, C_BRIGHT_YELLOW, "Die Kreatur wechselt die Seite!");
            update_sight();
            frame(dump);
            return;
        }
        if (target_spell == SP_CURSE) {
            render_message(1, C_BRIGHT_YELLOW, "Toedliche Wunde!");
            frame(dump);
            return;
        }
        if (target_spell == SP_MAGIC_ATTACK) {
            snprintf(msg, sizeof msg, "Magie trifft %u Kreaturen.",
                     shot.splash_hits);
            render_message(1, C_BRIGHT_YELLOW, msg);
            settle();
            update_sight();
            frame(dump);
            return;
        }
        if (target_spell == SP_ENCHANT) {
            render_message(1, C_BRIGHT_CYAN, "Waffen verzaubert.");
            frame(dump);
            return;
        }
        return;
    }
    if (shot.hit)
        snprintf(msg, sizeof msg, "Zauber trifft: %u Schaden.", shot.damage);
    else
        snprintf(msg, sizeof msg, "Zauber verpufft.");
    render_message(1, shot.hit ? C_BRIGHT_YELLOW : C_GREY, msg);
    if (shot.terrain_smashed)
        render_message(2, C_BRIGHT_YELLOW, "Blitz schlaegt das Terrain ein!");
    if (world.unit_count < count_before)
        render_message(2, C_BRIGHT_RED, "Mindestens eine Kreatur stirbt.");
    settle();
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
        char msg[48];
        uint8_t mover = active();
        if (game_try_enter_portal(&game, &world, mover)) {
            snprintf(msg, sizeof msg, "Gerettet! Zauberer-1: %u VP.",
                     game.vp[OWN_P1]);
            render_message(0, C_BRIGHT_MAGENTA, msg);
            settle();
        } else
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
            settle();
            update_sight();
            frame(dump);
            return;
        }
        case BUMP_TERRAIN: {
            bool destroyed;
            char msg[48];
            if (world_feature(&world, nx, ny) == FE_CHEST) {
                if (items_open_chest(&world, &turns.rng, active(), nx, ny)) {
                    render_message(1, C_BRIGHT_YELLOW, "Truhe geoeffnet!");
                    update_sight();
                } else {
                    render_message(1, C_BRIGHT_RED, "Truhe laesst sich nicht oeffnen.");
                }
                frame(dump);
                return;
            }
            {
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

    {   /* four areas over their life cycle (M4d: target < 500 ms) */
        uint8_t k2;
        Rng brng;
        area_reset();
        area_cast(&world, AREA_FIRE, 4, OWN_P1, 5, 20);
        area_cast(&world, AREA_BLOB, 4, OWN_P2, 13, 20);
        area_cast(&world, AREA_VINE, 4, OWN_NEUTRAL, 25, 20);
        area_cast(&world, AREA_FLOOD, 4, OWN_P2, 30, 20);
        rng_seed(&brng, 4);
        t0 = getsysvar_time();
        for (k2 = 0; k2 < 20; k2++)
            area_round_end(&world, &brng);
        snprintf(buf, sizeof buf, "BENCH area tick x20: %lu ms",
                 (unsigned long)((getsysvar_time() - t0) * 10));
        log_line(buf);
        area_reset();
    }

    snprintf(buf, sizeof buf, "BENCH full %u fields: %lu ms/frame",
             fields, (unsigned long)(full_cs * 10 / n));
    log_line(buf);
    render_message(0, C_BRIGHT_YELLOW, buf);
    snprintf(buf, sizeof buf, "BENCH candles only: %lu ms/frame",
             (unsigned long)(part_cs * 10 / n));
    log_line(buf);
    render_message(1, C_BRIGHT_YELLOW, buf);
}

/* ---------- main menu (GDD 2.3, M4f) ---------- */

static const char *const MENU_ITEMS[] = {
    "The Many Coloured Land (St. 1)",
    "Slayer's Dungeon (St. 2)",
    "Ragaril's Domain (St. 3)",
    "Zauberer entwerfen",
    "Zauberer zuruecksetzen",
    "Spiel beenden",
};
#define MENU_COUNT 6
#define MENU_SCENARIOS 3
/* map + book set per scenario (GDD 9.1); the .scn carries the books */
static const char *menu_scenario_map(uint8_t pick)
{
    static const char *const MAPS[MENU_SCENARIOS] = {
        "maps/many_coloured_land.map",
        "maps/slayers_dungeon.map",
        "maps/ragarils_domain.map",
    };
    static const char *const SCNS[MENU_SCENARIOS] = {
        "scenarios/many_coloured_land.scn",
        "scenarios/slayers_dungeon.scn",
        "scenarios/ragarils_domain.scn",
    };
    scnfile_load(books, SCNS[pick]);
    return MAPS[pick];
}

static void draw_menu(uint8_t cursor)
{
    uint8_t i;
    render_menu_clear();
    render_menu_text(2, 2, C_BRIGHT_YELLOW, "LORDS OF CHAOS");
    for (i = 0; i < MENU_COUNT; i++) {
        render_menu_text(4, (uint8_t)(5 + i), C_BRIGHT_WHITE,
                         i == cursor ? ">" : " ");
        render_menu_text(6, (uint8_t)(5 + i), C_BRIGHT_WHITE, MENU_ITEMS[i]);
    }
    render_menu_text(2, 22, C_GREY, "Pfeile + Enter");
}

/* One designer screen: raise attributes with +/-, Esc leaves (the XP
 * total lives in the header). */
static void designer_loop(uint8_t slot)
{
    struct keyboard_event_t e;
    static const char *const ATTRS[WA_COUNT] = {
        "Kampf", "Verteidigung", "Magieresistenz", "Konstitution", "Ausdauer"};
    Wizard *w = &wizard_slots[slot];
    uint8_t cursor = 0;
    bool running = true;
    char buf[40];
    while (running) {
        render_menu_clear();
        snprintf(buf, sizeof buf, "%s   Stufe %u   XP %u", w->name, w->level,
                 w->xp);
        render_menu_text(2, 1, C_BRIGHT_YELLOW, buf);
        {
            uint8_t i;
            for (i = 0; i < WA_COUNT; i++) {
                snprintf(buf, sizeof buf, "%c %-14.14s %3u (max %u)",
                         i == cursor ? '>' : ' ', ATTRS[i],
                         wizard_attr(w, (WizardAttr)i),
                         wizard_attr_max((WizardAttr)i));
                render_menu_text(3, (uint8_t)(4 + i), C_BRIGHT_WHITE, buf);
            }
        }
        render_menu_text(3, 12, C_GREY, "Hoch/Runter waehlen, +/- erhoehen,");
        render_menu_text(3, 13, C_GREY, "Esc zurueck ins Menue.");
        while (!kbuf_poll_event(&e))
            ;
        if (!e.isdown)
            continue;
        if (e.vkey == VK_ESC) {
            running = false;
        } else if (e.vkey == VK_UP) {
            cursor = cursor ? (uint8_t)(cursor - 1) : WA_COUNT - 1;
        } else if (e.vkey == VK_DOWN) {
            cursor = (uint8_t)((cursor + 1) % WA_COUNT);
        } else if (e.ascii == '+') {
            wizard_raise(w, (WizardAttr)cursor);
        } else if (e.ascii == '-') {
            wizard_lower(w, (WizardAttr)cursor);
        }
    }
}

/* The menu: returns the chosen map path or NULL to quit. Slot 0 is the
 * player wizard (loaded from SD, else stock). */
static const char *menu_loop(bool *free_round1)
{
    struct keyboard_event_t e;
    static const char *const SCENARIOS[] = {
        "maps/many_coloured_land.map",
    };
    uint8_t cursor = 0;
    bool running = true;
    bool confirm_reset = false;
    if (!wizards_load()) {
        uint8_t i;
        for (i = 0; i < WIZARD_SLOTS; i++)
            wizard_slot_reset(i);
    }
    while (running) {
        draw_menu(cursor);
        while (!kbuf_poll_event(&e))
            ;
        if (!e.isdown)
            continue;
        if (e.vkey != VK_SPACE && e.ascii != 13)
            confirm_reset = false;       /* any other key cancels the ask */
        if (e.vkey == VK_UP) {
            cursor = cursor ? (uint8_t)(cursor - 1) : MENU_COUNT - 1;
        } else if (e.vkey == VK_DOWN) {
            cursor = (uint8_t)((cursor + 1) % MENU_COUNT);
        } else if (e.ascii == 13 || e.vkey == VK_SPACE) {
            if (cursor < MENU_SCENARIOS) {
                const char *map = menu_scenario_map(cursor);
                wizard_apply_to_world(&wizard_slots[0], &world, active());
                memcpy(&books[OWN_P1], wizard_book(&wizard_slots[0]),
                       sizeof(Spellbook));
                wizards_save();
                *free_round1 = false;
                return map;
            }
            switch (cursor) {
            case 3:
                designer_loop(0);
                break;
            case 4:
                if (!confirm_reset) {   /* destructive: ask once */
                    confirm_reset = true;
                    render_menu_text(2, 20, C_BRIGHT_RED,
                                     "Nochmal Enter: Zauberer geht verloren.");
                    continue;
                }
                confirm_reset = false;
                wizard_slot_reset(0);   /* stock wizard over the slot */
                wizards_save();
                break;
            default:
                return NULL;
            }
        }
    }
    return NULL;
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
    for (i = 1; i < (uint8_t)argc; i++) {
        if (strcmp(argv[i], "--dump") == 0)
            dump = true;
        else if (strcmp(argv[i], "--bench") == 0)
            do_bench = true;
        else if (strcmp(argv[i], "--house") == 0)
            map_path = MAP_HOUSE;
        else if (strcmp(argv[i], "--testland") == 0)
            map_path = MAP_TESTLAND;
        else if (strcmp(argv[i], "--free-round1") == 0)
            free_round1 = true;
        else if (strcmp(argv[i], "--fly") == 0)
            do_fly = true;
    }

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
    {   /* spellbooks from the scenario file (M4a); test maps fall back
         * to an empty book */
        bool ok = scnfile_load(books, "scenarios/many_coloured_land.scn");
        log_line(ok ? "SCN loaded" : "SCN missing - empty books");
    }
    brew_register_map_cauldrons(&world);
    turn_init(&turns, &world, TURN_SEED, 1u << OWN_P1);
    {
        static AiCtx ai_ctx;              /* books + game for the wizard AI */
        ai_ctx.books = books;
        ai_ctx.game = &game;
        turns.ai = ai_wizard_phase;
        turns.ai_ctx = &ai_ctx;
        turns.on_round = on_round;
        turns.round_ctx = &game;
    }
    game_init(&game, world.portal_x, world.portal_y, world.portal_rmin,
              world.portal_rmax, &turns.rng);   /* portal from the map (v3) */
    game_new_round(&game, turns.round);
    view_set_portal(game.portal_open ? game.portal_x : -1, game.portal_y);
    if (free_round1)
        turns.round1_lock = false;
    if (do_fly) {                    /* the ISO key is not sendable yet (#3) */
        uint8_t k;
        for (k = 0; k < world.unit_count; k++)
            if (world.units[k].owner == OWN_P1 && world.units[k].ap_fly)
                world.units[k].flags |= UF_FLYING;
    }
    sight_init(&p1_sight, OWN_P1);
    update_sight();
    view_set_sight(&p1_sight);
    if (!render_init()) {
        log_line("ERR render_init");
        log_close();
        return 1;
    }
    kbuf_init(16);
    if (!dump && !do_bench) {            /* main menu (GDD 2.3, M4f) */
        const char *chosen = menu_loop(&free_round1);
        if (!chosen) {
            kbuf_deinit();
            render_shutdown();
            log_close();
            return 0;
        }
        if (chosen != map_path) {        /* reload for the chosen scenario */
            if (!mapfile_load(&world, chosen)) {
                kbuf_deinit();
                render_shutdown();
                log_close();
                return 1;
            }
            map_path = chosen;
            /* everything derived from the map must be rebuilt */
            brew_register_map_cauldrons(&world);
            turn_init(&turns, &world, TURN_SEED, 1u << OWN_P1);
            game_init(&game, world.portal_x, world.portal_y, world.portal_rmin,
                      world.portal_rmax, &turns.rng);
            game_new_round(&game, turns.round);
            view_set_portal(game.portal_open ? game.portal_x : -1, game.portal_y);
            if (free_round1)
                turns.round1_lock = false;
            {
                static AiCtx ai_ctx2;
                ai_ctx2.books = books;
                ai_ctx2.game = &game;
                turns.ai = ai_wizard_phase;
                turns.ai_ctx = &ai_ctx2;
                turns.on_round = on_round;
                turns.round_ctx = &game;
            }
            sight_init(&p1_sight, OWN_P1);
            update_sight();
            view_set_sight(&p1_sight);
        }
        /* the designer wizard becomes unit 0 (F5: no items, own book) */
        wizard_apply_to_world(&wizard_slots[0], &world, active());
        memcpy(&books[OWN_P1], wizard_book(&wizard_slots[0]), sizeof(Spellbook));
    }
    frame(dump);
    render_message(1, C_BRIGHT_YELLOW, "Willkommen in Testland.");
    render_message(2, C_BRIGHT_BLUE, "Tab Einheit  Leertaste fertig  E Zugende");
    if (do_bench)
        bench();

    chord_init(&chord, WINDOW_CS, DELAY_CS, REPEAT_CS);
    next_anim = getsysvar_time() + ANIM_CS;
    next_blink = getsysvar_time() + BLINK_CS;
    while (running) {
        /* Drain every queued key event first: drawing a step can take longer
         * than the repeat delay, and a stale "held" state would otherwise
         * repeat keys that were already released (ADR 0007). */
        while (running && kbuf_poll_event(&e)) {
            now = (uint16_t)getsysvar_time();
            if (game_ended && !(e.isdown && e.vkey == VK_ESC))
                continue;                        /* game over: Esc only */
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
                        } else if (SPELLS[i].category == SPC_POTION) {
                            if (brew_cast(&world, &books[OWN_P1], wiz, (uint8_t)i))
                                render_message(1, C_BRIGHT_GREEN,
                                               "Der Kessel brodelt.");
                            else
                                render_message(1, C_BRIGHT_RED,
                                               "Brauen braucht Kessel und Zutat.");
                        } else if (SPELLS[i].category == SPC_SUMMON) {
                            uint8_t got = spell_summon(&world, &books[OWN_P1], wiz, (uint8_t)i);
                            if (got)
                                render_message(1, C_BRIGHT_GREEN, "Beschworen!");
                            else
                                render_message(1, C_BRIGHT_RED, "Kein Platz - Mana verloren.");
                            update_sight();
                        } else {
                            /* everything else aims through the cursor */
                            targeting = true;
                            target_kind = TA_SPELL;
                            target_spell = (uint8_t)i;
                            target_x = world.units[wiz].x;
                            target_y = world.units[wiz].y;
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
                } else if (e.ascii == 13 || e.vkey == VK_SPACE) {
                    /* aiming at the caster cancels without cost (GDD 7.1)
                     * - except for spells that target the caster himself */
                    if (target_kind == TA_SPELL &&
                        target_spell != SP_MAGIC_SHIELD &&
                        target_spell != SP_ENCHANT &&
                        target_x == world.units[active()].x &&
                        target_y == world.units[active()].y) {
                        targeting = false;
                        log_line("SELF cancel");
                        render_message(1, C_GREY, "Abgebrochen.");
                        render_message(2, C_GREY, "");
                        frame(dump);
                    } else {
                        targeting = false;
                        if (target_kind == TA_SPELL)
                            cast_targeted(dump);
                        else
                            throw_or_fire(dump);
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
            } else if (e.ascii == 'g') {            /* pick up */
                confirm_end = false;
                if (items_pick_up(&world, active()))
                    render_message(1, C_BRIGHT_GREEN, "Aufgehoben.");
                else
                    render_message(1, C_BRIGHT_RED, "Nichts aufzuheben.");
                frame(dump);
            } else if (e.ascii == 'd') {            /* drop in use */
                confirm_end = false;
                if (items_drop(&world, active()))
                    render_message(1, C_BRIGHT_GREEN, "Fallen gelassen.");
                else
                    render_message(1, C_BRIGHT_RED, "Kein Objekt in der Hand.");
                update_sight();
                frame(dump);
            } else if (e.ascii == 'e') {            /* eat in-use food */
                confirm_end = false;
                if (items_eat(&world, active()))
                    render_message(1, C_BRIGHT_GREEN, "Gegessen.");
                else
                    render_message(1, C_BRIGHT_RED, "Kein Essen in der Hand.");
                frame(dump);
            } else if (e.ascii == 'r') {            /* read scroll in use */
                confirm_end = false;
                {
                    const char *txt = items_read(&world, active());
                    if (txt)
                        render_message(1, C_BRIGHT_CYAN, txt);
                    else
                        render_message(1, C_BRIGHT_RED, "Keine Schriftrolle in der Hand.");
                }
                frame(dump);
            } else if (e.ascii == 'b') {            /* board / dismount */
                confirm_end = false;
                if (world.units[active()].flags & UF_RIDDEN) {
                    if (ride_dismount(&world, active()))
                        render_message(1, C_BRIGHT_GREEN, "Abgestiegen.");
                    else
                        render_message(1, C_BRIGHT_RED, "Kein Platz zum Absteigen.");
                } else {
                    int16_t mx = world.units[active()].x;
                    int16_t my = (int16_t)(world.units[active()].y - 1);
                    if (ride_mount(&world, active(), mx, my) ||
                        ride_mount(&world, active(),
                                   (int16_t)(mx - 1), my) ||
                        ride_mount(&world, active(),
                                   (int16_t)(mx + 1), (int16_t)(my + 1))) {
                        turn_revalidate(&turns, &world);
                        render_message(1, C_BRIGHT_GREEN, "Aufgesessen!");
                    } else
                        render_message(1, C_BRIGHT_RED,
                                       "Kein Reittier in Reichweite.");
                }
                update_sight();
                frame(dump);
            } else if (e.ascii == 'q') {            /* quaff: vial or cauldron */
                confirm_end = false;
                if (brew_drink_vial(&world, active()))
                    render_message(1, C_BRIGHT_GREEN, "Phiole getrunken.");
                else if (brew_drink(&world, active()))
                    render_message(1, C_BRIGHT_GREEN, "Aus dem Kessel getrunken.");
                else
                    render_message(1, C_BRIGHT_RED, "Nichts zu trinken hier.");
                frame(dump);
            } else if (e.ascii == 'v') {            /* fill the empty vial */
                confirm_end = false;
                if (brew_fill(&world, active()))
                    render_message(1, C_BRIGHT_GREEN, "Phiole gefuellt.");
                else
                    render_message(1, C_BRIGHT_RED, "Kein Kessel oder keine leere Phiole.");
                frame(dump);
            } else if (e.ascii == 'w') {            /* wield next */
                confirm_end = false;
                if (items_cycle(&world, active())) {
                    const Unit *u = &world.units[active()];
                    if (u->in_use != NO_ITEM)
                        render_message(1, C_BRIGHT_GREEN,
                                       OBJECTS[u->items[u->in_use]].name);
                    else
                        render_message(1, C_GREY, "Leere Haende.");
                } else
                    render_message(1, C_BRIGHT_RED, "Zu wenig AP.");
                frame(dump);
            } else if (e.ascii == 't') {            /* throw in use */
                confirm_end = false;
                if (world.units[active()].in_use == NO_ITEM)
                    render_message(1, C_BRIGHT_RED, "Kein Objekt in der Hand.");
                else {
                    targeting = true;
                    target_kind = TA_THROW;
                    target_spell = 0;
                    target_x = world.units[active()].x;
                    target_y = world.units[active()].y;
                }
                frame(dump);
            } else if (e.ascii == 'f') {            /* fire bow in use */
                confirm_end = false;
                {
                    const Unit *u = &world.units[active()];
                    uint8_t weapon = u->in_use != NO_ITEM &&
                                     u->in_use < u->item_count
                                         ? OBJECTS[u->items[u->in_use]].weapon
                                         : WEAPON_NONE;
                    if (weapon == WEAPON_NONE || WEAPONS[weapon].ranged == 0)
                        render_message(1, C_BRIGHT_RED, "Kein Bogen in der Hand.");
                    else {
                        targeting = true;
                        target_kind = TA_FIRE;
                        target_x = world.units[active()].x;
                        target_y = world.units[active()].y;
                    }
                }
                frame(dump);
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
                    if (!turn_humans_present(&turns, &world))
                        render_message(1, C_BRIGHT_YELLOW, "Die KI spielt zu Ende ...");
                    turn_end_phase(&turns, &world);    /* round hook: portal */
                    game_credit_kills(&game, &world);
                    view_set_portal(game.portal_open ? game.portal_x : -1,
                                    game.portal_y);
                    if (game_over(&game, &world) ||
                        !turn_humans_present(&turns, &world)) {
                        char msg[48];
                        snprintf(msg, sizeof msg,
                                 "Spielende! Zauberer-1: %u VP  Zauberer-2: %u VP",
                                 game.vp[OWN_P1], game.vp[OWN_P2]);
                        render_message(0, C_BRIGHT_YELLOW, msg);
                        render_message(1, C_GREY, "Esc beendet.");
                        game_ended = true;
                    } else if (game.portal_open && turns.round == game.portal_round)
                        render_message(0, C_BRIGHT_MAGENTA, "Das Portal oeffnet sich!");
                    else
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
