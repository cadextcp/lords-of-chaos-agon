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
#include "../src/core/effect.h"
#include "../src/core/events.h"
#include "../src/core/gen/data.h"
#include "../src/core/gen/tiles.h"
#include "../src/core/items.h"
#include "../src/core/sight.h"
#include "../src/core/spells.h"
#include "../src/core/world.h"

#define ARENA_W 24
#define ARENA_H 24
#define MAX_ROUNDS 400
#define OBJECT_COUNT 13   /* 10 weapons + 3 shields */
#define WIZ_ARCH 8        /* wizard duel archetypes */

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

/* Per-weapon tally: how often held, damage per landed hit, holder wins.
 * Index WEAPON_COUNT = bare hands. Shields are tracked separately. */
typedef struct {
    uint32_t picks;          /* battle starts or pickups with this weapon */
    uint32_t hits;
    uint32_t dmg;
    uint16_t holder_wins;
} WStats;

static Stats stats[CR_COUNT];
static WStats wstats[WEAPON_COUNT + 1];
static uint16_t shield_holder_wins, shield_picks;

static int dist(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

static World world;
static Rng rng;
static bool skip_kind[CR_COUNT];   /* e.g. the dragons, for a mid-field run */
static bool equip_random;         /* everyone spawns with a random item */
static bool wizard_duel;          /* 8 wizards with books instead of the FFA */

/* Wizard duel: book archetypes (levels = charges), keyed by wizard id. */
static Spellbook books[WIZ_ARCH];
static const char *const ARCH_NAME[WIZ_ARCH] = {
    "Blaster", "Evoker", "Beschwoerer", "Nekromant", "Allround",
    "Ketzer", "Pyromane", "Kriegermagier",
};
static uint16_t arch_wins[WIZ_ARCH], arch_wiz_kills[WIZ_ARCH],
    arch_summon_kills[WIZ_ARCH], arch_deaths[WIZ_ARCH];
static uint32_t spell_use[SPELL_COUNT];   /* casts, via the event ring */

static void arch_book(uint8_t a, uint8_t spell, uint8_t level)
{
    books[a].level[spell] = level;
}

static void init_books(void)
{
    memset(books, 0, sizeof books);
    arch_book(0, SP_MAGIC_BOLT, 4);      /* Blaster */
    arch_book(0, SP_MAGIC_SHIELD, 2);
    arch_book(1, SP_MAGIC_LIGHTNING, 2); /* Evoker */
    arch_book(1, SP_MAGIC_BOLT, 2);
    arch_book(2, SP_DWARF, 2);           /* Beschwoerer */
    arch_book(2, SP_GOBLIN, 2);
    arch_book(2, SP_GIANT_BAT, 2);
    arch_book(3, SP_ZOMBIE, 2);          /* Nekromant */
    arch_book(3, SP_GHOST, 1);
    arch_book(3, SP_SPECTRE, 1);
    arch_book(4, SP_MAGIC_BOLT, 2);      /* Allround */
    arch_book(4, SP_DWARF, 1);
    arch_book(4, SP_MAGIC_SHIELD, 1);
    arch_book(4, SP_MAGIC_EYE, 1);
    arch_book(5, SP_CURSE, 2);           /* Ketzer */
    arch_book(5, SP_SUBVERSION, 1);
    arch_book(5, SP_MAGIC_ATTACK, 1);
    arch_book(5, SP_MAGIC_BOLT, 1);
    arch_book(6, SP_MAGIC_FIRE, 2);      /* Pyromane */
    arch_book(6, SP_MAGIC_BOLT, 1);
    arch_book(6, SP_MAGIC_SHIELD, 1);
    arch_book(7, SP_MAGIC_SHIELD, 2);    /* Kriegermagier */
    arch_book(7, SP_MAGIC_BOLT, 1);
}

/* Drain the event ring and tally spell casts. */
static void tally_events(void)
{
    static GameEvent ev[EVENT_RING];
    uint8_t n, i;
    n = events_drain(ev, EVENT_RING);
    for (i = 0; i < n; i++)
        if (ev[i].type == EV_SPELL && ev[i].kind < SPELL_COUNT) {
            spell_use[ev[i].kind]++;
            if (getenv("ARENA_KDBG"))
                fprintf(stderr, "SDBG spell=%u owner=%u\n", ev[i].kind,
                        ev[i].owner);
        }
}

/* Credit a kill to a faction (owner) - the core's kill log only records
 * owners 0..4, the sim attributes all eight factions itself. */
static void arch_kills(uint8_t owner, bool by_wizard)
{
    if (!wizard_duel || owner >= WIZ_ARCH)
        return;
    if (by_wizard)
        arch_wiz_kills[owner]++;
    else
        arch_summon_kills[owner]++;
}

/* One wizard's spell phase: shield, attack spell, summon. Returns true
 * when a cast happened (an action was spent). */
static bool wizard_casts(uint8_t unit)
{
    Unit *u = &world.units[unit];
    uint8_t b = u->id;                    /* book index */
    int best = -1, bestd = 1 << 20;
    uint8_t e;
    if (u->kind != CR_WIZARD)
        return false;
    for (e = 0; e < world.unit_count; e++) {
        int d;
        if (world.units[e].id == u->id)
            continue;
        d = dist(u->x, u->y, world.units[e].x, world.units[e].y);
        if (d < bestd) {
            bestd = d;
            best = e;
        }
    }
    if (best < 0)
        return false;
    /* keep a shield up (2x level defence, 2x level rounds) */
    if (books[b].level[SP_MAGIC_SHIELD] && !effect_active(u, EFF_SHIELD)) {
        SpellShot shot;
        if (spell_can_cast(&world, &books[b], unit, SP_MAGIC_SHIELD)) {
            spell_apply(&world, &books[b], unit, SP_MAGIC_SHIELD,
                        u->x, u->y, &rng, &shot);
            tally_events();
            return true;
        }
    }
    /* attack the nearest enemy in range and sight */
    if (bestd <= 6 && sight_has_los(&world, u->x, u->y,
                                    world.units[best].x,
                                    world.units[best].y)) {
        SpellShot shot;
        int16_t ex = world.units[best].x, ey = world.units[best].y;
        static const uint8_t ATTACKS[] = {
            SP_MAGIC_LIGHTNING, SP_MAGIC_BOLT, SP_MAGIC_ATTACK,
            SP_CURSE, SP_MAGIC_FIRE,
        };
        uint8_t k;
        for (k = 0; k < sizeof ATTACKS / sizeof ATTACKS[0]; k++) {
            uint8_t s = ATTACKS[k];
            uint8_t before = world.unit_count;
            if (!books[b].level[s] || !spell_can_cast(&world, &books[b], unit, s))
                continue;
            if (s == SP_MAGIC_BOLT || s == SP_MAGIC_LIGHTNING) {
                if (s == SP_MAGIC_LIGHTNING)
                    spell_lightning(&world, &books[b], unit, ex, ey, &rng,
                                    &shot);
                else
                    spell_bolt(&world, &books[b], unit, s, ex, ey, &rng,
                               &shot);
            } else {
                CastResult cr = spell_apply(&world, &books[b], unit, s, ex, ey,
                                            &rng, &shot);
                if (cr != CAST_OK)
                    continue;             /* e.g. wrong terrain for fire */
            }
            tally_events();
            if (world.unit_count < before)
                arch_kills(u->owner, true);
            return true;
        }
    }
    /* summon while there is room */
    if (world.unit_count < MAX_UNITS - 2) {
        static const uint8_t SUMMONS[] = {
            SP_SPECTRE, SP_GHOST, SP_ZOMBIE, SP_GIANT_BAT, SP_GOBLIN,
            SP_DWARF,
        };
        uint8_t k;
        for (k = 0; k < sizeof SUMMONS / sizeof SUMMONS[0]; k++) {
            uint8_t s = SUMMONS[k];
            if (!books[b].level[s] || !spell_can_cast(&world, &books[b], unit, s))
                continue;
            if (spell_summon(&world, &books[b], unit, s)) {
                tally_events();
                return true;
            }
        }
    }
    return false;
}

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
static uint8_t run_battle(uint32_t seed, uint16_t *rounds_out)
{
    uint8_t kind;
    int round_no, i;
    uint8_t order[MAX_UNITS];
    uint8_t order_n = 0;
    uint8_t held[MAX_UNITS] = {0};         /* objects grabbed so far (max 2) */
    bool no_loot[MAX_UNITS] = {0};        /* too heavy - stop chasing */
    uint8_t alive_kinds[CR_COUNT];
    uint8_t winner = 255;

    memset(&world, 0, sizeof world);
    rng_seed(&rng, seed);
    world.w = ARENA_W;
    world.h = ARENA_H;
    for (i = 0; i < ARENA_W * ARENA_H; i++)
        world.floor[i / ARENA_W][i % ARENA_W] = FL_GRASS;
    if (wizard_duel) {
        init_books();                     /* fresh charges every battle */
        /* 8 wizards, one per book archetype, ids/owners 0..7 */
        for (kind = 0; kind < WIZ_ARCH; kind++) {
            int16_t x, y;
            place_random(&x, &y);
            world.units[kind] = (Unit){0};
            world.units[kind].kind = CR_WIZARD;
            world.units[kind].owner = kind;
            world.units[kind].x = (uint8_t)x;
            world.units[kind].y = (uint8_t)y;
            world.units[kind].id = kind;
            world.units[kind].ap = world.units[kind].ap_max =
                CREATURES[CR_WIZARD].ap;
            world.units[kind].sta = world.units[kind].sta_max =
                CREATURES[CR_WIZARD].stamina;
            world.units[kind].con = world.units[kind].con_max =
                CREATURES[CR_WIZARD].con;
            world.units[kind].com = CREATURES[CR_WIZARD].combat;
            world.units[kind].def = CREATURES[CR_WIZARD].defence;
            world.units[kind].in_use = NO_ITEM;
            world.units[kind].mana = world.units[kind].mana_max =
                CREATURES[CR_WIZARD].mana;
            world.unit_count = (uint8_t)(kind + 1);
        }
        world.next_id = WIZ_ARCH;
        {   /* a few weapons and shields to fight over */
            static const uint8_t WLOOT[6] = {
                OBJ_SWORD, OBJ_AXE, OBJ_SLAYER, OBJ_SHIELD, OBJ_SHIELD,
                OBJ_BOW,
            };
            for (i = 0; i < 6; i++) {
                int16_t x, y;
                place_random(&x, &y);
                world.objects[world.object_count].x = (uint8_t)x;
                world.objects[world.object_count].y = (uint8_t)y;
                world.objects[world.object_count].tile =
                    OBJECTS[WLOOT[i]].tile;
                world.object_count++;
            }
        }
    }
    if (!wizard_duel)
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
        world.units[kind].in_use = NO_ITEM;  /* like map-loaded units */
        stats[kind].runs++;
        if (equip_random) {              /* random item at spawn (carry cap) */
            uint8_t obj = LOOT[rng_range(&rng, OBJECT_COUNT)];
            if (OBJECTS[obj].weight <= CREATURES[kind].carry) {
                Unit *u = &world.units[kind];
                u->items[0] = obj;
                u->item_count = 1;
                held[kind] = 1;   /* equipped ones still look for a 2nd */
                if (OBJECTS[obj].weapon == WEAPON_SHIELD) {
                    shield_picks++;      /* carried, defends anyway (D21/D28) */
                } else if (OBJECTS[obj].category == OC_WEAPON &&
                           (CREATURES[kind].flags & CF_WEAPONS)) {
                    u->in_use = 0;       /* wielded right away */
                    wstats[OBJECTS[obj].weapon].picks++;
                }
            }
        }
    }
    world.unit_count = CR_COUNT;
    world.next_id = CR_COUNT;
    for (kind = 0; kind < CR_COUNT; kind++)   /* clean removal keeps ids */
        if (skip_kind[kind]) {
            uint8_t idx = world_find_unit(&world, kind);
            if (idx != NO_UNIT)
                world_remove_unit(&world, idx);
        }
    if (!wizard_duel)
        for (i = 0; i < OBJECT_COUNT; i++) {
            int16_t x, y;
            place_random(&x, &y);
            world.objects[world.object_count].x = (uint8_t)x;
            world.objects[world.object_count].y = (uint8_t)y;
            world.objects[world.object_count].tile = OBJECTS[LOOT[i]].tile;
            world.object_count++;
        }

    for (round_no = 1; round_no <= MAX_ROUNDS; round_no++) {
        /* shuffled initiative over the living, whatever their ids */
        {
            int n = 0, k;
            for (k = 0; k < world.unit_count; k++)
                order[n++] = world.units[k].id;
            for (i = n - 1; i > 0; i--) {
                uint8_t j = (uint8_t)rng_range(&rng, (uint16_t)(i + 1));
                uint8_t t2 = order[i];
                order[i] = order[j];
                order[j] = t2;
            }
            order_n = (uint8_t)n;
        }
        for (i = 0; i < order_n; i++) {
            uint8_t id = order[i];
            uint8_t unit = world_find_unit(&world, id);
            int guard = 0;
            if (unit == NO_UNIT)
                continue;                  /* died earlier this round */
            world_release(&world, world.units[unit].owner);
            while (world.units[unit].ap >= 4 && guard++ < 40) {
                int16_t px = world.units[unit].x, py = world.units[unit].y;
                /* 0) wizards work their books first (duel mode) */
                if (wizard_casts(unit))
                    continue;
                /* 1) loot: walk to the nearest seen object, grab it */
                if (held[id] < 2 && !no_loot[id] &&
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
                            uint8_t obj = world.units[unit].items[k];
                            held[id]++;
                            if (OBJECTS[obj].weapon == WEAPON_SHIELD) {
                                shield_picks++;   /* carried only (D21/D28) */
                            } else if (OBJECTS[obj].category == OC_WEAPON) {
                                /* wield it when the creature uses weapons */
                                if (CREATURES[world.units[unit].kind].flags &
                                    CF_WEAPONS)
                                    world.units[unit].in_use = k;
                                wstats[OBJECTS[obj].weapon].picks++;
                            }
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
                /* 2) fight: the nearest living enemy (never own side) */
                {
                    int best = -1, bestd = 1 << 20;
                    uint8_t e;
                    for (e = 0; e < world.unit_count; e++) {
                        int d;
                        if (world.units[e].owner == world.units[unit].owner)
                            continue;      /* own faction, own summons */
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
                                uint8_t w = items_in_use_weapon(&world.units[unit]);
                                if (w == WEAPON_NONE)
                                    w = WEAPON_COUNT;
                                stats[mykind].hits++;
                                wstats[w].hits++;
                                wstats[w].dmg += r.damage;
                                if (r.crit)
                                    stats[mykind].crits++;
                            }
                            if (r.return_hit) {
                                uint8_t w2 = items_in_use_weapon(&world.units[best]);
                                if (w2 == WEAPON_NONE)
                                    w2 = WEAPON_COUNT;
                                stats[fkind].hits++;
                                wstats[w2].hits++;
                                wstats[w2].dmg += r.return_damage;
                                if (r.return_crit)
                                    stats[fkind].crits++;
                            }
                            if (wizard_duel) {   /* kills past the log gate */
                                if (r.died)
                                    arch_kills(world.units[unit].owner,
                                               mykind == CR_WIZARD);
                                if (r.attacker_died)
                                    arch_kills(world.units[best].owner,
                                               fkind == CR_WIZARD);
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
            if (wizard_duel)
                continue;                  /* attributed at the call sites */
            if (ko < CR_COUNT)
                stats[ko].kills++;
            if (vo < CR_COUNT)
                stats[vo].deaths++;
        }
        world.kill_count = 0;
        if (wizard_duel) {
            uint8_t wiza = 0;
            for (i = 0; i < world.unit_count; i++)
                if (world.units[i].kind == CR_WIZARD)
                    wiza++;
            if (wiza <= 1)
                break;
        } else if (world.unit_count <= 1)
            break;
        world_new_turn(&world);            /* AP/stamina, bleed, reaction back */
    }

    if (wizard_duel) {
        uint8_t a, wiza = 0, last = 255;
        for (a = 0; a < WIZ_ARCH; a++) {
            if (world_find_unit(&world, a) == NO_UNIT)
                arch_deaths[a]++;          /* exact, no log gate */
            else {
                wiza++;
                last = a;
            }
        }
        winner = wiza == 1 ? last : 255;
        *rounds_out = round_no;
        return winner;
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
        if (held[i])
            stats[i].items++;
        if (!skip_kind[i] && !alive_kinds[i])
            stats[i].real_deaths++;
    }
    /* winner's gear: which item won the battle */
    if (winner < CR_COUNT) {
        uint8_t idx = world_find_unit(&world, winner);
        if (idx != NO_UNIT) {
            uint8_t w = items_in_use_weapon(&world.units[idx]);
            uint8_t s;
            if (w == WEAPON_NONE)
                w = WEAPON_COUNT;
            wstats[w].holder_wins++;
            for (s = 0; s < world.units[idx].item_count; s++)
                if (OBJECTS[world.units[idx].items[s]].weapon == WEAPON_SHIELD)
                    shield_holder_wins++;
        }
    }
    (void)alive_kinds;
    *rounds_out = round_no;
    return winner;
}

/* Spearman rank correlation with average ranks for ties. */
static double spearman(const uint16_t *a, const uint16_t *b, int n)
{
    int i, j;
    double ra[64] = {0}, rb[64] = {0};
    double sumd2 = 0;
    for (i = 0; i < n; i++) {
        int lt = 0, eq = 0;
        for (j = 0; j < n; j++) {
            if (a[j] < a[i])
                lt++;
            else if (a[j] == a[i])
                eq++;
        }
        ra[i] = lt + (eq + 1) / 2.0;
        lt = eq = 0;
        for (j = 0; j < n; j++) {
            if (b[j] < b[i])
                lt++;
            else if (b[j] == b[i])
                eq++;
        }
        rb[i] = lt + (eq + 1) / 2.0;
    }
    for (i = 0; i < n; i++) {
        double d = ra[i] - rb[i];
        sumd2 += d * d;
    }
    return 1.0 - 6.0 * sumd2 / ((double)n * n * n - n);
}

/* Summon mana at level 1 for the creature, 0 when not summonable. */
static uint16_t summon_cost(uint8_t kind)
{
    uint16_t s;
    for (s = 0; s < SPELL_COUNT; s++)
        if (SUMMON_KIND[s] == kind)
            return (uint16_t)(SPELLS[s].mana_base + SPELLS[s].mana_step);
    return 0;
}

int main(int argc, char **argv)
{
    uint16_t runs = argc > 1 ? (uint16_t)atoi(argv[1]) : 100;
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : 1;
    int a;
    int verbose = 0;
    for (a = 3; a < argc; a++) {        /* flags in any order */
        if (argv[a][0] == 'v' && argv[a][1] == 0)
            verbose = 1;
        else if (strcmp(argv[a], "nodragons") == 0) {
            skip_kind[CR_GOLD_DRAGON] = true;    /* mid-field balance run */
            skip_kind[CR_GREEN_DRAGON] = true;
            skip_kind[CR_RED_DRAGON] = true;
        } else if (strcmp(argv[a], "equipped") == 0)
            equip_random = true;        /* random item on every creature */
        else if (strcmp(argv[a], "wizards") == 0) {
            wizard_duel = true;         /* 8 book archetypes fight it out */
            init_books();
        }
    }
    uint16_t r;
    uint32_t total_rounds = 0;
    uint16_t draws = 0;
    static uint16_t win_first[CR_COUNT], win_second[CR_COUNT];
    uint16_t half = (uint16_t)(runs / 2);

    printf("arena: %u runs, seed %lu, %dx%d grass%s%s\n",
           runs, (unsigned long)seed, ARENA_W, ARENA_H,
           wizard_duel ? ", wizard duel (8 archetypes)" :
           (skip_kind[CR_GOLD_DRAGON] ? ", no dragons" : ""),
           !wizard_duel && equip_random ? ", random equipment" : "");
    for (r = 0; r < runs; r++) {
        uint16_t rounds = 0;
        uint8_t winner = run_battle(seed * 1000 + r + 1, &rounds);
        total_rounds += rounds;
        if (wizard_duel) {
            if (winner < WIZ_ARCH)
                arch_wins[winner]++;
            else
                draws++;
            if (verbose)
                printf("run %3u: winner %-16s after %u rounds\n", r + 1,
                       winner < WIZ_ARCH ? ARCH_NAME[winner] : "(draw)",
                       rounds);
            continue;
        }
        if (winner < CR_COUNT) {
            stats[winner].wins++;
            if (r < half)
                win_first[winner]++;
            else
                win_second[winner]++;
        } else
            draws++;
        if (verbose)
            printf("run %3u: winner %-16s after %u rounds\n", r + 1,
                   winner < CR_COUNT ? CREATURES[winner].name : "(draw)",
                   rounds);
    }

    if (wizard_duel) {
        uint8_t k;
        printf("\n%-16s %5s %7s %8s %7s\n", "archetype", "wins", "kills",
               "bySumms", "deaths");
        for (k = 0; k < WIZ_ARCH; k++)
            printf("%-16s %5u %7u %8u %7u\n", ARCH_NAME[k], arch_wins[k],
                   arch_wiz_kills[k], arch_summon_kills[k], arch_deaths[k]);
        printf("\naverage battle length: %.1f rounds, undecided: %u\n",
               total_rounds / (double)runs, draws);
        printf("%-18s %7s\n", "spell", "casts");
        for (k = 0; k < SPELL_COUNT; k++)
            if (spell_use[k])
                printf("%-18s %7lu\n", SPELLS[k].name,
                       (unsigned long)spell_use[k]);
        return 0;
    }

    printf("\n%-18s %5s %5s %5s %6s %6s %6s %4s %5s\n",
           "creature", "wins", "top4", "kills", "dead", "crits", "items",
           "VP", "cost");
    {
        uint8_t k;
        for (k = 0; k < CR_COUNT; k++)
            printf("%-18s %5u %5u %5u %6u %6u %6u %4u %5u\n",
                   CREATURES[k].name, stats[k].wins, stats[k].top4,
                   stats[k].kills, stats[k].real_deaths, stats[k].crits,
                   stats[k].items, CREATURES[k].vp, summon_cost(k));
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
    {   /* plausibility: win share vs the game's own valuations */
        uint16_t vp[CR_COUNT], cost[CR_COUNT], wins[CR_COUNT];
        uint8_t k, n = 0, nc = 0;
        uint16_t c[CR_COUNT];
        double rho_vp, rho_cost;
        for (k = 0; k < CR_COUNT; k++) {
            if (skip_kind[k])
                continue;
            wins[n] = stats[k].wins;
            vp[n] = CREATURES[k].vp;
            if (summon_cost(k)) {
                cost[nc] = summon_cost(k);
                c[nc] = stats[k].wins;
                nc++;
            }
            n++;
        }
        rho_vp = spearman(wins, vp, n);
        rho_cost = spearman(c, cost, nc);
        printf("Spearman wins vs VP: %.2f (n=%u), wins vs summon cost: %.2f (n=%u)\n",
               rho_vp, n, rho_cost, nc);
        {
            /* stability: same correlation over each half of the runs */
            uint16_t w1[CR_COUNT], w2[CR_COUNT], v[CR_COUNT];
            uint8_t m = 0;
            for (k = 0; k < CR_COUNT; k++) {
                if (skip_kind[k])
                    continue;
                w1[m] = win_first[k];
                w2[m] = win_second[k];
                v[m] = CREATURES[k].vp;
                m++;
            }
            printf("halves: rho VP first %.2f, second %.2f\n",
                   spearman(w1, v, m), spearman(w2, v, m));
        }
    }
    {   /* items by rarity: does the damage and the win share follow the
         * weapon tiers (knife < spear/club/bow < sword < axe/slayer <
         * magic slayer)? */
        uint8_t w;
        printf("\n%-14s %6s %7s %9s %7s\n", "weapon", "picks", "hits",
               "dmg/hit", "wins");
        for (w = 0; w <= WEAPON_COUNT; w++) {
            if (wstats[w].picks == 0 && wstats[w].hits == 0)
                continue;
            printf("%-14s %6lu %7lu %9.1f %7u\n",
                   w == WEAPON_COUNT ? "(waffenlos)" : WEAPONS[w].name,
                   (unsigned long)wstats[w].picks,
                   (unsigned long)wstats[w].hits,
                   wstats[w].hits ? wstats[w].dmg / (double)wstats[w].hits : 0.0,
                   wstats[w].holder_wins);
        }
        printf("shields: %lu carried, %u winner carried one\n",
               (unsigned long)shield_picks, shield_holder_wins);
    }
    return 0;
}
