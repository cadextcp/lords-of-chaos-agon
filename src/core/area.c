#include "area.h"

#include <stddef.h>

#include "combat.h"
#include "gen/data.h"
#include "gen/tiles.h"
#include "items.h"

/* Areas live in a static pool - there is exactly one battle at a time.
 * One area per (kind, owner): all its fields share the level of the last
 * cast (K5.3). */
static Area areas[AREAS_MAX];
static uint8_t area_count;

uint8_t area_damage(AreaKind kind, uint8_t level)
{
    switch (kind) {
    case AREA_FIRE:
        return (uint8_t)(25 + 2 * level);
    case AREA_BLOB:
        return (uint8_t)(16 + 2 * (level > 0 ? level - 1 : 0));
    case AREA_VINE:
        return 8;
    default:
        return 0;
    }
}

uint8_t area_susceptibility(const World *w, AreaKind kind, int16_t x, int16_t y)
{
    uint8_t fl = w->floor[y][x], fe = w->feature[y][x];
    switch (kind) {
    case AREA_FIRE:
        return FEATURE_FIRE[fe] ? FEATURE_FIRE[fe] : FLOOR_FIRE[fl];
    case AREA_BLOB:
        return FLOOR_BLOB[fl];
    case AREA_VINE:
        return FLOOR_VINE[fl];
    case AREA_FLOOD:
        return FLOOR_FLOOD[fl];
    default:
        return 0;
    }
}

static Area *area_of(AreaKind kind, uint8_t owner)
{
    uint8_t i;
    for (i = 0; i < area_count; i++)
        if (areas[i].kind == kind && areas[i].owner == owner)
            return &areas[i];
    return NULL;
}

uint8_t area_level(AreaKind kind, uint8_t owner)
{
    Area *a = area_of(kind, owner);
    return a && a->strength ? a->strength : 1;
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
    Area *a = area_at(w, x, y);
    return a ? a->strength : 0;
}

/* Does the field take this kind? Fire needs something flammable (trees and
 * furniture burn); the others do not creep into walls and furniture. */
static bool field_takes(const World *w, AreaKind kind, int16_t x, int16_t y)
{
    if (area_susceptibility(w, kind, x, y) == 0)
        return false;
    return kind == AREA_FIRE || !world_blocks(w, x, y);
}

static void area_drop_field(Area *a, uint8_t f)
{
    a->count--;
    a->fields[f] = a->fields[a->count];
    a->power[f] = a->power[a->count];
}

/* Forget empty areas (swap-remove, from the back). */
static void area_prune(void)
{
    uint8_t a;
    for (a = area_count; a-- > 0;)
        if (areas[a].count == 0) {
            areas[a] = areas[area_count - 1];
            area_count--;
        }
}

/* Flood over a burning field puts the fire out (K5.3, overwrite). */
static bool flood_douses(const Area *held, AreaKind kind)
{
    return kind == AREA_FLOOD && held != NULL && held->kind == AREA_FIRE;
}

static void drop_from(Area *a, uint16_t field)
{
    uint8_t f;
    for (f = 0; f < a->count; f++)
        if (a->fields[f] == field) {
            area_drop_field(a, f);
            return;
        }
}

/* Put one field into the (kind, owner) area, starting it if needed. The
 * area's level becomes `level` (K5.3: the level of the last cast). */
static bool place(World *w, AreaKind kind, uint8_t level, uint8_t owner,
                  int16_t x, int16_t y)
{
    Area *same, *held;
    uint16_t target = (uint16_t)(y * w->w + x);
    uint8_t f;
    if (!field_takes(w, kind, x, y))
        return false;
    held = area_at(w, x, y);
    if (held && held->kind == kind && held->owner == owner)
        goto level_up;
    if (held && !flood_douses(held, kind))
        return false;                    /* another effect holds the field */
    same = area_of(kind, owner);
    if (same && same->count >= AREA_FIELDS_MAX)
        return false;
    if (!same && area_count >= AREAS_MAX)
        return false;
    if (held) {
        drop_from(held, target);         /* flood douses the fire */
        area_prune();
    }
    same = area_of(kind, owner);
    if (!same) {
        same = &areas[area_count++];
        same->kind = kind;
        same->owner = owner;
        same->count = 0;
    }
    same->fields[same->count] = target;
    same->power[same->count] = level;
    same->count++;
level_up:
    same = area_of(kind, owner);
    same->strength = level;
    for (f = 0; f < same->count; f++)
        same->power[f] = level;
    world_map_changed(w);               /* fire and blob block the sight (K11.4) */
    return true;
}

bool area_set(World *w, AreaKind kind, uint8_t level, uint8_t owner,
              int16_t x, int16_t y)
{
    if (!world_wrap(w, &x, &y))
        return false;
    return place(w, kind, level, owner, x, y);
}

uint8_t area_cast(World *w, Rng *rng, AreaKind kind, uint8_t level, uint8_t owner,
                  int16_t x, int16_t y)
{
    uint8_t placed = 0;
    if (!world_wrap(w, &x, &y))
        return 0;
    if (kind == AREA_FIRE || kind == AREA_BLOB) {
        uint16_t n = level < 10 ? (uint16_t)(10 - level) : 1;
        if (rng_range(rng, n) < area_susceptibility(w, kind, x, y) &&
            place(w, kind, level, owner, x, y))
            placed = 1;
        else if (area_of(kind, owner))
            area_of(kind, owner)->strength = level;   /* the level follows the last cast */
        return placed;
    }
    {   /* vine and flood: the 9x9 square around the target */
        int16_t dx, dy;
        for (dy = -4; dy <= 4; dy++)
            for (dx = -4; dx <= 4; dx++) {
                int16_t ax = (int16_t)(dx < 0 ? -dx : dx), ay = (int16_t)(dy < 0 ? -dy : dy);
                int16_t d = (int16_t)(ax > ay ? 2 * ax + ay : 2 * ay + ax);
                int16_t nx = (int16_t)(x + dx), ny = (int16_t)(y + dy);
                int16_t n;
                if (d > level + 3 || !world_wrap(w, &nx, &ny))
                    continue;
                n = (int16_t)(18 - 2 * level + 2 * d);
                if (n < 1)
                    n = 1;
                if (rng_range(rng, (uint16_t)n) < area_susceptibility(w, kind, nx, ny) &&
                    place(w, kind, level, owner, nx, ny))
                    placed++;
            }
    }
    return placed;
}

/* Objects that survive fire (flag "fireproof", K7): treasures, cauldrons,
 * keys and the magic weapons. */
static bool fireproof(uint16_t tile)
{
    uint8_t k = items_kind_of_tile(tile);
    if (k == NO_ITEM)
        return false;
    return OBJECTS[k].category == OC_TREASURE || OBJECTS[k].category == OC_KEY ||
           k == OBJ_CAULDRON_EMPTY || k == OBJ_CAULDRON_FULL ||
           k == OBJ_MAGIC_SLAYER;
}

/* Fire destroys what lies on the ground unless it is fireproof; flood
 * washes everything away (K5.3). */
static void area_burn_objects(World *w, int16_t x, int16_t y, bool flood)
{
    uint8_t i;
    for (i = w->object_count; i-- > 0;)
        if (w->objects[i].x == x && w->objects[i].y == y &&
            (flood || !fireproof(w->objects[i].tile))) {
            w->objects[i] = w->objects[w->object_count - 1];
            w->object_count--;
        }
}

/* One field's round effect on the units standing there. Units are
 * visited from the back, so the swap-remove of a kill only moves
 * units that were already handled. */
static void area_hit_units(World *w, Area *a, int16_t x, int16_t y, uint8_t damage,
                           Rng *rng)
{
    uint8_t i;
    for (i = w->unit_count; i-- > 0;) {
        Unit *u = &w->units[i];
        if (u->x != x || u->y != y)
            continue;
        if ((a->kind == AREA_FIRE || a->kind == AREA_BLOB) && u->owner == a->owner)
            continue;                    /* fire and blob spare their own */
        if (a->kind == AREA_FIRE && (u->flags & UF_FLYING))
            continue;                    /* the flames stay on the ground */
        if (damage > 0 &&
            combat_damage(w, i, damage, CR_WIZARD, a->owner, false, NULL, false))
            continue;                    /* dead: the slot holds another now */
        (void)rng;
    }
}

static const uint8_t FIRE_T[8] = {160, 127, 106, 90, 79, 70, 63, 57};
static const uint8_t BLOB_T[8] = {114, 100, 88, 80, 73, 66, 61, 57};

uint8_t area_round_end(World *w, Rng *rng)
{
    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t a, f;

    /* 1. damage and objects on the state before spreading */
    for (a = 0; a < area_count; a++)
        for (f = 0; f < areas[a].count; f++) {
            int16_t x = (int16_t)(areas[a].fields[f] % w->w);
            int16_t y = (int16_t)(areas[a].fields[f] / w->w);
            area_hit_units(w, &areas[a], x, y,
                           area_damage(areas[a].kind, areas[a].strength), rng);
            if (areas[a].kind == AREA_FIRE)
                area_burn_objects(w, x, y, false);
            else if (areas[a].kind == AREA_FLOOD)
                area_burn_objects(w, x, y, true);
        }

    /* 2. fire and blob: spread, then the survival test (K5.3) */
    for (a = 0; a < area_count; a++) {
        Area *src = &areas[a];
        uint8_t n = src->count;          /* new fields join after the pass */
        uint8_t lv, ti;
        const uint8_t *table;
        if (src->kind != AREA_FIRE && src->kind != AREA_BLOB)
            continue;
        lv = src->kind == AREA_FIRE ? src->strength
                                    : (uint8_t)(src->strength > 0 ? src->strength - 1 : 0);
        table = src->kind == AREA_FIRE ? FIRE_T : BLOB_T;
        ti = src->kind == AREA_FIRE ? (uint8_t)(lv < 1 ? 0 : (lv > 8 ? 7 : lv - 1))
                                    : (uint8_t)(lv > 7 ? 7 : lv);
        {   /* the fields that exist now, spread first */
            uint16_t snapshot[AREA_FIELDS_MAX];
            uint8_t k;
            for (k = 0; k < n; k++)
                snapshot[k] = src->fields[k];
            for (k = 0; k < n; k++) {
                uint8_t d;
                int16_t cx = (int16_t)(snapshot[k] % w->w), cy = (int16_t)(snapshot[k] / w->w);
                uint16_t span = (uint16_t)(70 - 3 * (uint16_t)lv);
                if (span < 1)
                    span = 1;
                for (d = 0; d < 8; d++) {
                    int16_t nx = (int16_t)(cx + DX[d]), ny = (int16_t)(cy + DY[d]);
                    uint8_t flam;
                    if (!world_wrap(w, &nx, &ny))
                        continue;
                    flam = area_susceptibility(w, src->kind, nx, ny);
                    if (flam == 0 || (uint16_t)(rng_range(rng, span) + 1) > flam)
                        continue;
                    if (src->count >= AREA_FIELDS_MAX)
                        break;
                    place(w, src->kind, src->strength, src->owner, nx, ny);
                    src = area_of(src->kind, src->owner);   /* pool may have moved */
                    if (!src)
                        break;
                }
                if (!src)
                    break;
            }
        }
        if (!src)
            continue;
        for (f = n; f-- > 0;) {          /* survival of the old fields */
            uint8_t d, sum = 0;
            int16_t cx = (int16_t)(src->fields[f] % w->w), cy = (int16_t)(src->fields[f] / w->w);
            uint16_t need;
            for (d = 0; d < 8; d++) {
                int16_t nx = (int16_t)(cx + DX[d]), ny = (int16_t)(cy + DY[d]);
                if (world_wrap(w, &nx, &ny))
                    sum = (uint8_t)(sum + area_susceptibility(w, src->kind, nx, ny));
            }
            need = (uint16_t)(sum / 4 + 40);
            if (rng_range(rng, table[ti]) < need)
                continue;                /* burns on */
            if (src->kind == AREA_FIRE) {
                if (w->feature[cy][cx] != FE_NONE && FEATURE_FIRE[w->feature[cy][cx]] > 0) {
                    w->feature[cy][cx] = FE_NONE;   /* the tree or furniture is ash */
                    world_map_changed(w);
                }
                if (FLOOR_FIRE[w->floor[cy][cx]] > 4) {
                    w->floor[cy][cx] = FL_PATH;     /* scorched earth does not burn again */
                    world_map_changed(w);
                }
            }
            area_drop_field(src, f);
        }
    }

    /* 3. forget dead areas */
    area_prune();
    world_map_changed(w);               /* the sight map follows the flames */
    return area_count;
}

uint8_t area_export(Area *dst, uint8_t cap)
{
    uint8_t i, n = area_count < cap ? area_count : cap;
    for (i = 0; i < n; i++)
        dst[i] = areas[i];
    return n;
}

void area_import(const Area *src, uint8_t n)
{
    uint8_t i;
    if (n > AREAS_MAX)
        n = AREAS_MAX;
    for (i = 0; i < n; i++)
        areas[i] = src[i];
    area_count = n;
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
    return a != NULL && (a->kind == AREA_BLOB || a->kind == AREA_VINE);
}

uint8_t area_toughness(const World *w, int16_t x, int16_t y)
{
    Area *a = area_at(w, x, y);
    if (a == NULL)
        return 0;
    return a->kind == AREA_VINE ? 40 : (a->kind == AREA_BLOB ? 50 : 0);
}

void area_remove_field(World *w, int16_t x, int16_t y)
{
    Area *a = area_at(w, x, y);
    if (a == NULL)
        return;
    drop_from(a, (uint16_t)(y * w->w + x));
    area_prune();
    world_map_changed(w);
}
