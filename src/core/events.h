/*
 * Presentation events (M5c, GDD 11.4): the rules report what happened
 * where; the frontend drains the ring and animates and sounds it. Pure
 * observation - no RNG use, no world changes, so replays, hashes and
 * savegames stay byte-identical. One global ring: the game is
 * single-threaded and the core owns no other globals besides this.
 */
#ifndef LOC_EVENTS_H
#define LOC_EVENTS_H

#include <stdint.h>

typedef enum {
    EV_NONE = 0,
    EV_SWING,    /* melee swing at (x, y); kind/owner = the attacker
                   (b = 1: against terrain) */
    EV_HIT,      /* a damage on the unit that stood at (x, y) */
    EV_WOUND,    /* fatal wound opened on (x, y) */
    EV_MISS,     /* an attack whiffed at (x, y) */
    EV_DEATH,    /* unit kind/owner died at (x, y); a = 1: bled out */
    EV_SPELL,    /* spell a cast at (x, y) */
    EV_SMASH,    /* terrain at (x, y) smashed to pieces */
    EV_PROJECTILE /* something flies from (x, y) to (x + (int8_t)a,
                    y + (int8_t)b), the shortest way on wrapping maps;
                    kind = ProjectileKind. Comes before its hit/miss. */
} EventKind;

typedef enum {
    PJ_BOLT,       /* magic bolt */
    PJ_LIGHTNING,  /* magic lightning */
    PJ_ARROW,      /* bow */
    PJ_THROWN      /* a thrown object */
} ProjectileKind;

typedef struct {
    uint8_t type;     /* EventKind */
    int16_t x, y;     /* world position */
    uint8_t kind;     /* creature kind (SWING/DEATH) or spell id (SPELL) */
    uint8_t owner;
    uint8_t a, b;     /* per-type payload (damage, flags) */
} GameEvent;

#define EVENT_RING 16

/* Clear the ring (call at scenario start and in tests). */
void events_reset(void);
/* Record one event; a full ring drops the event (presentation only). */
void events_push(uint8_t type, int16_t x, int16_t y, uint8_t kind,
                 uint8_t owner, uint8_t a, uint8_t b);
/* Copy up to cap events out in order and clear the ring. Returns the
 * number of events copied. */
uint8_t events_drain(GameEvent *out, uint8_t cap);
/* Events dropped by a full ring since the last reset (tests). */
uint8_t events_dropped(void);

#endif
