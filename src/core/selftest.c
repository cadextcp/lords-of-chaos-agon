#include "selftest.h"

#include <stdio.h>
#include <string.h>

#include "chord.h"
#include "names.h"
#include "rng.h"
#include "view.h"
#include "world.h"

/* view_hash() of the wizard house with the cursor on the wizard.
 * Must be identical on host and Agon; update deliberately when the map,
 * tiles or composition rules change. */
#define HOUSE_VIEW_HASH 0xC356BB16UL

static selftest_log_fn out;
static uint16_t fails;
static World world;

static void check(int ok, const char *what)
{
    char buf[80];
    if (!ok)
        fails++;
    snprintf(buf, sizeof buf, "%s %s", ok ? "ok  " : "FAIL", what);
    out(buf);
}

static int has_layer(const FieldLayers *f, uint8_t id)
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
            if (f->n != ref.n || memcmp(f->id, ref.id, ref.n) != 0)
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
    world_load(&world, &MAP_WIZARD_HOUSE);
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
    snprintf(buf, sizeof buf, "view: hash=0x%08lX", (unsigned long)h);
    out(buf);
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
    world_load(&world, &MAP_WIZARD_HOUSE);
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

    world_load(&world, &MAP_WIZARD_HOUSE);
    check(world.units[0].sta == 60 && world.units[0].mana == 80, "stats: wizard stamina 60, mana 80");
    world_move_unit(&world, 0, 1, 1);                         /* diagonal: 6 AP */
    check(world.units[0].sta == 57, "stats: step costs half the AP as stamina");
    world_new_turn(&world);
    check(world.units[0].sta == 60, "stats: new turn recovers stamina (capped)");
    world.units[0].sta = 10;
    world_new_turn(&world);
    check(world.units[0].sta == 25, "stats: recovery is 25 % of max");

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
    world_load(&world, &MAP_WIZARD_HOUSE);   /* leave a clean state */
    return fails;
}
