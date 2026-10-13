/* Host-local BSP polygon navigation adapter. No campaign actor changes.
   Double-buffer publication; all work/storage has explicit ceilings. */
#include "cseries.h"
#include "navigation_world.h"
#include "players.h"
#include "bots.h" /* port: effective private-lobby navigation demand */
#include "game.h"
#include "scenario/scenario.h"
#include "structures/structure_bsp_definitions.h"
#include "physics/collision_bsp_definitions.h"
#include "physics/collisions.h"
#include "physics/collision_features.h"
#include "units/units.h"
#include "units/biped_definitions.h"
#include "units/bipeds.h"
#include "physics/physics.h"
#include "cache/cache_files.h"
#include "extensions/extension_api.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HALO_FEATURE_BOTS

int config_boolean(char const *name);
long config_integer(char const *name);
char const *config_string(char const *name);
void platform_log(char const *format, ...);
boolean network_game_distributed_client(void);

/* A temporarily blocked polygon. The entry is remembered for 30 seconds after
   it expires; blocked again in that time, it stays blocked twice as long (2 s
   up to 32 s), so a parked vehicle or an unclimbable lip is learned rather
   than retried every two seconds. */
enum { NAV_BLOCKS = 64, NAV_BLOCK_MEMORY_TICKS = 30*TICKS_PER_SECOND, NAV_BLOCK_STRIKES_MAX = 4 };
struct nav_block { uint32_t polygon; long until, forget; short strikes; };
static struct {
    struct sn_resource resources[2];
    struct sn_build build;
    struct sn_key requested;
    int active, staging;
    uint64_t revision, obstacles;
    enum sn_status status;
    boolean requested_valid;
    long last_tick;
    struct nav_block blocks[NAV_BLOCKS];
    FILE *dump;
    uint32_t dump_cursor;
    char dump_path[512], temporary_path[528];
    clock_t cpu_time;
    /* Static verdicts of the canonical center->portal (low two bits) and
       portal->center (next two bits) halves of each polygon edge, for the
       active resource: 0 unknown, 1 clear, 2 blocked. */
    uint8_t edge_cache[SN_POLYGONS_MAX][SN_VERTICES_MAX];
    unsigned long sweeps, cache_hits, stats_sweeps, stats_hits;
    long stats_tick;
} world;

static struct sn_point nav_point(real_point3d p)
{
    struct sn_point out={p.x,p.y,p.z};
    return out;
}
static boolean key_equal(struct sn_key a, struct sn_key b)
{
    return a.map_identity==b.map_identity && a.overlay_revision==b.overlay_revision &&
        a.checksum==b.checksum && a.bsp_index==b.bsp_index &&
        a.agent_identity==b.agent_identity && a.format_version==b.format_version;
}
static uint64_t name_hash(char const *name)
{
    uint64_t value=UINT64_C(14695981039346656037);
    short i;
    for (i=0; i<256 && name && name[i]; i++)
        value=(value^(unsigned char)name[i])*UINT64_C(1099511628211);
    return value;
}
static uint32_t agent_hash(struct sn_agent agent)
{
    float values[4]={agent.radius,agent.height,agent.minimum_normal_z,agent.projection_height};
    uint32_t hash=2166136261u;
    short i;
    for (i=0;i<4;i++) {
        uint32_t bits;
        memcpy(&bits,&values[i],sizeof(bits));
        hash=(hash^bits)*16777619u;
    }
    return hash;
}
void navigation_world_reset(void)
{
    if (world.dump) { fclose(world.dump); world.dump=NULL; }
    world.active=-1; world.staging=0;
    world.requested_valid=FALSE; world.status=SN_INVALID;
    world.last_tick=NONE;
    world.revision++; world.obstacles++;
    csmemset(world.blocks,0,sizeof(world.blocks));
    csmemset(world.edge_cache,0,sizeof(world.edge_cache));
    world.stats_tick=NONE;
}
const struct sn_resource *navigation_world_resource(void)
{
    if (!world.requested_valid || world.active<0 ||
        !key_equal(world.resources[world.active].key,world.requested)) return NULL;
    /* an editor (Forge) changed the world since it was built */
    if (world.resources[world.active].key.overlay_revision!=halo_extensions_world_edit_revision()) return NULL;
    return &world.resources[world.active];
}
uint64_t navigation_world_obstacle_revision(void) { return world.obstacles; }

/* Decode only walkable, nonbreakable rings. Never invoke legacy NONE reads.
   Every dereference is preceded by count/index checks; loaded tag pointers have
   already passed the cache loader's address validation. */
static int read_surface(void *context, uint32_t index, struct sn_polygon *out)
{
    struct structure_bsp *structure=global_structure_bsp_get();
    struct collision_bsp *bsp;
    struct collision_surface *surface;
    byte const *flags;
    real_plane3d plane;
    long edge_index;
    uint32_t count;
    (void)context;
    if (!structure || structure->collision_bsp.count!=1) return -1;
    bsp=TAG_BLOCK_GET_ELEMENT(&structure->collision_bsp,0,struct collision_bsp);
    if (index>=(uint32_t)bsp->surfaces.count || index>=(uint32_t)structure->pathfinding_surfaces.count) return -1;
    flags=xbox_pointer(structure->pathfinding_surfaces.address);
    if (!flags) return -1;
    if (!(flags[index]&0x40) || (flags[index]&0x80)) return 0;
    surface=TAG_BLOCK_GET_ELEMENT(&bsp->surfaces,index,struct collision_surface);
    if ((uint32_t)(surface->plane_designator&0x7fffffff)>=(uint32_t)bsp->bsp3d.planes.count) return -1;
    bsp3d_get_plane_from_designator(&bsp->bsp3d,surface->plane_designator,&plane);
    if (!valid_real_plane3d(&plane)) return -1;
    out->normal.x=plane.n.i; out->normal.y=plane.n.j; out->normal.z=plane.n.k;
    /* Retail multiplayer low-bit clearance semantics are not verified. Never
       interpret the unused quantization tables as a guaranteed biped envelope.
       Zero explicitly requires CE pill collision checks at query time. */
    out->width=0.f; out->height=0.f;
    edge_index=surface->first_edge_index;
    for (count=0;count<SN_VERTICES_MAX;count++) {
        struct collision_edge *edge;
        struct collision_vertex *vertex;
        short side;
        long neighbor;
        if (edge_index<0 || edge_index>=bsp->edges.count) return -1;
        edge=TAG_BLOCK_GET_ELEMENT(&bsp->edges,edge_index,struct collision_edge);
        side=edge->surface_indices[1]==(long)index;
        if (edge->surface_indices[side]!=(long)index ||
            edge->vertex_indices[side]<0 || edge->vertex_indices[side]>=bsp->vertices.count) return -1;
        neighbor=edge->surface_indices[!side];
        if (neighbor!=NONE && (neighbor<0 || neighbor>=bsp->surfaces.count)) return -1;
        vertex=TAG_BLOCK_GET_ELEMENT(&bsp->vertices,edge->vertex_indices[side],struct collision_vertex);
        out->vertices[count]=nav_point(vertex->point);
        out->neighbors[count]=neighbor==NONE ? SN_NONE : (uint32_t)neighbor;
        edge_index=edge->edge_indices[side];
        if (edge_index==surface->first_edge_index) {
            out->vertex_count=(uint8_t)(count+1);
            return count>=2 ? 1 : -1;
        }
    }
    return -1;
}
static boolean source_valid(struct structure_bsp *s)
{
    struct collision_bsp *bsp;
    if (!s || s->collision_bsp.count!=1 || !xbox_pointer(s->collision_bsp.address)) return FALSE;
    bsp=TAG_BLOCK_GET_ELEMENT(&s->collision_bsp,0,struct collision_bsp);
    return bsp->surfaces.count>0 && bsp->surfaces.count<=SN_SOURCE_MAX &&
        s->pathfinding_surfaces.count==bsp->surfaces.count &&
        xbox_pointer(s->pathfinding_surfaces.address) && xbox_pointer(bsp->surfaces.address) &&
        bsp->edges.count>0 && bsp->edges.count<=MAXIMUM_EDGES_PER_COLLISION_BSP && xbox_pointer(bsp->edges.address) &&
        bsp->vertices.count>0 && bsp->vertices.count<=MAXIMUM_VERTICES_PER_COLLISION_BSP && xbox_pointer(bsp->vertices.address) &&
        bsp->bsp3d.planes.count>0 && bsp->bsp3d.planes.count<=SN_SOURCE_MAX && xbox_pointer(bsp->bsp3d.planes.address);
}
static void dump_begin(const struct sn_resource *r)
{
    char const *path=config_string("debug.nav_dump");
    size_t length;
    if (!path || !path[0]) return;
    length=strlen(path);
    if (length>=sizeof(world.dump_path)) { platform_log("nav: dump path too long"); return; }
    if (length<4 || strcmp(path+length-4,".nav")) {
        platform_log("nav: diagnostic filename must end in .nav (never overwrite a map/overlay)");
        return;
    }
    if (world.dump && fclose(world.dump)) platform_log("nav: previous dump close failed");
    csstrncpy(world.dump_path,path,sizeof(world.dump_path)-1);
    world.dump_path[sizeof(world.dump_path)-1]=0;
    snprintf(world.temporary_path,sizeof(world.temporary_path),"%s.partial",path);
    world.dump=fopen(world.temporary_path,"w"); world.dump_cursor=0;
    if (!world.dump) { platform_log("nav: cannot open diagnostic dump"); return; }
    fprintf(world.dump,"NAV %u %u %.9g %.9g %.9g %.9g\n",SN_FORMAT_VERSION,r->polygon_count,
        r->agent.radius,r->agent.height,r->agent.minimum_normal_z,r->agent.projection_height);
    fprintf(world.dump,"KEY %llu %u %u %u %llu %llu\n",
        (unsigned long long)r->key.map_identity,r->key.checksum,r->key.bsp_index,
        r->key.agent_identity,(unsigned long long)r->key.overlay_revision,
        (unsigned long long)r->revision);
}
/* At most 64 polygons/file rows per simulation tick. Publish the dump atomically. */
static void dump_step(const struct sn_resource *r)
{
    uint32_t work;
    if (!world.dump || !r) return;
    for (work=0;work<64 && world.dump_cursor<r->polygon_count;work++,world.dump_cursor++) {
        const struct sn_polygon *p=&r->polygons[world.dump_cursor];
        uint32_t edge;
        fprintf(world.dump,"P %u %u %u %.9g %.9g %.9g %.9g %.9g",world.dump_cursor,
            p->source_index,p->vertex_count,p->width,p->height,p->normal.x,p->normal.y,p->normal.z);
        for (edge=0;edge<p->vertex_count;edge++)
            fprintf(world.dump," %.9g %.9g %.9g %u",p->vertices[edge].x,p->vertices[edge].y,p->vertices[edge].z,p->neighbors[edge]);
        fputc('\n',world.dump);
    }
    if (ferror(world.dump)) { fclose(world.dump); world.dump=NULL; platform_log("nav: dump write failed"); return; }
    if (world.dump_cursor==r->polygon_count) {
        int closed=fclose(world.dump); world.dump=NULL;
        if (closed || rename(world.temporary_path,world.dump_path)) platform_log("nav: dump publication failed");
        else platform_log("nav: walkable polygon diagnostic dump complete (%u polygons)",r->polygon_count);
    }
}
static void update_resource(long unit_index)
{
    struct unit_datum *unit=unit_get(unit_index);
    struct biped_definition *definition=biped_definition_get(unit->definition_index);
    struct structure_bsp *structure=global_structure_bsp_get();
    struct sn_agent agent;
    struct sn_key key;
    clock_t before;
    agent.radius=definition->biped.collision_radius;
    agent.height=definition->biped.collision_height_standing;
    agent.minimum_normal_z=definition->biped.runtime_minimum_normal_k;
    agent.projection_height=agent.height;
    key.map_identity=name_hash(cache_file_loaded_map_name()); key.checksum=(uint32_t)cache_files_get_checksum();
    key.bsp_index=(uint32_t)global_structure_bsp_index; key.agent_identity=agent_hash(agent);
    key.format_version=SN_FORMAT_VERSION;
    key.overlay_revision=halo_extensions_world_edit_revision();
    if (!world.requested_valid || !key_equal(key,world.requested)) {
        world.requested=key; world.requested_valid=TRUE; world.cpu_time=0;
        if (world.dump) { fclose(world.dump); world.dump=NULL; }
        world.staging=world.active==0 ? 1 : 0;
        if (!source_valid(structure)) { world.status=SN_INVALID; platform_log("nav: invalid BSP source blocks"); return; }
        world.status=sn_build_begin(&world.build,&world.resources[world.staging],
            (uint32_t)structure->pathfinding_surfaces.count,key,++world.revision,agent);
        platform_log("nav: build BSP %d, radius %.3f height %.3f normal-k %.3f, source surfaces %ld",
            global_structure_bsp_index,agent.radius,agent.height,agent.minimum_normal_z,structure->pathfinding_surfaces.count);
    }
    if (world.status==SN_RUNNING) {
        before=clock();
        world.status=sn_build_step(&world.build,&world.resources[world.staging],128,read_surface,NULL);
        world.cpu_time+=clock()-before;
        if (world.status==SN_READY) {
            world.active=world.staging; world.obstacles++;
            csmemset(world.blocks,0,sizeof(world.blocks));
            csmemset(world.edge_cache,0,sizeof(world.edge_cache));
            platform_log("nav: published %u walkable polygons, BSP %d, CPU %.2f ms, resource %lu bytes",
                world.resources[world.active].polygon_count,global_structure_bsp_index,
                1000.0*(double)world.cpu_time/CLOCKS_PER_SEC,(unsigned long)sizeof(struct sn_resource));
            dump_begin(&world.resources[world.active]);
        } else if (world.status!=SN_RUNNING) platform_log("nav: build failed (%s), no partial publication",sn_status_name(world.status));
    }
    dump_step(navigation_world_resource());
}
void navigation_world_update_for_players(void)
{
    struct data_iterator iterator;
    struct player_datum *player;
    long now=game_time_get();
    short i;
    char const *dump=config_string("debug.nav_dump");
    /* The ordinary bots now use this BSP resource too. Keep it dormant when
     * bots and diagnostics are all off so the default build does no mesh work. */
    if (!config_boolean("debug.nav_probe") && (!dump || !dump[0]) &&
        config_integer("bots.count") <= 0 && !bots_navigation_requested()) return;
    if (network_game_distributed_client() || (game_connection()!=_game_connection_local &&
        game_connection()!=_game_connection_network_server)) return;
    if (world.last_tick==now) return;
    if (world.last_tick!=NONE && now<world.last_tick) navigation_world_reset();
    world.last_tick=now;
    for (i=0;i<NAV_BLOCKS;i++) if (world.blocks[i].until && now>=world.blocks[i].until) {
        world.blocks[i].until=0; world.obstacles++;
    }
    /* one line every ten seconds: the cost of route searches' collision sweeps */
    if (world.stats_tick==NONE) world.stats_tick=now;
    else if (now-world.stats_tick>=10*TICKS_PER_SECOND) {
        if (world.sweeps!=world.stats_sweeps || world.cache_hits!=world.stats_hits)
            platform_log("nav: search sweeps %lu, cached edge verdicts %lu (last 10 s)",
                world.sweeps-world.stats_sweeps,world.cache_hits-world.stats_hits);
        world.stats_sweeps=world.sweeps; world.stats_hits=world.cache_hits; world.stats_tick=now;
    }
    data_iterator_new(&iterator,player_data);
    while ((player=data_iterator_next(&iterator))!=NULL) {
        if (player->unit_index!=NONE && object_get(player->unit_index)->object.type==_object_type_biped) {
            update_resource(player->unit_index);
            return;
        }
    }
}
void navigation_world_block(uint32_t polygon)
{
    short i,slot=NONE;
    long now=game_time_get();
    struct nav_block *block;
    const struct sn_resource *r=navigation_world_resource();
    if (!r || polygon>=r->polygon_count) return;
    for (i=0;i<NAV_BLOCKS;i++) {
        block=&world.blocks[i];
        if ((block->until || now<block->forget) && block->polygon==polygon) { slot=i; break; }
        if (slot==NONE && !block->until && now>=block->forget) slot=i;
    }
    if (slot==NONE) {
        platform_log("nav: temporary blockage capacity exhausted; wait rather than corrupt topology");
        return;
    }
    block=&world.blocks[slot];
    if (block->polygon==polygon && (block->until || now<block->forget))
        block->strikes=(short)MIN(block->strikes+1,NAV_BLOCK_STRIKES_MAX);
    else block->strikes=0;
    block->polygon=polygon;
    block->until=now+(2*TICKS_PER_SECOND<<block->strikes);
    block->forget=block->until+NAV_BLOCK_MEMORY_TICKS;
    world.obstacles++;
    if (block->strikes)
        platform_log("nav: polygon %lu blocked again; %ld s",(unsigned long)polygon,2L<<block->strikes);
}
bool navigation_world_polygon_blocked(uint32_t polygon)
{
    short i;
    for (i=0;i<NAV_BLOCKS;i++) if (world.blocks[i].until && world.blocks[i].polygon==polygon) return true;
    return false;
}
static bool sweep_clear(const struct sn_resource *r, long flags, long ignore,
    struct sn_point a, uint32_t ap, struct sn_point b, uint32_t bp)
{
    real_point3d from,clipped;
    real_vector3d delta,clipped_velocity;
    struct collision_plane planes[8];
    float target_z;
    /* CE's pill base is the lower sphere CENTER; its height is the distance
       between sphere centers, not total standing height (biped_get_physics_pill).
       Raise it by radius/normal-k to stay tangent to sloping ground. */
    from.x=a.x; from.y=a.y;
    from.z=a.z+r->agent.radius/r->polygons[ap].normal.z+0.02f;
    target_z=b.z+r->agent.radius/r->polygons[bp].normal.z+0.02f;
    delta.i=b.x-a.x; delta.j=b.y-a.y; delta.k=target_z-from.z;
    collision_move_pill(flags,&from,&delta,
        r->agent.height-2.f*r->agent.radius,r->agent.radius,ignore,
        &clipped,&clipped_velocity,8,planes);
    return fabsf(clipped.x-b.x)<0.08f && fabsf(clipped.y-b.y)<0.08f &&
        fabsf(clipped.z-target_z)<0.15f;
}
static bool blocked_pair(uint32_t ap, uint32_t bp)
{
    short i;
    for (i=0;i<NAV_BLOCKS;i++) if (world.blocks[i].until &&
        (world.blocks[i].polygon==ap || world.blocks[i].polygon==bp)) return true;
    return false;
}
bool navigation_world_segment_clear(void *context, struct sn_point a, uint32_t ap,
    struct sn_point b, uint32_t bp)
{
    const struct sn_resource *r=navigation_world_resource();
    if (!context || !r || ap>=r->polygon_count || bp>=r->polygon_count) return false;
    if (blocked_pair(ap,bp)) return false;
    return sweep_clear(r,_collision_test_for_bipeds_living_flags,*(long *)context,a,ap,b,bp);
}
/* Following: vehicles and machines only. The ground was checked by the route
   search, and a straight sweep from the bot to its waypoint across uneven
   terrain reads a hill as an obstacle; other bipeds are avoided by the bot's
   own steering, which probes around them every tick. */
bool navigation_world_occupant_clear(void *context, struct sn_point a, uint32_t ap,
    struct sn_point b, uint32_t bp)
{
    const struct sn_resource *r=navigation_world_resource();
    long const flags=FLAG(_collision_test_objects_bit)|FLAG(_collision_test_objects_vehicles_bit)|
        FLAG(_collision_test_objects_machines_bit);
    if (!context || !r || ap>=r->polygon_count || bp>=r->polygon_count) return false;
    if (blocked_pair(ap,bp)) return false;
    return sweep_clear(r,flags,*(long *)context,a,ap,b,bp);
}
/* A short ascent on an existing corridor, never an inferred island link.
   Use the body's current momentum rather than assuming immediate run speed. */
bool navigation_world_jump_clear(long unit_index, struct sn_point start, struct sn_point goal)
{
    const struct sn_resource *r=navigation_world_resource();
    struct biped_datum *biped=biped_get(unit_index);
    struct biped_definition *definition=biped_definition_get(biped->definition_index);
    struct sn_jump_arc arc;
    struct sn_point landing;
    real_point3d base,from,clipped;
    real_vector3d delta,velocity;
    struct collision_plane planes[8];
    real height,radius,offset;
    uint32_t polygon,tick;
    float dx=goal.x-start.x,dy=goal.y-start.y,rise=goal.z-start.z;
    if (!r || TEST_FLAG(biped->biped.flags,_biped_airborne_bit) ||
        biped->biped.landing==1 || biped->unit.body_stun>0.f ||
        fabsf(biped->object.up.i)>0.01f || fabsf(biped->object.up.j)>0.01f ||
        biped->object.up.k<0.99f || rise<0.3f || rise>1.6f || dx*dx+dy*dy>6.25f) return false;
    if (sn_project(r,goal,&polygon,&landing)!=SN_FOUND ||
        fabsf(landing.z-goal.z)>0.15f || navigation_world_polygon_blocked(polygon)) return false;
    if (!sn_jump_candidate(start,goal,biped->object.translational_velocity.i,
        biped->object.translational_velocity.j,definition->biped.jump_velocity,
        global_gravity,0.25f,&arc)) return false;
    biped_get_physics_pill(unit_index,&base,&height,&radius);
    offset=base.z-start.z+0.02f;
    from=base; from.z+=0.02f;
    for (tick=1; tick<=arc.ticks; tick++) {
        struct sn_point p=arc.points[tick];
        delta.i=p.x-from.x; delta.j=p.y-from.y; delta.k=p.z+offset-from.z;
        collision_move_pill(_collision_test_for_bipeds_living_flags,&from,&delta,
            height,radius,unit_index,&clipped,&velocity,8,planes);
        if (fabsf(clipped.x-p.x)>0.05f || fabsf(clipped.y-p.y)>0.05f ||
            fabsf(clipped.z-p.z-offset)>0.1f) return false;
        from=clipped;
    }
    return true;
}
/* Which edge of polygon p has this midpoint; SN_VERTICES_MAX if none. */
static uint32_t portal_edge(const struct sn_polygon *p, struct sn_point portal)
{
    uint32_t e;
    for (e=0;e<p->vertex_count;e++) {
        struct sn_point u=p->vertices[e],v=p->vertices[(e+1)%p->vertex_count];
        float x=(u.x+v.x)*0.5f-portal.x,y=(u.y+v.y)*0.5f-portal.y,z=(u.z+v.z)*0.5f-portal.z;
        if (x*x+y*y+z*z<0.0001f) return e;
    }
    return SN_VERTICES_MAX;
}
/* Route search: the structure and scenery only, never units, vehicles or
   machines, which move. A route is a claim about the map; occupants are the
   follower's live check (navigation_world_occupant_clear), which waits and,
   if a bot stays stuck, blocks a polygon here. The canonical halves of an
   edge (center->portal, portal->center) are swept once per resource and the
   verdict cached; segments to or from a start or goal point are swept each time. */
bool navigation_world_search_clear(void *context, struct sn_point a, uint32_t ap,
    struct sn_point b, uint32_t bp)
{
    const struct sn_resource *r=navigation_world_resource();
    long const flags=_collision_test_for_bipeds_living_flags &
        ~(FLAG(_collision_test_objects_bipeds_bit)|FLAG(_collision_test_objects_vehicles_bit)|
        FLAG(_collision_test_objects_machines_bit));
    const struct sn_polygon *p;
    uint32_t polygon=SN_NONE,edge=SN_VERTICES_MAX,shift=0;
    bool clear;
    if (!context || !r || ap>=r->polygon_count || bp>=r->polygon_count) return false;
    p=&r->polygons[ap];
    /* A block on the polygon the bot stands in must not keep it from leaving:
       the segment from the start point to one of its own portals ignores it. */
    if (!(ap==bp && !(a.x==p->center.x && a.y==p->center.y && a.z==p->center.z) &&
        portal_edge(p,b)<SN_VERTICES_MAX && portal_edge(p,a)==SN_VERTICES_MAX) &&
        blocked_pair(ap,bp)) return false;
    if (ap==bp) {
        if (a.x==p->center.x && a.y==p->center.y && a.z==p->center.z) {
            polygon=ap; edge=portal_edge(p,b); shift=0;
        } else if (b.x==p->center.x && b.y==p->center.y && b.z==p->center.z) {
            polygon=ap; edge=portal_edge(p,a); shift=2;
        }
    }
    if (edge<SN_VERTICES_MAX) {
        uint8_t state=(uint8_t)((world.edge_cache[polygon][edge]>>shift)&3);
        if (state) { world.cache_hits++; return state==1; }
    }
    world.sweeps++;
    clear=sweep_clear(r,flags,*(long *)context,a,ap,b,bp);
    if (edge<SN_VERTICES_MAX)
        world.edge_cache[polygon][edge]|=(uint8_t)((clear ? 1 : 2)<<shift);
    return clear;
}

#endif /* HALO_FEATURE_BOTS */
