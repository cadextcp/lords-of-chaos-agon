#include "populate.h"

#include "gen/data.h"

/* Which wild animals live where (D35, D55): the ground under the spawn
 * field picks the animal by the weights in data/habitats.csv - crocodiles in
 * the swamp and on the shore, bears in the forest, spiders in the dead wood.
 * Listed in the order of the table. */
static const uint8_t WILD_KINDS[] = {
    CR_LION, CR_BEAR, CR_GORILLA, CR_UNICORN, CR_GIANT_BAT, CR_GIANT_SPIDER,
    CR_ELEPHANT, CR_PEGASUS, CR_GRYPHON, CR_CROCODILE,
};
static const uint8_t HERD_ANIMALS[] = {CR_ELEPHANT, CR_UNICORN, CR_PEGASUS};

/* Loose finds by ground (most treasure is in chests, items.c). */
static const uint8_t FINDS_OPEN[] = {OBJ_APPLE, OBJ_APPLE, OBJ_CLOVER, OBJ_CRYSTAL};
static const uint8_t FINDS_WOOD[] = {OBJ_MUSHROOM, OBJ_MISTLETOE, OBJ_APPLE,
                                     OBJ_DRAGON_HERB};
static const uint8_t FINDS_MAGIC[] = {OBJ_FAIRYWING, OBJ_MAGIC_MUSHROOM,
                                      OBJ_MAGIC_APPLE, OBJ_MISTLETOE};
static const uint8_t FINDS_DEAD[] = {OBJ_SULPH, OBJ_NITRO, OBJ_RUNE_STONE};
static const uint8_t FINDS_WET[] = {OBJ_SULPH, OBJ_NITRO, OBJ_MUSHROOM};
static const uint8_t FINDS_ROCK[] = {OBJ_CRYSTAL, OBJ_CRYSTAL, OBJ_NITRO};

static const int8_t DIR_X[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t DIR_Y[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

/* Not under a roof (houses), no water, nothing standing or lying there.
 * Dungeon corridors count as open. */
static bool open_field(const World *w, int16_t x, int16_t y)
{
    uint8_t f, i;
    if (x < 0 || y < 0 || x >= w->w || y >= w->h)
        return false;
    f = w->floor[y][x];
    if (f == FL_WATER || world_blocks(w, x, y) || world_has_roof(w, x, y))
        return false;
    if (world_unit_at(w, x, y, UL_GROUND) != NO_UNIT ||
        world_unit_at(w, x, y, UL_AIR) != NO_UNIT)
        return false;
    for (i = 0; i < w->object_count; i++)
        if (w->objects[i].x == x && w->objects[i].y == y)
            return false;
    return true;
}

static bool far_from_wizards(const World *w, int16_t x, int16_t y, uint8_t gap)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].kind == CR_WIZARD &&
            world_distance(w, x, y, w->units[i].x, w->units[i].y) < gap)
            return false;
    return true;
}

/* A random open field at least gap away from every wizard; false when
 * none was found in a fair number of tries. */
static bool random_field(const World *w, Rng *rng, uint8_t gap,
                         int16_t *ox, int16_t *oy)
{
    uint8_t tries;
    for (tries = 0; tries < 200; tries++) {
        int16_t x = (int16_t)rng_range(rng, w->w);
        int16_t y = (int16_t)rng_range(rng, w->h);
        if (open_field(w, x, y) && far_from_wizards(w, x, y, gap)) {
            *ox = x;
            *oy = y;
            return true;
        }
    }
    return false;
}

static bool wet(uint8_t floor)
{
    return floor == FL_SWAMP;
}

/* Water on one of the eight sides (the crocodile's shore). */
static bool by_water(const World *w, int16_t x, int16_t y)
{
    int8_t dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if ((dx || dy) && world_floor(w, (int16_t)(x + dx), (int16_t)(y + dy)) == FL_WATER)
                return true;
    return false;
}

static uint8_t habitat_weight(const World *w, uint8_t kind, int16_t x, int16_t y)
{
    uint8_t wt = HABITAT[kind][w->floor[y][x]];
    if (by_water(w, x, y))
        wt = (uint8_t)(wt + HABITAT_SHORE[kind]);
    return wt;
}

static void put_object(World *w, uint8_t kind, int16_t x, int16_t y)
{
    if (w->object_count >= MAX_OBJECTS)
        return;
    w->objects[w->object_count].x = (uint8_t)x;
    w->objects[w->object_count].y = (uint8_t)y;
    w->objects[w->object_count].tile = OBJECTS[kind].tile;
    w->object_count++;
}

static void spawn_animal(World *w, Rng *rng)
{
    int16_t x, y;
    uint8_t kind = CR_COUNT, slot, tries, i;
    if (w->unit_count + HERD_ROOM >= MAX_UNITS)
        return;
    /* a field whose ground suits somebody; the ground picks the animal */
    for (tries = 0; tries < 12 && kind == CR_COUNT; tries++) {
        uint16_t total = 0, roll;
        if (!random_field(w, rng, POP_WIZARD_GAP, &x, &y))
            return;
        for (i = 0; i < sizeof WILD_KINDS; i++)
            total = (uint16_t)(total + habitat_weight(w, WILD_KINDS[i], x, y));
        if (!total)
            continue;
        roll = (uint16_t)rng_range(rng, total);
        for (i = 0; i < sizeof WILD_KINDS; i++) {
            uint8_t wt = habitat_weight(w, WILD_KINDS[i], x, y);
            if (roll < wt) {
                kind = WILD_KINDS[i];
                break;
            }
            roll = (uint16_t)(roll - wt);
        }
    }
    if (kind == CR_COUNT)
        return;
    slot = world_spawn_unit(w, OWN_NEUTRAL, kind, (uint8_t)x, (uint8_t)y);
    if (slot != NO_UNIT && CREATURES[kind].wild == WILD_TERRITORIAL) {
        w->units[slot].post_x = (uint8_t)x;   /* its territory */
        w->units[slot].post_y = (uint8_t)y;
    }
}

void populate_scenario(World *w, Rng *rng)
{
    uint8_t i, n;
    int16_t x, y;

    n = (uint8_t)(POP_ANIMALS_MIN + rng_range(rng, 4));
    for (i = 0; i < n; i++)
        spawn_animal(w, rng);

    n = (uint8_t)(POP_CHESTS_MIN + rng_range(rng, 3));
    for (i = 0; i < n; i++)
        if (random_field(w, rng, POP_CHEST_GAP, &x, &y)) {
            w->feature[y][x] = rng_range(rng, 2) ? FE_CHEST : FE_CHEST_FREE;
            world_map_changed(w);
        }

    for (i = 0; i < POP_KEYS; i++)
        if (random_field(w, rng, POP_CHEST_GAP, &x, &y))
            put_object(w, OBJ_CHEST_KEY, x, y);

    n = (uint8_t)(POP_FINDS_MIN + rng_range(rng, 4));
    for (i = 0; i < n; i++) {
        uint8_t f;
        if (!random_field(w, rng, POP_CHEST_GAP, &x, &y))
            continue;
        f = w->floor[y][x];
        if (f == FL_FOREST)
            put_object(w, FINDS_WOOD[rng_range(rng, sizeof FINDS_WOOD)], x, y);
        else if (f == FL_MAGIC_WOOD)
            put_object(w, FINDS_MAGIC[rng_range(rng, sizeof FINDS_MAGIC)], x, y);
        else if (f == FL_SHADOW_WOOD)
            put_object(w, FINDS_DEAD[rng_range(rng, sizeof FINDS_DEAD)], x, y);
        else if (f == FL_RUBBLE)
            put_object(w, FINDS_ROCK[rng_range(rng, sizeof FINDS_ROCK)], x, y);
        else if (wet(f))
            put_object(w, FINDS_WET[rng_range(rng, sizeof FINDS_WET)], x, y);
        else
            put_object(w, FINDS_OPEN[rng_range(rng, sizeof FINDS_OPEN)], x, y);
    }
}

static bool herd_out(const World *w)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++)
        if (w->units[i].herd_dir)
            return true;
    return false;
}

bool populate_herd(World *w, Rng *rng, uint8_t round)
{
    uint8_t dir, kind, size, placed = 0, k, tries, leader = 0;
    int16_t x0, y0;
    if (round < HERD_FIRST_ROUND || herd_out(w))
        return false;
    if (rng_range(rng, 100) >= HERD_CHANCE)
        return false;
    size = (uint8_t)(3 + rng_range(rng, 2));
    if (w->unit_count + size + HERD_ROOM > MAX_UNITS)
        return false;
    kind = HERD_ANIMALS[rng_range(rng, sizeof HERD_ANIMALS)];
    /* straight across: east, south, west or north (dir index 2/4/6/0) */
    dir = (uint8_t)(rng_range(rng, 4) * 2);
    if (DIR_X[dir] > 0) {
        x0 = 0;
        y0 = (int16_t)rng_range(rng, w->h);
    } else if (DIR_X[dir] < 0) {
        x0 = (int16_t)(w->w - 1);
        y0 = (int16_t)rng_range(rng, w->h);
    } else if (DIR_Y[dir] > 0) {
        x0 = (int16_t)rng_range(rng, w->w);
        y0 = 0;
    } else {
        x0 = (int16_t)rng_range(rng, w->w);
        y0 = (int16_t)(w->h - 1);
    }
    /* the herd enters side by side along the edge; the first one leads */
    for (k = 0, tries = 0; placed < size && tries < 12; tries++, k++) {
        int16_t off = (int16_t)((k & 1) ? -(int16_t)((k + 1) / 2) : (int16_t)(k / 2));
        int16_t x = (int16_t)(x0 + (DIR_X[dir] == 0 ? off : 0));
        int16_t y = (int16_t)(y0 + (DIR_Y[dir] == 0 ? off : 0));
        uint8_t slot;
        if (!world_wrap(w, &x, &y) || !open_field(w, x, y))
            continue;
        slot = world_spawn_unit(w, OWN_NEUTRAL, kind, (uint8_t)x, (uint8_t)y);
        if (slot == NO_UNIT)
            break;
        w->units[slot].herd_dir = (uint8_t)(dir + 1);
        w->units[slot].travel = 0;
        if (!placed)
            leader = w->units[slot].id;
        w->units[slot].group = leader;
        placed++;
    }
    return placed > 0;
}
