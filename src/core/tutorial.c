#include "tutorial.h"

#include <string.h>

/* The player's wizard (first own CR_WIZARD); tutorials start with him. */
static uint8_t find_wizard(const World *w)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner == OWN_P1 && w->units[i].kind == CR_WIZARD)
            return i;
    return NO_UNIT;
}

static uint8_t count_enemies(const World *w)
{
    uint8_t i, n = 0;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].owner != OWN_P1 && w->units[i].owner != OWN_NEUTRAL)
            n++;
    return n;
}

void tutorial_init(Tutorial *t, const World *w)
{
    uint8_t i;
    memset(t, 0, sizeof *t);
    t->step = TUT_MOVE;
    t->wiz_x = t->wiz_y = 0xFF;
    t->key_x = t->key_y = 0xFF;
    t->chest_x = t->chest_y = 0xFF;
    {
        uint8_t wiz = find_wizard(w);
        if (wiz != NO_UNIT) {
            t->wiz_x = w->units[wiz].x;
            t->wiz_y = w->units[wiz].y;
        }
    }
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].tile == OBJECTS[OBJ_CHEST_KEY].tile) {
            t->key_x = w->objects[i].x;
            t->key_y = w->objects[i].y;
            break;
        }
    {   /* first chest on the map is the tutorial chest */
        uint16_t f;
        for (f = 0; f < (uint16_t)(MAP_MAX_H * MAP_MAX_W); f++) {
            uint8_t y = (uint8_t)(f / MAP_MAX_W), x = (uint8_t)(f % MAP_MAX_W);
            if (x < w->w && y < w->h && w->feature[y][x] == FE_CHEST) {
                t->chest_x = x;
                t->chest_y = y;
                break;
            }
        }
    }
    t->enemies = count_enemies(w);
}

static bool key_taken(const Tutorial *t, const World *w)
{
    uint8_t i;
    if (t->key_x == 0xFF)
        return false;                     /* no key on this map */
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].x == t->key_x && w->objects[i].y == t->key_y &&
            w->objects[i].tile == OBJECTS[OBJ_CHEST_KEY].tile)
            return false;                 /* still lying there */
    return true;
}

static bool step_holds(const Tutorial *t, const World *w, const Game *g)
{
    switch (t->step) {
    case TUT_MOVE: {
        uint8_t wiz = find_wizard(w);
        return wiz != NO_UNIT && (w->units[wiz].x != t->wiz_x ||
                                  w->units[wiz].y != t->wiz_y);
    }
    case TUT_SWITCH:
        return t->switch_done;
    case TUT_PICKUP:
        return key_taken(t, w);
    case TUT_CHEST:
        return t->chest_x != 0xFF &&
               w->feature[t->chest_y][t->chest_x] != FE_CHEST;
    case TUT_KILL:
        return count_enemies(w) == 0;
    case TUT_SPELL:
        return t->spell_done;
    case TUT_PORTAL:
        return g != NULL && game_outcome(g, w, OWN_P1) != OUT_RUNNING;
    default:
        return false;
    }
}

uint8_t tutorial_update(Tutorial *t, const World *w, const Game *g)
{
    while (t->step < TUT_DONE && step_holds(t, w, g))
        t->step++;
    return t->step;
}

void tutorial_notify(Tutorial *t, uint8_t event)
{
    if (event == TUT_SWITCH)
        t->switch_done = true;
    else if (event == TUT_SPELL)
        t->spell_done = true;
}

bool tutorial_finished(const Tutorial *t)
{
    return t->step >= TUT_DONE;
}
