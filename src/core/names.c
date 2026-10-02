#include "names.h"

#include "gen/data.h"
#include "gen/tiles.h"

static const char *const FLOOR_NAMES[FL_COUNT] = {
    [FL_STONE] = "Steinboden", [FL_WOOD] = "Holzdielen",
    [FL_GRASS] = "Gras", [FL_PATH] = "Weg", [FL_TALL_GRASS] = "Hohes Gras",
    [FL_FOREST] = "Wald", [FL_MAGIC_WOOD] = "Zauberwald", [FL_SHADOW_WOOD] = "Schattenwald",
    [FL_SWAMP] = "Sumpf", [FL_WATER] = "Wasser", [FL_RUBBLE] = "Geroell",
};

static const char *const FEATURE_NAMES[FE_COUNT] = {
    [FE_NONE] = "", [FE_WALL] = "Wand", [FE_DOOR_CLOSED] = "Tuer (zu)",
    [FE_DOOR_OPEN] = "Tuer (offen)", [FE_BED] = "Bett", [FE_BOOKSHELF] = "Regal",
    [FE_CANDLE] = "Kerzenstaender", [FE_CAULDRON] = "Kessel", [FE_TABLE] = "Tisch",
    [FE_CHAIR] = "Stuhl", [FE_DRAWERS] = "Kommode", [FE_CHEST] = "Truhe",
    [FE_TREE] = "Baum", [FE_ROCK] = "Fels",
};

const char *name_unit(const Unit *u)
{
    switch (u->kind) {
    case CR_WIZARD:
        switch (u->owner) {
        case OWN_P1: return "Zauberer-1";
        case OWN_P2: return "Zauberer-2";
        case OWN_P3: return "Zauberer-3";
        case OWN_P4: return "Zauberer-4";
        default: return "Zauberer";
        }
    default: return u->kind < CR_COUNT ? CREATURES[u->kind].name : "?";
    }
}

const char *name_owner(uint8_t owner)
{
    switch (owner) {
    case OWN_P1: return "Zauberer-1";
    case OWN_P2: return "Zauberer-2";
    case OWN_P3: return "Zauberer-3";
    case OWN_P4: return "Zauberer-4";
    default: return "";
    }
}

const char *name_floor(uint8_t floor)
{
    return floor < FL_COUNT ? FLOOR_NAMES[floor] : "?";
}

const char *name_feature(uint8_t feature)
{
    return feature < FE_COUNT ? FEATURE_NAMES[feature] : "?";
}

const char *name_decor(uint8_t decor)
{
    switch (decor) {
    case DE_RUG: return "Teppich";
    case DE_PENTACLE: return "Pentakel";
    default: return "";
    }
}

const char *name_object(uint16_t tile)
{
    switch (tile) {
    case T_OBJ_SCROLL: return "Schriftrolle";
    default: return "Gegenstand";
    }
}

uint8_t ground_names(const World *w, int16_t x, int16_t y, const char *out[GROUND_MAX])
{
    uint8_t n = 0, i, fe, de;
    if (!world_wrap(w, &x, &y)) {
        out[0] = name_floor(FL_GRASS);
        return 1;
    }
    for (i = 0; i < w->object_count && n < GROUND_MAX; i++)
        if (w->objects[i].x == x && w->objects[i].y == y)
            out[n++] = name_object(w->objects[i].tile);
    fe = w->feature[y][x];
    if (fe != FE_NONE && n < GROUND_MAX)
        out[n++] = name_feature(fe);
    de = w->decor[y][x];
    if (de != DE_NONE && n < GROUND_MAX)
        out[n++] = name_decor(de);
    if (n == 0)
        out[n++] = name_floor(w->floor[y][x]);
    return n;
}
