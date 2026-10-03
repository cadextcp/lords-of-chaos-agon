#include "area.h"

#include <stddef.h>

#include "combat.h"
#include "gen/data.h"
#include "gen/tiles.h"

/* Areas live in a static pool - there is exactly one battle at a time. */
static Area areas[AREAS_MAX];
static uint8_t area_count;

/* Start values (own design, D7, recorded in the GDD): fire 6, blob 3,
 * vine 2 per round; flood drowns instead. Fire only hits enemies. */
uint8_t area_damage(AreaKind kind)
{
    switch (kind) {
    case AREA_FIRE:
        return 6;
    case AREA_BLOB:
        return 3;
    case AREA_VINE:
        return 2;
    default:
        return 0;
    }
}

bool area_terrain_ok(AreaKind kind, uint8_t floor, uint8_t feature)
{
    switch (kind) {
    case AREA_FIRE:                      /* flammable: grasses, woods, tree */
        return floor == FL_GRASS || floor == FL_TALL_GRASS ||
               floor == FL_WOOD || floor == FL_FOREST ||
               floor == FL_MAGIC_WOOD || floor == FL_SHADOW_WOOD ||
               feature == FE_TREE;
    case AREA_BLOB:                      /* sticky dough: walkable, not water */
        return floor != FL_WATER;
    case AREA_VINE:                      /* vulnerable: grass and forest */
        return floor == FL_GRASS || floor == FL_TALL_GRASS ||
               floor == FL_FOREST;
    case AREA_FLOOD:                     /* anywhere walkable */
        return floor != FL_WATER;
    default:
        return false;
    }
}

static Area *area_of_kind(AreaKind kind)
{
    uint8_t i;
    for (i = 0; i < area_count; i++)
        if (areas[i].kind == kind)
            return &areas[i];
    return NULL;
}

Area *area_at(const World *w, int16_t x, int16_t y)
{
    uint8_t a, f;
    for (a = 0; a < area_count; a++)
        for (f = 0; f < areas[a].count; f++)
            if (areas[a].fields[f] == (uint16_t)(y * w->w + x))
                return &areas[a];
    return NULL;
}

AreaKind area_kind_at(const World *w, int16_t x, int16_t y)
{
    Area *a = area_at(w, x, y);
    return a ? a->kind : AREA_NONE;
}

uint8_t area_power_at(const World *w, int16_t x, int16_t y)
{
    uint8_t a, f;
    for (a = 0; a < area_count; a++)
        for (f = 0; f < areas[a].count; f++)
            if (areas[a].fields[f] == (uint16_t)(y * w->w + x))
                return areas[a].power[f];
    return 0;
}

/* Cast (GDD 7.2, F4): fields of the same kind refresh up to the new
 * level and weaken above it (own fire weakens own fire); a new kind
 * starts as a one-field area. */
bool area_cast(World *w, AreaKind kind, uint8_t level, uint8_t owner,
               int16_t x, int16_t y)
{
    Area *same = area_of_kind(kind);
    if (!world_wrap(w, &x, &y))
        return false;
    if (!area_terrain_ok((AreaKind)kind, w->floor[y][x], w->feature[y][x]))
        return false;                    /* the target must take the stuff */
    if (same) {
        uint8_t f;
        for (f = 0; f < same->count; f++) {
            if (same->power[f] < level)
                same->power[f] = level;
            else if (same->power[f] > 0)
                same->power[f]--;
        }
        if (level > same->strength)
            same->strength = level;
        return true;
    }
    if (area_count >= AREAS_MAX)
        return false;
    {
        Area *a = &areas[area_count++];
        a->kind = (AreaKind)kind;
        a->strength = level;
        a->owner = owner;
        a->count = 1;
        a->fields[0] = (uint16_t)(y * w->w + x);
        a->power[0] = level;
    }
    return true;
}

/* One field's round effect on the units standing there. */
static void area_hit_units(World *w, Area *a, int16_t x, int16_t y,
                           uint8_t damage, Rng *rng)
{
    uint8_t i;
    for (i = w->unit_count; i-- > 0;) {
        Unit *u = &w->units[i];
        if (u->x != x || u->y != y)
            continue;
        if (a->kind == AREA_FIRE && u->owner == a->owner)
            continue;                    /* fire spares its own (GDD) */
        if (damage > 0)
            combat_damage(w, i, damage, CR_WIZARD, a->owner, false, NULL);
        if (a->kind == AREA_FLOOD && u->con > 0 && u->x == x && u->y == y &&
            !(CREATURES[u->kind].native & NATIVE_WATER) &&
            !(u->flags & UF_FLYING) &&
            rng_range(rng, 2) == 0)      /* drowning: 50 % (start value) */
            combat_damage(w, i, u->con, CR_WIZARD, a->owner, false, NULL);
    }
}

uint8_t area_round_end(World *w, Rng *rng)
{
    uint8_t a, f;
    bool changed;

    /* 1. damage on the state before spreading */
    for (a = 0; a < area_count; a++)
        for (f = 0; f < areas[a].count; f++) {
            int16_t x = (int16_t)(areas[a].fields[f] % w->w);
            int16_t y = (int16_t)(areas[a].fields[f] / w->w);
            area_hit_units(w, &areas[a], x, y, area_damage(areas[a].kind),
                           rng);
        }

    /* 2. spread (F4): one attempt per field, chance strength*10 %;
     * new fields start at strength-1, old fields lose 1 */
    for (a = 0; a < area_count; a++) {
        Area *src = &areas[a];
        static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        uint8_t n = src->count;          /* new fields join after the pass */
        for (f = 0; f < n && src->count < AREA_FIELDS_MAX; f++) {
            uint8_t d, pick = 0xFF;
            int16_t x = (int16_t)(src->fields[f] % w->w);
            int16_t y = (int16_t)(src->fields[f] / w->w);
            for (d = 0; d < 8; d++)      /* one attempt per field */
                if (rng_range(rng, 10) < src->strength)
                    pick = d;            /* direction of the last hit */
            if (pick == 0xFF)
                continue;
            {
                int16_t nx = (int16_t)(x + DX[pick]);
                int16_t ny = (int16_t)(y + DY[pick]);
                if (!world_wrap(w, &nx, &ny) ||
                    !area_terrain_ok(src->kind, w->floor[ny][nx],
                                     w->feature[ny][nx]) ||
                    area_at(w, nx, ny))
                    continue;
                src->fields[src->count] = (uint16_t)(ny * w->w + nx);
                src->power[src->count] = src->strength > 0
                                             ? (uint8_t)(src->strength - 1)
                                             : 0;
                src->count++;
            }
        }
        for (f = 0; f < src->count; f++)
            if (src->power[f] > 0)
                src->power[f]--;
    }

    /* 3. prune powerless fields and dead areas */
    do {
        changed = false;
        for (a = area_count; a-- > 0;) {
            uint8_t f2;
            for (f2 = areas[a].count; f2-- > 0;) {
                if (areas[a].power[f2] == 0) {
                    areas[a].fields[f2] = areas[a].fields[areas[a].count - 1];
                    areas[a].power[f2] = areas[a].power[areas[a].count - 1];
                    areas[a].count--;
                    changed = true;
                }
            }
            if (areas[a].count == 0) {
                areas[a] = areas[area_count - 1];
                area_count--;
                changed = true;
            }
        }
    } while (changed);
    return area_count;
}


void area_reset(void)
{
    area_count = 0;
}

uint8_t area_active_count(void)
{
    return area_count;
}

bool area_blocks_kind(const World *w, int16_t x, int16_t y)
{
    Area *a = area_at(w, x, y);
    return a != NULL && (a->kind == AREA_BLOB || a->kind == AREA_VINE) &&
           area_power_at(w, x, y) >= 2;
}
