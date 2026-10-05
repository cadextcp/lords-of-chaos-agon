/*
 * World state: map layers (floor, decor, feature), units and objects.
 * Platform-free; the view (view.h) turns it into tile layers for drawing.
 */
#ifndef LOC_WORLD_H
#define LOC_WORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "map_def.h"

#define MAP_MAX_W 36
#define MAP_MAX_H 36
#define MAX_UNITS 32
#define MAX_OBJECTS 64
#define WORLD_DISTURB 6
#define NO_UNIT 0xFF
#define MAX_KILLS 16

typedef enum {
    FL_STONE, FL_WOOD, FL_GRASS, FL_PATH, FL_TALL_GRASS, FL_FOREST, FL_MAGIC_WOOD,
    FL_SHADOW_WOOD, FL_SWAMP, FL_WATER, FL_RUBBLE, FL_COUNT
} Floor;
typedef enum { DE_NONE, DE_RUG, DE_PENTACLE } Decor;
typedef enum {
    FE_NONE, FE_WALL, FE_DOOR_CLOSED, FE_DOOR_OPEN, FE_BED, FE_BOOKSHELF,
    FE_CANDLE, FE_CAULDRON, FE_TABLE, FE_CHAIR, FE_DRAWERS, FE_CHEST, FE_TREE,
    FE_ROCK, FE_COUNT
} Feature;

/* Status flags shown as panel icons (PM 11). */
enum { UF_UNDEAD = 1, UF_FLYING = 2, UF_MOUNT = 4, UF_WOUNDED = 8,
       UF_INVISIBLE = 16, UF_MAGIC_WEAPON = 32 /* enchanted (M4b Enchant) */,
       /* 64 is UF_RIDDEN (ride.h). The reaction of D29 lives in
        * Unit.reacted: it shared bit 64 with UF_RIDDEN, and the round
        * reset dropped riders out of the world (wizard "died"). */
       UF_ENGAGED = 128 /* bound in melee until its owner's phase ends (GDD 6) */ };

/* Brewing cauldron on a field (M4c, GDD 7.2). */
#define CAULDRONS_MAX 4
typedef struct Cauldron {
    uint8_t x, y;
    uint8_t potion;   /* SP_* potion spell it holds, 0xFF = empty */
    uint8_t doses;    /* draughts left */
    uint8_t level;    /* brewed spell level: strength and duration (F1) */
} Cauldron;

/* Timed effect on a unit (M4b, D22/F1): kind, strength, rounds left. */
typedef enum {
    EFF_SHIELD, EFF_PROTECT, EFF_STRENGTH, EFF_INVISIBLE, EFF_SPEED,
    EFF_FLYING, EFF_MAGIC_WEAPON
} EffectKind;
#define UNIT_EFFECTS 4
typedef struct {
    uint8_t kind;    /* EffectKind */
    uint8_t power;
    uint8_t rounds;
} Effect;

typedef struct {
    uint8_t x, y;
    uint8_t kind;   /* CreatureKind */
    uint8_t owner;  /* Owner */
    uint8_t flags;  /* UF_* */
    uint8_t native; /* NATIVE_* terrain type (pays floor cost there) */
    uint8_t ap, ap_max, ap_fly;
    uint8_t sta, sta_max;     /* stamina */
    uint8_t con, con_max;     /* constitution */
    uint8_t com, def;         /* combat, defence */
    uint8_t mr;               /* magic resistance */
    uint8_t mana, mana_max;   /* wizards only */
    uint8_t items[6];         /* carried object kinds (OBJ_*) */
    uint8_t item_count;
    uint8_t in_use;           /* index into items, 0xFF = bare hands */
    uint8_t id;               /* stable while the unit lives (indices shift) */
    uint8_t rider_kind;       /* kind carried on this mount, 0xFF = none */
    uint8_t post_x, post_y;   /* guard post (M4h) / territory, 0xFF = none */
    uint8_t grudge;           /* wild animals (D35): owners that attacked it */
    uint8_t herd_dir;         /* crossing herd: direction 1..8, 0 = none */
    uint8_t travel;           /* crossing herd: fields walked so far */
    uint8_t group;            /* herd: the leader's id (0 = alone) */
    bool reacted;             /* the round's defensive reaction is spent (D29) */
    uint8_t alarm;            /* alarmed for this many rounds (D37) */
    uint8_t alarm_charge;     /* 1 = attack the disturber, 0 = flee */
    uint8_t alarm_x, alarm_y; /* where the trouble was */
    uint8_t alarm_owner;      /* who caused it */
    bool done;                /* finished for this phase (space, turn.h) */
    Effect effects[UNIT_EFFECTS];   /* timed, tick at the round end (M4b) */
} Unit;

/* One death with its killer, for the VP account (game_credit_kills). */
typedef struct {
    uint8_t victim_kind, victim_owner;
    uint8_t killer_kind, killer_owner;
    bool melee;
} Kill;

typedef struct {
    uint8_t x, y;
    uint16_t tile;   /* TileId */
} Object;

typedef struct {
    uint8_t w, h, wrap;
    uint8_t generation;   /* bumped whenever the map layers change (view cache) */
    uint8_t floor[MAP_MAX_H][MAP_MAX_W];
    uint8_t decor[MAP_MAX_H][MAP_MAX_W];
    uint8_t feature[MAP_MAX_H][MAP_MAX_W];
    Unit units[MAX_UNITS];
    /* aggressive acts this round (D37): x, y, owner - the independents'
     * phase scares the animals nearby */
    uint8_t disturb_n;
    uint8_t disturb[WORLD_DISTURB][3];
    uint8_t unit_count;
    Object objects[MAX_OBJECTS];
    uint8_t object_count;
    int16_t portal_x, portal_y;   /* v3 maps: -1 = none */
    uint8_t portal_rmin, portal_rmax;
    uint8_t next_id;              /* unit ids, see world_spawn_unit */
    Kill kills[MAX_KILLS];        /* deaths not yet credited */
    uint8_t kill_count;
    Cauldron cauldrons[CAULDRONS_MAX];
    uint8_t cauldron_count;
    char save_map[32];            /* map of the running scenario (save) */
    uint8_t roof[MAP_MAX_W * MAP_MAX_H / 8 + 1];   /* v4: bit per field */
} World;

/* Load a binary map (.map, ADR 0008). Validates everything first; on
 * false the world is left unchanged. */
bool world_load_bin(World *w, const uint8_t *data, uint16_t len);
/* Call after changing floor/decor/feature (door opened ...). */
void world_map_changed(World *w);

/* Normalise (x, y) for wrapping maps. Returns false if outside a
 * non-wrapping map. */
bool world_wrap(const World *w, int16_t *x, int16_t *y);

/* Feature at (x, y); FE_NONE outside a non-wrapping map. */
uint8_t world_feature(const World *w, int16_t x, int16_t y);
/* Floor at (x, y); FL_GRASS outside a non-wrapping map. */
uint8_t world_floor(const World *w, int16_t x, int16_t y);
/* Wall or door: forms the connected wall line (GDD 11.2). */
bool world_is_wall_line(const World *w, int16_t x, int16_t y);
/* Blocks sight between ground positions: floor (data/costs.csv) or a
 * tall feature (GDD 3.4). */
bool world_blocks_sight(const World *w, int16_t x, int16_t y);
/* Same test for coordinates already normalised inside the map (ray fast
 * path; see world.c). */
bool world_blocks_sight_at(const World *w, uint8_t x, uint8_t y);
/* Only the feature on (x, y) (wall, tree, closed door ...) blocks sight. */
bool world_feature_blocks_sight(const World *w, uint8_t x, uint8_t y);
/* Roof of the field (v4 maps): blocks sight and landing (GDD 3.2). */
bool world_has_roof(const World *w, int16_t x, int16_t y);
/* Eight blocking flags of row y starting at column x, packed MSB-first;
 * see world.c. */
uint8_t world_sight_byte(const World *w, uint8_t y, uint8_t x);
/* Feature blocks ground movement (GDD 3.3 furniture table). */
bool world_blocks(const World *w, int16_t x, int16_t y);

/* The two layers a unit can occupy per field (GDD 3.1): at most one
 * ground unit and one flying unit share a field. */
typedef enum { UL_GROUND, UL_AIR } UnitLayer;

/* Unit at (x, y) on that layer, NO_UNIT if none (ground layer unless
 * stated). */
uint8_t world_unit_at(const World *w, int16_t x, int16_t y, UnitLayer layer);
/* AP cost to enter (x, y); diagonal steps cost 3/2, rounded up (GDD 5.3). */
uint8_t world_step_cost(const World *w, int16_t x, int16_t y, bool diagonal);
/* Same for a unit: its terrain type (wood/water/rock) pays only the plain
 * floor cost in matching terrain (GDD 5.3). */
uint8_t world_unit_step_cost(const World *w, uint8_t unit, int16_t x, int16_t y,
                             bool diagonal);
/* Flying step (GDD 5.3): constant, whatever the ground below looks like. */
uint8_t world_air_step_cost(bool diagonal);
/* Move a unit one step (8 directions); false if blocked, occupied, outside
 * or not enough AP. Airborne units ignore terrain and ground units and
 * only respect the air layer. Spends the AP on success. */
bool world_move_unit(World *w, uint8_t unit, int8_t dx, int8_t dy);
/* Take off (<) / land (>): pay the action cost (actions.csv), switch
 * layers. Landing needs a free ground slot and no roof (v4 maps)
 * (GDD 3.1, deferred). */
bool world_take_off(World *w, uint8_t unit);
bool world_land(World *w, uint8_t unit);
/* Spend AP and half of it as stamina (GDD 5.3). */
void world_spend(World *w, uint8_t unit, uint8_t ap);
/* Remove a unit (swap with the last): indices of other units may change,
 * so callers re-find units by id (world_find_unit, turn_revalidate). */
void world_remove_unit(World *w, uint8_t unit);
/* A wild animal (D35) remembers who attacked it and fights back. */
void world_provoke(World *w, uint8_t unit, uint8_t attacker_owner);
/* Note an aggressive act at (x, y) by owner (D37); a full list keeps the
 * first ones. */
void world_disturb(World *w, int16_t x, int16_t y, uint8_t owner);
/* A unit dies by someone's hand: its carried objects drop onto its
 * field (D21), the kill is logged for the VP account (game_credit_kills)
 * unless the killer is independent, then the unit is removed. Killer kind and owner are passed by value - the killer itself
 * may already be gone (lightning splash). */
void world_kill_unit(World *w, uint8_t victim, uint8_t killer_kind,
                     uint8_t killer_owner, bool melee);
/* Add a freshly initialised unit (summons) with a fresh id; returns its
 * index. */
uint8_t world_spawn_unit(World *w, uint8_t owner, uint8_t kind, uint8_t x, uint8_t y);
/* Chebyshev distance between two fields, honouring wrap-around. */
uint8_t world_distance(const World *w, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
/* Offset from (x0, y0) to (x1, y1), the shortest way on wrapping maps. */
void world_delta(const World *w, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 int16_t *dx, int16_t *dy);
/* Index of the unit with this id, NO_UNIT when it is gone. */
uint8_t world_find_unit(const World *w, uint8_t id);
/* Ground unit that was engaged this turn (UF_ENGAGED) and still stands
 * next to a living enemy: bound (GDD 6), only the attack remains. The
 * binding ends with its owner's next phase. */
bool world_engaged(const World *w, uint8_t unit);
/* Living ground enemy adjacent to the unit (flag-free: for the free
 * swing rule D26, regardless of phase flags). */
bool world_enemy_adjacent(const World *w, uint8_t unit);
/* Melee contact: the unit and every ground enemy next to it become bound
 * (a move next to an enemy, an attack). */
void world_engage(World *w, uint8_t unit);
/* The owner's phase is over: his units are free to move again. */
void world_release(World *w, uint8_t owner);
/* Round end: fatal wounds bleed (PM 17), the bled-out drop their
 * objects; refill AP - the layer budget
 * while flying (ap_fly), halved when exhausted (PM 12) - recover 25 %
 * stamina (GDD 5.3), regenerate 4 % mana; bleeders that reach 0 are
 * removed. */
void world_new_turn(World *w);

/* Why a step fails, for bump messages (GDD 5.1). */
typedef enum {
    BUMP_OK,        /* the step would succeed */
    BUMP_NO_AP,     /* destination fine, not enough AP */
    BUMP_DOOR,      /* closed door: try world_open_door */
    BUMP_UNIT,      /* a unit blocks the layer */
    BUMP_TERRAIN,   /* impassable feature (attack on terrain, M3) */
    BUMP_HELD,      /* strong blob or vine on the field (M4d) */
    BUMP_OUTSIDE    /* outside a non-wrapping map */
} BumpKind;
BumpKind world_bump_kind(const World *w, uint8_t unit, int8_t dx, int8_t dy);
/* Bump-open a closed door (GDD 5.1): creatures with hands (CF_USE) pay
 * the action cost, the door opens and the view cache is invalidated. */
bool world_open_door(World *w, uint8_t unit, int16_t x, int16_t y);

/* Character for dumps (floor/feature/unit at a glance). */
char world_char(const World *w, int16_t x, int16_t y);

#endif
