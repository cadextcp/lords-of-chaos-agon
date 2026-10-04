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

/* Does the field take this kind? Terrain fits, and blob, vine and flood
 * do not creep into walls and furniture (fire burns trees, GDD 7.2). */
static bool field_takes(const World *w, AreaKind kind, int16_t x, int16_t y)
{
    return area_terrain_ok(kind, w->floor[y][x], w->feature[y][x]) &&
           (kind == AREA_FIRE || !world_blocks(w, x, y));
}

static void area_drop_field(Area *a, uint8_t f)
{
    a->count--;
    a->fields[f] = a->fields[a->count];
    a->power[f] = a->power[a->count];
}

/* Forget powerless fields and empty areas (swap-remove, from the back). */
static void area_prune(void)
{
    bool changed;
    uint8_t a;
    do {
        changed = false;
        for (a = area_count; a-- > 0;) {
            uint8_t f;
            for (f = areas[a].count; f-- > 0;)
                if (areas[a].power[f] == 0) {
                    area_drop_field(&areas[a], f);
                    changed = true;
                }
            if (areas[a].count == 0) {
                areas[a] = areas[area_count - 1];
                area_count--;
                changed = true;
            }
        }
    } while (changed);
}

/* Flood over a burning field puts the fire out (GDD 7.2, overwrite). */
static bool flood_douses(const Area *held, AreaKind kind)
{
    return kind == AREA_FLOOD && held != NULL && held->kind == AREA_FIRE;
}

static void douse_field(Area *fire, uint16_t field)
{
    uint8_t f;
    for (f = 0; f < fire->count; f++)
        if (fire->fields[f] == field) {
            area_drop_field(fire, f);
            return;
        }
}

/* Cast (GDD 7.2, F4): the target field joins the area of its kind (or
 * starts it); the area's other fields refresh up to the new level and
 * weaken above it (own fire weakens own fire). A field held by another
 * kind refuses the cast, except flood, which douses fire there. */
bool area_cast(World *w, AreaKind kind, uint8_t level, uint8_t owner,
               int16_t x, int16_t y)
{
    Area *same, *held;
    uint16_t target;
    if (!world_wrap(w, &x, &y))
        return false;
    if (!field_takes(w, kind, x, y))
        return false;                    /* the target must take the stuff */
    target = (uint16_t)(y * w->w + x);
    held = area_at(w, x, y);
    if (held && held->kind != kind && !flood_douses(held, kind))
        return false;
    same = area_of_kind(kind);
    if (!held && same && same->count >= AREA_FIELDS_MAX)
        return false;                    /* the new field would not fit */
    if (!held && !same && area_count >= AREAS_MAX)
        return false;
    if (held && held->kind != kind) {
        douse_field(held, target);       /* may leave an empty fire area */
        area_prune();
        same = area_of_kind(kind);       /* pointers moved */
    }
    if (same) {
        uint8_t f;
        bool have = false;
        for (f = 0; f < same->count; f++) {
            if (same->fields[f] == target)
                have = true;
            if (same->power[f] < level)
                same->power[f] = level;
            else if (same->power[f] > 0)
                same->power[f]--;
        }
        if (!have) {
            same->fields[same->count] = target;
            same->power[same->count] = level;
            same->count++;
        }
        if (level > same->strength)
            same->strength = level;
        return true;
    }
    {
        Area *a = &areas[area_count++];
        a->kind = kind;
        a->strength = level;
        a->owner = owner;
        a->count = 1;
        a->fields[0] = target;
        a->power[0] = level;
    }
    return true;
}

/* Objects fire burns (start values, GDD 7.2): paper, wood, plants. */
static bool object_burns(uint8_t tile)
{
    static const uint8_t BURNS[] = {OBJ_SCROLL, OBJ_BOW, OBJ_APPLE,
                                    OBJ_MUSHROOM, OBJ_MAGIC_APPLE,
                                    OBJ_MAGIC_MUSHROOM, OBJ_MISTLETOE,
                                    OBJ_CLOVER, OBJ_DRAGON_HERB};
    uint8_t i;
    for (i = 0; i < sizeof BURNS; i++)
        if (OBJECTS[BURNS[i]].tile == tile)
            return true;
    return false;
}

static void area_burn_objects(World *w, int16_t x, int16_t y)
{
    uint8_t i;
    for (i = w->object_count; i-- > 0;)
        if (w->objects[i].x == x && w->objects[i].y == y &&
            object_burns(w->objects[i].tile)) {
            w->objects[i] = w->objects[w->object_count - 1];
            w->object_count--;
        }
}

/* One field's round effect on the units standing there. Units are
 * visited from the back, so the swap-remove of a kill only moves
 * units that were already handled. */
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
        if (damage > 0 &&
            combat_damage(w, i, damage, CR_WIZARD, a->owner, false, NULL, false))
            continue;                    /* dead: the slot holds another now */
        if (a->kind == AREA_FLOOD &&
            !(CREATURES[u->kind].native & NATIVE_WATER) &&
            !(u->flags & UF_FLYING) &&
            rng_range(rng, 2) == 0)      /* drowning: 50 % (start value) */
            combat_damage(w, i, u->con, CR_WIZARD, a->owner, false, NULL, false);
    }
}

uint8_t area_round_end(World *w, Rng *rng)
{
    uint8_t a, f;

    /* 1. damage on the state before spreading */
    for (a = 0; a < area_count; a++)
        for (f = 0; f < areas[a].count; f++) {
            int16_t x = (int16_t)(areas[a].fields[f] % w->w);
            int16_t y = (int16_t)(areas[a].fields[f] / w->w);
            area_hit_units(w, &areas[a], x, y, area_damage(areas[a].kind),
                           rng);
            if (areas[a].kind == AREA_FIRE)
                area_burn_objects(w, x, y);
        }

    /* 2. spread (F4): one attempt per field with chance strength*10 %
     * in a random direction; new fields start at strength-1 and keep
     * it this round, old fields lose 1 */
    for (a = 0; a < area_count; a++) {
        Area *src = &areas[a];
        static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int8_t DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        uint8_t n = src->count;          /* new fields join after the pass */
        for (f = 0; f < n; f++) {
            uint8_t pick;
            int16_t nx, ny;
            Area *held;
            if (rng_range(rng, 10) >= src->strength)
                continue;
            pick = (uint8_t)rng_range(rng, 8);
            nx = (int16_t)(src->fields[f] % w->w + DX[pick]);
            ny = (int16_t)(src->fields[f] / w->w + DY[pick]);
            if (!world_wrap(w, &nx, &ny) || !field_takes(w, src->kind, nx, ny))
                continue;
            held = area_at(w, nx, ny);
            if (held && !flood_douses(held, src->kind))
                continue;
            if (src->count >= AREA_FIELDS_MAX)
                break;
            if (held)
                douse_field(held, (uint16_t)(ny * w->w + nx));
            src->fields[src->count] = (uint16_t)(ny * w->w + nx);
            src->power[src->count] = src->strength > 0
                                         ? (uint8_t)(src->strength - 1)
                                         : 0;
            src->count++;
        }
        for (f = 0; f < n; f++)
            if (src->power[f] > 0)
                src->power[f]--;
    }

    /* 3. prune powerless fields and dead areas */
    area_prune();
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
    return a != NULL && (a->kind == AREA_BLOB || a->kind == AREA_VINE) &&
           area_power_at(w, x, y) >= 2;
}
