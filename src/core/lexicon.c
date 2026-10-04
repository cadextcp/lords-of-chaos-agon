#include "lexicon.h"

#include <string.h>

#include "gen/data.h"

#define LEX_MAGIC "LOCL"
#define LEX_VERSION 1
#define LEX_BYTES 12   /* 4 creature bits + 8 object bits, both u32 LE */

void lexicon_init(Lexicon *l)
{
    memset(l, 0, sizeof *l);
}

void lexicon_see_creature(Lexicon *l, uint8_t kind)
{
    if (kind < CR_COUNT && kind < 32)
        l->seen_creature |= (uint32_t)1 << kind;
}

void lexicon_see_object(Lexicon *l, uint8_t kind)
{
    if (kind < 64)
        l->seen_object[kind / 32] |= (uint32_t)1 << (kind % 32);
}

bool lexicon_seen_creature(const Lexicon *l, uint8_t kind)
{
    if (kind >= CR_COUNT || kind >= 32)
        return false;
    return (l->seen_creature & ((uint32_t)1 << kind)) != 0;
}

bool lexicon_seen_object(const Lexicon *l, uint8_t kind)
{
    if (kind >= OBJ_COUNT || kind >= 64)
        return false;
    return (l->seen_object[kind / 32] & ((uint32_t)1 << (kind % 32))) != 0;
}

uint8_t lexicon_seen_count(const Lexicon *l)
{
    uint8_t n = 0, i;
    for (i = 0; i < CR_COUNT && i < 32; i++)
        n += lexicon_seen_creature(l, i) ? 1 : 0;
    for (i = 0; i < OBJ_COUNT && i < 64; i++)
        n += lexicon_seen_object(l, i) ? 1 : 0;
    return n;
}

void lexicon_watch(Lexicon *l, const World *w, const Sight *s)
{
    uint8_t i;
    for (i = 0; i < w->unit_count; i++) {
        const Unit *u = &w->units[i];
        if (u->owner == s->owner || sight_visible(s, w, u->x, u->y))
            lexicon_see_creature(l, u->kind);
    }
    for (i = 0; i < w->object_count; i++) {
        int16_t kind;
        if (!sight_visible(s, w, w->objects[i].x, w->objects[i].y))
            continue;
        kind = lexicon_object_kind_of_tile(w->objects[i].tile);
        if (kind >= 0)
            lexicon_see_object(l, (uint8_t)kind);
    }
}

int16_t lexicon_object_kind_of_tile(uint16_t tile)
{
    uint16_t k;
    for (k = 0; k < OBJ_COUNT; k++)
        if (OBJECTS[k].tile == tile)
            return (int16_t)k;
    return -1;
}

uint16_t lexicon_export(const Lexicon *l, uint8_t *buf, uint16_t cap)
{
    if (cap < 4 + 1 + LEX_BYTES)
        return 0;
    memcpy(buf, LEX_MAGIC, 4);
    buf[4] = LEX_VERSION;
    buf[5] = (uint8_t)(l->seen_creature & 0xFF);
    buf[6] = (uint8_t)(l->seen_creature >> 8);
    buf[7] = (uint8_t)(l->seen_creature >> 16);
    buf[8] = (uint8_t)(l->seen_creature >> 24);
    buf[9] = (uint8_t)(l->seen_object[0] & 0xFF);
    buf[10] = (uint8_t)(l->seen_object[0] >> 8);
    buf[11] = (uint8_t)(l->seen_object[0] >> 16);
    buf[12] = (uint8_t)(l->seen_object[0] >> 24);
    buf[13] = (uint8_t)(l->seen_object[1] & 0xFF);
    buf[14] = (uint8_t)(l->seen_object[1] >> 8);
    buf[15] = (uint8_t)(l->seen_object[1] >> 16);
    buf[16] = (uint8_t)(l->seen_object[1] >> 24);
    return 4 + 1 + LEX_BYTES;
}

bool lexicon_import(Lexicon *l, const uint8_t *buf, uint16_t len)
{
    if (len != 4 + 1 + LEX_BYTES || memcmp(buf, LEX_MAGIC, 4) != 0 ||
        buf[4] != LEX_VERSION)
        return false;
    l->seen_creature = (uint32_t)buf[5] | ((uint32_t)buf[6] << 8) |
                       ((uint32_t)buf[7] << 16) | ((uint32_t)buf[8] << 24);
    l->seen_object[0] = (uint32_t)buf[9] | ((uint32_t)buf[10] << 8) |
                        ((uint32_t)buf[11] << 16) | ((uint32_t)buf[12] << 24);
    l->seen_object[1] = (uint32_t)buf[13] | ((uint32_t)buf[14] << 8) |
                        ((uint32_t)buf[15] << 16) | ((uint32_t)buf[16] << 24);
    /* bits beyond the table sizes would never show: drop them */
    if (CR_COUNT < 32)
        l->seen_creature &= (uint32_t)((1ul << CR_COUNT) - 1);
    if (OBJ_COUNT > 32)
        l->seen_object[1] &= (uint32_t)((1ul << (OBJ_COUNT - 32)) - 1);
    else {
        if (OBJ_COUNT < 32)
            l->seen_object[0] &= (uint32_t)((1ul << (OBJ_COUNT & 31)) - 1);
        l->seen_object[1] = 0;
    }
    return true;
}
