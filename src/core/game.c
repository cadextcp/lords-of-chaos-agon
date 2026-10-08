#include "game.h"

#include "gen/data.h"
#include "items.h"
#include "ride.h"
#include "rng.h"

void game_init(Game *g, int16_t x, int16_t y, uint8_t rmin, uint8_t rmax,
               Rng *rng)
{
    uint8_t i;
    g->portal_x = x;
    g->portal_y = y;
    g->portal_round = rmax > rmin ? (uint8_t)(rmin + rng_range(rng,
                          (uint16_t)(rmax - rmin + 1))) : rmin;
    g->portal_open = false;
    g->portal_closed = false;
    g->portal_span = 0;
    g->escaped = 0;
    g->eye_x = g->eye_y = -1;
    g->eye_rounds = 0;
    for (i = 0; i < OWN_NEUTRAL; i++) {
        g->vp[i] = 0;
        g->wizard_vp[i] = 19;
        g->kills[i] = 0;
        g->loot_vp[i] = 0;
        g->home_x[i] = g->home_y[i] = 0xFF;
        g->rage[i] = g->rage_round[i] = 0;
    }
}

void game_new_round(Game *g, uint8_t round)
{
    if (g->eye_rounds > 0 && --g->eye_rounds == 0) {
        g->eye_x = g->eye_y = -1;       /* the eye closes (GDD 7.2) */
    }
    if (!g->portal_open && g->portal_x >= 0 && round >= g->portal_round)
        g->portal_open = true;
    if (g->portal_open && g->portal_span &&
        round >= (uint16_t)(g->portal_round + g->portal_span))
        g->portal_closed = true;        /* the span ran out: it is over (K4) */
}

void game_set_portal_span(Game *g, uint8_t span)
{
    g->portal_span = span;
}

void game_set_wizard_level(Game *g, uint8_t owner, uint8_t level)
{
    if (owner < OWN_NEUTRAL)
        g->wizard_vp[owner] = (uint8_t)(4 * level + 15);
}

bool game_try_enter_portal(Game *g, World *w, uint8_t unit)
{
    Unit *u;
    uint8_t i;
    uint16_t vp;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (!g->portal_open || g->portal_closed || u->x != g->portal_x ||
        u->y != g->portal_y)
        return false;
    if (ride_actor_kind(u) != CR_WIZARD)
        return false;                    /* only wizards escape (PM 29) */
    vp = VP_ESCAPE;
    for (i = 0; i < u->item_count; i++)
        if (OBJECTS[u->items[i]].category == OC_TREASURE)
            vp = (uint16_t)(vp + OBJECTS[u->items[i]].vp);
    g->vp[u->owner] = (uint16_t)(g->vp[u->owner] + vp > 255 ? 255 : g->vp[u->owner] + vp);
    g->loot_vp[u->owner] = (uint16_t)(g->loot_vp[u->owner] + vp - VP_ESCAPE);
    g->escaped |= (uint8_t)(1u << u->owner);
    world_remove_unit(w, unit);
    return true;
}

void game_kill_credit(Game *g, const Kill *k)
{
    uint16_t vp, total;
    if (k->killer_owner >= OWN_NEUTRAL || k->killer_owner == k->victim_owner)
        return;                                /* independents, friendly fire */
    if (k->victim_kind == CR_WIZARD && k->victim_owner < OWN_NEUTRAL)
        vp = g->wizard_vp[k->victim_owner];    /* 4 Level + 15 */
    else
        vp = (uint16_t)(2u * CREATURES[k->victim_kind].vp);   /* the byte */
    if (k->killer_kind != CR_WIZARD)
        vp = (uint16_t)(vp / 2);               /* a summoned creature: half */
    total = (uint16_t)(g->vp[k->killer_owner] + vp);
    g->vp[k->killer_owner] = total > 255 ? 255 : total;
    if (g->kills[k->killer_owner] < 255)
        g->kills[k->killer_owner]++;
}

void game_credit_kills(Game *g, World *w)
{
    uint8_t i;
    for (i = 0; i < w->kill_count; i++)
        game_kill_credit(g, &w->kills[i]);
    w->kill_count = 0;
}

/* Does `owner` still field a wizard (0xFF: any owner)? */
static bool wizard_alive(const World *w, uint8_t owner)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *u = &w->units[i];
        if (owner != 0xFF && u->owner != owner)
            continue;
        if (u->kind == CR_WIZARD ||
            ((u->flags & UF_RIDDEN) && u->rider_kind == CR_WIZARD))
            return true;
    }
    return false;
}

bool game_over(const Game *g, const World *w)
{
    if (g->portal_x < 0)
        return false;                    /* no portal: endless test map */
    return g->portal_closed || !wizard_alive(w, 0xFF);
}

GameOutcome game_outcome(const Game *g, const World *w, uint8_t owner)
{
    if (owner >= OWN_NEUTRAL || g->portal_x < 0)
        return OUT_RUNNING;
    if (g->escaped & (uint8_t)(1u << owner))
        return OUT_WIN;
    if (g->portal_closed)
        return OUT_LOSE;                 /* too late: the portal is shut */
    return wizard_alive(w, owner) ? OUT_RUNNING : OUT_LOSE;
}
