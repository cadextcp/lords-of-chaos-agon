#include "selftest.h"

#include <stdio.h>

#include "demo.h"
#include "rng.h"
#include "screen.h"

/* screen_hash() after demo_init(DEMO_SEED). Must be identical on host and
 * Agon; update deliberately when the demo scene changes. */
#define DEMO_SEED 42UL
#define DEMO_HASH 0xF0A1B598UL

static selftest_log_fn out;
static uint16_t fails;

static void check(int ok, const char *what)
{
    char buf[80];
    if (!ok)
        fails++;
    snprintf(buf, sizeof buf, "%s %s", ok ? "ok  " : "FAIL", what);
    out(buf);
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

static void test_screen(void)
{
    screen_clear(0);
    check(screen_dirty_count() == SCREEN_W * SCREEN_H, "screen: clear marks all dirty");
    screen_clean_all();
    check(screen_dirty_count() == 0, "screen: clean");
    screen_put(3, 4, ' ', 0, 0);
    check(screen_dirty_count() == 0, "screen: unchanged put stays clean");
    screen_put(3, 4, 'A', 7, 0);
    check(screen_dirty_count() == 1 && screen_is_dirty(3, 4), "screen: changed put marks one cell");
    screen_put(SCREEN_W, 0, 'X', 7, 0);
    check(screen_dirty_count() == 1, "screen: out of bounds ignored");
}

static void test_demo(void)
{
    char buf[48];
    uint32_t h;
    uint8_t x, y;

    demo_init(DEMO_SEED);
    h = screen_hash();
    snprintf(buf, sizeof buf, "demo: hash=0x%08lX", (unsigned long)h);
    out(buf);
    check(h == DEMO_HASH, "demo: deterministic scene hash");

    /* Walking into the border wall must be blocked. */
    for (x = 0; x < SCREEN_W; x++)
        demo_move(DIR_W);
    check(demo_wizard_x() >= 1, "demo: wall blocks movement");
    y = demo_wizard_y();
    demo_move(DIR_NONE);
    check(demo_wizard_y() == y, "demo: no-op move");
}

uint16_t core_selftest(selftest_log_fn log)
{
    out = log;
    fails = 0;
    test_rng();
    test_screen();
    test_demo();
    return fails;
}
