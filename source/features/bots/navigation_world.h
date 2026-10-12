#ifndef HALO_NAVIGATION_WORLD_H
#define HALO_NAVIGATION_WORLD_H
#include "engine_ai/surface_navigation.h"
/* Derived, host-local data. Reset before BSP/map/game-state storage changes. */
void navigation_world_reset(void);
void navigation_world_update_for_players(void);
const struct sn_resource *navigation_world_resource(void);
uint64_t navigation_world_obstacle_revision(void);
/* context points to the unit datum index to ignore, not to an ECS entity.
   segment_clear is the live check (everything a living biped collides with);
   search_clear is for route searches: structure and scenery only, with cached
   per-edge verdicts, so a route is not refused by a body that will move. Both
   refuse temporarily blocked polygons. */
bool navigation_world_segment_clear(void *context, struct sn_point a, uint32_t ap,
    struct sn_point b, uint32_t bp);
bool navigation_world_search_clear(void *context, struct sn_point a, uint32_t ap,
    struct sn_point b, uint32_t bp);
/* A bot following its route: vehicles and machines only (bipeds are steered around). */
bool navigation_world_occupant_clear(void *context, struct sn_point a, uint32_t ap,
    struct sn_point b, uint32_t bp);
/* Temporary blockages bump the global obstacle revision (the probe uses it). Bots
   instead check their own upcoming polygons, so an unrelated blockage elsewhere
   on the map does not discard their routes. */
/* Validate a short ascent at the biped's current momentum with a capsule sweep
   along its ballistic arc and a supported landing. Never mutates the body. */
bool navigation_world_jump_clear(long unit_index, struct sn_point start, struct sn_point goal);
void navigation_world_block(uint32_t polygon);
bool navigation_world_polygon_blocked(uint32_t polygon);
#endif
