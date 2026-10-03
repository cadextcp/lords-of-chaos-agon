/*
 * Turn order (GDD 2.1): a round runs the independent creatures, then wizard
 * 1 to n; at the round end AP, stamina and mana regenerate. Within a
 * wizard's phase exactly one of his units is active (GDD 5.1). AI phases
 * pass for now - real behaviour profiles come with M3 (GDD 10).
 */
#ifndef LOC_TURN_H
#define LOC_TURN_H

#include <stdbool.h>
#include <stdint.h>

#include "rng.h"
#include "world.h"

/* AI phases call this instead of passing (M3f); registered by the
 * frontend with its books/game context. */
typedef struct Turns Turns;
typedef void (*TurnAiFn)(Turns *t, World *w, void *ctx);

/* Rounds the AI keeps playing on its own once no human has a unit left
 * (escaped or dead): enough to reach the portal, and a bound so the
 * phase loop always ends. */
#define TURN_AUTOPLAY_ROUNDS 40

struct Turns {
    uint8_t round;       /* 1-based game round */
    uint8_t phase;       /* owner whose units act (OWN_P1..OWN_P4) */
    uint8_t active;      /* active unit index, NO_UNIT only without units */
    uint8_t active_id;   /* its Unit.id: survives reordering removals */
    uint8_t humans;      /* owner bitmask of human players */
    bool round1_lock;    /* no movement in round 1, casting only (PM 7) */
    Rng rng;             /* independent creatures; seeded, so runs replay */
    TurnAiFn ai;         /* NULL: AI phases pass (tests) */
    void *ai_ctx;
    TurnAiFn on_round;   /* called after each round change (portal etc.) */
    void *round_ctx;
    /* Called after an AI phase (and the independents' steps) so the
     * frontend can drain the event ring and animate (M5c). NULL: off. */
    TurnAiFn on_ai;
    void *on_ai_ctx;
};

/* Start round 1: run the independents, then the first owner's phase. */
void turn_init(Turns *t, World *w, uint32_t seed, uint8_t humans);
/* Movement allowed right now? False only in round 1 (PM 7). */
bool turn_may_move(const Turns *t);
/* Next (Tab) or previous (Shift+Tab) own unit with AP left; wraps around. */
void turn_next_unit(Turns *t, const World *w, bool backwards);
/* Active unit is done (space): it is skipped until the next phase. */
void turn_finish_unit(Turns *t, World *w);
/* Any own unit with AP left that is not done? */
bool turn_units_left(const Turns *t, const World *w);
/* End the phase (Shift+E): AI phases pass automatically until a human
 * phase is active again; the round end regenerates and lets the
 * independents take their steps. Without human units the AI plays on
 * until no wizard is left, at most TURN_AUTOPLAY_ROUNDS rounds. */
void turn_end_phase(Turns *t, World *w);
/* Does any human player still have a unit on the map? */
bool turn_humans_present(const Turns *t, const World *w);
/* Independent creatures' phase: placeholder wandering, deterministic
 * through the seeded RNG. Respects the round 1 lock (PM 7). */
void turn_independents(Turns *t, World *w);
/* After anything that may remove units: find the active unit again by
 * its id; if it is gone, select the next usable own unit. */
void turn_revalidate(Turns *t, const World *w);

#endif
