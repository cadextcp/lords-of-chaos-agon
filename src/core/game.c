#include "game.h"

#include "gen/data.h"
#include "items.h"
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
    g->escaped = 0;
    g->eye_x = g->eye_y = -1;
    g->eye_rounds = 0;
    for (i = 0; i < OWN_NEUTRAL; i++)
        g->vp[i] = 0;
}

void game_new_round(Game *g, uint8_t round)
{
    if (g->eye_rounds > 0 && --g->eye_rounds == 0) {
        g->eye_x = g->eye_y = -1;       /* the eye closes (GDD 7.2) */
    }
    if (!g->portal_open && g->portal_x >= 0 && round >= g->portal_round)
        g->portal_open = true;
}

bool game_try_enter_portal(Game *g, World *w, uint8_t unit)
{
    Unit *u;
    uint8_t i;
    uint16_t vp;
    if (unit >= w->unit_count)
        return false;
    u = &w->units[unit];
    if (!g->portal_open || u->x != g->portal_x || u->y != g->portal_y)
        return false;
    if (u->kind != CR_WIZARD)
        return false;                    /* only wizards escape (PM 29) */
    vp = VP_ESCAPE;
    for (i = 0; i < u->item_count; i++)
        if (OBJECTS[u->items[i]].category == OC_TREASURE)
            vp = (uint16_t)(vp + OBJECTS[u->items[i]].vp);
    g->vp[u->owner] = (uint16_t)(g->vp[u->owner] + vp);
    g->escaped |= (uint8_t)(1u << u->owner);
    world_remove_unit(w, unit);
    return true;
}

void game_kill_credit(Game *g, const Kill *k)
{
    uint16_t vp;
    if (k->killer_owner >= OWN_NEUTRAL || k->killer_owner == k->victim_owner)
        return;                                /* independents, friendly fire */
    vp = CREATURES[k->victim_kind].vp;         /* wizards count as 20 */
    if (k->killer_kind == CR_WIZARD && k->melee)
        vp = (uint16_t)(vp * 2);               /* AMI 4 */
    g->vp[k->killer_owner] = (uint16_t)(g->vp[k->killer_owner] + vp);
}

void game_credit_kills(Game *g, World *w)
{
    uint8_t i;
    for (i = 0; i < w->kill_count; i++)
        game_kill_credit(g, &w->kills[i]);
    w->kill_count = 0;
}

bool game_over(const Game *g, const World *w)
{
    uint8_t i;
    if (g->portal_x < 0)
        return false;                    /* no portal: endless test map */
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].kind == CR_WIZARD)
            return false;
    return true;
}
