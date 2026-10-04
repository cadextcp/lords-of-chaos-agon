/*
 * Host battle-royale simulator (2026-10-04, user request): all 26
 * creatures, every weapon plus three shields scattered over an open
 * arena, everyone against everyone. Units walk to the nearest object
 * they see, pick it up (one item each), wield it when they can use
 * weapons, then fight to the last. Everything runs through the REAL
 * core rules - movement, AP/stamina, engagement, the free reaction
 * (D27/D29), weapon dice and criticals (D28/D30), undead immunity,
 * fatal wounds, drops on death.
 *
 * Build (WSL, like build.py does for the host build):
 *   gcc -std=c99 -Wall -Wextra -O2 -o build/host/arena \
 *       src/core/*.c src/core/gen/*.c host/arena.c
 * Run:
 *   build/host/arena [runs=100] [seed=1] [verbose]
 *
 * No GUI, no VDP - pure core, printed statistics.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/combat.h"
#include "../src/core/events.h"
#include "../src/core/gen/data.h"
#include "../src/core/gen/tiles.h"
#include "../src/core/items.h"
#include "../src/core/sight.h"
#include "../src/core/world.h"

#define ARENA_W 24
#define ARENA_H 24
#define MAX_ROUNDS 400
#define OBJECT_COUNT 13   /* 10 weapons + 3 shields */

/* Loot as object kinds; placed with their field tile. */
static const uint8_t LOOT[OBJECT_COUNT] = {
    OBJ_SWORD, OBJ_KNIFE, OBJ_SPEAR, OBJ_CLUB, OBJ_AXE, OBJ_NINJA_STAR,
    OBJ_SLAYER, OBJ_MAGIC_SLAYER, OBJ_BOW, OBJ_SHIELD, OBJ_SHIELD,
    OBJ_SHIELD,
};

/* Everything the run tally needs, keyed by creature kind. */
typedef struct {
    uint16_t wins;
    uint16_t kills;
    uint16_t deaths;
    uint16_t top4;
    uint32_t rounds_alive;
    uint16_t items;          /* runs in which it grabbed an object */
    uint32_t hits;           /* landed blows (crit check base) */
    uint16_t crits;
    uint16_t runs;
    uint16_t real_deaths;   /* gone by the end, bleeding included */
} Stats;

static World world;
static Rng rng;
static bool skip_kind[CR_COUNT];   /* e.g. the dragons, for a mid-field run */

/* Chebyshev distance without wrap (the arena does not wrap). */
static int dist(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int dx = x0 > x1 ? x0 - x1 : x1 - x0;
    int dy = y0 > y1 ? y0 - y1 : y1 - y0;
    return dx > dy ? dx : dy;
}

static bool field_free(int16_t x, int16_t y)
{
    uint8_t i;
    for (i = 0; i < world.unit_count; i++)
        if (world.units[i].x == x && world.units[i].y == y)
            return false;
    for (i = 0; i < world.object_count; i++)
        if (world.objects[i].x == x && world.objects[i].y == y)
            return false;
    return true;
}

static void place_random(int16_t *x, int16_t *y)
{
    do {
        *x = (int16_t)rng_range(&rng, ARENA_W);
        *y = (int16_t)rng_range(&rng, ARENA_H);
    } while (!field_free(*x, *y));
}

/* One battle: returns the winner's kind (255 = draw/timeout), fills
 * per-kind bookkeeping. */
static uint8_t run_battle(uint32_t seed, Stats *stats, uint16_t *rounds_out)
{
    uint8_t kind;
    int round_no, i;
    uint8_t order[MAX_UNITS];
    bool has_item[MAX_UNITS] = {0};       /* already grabbed the loot */
    bool no_loot[MAX_UNITS] = {0};        /* too heavy - stop chasing */
    uint8_t alive_kinds[CR_COUNT];
    uint8_t winner = 255;

    memset(&world, 0, sizeof world);
    rng_seed(&rng, seed);
    world.w = ARENA_W;
    world.h = ARENA_H;
    for (i = 0; i < ARENA_W * ARENA_H; i++)
        world.floor[i / ARENA_W][i % ARENA_W] = FL_GRASS;
    for (kind = 0; kind < CR_COUNT; kind++) {
        int16_t x, y;
        place_random(&x, &y);
        world.units[kind] = (Unit){0};
        world.units[kind].kind = kind;
        world.units[kind].owner = kind;   /* unique faction: everyone vs everyone */
        world.units[kind].x = (uint8_t)x;
        world.units[kind].y = (uint8_t)y;
        world.units[kind].id = kind;
        world.units[kind].ap = world.units[kind].ap_max =
            CREATURES[kind].ap;
        world.units[kind].sta = world.units[kind].sta_max =
            CREATURES[kind].stamina;
        world.units[kind].con = world.units[kind].con_max =
            CREATURES[kind].con;
        world.units[kind].com = CREATURES[kind].combat;
        world.units[kind].def = CREATURES[kind].defence;
        world.units[kind].native = CREATURES[kind].native;
        stats[kind].runs++;
    }
    world.unit_count = CR_COUNT;
    world.next_id = CR_COUNT;
    for (kind = 0; kind < CR_COUNT; kind++)   /* clean removal keeps ids */
        if (skip_kind[kind]) {
            uint8_t idx = world_find_unit(&world, kind);
            if (idx != NO_UNIT)
                world_remove_unit(&world, idx);
        }
    for (i = 0; i < OBJECT_COUNT; i++) {
        int16_t x, y;
        place_random(&x, &y);
        world.objects[world.object_count].x = (uint8_t)x;
        world.objects[world.object_count].y = (uint8_t)y;
        world.objects[world.object_count].tile = OBJECTS[LOOT[i]].tile;
        world.object_count++;
    }

    for (round_no = 1; round_no <= MAX_ROUNDS; round_no++) {
        /* shuffled initiative: chaotic, not index-ordered */
        for (i = 0; i < CR_COUNT; i++)
            order[i] = (uint8_t)i;         /* IDs: stable across swaps */
        for (i = CR_COUNT - 1; i > 0; i--) {
            uint8_t j = (uint8_t)rng_range(&rng, (uint16_t)(i + 1));
            uint8_t t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        for (i = 0; i < CR_COUNT; i++) {
            uint8_t id = order[i];
            uint8_t unit = world_find_unit(&world, id);
            int guard = 0;
            if (unit == NO_UNIT)
                continue;                  /* died earlier this round */
            world_release(&world, world.units[unit].owner);
            while (world.units[unit].ap >= 4 && guard++ < 40) {
                int16_t px = world.units[unit].x, py = world.units[unit].y;
                /* 1) loot: walk to the nearest seen object, grab it */
                if (!has_item[id] && !no_loot[id] &&
                    world.object_count > 0) {
                    int best = -1, bestd = 1 << 20;
                    uint8_t o;
                    for (o = 0; o < world.object_count; o++) {
                        int d = dist(px, py, world.objects[o].x,
                                     world.objects[o].y);
                        if (d < bestd) {
                            bestd = d;
                            best = o;
                        }
                    }
                    if (bestd == 0) {
                        if (items_pick_up(&world, unit)) {
                            uint8_t k = world.units[unit].item_count - 1;
                            has_item[id] = true;
                            /* wield it when the creature uses weapons */
                            if ((CREATURES[world.units[unit].kind].flags &
                                 CF_WEAPONS) &&
                                OBJECTS[world.units[unit].items[k]].category
                                    == OC_WEAPON)
                                world.units[unit].in_use = k;
                        } else
                            no_loot[id] = true;   /* too heavy for it */
                        continue;
                    }
                    {
                        int8_t dx = 0, dy = 0;
                        if (world.objects[best].x > px)
                            dx = 1;
                        else if (world.objects[best].x < px)
                            dx = -1;
                        if (world.objects[best].y > py)
                            dy = 1;
                        else if (world.objects[best].y < py)
                            dy = -1;
                        if (!world_move_unit(&world, unit, dx, dy)) {
                            if (dx && !world_move_unit(&world, unit, dx, 0))
                                if (dy)
                                    world_move_unit(&world, unit, 0, dy);
                        }
                    }
                    continue;
                }
                /* 2) fight: the nearest living enemy */
                {
                    int best = -1, bestd = 1 << 20;
                    uint8_t e;
                    for (e = 0; e < world.unit_count; e++) {
                        int d;
                        if (world.units[e].id == id)
                            continue;      /* never oneself */
                        d = dist(px, py, world.units[e].x, world.units[e].y);
                        if (d < bestd) {
                            bestd = d;
                            best = e;
                        }
                    }
                    if (best < 0)
                        break;             /* nobody left to fight */
                    if (bestd == 1) {
                        CombatResult r;
                        uint8_t mykind = world.units[unit].kind;
                        uint8_t fkind = world.units[best].kind;
                        if (combat_melee(&world, &rng, unit, (uint8_t)best, &r)) {
                            if (r.hit) {
                                stats[mykind].hits++;
                                if (r.crit)
                                    stats[mykind].crits++;
                            }
                            if (r.return_hit) {
                                stats[fkind].hits++;
                                if (r.return_crit)
                                    stats[fkind].crits++;
                            }
                        }
                        /* a free counter may have killed us mid-action */
                        if (world_find_unit(&world, id) == NO_UNIT)
                            break;
                        continue;          /* swinging until AP run out */
                    }
                    {
                        uint8_t weapon = items_in_use_weapon(&world.units[unit]);
                        if (weapon != WEAPON_NONE &&
                            WEAPONS[weapon].ranged && bestd <= 6 &&
                            sight_has_los(&world, px, py,
                                          world.units[best].x,
                                          world.units[best].y)) {
                            items_fire(&world, &rng, unit,
                                       world.units[best].x,
                                       world.units[best].y, NULL);
                            continue;
                        }
                    }
                    {
                        int8_t dx = 0, dy = 0;
                        if (world.units[best].x > px)
                            dx = 1;
                        else if (world.units[best].x < px)
                            dx = -1;
                        if (world.units[best].y > py)
                            dy = 1;
                        else if (world.units[best].y < py)
                            dy = -1;
                        if (!world_move_unit(&world, unit, dx, dy)) {
                            if (dx && !world_move_unit(&world, unit, dx, 0))
                                if (dy)
                                    world_move_unit(&world, unit, 0, dy);
                        }
                    }
                }
            }
        }
        /* bookkeeping: kills since the last round (world_kill_unit log) */
        for (i = 0; i < world.kill_count; i++) {
            uint8_t ko = world.kills[i].killer_owner;
            uint8_t vo = world.kills[i].victim_owner;
            if (ko < CR_COUNT)
                stats[ko].kills++;
            if (vo < CR_COUNT)
                stats[vo].deaths++;
        }
        world.kill_count = 0;
        if (world.unit_count <= 1)
            break;
        world_new_turn(&world);            /* AP/stamina, bleed, reaction back */
    }

    /* survivors -> placement */
    memset(alive_kinds, 0, sizeof alive_kinds);
    {
        uint8_t alive = 0;
        for (i = 0; i < world.unit_count; i++) {
            kind = world.units[i].kind;
            alive_kinds[kind] = 1;   /* real deaths: bled-out included */
            stats[kind].rounds_alive += round_no;
            if (alive == 0)
                winner = kind;
            alive++;
            if (alive <= 4)
                stats[kind].top4++;
        }
        if (alive > 1)
            winner = 255;                  /* mutual standoff / timeout */
    }
    /* survival credit for the fallen: tracked via deaths is approximate;
       rounds_alive only counts finalists - the table notes this. */
    for (i = 0; i < CR_COUNT; i++) {
        if (has_item[i])
            stats[i].items++;
        if (!skip_kind[i] && !alive_kinds[i])
            stats[i].real_deaths++;
    }
    (void)alive_kinds;
    *rounds_out = round_no;
    return winner;
}

int main(int argc, char **argv)
{
    static Stats stats[CR_COUNT];
    uint16_t runs = argc > 1 ? (uint16_t)atoi(argv[1]) : 100;
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : 1;
    int verbose = argc > 3 && argv[3][0] == 'v';
    if (argc > 3 && strcmp(argv[3], "nodragons") == 0) {
        skip_kind[CR_GOLD_DRAGON] = true;    /* mid-field balance run */
        skip_kind[CR_GREEN_DRAGON] = true;
        skip_kind[CR_RED_DRAGON] = true;
    }
    uint16_t r;
    uint32_t total_rounds = 0;
    uint16_t draws = 0;

    printf("arena: %u runs, seed %lu, %dx%d grass, %d objects\n",
           runs, (unsigned long)seed, ARENA_W, ARENA_H, OBJECT_COUNT);
    for (r = 0; r < runs; r++) {
        uint16_t rounds = 0;
        uint8_t winner = run_battle(seed * 1000 + r + 1, stats, &rounds);
        total_rounds += rounds;
        if (winner < CR_COUNT)
            stats[winner].wins++;
        else
            draws++;
        if (verbose)
            printf("run %3u: winner %-16s after %u rounds\n", r + 1,
                   winner < CR_COUNT ? CREATURES[winner].name : "(draw)",
                   rounds);
    }

    printf("\n%-18s %5s %5s %5s %6s %6s %6s %6s\n",
           "creature", "wins", "top4", "kills", "dead", "crits", "items",
           "VP-w");
    {
        uint8_t k;
        for (k = 0; k < CR_COUNT; k++)
            printf("%-18s %5u %5u %5u %6u %6u %6u %6u\n",
                   CREATURES[k].name, stats[k].wins, stats[k].top4,
                   stats[k].kills, stats[k].real_deaths, stats[k].crits,
                   stats[k].items, CREATURES[k].vp);
    }
    printf("\naverage battle length: %.1f rounds, undecided (timeout): %u\n",
           total_rounds / (double)runs, draws);
    {
        uint32_t hits = 0, crits = 0;
        uint8_t k;
        for (k = 0; k < CR_COUNT; k++) {
            hits += stats[k].hits;
            crits += stats[k].crits;
        }
        printf("total melee hits %lu, criticals %lu (%.1f %%)\n",
               (unsigned long)hits, (unsigned long)crits,
               hits ? 100.0 * crits / hits : 0.0);
    }
    return 0;
}
