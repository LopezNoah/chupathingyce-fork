# Bot objective modes: source audit and implementation gates

Status: design, not implemented. The PC/XML and Xbox Bot Match menus still
advertise only FFA Slayer and Team Slayer. This audit does not relax
`bots_host_may_have_bots()` or change network compatibility.

## Why selecting another variant is not enough

- `source/features/bots/bot_manager.c:bot_manager_publish_ambitions()` clears
  the list every tick and publishes nothing.
- `bot_manager.h` defines take, deliver, guard, hold, hunt and reach ambitions,
  but `bots.c` does not read the ambition getters or score those behaviours.
  `bot_score_behaviors`, `bot_leaf_viable` and the behaviour/intent dispatch
  need actual objective execution, not just new goals on the blackboard.
- `bots_host_may_have_bots()` rejects every non-Slayer engine. Removing that
  check would give objective matches Slayer roaming, not objective bots.
- Pickup claims and lane squads exist, but they do not assign objective roles.
- Navigation is continuous walking plus local ascent attempts, not general
  floor-transition or teleporter routing. An objective being published or a
  route being constructed is not proof a bot can reach or score it.

The game engines must remain the authority. Bots may submit ordinary movement,
pickup, melee and weapon actions; never write flag ownership, scores, hill state
or round state themselves. Keep bots local and remove them when a remote
machine joins, as today. Online bot replication is a separate project.

## Read-only game-state boundary

Add small host-only, caller-owned snapshots next to each engine's private state.
Declare them in a port objective header, with `port:` comments on game-source
changes. Do not export engine globals, retain scenario pointers in bots, or
reuse the serialized network-state buffers as a bot API. The existing network
writers do not describe all objective rules or scoring regions.

Rebuild at most `BOT_AMBITIONS_MAXIMUM` (16) goals before bots think. Each goal
needs a stable identity including the map/round epoch, not an array offset that
silently changes meaning after pickup, reset or a moving hill. Revalidate salted
object/player datums with `*_try_and_get` before every use. Missing objects,
invalid teams, non-finite positions, overflow or unavailable goals fail closed.
A bot retains an identity and last observed position, not pointers into a
previous tick's snapshot. Bound any carrier search by the player capacity.

Public objective locations may be shared. An enemy carrier's live private
position must not bypass perception: hunt from a visible or remembered contact,
or from information the actual mode explicitly reveals to players. Distinguish
rules/ownership knowledge from permission to aim at an invisible player.

## Capture the Flag: first complete objective increment

Relevant production functions in `source/game/game_engine_ctf.c`:

- `ctf_globals` owns two stand pointers and two flag weapon datums.
- `ctf_engine_allow_pick_up()` refuses a team's untouched home flag.
- `ctf_weapon_pickup()` returns one's own dropped flag through the engine and
  returns FALSE: **a return is successful without acquiring the weapon**.
  With `flag_must_reset`, the same interaction does not return it.
- `ctf_engine_player_update()` scores a carried enemy flag within 1.0 m of the
  player's stand and, when required, only while that player's flag is home.
- `ctf_find_flag_carrier`, `ctf_player_drop_flag`, `ctf_reset_flag` and
  `ctf_engine_weapon_update` own carrier, drop and reset transitions.

Initial scope: ordinary two-team, two-flag CTF only. Explicitly refuse Assault,
single-flag and manual-reset variants until separately tested; do not infer
support from the engine index alone.

Snapshot: for each of two teams, copy stand position, valid flag datum,
home/dropped/carried state, carrier identity and the relevant rules. Publish
bounded take/return/guard/deliver candidates. Return needs an explicit policy
(or a return goal kind); enemy pickup and friendly touch-return cannot share a
success condition that requires inventory ownership. Delivery applies only to
the particular carrier and uses the engine's actual stand tolerance.

Execution and selection:

1. Pick up an available enemy flag using the ordinary interaction path.
2. After acquisition, cancel weapon/vehicle scavenging and route to the own
   stand. Validate flag possession every tick, including death and forced drop.
3. If the own flag must be home and is away, wait in a valid reachable area or
   help return it according to a tested policy; never award a capture manually.
4. Return a reachable friendly dropped flag when the rules permit it; finish
   when the engine reports home/reset, not when a weapon appears in inventory.
5. Assign a bounded guard slot and attacker/return slots per team. Claims
   expire and release on death, departure, reset, unreachable routes or changed
   ownership. Humans are not blocked by those assignments.
6. Carriers keep delivering despite low-value shared contacts. Independent
   shooting must obey the flag's actual weapon restrictions; ABS must not swap
   away or drop the carried flag to shoot a normal weapon.

Gate before advertising CTF: both teams can acquire and capture, friendly
return succeeds, death/drop/reset invalidates goals, own-flag-away blocks
scoring as configured, stale datums never crash, unreachable objectives do not
cause tick-by-tick replanning, and guard/carrier assignments survive distracting
callouts. Run ASan/UBSan helper/lifecycle tests and hidden live Blood Gulch rounds
with logged **actual flag returns and captures**, not movement alone.

## King of the Hill: the smallest movement-only slice

`source/game/game_engine_king.c:player_inside_hill()` uses the unit's bounding
sphere centre, the hill's vertical bounds, and its convex hull (at most 12
points). An arbitrary circular ambition radius does **not** reproduce that
scoring test. `king_engine_player_update()` awards time via this predicate.

Expose a copied hull/height snapshot, hill identity and a read-only membership
query. Find a reachable interior goal on the correct floor, walk there, and
hold position while independently fighting. Finish/replan on hill change, not
just proximity to the geometric centre. Do not assume Infinite's contested-zone
rules: CE's actual engine determines scoring, including shared team score ticks.

Start with static normal King, FFA and team variants; moving hills need epoch
invalidation and a separate reachability test. Gate on actual `time_on_hill`
and score advancement, including enemies present, death/respawn, stacked floors,
hull boundaries and hill movement. This is useful as an early isolated movement
fixture, but does not replace the CTF lifecycle work.

## Oddball: carrier survival, not flag delivery

`source/game/game_engine_oddball.c:oddball_engine_player_update()` derives
ownership from the current rules, adjusts carrier speed, and awards time.
`oddball_ball_transfer_by_killing()` is true for special ball types: those are
not ordinary weapon pickups.

Start with a single normal ball. Copy ball/carrier identities and mode rules;
seek a loose ball, carry and survive in a reachable area, and pursue an opposing
carrier using permitted awareness. A carrier-specific hold/survive behaviour is
needed; the current area-hold and flag-delivery concepts are not enough. Test
weapons/melee/grenade restrictions and speed traits through the real player
path, without giving bots a second gun while carrying the ball.

Gate on actual time-with-ball and score, pickup/death/drop/respawn, FFA/team
ownership, teammates refusing a teammate's carried ball, and stale datums.
Multi-ball, reverse/tag/juggernaut/terminator rules remain refused until their
scoring and transfer semantics have dedicated tests.

## Delivery order and compatibility

1. Finish repeatable objective-location reachability fixtures on chosen maps.
2. Add read-only snapshots and stable goal identities with boundary tests.
3. Complete standard CTF selection/execution and lifecycle tests. A static King
   fixture can validate the simpler reach/hold path alongside this work.
4. Add normal King, then normal Oddball, each behind its own tested capability.
5. Only then extend the **shared** lobby mode table, launch validation and host
   admission gate together. Both menu frontends must reject unsupported variants.
6. Treat richer variants, Race, teleporter links, vehicles carrying objectives,
   and online replication as separate increments.

No wire change is needed for these host-local snapshots or this document.
Changes to replicated bots, engine rules or anything peers must agree on require
reading `port/linux/NETCODE.md`, raising `HALO_PORT_NETWORK_VERSION`, and explicit
cross-version rejection tests. No such change is made by the XML lobby work.
