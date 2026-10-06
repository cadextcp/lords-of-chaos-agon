#include "turn.h"

#include "populate.h"

#include <string.h>

#include "ai.h"
#include "ride.h"

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
           !w->units[i].done;
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

static void set_active(Turns *t, const World *w, uint8_t i)
{
    t->active = i;
    t->active_id = i < w->unit_count ? w->units[i].id : NO_UNIT;
}

/* First usable own unit, else any own unit (shown even without AP). */
static void select_first(Turns *t, const World *w)
{
    uint8_t i = find_usable(t, w, 0, false);
    if (i == NO_UNIT)
        i = any_unit_of(w, t->phase);
    set_active(t, w, i);
}

static void start_phase(Turns *t, World *w, uint8_t owner)
{
    uint8_t i;
    t->phase = owner;
    for (i = 0; i < w->unit_count; i++)
        w->units[i].done = false;
    select_first(t, w);
}

bool turn_may_move(const Turns *t)
{
    return !(t->round1_lock && t->round == 1);
}

void turn_independents(Turns *t, World *w)
{
    if (!turn_may_move(t))
        return;
    /* hunters chase the nearest enemy they see (GDD 10); without prey
     * they keep the old wandering as fallback */
    ai_run_hunters(w, &t->rng, OWN_NEUTRAL, NO_UNIT);
}

void turn_init(Turns *t, World *w, uint32_t seed, uint8_t humans)
{
    memset(t, 0, sizeof *t);           /* clears the ai callback too */
    t->round = 1;
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
        set_active(t, w, i);
}

void turn_finish_unit(Turns *t, World *w)
{
    if (t->active < w->unit_count)
        w->units[t->active].done = true;
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

void turn_revalidate(Turns *t, const World *w)
{
    uint8_t i = world_find_unit(w, t->active_id);
    if (i != NO_UNIT && w->units[i].owner == t->phase)
        t->active = i;                    /* same unit, maybe a new index */
    else
        select_first(t, w);               /* it died or escaped */
}

bool turn_humans_present(const Turns *t, const World *w)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner < OWN_NEUTRAL &&
            (t->humans & (1u << w->units[i].owner)) != 0)
            return true;
    return false;
}

static bool wizards_present(const World *w)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (ride_actor_kind(&w->units[i]) == CR_WIZARD)
            return true;   /* a mounted wizard counts (D60) */
    return false;
}

void turn_end_phase(Turns *t, World *w)
{
    uint8_t autoplay = 0;
    for (;;) {
        uint8_t o;
        world_release(w, t->phase);       /* bound for that phase only (GDD 6) */
        o = next_owner(w, t->phase);
        if (o == OWN_COUNT) {             /* last owner done: round end */
            /* nobody left to hand the turn back to: let the AI finish
             * the game, but never loop forever */
            if (!turn_humans_present(t, w) &&
                (!wizards_present(w) || autoplay++ >= TURN_AUTOPLAY_ROUNDS))
                return;
            if (t->round < 255)
                t->round++;
            world_new_turn(w);            /* regeneration (GDD 2.1.4) */
            if (t->on_round)
                t->on_round(t, w, t->round_ctx);
            if (t->wildlife)              /* now and then a herd (D35) */
                populate_herd(w, &t->rng, t->round);
            if (t->on_phase)              /* the independents' phase */
                t->on_phase(t, w, OWN_NEUTRAL, t->on_phase_ctx);
            turn_independents(t, w);      /* next round starts (GDD 2.1.1) */
            if (t->on_ai)                 /* their fights animate too (M5c) */
                t->on_ai(t, w, t->on_ai_ctx);
            world_release(w, OWN_NEUTRAL);
            o = first_owner(w);
            if (o == OWN_COUNT)
                return;                   /* world without wizards */
        }
        start_phase(t, w, o);
        if ((t->humans & (1u << o)) != 0)
            return;                       /* human players act */
        if (t->on_phase)
            t->on_phase(t, w, o, t->on_phase_ctx);
        if (t->ai)
            t->ai(t, w, t->ai_ctx);       /* wizard AI (GDD 10, M3f) */
        if (t->on_ai)                     /* per AI phase (M5c) */
            t->on_ai(t, w, t->on_ai_ctx);
    }
}
