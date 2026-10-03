#include "save.h"

#include <string.h>

#define SAVE_MAGIC "LOCSG"
#define SAVE_VERSION 3   /* v3: Turns.on_ai callback (M5c); v2 saves rejected */

/* Length of the blob: header + the member images (no struct padding). */
static uint16_t blob_size(void)
{
    const SaveGame *s = 0;
    return (uint16_t)(5 + 1 + sizeof s->world + sizeof s->turns
                      + sizeof s->game + OWN_NEUTRAL * sizeof(Spellbook) + 1
                      + sizeof s->explored + 1 + sizeof s->areas);
}

uint16_t save_serialize(const SaveGame *s, uint8_t *out, uint16_t cap)
{
    const uint16_t need = blob_size();
    uint16_t i;
    uint8_t *p;
    if (cap < need)
        return 0;
    p = out;
    memcpy(p, SAVE_MAGIC, 5);
    p += 5;
    *p++ = SAVE_VERSION;
    /* world, turns, game, books field by field: the structs contain
     * padding-free byte arrays, so a flat copy is portable between
     * host and eZ80 builds of the same code version */
    memcpy(p, &s->world, sizeof s->world);
    p += sizeof s->world;
    memcpy(p, &s->turns, sizeof s->turns);
    p += sizeof s->turns;
    memcpy(p, &s->game, sizeof s->game);
    p += sizeof s->game;
    for (i = 0; i < OWN_NEUTRAL; i++) {
        memcpy(p, &s->books[i], sizeof(Spellbook));
        p += sizeof(Spellbook);
    }
    *p++ = s->loads_left;
    memcpy(p, s->explored, sizeof s->explored);
    p += sizeof s->explored;
    *p++ = s->area_count;
    memcpy(p, s->areas, sizeof s->areas);
    p += sizeof s->areas;
    return (uint16_t)(p - out);
}

bool save_deserialize(SaveGame *s, const uint8_t *in, uint16_t len)
{
    const uint16_t need = blob_size();
    uint16_t i;
    const uint8_t *p;
    if (len != need || memcmp(in, SAVE_MAGIC, 5) != 0 || in[5] != SAVE_VERSION)
        return false;
    memset(s, 0, sizeof *s);
    p = in + 6;
    memcpy(&s->world, p, sizeof s->world);
    p += sizeof s->world;
    memcpy(&s->turns, p, sizeof s->turns);
    p += sizeof s->turns;
    memcpy(&s->game, p, sizeof s->game);
    p += sizeof s->game;
    for (i = 0; i < OWN_NEUTRAL; i++) {
        memcpy(&s->books[i], p, sizeof(Spellbook));
        p += sizeof(Spellbook);
    }
    s->loads_left = *p++;
    memcpy(s->explored, p, sizeof s->explored);
    p += sizeof s->explored;
    s->area_count = *p++;
    memcpy(s->areas, p, sizeof s->areas);
    /* sanity: the world must be within the map bounds */
    if (s->world.w == 0 || s->world.h == 0 ||
        s->world.w > MAP_MAX_W || s->world.h > MAP_MAX_H ||
        s->area_count > SAVE_AREAS ||
        s->world.unit_count > MAX_UNITS ||
        s->world.object_count > MAX_OBJECTS)
        return false;
    s->world.save_map[sizeof s->world.save_map - 1] = '\0';
    return true;
}

bool save_may_load(const SaveGame *s)
{
    return s->loads_left != 0;
}

uint32_t save_hash(const SaveGame *s)
{
    uint8_t buf[sizeof(SaveGame) + 16];
    uint16_t len = save_serialize(s, buf, sizeof buf);
    uint32_t h = 2166136261UL;
    uint16_t i;
    for (i = 0; i < len; i++)
        h = (h ^ buf[i]) * 16777619UL;
    return h;
}
