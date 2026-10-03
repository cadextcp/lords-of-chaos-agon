#include "events.h"

static GameEvent ring[EVENT_RING];
static uint8_t ring_head;    /* next write slot */
static uint8_t ring_count;
static uint8_t dropped;

void events_reset(void)
{
    ring_head = 0;
    ring_count = 0;
    dropped = 0;
}

void events_push(uint8_t type, int16_t x, int16_t y, uint8_t kind,
                 uint8_t owner, uint8_t a, uint8_t b)
{
    GameEvent *e;
    if (ring_count >= EVENT_RING) {
        dropped++;                         /* busy moment: drop the new one */
        return;
    }
    e = &ring[ring_head];
    e->type = type;
    e->x = x;
    e->y = y;
    e->kind = kind;
    e->owner = owner;
    e->a = a;
    e->b = b;
    ring_head = (uint8_t)((ring_head + 1) % EVENT_RING);
    ring_count++;
}

uint8_t events_drain(GameEvent *out, uint8_t cap)
{
    uint8_t i, n = ring_count < cap ? ring_count : cap;
    uint8_t first = (uint8_t)((ring_head + EVENT_RING - ring_count) % EVENT_RING);
    for (i = 0; i < n; i++)
        out[i] = ring[(first + i) % EVENT_RING];
    ring_count = 0;
    return n;
}

uint8_t events_dropped(void)
{
    return dropped;
}
