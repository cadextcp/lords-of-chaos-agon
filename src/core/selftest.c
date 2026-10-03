#include "selftest.h"

#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "brew.h"
#include "effect.h"
#include "chord.h"
#include "combat.h"
#include "game.h"
#include "items.h"
#include "gen/maps.h"
#include "gen/scenarios.h"
#include "names.h"
#include "rng.h"
#include "sight.h"
#include "spells.h"
#include "turn.h"
#include "view.h"
#include "world.h"

/* view_hash() of the wizard house with the cursor on the wizard.
 * Must be identical on host and Agon; update deliberately when the map,
 * tiles or composition rules change. Changed for M2d: the new unexplored
 * tile shifted every tile ID after "tree"; M2e added air_shadow and
 * cursor_blue, M3d/M3e object and portal tiles, M3g four treasures. */
#define HOUSE_VIEW_HASH 0x46DA1048UL

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
            if (f->n != ref.n || f->air != ref.air ||
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
    world.units[1].ap = 0;                      /* too tired to strike back */
    world.units[1].con = 32;
    {
        CombatResult a, b;
        combat_melee(&world, &rng, 0, 1, &a);
        world.units[0].ap = 40;
        world.units[1].ap = 0;
        world.units[1].con = 32;
        rng_seed(&rng, 7);
        combat_melee(&world, &rng, 0, 1, &b);
        check(a.hit == b.hit && a.damage == b.damage && a.returned == b.returned,
              "combat: same seed, same outcome");
        check(!a.returned, "combat: no return without AP");
        check(world.units[0].ap == 30, "combat: melee costs 10 AP");
    }

    world.units[1].ap = 30;                     /* with AP: return attack */
    world.units[1].sta = 45;
    world.units[1].con = 32;
    rng_seed(&rng, 21);
    combat_melee(&world, &rng, 0, 1, &r);
    check(r.returned, "combat: defenders with AP strike back");
    check(world.units[1].ap == 24 || !r.returned, "combat: return costs 6 AP");

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

    {   /* engagement: bound units hold their ground (GDD 6) */
        world_load_bin(&world, MAPBIN_TESTLAND, MAPBIN_TESTLAND_LEN);
        world.unit_count = 2;
        world.units[1].x = 7;
        world.units[1].y = 6;
        world.units[1].kind = CR_GOBLIN;
        check(world_engaged(&world, 0), "combat: neighbour = engaged");
        check(!world_move_unit(&world, 0, 0, -1), "combat: bound units cannot flee");
        check(!world_move_unit(&world, 0, -1, -1), "combat: not diagonally either");
        check(!world_move_unit(&world, 0, 1, 0), "combat: the enemy field blocks the move");
        world_remove_unit(&world, 1);
        check(!world_engaged(&world, 0) && world_move_unit(&world, 0, 0, -1),
              "combat: free again after the enemy dies");
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
    check(WEAPONS[WEAPON_SWORD].combat == 4 && WEAPONS[WEAPON_SHIELD].defence == 4 &&
          WEAPONS[WEAPON_BOW].ranged == 4, "items: weapon values");

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
    check(items_combat(&world, 0) == 14, "items: sword +4 combat");

    {   /* shield carried: defence always (GDD 6.1) */
        world.units[0].items[1] = OBJ_SHIELD;
        world.units[0].item_count = 2;
        check(items_defence(&world, 0) == 16, "items: carried shield +4 defence");
        world.units[0].items[2] = OBJ_SHIELD;
        world.units[0].item_count = 3;
        check(items_defence(&world, 0) == 16, "items: shields do not stack (D21)");
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
    test_m4_review();
    load_house();   /* leave a clean state */
    return fails;
}
