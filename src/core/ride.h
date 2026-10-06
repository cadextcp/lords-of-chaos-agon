/*
 * Riding (GDD 4.2, M4e): mounts (CF_MOUNT) carry riders (CF_RIDE -
 * wizards, pixies, dwarves, goblins, trolls). The rider occupies the
 * mount: the pair is ONE unit on the map (the mount's kind with a
 * rider flag), moves and fights with its own AP, and the Tab order
 * lists the pair once. `b` mounts a friendly mount standing next to the
 * unit; dismounting needs a free field. Riders may attack from a
 * friendly field (D21 exception). The rider acts from the saddle (D60):
 * spells, doors, chests and picking up go by his kind (ride_actor_kind),
 * his values wait in the mount until he gets off.
 */
#ifndef LOC_RIDE_H
#define LOC_RIDE_H

#include <stdbool.h>
#include <stdint.h>

#include "world.h"

enum { UF_RIDDEN = 64 };   /* this mount carries its owner-unit inside */

/* Mount `rider` on the friendly mount at (x, y): the mount absorbs the
 * rider (ACT_RIDE), the rider unit vanishes from the list. False when
 * no mount, wrong owner, no permission or no AP. */
bool ride_mount(World *w, uint8_t rider, int16_t x, int16_t y);
/* Mount the first friendly mount on any of the eight neighbour fields
 * (the `b` key). False when none is there or ride_mount refuses. */
bool ride_mount_adjacent(World *w, uint8_t rider);
/* Dismount: the rider reappears on a free field next to the mount
 * (ACT_DISMOUNT). */
bool ride_dismount(World *w, uint8_t mounted);
/* The mount `mount` (a copy - it is already gone) died: its rider lands
 * on the field, or next to it, with his own values and pack (D60). */
void ride_throw_off(World *w, const Unit *mount);
/* May this unit attack from a field where a friend stands? Riders
 * (UF_RIDDEN) can (D21: "except for riders"). */
bool ride_may_attack_from(const World *w, uint8_t attacker);
/* The creature that acts for this unit: the rider on a ridden mount
 * (it has the hands and the spells, D60), else the unit itself. */
uint8_t ride_actor_kind(const Unit *u);
/* Mount of the rider kind carried by this unit (CreatureKind), 0xFF
 * when it carries nobody. */
uint8_t ride_rider_kind(const Unit *mounted);

#endif
