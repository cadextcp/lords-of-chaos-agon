#include "turn.h"

/* Placeholder wandering until the behaviour profiles (GDD 10): every
 * independent creature tries up to two random steps, three directions per
 * step; blocked or too expensive directions simply fail. */
#define WANDER_STEPS 2
#define WANDER_TRIES 3

static const int8_t DX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int8_t DY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

static bool owner_present(const World *w, uint8_t owner)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner)
            return true;
    return false;
}

/* First owner (round order P1..P4) with units on the map. */
static uint8_t first_owner(const World *w)
{
    uint8_t o;
    for (o = OWN_P1; o < OWN_NEUTRAL; o++)
        if (owner_present(w, o))
            return o;
    return OWN_COUNT;
}

/* Next owner after `after` that still has units, OWN_COUNT after P4. */
static uint8_t next_owner(const World *w, uint8_t after)
{
    uint8_t o;
    for (o = (uint8_t)(after + 1); o < OWN_NEUTRAL; o++)
        if (owner_present(w, o))
            return o;
    return OWN_COUNT;
}

/* Own unit usable as the active one: has AP and is not finished. */
static bool unit_usable(const Turns *t, const World *w, uint8_t i)
{
    return w->units[i].owner == t->phase && w->units[i].ap > 0 &&
           (t->done & (1u << i)) == 0;
}

/* Search for a usable unit, starting at `from` (inclusive), wrapping over
 * the unit list. NO_UNIT if every own unit is spent or finished. */
static uint8_t find_usable(const Turns *t, const World *w, uint8_t from,
                           bool backwards)
{
    uint8_t k, i;
    for (k = 0; k < w->unit_count; k++) {
        i = backwards ? (uint8_t)((from + w->unit_count - k) % w->unit_count)
                      : (uint8_t)((from + k) % w->unit_count);
        if (unit_usable(t, w, i))
            return i;
    }
    return NO_UNIT;
}

static uint8_t any_unit_of(const World *w, uint8_t owner)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == owner)
            return i;
    return NO_UNIT;
}

static void start_phase(Turns *t, const World *w, uint8_t owner)
{
    t->phase = owner;
    t->done = 0;
    t->active = find_usable(t, w, 0, false);
    if (t->active == NO_UNIT)             /* all AP spent: show one anyway */
        t->active = any_unit_of(w, owner);
}

bool turn_may_move(const Turns *t)
{
    return !(t->round1_lock && t->round == 1);
}

void turn_independents(Turns *t, World *w)
{
    uint8_t i, s, k;
    if (!turn_may_move(t))
        return;
    for (i = 0; i < w->unit_count; i++) {
        if (w->units[i].owner != OWN_NEUTRAL)
            continue;
        for (s = 0; s < WANDER_STEPS; s++)
            for (k = 0; k < WANDER_TRIES; k++)
                if (world_move_unit(w, i, DX[rng_range(&t->rng, 8)],
                                    DY[rng_range(&t->rng, 8)]))
                    break;
    }
}

void turn_init(Turns *t, World *w, uint32_t seed, uint8_t humans)
{
    t->round = 1;
    t->done = 0;
    t->humans = humans;
    t->round1_lock = true;
    rng_seed(&t->rng, seed);
    turn_independents(t, w);              /* round start (stays put, PM 7) */
    start_phase(t, w, first_owner(w));
}

void turn_next_unit(Turns *t, const World *w, bool backwards)
{
    uint8_t from = t->active == NO_UNIT ? 0 : t->active;
    uint8_t i;
    if (backwards && from == 0)
        from = w->unit_count;             /* wraps to the last unit */
    i = find_usable(t, w, (uint8_t)(backwards ? from - 1 : from + 1), backwards);
    if (i != NO_UNIT)
        t->active = i;
}

void turn_finish_unit(Turns *t, const World *w)
{
    if (t->active < w->unit_count)
        t->done |= 1u << t->active;
    turn_next_unit(t, w, false);
}

bool turn_units_left(const Turns *t, const World *w)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (unit_usable(t, w, i))
            return true;
    return false;
}

void turn_end_phase(Turns *t, World *w)
{
    for (;;) {
        uint8_t o = next_owner(w, t->phase);
        if (o == OWN_COUNT) {             /* last owner done: round end */
            t->round++;
            world_new_turn(w);            /* regeneration (GDD 2.1.4) */
            turn_independents(t, w);      /* next round starts (GDD 2.1.1) */
            o = first_owner(w);
            if (o == OWN_COUNT)
                return;                   /* world without wizards */
        }
        start_phase(t, w, o);
        if ((t->humans & (1u << o)) != 0)
            return;                       /* human players act */
        /* AI placeholder until M3: the phase passes. */
    }
}
