/*
 * Host duel simulator (D67 KI, phase 5): two AI wizards on a scenario map,
 * both run through the real AI (decision loop, spell choice, routes) and the
 * real core rules. Prints who wins, how long it takes and which of the
 * enemy wizard's spells he casts - the numbers the AI gets tuned against.
 *
 * Build (WSL):
 *   gcc -std=c99 -Wall -Wextra -O2 -Isrc/core -o build/host/duel \
 *       $(find src/core -name '*.c') host/duel.c
 * Run:
 *   build/host/duel [games=20] [scenario=0..2] [seed=1] [rounds=60]
 *
 * Player 2 is the scenario's wizard (values, priorities of K10.2); player 1
 * gets the scenario's player book, the designer's standard template and the
 * default priority list. No GUI, no VDP.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/ai.h"
#include "../src/core/area.h"
#include "../src/core/effect.h"
#include "../src/core/events.h"
#include "../src/core/game.h"
#include "../src/core/gen/data.h"
#include "../src/core/gen/maps.h"
#include "../src/core/gen/scenarios.h"
#include "../src/core/items.h"
#include "../src/core/populate.h"
#include "../src/core/sight.h"
#include "../src/core/spells.h"
#include "../src/core/turn.h"
#include "../src/core/wizard.h"
#include "../src/core/world.h"

static World world;
static Game game;
static Spellbook books[OWN_NEUTRAL];
static AiProfile profiles[OWN_NEUTRAL];

typedef struct {
    const uint8_t *map;
    uint16_t map_len;
    const uint8_t *scn;
    uint16_t scn_len;
    const char *name;
} Scen;

static Scen SCENS[3];

static void scen_init(void)
{
    SCENS[0] = (Scen){MAPBIN_MANY_COLOURED_LAND, MAPBIN_MANY_COLOURED_LAND_LEN,
                      SCN_MANY_COLOURED_LAND, SCN_MANY_COLOURED_LAND_LEN, "Many Coloured Land"};
    SCENS[1] = (Scen){MAPBIN_SLAYERS_DUNGEON, MAPBIN_SLAYERS_DUNGEON_LEN, SCN_SLAYERS_DUNGEON,
                      SCN_SLAYERS_DUNGEON_LEN, "Slayer's Dungeon"};
    SCENS[2] = (Scen){MAPBIN_RAGARILS_DOMAIN, MAPBIN_RAGARILS_DOMAIN_LEN, SCN_RAGARILS_DOMAIN,
                      SCN_RAGARILS_DOMAIN_LEN, "Ragaril's Domain"};
}

int main(int argc, char **argv)
{
    int games = argc > 1 ? atoi(argv[1]) : 20;
    int sc = argc > 2 ? atoi(argv[2]) : 0;
    unsigned seed = argc > 3 ? (unsigned)atoi(argv[3]) : 1;
    int max_rounds = argc > 4 ? atoi(argv[4]) : 60;
    int g, s, wins[3] = {0, 0, 0}, kills_p1 = 0, kills_p2 = 0;
    long rounds_sum = 0;
    long casts[SPELL_COUNT];
    long creatures_p1 = 0, creatures_p2 = 0;

    scen_init();
    if (sc < 0 || sc > 2)
        sc = 0;
    memset(casts, 0, sizeof casts);
    for (g = 0; g < games; g++) {
        Rng rng;
        Turns t;
        AiCtx ctx;
        uint8_t initial[SPELL_COUNT];
        int round, outcome = 0;
        uint8_t i;
        rng_seed(&rng, seed + (unsigned)g * 977u);
        area_reset();
        world_load_bin(&world, SCENS[sc].map, SCENS[sc].map_len);
        memset(books, 0, sizeof books);
        spellbook_load(books, SCENS[sc].scn, SCENS[sc].scn_len);
        ai_scenario_load(&world, profiles, SCENS[sc].scn, SCENS[sc].scn_len);
        wizard_slot_reset(0);
        wizard_apply_standard_set(&wizard_slots[0]);
        memcpy(&books[OWN_P1], wizard_book(&wizard_slots[0]), sizeof(Spellbook));
        wizard_apply_to_world(&wizard_slots[0], &world, ai_wizard_of(&world, OWN_P1));
        memset(&t, 0, sizeof t);
        turn_init(&t, &world, seed + (unsigned)g, 0);
        populate_scenario(&world, &t.rng);
        game_init(&game, world.portal_x, world.portal_y, world.portal_rmin,
                  world.portal_rmax, &t.rng);
        game_set_portal_span(&game, world.portal_span);
        ai_profile_apply(profiles, &world, &game, OWN_P2);
        memset(&ctx, 0, sizeof ctx);
        ctx.books = books;
        ctx.game = &game;
        ctx.profiles = profiles;
        for (s = 0; s < SPELL_COUNT; s++)
            initial[s] = books[OWN_P2].level[s];
        for (round = 1; round <= max_rounds && !outcome; round++) {
            uint8_t o;
            t.round = (uint8_t)round;
            game_new_round(&game, t.round);
            ai_run_hunters(&world, &t.rng, OWN_NEUTRAL, NO_UNIT);
            for (o = OWN_P1; o <= OWN_P2; o++) {
                t.phase = o;
                ai_wizard_phase(&t, &world, &ctx);
            }
            area_round_end(&world, &t.rng);
            world_new_turn(&world);
            if (game_over(&game, &world) ||
                game_outcome(&game, &world, OWN_P1) != OUT_RUNNING ||
                game_outcome(&game, &world, OWN_P2) != OUT_RUNNING) {
                GameOutcome a = game_outcome(&game, &world, OWN_P1);
                GameOutcome b = game_outcome(&game, &world, OWN_P2);
                outcome = 3;
                if (a == OUT_WIN && b != OUT_WIN)
                    outcome = 1;
                else if (b == OUT_WIN && a != OUT_WIN)
                    outcome = 2;
                else if (a == OUT_LOSE && b == OUT_RUNNING)
                    outcome = 2;
                else if (b == OUT_LOSE && a == OUT_RUNNING)
                    outcome = 1;
                else if (game.vp[OWN_P1] != game.vp[OWN_P2])
                    outcome = game.vp[OWN_P1] > game.vp[OWN_P2] ? 1 : 2;
            }
        }
        if (!outcome)
            outcome = game.vp[OWN_P1] == game.vp[OWN_P2] ? 3
                    : game.vp[OWN_P1] > game.vp[OWN_P2] ? 1 : 2;
        wins[outcome - 1]++;
        rounds_sum += round - 1;
        kills_p1 += game.kills[OWN_P1];
        kills_p2 += game.kills[OWN_P2];
        for (s = 0; s < SPELL_COUNT; s++)
            casts[s] += (long)initial[s] - books[OWN_P2].level[s];
        for (i = 0; i < world.unit_count; i++) {
            if (world.units[i].owner == OWN_P1)
                creatures_p1++;
            if (world.units[i].owner == OWN_P2)
                creatures_p2++;
        }
    }
    printf("Duell: %s, %d Partien, Seed %u, hoechstens %d Runden\n", SCENS[sc].name, games,
           seed, max_rounds);
    printf("  Spieler 1 (Standardzauberer): %d Siege\n", wins[0]);
    printf("  Spieler 2 (%s): %d Siege\n", profiles[OWN_P2].name, wins[1]);
    printf("  unentschieden: %d\n", wins[2]);
    printf("  Runden im Mittel: %.1f   Kills je Partie P1 %.1f / P2 %.1f\n",
           games ? (double)rounds_sum / games : 0.0, games ? (double)kills_p1 / games : 0.0,
           games ? (double)kills_p2 / games : 0.0);
    printf("  Einheiten am Ende (Mittel): P1 %.1f / P2 %.1f\n",
           games ? (double)creatures_p1 / games : 0.0, games ? (double)creatures_p2 / games : 0.0);
    printf("  Zauber des Gegners (gewirkt, alle Partien):\n");
    for (s = 0; s < SPELL_COUNT; s++)
        if (casts[s])
            printf("    %-18s %ld\n", SPELLS[s].name, casts[s]);
    return 0;
}
