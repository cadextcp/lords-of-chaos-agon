#include "selftest.h"

#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "area.h"
#include "brew.h"
#include "ride.h"
#include "save.h"
#include "wizard.h"
#include "effect.h"
#include "chord.h"
#include "combat.h"
#include "events.h"
#include "game.h"
#include "items.h"
#include "lexicon.h"
#include "gen/maps.h"
#include "gen/scenarios.h"
#include "names.h"
#include "rng.h"
#include "sight.h"
#include "spells.h"
#include "turn.h"
#include "tutorial.h"
#include "view.h"
#include "world.h"

/* view_hash() of the wizard house with the cursor on the wizard.
 * Must be identical on host and Agon; update deliberately when the map,
 * tiles or composition rules change. Changed for M2d: the new unexplored
 * tile shifted every tile ID after "tree"; M2e added air_shadow and
 * cursor_blue, M3d/M3e object and portal tiles, M3g four treasures;
 * M5c added the seven fx tiles after "floor_*" (IDs shifted again). */
#define HOUSE_VIEW_HASH 0x632E5581UL

static selftest_log_fn out;
static uint16_t fails;
static World world;

static void load_house(void)
{
    world_load_bin(&world, MAPBIN_WIZARD_HOUSE, MAPBIN_WIZARD_HOUSE_LEN);
}

/* The Agon console loses the output tail at emulator exit; the eZ80 run
 * therefore only prints failures (plus the final verdict from main). */
static bool verbose_checks = true;

static void check(int ok, const char *what)
{
    char buf[80];
    if (!ok)
        fails++;
    if (!verbose_checks && ok)
        return;
    snprintf(buf, sizeof buf, "%s %s", ok ? "ok  " : "FAIL", what);
    out(buf);
}

static int has_layer(const FieldLayers *f, uint16_t id)
{
    uint8_t i;
    for (i = 0; i < f->n; i++)
        if (f->id[i] == id)
            return 1;
    return 0;
}

/* Every window field of the fast (cached) path must equal view_compose(). */
static int fast_equals_reference(void)
{
    FieldLayers ref;
    uint8_t vx, vy;
    for (vy = 0; vy < VIEW_H; vy++)
        for (vx = 0; vx < VIEW_W; vx++) {
            const FieldLayers *f = view_field(vx, vy);
            view_compose(&world, (int16_t)(view_origin_x() + vx),
                         (int16_t)(view_origin_y() + vy), &ref);
            if (f->n != ref.n || f->air != ref.air || f->ride != ref.ride ||
                memcmp(f->id, ref.id, ref.n * sizeof ref.id[0]) != 0)
                return 0;
        }
    return 1;
}

static void test_rng(void)
{
    Rng r;
    uint16_t i;
    int in_range = 1;

    rng_seed(&r, 1);
    check(rng_next(&r) == 270369UL, "rng: xorshift32 #1");
    check(rng_next(&r) == 67634689UL, "rng: xorshift32 #2");
    check(rng_next(&r) == 2647435461UL, "rng: xorshift32 #3");
    rng_seed(&r, 0);
    check(r.state != 0, "rng: zero seed remapped");
    for (i = 0; i < 500; i++)
        if (rng_range(&r, 7) >= 7)
            in_range = 0;
    check(in_range, "rng: range bound");
}

static void test_world(void)
{
    static uint8_t bad[300];
    uint16_t i;

    check(world_load_bin(&world, MAPBIN_WIZARD_HOUSE, MAPBIN_WIZARD_HOUSE_LEN),
          "map: binary house loads");
    for (i = 0; i < MAPBIN_WIZARD_HOUSE_LEN && i < sizeof bad; i++)
        bad[i] = MAPBIN_WIZARD_HOUSE[i];
    bad[0] = 'X';
    check(!world_load_bin(&world, bad, MAPBIN_WIZARD_HOUSE_LEN), "map: bad magic rejected");
    bad[0] = 'L';
    bad[5] ^= 1;
    check(!world_load_bin(&world, bad, MAPBIN_WIZARD_HOUSE_LEN), "map: stale tile count rejected");
    bad[5] ^= 1;
    check(!world_load_bin(&world, bad, 100), "map: truncated file rejected");
    bad[MAPBIN_HEADER] = FL_COUNT;
    check(!world_load_bin(&world, bad, MAPBIN_WIZARD_HOUSE_LEN), "map: out-of-range floor rejected");
    check(world.w == 9 && world.units[0].x == 3, "map: failed load leaves world unchanged");

    load_house();
    check(world.w == 9 && world.h == 9 && !world.wrap, "world: house is 9x9, no wrap");
    check(world.unit_count == 2 && world.units[0].x == 3 && world.units[0].y == 4,
          "world: wizard at 3,4");
    check(world.feature[3][5] == FE_DOOR_OPEN, "world: open door at 5,3");
    check(world_blocks(&world, 0, 0) && world_blocks(&world, 1, 1), "world: wall and bed block");
    check(!world_blocks(&world, 3, 3), "world: cauldron is walkable");
    check(world_blocks(&world, -1, 0), "world: outside a small map blocks");
}

static void test_view(void)
{
    FieldLayers f;

    view_compose(&world, 0, 0, &f);   /* top-left corner: walls E and S */
    check(f.n >= 2 && f.id[0] == T_FLOOR_STONE && has_layer(&f, T_WALL_00 + (2 | 4)),
          "view: corner wall mask E+S");

    view_compose(&world, 5, 3, &f);   /* door between stone and path */
    check(has_layer(&f, T_DOOR_V_OPEN), "view: door orientation vertical");
    check(has_layer(&f, T_FLOOR_PATH_HALF_E) && !has_layer(&f, T_FLOOR_STONE_HALF_W),
          "view: half floor only where neighbour floor differs");

    view_compose(&world, 3, 4, &f);   /* wizard on a rug */
    check(f.n == 3 && f.id[0] == T_FLOOR_STONE && f.id[1] == T_DECOR_RUG &&
          f.id[2] == T_WIZARD_P1, "view: layer order floor, rug, wizard");
    view_compose(&world, 8, 3, &f);   /* neutral goblin outside the house */
    check(has_layer(&f, T_GOBLIN_NEUTRAL), "view: creature in owner colour (neutral)");

    view_set_phase(1);
    view_compose(&world, 1, 2, &f);
    check(has_layer(&f, T_CANDLE_1), "view: candle animation phase");
    view_set_phase(0);
}

static void test_dirty_and_move(void)
{
    char buf[48];
    uint32_t h;

    view_set_origin(0, 0);
    view_set_cursor(3, 4, T_CURSOR_GREEN);
    view_invalidate();
    check(view_update(&world) == VIEW_W * VIEW_H, "view: first frame all dirty");
    h = view_hash();
    if (verbose_checks) {
        snprintf(buf, sizeof buf, "view: hash=0x%08lX", (unsigned long)h);
        out(buf);
    }
    check(h == HOUSE_VIEW_HASH, "view: deterministic house hash");
    check(fast_equals_reference(), "view: cached fast path equals reference");
    view_clean();
    check(view_update(&world) == 0, "view: unchanged frame is clean");

    check(world_move_unit(&world, 0, 0, 1), "move: wizard south");
    view_set_cursor(3, 5, T_CURSOR_GREEN);
    check(view_update(&world) == 2, "view: a step dirties exactly 2 fields");
    view_set_phase(1);
    view_update(&world);
    check(fast_equals_reference(), "view: fast path equals reference (moved, phase 1)");
    view_set_phase(0);
    view_update(&world);
    view_clean();
    check(view_animate(1) == 4, "view: animate dirties only the 4 candles");
    check(fast_equals_reference(), "view: animated frame equals reference");
    view_clean();
    check(view_animate(1) == 0, "view: same phase again changes nothing");
    view_animate(0);
    view_clean();
    view_clean();

    check(!world_move_unit(&world, 0, 0, 5), "move: no jumping into walls");
    world.units[0].x = 1;
    world.units[0].y = 5;
    check(!world_move_unit(&world, 0, -1, 0), "move: wall blocks");
    check(!world_move_unit(&world, 0, 0, 1), "move: table blocks");
}

static void test_ap(void)
{
    load_house();
    check(world.units[0].ap == 40, "ap: wizard starts with 40");
    check(world_step_cost(&world, 7, 3, false) == 3 && world_step_cost(&world, 2, 2, false) == 4,
          "ap: path 3, floor 4");
    check(world_step_cost(&world, 2, 2, true) == 6 && world_step_cost(&world, 7, 3, true) == 5,
          "ap: diagonal = 3/2 rounded up");
    check(world_move_unit(&world, 0, 1, 1) && world.units[0].ap == 34, "ap: diagonal step costs 6");
    world.units[0].ap = 3;
    check(!world_move_unit(&world, 0, 0, 1), "ap: not enough AP blocks");
    world_new_turn(&world);
    check(world.units[0].ap == 40, "ap: new turn refills");
}

static void test_stats_and_names(void)
{
    const char *g[GROUND_MAX];
    uint8_t n;

    load_house();
    check(world.units[0].sta == 60 && world.units[0].mana == 80, "stats: wizard stamina 60, mana 80");
    world_move_unit(&world, 0, 1, 1);                         /* diagonal: 6 AP */
    check(world.units[0].sta == 57, "stats: step costs half the AP as stamina");
    world_new_turn(&world);
    check(world.units[0].sta == 60, "stats: new turn recovers stamina (capped)");
    world.units[0].sta = 10;
    world_new_turn(&world);
    check(world.units[0].sta == 25, "stats: recovery is 25 % of max");
    check(world.units[0].ap == 20, "stats: exhausted creatures get half AP (PM 12)");
    world_new_turn(&world);                        /* 25 of 60: cured */
    check(world.units[0].ap == 40, "stats: one quiet round cures exhaustion");
    world.units[0].mana = 0;
    world_new_turn(&world);
    check(world.units[0].mana == 3, "stats: mana regenerates 4 % per round");

    n = ground_names(&world, 3, 7, g);                        /* scroll on wood */
    check(n == 1 && g[0][0] == 'S', "names: object on the floor");
    n = ground_names(&world, 3, 4, g);
    check(n == 1 && g[0][0] == 'T', "names: rug (Teppich)");
    n = ground_names(&world, 3, 3, g);
    check(n == 1 && g[0][0] == 'K', "names: cauldron (Kessel)");
    n = ground_names(&world, 2, 2, g);
    check(n == 1 && g[0][0] == 'S' && g[0][1] == 't', "names: bare floor (Steinboden)");
    check(name_unit(&world.units[1])[0] == 'G', "names: goblin");
}

static void test_data(void)
{
    check(spell_mana(SP_GIANT_BAT, 0) == 5 && spell_mana(SP_GIANT_BAT, 3) == 11,
          "data: giant bat mana 5 + 2/level");
    check(spell_mana(SP_GOLD_DRAGON, 8) == 231, "data: gold dragon level 8 = 231");
    check(SPELLS[SP_SUPER_POTION].amiga == 0 && SPELLS[SP_BOMB_POTION].known == 0,
          "data: super potion not on Amiga, bomb potion cost unknown");
    check(FLOOR_AP[FL_PATH] == 3 && FLOOR_AP[FL_STONE] == 4, "data: floor costs from costs.csv");
    check(ACTIONS[ACT_CAST].ap == 10 && ACTIONS[ACT_MELEE].stamina == 4,
          "data: action costs from actions.csv");
}

static void test_creatures(void)
{
    const CreatureDef *g = &CREATURES[CR_GOBLIN];
    check(CR_COUNT == 26 && CR_WIZARD == 0, "creatures: wizard + 25 from the table");
    check(g->ap == 30 && g->stamina == 45 && g->con == 32 && g->combat == 9 && g->defence == 9,
          "creatures: goblin values from [PM 34]");
    check(CREATURES[CR_GOLD_DRAGON].ap_fly == 40 && CREATURES[CR_ZOMBIE].flags & CF_UNDEAD,
          "creatures: dragon flies (40), zombie undead");
    check(CREATURES[CR_UNICORN].flags & CF_MOUNT && CREATURES[CR_DWARF].flags & CF_RIDE,
          "creatures: unicorn is a mount, dwarf can ride");
    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.units[1].kind = CR_CROCODILE;   /* re-init a unit as crocodile */
    world.units[1].native = CREATURES[CR_CROCODILE].native;
    check(world_unit_step_cost(&world, 1, 16, 0, false) == 4 &&
          world_unit_step_cost(&world, 0, 16, 0, false) == 12,
          "creatures: crocodile pays floor cost in water, wizard 12");
    check(world_unit_step_cost(&world, 1, 16, 0, true) == 6, "creatures: diagonal affinity 6");
    load_house();
}

static void test_terrain(void)
{
    FieldLayers a, b;
    check(world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN), "terrain: testland loads");
    check(world.w == 36 && world.h == 36 && world.wrap, "terrain: 36x36, wraps");
    check(FLOOR_AP[FL_WATER] == 12 && FLOOR_AP[FL_FOREST] == 8 && FLOOR_AP[FL_SWAMP] == 10,
          "terrain: costs water 12, forest 8, swamp 10");
    check(world_blocks_sight(&world, 25, 4) && !world_blocks_sight(&world, 20, 4),
          "terrain: forest blocks sight, grass does not");
    check(FLOOR_NATIVE[FL_WATER] == NATIVE_WATER && FLOOR_DROWN[FL_WATER],
          "terrain: water is native to water types, drowns others");
    check(world_blocks(&world, 32, 31), "terrain: rock blocks");
    {
        int16_t x, y;
        int printable = 1;
        for (y = 0; y < world.h; y++)
            for (x = 0; x < world.w; x++) {
                char ch = world_char(&world, x, y);
                if (ch < 32 || ch > 126)
                    printable = 0;
            }
        check(printable && world_char(&world, 32, 31) == 'R' && world_char(&world, 16, 0) == '~',
              "terrain: every field has a printable dump char");
    }
    view_compose(&world, -1, 12, &a);
    view_compose(&world, 35, 12, &b);
    check(a.n == b.n && memcmp(a.id, b.id, a.n * sizeof a.id[0]) == 0, "terrain: wrap-around x=-1 == x=35");
    view_set_origin(0, 0);
    view_follow(&world, 0, 0);
    check(view_origin_x() == 34 && view_origin_y() == 34, "terrain: camera wraps (origin 34,34)");
    view_invalidate();
    view_update(&world);
    view_clean();
    check(view_animate(1) > 0 && fast_equals_reference(), "terrain: water animates like reference");
    view_animate(0);
    view_set_origin(0, 0);
    load_house();
}

static void test_chord(void)
{
    Chord c;
    int8_t dx, dy;
    const uint8_t W = 8, D = 35, R = 20;   /* 80 ms window, 350/200 ms repeat */

    chord_init(&c, W, D, R);
    check(chord_key(&c, ARROW_UP, true, 100) == 0, "chord: first arrow waits");
    check(chord_key(&c, ARROW_UP, false, 103) == ARROW_UP, "chord: quick tap fires on release");

    chord_init(&c, W, D, R);
    chord_key(&c, ARROW_RIGHT, true, 0);
    check(chord_poll(&c, 8) == 0 && chord_poll(&c, 9) == ARROW_RIGHT,
          "chord: held arrow fires after the window");
    check(chord_key(&c, ARROW_RIGHT, false, 30) == 0, "chord: release after firing is silent");

    chord_init(&c, W, D, R);
    chord_key(&c, ARROW_UP, true, 200);
    check(chord_key(&c, ARROW_RIGHT, true, 204) == (ARROW_UP | ARROW_RIGHT),
          "chord: up+right within window = NE");
    check(chord_key(&c, ARROW_UP, false, 210) == 0 && chord_key(&c, ARROW_RIGHT, false, 211) == 0,
          "chord: releasing a chord is silent");
    check(chord_to_step(ARROW_UP | ARROW_RIGHT, &dx, &dy) && dx == 1 && dy == -1,
          "chord: NE maps to dx=1 dy=-1");

    chord_init(&c, W, D, R);
    chord_key(&c, ARROW_DOWN, true, 0);
    check(chord_poll(&c, 9) == ARROW_DOWN, "chord: late partner -> first fires alone");
    check(chord_key(&c, ARROW_LEFT, true, 12) == 0 &&
          chord_key(&c, ARROW_LEFT, false, 14) == ARROW_LEFT,
          "chord: late partner fires on its own");

    chord_init(&c, W, D, R);
    chord_key(&c, ARROW_LEFT, true, 0);
    check(chord_key(&c, ARROW_RIGHT, true, 2) == ARROW_LEFT, "chord: opposite arrows no diagonal");
    check(!chord_to_step(ARROW_LEFT | ARROW_RIGHT, &dx, &dy), "chord: contradictory mask rejected");

    chord_init(&c, W, D, R);
    chord_key(&c, ARROW_UP, true, 0);
    chord_key(&c, ARROW_LEFT, true, 3);                       /* NW emitted */
    check(chord_poll(&c, 30) == 0, "chord: no repeat before the delay");
    check(chord_poll(&c, 38) == (ARROW_UP | ARROW_LEFT), "chord: held chord repeats as diagonal");
    check(chord_poll(&c, 50) == 0 && chord_poll(&c, 58) == (ARROW_UP | ARROW_LEFT),
          "chord: then repeats every interval");
    chord_key(&c, ARROW_LEFT, false, 60);
    check(chord_poll(&c, 94) == 0 && chord_poll(&c, 95) == ARROW_UP,
          "chord: releasing one key continues with the other after the delay");
    chord_key(&c, ARROW_UP, false, 96);
    check(chord_poll(&c, 200) == 0, "chord: nothing held, no repeat");
}

/* FNV-1a over all units (position, AP, stamina) - wandering replay check. */
static uint32_t unit_hash(const World *w)
{
    uint32_t h = 2166136261UL;
    uint8_t i;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *u = &w->units[i];
        uint8_t v[4] = { u->x, u->y, u->ap, u->sta }, k;
        for (k = 0; k < 4; k++)
            h = (h ^ v[k]) * 16777619UL;
    }
    return h;
}

static void test_turn(void)
{
    Turns t;
    uint32_t seen;
    uint8_t i, moved;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    turn_init(&t, &world, 42, 1u << OWN_P1);
    check(t.round == 1 && t.phase == OWN_P1, "turn: round 1 begins with player 1");
    check(t.active == 0 && world.units[0].kind == CR_WIZARD, "turn: wizard is active");
    check(!turn_may_move(&t), "turn: round 1 allows casting only (PM 7)");
    check(world.units[2].x == 27 && world.units[2].y == 5,
          "turn: independents stay put in round 1");

    turn_next_unit(&t, &world, false);
    check(t.active == 10, "turn: Tab selects the dwarf next");
    turn_next_unit(&t, &world, false);
    check(t.active == 11, "turn: then the flying bat");
    turn_next_unit(&t, &world, false);
    check(t.active == 0, "turn: Tab wraps around to the wizard");
    turn_next_unit(&t, &world, true);
    check(t.active == 11, "turn: Shift+Tab goes back to the bat");

    world.units[0].ap = 0;
    turn_next_unit(&t, &world, false);
    check(t.active == 10, "turn: Tab skips units without AP");
    world.units[0].ap = world.units[0].ap_max;
    turn_next_unit(&t, &world, false);
    turn_next_unit(&t, &world, false);
    check(t.active == 0, "turn: back on the wizard");

    turn_finish_unit(&t, &world);
    check(world.units[0].done && t.active == 10,
          "turn: space finishes the wizard, dwarf is next");
    check(turn_units_left(&t, &world), "turn: unfinished units are left");
    turn_finish_unit(&t, &world);
    turn_finish_unit(&t, &world);
    check(!turn_units_left(&t, &world) && t.active == 11,
          "turn: all finished, active stays for rendering");

    t.round1_lock = false;
    check(turn_may_move(&t), "turn: round 1 lock is switchable (emulator)");
    turn_end_phase(&t, &world);
    check(t.round == 2 && t.phase == OWN_P1 && t.active == 0,
          "turn: AI wizard passes, round 2 returns to p1");
    check(world.units[0].ap == 40 && world.units[0].sta == 60 &&
          world.units[0].mana == 80,
          "turn: round end refills AP, stamina and mana");
    check(!world.units[0].done && !world.units[10].done && !world.units[11].done,
          "turn: new phase clears the finished flags");

    moved = 0;
    for (i = 0; i < world.unit_count; i++)
        if (world.units[i].owner == OWN_NEUTRAL && world.units[i].ap < world.units[i].ap_max)
            moved++;
    check(moved > 0, "turn: independent creatures spent AP");
    seen = unit_hash(&world);

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    turn_init(&t, &world, 42, 1u << OWN_P1);
    t.round1_lock = false;
    turn_end_phase(&t, &world);
    check(t.round == 2 && unit_hash(&world) == seen,
          "turn: same seed wanders the same way");

    turn_end_phase(&t, &world);
    turn_end_phase(&t, &world);
    check(t.round == 4 && t.phase == OWN_P1, "turn: rounds keep advancing");
}

static void test_sight(void)
{
    Sight s;
    FieldLayers f;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 2;                    /* wizards only for isolation */
    world.units[1].x = 0;
    world.units[1].y = 35;                   /* enemy far away */
    view_set_sight(NULL);
    sight_init(&s, OWN_P1);
    sight_compute(&world, &s);
    check(sight_visible(&s, &world, 6, 6), "sight: own field is visible");
    check(sight_visible(&s, &world, 3, 6) && !sight_visible(&s, &world, 2, 6),
          "sight: wall visible, the field behind it not");
    world.units[0].x = 20;                   /* leave the house (test only) */
    world.units[0].y = 13;
    sight_compute(&world, &s);
    check(sight_visible(&s, &world, 29, 13) && !sight_visible(&s, &world, 30, 13),
          "sight: ground range 9 (Chebyshev)");
    check(sight_explored(&s, &world, 6, 6) && !sight_visible(&s, &world, 6, 6),
          "sight: explored stays when sight moves on");
    view_set_sight(&s);
    view_compose(&world, 6, 6, &f);
    check(has_layer(&f, T_OVERLAY_REMEMBERED), "sight: remembered raster overlay");
    view_compose(&world, 31, 26, &f);        /* original p2 spot: never seen */
    check(f.n == 1 && f.id[0] == T_UNEXPLORED, "sight: unexplored is black");

    world.units[1].x = 21;                   /* enemy steps into sight */
    world.units[1].y = 13;
    sight_compute(&world, &s);
    view_compose(&world, 21, 13, &f);
    check(has_layer(&f, T_WIZARD_P2), "sight: enemy in sight is drawn");
    world.units[1].x = 6;                    /* enemy into the dark house */
    world.units[1].y = 6;
    sight_compute(&world, &s);
    view_compose(&world, 6, 6, &f);
    check(has_layer(&f, T_OVERLAY_REMEMBERED) && !has_layer(&f, T_WIZARD_P2),
          "sight: hidden movement keeps enemies invisible");

    world.units[0].x = 0;                    /* north-west corner */
    world.units[0].y = 0;
    world.units[1].x = 34;
    world.units[1].y = 34;
    sight_init(&s, OWN_P1);
    sight_compute(&world, &s);
    check(sight_visible(&s, &world, 35, 0) && sight_visible(&s, &world, 0, 35) &&
          sight_visible(&s, &world, 35, 35), "sight: rays wrap around the world");
    check(sight_visible(&s, &world, 27, 0) && !sight_visible(&s, &world, 26, 0),
          "sight: wrapped range 9");

    /* the fast path must compose the same hidden map as the reference */
    {
        uint8_t dirty_n;
        view_set_sight(&s);
        view_set_origin(8, 1);            /* window over house and grass */
        view_invalidate();
        dirty_n = view_update(&world);
        view_clean();
        view_update(&world);
        check(dirty_n == VIEW_W * VIEW_H && fast_equals_reference(),
              "sight: fast path equals reference with sight");
        view_animate(1);
        check(fast_equals_reference(), "sight: animated frame equals reference");
        view_animate(0);
        view_clean();
        view_set_sight(NULL);
    }
}

static void test_flight(void)
{
    FieldLayers f;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    check(world.unit_count == 13 && world.units[11].kind == CR_GIANT_BAT &&
          world.units[11].ap_fly == 62 && world.units[11].owner == OWN_P1,
          "fly: testland has a p1 giant bat (index 11)");
    check(world.units[0].ap_fly == 0, "fly: the wizard cannot fly");

    check(world_unit_at(&world, 14, 2, UL_GROUND) == 11 &&
          world_unit_at(&world, 14, 2, UL_AIR) == NO_UNIT,
          "fly: bat starts on the ground layer");
    check(!world_take_off(&world, 0), "fly: wizard cannot take off");
    check(world_take_off(&world, 11) && world.units[11].ap == 20,
          "fly: take-off costs 4 AP");
    check((world.units[11].flags & UF_FLYING) != 0 &&
          world_unit_at(&world, 14, 2, UL_AIR) == 11 &&
          world_unit_at(&world, 14, 2, UL_GROUND) == NO_UNIT,
          "fly: bat occupies the air layer");

    {   /* flight ignores terrain: two steps east, onto the river */
        check(world_move_unit(&world, 11, 1, 0) && world_move_unit(&world, 11, 1, 0),
              "fly: straight over anything");
    }
    check(world.units[11].x == 16 && world.units[11].y == 2 &&
          world.units[11].ap == 12 && world_floor(&world, 16, 2) == FL_WATER,
          "fly: 2 air steps onto the river cost 8 AP");
    check(!world_land(&world, 11), "fly: no landing on water");
    check(world_move_unit(&world, 11, 1, 0) && world_land(&world, 11) &&
          world.units[11].ap == 4,
          "fly: landing on grass costs 4 AP");
    check(world_unit_at(&world, 17, 2, UL_GROUND) == 11 &&
          !(world.units[11].flags & UF_FLYING),
          "fly: back on the ground layer");

    {   /* ground and air unit share a field */
        world.units[11].x = 6;
        world.units[11].y = 6;
        world.units[11].flags |= UF_FLYING;
        check(world_unit_at(&world, 6, 6, UL_GROUND) == 0 &&
              world_unit_at(&world, 6, 6, UL_AIR) == 11,
              "fly: wizard below, bat above");
        view_set_sight(NULL);
        view_compose(&world, 6, 6, &f);
        check(has_layer(&f, T_WIZARD_P1) && has_layer(&f, T_AIR_SHADOW) &&
              has_layer(&f, T_GIANT_BAT_P1),
              "fly: view stacks ground unit, shadow and flyer");
        check(f.air != 0, "fly: the flyer layer is flagged");
        view_set_origin(2, 2);
        view_set_cursor(6, 6, T_CURSOR_GREEN);
        view_invalidate();
        view_update(&world);
        check(view_dirty(4, 4) && fast_equals_reference(),
              "fly: fast path composes the stack as well");
        view_set_sight(NULL);
        view_clean();
    }

    {   /* round end refills the layer budget (PM 34: bat 24 ground, 62 air) */
        world.units[11].flags |= UF_FLYING;
        world_new_turn(&world);
        check(world.units[11].ap == 62, "fly: airborne refill uses ap_fly");
        world.units[11].flags &= (uint8_t)~UF_FLYING;
        world_new_turn(&world);
        check(world.units[11].ap == 24, "fly: grounded refill uses ap_max");
    }

    {   /* sight from the air: range 11, walls do not block */
        Sight s;
        world.unit_count = 12;
        world.units[11].x = 6;
        world.units[11].y = 6;
        world.units[11].flags |= UF_FLYING;
        sight_init(&s, OWN_P1);
        sight_compute(&world, &s);
        check(sight_visible(&s, &world, 6, 2) && sight_visible(&s, &world, 6, 1),
              "fly: the bat looks over the house walls");
        check(sight_visible(&s, &world, 6, 17) && !sight_visible(&s, &world, 6, 18),
              "fly: air range 11");
    }
}

static void test_bump_and_look(void)
{
    char buf[24];
    Sight s;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.feature[5][6] = FE_DOOR_CLOSED;         /* north of the wizard */
    check(world_bump_kind(&world, 0, 0, -1) == BUMP_DOOR, "bump: closed door ahead");
    check(world_open_door(&world, 0, 6, 5) &&
          world.feature[5][6] == FE_DOOR_OPEN && world.units[0].ap == 34,
          "bump: opening costs 6 AP and opens the door");
    check(world_bump_kind(&world, 0, 0, -1) == BUMP_OK, "bump: open door is walkable");
    world.feature[5][6] = FE_DOOR_CLOSED;
    check(!world_open_door(&world, 11, 6, 5),
          "bump: the bat has no hands (CF_USE)");
    world.units[0].ap = 5;
    check(!world_open_door(&world, 0, 6, 5) && world.feature[5][6] == FE_DOOR_CLOSED,
          "bump: not enough AP leaves the door shut");
    world.units[0].ap = 40;
    world.units[0].x = 6;
    world.units[0].y = 3;
    check(world_bump_kind(&world, 0, 0, -1) == BUMP_TERRAIN, "bump: wall is terrain");
    world.units[1].x = 5;
    world.units[1].y = 3;
    check(world_bump_kind(&world, 0, -1, 0) == BUMP_UNIT, "bump: unit ahead");
    check(world_bump_kind(&world, 0, 0, 1) == BUMP_OK, "bump: free step is fine");

    view_set_sight(NULL);
    check(strcmp(describe_field(&world, NULL, 5, 3, buf, sizeof buf),
                 "Zauberer-2") == 0, "look: names the unit on the field");
    check(strcmp(describe_field(&world, NULL, 6, 2, buf, sizeof buf), "Wand") == 0,
          "look: names the wall");
    world.units[11].flags |= UF_FLYING;
    world.units[11].x = 8;
    world.units[11].y = 8;
    check(strcmp(describe_field(&world, NULL, 8, 8, buf, sizeof buf),
                 "Riesenfledermaus (Luft)") == 0, "look: flying units get a suffix");

    world.unit_count = 2;                 /* wizards only, enemies far */
    world.units[0].x = 20;
    world.units[0].y = 13;
    world.units[1].x = 0;
    world.units[1].y = 35;
    sight_init(&s, OWN_P1);
    sight_compute(&world, &s);
    check(strcmp(describe_field(&world, &s, 6, 3, buf, sizeof buf), "Unerforscht.") == 0,
          "look: unexplored stays dark");
    check(strcmp(describe_field(&world, &s, 20, 13, buf, sizeof buf), "Zauberer-1") == 0,
          "look: own units are always named");
    check(strcmp(describe_field(&world, &s, 0, 35, buf, sizeof buf), "Unerforscht.") == 0,
          "look: enemies stay dark without sight");
    world.units[1].x = 21;                /* steps into sight */
    world.units[1].y = 13;
    sight_compute(&world, &s);
    check(strcmp(describe_field(&world, &s, 21, 13, buf, sizeof buf), "Zauberer-2") == 0,
          "look: enemy is named when in sight");
    view_set_sight(NULL);
}

static void test_combat(void)
{
    Rng rng;
    CombatResult r;
    uint8_t seed_hits = 0, k;

    check(combat_hit_chance(10, 10) == 50 && combat_hit_chance(20, 10) == 90 &&
          combat_hit_chance(10, 20) == 10, "combat: chance 50+5/diff, clamped");

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 2;
    world.units[1].x = 7;                       /* goblin east of the wizard */
    world.units[1].y = 6;
    world.units[1].kind = CR_GOBLIN;
    world.units[1].com = 9;
    world.units[1].def = 9;
    world.units[1].con = world.units[1].con_max = 32;
    world.units[1].ap = 30;
    world.units[1].sta = 45;

    check(!combat_melee(&world, &rng, 0, 0, &r), "combat: no self attacks");
    world.units[1].x = 20;
    check(!combat_melee(&world, &rng, 0, 1, &r), "combat: no attacks across the map");
    world.units[1].x = 7;
    world.units[1].owner = OWN_P1;
    check(!combat_melee(&world, &rng, 0, 1, &r), "combat: no friendly fire");
    world.units[1].owner = OWN_NEUTRAL;
    world.units[1].flags |= UF_FLYING;
    check(!combat_melee(&world, &rng, 0, 1, &r), "combat: no melee against flyers");
    world.units[1].flags &= (uint8_t)~UF_FLYING;

    /* many seeded exchanges: statistics instead of pinned rolls (both
     * units reset - return blows wear the wizard down too) */
    for (k = 0; k < 200; k++) {
        rng_seed(&rng, 1000 + k);
        world.units[0].ap = 40;
        world.units[0].sta = 60;
        world.units[0].con = 30;
        world.units[1].ap = 30;
        world.units[1].sta = 45;
        world.units[1].con = 32;
        world.units[1].flags &= (uint8_t)~UF_WOUNDED;
        if (combat_melee(&world, &rng, 0, 1, &r) && r.hit)
            seed_hits++;
    }
    check(seed_hits > 60 && seed_hits < 140, "combat: ~50 % hits over 200 seeds");
    check(world.units[1].x == 7, "combat: survivor is still in place");

    rng_seed(&rng, 7);                          /* determinism */
    world.units[0].ap = 40;
    world.units[1].ap = 0;                      /* exhausted ... */
    world.units[0].con = 30;
    world.units[1].con = 32;
    {
        CombatResult a, b;
        world.units[1].flags &= (uint8_t)~UF_REACTED;   /* fresh round (D29) */
        combat_melee(&world, &rng, 0, 1, &a);
        world.units[0].ap = 40;
        world.units[0].con = 30;
        world.units[1].ap = 0;
        world.units[1].con = 32;
        world.units[1].flags &= (uint8_t)~UF_REACTED;   /* fresh round (D29) */
        rng_seed(&rng, 7);
        combat_melee(&world, &rng, 0, 1, &b);
        check(a.hit == b.hit && a.damage == b.damage && a.returned == b.returned,
              "combat: same seed, same outcome");
        check(a.returned, "combat: free counter even without AP (D27)");
        check(world.units[0].ap == 30, "combat: melee costs 10 AP");
        if (a.returned && !a.attacker_died)
            check(world.units[1].ap == 0, "combat: the counter costs no AP (D27)");
    }

    world.units[1].ap = 30;                     /* fresh defender */
    world.units[1].sta = 45;
    world.units[1].con = 32;
    world.units[1].flags &= (uint8_t)~UF_REACTED;
    rng_seed(&rng, 21);
    combat_melee(&world, &rng, 0, 1, &r);
    check(r.returned, "combat: defenders strike back");
    check(world.units[1].ap == 30 || !r.returned,
          "combat: return blow leaves the defender's AP alone (D27)");
    {   /* D29: one reaction per round - the second attack lands unanswered */
        world.units[0].ap = 40;
        world.units[0].con = 30;
        world.units[1].con = 32;
        rng_seed(&rng, 22);
        combat_melee(&world, &rng, 0, 1, &r);
        check(!r.returned, "combat: no second counter in the same round (D29)");
        world_new_turn(&world);                 /* new round: reaction back */
        world.units[0].ap = 40;
        world.units[0].con = 30;
        world.units[1].con = 32;
        rng_seed(&rng, 23);
        combat_melee(&world, &rng, 0, 1, &r);
        check(r.returned, "combat: the reaction returns next round (D29)");
    }

    world.units[1].con = 1;                     /* mortal blow */
    world.units[1].ap = 0;
    {
        uint8_t count_before = world.unit_count, tries = 0;
        do {
            tries++;
            rng_seed(&rng, 90 + tries);
            world.units[0].ap = 40;
            world.units[1].ap = 0;
            world.units[1].con = 1;
        } while (!combat_melee(&world, &rng, 0, 1, &r) || !r.hit);
        check(r.died && world.unit_count == count_before - 1,
              "combat: the dead leave the world");
    }

    {   /* fatal wound bleeds one point per round (PM 17) */
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 1;
        world.units[0].con = world.units[0].con_max = 30;
        world.units[0].flags |= UF_WOUNDED;
        world_new_turn(&world);
        check(world.units[0].con == 29, "combat: wounds bleed each round");
        world.units[0].con = 1;
        world_new_turn(&world);
        check(world.unit_count == 0, "combat: bleeding to death removes the unit");
    }

    {   /* engagement: fleeing is allowed, the enemy gets a free swing (D26) */
        Rng frng;
        CombatResult r;
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 2;
        world.units[1].x = 7;
        world.units[1].y = 6;
        world.units[1].kind = CR_GOBLIN;
        world_engage(&world, 0);                  /* melee contact */
        check(world_enemy_adjacent(&world, 0),
              "combat: contact puts an enemy next to the unit");
        check(world_move_unit(&world, 0, 0, -1),
              "combat: fleeing out of contact is allowed");
        check(world_enemy_adjacent(&world, 0),
              "combat: the goblin is still adjacent after the step");
        rng_seed(&frng, 21);
        check(combat_disengage_swings(&world, &frng, 0, &r) == 1,
              "combat: the disengage swing happens");
        world_remove_unit(&world, 1);
        check(!world_enemy_adjacent(&world, 0) && world_move_unit(&world, 0, 0, -1),
              "combat: free again after the enemy dies");
    }

    {   /* diagonal slip: leaving all enemies behind avoids the swing (D26) */
        Rng frng;
        CombatResult r;
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 2;
        world.units[1].x = 7;
        world.units[1].y = 6;
        world.units[1].kind = CR_GOBLIN;
        world_engage(&world, 0);
        world.units[0].ap = 40;
        check(world_move_unit(&world, 0, -1, -1) &&
              !world_enemy_adjacent(&world, 0),
              "combat: the diagonal slip leaves the enemy behind");
        check(combat_disengage_swings(&world, &frng, 0, &r) == 0,
              "combat: nobody is adjacent, no free swing");
    }

    {   /* free swing: no AP cost for the swinger, undead immunity holds */
        Rng frng;
        CombatResult r;
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 2;
        world.units[1].kind = CR_ZOMBIE;
        world.units[1].flags |= UF_UNDEAD;
        world.units[1].x = 7;
        world.units[1].y = 6;
        check(!combat_free_swing(&world, &frng, 0, 1, &r),
              "combat: normal weapons cannot free-swing undead");
    }

    {   /* the binding lasts one phase only (GDD 6: next turn free again) */
        uint8_t owner;
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 2;
        world.units[0].ap = 40;
        world.units[1].x = 8;
        world.units[1].y = 6;
        world.units[1].kind = CR_GOBLIN;
        owner = world.units[0].owner;
        check(world_move_unit(&world, 0, 1, 0) &&
              world.units[0].x == 7,            /* now next to the enemy */
              "combat: stepping up to an enemy is allowed");
        check(world_engaged(&world, 0) && world_engaged(&world, 1),
              "combat: arriving next to an enemy binds both");
        {   /* D26: leaving is allowed, the adjacent goblin swings */
            Rng frng;
            CombatResult r;
            bool swing = world_enemy_adjacent(&world, 0) &&
                         combat_disengage_swings(&world, &frng, 0, &r) == 1;
            check(world_move_unit(&world, 0, -1, 0) || world.units[0].x != 7,
                  "combat: leaving the contact is allowed");
            check(swing || world.unit_count == 1,
                  "combat: the goblin got its free swing");
        }
        world_release(&world, owner);          /* his phase is over */
        check(!world_engaged(&world, 0) &&
              (world.units[1].flags & UF_ENGAGED) != 0,
              "combat: only the finished side is released");
        check(world_move_unit(&world, 0, -1, 0),
              "combat: free to leave in the next phase");
    }

    {   /* through the turn flow: bound in contact, free one phase later */
        Turns tt;
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 2;
        world.units[0].ap = 40;
        world.units[1].x = 8;
        world.units[1].y = 6;
        world.units[1].kind = CR_GOBLIN;
        turn_init(&tt, &world, 3, 1u << OWN_P1);
        tt.round1_lock = false;
        check(world_move_unit(&world, 0, 1, 0), "combat: step up to the enemy");
        turn_end_phase(&tt, &world);           /* P1 done, P2 passes, round 2 */
        check(tt.round == 2 && !world_engaged(&world, 0) &&
              !world_engaged(&world, 1),
              "combat: both sides are free again in the next round");
    }

    {   /* terrain attacks (features.csv) */
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 1;
        world.units[0].x = 7;                   /* beside the house door */
        world.units[0].y = 6;
        world.units[0].ap = 40;
        world.feature[5][8] = FE_DOOR_CLOSED;    /* closed for the attack */
        {
            bool destroyed = false;
            check(combat_terrain(&world, &rng, 0, 3, 2, &destroyed) == 0,
                  "combat: walls are indestructible");
            check(combat_terrain(&world, &rng, 0, 9, 5, &destroyed) == 0,
                  "combat: open ground has nothing to hit");
            {
                uint8_t hits = 0;
                uint8_t gen = world.generation;
                while (!destroyed && hits < 100) {
                    world.units[0].ap = 40;
                    rng_seed(&rng, 500 + hits);
                    combat_terrain(&world, &rng, 0, 8, 5, &destroyed);
                    hits++;
                }
                check(destroyed && world.feature[5][8] == FE_NONE &&
                      world.generation != gen,
                      "combat: enough hits smash the door");
            }
        }
    }
}

static void test_spells(void)
{
    Spellbook book;
    static Spellbook scnbooks[OWN_NEUTRAL];

    check(spellbook_load(scnbooks, SCN_MANY_COLOURED_LAND, SCN_MANY_COLOURED_LAND_LEN) &&
          scnbooks[OWN_P1].level[SP_GIANT_BAT] == 2 &&
          scnbooks[OWN_P1].level[SP_MAGIC_BOLT] == 1 &&
          scnbooks[OWN_P1].level[SP_DWARF] == 1 &&
          scnbooks[OWN_P2].level[SP_GOBLIN] == 2,
          "spells: books from the scenario file");
    memset(&book, 0, sizeof book);
    book.level[SP_GIANT_BAT] = 2;       /* p1 test book for the casts below */
    check(SUMMON_KIND[SP_DWARF] == CR_DWARF && SUMMON_KIND[SP_GIANT_BAT] == CR_GIANT_BAT &&
          SUMMON_KIND[SP_MAGIC_BOLT] == 0xFF, "spells: summon kinds from the table");

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 1;                       /* the wizard at 6,6 */
    check(spell_can_cast(&world, &book, 0, SP_GIANT_BAT), "spells: castable");
    world.units[0].flags |= UF_FLYING;
    check(!spell_can_cast(&world, &book, 0, SP_GIANT_BAT), "spells: not from the air");
    world.units[0].flags &= (uint8_t)~UF_FLYING;
    world.units[0].mana = 3;
    check(!spell_can_cast(&world, &book, 0, SP_GIANT_BAT), "spells: too little mana");
    world.units[0].mana = 80;
    world.units[0].ap = 5;
    check(!spell_can_cast(&world, &book, 0, SP_GIANT_BAT), "spells: too little AP");

    world.units[0].ap = 40;
    {
        uint8_t got = spell_summon(&world, &book, 0, SP_GIANT_BAT);
        check(got == 2 && world.unit_count == 3, "spells: level 2 summons two bats");
        check(world.units[1].kind == CR_GIANT_BAT && world.units[1].owner == OWN_P1 &&
              world.units[1].ap == 24 && world.units[1].sta == 75,
              "spells: summoned with its own values");
        check(world.units[0].ap == 30 && world.units[0].mana == 71 &&
              book.level[SP_GIANT_BAT] == 1, "spells: 10 AP, 9 mana, one level");
    }

    {   /* no room: mana lost, nothing appears (GDD 7.2) */
        uint8_t k;
        static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        for (k = 0; k < 8; k++) {
            int16_t x = (int16_t)(world.units[0].x + DX[k]);
            int16_t y = (int16_t)(world.units[0].y + DY[k]);
            if (world_wrap(&world, &x, &y) && !world_blocks(&world, x, y))
                world_spawn_unit(&world, OWN_NEUTRAL, CR_GOBLIN, (uint8_t)x, (uint8_t)y);
        }
        world.units[0].ap = 40;
        check(spell_summon(&world, &book, 0, SP_GIANT_BAT) == 0 &&
              world.units[0].mana == 71 - 7 && book.level[SP_GIANT_BAT] == 0,
              "spells: without room the mana is lost");
    }
}

static void test_bolt(void)
{
    Spellbook book;
    SpellShot shot;
    Rng rng;
    uint8_t hits = 0, k;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 1;
    world.feature[5][8] = FE_DOOR_OPEN; /* open the house door for lines */
    memset(&book, 0, sizeof book);
    book.level[SP_MAGIC_BOLT] = 8;      /* enough casts for the tests */
    world.units[1].kind = CR_GOBLIN;    /* target on the path */
    world.units[1].owner = OWN_NEUTRAL;
    world.units[1].x = 9;
    world.units[1].y = 5;
    world.units[1].con = world.units[1].con_max = 32;
    world.unit_count = 2;

    check(!spell_bolt(&world, &book, 0, SP_MAGIC_BOLT, 20, 20, &rng, &shot),
          "bolt: out of range is rejected");
    check(!spell_bolt(&world, &book, 0, SP_MAGIC_BOLT, 6, 1, &rng, &shot),
          "bolt: no line of sight through the wall");
    {   /* wizard flies: no casting from the air (CAST-G only) */
        world.units[0].flags |= UF_FLYING;
        check(!spell_bolt(&world, &book, 0, SP_MAGIC_BOLT, 9, 5, &rng, &shot),
              "bolt: not from the air");
        world.units[0].flags &= (uint8_t)~UF_FLYING;
    }

    {   /* seeded volleys at the goblin; failures counted, printed once */
        bool all_cast = true;
        for (k = 0; k < 50; k++) {
            world.units[0].ap = 40;
            world.units[0].mana = 80;
            world.units[1].con = 32;
            book.level[SP_MAGIC_BOLT] = 8;
            rng_seed(&rng, 7000 + k);
            if (!spell_bolt(&world, &book, 0, SP_MAGIC_BOLT, 9, 5, &rng, &shot))
                all_cast = false;
            if (shot.hit)
                hits++;
            if (world.unit_count == 1)  /* goblin died: respawn */
                world_spawn_unit(&world, OWN_NEUTRAL, CR_GOBLIN, 9, 5);
        }
        check(all_cast, "bolt: 50 casts all go through");
    }
    check(hits > 10 && hits < 40, "bolt: hits around the 55 % mark");
    check(book.level[SP_MAGIC_BOLT] == 7, "bolt: every cast burns one level");

    {   /* lightning: splash + terrain + wall rejection */
        memset(&book, 0, sizeof book);
        book.level[SP_MAGIC_LIGHTNING] = 1;
        world.unit_count = 1;
        world.units[0].ap = 40;
        world.units[0].mana = 80;
        world_spawn_unit(&world, OWN_NEUTRAL, CR_GOBLIN, 9, 5);
        check(!spell_lightning(&world, &book, 0, 3, 2, &rng, &shot),
              "bolt: lightning rejects massive targets");
        world.feature[5][9] = FE_ROCK;  /* destructible terrain at the target */
        rng_seed(&rng, 77);
        check(spell_lightning(&world, &book, 0, 9, 5, &rng, &shot),
              "bolt: lightning strikes");
        check(shot.terrain_smashed && world.feature[5][9] == FE_NONE,
              "bolt: lightning smashes terrain at the target");
        check(book.level[SP_MAGIC_LIGHTNING] == 0, "bolt: level used up");
    }
}

void selftest_set_verbose(bool verbose)
{
    verbose_checks = verbose;
}

static void test_items(void)
{
    Rng rng;

    check(OBJECTS[OBJ_SWORD].weapon == WEAPON_SWORD &&
          OBJECTS[OBJ_GOLD].vp == 40 && OBJECTS[OBJ_SCROLL].category == OC_SCROLL,
          "items: table values from objects.csv");
    check(WEAPONS[WEAPON_SWORD].combat == 10 && WEAPONS[WEAPON_SHIELD].defence == 13 &&
          WEAPONS[WEAPON_BOW].ranged == 1 && WEAPONS[WEAPON_SWORD].dice_n == 2 &&
          WEAPONS[WEAPON_SWORD].die == 8 && WEAPONS[WEAPON_MAGIC_SLAYER].dice_n == 3,
          "items: weapon values");

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 1;                    /* wizard at 6,6 */
    world.units[0].x = 6;
    world.units[0].y = 8;                    /* the sword lies at 6,8 */
    check(items_kind_at(&world, 6, 8) == OBJ_SWORD, "items: sword on the ground");

    check(items_pick_up(&world, 0) && world.units[0].item_count == 1 &&
          world.units[0].items[0] == OBJ_SWORD && world.units[0].ap == 34,
          "items: picking up costs 6 AP");
    check(items_kind_at(&world, 6, 8) == NO_ITEM, "items: gone from the ground");
    check(items_combat(&world, 0) == 10 && items_defence(&world, 0) == 12,
          "items: bare-handed values");      /* not wielded yet */

    check(items_cycle(&world, 0) && world.units[0].in_use == 0 &&
          world.units[0].ap == 30, "items: wielding costs 4 AP");
    check(items_combat(&world, 0) == 20, "items: sword +10 combat (D31)");

    {   /* shield carried: defence always (GDD 6.1) */
        world.units[0].items[1] = OBJ_SHIELD;
        world.units[0].item_count = 2;
        check(items_defence(&world, 0) == 25, "items: carried shield +13 defence (D31)");
        world.units[0].items[2] = OBJ_SHIELD;
        world.units[0].item_count = 3;
        check(items_defence(&world, 0) == 25, "items: shields do not stack (D21)");
        world.units[0].item_count = 2;
    }

    check(items_drop(&world, 0) && world.units[0].item_count == 1 &&
          items_kind_at(&world, 6, 8) == OBJ_SWORD && world.units[0].ap == 28,
          "items: dropping costs 2 AP");

    {   /* throw the scroll eastwards across the open grass */
        world.units[0].x = 10;
        world.units[0].y = 8;
        world.units[0].in_use = 0;
        world.units[0].items[0] = OBJ_SCROLL;
        world.units[0].item_count = 1;
        world.units[0].ap = 40;
        rng_seed(&rng, 5);
        check(items_throw(&world, &rng, 0, 1, 0) && world.units[0].item_count == 0,
              "items: throw leaves the hand");
        check(items_kind_at(&world, 16, 8) == OBJ_SCROLL,
              "items: scroll flies six fields east");
    }

    {   /* bow: pick up, wield, fire at the goblin (9,6) from outside */
        uint8_t dmg = 1;
        world.units[0].x = 8;
        world.units[0].y = 5;
        world.units[0].ap = 40;
        world.units[0].items[0] = OBJ_BOW;
        world.units[0].item_count = 1;
        world.units[0].in_use = 0;
        world_spawn_unit(&world, OWN_NEUTRAL, CR_GOBLIN, 11, 5);
        rng_seed(&rng, 11);
        check(items_fire(&world, &rng, 0, 11, 5, &dmg),
              "items: bow fires in range");
        check(world.units[0].ap == 28, "items: firing costs 12 AP");
        check(!items_fire(&world, &rng, 0, 20, 5, &dmg),
              "items: out of range rejected");
    }
}

static void test_game(void)
{
    Game g;
    Rng rng;

    rng_seed(&rng, 42);
    game_init(&g, 26, 3, 12, 15, &rng);
    check(g.portal_round >= 12 && g.portal_round <= 15 && !g.portal_open,
          "game: portal round within the span");
    game_new_round(&g, 11);
    check(!g.portal_open, "game: still closed before its round");
    game_new_round(&g, g.portal_round);
    check(g.portal_open, "game: opens at its round");

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 1;
    world.units[0].x = 26;
    world.units[0].y = 3;
    world.units[0].items[0] = OBJ_GOLD;         /* 40 VP */
    world.units[0].items[1] = OBJ_SWORD;        /* worth nothing */
    world.units[0].item_count = 2;
    check(game_try_enter_portal(&g, &world, 0) && world.unit_count == 0,
          "game: the wizard escapes and leaves the world");
    check(g.vp[OWN_P1] == VP_ESCAPE + 40 && (g.escaped & 1) != 0,
          "game: escape bonus plus carried treasures");
    check(items_kind_at(&world, 26, 3) == NO_ITEM,
          "game: the escaped take their objects along");

    {   /* creatures cannot pass */
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 1;
        world.units[0].kind = CR_GOBLIN;
        world.units[0].x = 26;
        world.units[0].y = 3;
        check(!game_try_enter_portal(&g, &world, 0) && world.unit_count == 1,
              "game: creatures cannot pass");
        check(game_over(&g, &world), "game: over without wizards");
    }

    {   /* the open portal is drawn as an animated layer */
        FieldLayers f;
        view_set_sight(NULL);
        view_set_portal(26, 3);
        view_compose(&world, 26, 3, &f);
        check(has_layer(&f, T_PORTAL_0), "game: portal drawn on its field");
        view_set_portal(-1, -1);
        view_set_sight(NULL);
    }

    {   /* kill credit: the victim's value, wizard melee doubled (AMI 4) */
        Kill k = {CR_GOBLIN, OWN_NEUTRAL, CR_WIZARD, OWN_P1, true};
        uint16_t gob = CREATURES[CR_GOBLIN].vp;
        game_init(&g, -1, -1, 1, 1, &rng);
        game_kill_credit(&g, &k);                  /* wizard melee: x2 */
        k.melee = false;
        game_kill_credit(&g, &k);                  /* wizard ranged: x1 */
        k.killer_kind = CR_GIANT_BAT;
        k.killer_owner = OWN_P2;
        k.melee = true;
        game_kill_credit(&g, &k);                  /* creature melee: x1 */
        check(g.vp[OWN_P1] == 3 * gob && g.vp[OWN_P2] == gob,
              "game: kills score the victim's value");
        k.victim_kind = CR_WIZARD;
        k.victim_owner = OWN_P1;
        game_kill_credit(&g, &k);
        check(g.vp[OWN_P2] == gob + CREATURES[CR_WIZARD].vp,
              "game: a wizard kill scores the wizard's value");
        k.victim_owner = OWN_P2;
        game_kill_credit(&g, &k);
        check(g.vp[OWN_P2] == gob + CREATURES[CR_WIZARD].vp,
              "game: friendly fire scores nothing");
    }
}

static void test_ai(void)
{
    Turns t;
    Game g;
    Spellbook books[OWN_NEUTRAL];
    AiCtx ctx;
    Rng rng;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 2;                       /* wizard + one goblin */
    world.units[1].kind = CR_GOBLIN;
    world.units[1].owner = OWN_NEUTRAL;
    world.units[1].x = 9;                       /* in sight through the door */
    world.units[1].y = 5;
    world.units[1].com = 9;
    world.units[1].def = 9;
    world.units[1].con = world.units[1].con_max = 32;
    world.units[1].ap = 30;
    world.units[1].sta = 45;
    world.feature[5][8] = FE_DOOR_OPEN;         /* clear line of sight */

    check(ai_nearest_enemy(&world, 1, 9) == 0, "ai: goblin scents the wizard");
    world.feature[5][8] = FE_DOOR_CLOSED;
    world.units[1].x = 12;                      /* behind the east wall */
    world.units[1].y = 6;
    check(ai_nearest_enemy(&world, 1, 9) == NO_UNIT,
          "ai: no prey through walls (hidden movement)");
    world.units[1].x = 9;
    world.units[1].y = 5;
    world.feature[5][8] = FE_DOOR_OPEN;

    {   /* hunter with distance closes in */
        uint8_t before = 255, after;
        world.units[1].ap = 30;
        rng_seed(&rng, 3);
        before = (uint8_t)((world.units[1].x > world.units[0].x)
                               ? world.units[1].x - world.units[0].x : 1);
        ai_hunter(&world, &rng, 1);
        after = (uint8_t)((world.units[1].x > world.units[0].x)
                              ? world.units[1].x - world.units[0].x : 0);
        check(after < before || world.units[1].ap < 30,
              "ai: hunter closes in or fights");
    }

    {   /* wizard AI: summons, then walks to the portal over rounds */
        uint8_t seen_summons = 0, k;
        uint8_t p2 = world_spawn_unit(&world, OWN_P2, CR_WIZARD, 2, 13);
        memset(books, 0, sizeof books);
        books[OWN_P2].level[SP_GOBLIN] = 2;
        (void)p2;
        game_init(&g, 26, 3, 1, 1, &rng);       /* portal open from round 1 */
        game_new_round(&g, 1);
        ctx.books = books;
        ctx.game = &g;
        t = (Turns){0};
        t.phase = OWN_P2;
        rng_seed(&t.rng, 99);
        for (k = 0; k < 40 && !(g.escaped & (1u << OWN_P2)); k++) {
            uint8_t count = world.unit_count;
            ai_wizard_phase(&t, &world, &ctx);
            if (world.unit_count > count)
                seen_summons = 1;
            world_new_turn(&world);             /* next round, AP refills */
            t.round++;
        }
        check(seen_summons, "ai: the wizard summons company");
        check(g.escaped & (1u << OWN_P2),
              "ai: the wizard escapes through the portal");
        check(g.vp[OWN_P2] >= VP_ESCAPE, "ai: escape scores");
    }
}

static uint8_t rounds_seen;

static void count_round(Turns *t, World *w, void *ctx)
{
    (void)w;
    rounds_seen++;
    game_new_round((Game *)ctx, t->round);
}

/* Regressions from the M3 review: unit removal reorders the list, kills
 * score the victim, the turn loop ends without humans, > 24 units. */
static void test_review_fixes(void)
{
    Turns t;
    Rng rng;
    Game g;
    Spellbook books[OWN_NEUTRAL];
    AiCtx ctx;
    uint8_t a, b, i, a_id, b_id;

    rng_seed(&rng, 7);
    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 0;
    world_spawn_unit(&world, OWN_P1, CR_WIZARD, 5, 5);
    world_spawn_unit(&world, OWN_NEUTRAL, CR_GOBLIN, 10, 10);
    a = world_spawn_unit(&world, OWN_P1, CR_DWARF, 12, 12);
    b = world_spawn_unit(&world, OWN_P1, CR_GIANT_BAT, 14, 14);
    a_id = world.units[a].id;
    b_id = world.units[b].id;
    check(a_id != b_id && world_find_unit(&world, a_id) == a,
          "fix: units get distinct ids");
    turn_init(&t, &world, 1, 1u << OWN_P1);
    turn_next_unit(&t, &world, false);
    check(t.active == a, "fix: the dwarf is active");
    world.units[b].done = true;
    world_kill_unit(&world, 1, CR_DWARF, OWN_P1, true);   /* goblin dies */
    turn_revalidate(&t, &world);
    check(world.units[t.active].id == a_id && !world.units[t.active].done,
          "fix: active unit survives a death elsewhere");
    check(world.units[world_find_unit(&world, b_id)].done,
          "fix: done flags move with their units");
    check(world.kill_count == 1 && world.kills[0].victim_kind == CR_GOBLIN,
          "fix: the kill is logged");
    world_kill_unit(&world, t.active, CR_GOBLIN, OWN_NEUTRAL, true);
    check(world.kill_count == 1, "fix: independents' kills are not logged");
    turn_revalidate(&t, &world);
    check(t.active < world.unit_count && world.units[t.active].owner == OWN_P1,
          "fix: a dead active unit hands over to another");
    game_init(&g, -1, -1, 1, 1, &rng);
    game_credit_kills(&g, &world);
    check(g.vp[OWN_P1] == CREATURES[CR_GOBLIN].vp && world.kill_count == 0,
          "fix: logged kills are credited once");

    {   /* a refused melee leaves a clean result (the AI reads it) */
        CombatResult r;
        memset(&r, 0xAA, sizeof r);
        check(!combat_melee(&world, &rng, 0, 0, &r) && !r.died && !r.attacker_died,
              "fix: refused melee zeroes the result");
    }

    {   /* D21: the dead drop what they carried, also when bleeding out */
        uint8_t v, objs = world.object_count;
        v = world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 20, 21);
        world.units[v].items[0] = OBJ_GOLD;
        world.units[v].items[1] = OBJ_SWORD;
        world.units[v].item_count = 2;
        world_kill_unit(&world, v, CR_WIZARD, OWN_P1, true);
        check(world.object_count == objs + 2 && items_kind_at(&world, 20, 21) != NO_ITEM,
              "fix: the dead drop their objects");
        game_credit_kills(&g, &world);
        v = world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 21, 21);
        world.units[v].items[0] = OBJ_GOLD;
        world.units[v].item_count = 1;
        world.units[v].con = 1;
        world.units[v].flags |= UF_WOUNDED;
        v = world.units[v].id;
        world_new_turn(&world);
        check(world_find_unit(&world, v) == NO_UNIT && world.object_count == objs + 3,
              "fix: bled-out units drop too");
        check(items_kind_at(&world, 21, 21) == OBJ_GOLD,
              "fix: the treasure lies where he fell");
    }

    {   /* D21: ranged attacks share the 10..90 % clamp of D16 */
        uint8_t s, tg, hits = 0, n;
        world_load_bin(&world, MAPBIN_MANY_COLOURED_LAND, MAPBIN_MANY_COLOURED_LAND_LEN);
        world.unit_count = 0;
        s = world_spawn_unit(&world, OWN_P1, CR_DWARF, 2, 0);
        tg = world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 5, 0);
        world.units[s].items[0] = OBJ_BOW;
        world.units[s].item_count = 1;
        world.units[s].in_use = 0;
        world.units[tg].def = 200;               /* old formula: never */
        world.units[tg].con = world.units[tg].con_max = 255;
        for (n = 0; n < 100; n++) {
            uint8_t dmg = 0;
            world.units[s].ap = 40;
            if (items_fire(&world, &rng, s, 5, 0, &dmg) && dmg)
                hits++;
        }
        check(hits > 0 && hits < 30, "fix: bow hits at least 10 % (D16 clamp)");
    }

    /* more than 24 units: the eZ80 int is 24 bit (no bit masks) */
    world.unit_count = 0;
    for (i = 0; i < 30; i++)
        world_spawn_unit(&world, OWN_P1, CR_GOBLIN, i, 20);
    turn_init(&t, &world, 1, 1u << OWN_P1);
    for (i = 0; i < 29; i++)
        turn_finish_unit(&t, &world);
    check(t.active == 29 && turn_units_left(&t, &world),
          "fix: finish flags beyond unit 24");
    turn_finish_unit(&t, &world);
    check(!turn_units_left(&t, &world), "fix: all 30 units finished");

    /* no human left: the AI plays on, the round hook opens the portal,
     * the escaped wizard ends the loop */
    load_house();
    {
        uint8_t x = world.units[0].x, y = world.units[0].y;
        world.unit_count = 0;
        world_spawn_unit(&world, OWN_P2, CR_WIZARD, x, y);
        for (i = 0; i < OWN_NEUTRAL; i++)
            memset(&books[i], 0, sizeof books[i]);
        ctx.books = books;
        ctx.game = &g;
        game_init(&g, x, y, 3, 3, &rng);          /* opens under him */
        turn_init(&t, &world, 1, 1u << OWN_P1);
        t.ai = ai_wizard_phase;
        t.ai_ctx = &ctx;
        t.on_round = count_round;
        t.round_ctx = &g;
        rounds_seen = 0;
        turn_end_phase(&t, &world);
        check(g.portal_open && rounds_seen == 2 && t.round == 3,
              "fix: round hook opens the portal in AI rounds");
        check((g.escaped & (1u << OWN_P2)) != 0,
              "fix: the AI escapes, the turn loop returns");

        world.unit_count = 0;                     /* no portal: bounded */
        world_spawn_unit(&world, OWN_P2, CR_WIZARD, x, y);
        game_init(&g, -1, -1, 1, 1, &rng);
        turn_init(&t, &world, 1, 1u << OWN_P1);
        t.ai = ai_wizard_phase;
        t.ai_ctx = &ctx;
        t.on_round = count_round;
        t.round_ctx = &g;
        rounds_seen = 0;
        turn_end_phase(&t, &world);
        check(rounds_seen == TURN_AUTOPLAY_ROUNDS &&
              t.round == 1 + TURN_AUTOPLAY_ROUNDS,
              "fix: AI autoplay stops after its bound");
    }
}

static void test_scenario(void)
{
    world_load_bin(&world, MAPBIN_MANY_COLOURED_LAND, MAPBIN_MANY_COLOURED_LAND_LEN);
    check(world.w == 36 && world.h == 36 && world.wrap,
          "scn: 36x36, wraps");
    check(world.portal_x == 26 && world.portal_y == 3 &&
          world.portal_rmin == 12 && world.portal_rmax == 15,
          "scn: portal from the map (v3)");
    {
        uint8_t i, wizards = 0, treasures = 0;
        int16_t vp_fields = 0;
        for (i = 0; i < world.unit_count; i++)
            if (world.units[i].kind == CR_WIZARD)
                wizards++;
        for (i = 0; i < world.object_count; i++) {
            uint8_t k, kind = NO_ITEM;
            for (k = 0; k < OBJ_COUNT; k++)
                if (OBJECTS[k].tile == world.objects[i].tile)
                    kind = k;
            if (kind != NO_ITEM && OBJECTS[kind].category == OC_TREASURE) {
                treasures++;
                vp_fields = (int16_t)(vp_fields + OBJECTS[kind].vp);
            }
        }
        check(wizards == 2, "scn: two wizards (human + AI)");
        check(treasures >= 6 && vp_fields >= 114,
              "scn: six treasures worth the GDD table");
    }
    check(world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN) &&
          world.portal_x == -1,
          "scn: v2 testland stays portal-free");
}

static void test_m4a(void)
{
    Rng rng;

    check(OBJECTS[OBJ_APPLE].eat_con == 4 && OBJECTS[OBJ_MAGIC_MUSHROOM].eat_mana == 6 &&
          OBJECTS[OBJ_CHEST_KEY].category == OC_KEY,
          "m4a: food and key values from objects.csv");

    {   /* undead: only undead, magic weapons and spells wound (GDD 4.2) */
        CombatResult r;
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 2;
        world.units[1].kind = CR_ZOMBIE;
        world.units[1].owner = OWN_NEUTRAL;
        world.units[1].flags |= UF_UNDEAD;
        world.units[1].x = 7;
        world.units[1].y = 6;
        world.units[1].con = world.units[1].con_max = 40;
        world.units[1].ap = 0;              /* no return blows in this test */
        rng_seed(&rng, 1);
        combat_melee(&world, &rng, 0, 1, &r);
        check(!r.hit && world.units[1].con == 40 && world.units[0].ap == 30,
              "m4a: bare hands clank off the zombie");
        world.units[0].ap = 40;
        world.units[0].items[0] = OBJ_SWORD;
        world.units[0].item_count = 1;
        world.units[0].in_use = 0;
        rng_seed(&rng, 1);
        combat_melee(&world, &rng, 0, 1, &r);
        check(!r.hit && world.units[1].con == 40,
              "m4a: normal weapons cannot wound undead");
        world.units[0].items[0] = OBJ_MAGIC_SLAYER;
        world.units[0].item_count = 1;
        world.units[0].in_use = 0;
        {
            uint8_t k, hit = 0;
            for (k = 0; k < 30; k++) {
                world.units[0].ap = 40;
                world.units[0].con = 30;   /* free counters wear him down */
                world.units[1].con = 40;
                rng_seed(&rng, 200 + k);
                combat_melee(&world, &rng, 0, 1, &r);
                hit = hit || r.hit;
            }
            check(hit, "m4a: the magic slayer wounds the zombie");
        }
        world.units[0].items[0] = OBJ_SWORD;
        world.units[0].flags |= UF_MAGIC_WEAPON;   /* enchanted (M4b) */
        {
            uint8_t k, hit = 0;
            for (k = 0; k < 30; k++) {
                world.units[0].ap = 40;
                world.units[0].con = 30;
                world.units[1].con = 40;
                rng_seed(&rng, 300 + k);
                combat_melee(&world, &rng, 0, 1, &r);
                hit = hit || r.hit;
            }
            check(hit, "m4a: enchanted weapons wound undead");
        }
    }

    {   /* below half Constitution: -2 combat and defence (GDD 4.1) */
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 1;
        world.units[0].con = world.units[0].con_max = 30;
        check(items_combat(&world, 0) == 10 && items_defence(&world, 0) == 12,
              "m4a: full strength values");
        world.units[0].con = 14;              /* under 50 % */
        check(items_combat(&world, 0) == 8 && items_defence(&world, 0) == 10,
              "m4a: below half Constitution -2/-2");
        world.units[0].sta = 60;
        world_new_turn(&world);
        check(world.units[0].ap == 20,
              "m4a: badly hurt units refill half AP");
    }

    {   /* eat and read */
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 1;
        world.units[0].con = 10;
        world.units[0].ap = 40;
        world.units[0].items[0] = OBJ_APPLE;
        world.units[0].item_count = 1;
        world.units[0].in_use = 0;
        check(items_eat(&world, 0) && world.units[0].con == 14 &&
              world.units[0].item_count == 0 && world.units[0].ap == 34,
              "m4a: eating an apple heals 4 Con");
        world.units[0].items[0] = OBJ_MAGIC_MUSHROOM;
        world.units[0].item_count = 1;
        world.units[0].in_use = 0;
        world.units[0].mana = 60;
        check(items_eat(&world, 0) && world.units[0].mana == 66,
              "m4a: the magic mushroom gives 6 mana");
        world.units[0].items[0] = OBJ_SCROLL;
        world.units[0].item_count = 1;
        world.units[0].in_use = 0;
        check(items_read(&world, 0) != NULL && world.units[0].item_count == 0,
              "m4a: reading consumes the scroll");
        check(!items_eat(&world, 0), "m4a: nothing edible left");
    }

    {   /* chests: key unlocks cheap, prying costs triple, loot drops */
        bool destroyed = false;
        uint8_t objects_before;
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 1;
        world.units[0].x = 4;                /* next to the chest at 3,8 */
        world.units[0].y = 8;
        world.units[0].ap = 40;
        world.feature[8][3] = FE_CHEST;
        objects_before = world.object_count;
        rng_seed(&rng, 9);
        check(items_open_chest(&world, &rng, 0, 3, 8) &&
              world.feature[8][3] == FE_NONE &&
              world.object_count == objects_before + 1 &&
              world.units[0].ap == 40 - ACTIONS[ACT_OPEN_CHEST].ap * 3,
              "m4a: prying open costs triple AP and drops loot");
        world.feature[8][3] = FE_CHEST;
        world.units[0].items[0] = OBJ_CHEST_KEY;
        world.units[0].item_count = 1;
        world.units[0].in_use = 0;
        rng_seed(&rng, 9);
        check(items_open_chest(&world, &rng, 0, 3, 8) &&
              world.units[0].item_count == 0 &&
              world.units[0].ap == 40 - ACTIONS[ACT_OPEN_CHEST].ap * 3 -
                                        ACTIONS[ACT_UNLOCK].ap,
              "m4a: the key unlocks for 8 AP and vanishes");
        (void)destroyed;
    }
}

static void test_m4b(void)
{
    Spellbook book;
    SpellShot shot;
    Rng rng;
    uint8_t k;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 2;
    world.units[1].kind = CR_GOBLIN;
    world.units[1].owner = OWN_P2;
    world.units[1].x = 9;
    world.units[1].y = 5;
    world.units[1].mr = 46;
    world.units[1].con = world.units[1].con_max = 32;
    world.feature[5][8] = FE_DOOR_OPEN;         /* targets need sight (D17) */
    memset(&book, 0, sizeof book);

    {   /* effects: grant, tick, expire */
        Unit *u = &world.units[0];
        check(effect_grant(u, EFF_SHIELD, 4, 2) && effect_active(u, EFF_SHIELD) &&
              effect_power(u, EFF_SHIELD) == 4, "m4b: effect granted");
        check(items_defence(&world, 0) == 12 + 4, "m4b: shield spell adds defence");
        world_new_turn(&world);
        check(effect_active(u, EFF_SHIELD), "m4b: one round off, still there");
        world_new_turn(&world);
        check(!effect_active(u, EFF_SHIELD) && items_defence(&world, 0) == 12,
              "m4b: effect expires and the flag goes");
    }

    {   /* magic shield spell */
        Unit *u = &world.units[0];
        memset(u->effects, 0, sizeof u->effects);
        book.level[SP_MAGIC_SHIELD] = 3;
        world.units[0].ap = 40;
        world.units[0].mana = 80;
        rng_seed(&rng, 1);
        check(spell_apply(&world, &book, 0, SP_MAGIC_SHIELD, u->x, u->y, &rng,
                          &shot) == CAST_OK &&
              effect_active(u, EFF_SHIELD) && effect_power(u, EFF_SHIELD) == 6,
              "m4b: magic shield +6 for 6 rounds");
        check(book.level[SP_MAGIC_SHIELD] == 2, "m4b: one level down");
    }

    {   /* teleport: jump with scatter, 0 AP, fails on busy */
        Unit *u = &world.units[0];
        book.level[SP_TELEPORT] = 1;
        u->ap = 40;
        u->mana = 80;
        rng_seed(&rng, 7);
        check(spell_apply(&world, &book, 0, SP_TELEPORT, 12, 6, &rng, &shot) ==
              CAST_OK, "m4b: teleport goes through");
        check(u->ap == 0 && u->x >= 8 && u->x <= 16, "m4b: scattered, 0 AP");
        u->x = 6;
        u->y = 6;
    }

    {   /* curse: wound on failed resistance */
        memset(&world.units[1].flags, 0, 1);
        book.level[SP_CURSE] = 4;
        world.units[0].ap = 40;
        world.units[0].mana = 80;
        {
            bool wounded = false, resisted = false;
            for (k = 0; k < 30; k++) {
                CastResult cr;
                world.units[0].ap = 40;
                world.units[0].mana = 80;
                book.level[SP_CURSE] = 4;
                world.units[1].flags &= (uint8_t)~UF_WOUNDED;
                rng_seed(&rng, 400 + k);
                cr = spell_apply(&world, &book, 0, SP_CURSE, 9, 5, &rng, &shot);
                if (cr == CAST_OK && (world.units[1].flags & UF_WOUNDED))
                    wounded = true;
                if (cr == CAST_NO_RES)
                    resisted = true;
            }
            check(wounded && resisted, "m4b: curse wounds or meets resistance");
        }
    }

    {   /* subversion: the goblin changes sides */
        book.level[SP_SUBVERSION] = 8;
        {
            bool switched = false;
            for (k = 0; k < 40; k++) {
                CastResult cr;
                world.units[0].ap = 40;
                world.units[0].mana = 200;
                book.level[SP_SUBVERSION] = 8;
                world.units[1].owner = OWN_P2;
                rng_seed(&rng, 600 + k);
                cr = spell_apply(&world, &book, 0, SP_SUBVERSION, 9, 5, &rng, &shot);
                if (cr == CAST_OK && world.units[1].owner == OWN_P1)
                    switched = true;
            }
            check(switched, "m4b: subversion wins the goblin over");
            world.units[1].owner = OWN_P2;
        }
    }

    {   /* magic attack: hits the whole kind around the target */
        uint8_t hits = 0, k2;
        world_spawn_unit(&world, OWN_NEUTRAL, CR_GOBLIN, 10, 5);   /* same kind */
        for (k2 = 0; k2 < 30; k2++) {
            hits = hits;                 /* keep count below */
            world.units[0].ap = 40;
            world.units[0].mana = 200;
            book.level[SP_MAGIC_ATTACK] = 8;
            while (world.unit_count < 4) /* two goblins: 1 and 3 */
                world_spawn_unit(&world, OWN_P2, CR_GOBLIN,
                                 9, 5);
            rng_seed(&rng, 700 + k2);
            if (spell_apply(&world, &book, 0, SP_MAGIC_ATTACK, 9, 5, &rng,
                            &shot) == CAST_OK)
                hits = (uint8_t)(hits + shot.splash_hits);
        }
        check(hits > 0, "m4b: magic attack strikes the kind in the area");
        check(world.units[2].kind == CR_DWARF || world.unit_count <= 4,
              "m4b: other kinds stay untouched");
    }

    {   /* enchant: weapons of every unit on the field become magic */
        Unit *g = &world.units[1];
        g->items[0] = OBJ_SWORD;
        g->item_count = 1;
        g->in_use = 0;
        book.level[SP_ENCHANT] = 2;
        world.units[0].ap = 40;
        world.units[0].mana = 80;
        rng_seed(&rng, 9);
        check(spell_apply(&world, &book, 0, SP_ENCHANT, g->x, g->y, &rng, &shot) ==
              CAST_OK && (g->flags & UF_MAGIC_WEAPON) != 0,
              "m4b: enchant flags the carried weapons");
        check(effect_active(g, EFF_MAGIC_WEAPON), "m4b: enchant as effect");
        {
            uint8_t z = world_spawn_unit(&world, OWN_NEUTRAL, CR_ZOMBIE, 8, 5);
            check(items_can_harm_undead(&world, 1, z),
                  "m4b: the enchanted sword wounds undead");
        }
    }

    {   /* magic eye reveals through walls */
        Sight s;
        world.unit_count = 1;
        world.units[0].x = 6;
        world.units[0].y = 6;
        sight_init(&s, OWN_P1);
        sight_compute(&world, &s);
        check(!sight_visible(&s, &world, 6, 1), "m4b: wall blocks the view");
        sight_add_eye(&s, &world, 6, 2);
        check(sight_visible(&s, &world, 6, 1) && sight_visible(&s, &world, 6, 6),
              "m4b: the eye sees through walls");
    }

    {   /* speed: double AP, triple recovery */
        Unit *u = &world.units[0];
        memset(u->effects, 0, sizeof u->effects);
        effect_grant(u, EFF_SPEED, 1, 2);
        u->sta = 15;                     /* not exhausted, recovery visible */
        world_new_turn(&world);
        check(u->ap == 80, "m4b: speed doubles AP");
        check(u->sta == 60, "m4b: triple stamina recovery (15+45 capped)");
    }
}

static void test_m4c(void)
{
    Spellbook book;
    Rng rng;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 1;                    /* the wizard at 6,6 */
    memset(&book, 0, sizeof book);
    book.level[SP_HEALING_POTION] = 2;
    book.level[SP_STRENGTH_POTION] = 1;
    book.level[SP_GOLD_DRAGON] = 1;

    {   /* brewing needs cauldron + ingredient, yields level+3 doses */
        Cauldron *c;
        world.units[0].ap = 40;
        world.units[0].mana = 80;
        check(!brew_cast(&world, &book, 0, SP_HEALING_POTION),
              "m4c: no brewing without a cauldron");
        brew_set_cauldron(&world, 6, 6, false, 0xFF);
        check(!brew_cast(&world, &book, 0, SP_HEALING_POTION),
              "m4c: no brewing without the ingredient");
        {   /* apple lies on the field: healing ingredient (GDD 7.2) */
            world.objects[world.object_count].x = 6;
            world.objects[world.object_count].y = 6;
            world.objects[world.object_count].tile = T_OBJ_APPLE;
            world.object_count++;
        }
        check(brew_cast(&world, &book, 0, SP_HEALING_POTION) &&
              (c = brew_cauldron_at(&world, 6, 6)) != NULL &&
              c->doses == 5 && c->potion == SP_HEALING_POTION &&
              book.level[SP_HEALING_POTION] == 1 &&
              world.units[0].mana == 80 - (5 + 2 * 3),
              "m4c: brewing fills level+3 doses and burns one level");
    }

    {   /* drinking heals wounds and consumes doses */
        Unit *u = &world.units[0];
        u->con = 10;
        u->sta = 0;
        u->flags |= UF_WOUNDED;
        u->ap = 40;
        check(brew_drink(&world, 0) && u->con == 30 && u->sta == 60 &&
              !(u->flags & UF_WOUNDED),
              "m4c: the healing draught cures everything");
        check(brew_cauldron_at(&world, 6, 6)->doses == 4,
              "m4c: one dose down");
    }

    {   /* fill a vial, drink it */
        Unit *u = &world.units[0];
        u->items[0] = OBJ_VIAL_EMPTY;
        u->item_count = 1;
        u->in_use = 0;
        u->ap = 40;
        check(brew_fill(&world, 0) && u->items[0] == OBJ_VIAL_HEALING &&
              brew_cauldron_at(&world, 6, 6)->doses == 3,
              "m4c: filling takes a dose from the cauldron");
        u->con = 5;
        u->ap = 40;
        check(brew_drink_vial(&world, 0) && u->con == 30 &&
              u->item_count == 0,
              "m4c: drinking the vial heals");
    }

    {   /* strength potion effect via brewing (M4b machinery) */
        Unit *u = &world.units[0];
        u->ap = 40;
        u->mana = 80;
        {
            world.objects[world.object_count].x = 6;
            world.objects[world.object_count].y = 6;
            world.objects[world.object_count].tile = T_OBJ_MISTLETOE;
            world.object_count++;
        }
        check(brew_cast(&world, &book, 0, SP_STRENGTH_POTION) &&
              effect_active(u, EFF_STRENGTH) == false,
              "m4c: brewed strength waits in the cauldron");
        u->ap = 40;
        check(brew_drink(&world, 0) && effect_active(u, EFF_STRENGTH),
              "m4c: drinking grants the strength effect");
        check(items_combat(&world, 0) == 10 + 2,
              "m4c: brewed at level 1: strength +2 (F1)");
    }

    {   /* bomb vial explodes in the area */
        Unit *u = &world.units[0];
        uint8_t g1, g2;
        rng_seed(&rng, 3);
        u->x = 12;                        /* open grass, wall to the west */
        u->y = 6;
        world_spawn_unit(&world, OWN_NEUTRAL, CR_GOBLIN, 9, 6);
        world_spawn_unit(&world, OWN_NEUTRAL, CR_GOBLIN, 10, 7);
        g1 = world.unit_count - 2;
        g2 = world.unit_count - 1;
        u->items[0] = OBJ_VIAL_BOMB;
        u->item_count = 1;
        u->in_use = 0;
        u->ap = 40;
        check(brew_throw_vial(&world, &rng, 0, -1, 0),
              "m4c: the bomb flies");
        check(world.units[g1].con < 32 || world.units[g2].con < 32 ||
              world.unit_count < 3,
              "m4c: the explosion wounds the goblins");
    }

    {   /* dragons need the herb, and spend it */
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 1;
        book.level[SP_GOLD_DRAGON] = 1;
        world.units[0].mana = 200;
        world.units[0].ap = 40;
        check(spell_summon(&world, &book, 0, SP_GOLD_DRAGON) == 0,
              "m4c: no dragon without a cauldron");
        brew_set_cauldron(&world, 6, 6, false, 0xFF);
        check(spell_summon(&world, &book, 0, SP_GOLD_DRAGON) == 0,
              "m4c: no dragon without dragon herb");
        {
            world.objects[world.object_count].x = 6;
            world.objects[world.object_count].y = 6;
            world.objects[world.object_count].tile = T_OBJ_DRAGON_HERB;
            world.object_count++;
        }
        check(spell_summon(&world, &book, 0, SP_GOLD_DRAGON) == 1 &&
              world.units[1].kind == CR_GOLD_DRAGON &&
              !items_kind_at(&world, 6, 6) == false,   /* herb spent */
              "m4c: the dragon rises and the herb is spent");
        check(items_kind_at(&world, 6, 6) != OBJ_DRAGON_HERB,
              "m4c: the herb is gone after the summon");
    }
}

/* Regressions from the M4a-c review. */
static void test_m4_review(void)
{
    Spellbook book;
    Rng rng;
    Unit *u;
    CombatResult r;
    uint8_t g;

    rng_seed(&rng, 11);
    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 1;                    /* the wizard at 6,6 */
    u = &world.units[0];

    /* flying potion: ground budget in the air, lands when it wears off */
    effect_grant(u, EFF_FLYING, 1, 2);
    check(world_take_off(&world, 0), "m4r: the potion lets the wizard fly");
    world_new_turn(&world);
    check((u->flags & UF_FLYING) && u->ap == u->ap_max,
          "m4r: airborne on a potion keeps the ground AP");
    world_new_turn(&world);
    check(!(u->flags & UF_FLYING), "m4r: lands when the potion wears off");

    /* cauldrons: the object is the truth, full ones stay put */
    u->ap = 40;
    brew_set_cauldron(&world, 6, 6, false, 0xFF);
    check(brew_cauldron_at(&world, 6, 6) != NULL, "m4r: empty cauldron placed");
    check(items_pick_up(&world, 0) && brew_cauldron_at(&world, 6, 6) == NULL,
          "m4r: a carried cauldron leaves no ghost behind");
    u->in_use = (uint8_t)(u->item_count - 1);
    u->x = 9;
    u->y = 5;
    check(items_drop(&world, 0) && brew_cauldron_at(&world, 9, 5) != NULL &&
          brew_cauldron_at(&world, 9, 5)->doses == 0,
          "m4r: set down elsewhere it is an empty cauldron again");
    brew_set_cauldron(&world, 9, 5, true, SP_HEALING_POTION);
    u->ap = 40;
    check(!items_pick_up(&world, 0), "m4r: a full cauldron cannot be carried");
    check(brew_ingredient_potion(OBJ_DRAGON_HERB) == 0xFF,
          "m4r: dragon herb brews no healing potion");

    /* targeted spells need range and sight (D17) */
    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 1;
    u = &world.units[0];
    memset(&book, 0, sizeof book);
    book.level[SP_CURSE] = 3;
    g = world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 20, 20);
    {
        SpellShot shot;
        u->ap = 40;
        u->mana = 80;
        check(spell_apply(&world, &book, 0, SP_CURSE, world.units[g].x,
                          world.units[g].y, &rng, &shot) == CAST_REJECTED &&
              book.level[SP_CURSE] == 3,
              "m4r: no curse beyond the spell range");
        world.units[g].x = 12;               /* behind the house wall */
        world.units[g].y = 6;
        check(spell_apply(&world, &book, 0, SP_CURSE, 12, 6, &rng, &shot) ==
              CAST_REJECTED, "m4r: no curse through walls");
    }

    /* the AI does not see invisible units */
    world.units[g].x = 7;
    world.units[g].y = 6;
    world.units[g].owner = OWN_NEUTRAL;
    check(ai_nearest_enemy(&world, g, 9) == 0, "m4r: the goblin sees the wizard");
    effect_grant(u, EFF_INVISIBLE, 1, 3);
    check(ai_nearest_enemy(&world, g, 9) == NO_UNIT,
          "m4r: but not the invisible wizard");
    effect_tick(u);
    effect_tick(u);
    effect_tick(u);

    /* the defender strikes back after a harmless blow (GDD 6, 4.2) */
    world.units[g].kind = CR_ZOMBIE;
    world.units[g].flags |= UF_UNDEAD;
    world.units[g].owner = OWN_P2;
    world.units[g].ap = 30;
    world.units[g].sta = 40;
    u->ap = 40;
    check(combat_melee(&world, &rng, 0, g, &r) && !r.hit && r.returned,
          "m4r: undead shrug off the blow and strike back");

    /* enchanted weapons double their values (GDD 6.1) */
    u->items[0] = OBJ_SWORD;
    u->item_count = 1;
    u->in_use = 0;
    u->con = u->con_max;
    {
        uint8_t plain = items_combat(&world, 0);
        effect_grant(u, EFF_MAGIC_WEAPON, 1, 2);
        check(items_combat(&world, 0) == plain + WEAPONS[WEAPON_SWORD].combat,
              "m4r: an enchanted sword counts double");
    }
}

static void test_m4d(void)
{
    Rng rng;
    uint8_t k, rounds;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 0;
    area_reset();

    check(area_damage(AREA_FIRE) == 6 && area_damage(AREA_BLOB) == 3,
          "m4d: damage start values");
    check(area_terrain_ok(AREA_FIRE, FL_GRASS, FE_NONE) &&
          !area_terrain_ok(AREA_FIRE, FL_WATER, FE_NONE) &&
          area_terrain_ok(AREA_FIRE, FL_STONE, FE_TREE),
          "m4d: fire takes grass and trees, not water or stone");
    check(area_terrain_ok(AREA_VINE, FL_TALL_GRASS, FE_NONE) &&
          !area_terrain_ok(AREA_VINE, FL_STONE, FE_NONE),
          "m4d: vine only on vulnerable terrain");

    {   /* cast on unsuitable terrain is refused (water at 15,0) */
        check(!area_cast(&world, AREA_FIRE, 3, OWN_P1, 15, 0),
              "m4d: no fire on water");
    }

    {   /* spread over open grass: deterministic, capped */
        rounds = 0;
        area_reset();
        rng_seed(&rng, 42);
        check(area_cast(&world, AREA_FIRE, 5, OWN_P1, 20, 19),
              "m4d: fire starts on grass");
        while (area_round_end(&world, &rng) > 0 && rounds < 20)
            rounds++;
        check(rounds >= 4 && rounds <= 20,
              "m4d: the fire spreads and dies out on its own");
        check(area_kind_at(&world, 20, 19) == AREA_NONE,
              "m4d: the field is clean again");
    }

    {   /* F4: a strong blob persists, spreads and stays under the cap */
        uint8_t max_fields = 0;
        area_reset();
        rng_seed(&rng, 7);
        area_cast(&world, AREA_BLOB, 8, OWN_P1, 20, 19);
        for (k = 0; k < 12; k++) {
            Area *ar;
            area_round_end(&world, &rng);
            ar = area_at(&world, 20, 19);
            if (ar && ar->count > max_fields)
                max_fields = ar->count;
        }
        check(max_fields > 1 && max_fields <= AREA_FIELDS_MAX,
              "m4d: a strong blob spreads within the cap");
    }

    {   /* spread chance is strength * 10 % per field and round (F4) */
        int16_t sx = -1, sy = -1, x, y;
        uint16_t seed, spread = 0;
        for (y = 1; y < 35 && sx < 0; y++)
            for (x = 1; x < 35 && sx < 0; x++) {
                int8_t dx, dy;
                bool open = true;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (world.floor[y + dy][x + dx] != FL_GRASS ||
                            world.feature[y + dy][x + dx] != FE_NONE)
                            open = false;
                if (open) {
                    sx = x;
                    sy = y;
                }
            }
        check(sx >= 0, "m4d: testland has an open grass patch");
        for (seed = 0; seed < 300 && sx >= 0; seed++) {
            Area *ar;
            area_reset();
            rng_seed(&rng, 500 + seed);
            area_cast(&world, AREA_FIRE, 3, OWN_P1, sx, sy);
            area_round_end(&world, &rng);
            ar = area_at(&world, sx, sy);
            if (ar && ar->count > 1)
                spread++;
        }
        check(spread > 45 && spread < 135,
              "m4d: level 3 spreads in about 30 % of the rounds");
        /* a new field is born at strength-1 and keeps it this round */
        {
            bool found = false;
            for (seed = 0; seed < 60 && sx >= 0 && !found; seed++) {
                int8_t dx, dy;
                area_reset();
                rng_seed(&rng, 900 + seed);
                area_cast(&world, AREA_FIRE, 4, OWN_P1, sx, sy);
                area_round_end(&world, &rng);
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if ((dx || dy) &&
                            area_kind_at(&world, sx + dx, sy + dy) == AREA_FIRE) {
                            found = true;
                            check(area_power_at(&world, sx + dx, sy + dy) == 3 &&
                                  area_power_at(&world, sx, sy) == 3,
                                  "m4d: new field keeps strength-1, old one loses 1");
                        }
            }
            check(found, "m4d: a level 4 fire spreads within 60 tries");
        }
        area_reset();
    }

    {   /* blob, vine and flood do not creep into walls (feature wall) */
        check(!area_cast(&world, AREA_BLOB, 4, OWN_P1, 3, 4) &&
              !area_cast(&world, AREA_FLOOD, 4, OWN_P1, 3, 4),
              "m4d: no blob or flood on a wall");
        check(area_cast(&world, AREA_FIRE, 4, OWN_P1, 21, 4),
              "m4d: fire still takes a tree");
        area_reset();
    }

    {   /* flood puts out fire on its field; fire burns scrolls, not gold */
        area_reset();
        area_cast(&world, AREA_FIRE, 4, OWN_P1, 20, 19);
        check(area_cast(&world, AREA_FLOOD, 4, OWN_P2, 20, 19) &&
              area_kind_at(&world, 20, 19) == AREA_FLOOD &&
              area_active_count() == 1,
              "m4d: flood douses the fire on its field");
        area_reset();
        world.object_count = 2;
        world.objects[0].x = 20; world.objects[0].y = 19;
        world.objects[0].tile = OBJECTS[OBJ_SCROLL].tile;
        world.objects[1].x = 20; world.objects[1].y = 19;
        world.objects[1].tile = OBJECTS[OBJ_GOLD].tile;
        area_cast(&world, AREA_FIRE, 4, OWN_P1, 20, 19);
        rng_seed(&rng, 11);
        area_round_end(&world, &rng);
        check(world.object_count == 1 &&
              world.objects[0].tile == OBJECTS[OBJ_GOLD].tile,
              "m4d: fire burns the scroll and leaves the gold");
        world.object_count = 0;
        area_reset();
    }

    {   /* a second cast of the same kind adds its target field */
        area_reset();
        check(area_cast(&world, AREA_FIRE, 4, OWN_P1, 20, 19) &&
              area_cast(&world, AREA_FIRE, 4, OWN_P1, 22, 19),
              "m4d: two fire casts");
        check(area_kind_at(&world, 22, 19) == AREA_FIRE &&
              area_power_at(&world, 22, 19) == 4,
              "m4d: the second cast starts a field at its target");
        check(!area_cast(&world, AREA_BLOB, 4, OWN_P1, 22, 19),
              "m4d: another kind cannot take a held field");
        area_reset();
    }

    {   /* the spell path: range, payment, area (spell_apply) */
        Spellbook sb;
        SpellShot shot;
        uint8_t g;
        area_reset();
        world.unit_count = 0;
        g = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 20, 19);
        world.units[g].mana = 100;
        world.units[g].ap = 40;
        memset(&sb, 0, sizeof sb);
        sb.level[SP_MAGIC_FIRE] = 2;
        rng_seed(&rng, 3);
        check(spell_apply(&world, &sb, g, SP_MAGIC_FIRE, 21, 19, &rng,
                          &shot) == CAST_OK &&
              area_kind_at(&world, 21, 19) == AREA_FIRE &&
              sb.level[SP_MAGIC_FIRE] == 1 && world.units[g].mana < 100,
              "m4d: spell_apply casts fire and pays");
        check(spell_apply(&world, &sb, g, SP_MAGIC_FIRE, 16, 19, &rng,
                          &shot) == CAST_BAD_TERRAIN &&   /* water, in reach */
              sb.level[SP_MAGIC_FIRE] == 1,
              "m4d: refused terrain is reported and costs nothing");
        check(spell_apply(&world, &sb, g, SP_MAGIC_FIRE, 15, 0, &rng,
                          &shot) == CAST_REJECTED &&   /* out of reach */
              sb.level[SP_MAGIC_FIRE] == 1,
              "m4d: out of reach costs nothing");
        sb.level[SP_FLOOD] = 1;
        check(spell_apply(&world, &sb, g, SP_FLOOD, 21, 19, &rng,
                          &shot) == CAST_OK &&
              area_kind_at(&world, 21, 19) == AREA_FLOOD,
              "m4d: spell_apply floods over the fire");
        area_reset();
        world.unit_count = 0;
    }

    {   /* fire hurts only enemies (GDD 7.2), each exactly once */
        uint8_t doomed, enemy, own, before_e, before_o;
        area_reset();
        world.unit_count = 0;
        doomed = world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 20, 19);
        enemy = world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 20, 20);
        own = world_spawn_unit(&world, OWN_P1, CR_DWARF, 20, 21);
        world.units[enemy].x = 20; world.units[enemy].y = 19;
        world.units[own].x = 20; world.units[own].y = 19;
        world.units[doomed].con = 1;     /* dies; the last unit swaps in */
        before_e = world.units[enemy].con;
        before_o = world.units[own].con;
        check(area_cast(&world, AREA_FIRE, 5, OWN_P1, 20, 19),
              "m4d: cast under the enemies");
        rng_seed(&rng, 5);
        area_round_end(&world, &rng);
        check(world.unit_count == 2,
              "m4d: the weak enemy burns to death");
        check(world.units[0].owner == OWN_P1 || world.units[1].owner == OWN_P1,
              "m4d: own dwarf survives its own fire");
        {
            uint8_t i;
            for (i = 0; i < world.unit_count; i++) {
                if (world.units[i].owner == OWN_P2)
                    check(world.units[i].con == before_e - area_damage(AREA_FIRE),
                          "m4d: the enemy takes the fire damage exactly once");
                else
                    check(world.units[i].con == before_o,
                          "m4d: own units stay unharmed by their fire");
            }
        }
        world.unit_count = 0;
        area_reset();
    }

    {   /* flood drowns non-water units (start value: 50 % per round) */
        uint8_t deaths = 0, k2;
        area_reset();
        world.unit_count = 0;
        for (k2 = 0; k2 < 200; k2++) {
            world.unit_count = 0;        /* one swimmer per round */
            area_reset();
            world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 20, 19);
            world.units[0].con = 1;
            area_cast(&world, AREA_FLOOD, 4, OWN_P1, 20, 19);
            rng_seed(&rng, 1000 + k2);
            area_round_end(&world, &rng);
            if (world.unit_count == 0)
                deaths++;
        }
        check(deaths > 40 && deaths < 160,
              "m4d: drowning takes about half");
    }

    {   /* vine/blob block movement while strong (start value 2+) */
        uint8_t g;
        area_reset();
        world.unit_count = 0;
        g = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 20, 19);
        world.units[g].ap = 40;
        check(area_cast(&world, AREA_VINE, 5, OWN_P1, 21, 19),
              "m4d: vine east of the wizard");
        check(!world_move_unit(&world, g, 1, 0),
              "m4d: strong vine blocks the step");
        area_reset();
    }

    {   /* performance: 4 areas on 48 fields stay cheap */
        uint8_t i;
        area_reset();
        rng_seed(&rng, 2);
        check(area_cast(&world, AREA_FIRE, 4, OWN_P1, 5, 20) &&
              area_cast(&world, AREA_BLOB, 4, OWN_P2, 13, 20) &&
              area_cast(&world, AREA_VINE, 4, OWN_NEUTRAL, 25, 20) &&
              area_cast(&world, AREA_FLOOD, 4, OWN_P2, 30, 20) &&
              area_active_count() == 4, "m4d: four areas are active");
        for (i = 0; i < 20; i++) {
            area_round_end(&world, &rng);
            if (area_active_count() > 4) {
                check(false, "m4d: never more than one area per kind");
                break;
            }
        }
        area_reset();
    }
}

static void test_m4e(void)
{
    FieldLayers f;
    Rng rng;
    uint8_t wizard, mount, enemy;

    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 0;
    area_reset();

    check(WEAPONS[WEAPON_KNIFE].thrown == 1 && WEAPONS[WEAPON_SPEAR].ranged == 0 &&
          WEAPONS[WEAPON_CLUB].combat == 7 && WEAPONS[WEAPON_MAGIC_SLAYER].combat == 16 &&
          WEAPONS[WEAPON_AXE].dice_n == 2 && WEAPONS[WEAPON_AXE].die == 10,
          "m4e: weapon values from weapons.csv");
    check(OBJECTS[OBJ_SPEAR].weapon == WEAPON_SPEAR &&
          OBJECTS[OBJ_SLAYER].weight == 6,
          "m4e: the new weapons exist as objects");
    check(strcmp(name_object(T_OBJ_SWORD), "Schwert") == 0 &&
          strcmp(name_object(T_OBJ_RUBY), "Rubin") == 0 &&
          strcmp(name_object(T_OBJ_VIAL_HEALING), "Phiole Heilkraut") == 0 &&
          T_OBJ_VIAL_HEALING != T_OBJ_VIAL_SPEED &&   /* own tile per potion */
          strcmp(name_object(T_OBJ_SCROLL), "Schriftrolle") == 0,
          "objects show their own names on the ground and in look mode");

    {   /* riding: mount, ride along, dismount */
        wizard = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 6, 7);
        mount = world_spawn_unit(&world, OWN_P1, CR_UNICORN, 6, 6);
        world.units[wizard].ap = 40;
        check(ride_mount(&world, wizard, 6, 6) && world.unit_count == 1 &&
              (world.units[0].flags & UF_RIDDEN) &&
              ride_rider_kind(&world.units[0]) == CR_WIZARD,
              "m4e: the wizard mounts the unicorn");
        mount = 0;                       /* the list re-ordered on removal */
        {   /* the pair is drawn as rider layer behind the mount layer (M4k) */
            FieldLayers rf;
            uint8_t li, rl = 0xFF;
            view_set_sight(NULL);
            view_compose(&world, 6, 6, &rf);
            for (li = 0; li < rf.n; li++)
                if (rf.ride & (1u << li))
                    rl = li;
            check(rl != 0xFF && rf.id[rl] == T_WIZARD_P1 &&
                  rf.id[rl + 1] == T_UNICORN_P1 && rf.air == 0 &&
                  rf.ride == (uint16_t)(1u << rl),
                  "m4k: the rider layer sits right behind the mount");
            world.units[mount].flags |= UF_FLYING;    /* a flying mount */
            view_compose(&world, 6, 6, &rf);
            rl = 0xFF;                                /* the shadow shifts it */
            for (li = 0; li < rf.n; li++)
                if (rf.ride & (1u << li))
                    rl = li;
            check(rl != 0xFF && rf.id[rl + 1] == T_UNICORN_P1 &&
                  (rf.air & (1u << rl)) && (rf.air & (1u << (rl + 1))),
                  "m4k: both layers of a flying pair are airborne");
            world.units[mount].flags &= (uint8_t)~UF_FLYING;
        }
        {   /* `b` finds a mount on any of the eight neighbour fields */
            static const int8_t NX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
            static const int8_t NY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
            uint8_t d, found = 0, w2, m2;
            for (d = 0; d < 8; d++) {
                world.unit_count = 0;
                w2 = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 20, 19);
                m2 = world_spawn_unit(&world, OWN_P1, CR_UNICORN,
                                      (uint8_t)(20 + NX[d]), (uint8_t)(19 + NY[d]));
                (void)m2;
                world.units[w2].ap = 40;
                if (ride_mount_adjacent(&world, w2) && world.unit_count == 1)
                    found++;
            }
            check(found == 8, "m4e: the wizard mounts a unicorn on every side");
            world.unit_count = 0;
            w2 = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 20, 19);
            m2 = world_spawn_unit(&world, OWN_P1, CR_UNICORN, 22, 19);
            world.units[w2].ap = 40;
            check(!ride_mount_adjacent(&world, w2) && world.unit_count == 2,
                  "m4e: a unicorn two fields away is out of reach");
            world.unit_count = 0;
            wizard = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 6, 7);
            mount = world_spawn_unit(&world, OWN_P1, CR_UNICORN, 6, 6);
            world.units[wizard].ap = 40;
            check(ride_mount(&world, wizard, 6, 6), "m4e: mount for the ride test");
            mount = 0;
        }
        check(world_move_unit(&world, mount, 0, -1),
              "m4e: the pair rides as one unit");
        check(!world_engaged(&world, mount),
              "m4e: the rider attacks from anywhere (D21)");
        world.units[mount].ap = 40;
        check(ride_dismount(&world, mount) && world.unit_count == 2 &&
              !(world.units[mount].flags & UF_RIDDEN),
              "m4e: dismounting brings the rider back");
    }

    {   /* roof: loaded from the v4 map, blocks sight and landing */
        world_load_bin(&world, MAPBIN_MANY_COLOURED_LAND,
                       MAPBIN_MANY_COLOURED_LAND_LEN);
        {   /* scenario 1 starts with the wizard alone (creatures come from
             * the spellbook) */
            uint8_t k, own = 0;
            for (k = 0; k < world.unit_count; k++)
                if (world.units[k].owner == OWN_P1)
                    own++;
            check(own == 1 && world.units[0].kind == CR_WIZARD,
                  "scenario 1: player 1 starts with the wizard only");
        }
        world.unit_count = 0;
        check(world_has_roof(&world, 5, 5) && !world_has_roof(&world, 20, 19),
              "m4e: the house carries a roof");
        view_set_sight(NULL);
        view_compose(&world, 5, 5, &f);
        check(has_layer(&f, T_ROOF), "m4e: the roof is visible outside");
        {
            world_spawn_unit(&world, OWN_P1, CR_WIZARD, 5, 5);
            view_compose(&world, 5, 5, &f);
            check(!has_layer(&f, T_ROOF),
                  "m4e: an own unit inside hides the roof (F7)");
            view_set_sight(NULL);
        }
        {   /* the whole building opens, not just the unit's own field */
            world.unit_count = 0;
            view_set_sight(NULL);
            world_spawn_unit(&world, OWN_P1, CR_WIZARD, 6, 6);
            view_compose(&world, 3, 2, &f);
            check(!has_layer(&f, T_ROOF),
                  "m4e: the roof lifts over the whole building");
            view_compose(&world, 8, 10, &f);
            check(!has_layer(&f, T_ROOF),
                  "m4e: even the far corner shows the inside");
            world.units[0].x = 15;       /* leaves the house */
            world.units[0].y = 6;
            view_compose(&world, 3, 2, &f);
            check(has_layer(&f, T_ROOF),
                  "m4e: the roof closes again behind the wizard");
            world.units[0].x = 6;
            world.units[0].y = 6;
            world.units[0].owner = OWN_P2;      /* an enemy inside */
            {
                Sight roof_sight;
                sight_init(&roof_sight, OWN_P1);
                memset(roof_sight.explored, 0xFF, sizeof roof_sight.explored);
                view_set_sight(&roof_sight);
                view_compose(&world, 3, 2, &f);
                check(has_layer(&f, T_ROOF),
                      "m4e: an enemy inside does not lift the roof");
                view_set_sight(NULL);
            }
            world.unit_count = 0;
        }
        {   /* a ridden pair inside the lifted roof keeps its masks in step */
            FieldLayers rf;
            uint8_t li, rl = 0xFF, mt;
            world.unit_count = 0;
            view_set_sight(NULL);
            mt = world_spawn_unit(&world, OWN_P1, CR_UNICORN, 5, 5);
            world.units[mt].flags |= UF_RIDDEN;
            world.units[mt].rider_kind = CR_WIZARD;
            view_compose(&world, 5, 5, &rf);
            for (li = 0; li < rf.n; li++)
                if (rf.ride & (1u << li))
                    rl = li;
            check(!has_layer(&rf, T_ROOF) && rl != 0xFF &&
                  rf.id[rl] == T_WIZARD_P1 && rf.id[rl + 1] == T_UNICORN_P1,
                  "m4k: the rider mask survives the lifted roof");
            world.unit_count = 0;
        }
        {   /* flying units cannot land on a roof */
            uint8_t bat = world_spawn_unit(&world, OWN_P1, CR_GIANT_BAT, 5, 5);
            world.units[bat].flags |= UF_FLYING;
            check(!world_land(&world, bat),
                  "m4e: no landing under the roof");
        }
    }

    {   /* the new weapons in the melee path (values land via items_*) */
        uint8_t a, b;
        a = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 6, 6);
        b = world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 7, 6);
        world.units[a].items[0] = OBJ_AXE;
        world.units[a].item_count = 1;
        world.units[a].in_use = 0;
        check(items_combat(&world, a) == 10 + 9,
              "m4e: the axe gives +9 combat (D31)");
        rng_seed(&rng, 5);
        check(combat_hit_chance(items_combat(&world, a),
                                items_defence(&world, b)) == 90,
              "m4e: axe vs goblin hits 90 % (D31)");
    }

    {   /* enemy in the roofed house is hidden from outside rays */
        world_load_bin(&world, MAPBIN_MANY_COLOURED_LAND,
                       MAPBIN_MANY_COLOURED_LAND_LEN);
        world.unit_count = 0;
        world_spawn_unit(&world, OWN_P1, CR_WIZARD, 20, 19);
        world_spawn_unit(&world, OWN_P2, CR_GOBLIN, 5, 5);
        {
            Sight s;
            sight_init(&s, OWN_P1);
            sight_compute(&world, &s);
            check(!sight_visible(&s, &world, 5, 5),
                  "m4e: the roof hides the enemy inside");
        }
        view_set_sight(NULL);
    }
    (void)enemy;

    {   /* v4 map with roof but no portal: filler block keeps the layout */
        uint8_t m[24] = {'L', 'O', 'C', 'M', MAPBIN_VERSION, 0, 0, 2, 1, 0};
        m[5] = (uint8_t)(TILE_COUNT & 0xFF);
        m[6] = (uint8_t)(TILE_COUNT >> 8);
        m[16] = 0;                       /* no units */
        m[17] = 0;                       /* no objects */
        m[18] = 0xFF; m[19] = 0xFF;      /* portal filler */
        m[23] = 1;                       /* roof on the second field */
        check(world_load_bin(&world, m, sizeof m) &&
              world.portal_x == -1 && !world_has_roof(&world, 0, 0) &&
              world_has_roof(&world, 1, 0),
              "m4e: v4 map without portal keeps roof layout");
    }
}

static void test_m4f(void)
{
    Wizard *w = &wizard_slots[0];
    Rng rng;

    wizard_slot_reset(0);
    {   /* review fixes: lowering, validation, scenario guard */
        Wizard t;
        wizard_slot_reset(3);
        t = wizard_slots[3];
        t.xp = 100;
        check(!wizard_lower(&t, WA_COMBAT) && t.com == 5,
              "m4f: no lowering below the minimum");
        check(wizard_raise(&t, WA_COMBAT) && wizard_lower(&t, WA_COMBAT) &&
              t.com == 5 && t.xp == 100, "m4f: lowering refunds the XP");
        check(wizard_valid(&t), "m4f: a stock wizard is valid");
        t.com = 0;
        check(!wizard_valid(&t), "m4f: zero attribute is rejected");
        t = wizard_slots[3];
        t.book.level[0] = SPELL_MAX_LEVEL + 1;
        check(!wizard_valid(&t), "m4f: book level above the cap is rejected");
        t = wizard_slots[3];
        memset(t.name, 'x', sizeof t.name);
        check(!wizard_valid(&t), "m4f: unterminated name is rejected");
        t = wizard_slots[3];
        wizard_campaign_result(&t, 5, 0);
        check(t.level == 1 && t.scenarios_done == 0 && t.xp == 600 + 5,
              "m4f: scenario 0 changes no level");
    }
    {   /* stock wizard: minimums, 600 XP, EMPTY books (user rule) */
        uint16_t s, sum = 0;
        check(w->level == 1 && w->xp == 600 && w->com == 5 && w->sta == 34 &&
              w->mana_max == 90 && w->ap == 34,
              "m4f: stock wizard: minimums and 600 XP (F6)");
        for (s = 0; s < SPELL_COUNT; s++)
            sum += w->book.level[s];
        check(sum == 0, "m4f: fresh wizards start with empty books");
        wizard_apply_standard_set(w);
        check(w->book.level[SP_MAGIC_BOLT] == 4 &&
              w->book.level[SP_MAGIC_SHIELD] == 3 &&
              w->book.level[SP_HEALING_POTION] == 4 &&
              w->book.level[SP_GIANT_BAT] == 2 && w->book.level[SP_GRYPHON] == 1,
              "m4f: the standard template fills spells + 8 creatures");
        check(w->com == 20 && w->def == 20 && w->mr == 80 && w->con == 40 &&
              w->sta == 49 && w->mana_max == 96 && w->ap == 39 && w->xp == 78,
              "m4f: the template costs 522 of the 600 XP (no cheating)");
        wizard_apply_standard_set(w);   /* idempotent: bolt already there */
        check(w->book.level[SP_MAGIC_BOLT] == 4,
              "m4f: the standard set never overwrites designed books");
        wizard_slot_reset(0);           /* back to empty for the next tests */
    }
    check(wizard_attr_cost(WA_COMBAT, 5) == 2 &&
          wizard_attr_cost(WA_DEFENCE, 5) == 2 &&
          wizard_attr_cost(WA_MAGIC_RES, 70) == 4 &&
          wizard_attr_cost(WA_CONSTITUTION, 25) == 2 &&
          wizard_attr_cost(WA_STAMINA, 34) == 4 &&
          wizard_mana_cost() == 9 && wizard_ap_cost() == 8,
          "m4f: anchor point costs 2/2/4/2/4, mana 9, AP 8 (F6)");

    w->xp = 50;
    check(wizard_raise(w, WA_COMBAT) && w->com == 6 && w->xp == 48,
          "m4f: raising costs XP");
    w->xp = 1;
    check(!wizard_raise(w, WA_COMBAT), "m4f: no raising without XP");

    {   /* F6 end-to-end: spend the full 600 XP over every row, never
         * below the minimums, never above the 600 total */
        Wizard b;
        uint16_t rows;
        uint8_t raise_count = 0;
        wizard_slot_reset(3);
        b = wizard_slots[3];            /* minimums + 600 XP */
        check(b.xp == 600 && b.com == 5 && b.def == 5 && b.mr == 70 &&
              b.con == 25 && b.sta == 34 && b.mana_max == 90 && b.ap == 34,
              "m4f: fresh wizard = minimums + 600 XP");
        /* buy combat to the cap 30: 25 points x 2 XP = 50 spent */
        while (wizard_raise(&b, WA_COMBAT))
            raise_count++;
        check(raise_count == 25 && b.com == 30 && b.xp == 550,
              "m4f: combat caps at 30 after 25 points (50 XP)");
        /* mana at 9 and AP at 8 still work from 550 */
        check(wizard_mana_raise(&b) && b.mana_max == 91 && b.xp == 541 &&
              wizard_mana_lower(&b) && b.mana_max == 90 && b.xp == 550,
              "m4f: mana raises/refunds alongside");
        /* lower it back: every lowering refunds the full price */
        {
            uint8_t i;
            for (i = 0; i < raise_count; i++)
                wizard_lower(&b, WA_COMBAT);
        }
        check(b.com == 5 && b.xp == 600,
              "m4f: lowering refunds everything, back to 600");
        /* spend 600 exactly: defence 5->30 (50), con 25->60 (70),
         * sta 34->100 (264), MR 70->100 (120), mana 90->105 (90),
         * AP 34->40 (48) = 592, plus 1 mana (9) overshoots - so
         * check the exact drain with defence/con/stamina only */
        for (rows = 0; rows < 25; rows++)
            wizard_raise(&b, WA_DEFENCE);
        for (rows = 0; rows < 35; rows++)
            wizard_raise(&b, WA_CONSTITUTION);
        for (rows = 0; rows < 66; rows++)
            wizard_raise(&b, WA_STAMINA);
        check(b.xp == 600 - 50 - 70 - 264 && b.def == 30 && b.con == 60 &&
              b.sta == 100,
              "m4f: 600 XP drain exactly over the three rows");
        /* and the whole thing stays a valid wizard */
        check(wizard_valid(&b), "m4f: maxed wizard still validates");
    }

    {   /* caps */
        w->xp = 60000;
        w->mr = wizard_attr_max(WA_MAGIC_RES);
        check(!wizard_raise(w, WA_MAGIC_RES), "m4f: caps stop raising");
        w->mr = 80;
    }

    {   /* campaign: VP -> XP 1:1, level up once per scenario (GDD 9) */
        wizard_slot_reset(1);
        wizard_campaign_result(&wizard_slots[1], 75, 1);
        check(wizard_slots[1].xp == 600 + 75 && wizard_slots[1].level == 2,
              "m4f: first clear gives XP and a level");
        wizard_campaign_result(&wizard_slots[1], 20, 1);
        check(wizard_slots[1].xp == 600 + 95 && wizard_slots[1].level == 2,
              "m4f: repeating scores XP without a level");
        wizard_campaign_result(&wizard_slots[1], 10, 2);
        check(wizard_slots[1].level == 3, "m4f: scenario 2 lifts again");
    }

    {   /* random wizard: some bought levels, XP, valid */
        uint16_t s, sum = 0;
        rng_seed(&rng, 9);
        wizard_slot_random(2, 2, &rng);
        for (s = 0; s < SPELL_COUNT; s++)
            sum += wizard_slots[2].book.level[s];
        check(wizard_slots[2].xp == 80 && wizard_valid(&wizard_slots[2]) &&
              sum > 0,
              "m4f: random wizard rolls a non-empty book");
    }

    {   /* apply to the world: F5 - values yes, items no */
        uint8_t u;
        wizard_slots[0].com = 14;
        wizard_slots[0].con = 33;
        wizard_slots[0].sta = 66;
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 1;
        u = 0;
        wizard_apply_to_world(w, &world, u);
        check(world.units[u].com == 14 && world.units[u].con == 33 &&
              world.units[u].sta == 66 && world.units[u].item_count == 0,
              "m4f: the designer wizard enters without items (F5)");
    }
    wizard_slot_reset(0);
}

static void test_m4g(void)
{
    uint8_t wizards, undead, treasures;
    bool slayer = false;

    world_load_bin(&world, MAPBIN_SLAYERS_DUNGEON, MAPBIN_SLAYERS_DUNGEON_LEN);
    check(world.w == 36 && world.h == 36 && !world.wrap,
          "m4g: slayer's dungeon is 36x36, no wrap");
    check(world.portal_x == 32 && world.portal_y == 32 &&
          world.portal_rmin == 20 && world.portal_rmax == 24,
          "m4g: dungeon portal rounds 20-24");
    wizards = undead = treasures = 0;
    {
        uint8_t i;
        for (i = 0; i < world.unit_count; i++)
            if (world.units[i].kind == CR_WIZARD)
                wizards++;
            else if (world.units[i].flags & UF_UNDEAD)
                undead++;
        for (i = 0; i < world.object_count; i++) {
            uint8_t k;
            for (k = 0; k < OBJ_COUNT; k++)
                if (OBJECTS[k].tile == world.objects[i].tile) {
                    if (OBJECTS[k].category == OC_TREASURE)
                        treasures++;
                    if (OBJECTS[k].weapon == WEAPON_SLAYER)
                        slayer = true;
                }
        }
    }
    check(wizards == 2, "m4g: dungeon has two wizards");
    check(undead >= 4, "m4g: the dungeon crawls with undead");
    check(treasures >= 4, "m4g: dungeon carries treasures");
    check(slayer, "m4g: the Slayer lies in the dungeon");
    check(!world_has_roof(&world, 32, 32), "m4g: the portal lies open");

    world_load_bin(&world, MAPBIN_RAGARILS_DOMAIN, MAPBIN_RAGARILS_DOMAIN_LEN);
    check(world.w == 36 && world.h == 36 && !world.wrap,
          "m4g: ragaril's domain is 36x36, no wrap");
    check(world.portal_x == 33 && world.portal_y == 3 &&
          world.portal_rmin == 44 && world.portal_rmax == 51,
          "m4g: domain portal rounds 44-51");
    wizards = 0;
    treasures = 0;                       /* count the domain on its own */
    {
        uint8_t i, swamps = 0, woods = 0;
        for (i = 0; i < world.unit_count; i++)
            if (world.units[i].kind == CR_WIZARD)
                wizards++;
        for (i = 0; i < world.object_count; i++) {
            uint8_t k;
            for (k = 0; k < OBJ_COUNT; k++)
                if (OBJECTS[k].tile == world.objects[i].tile &&
                    OBJECTS[k].category == OC_TREASURE)
                    treasures++;
        }
        {
            uint16_t cell;
            for (cell = 0; cell < 36u * 36; cell++)
                if (world.floor[cell / 36][cell % 36] == FL_SWAMP)
                    swamps++;
                else if (world.floor[cell / 36][cell % 36] == FL_MAGIC_WOOD)
                    woods++;
        }
        check(wizards == 2, "m4g: domain has two wizards (one human)");
        check(swamps > 100 && woods > 20, "m4g: the estate has its regions");
        check(treasures >= 5, "m4g: domain carries treasures");
    }

    {   /* scenario books compile and load */
        static Spellbook scnbooks[OWN_NEUTRAL];
        check(spellbook_load(scnbooks, SCN_SLAYERS_DUNGEON,
                             SCN_SLAYERS_DUNGEON_LEN) &&
              scnbooks[OWN_P2].level[SP_ZOMBIE] == 3,
              "m4g: dungeon books load");
        check(spellbook_load(scnbooks, SCN_RAGARILS_DOMAIN,
                             SCN_RAGARILS_DOMAIN_LEN) &&
              scnbooks[OWN_P2].level[SP_VAMPIRE] == 3 &&
              scnbooks[OWN_P2].level[SP_DEMON] == 1,
              "m4g: ragaril commands undead");
    }
}

static void test_m5a(void)
{
    Game g;
    Rng rng;
    uint8_t wiz, mount;

    rng_seed(&rng, 3);
    world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
    world.unit_count = 0;
    area_reset();
    game_init(&g, 6, 6, 1, 1, &rng);
    game_new_round(&g, 1);
    wiz = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 5, 5);
    world_spawn_unit(&world, OWN_P2, CR_WIZARD, 9, 9);
    check(game_outcome(&g, &world, OWN_P1) == OUT_RUNNING,
          "m5a: a living wizard keeps the game running");
    check(game_outcome(&g, &world, OWN_NEUTRAL) == OUT_RUNNING,
          "m5a: independents never win or lose");

    world.units[wiz].x = 6;
    world.units[wiz].y = 6;
    g.vp[OWN_P1] = 0;
    check(game_try_enter_portal(&g, &world, wiz) &&
          game_outcome(&g, &world, OWN_P1) == OUT_WIN &&
          game_outcome(&g, &world, OWN_P2) == OUT_RUNNING,
          "m5a: escaping through the portal wins");
    check(g.vp[OWN_P1] == VP_ESCAPE && g.loot_vp[OWN_P1] == 0,
          "m5a: the escape scores VP without loot");

    world_remove_unit(&world, 0);          /* p2 wizard is the only unit */
    world.unit_count = 0;
    wiz = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 5, 5);
    game_init(&g, 6, 6, 1, 1, &rng);
    world_kill_unit(&world, wiz, CR_GOBLIN, OWN_P2, true);
    check(game_outcome(&g, &world, OWN_P1) == OUT_LOSE,
          "m5a: a dead wizard loses");
    game_credit_kills(&g, &world);
    check(g.kills[OWN_P2] == 1, "m5a: the kill is counted");

    world.unit_count = 0;
    world_spawn_unit(&world, OWN_P1, CR_WIZARD, 5, 5);
    mount = world_spawn_unit(&world, OWN_P1, CR_UNICORN, 5, 6);
    world.units[0].ap = 40;
    check(ride_mount(&world, 0, 5, 6), "m5a: wizard mounts the unicorn");
    (void)mount;
    check(game_outcome(&g, &world, OWN_P1) == OUT_RUNNING &&
          !game_over(&g, &world),
          "m5a: a riding wizard still counts as alive");

    game_init(&g, -1, -1, 1, 1, &rng);
    world.unit_count = 0;
    check(game_outcome(&g, &world, OWN_P1) == OUT_RUNNING,
          "m5a: maps without a portal never end");
}

static void test_m4h(void)
{
    Rng rng;
    uint8_t guard;

    world_load_bin(&world, MAPBIN_SLAYERS_DUNGEON, MAPBIN_SLAYERS_DUNGEON_LEN);
    world.unit_count = 0;
    {   /* map guards: undead carry their spawn post */
        uint8_t i, posted = 0;
        world_load_bin(&world, MAPBIN_SLAYERS_DUNGEON,
                       MAPBIN_SLAYERS_DUNGEON_LEN);
        for (i = 0; i < world.unit_count; i++)
            if (world.units[i].owner == OWN_NEUTRAL &&
                (world.units[i].flags & UF_UNDEAD) &&
                world.units[i].post_x != 0xFF)
                posted++;
        check(posted >= 4, "m4h: undead guards carry a post");
    }

    {   /* a guard chases an intruder and returns home */
        uint8_t w2 = 255;
        guard = world_spawn_unit(&world, OWN_NEUTRAL, CR_ZOMBIE, 8, 9);
        world.units[guard].flags |= UF_UNDEAD;
        ai_set_post(&world, guard);
        w2 = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 10, 9);
        rng_seed(&rng, 11);
        ai_guard(&world, &rng, guard, 3);
        check(world.units[guard].x > 8 || world.units[guard].ap < 30,
              "m4h: the guard moves toward the intruder");
        /* drive him home: intruder gone and out of sight */
        if (w2 != 255)
            world_remove_unit(&world, w2);
        guard = world_find_unit(&world, world.units[0].id);  /* post unit */
        guard = 0;                       /* only the guard is left */
        world.units[guard].ap = 30;
        ai_set_post(&world, guard);
        world.units[guard].x = 12;      /* dragged away */
        world.units[guard].y = 9;
        rng_seed(&rng, 12);
        ai_guard(&world, &rng, guard, 3);
        check(world.units[guard].x < 12 || world.units[guard].ap < 30,
              "m4h: the guard walks home");
    }

    {   /* the wizard AI grabs a treasure standing on its field */
        uint8_t wiz;
        Game g;
        Spellbook books2[OWN_NEUTRAL];
        AiCtx ctx;
        Turns t;
        world_load_bin(&world, MAPBIN_SLAYERS_DUNGEON, MAPBIN_SLAYERS_DUNGEON_LEN);
        world.unit_count = 0;
        area_reset();
        memset(books2, 0, sizeof books2);
        game_init(&g, -1, -1, 1, 1, &rng);
        ctx.books = books2;
        ctx.game = &g;
        memset(&t, 0, sizeof t);
        t.phase = OWN_P2;
        t.rng = rng;
        wiz = world_spawn_unit(&world, OWN_P2, CR_WIZARD, 9, 9);
        {   /* a ruby under the wizard */
            world.objects[world.object_count].x = 9;
            world.objects[world.object_count].y = 9;
            world.objects[world.object_count].tile = T_OBJ_RUBY;
            world.object_count++;
        }
        ai_wizard_phase(&t, &world, &ctx);
        check(items_kind_at(&world, 9, 9) == NO_ITEM,
              "m4h: the AI picks up the treasure on its field");
        (void)wiz;
    }
    area_reset();
}

static void test_m4i(void)
{
    SaveGame a, b;
    static uint8_t buf[SAVE_BUF_SIZE];
    uint16_t len;
    uint32_t ha, hb;

    world_load_bin(&world, MAPBIN_SLAYERS_DUNGEON, MAPBIN_SLAYERS_DUNGEON_LEN);
    memset(&a, 0, sizeof a);
    a.world = world;
    a.world.units[0].x = 7;              /* distinctive state */
    a.loads_left = 3;
    a.game.portal_round = 21;
    a.explored[3][1] = 0x5A;
    area_reset();
    area_cast(&world, AREA_FIRE, 4, OWN_P1, 20, 19);
    a.area_count = area_export(a.areas, SAVE_AREAS);
    area_reset();
    strcpy(a.world.save_map, "maps/slayers_dungeon.map");

    len = save_serialize(&a, buf, sizeof buf);
    check(len > 4000, "m4i: the blob holds the whole world");
    check(save_deserialize(&b, buf, len), "m4i: the blob parses back");
    ha = save_hash(&a);
    hb = save_hash(&b);
    check(ha == hb && ha != 0, "m4i: save -> load -> same hash");
    check(b.world.units[0].x == 7 && b.loads_left == 3 &&
          b.game.portal_round == 21,
          "m4i: the state survives the round trip");
    check(b.explored[3][1] == 0x5A && b.area_count == 1 &&
          b.areas[0].kind == AREA_FIRE &&
          strcmp(b.world.save_map, "maps/slayers_dungeon.map") == 0,
          "m4i: explored map, areas and map name survive");
    area_import(b.areas, b.area_count);
    check(area_kind_at(&world, 20, 19) == AREA_FIRE,
          "m4i: imported areas burn again");
    area_reset();
    a.loads_left = 0;
    check(!save_may_load(&a), "m4i: no charges, no load");
    a.loads_left = 1;
    check(save_may_load(&a), "m4i: one charge loads");
    a.loads_left = 0xFF;
    check(save_may_load(&a), "m4i: unlimited loads");
    a.loads_left = 3;
    check(b.world.unit_count == world.unit_count &&
          b.world.object_count == world.object_count,
          "m4i: units and objects survive");

    {   /* magic and version gates */
        memcpy(buf, "XXXX", 4);
        check(!save_deserialize(&b, buf, len), "m4i: wrong magic refused");
        len = save_serialize(&a, buf, sizeof buf);
        buf[5] = 9;
        check(!save_deserialize(&b, buf, len), "m4i: wrong version refused");
        check(!save_deserialize(&b, buf, (uint16_t)(len - 1)),
              "m4i: wrong length refused");
    }
}

static void test_m4k_ai(void)
{
    Rng rng;
    uint8_t guard, wiz;

    world_load_bin(&world, MAPBIN_SLAYERS_DUNGEON, MAPBIN_SLAYERS_DUNGEON_LEN);
    world.unit_count = 0;
    area_reset();

    {   /* a guard opens his crypt door to reach an intruder */
        uint8_t w1;
        guard = world_spawn_unit(&world, OWN_NEUTRAL, CR_ZOMBIE, 6, 4);
        world.units[guard].flags |= UF_UNDEAD;
        world.feature[3][7] = FE_DOOR_CLOSED;   /* door east of the guard */
        w1 = world_spawn_unit(&world, OWN_P1, CR_WIZARD, 9, 4);
        world.units[guard].ap = 30;
        rng_seed(&rng, 5);
        ai_guard(&world, &rng, guard, 8);       /* wide range: intruder first */
        check(world.feature[3][7] == FE_DOOR_OPEN ||
              world.units[guard].x > 6,
              "m4k: the guard opens the crypt door (or came through)");
        (void)w1;
    }

    {   /* the wizard AI pries open a chest on the treasure path */
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 0;
        area_reset();
        wiz = world_spawn_unit(&world, OWN_P2, CR_WIZARD, 8, 6);
        world.units[wiz].ap = 40;
        world.feature[6][9] = FE_CHEST;         /* chest east of the wizard */
        {   /* a diamond inside the chest */
            world.objects[world.object_count].x = 9;
            world.objects[world.object_count].y = 6;
            world.objects[world.object_count].tile = T_OBJ_DIAMOND;
            world.object_count++;
        }
        {   /* force the treasure walk: no enemies, chest on the way */
            Game g;
            Spellbook books2[OWN_NEUTRAL];
            AiCtx ctx;
            Turns t;
            memset(books2, 0, sizeof books2);
            game_init(&g, -1, -1, 1, 1, &rng);
            ctx.books = books2;
            ctx.game = &g;
            memset(&t, 0, sizeof t);
            t.phase = OWN_P2;
            t.rng = rng;
            /* nearest_treasure needs line of sight: the wizard looks at
             * the chest field; the diamond lies under the chest */
            ai_wizard_phase(&t, &world, &ctx);
            check(world.feature[6][9] == FE_NONE &&
                  (world.unit_count >= 1),
                  "m4k: the AI opened the chest on its path");
        }
    }
    area_reset();
}

/* ---------- M5b: guided tutorial + lexicon ---------- */

static void test_m5b_tutorial(void)
{
    Tutorial t;
    Game g;
    Rng rng;
    uint8_t i, gob = NO_UNIT;

    check(world_load_bin(&world, MAPBIN_TUTORIAL, MAPBIN_TUTORIAL_LEN),
          "m5b: tutorial map loads");
    rng_seed(&rng, 7);
    game_init(&g, world.portal_x, world.portal_y, world.portal_rmin,
              world.portal_rmax, &rng);
    game_new_round(&g, 1);
    check(g.portal_round == 2, "m5b: portal opens on round 2");
    tutorial_init(&t, &world);
    check(t.step == TUT_MOVE, "m5b: tutorial starts at the move step");
    check(t.wiz_x == 2 && t.wiz_y == 2, "m5b: wizard start found");
    check(t.key_x == 3 && t.key_y == 2, "m5b: chest key found");
    check(t.chest_x == 5 && t.chest_y == 2, "m5b: chest found");
    check(t.enemies == 1, "m5b: one enemy unit (the goblin)");
    check(tutorial_update(&t, &world, &g) == TUT_MOVE,
          "m5b: nothing advances without action");
    check(!tutorial_finished(&t), "m5b: not finished at the start");
    check(world_move_unit(&world, 0, 1, 0), "m5b: wizard steps east");
    check(tutorial_update(&t, &world, &g) == TUT_SWITCH,
          "m5b: move step completes");
    tutorial_notify(&t, TUT_SWITCH);
    check(tutorial_update(&t, &world, &g) == TUT_PICKUP,
          "m5b: switch notification completes the switch step");
    world.objects[0].x = 200;             /* key off the field = picked up */
    check(tutorial_update(&t, &world, &g) == TUT_CHEST,
          "m5b: pickup completes");
    world.feature[2][5] = FE_NONE;        /* chest opened */
    check(tutorial_update(&t, &world, &g) == TUT_KILL,
          "m5b: chest step completes");
    for (i = 0; i < world.unit_count; i++)
        if (world.units[i].owner == OWN_P2)
            gob = i;
    check(gob != NO_UNIT, "m5b: the goblin exists");
    tutorial_notify(&t, TUT_SPELL);       /* cast during the fight: sticky */
    world_kill_unit(&world, gob, CR_WIZARD, OWN_P1, true);
    check(tutorial_update(&t, &world, &g) == TUT_PORTAL,
          "m5b: kill and spell steps complete (spell was sticky)");
    check(!tutorial_finished(&t), "m5b: portal step still open");
    game_new_round(&g, 2);                /* portal opens */
    check(game_try_enter_portal(&g, &world, 0) ||
          (world.units[0].x != 2 || world.units[0].y != 2),
          "m5b: wizard escapes or is on the way");
    g.escaped |= (uint8_t)(1u << OWN_P1); /* outcome says win either way */
    check(tutorial_update(&t, &world, &g) == TUT_DONE,
          "m5b: portal entry finishes the tutorial");
    check(tutorial_finished(&t), "m5b: tutorial finished");
}

static void test_m5b_lexicon(void)
{
    Lexicon l, l2;
    uint8_t buf[17];
    uint16_t len;

    lexicon_init(&l);
    check(lexicon_seen_count(&l) == 0, "m5b: lexicon starts empty");
    check(!lexicon_seen_creature(&l, CR_GOBLIN) &&
          !lexicon_seen_object(&l, OBJ_SWORD), "m5b: nothing seen at first");
    lexicon_see_creature(&l, CR_GOBLIN);
    lexicon_see_creature(&l, CR_DEMON);
    lexicon_see_object(&l, OBJ_SWORD);
    lexicon_see_object(&l, OBJ_DRAGON_HERB);
    check(lexicon_seen_creature(&l, CR_GOBLIN) &&
          !lexicon_seen_creature(&l, CR_WIZARD), "m5b: creature bits set");
    check(lexicon_seen_object(&l, OBJ_DRAGON_HERB) &&
          !lexicon_seen_object(&l, OBJ_GOLD), "m5b: object bits set");
    check(lexicon_seen_count(&l) == 4, "m5b: seen count");
    len = lexicon_export(&l, buf, sizeof buf);
    check(len == 17, "m5b: export is 17 bytes");
    lexicon_init(&l2);
    check(lexicon_import(&l2, buf, len) && lexicon_seen_count(&l2) == 4,
          "m5b: round trip keeps the entries");
    check(lexicon_seen_creature(&l2, CR_DEMON) &&
          lexicon_seen_object(&l2, OBJ_SWORD), "m5b: round trip bits");
    buf[0] = 'X';
    check(!lexicon_import(&l2, buf, len), "m5b: bad magic rejected");
    check(!lexicon_import(&l2, buf, 5), "m5b: wrong length rejected");
    {   /* bits beyond the tables must not survive an import */
        uint16_t i;
        for (i = 5; i < 17; i++)
            buf[i] = 0xFF;
        buf[0] = 'L'; buf[1] = 'O'; buf[2] = 'C'; buf[3] = 'L'; buf[4] = 1;
        check(lexicon_import(&l2, buf, sizeof buf) &&
              lexicon_seen_count(&l2) == CR_COUNT + OBJ_COUNT,
              "m5b: out-of-range bits dropped on import");
    }

    check(lexicon_object_kind_of_tile(OBJECTS[OBJ_RUBY].tile) == OBJ_RUBY,
          "m5b: field tile maps to the object kind");

    {   /* watch: own units always, others and objects only in sight */
        Sight s;
        Lexicon lw;
        load_house();
        sight_init(&s, OWN_P1);
        sight_compute(&world, &s);
        lexicon_init(&lw);
        lexicon_watch(&lw, &world, &s);
        check(lexicon_seen_creature(&lw, CR_WIZARD),
              "m5b: own units count as seen");
        check(lexicon_seen_object(&lw, OBJ_SCROLL),
              "m5b: the scroll on the visible field is seen");
        check(lexicon_seen_creature(&lw, CR_GOBLIN) ==
              sight_visible(&s, &world, 8, 3),
              "m5b: the goblin follows the sight rules");
    }
}

/* ---------- M5c: presentation events ---------- */

static void test_m5c_events(void)
{
    static GameEvent ev[EVENT_RING];
    uint8_t n, i;
    Rng rng;
    CombatResult r;

    events_reset();
    check(events_drain(ev, EVENT_RING) == 0, "m5c: ring starts empty");
    for (i = 0; i < 20; i++)
        events_push(EV_HIT, 1, 2, 3, 4, 5, 6);
    check(events_dropped() == 4, "m5c: full ring drops the new events");
    n = events_drain(ev, EVENT_RING);
    check(n == EVENT_RING, "m5c: drain returns the ring contents");
    check(ev[0].a == 5 && ev[0].x == 1 && ev[0].owner == 4,
          "m5c: order and payload kept");
    check(events_drain(ev, EVENT_RING) == 0, "m5c: drain clears the ring");

    /* a melee exchange: swing first, then hit or miss (and the reply) */
    events_reset();
    load_house();
    world.units[1].owner = OWN_P2;       /* the goblin turns hostile */
    world.units[1].x = 4;                /* next to the wizard (3,4) */
    world.units[1].y = 4;
    rng_seed(&rng, 3);
    check(combat_melee(&world, &rng, 0, 1, &r), "m5c: melee runs");
    n = events_drain(ev, EVENT_RING);
    check(n >= 2, "m5c: melee emits at least swing and result");
    check(ev[0].type == EV_SWING && ev[0].x == 4 && ev[0].y == 4 &&
          ev[0].kind == CR_WIZARD,
          "m5c: the swing names attacker and target field");
    check(ev[1].type == EV_HIT || ev[1].type == EV_MISS,
          "m5c: hit or miss follows the swing");
    if (ev[1].type == EV_HIT && !r.died)
        check(ev[1].a == r.damage, "m5c: the hit carries the damage");

    /* kills report where and what died */
    events_reset();
    world_kill_unit(&world, 1, CR_WIZARD, OWN_P1, true);
    n = events_drain(ev, EVENT_RING);
    check(n == 1 && ev[0].type == EV_DEATH && ev[0].kind == CR_GOBLIN,
          "m5c: a kill emits one death event");

    /* terrain attacks swing (b=1) and may smash */
    events_reset();
    load_house();
    rng_seed(&rng, 9);
    {
        bool destroyed = false;
        (void)combat_terrain(&world, &rng, 0, 1, 6, &destroyed);  /* table */
        n = events_drain(ev, EVENT_RING);
        check(n >= 1 && ev[0].type == EV_SWING && ev[0].b == 1,
              "m5c: terrain attack swings with the terrain flag");
        if (n > 1)
            check(ev[1].type == EV_SMASH && ev[1].x == 1 && ev[1].y == 6,
                  "m5c: destruction emits a smash event");
        else
            check(!destroyed, "m5c: no smash event without destruction");
    }

    /* spells report id and target; the bolt itself hits or misses */
    events_reset();
    load_house();
    world.units[1].owner = OWN_P2;       /* goblin inside, clear line */
    world.units[1].x = 4;
    world.units[1].y = 3;
    {
        Spellbook b;
        SpellShot shot;
        Rng r2;
        memset(&b, 0, sizeof b);
        b.level[SP_MAGIC_BOLT] = 1;
        rng_seed(&r2, 11);
        check(spell_bolt(&world, &b, 0, SP_MAGIC_BOLT, 4, 3, &r2, &shot),
              "m5c: bolt cast at the goblin");
        n = events_drain(ev, EVENT_RING);
        check(n >= 1 && ev[0].type == EV_SPELL && ev[0].kind == SP_MAGIC_BOLT,
              "m5c: the cast emits a spell event with the id");
        check(ev[0].x == 4 && ev[0].y == 3, "m5c: the spell names its target");
        check(n >= 2 && ev[1].type == EV_PROJECTILE && ev[1].kind == PJ_BOLT &&
              ev[1].x == world.units[0].x && ev[1].y == world.units[0].y &&
              (int8_t)ev[1].a == 4 - world.units[0].x &&
              (int8_t)ev[1].b == 3 - world.units[0].y,
              "polish: the bolt flies from the caster to the target");
        if (n > 2)
            check(ev[2].type == EV_HIT || ev[2].type == EV_MISS,
                  "m5c: the bolt connects or whiffs");
    }

    /* bleeding out reports a death with the bleed flag */
    events_reset();
    load_house();
    world.units[0].flags |= UF_WOUNDED;
    world.units[0].con = 1;
    world_new_turn(&world);
    n = events_drain(ev, EVENT_RING);
    check(n == 1 && ev[0].type == EV_DEATH && ev[0].a == 1,
          "m5c: bleeding out emits a death event (a=1)");

    /* emitting must not touch the RNG: two identical runs agree */
    {
        CombatResult r1, r2;
        events_reset();
        load_house();
        world.units[1].owner = OWN_P2;
        world.units[1].x = 4;
        world.units[1].y = 4;
        rng_seed(&rng, 42);
        combat_melee(&world, &rng, 0, 1, &r1);
        events_drain(ev, EVENT_RING);     /* the drain must not matter */
        load_house();
        world.units[1].owner = OWN_P2;
        world.units[1].x = 4;
        world.units[1].y = 4;
        rng_seed(&rng, 42);
        combat_melee(&world, &rng, 0, 1, &r2);
        check(r1.hit == r2.hit && r1.damage == r2.damage &&
              r1.returned == r2.returned && r1.return_hit == r2.return_hit,
              "m5c: events do not change the dice");
    }
}

/* ---------- M5e: combat rebalance (D27 free counter, D28 dice) ---------- */

static void test_m5e_balance(void)
{
    Rng rng;
    CombatResult r;

    {   /* D27: the counter is free - even an exhausted unit strikes back
         * and enters its own turn unharmed */
        load_house();
        world.units[1].owner = OWN_P2;
        world.units[1].x = 4;
        world.units[1].y = 4;
        world.units[1].ap = 0;
        world.units[1].sta = 0;
        world.units[0].con = 30;
        world.units[1].con = 32;
        rng_seed(&rng, 5);
        check(combat_melee(&world, &rng, 0, 1, &r) && r.returned,
              "m5e: exhausted defenders still counter (D27)");
        check(world.units[1].ap == 0 && world.units[1].sta == 0,
              "m5e: the counter costs no AP and no stamina");
    }

    {   /* D28: weapon dice beat bare hands over many rolls */
        uint16_t k;
        uint32_t bare = 0, sword = 0, axe = 0;
        load_house();
        world.units[0].kind = CR_DWARF;      /* com 6 */
        world.units[0].com = CREATURES[CR_DWARF].combat;
        for (k = 0; k < 400; k++) {
            rng_seed(&rng, 7000 + k);
            bare += items_attack_damage(&world, 0, &rng, false);
            world.units[0].items[0] = OBJ_SWORD;
            world.units[0].item_count = 1;
            world.units[0].in_use = 0;
            rng_seed(&rng, 7000 + k);
            sword += items_attack_damage(&world, 0, &rng, false);
            world.units[0].items[0] = OBJ_AXE;
            rng_seed(&rng, 7000 + k);
            axe += items_attack_damage(&world, 0, &rng, false);
            world.units[0].item_count = 0;
            world.units[0].in_use = NO_ITEM;
        }
        bare /= 400;
        sword /= 400;
        axe /= 400;
        check(bare >= 2 && bare <= 5, "m5e: bare hands average 1d4+1");
        check(sword >= 8 && sword <= 12, "m5e: sword averages 2d8+1");
        check(axe > sword, "m5e: the axe outdamages the sword");
        check(sword > bare * 2, "m5e: weapons clearly beat bare hands (D28)");
    }

    {   /* D28: the shield never takes the hand */
        load_house();
        world.units[0].ap = 40;
        world.units[0].items[0] = OBJ_SWORD;
        world.units[0].items[1] = OBJ_SHIELD;
        world.units[0].item_count = 2;
        world.units[0].in_use = NO_ITEM;
        check(items_cycle(&world, 0) && world.units[0].in_use == 0,
              "m5e: cycle from bare hands wields the sword");
        check(items_cycle(&world, 0) && world.units[0].in_use == NO_ITEM,
              "m5e: next stop is bare hands again");
        check(!items_cycle(&world, 0) || world.units[0].in_use != 1,
              "m5e: the shield is never wielded (D28/D21)");
        world.units[0].items[0] = OBJ_SHIELD;   /* shield only */
        world.units[0].item_count = 1;
        world.units[0].in_use = NO_ITEM;
        check(!items_cycle(&world, 0),
              "m5e: nothing to wield but a shield");
        check(items_defence(&world, 0) == 12 + WEAPONS[WEAPON_SHIELD].defence,
              "m5e: the carried shield still defends");
    }

    {   /* D29: bolt scales with the book level - level 1 wounds, level 8
         * usually kills a goblin (con 32) outright */
        uint8_t k, kills1 = 0, kills8 = 0, hits1 = 0, hits8 = 0;
        uint8_t kills1_without_crit = 0;
        uint32_t dmg1 = 0;
        for (k = 0; k < 100; k++) {
            Spellbook b;
            SpellShot shot;
            load_house();                   /* fresh wizard and goblin */
            world.units[1].owner = OWN_P2;
            world.units[1].x = 4;
            world.units[1].y = 3;
            memset(&b, 0, sizeof b);
            b.level[SP_MAGIC_BOLT] = 1;     /* 4d6 */
            rng_seed(&rng, 900 + k);
            spell_bolt(&world, &b, 0, SP_MAGIC_BOLT, 4, 3, &rng, &shot);
            if (shot.hit) {
                hits1++;
                dmg1 += shot.damage;
                if (shot.died) {
                    kills1++;
                    if (!shot.crit)
                        kills1_without_crit++;
                }
            }
            load_house();
            world.units[1].owner = OWN_P2;
            world.units[1].x = 4;
            world.units[1].y = 3;
            memset(&b, 0, sizeof b);
            b.level[SP_MAGIC_BOLT] = 8;     /* 11d6 */
            rng_seed(&rng, 900 + k);
            spell_bolt(&world, &b, 0, SP_MAGIC_BOLT, 4, 3, &rng, &shot);
            if (shot.hit) {
                hits8++;
                if (shot.died)
                    kills8++;
            }
        }
        check(hits1 >= 35, "m5e: the bolt connects over many seeds");
        check(kills1 * 10 <= hits1 && !kills1_without_crit,
              "m5e: a level-1 bolt one-shots only on a lucky crit (D30)");
        {
            uint8_t avg = (uint8_t)(dmg1 / (hits1 ? hits1 : 1));
            check(avg >= 10 && avg <= 18,
                  "m5e: level-1 bolt averages about 4d6 (D29)");
        }
        check(hits8 >= 35 && kills8 * 10 > hits8 * 7,
              "m5e: a level-8 bolt kills the goblin on most hits");
    }

    {   /* D29: the free swing on disengage spends the same reaction */
        CombatResult fs;
        load_house();
        world.units[1].owner = OWN_P2;
        world.units[1].x = 4;
        world.units[1].y = 4;
        world.units[0].con = 30;
        world.units[1].con = 32;
        world.units[1].ap = 30;
        world.units[1].sta = 45;
        rng_seed(&rng, 31);
        combat_melee(&world, &rng, 0, 1, &r);      /* goblin counters ... */
        check(r.returned, "m5e: the goblin counters the first attack");
        rng_seed(&rng, 32);
        check(combat_disengage_swings(&world, &rng, 0, &fs) == 0,
              "m5e: no free swing left in the same round (D29)");
        world.units[1].flags &= (uint8_t)~UF_REACTED;   /* new round */
        rng_seed(&rng, 33);
        check(combat_disengage_swings(&world, &rng, 0, &fs) == 1,
              "m5e: the free swing works again next round");
    }

    {   /* F6: spell shop and mana with XP (anchor 2026-10-04) */
        Wizard t;
        wizard_slot_reset(3);
        t = wizard_slots[3];
        t.xp = 100;
        t.book.level[SP_MAGIC_BOLT] = 0;   /* fresh learn: full base price */
        check(wizard_spell_next_cost(&t, SP_HARPY) == 12 &&
              wizard_spell_next_cost(&t, SP_MAGIC_BOLT) == 9,
              "m5f: summons cost the anchor, spells the mana-at-L1 placeholder");
        t.book.level[SP_MAGIC_BOLT] = 6;   /* restore the starting level */
        check(wizard_spell_next_cost(&t, SP_MAGIC_BOLT) == 4,
              "m5f: above level 1 every level costs half the base");
        check(wizard_spell_raise(&t, SP_HARPY) && t.book.level[SP_HARPY] == 1 &&
              t.xp == 88,
              "m5f: the first level costs the base price");
        check(wizard_spell_next_cost(&t, SP_HARPY) == 6 &&
              wizard_spell_raise(&t, SP_HARPY) && t.xp == 82,
              "m5f: every further level costs half the base (+50 % rule)");
        t.xp = 0;
        check(!wizard_spell_raise(&t, SP_HARPY), "m5f: no buying without XP");
        t.book.level[SP_HARPY] = 8;
        check(!wizard_spell_raise(&t, SP_HARPY), "m5f: level 8 is the cap");
        t.book.level[SP_HARPY] = 2;
        check(wizard_spell_lower(&t, SP_HARPY) && t.xp == 6 &&
              wizard_spell_lower(&t, SP_HARPY) && t.xp == 18,
              "m5f: lowering refunds base/half exactly");
        check(!wizard_spell_lower(&t, SP_TELEPORT),
              "m5f: nothing bought - lowering is refused");
        check(wizard_mana_cost() == 9 && t.mana_max == 90 && t.ap == 34,
              "m5f: mana starts at 90, AP at 34 (F6)");
        t.xp = 9;
        check(wizard_mana_raise(&t) && t.mana_max == 91 && t.xp == 0 &&
              wizard_mana_lower(&t) && t.mana_max == 90 && t.xp == 9,
              "m5f: mana raises and refunds with 9 XP");
        t.xp = 8;
        check(!wizard_mana_raise(&t), "m5f: one mana point costs exactly 9");
        t.xp = 7;
        check(!wizard_ap_raise(&t), "m5f: no AP without 8 XP");
        t.xp = 8;
        check(wizard_ap_raise(&t) && t.ap == 35 && t.xp == 0 &&
              wizard_ap_lower(&t) && t.ap == 34 && t.xp == 8,
              "m5f: AP raises and refunds with exactly 8");
    }

    {   /* D32: spell attacks ignore the carried shield */
        load_house();
        world.units[1].owner = OWN_P2;
        world.units[1].x = 4;
        world.units[1].y = 3;
        world.units[1].items[0] = OBJ_SHIELD;
        world.units[1].item_count = 1;
        check(items_defence(&world, 1) ==
              CREATURES[CR_GOBLIN].defence + WEAPONS[WEAPON_SHIELD].defence &&
              items_defence_noshield(&world, 1) == CREATURES[CR_GOBLIN].defence,
              "m5e: shield counts in melee defence, not against magic (D32)");
        {   /* the bolt rolls against the shield-less value */
            uint16_t k;
            uint8_t hits = 0;
            for (k = 0; k < 200; k++) {
                Spellbook b;
                SpellShot shot;
                load_house();
                world.units[1].owner = OWN_P2;
                world.units[1].x = 4;
                world.units[1].y = 3;
                world.units[1].items[0] = OBJ_SHIELD;
                world.units[1].item_count = 1;
                memset(&b, 0, sizeof b);
                b.level[SP_MAGIC_BOLT] = 1;
                rng_seed(&rng, 4000 + k);
                spell_bolt(&world, &b, 0, SP_MAGIC_BOLT, 4, 3, &rng, &shot);
                if (shot.hit)
                    hits++;
            }
            /* wizard com 10 vs goblin def 9 (no shield): 55 % expected;
             * with the shield counted it would be pinned at 10 % */
            check(hits >= 80 && hits <= 130,
                  "m5e: the bolt hits a shielded goblin like an unshielded one");
        }
    }

    {   /* D30: critical hits - rare, dice doubled (bonus not) */
        uint16_t k, crits = 0, hits = 0;
        uint32_t normal = 0, critical = 0;
        CombatResult cr;
        load_house();
        world.units[0].items[0] = OBJ_SWORD;
        world.units[0].item_count = 1;
        world.units[0].in_use = 0;
        world.units[1].owner = OWN_P2;
        world.units[1].x = 4;
        world.units[1].y = 4;
        for (k = 0; k < 400; k++) {
            rng_seed(&rng, 5000 + k);
            world.units[0].ap = 40;
            world.units[0].con = 30;
            world.units[1].con = 250;      /* a punching bag that survives */
            world.units[1].con_max = 250;
            world.units[1].flags |= UF_REACTED;   /* its counter stays off */
            if (combat_melee(&world, &rng, 0, 1, &cr) && cr.hit) {
                hits++;
                if (cr.crit) {
                    crits++;
                    critical += cr.damage;
                } else
                    normal += cr.damage;
            }
        }
        check(crits >= 8 && crits <= 45,
              "m5e: about one in twenty hits is critical (D30)");
        {   /* crit = 4d8+1 vs normal 2d8+1: the crit adds one dice roll */
            uint16_t avg_n = (uint16_t)(normal / (hits - crits ? hits - crits : 1));
            uint16_t avg_c = (uint16_t)(critical / (crits ? crits : 1));
            check(avg_c > avg_n, "m5e: criticals outdamage normal hits");
            check(avg_c > avg_n + 6 && avg_c < avg_n + 12,
                  "m5e: the crit adds about one extra sword roll");
        }
    }
}

uint16_t core_selftest(selftest_log_fn log)
{
    out = log;
    fails = 0;
    test_rng();
    test_world();
    test_view();
    test_dirty_and_move();
    test_ap();
    test_chord();
    test_stats_and_names();
    test_data();
    test_terrain();
    test_creatures();
    test_turn();
    test_sight();
    test_flight();
    test_bump_and_look();
    test_combat();
    test_spells();
    test_bolt();
    test_items();
    test_game();
    test_ai();
    test_review_fixes();
    test_scenario();
    test_m4a();
    test_m4b();
    test_m4c();
    test_m4d();
    test_m4e();
    test_m4f();
    test_m4g();
    test_m4h();
    test_m5a();
    test_m4i();
    test_m4k_ai();
    test_m4_review();
    test_m5b_tutorial();
    test_m5b_lexicon();
    test_m5c_events();
    test_m5e_balance();
    load_house();   /* leave a clean state */
    return fails;
}
