/*
 * Map data shared by the core and tools/gen_maps.py (ADR 0008).
 *
 * Maps are written as text (data/maps/<name>.txt) and compiled into the
 * binary .map format (see tools/gen_maps.py). The game loads them from
 * /loc/maps on the SD card; the same bytes are compiled into
 * src/core/gen/maps.c for the selftest and the host build.
 * gen_maps.py reads the enum values below directly from this header.
 */
#ifndef LOC_MAP_DEF_H
#define LOC_MAP_DEF_H

#include <stdint.h>

typedef enum { CR_WIZARD, CR_GOBLIN, CR_COUNT } CreatureKind;
typedef enum { OWN_P1, OWN_P2, OWN_P3, OWN_P4, OWN_NEUTRAL, OWN_COUNT } Owner;

#define MAPBIN_VERSION 1
#define MAPBIN_HEADER 10

extern const uint8_t MAPBIN_WIZARD_HOUSE[];
extern const uint16_t MAPBIN_WIZARD_HOUSE_LEN;

#endif
