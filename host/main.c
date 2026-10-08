/*
 * Host (PC) frontend for the platform-free core.
 *
 *   loc_host --selftest   run core self-test, exit code 1 on failure
 *   loc_host --dump       print the wizard house as ASCII and exit
 *   loc_host --layers     print the tile layers of every view field
 *   loc_host --map-file <path.map> [x y]  the same for a .map file (map variants)
 *   loc_host --map-layers <n> [x y]  tile layers of every field of map n (0 many coloured
 *                         land, 1 ragaril, 2 slayer, 3 testland, 4 tutorial); tools/art/map_preview.py draws them
 *   loc_host              line-based play: w/a/s/d + Enter, q quits
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/gen/maps.h"
#include "../tests/selftest.h"
#include "../src/core/view.h"
#include "../src/core/world.h"

static World world;

static void print_line(const char *line)
{
    puts(line);
}

static void dump(void)
{
    char line[MAP_MAX_W + 1];
    int16_t x, y;
    for (y = 0; y < world.h; y++) {
        for (x = 0; x < world.w; x++)
            line[x] = world_char(&world, x, y);
        line[world.w] = '\0';
        puts(line);
    }
}

static void layers(void)
{
    uint8_t vx, vy, i;
    view_set_origin(0, 0);
    view_invalidate();
    view_update(&world);
    for (vy = 0; vy < VIEW_H; vy++) {
        for (vx = 0; vx < VIEW_W; vx++) {
            const FieldLayers *f = view_field(vx, vy);
            printf("%u,%u:", vx, vy);
            for (i = 0; i < f->n; i++)
                printf(" %u", f->id[i]);
            putchar('\n');
        }
    }
}

static void print_layers(void)
{
    int16_t x, y;
    uint8_t i;
    printf("%d %d\n", world.w, world.h);
    for (y = 0; y < world.h; y++)
        for (x = 0; x < world.w; x++) {
            FieldLayers f;
            view_compose(&world, x, y, &f);
            for (i = 0; i < f.n; i++)
                printf("%u ", f.id[i]);
            putchar('\n');
        }
}

static void map_layers(int which)
{
    static const uint8_t *const BINS[] = {MAPBIN_MANY_COLOURED_LAND, MAPBIN_RAGARILS_DOMAIN,
                                          MAPBIN_SLAYERS_DUNGEON, MAPBIN_TESTLAND, MAPBIN_TUTORIAL};
    const uint16_t lens[] = {MAPBIN_MANY_COLOURED_LAND_LEN, MAPBIN_RAGARILS_DOMAIN_LEN,
                             MAPBIN_SLAYERS_DUNGEON_LEN, MAPBIN_TESTLAND_LEN,
                             MAPBIN_TUTORIAL_LEN};
    world_load_bin(&world, BINS[which], lens[which]);
    print_layers();
}

/* A .map file from disk, e.g. build/maps/mcl_v03.map (map variants, D57). */
static int map_layers_file(const char *path)
{
    static uint8_t buf[16384];        /* a 46x46 map is ~8.5 KB (D64) */
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f)
        return 1;
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (!world_load_bin(&world, buf, (uint16_t)n))
        return 1;
    print_layers();
    return 0;
}

int main(int argc, char **argv)
{
    int c;

    if (argc > 1 && strcmp(argv[1], "--selftest") == 0) {
        uint16_t fails = core_selftest(print_line);
        puts(fails ? "=== TEST FAIL ===" : "=== TEST PASS ===");
        return fails ? 1 : 0;
    }
    if (argc > 2 && strcmp(argv[1], "--map-layers") == 0) {
        if (argc > 4)                    /* optional viewer: the roof opens on what it sees */
            view_set_roof_viewer((int16_t)atoi(argv[3]), (int16_t)atoi(argv[4]));
        map_layers(atoi(argv[2]));
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "--map-file") == 0) {
        if (argc > 4)
            view_set_roof_viewer((int16_t)atoi(argv[3]), (int16_t)atoi(argv[4]));
        return map_layers_file(argv[2]);
    }
    world_load_bin(&world, MAPBIN_WIZARD_HOUSE, MAPBIN_WIZARD_HOUSE_LEN);
    if (argc > 1 && strcmp(argv[1], "--dump") == 0) {
        dump();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--layers") == 0) {
        layers();
        return 0;
    }

    dump();
    while ((c = getchar()) != EOF && c != 'q') {
        int8_t dx = 0, dy = 0;
        switch (c) {
        case 'w': dy = -1; break;
        case 's': dy = 1; break;
        case 'a': dx = -1; break;
        case 'd': dx = 1; break;
        default: continue;
        }
        if (world_move_unit(&world, 0, dx, dy))
            dump();
    }
    return 0;
}
