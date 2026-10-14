# Controlled bot scenarios

`debug.bot_scenario` runs the bots through controlled, repeatable trials in
a purpose-built arena and records every tick of their decisions, so that one
behavior can be examined at a time, before and after a change, and at every
skill. You can watch it: a camera follows the bot under test, with its
behavior, utility scores, target knowledge and route drawn over the view.

It needs a bots build (`configure.py --bots`) on a 64-bit desktop target
(macOS, 64-bit Linux): the arena is host-local world geometry, which other
builds do not have. Blood Gulch is the default map (legal Xbox maps, as for
every bot run).

```sh
python3 configure.py --bots            # with your other options
ninja macos
python3 tools/run_bot_scenarios.py                                # every scenario, Spartan, watched
python3 tools/run_bot_scenarios.py --scenarios S02,S09 --skills all
python3 tools/run_bot_scenarios.py --skills all --trials 20 --hidden   # unattended
python3 tools/check_bot_scenarios.py build/macos/botrun/scenarios/<run>/data/telemetry.jsonl
```

The runner makes a fresh run directory under `build/macos/botrun/scenarios/`
(maps symlinked from `build/macos/botrun/data/maps`, fresh saves, every
online and reporting setting off), starts the game, waits for it to quit
when the last trial ends (or stops it with its watchdog), and prints the
report. `--camera overview|first_person`, `--screenshots N` (a screenshot
every N frames, in the run directory), `--seed`, `--crowd` (S10's bots) and
`--hidden` (a hidden window, still rendered) are its other options.

## The arena

The layout is `halo_bot_arena_prototype/geometry_manifest.json`'s at half
scale: its 64 x 48 units are 32 x 24 world units here (about 98 by 73
metres; a Spartan is 0.7 units tall), x east and y north. A 2-unit perimeter
wall, a 1.5-unit central wall with returns at either end (an S that blocks
the middle), two 1-unit pillars and a route block in the east, low cover
(0.65 to 0.75 units: it hides a body, not a standing head) in the west and
south, and a 1.6-unit pillar in the north-west.

It is built above Blood Gulch, midway between the bases (or at
`debug.bot_scenario_origin` on another map): a floor 2 units above the
highest ground or scenery found beneath it, so that it is flat whatever the
terrain. The boxes are:

- solid to movement, sight and shots through the world-geometry hooks
  (`extension_api.h`), as the platform feature's board is: the engine's own
  collision tests see them, so bipeds stand on them, line-of-sight tests
  stop at them and bullets hit them;
- walkable for the bots' routes: the floor is 192 polygons (2-unit cells)
  appended after the BSP's surfaces (`navigation_world.c`), its own connected
  set, and every route over it is checked with the live biped capsule, which
  sees the walls. The arena's revision is an editor's world-edit revision,
  so the navigation resource is built again when the arena appears;
- drawn as flat-shaded debug geometry (two-tone tiles on the floor; walls
  grey-blue, cover orange, pillars teal).

The host's own player and any bot a trial does not use stay where they
spawned, far below; the subjects never see them (`bot_scenario_ignores_player`
in perception, attackers and confidence). Bodies and dropped weapons on the
arena are cleared before each trial, and the teams' shared sightings and
claims are forgotten.

## A trial

1. The trial's bots are alive (a dead one respawns where the map spawns it).
2. Each is placed at its scenario's position and facing, with its shields,
   ammunition and the trial's skill; its decisions, target, route and memory
   start afresh, alert at once (no spawn orientation), its random numbers
   from the run's seed and the trial number.
3. Half a second held still, then the trial runs: subjects decide for
   themselves (`_bot_scenario_control_ai`), or walk their own route to a
   goal (`_bot_scenario_control_goal`, S03); scripted targets walk straight
   between their steps, always face the nearest subject, and shoot at it
   when a step says so and they have a clear line (1.5 degrees of error).
4. It ends at its time limit, when a subject or every target dies, or at the
   scenario's own end (arrival, a pickup, reacquiring a target). The result
   is written, shown for two seconds, and the next trial is placed.

Order: each scenario at each skill, `debug.bot_scenario_trials` trials; a
scenario's variants take turns from trial to trial.

## The scenarios

| Id | Setup | Checks (PASS/FAIL where the design says what should happen) |
| --- | --- | --- |
| S01 | Bot in the open; a still, unarmed target 5, 10 or 15 units ahead | Sees, fires, hits; reaction, aim error, time to kill |
| S02 | Target shows itself until seen for 1.5 s, walks round the north return behind the central wall, waits 3 s, moves along behind it (kept alive) | Loses sight, pursues (units walked after), reacquires; time unseen |
| S03 | No enemy; walk from one side of the central S to the other (both ways) | Arrives; route found, path efficiency, stalls |
| S04 | Target stands and shoots; the bot starts with 10%, 40% or full shields | ODST and Spartan retreat from 10%; retreat score, survival |
| S05 | A loose weapon from the map placed beside a bot with 4 rounds and none spare (or full); unarmed target in sight (kept alive) | Picks it up when low; scavenge score |
| S06 | Bot A sees a target; teammate B faces away; a second target behind a pillar | Spartans' B learns of it by a shared sighting |
| S07 | 15 s against a shooting target, still or stepping side to side (both kept alive) | Strafing skills strafe, Spartans crouch; hits per firing second |
| S08 | CTF priority | Not run: needs a CTF variant and the map's flags |
| S09 | Bot keeps watch westward until shot in the back from beside the central wall's north end; the target fires for a second, then walks east behind the wall | No exact knowledge of the unseen attacker's place; whether its idea of it moves unseen |
| S10 | 8 bots (`debug.bot_scenario_crowd`, up to 24), two teams, 30 s | AI time per tick under 33 ms; kills, deaths, stalls |

The checks are in `tools/check_bot_scenarios.py`; most measurements are
reported as INFO, for comparison between runs and skills rather than a
verdict.

### Findings from the first runs (2026-10-14)

Single trials on Blood Gulch, seed 1, base commit `03b6bca6` plus this
change, macOS hidden-window runs (still rendered). Run directories are under
`build/macos/botrun/scenarios/` (local evidence, not committed):

| Run | Directory | Trials |
| --- | --- | --- |
| A: every scenario, Spartan | `scenarios-20261014T182926-056444Z` | 9 |
| B: every scenario, all skills (stopped by hand after trial 19: S01-S05 only) | `scenarios-20261014T183715-018498Z` | 19 |
| C: S01, S02, S04, Spartan, 2 trials each (variants) | `scenarios-20261014T183359-018884Z` | 6 |
| D: every scenario, Spartan, visible window | `scenarios-20261014T184519-250897Z` | 9 |

These are single samples, not distributions; the order of magnitude and the
qualitative behavior are what they show. Run `--skills all --trials 20`
before tuning on any of them.

**1. A hurt bot knows exactly where its unseen attacker is (S09: FAIL in
every run).** `bot_perceive`'s attacker branch copies `bot_aim_point` of the
attacker's *current* unit into `target_last_position` on every tick within
`BOT_ATTACKER_AWARENESS_TICKS` (15 ticks, 0.5 s) of a hit, whether or not
the bot can see it. Run B's Recruit trial shows it tick by tick: from the
first hit (t = 12) to t = 37 the source is `attacker`, `visible` is 0, and
`belief` equals `truth` to the millimetre each tick. A Recruit (200 deg/s
turn) had that exact knowledge for 26 ticks (0.87 s); a Spartan (720 deg/s)
for 3 ticks (0.1 s) before it turned and saw the attacker for itself. While
the attacker keeps hitting, the window keeps renewing, so a bot being shot
from cover tracks a moving shooter. A player knows the direction the damage
came from, not the shooter's place: the fix belongs in `bot_perceive` (a
point along the damage direction, or the place the shot came from at the
moment of the hit, never refreshed from the live unit). The
`unseen_exact_ticks` metric and S09's check measure it. `hidden_updates`
(the idea moving while unseen) stayed 0 in these runs only because the
target stood still while firing; it walks away only after its last shot,
by which time the bot had turned and seen it.

**2. Low-shield retreat rarely wins (S04).** At 10% shields:

| Skill | Max retreat score | Retreated | Outcome |
| --- | ---: | --- | --- |
| Recruit, Marine | 0.00 | no (by design: `retreats` is FALSE) | died in 2.8 s, 1.6 s |
| ODST | 0.64 | no | killed the target in 3.0 s |
| Spartan (run A) | 0.59 | no | died in 3.5 s |
| Spartan (run B) | 0.60 | no | died in 2.6 s |
| Spartan (run C) | 0.70 | 0.83 s, from 1.6 s | died in 2.4 s |
| Spartan (run D) | 0.70 | 0.67 s, from 4.0 s | died in 4.7 s |

Fight scored about 0.65-0.70 against a visible target at 5 units with the
bot's confidence near 0.5, so retreat only overtakes it once confidence
falls further, by which time the bot is nearly dead: it retreated in 2 of
5 Spartan trials, late and briefly, and never survived. At 40% shields the
retreat score peaked at 0.03 (the curve is zero above half shields and
quadratic below), and the Spartan killed the target in 6.2 s. Whether bots
should break off earlier is a tuning decision (`bot_slayer_tuning`'s
`retreat_shields`, `retreat_cap` and the confidence weights); the scenario
now measures it. Note the scripted target's aim is near perfect (1.5 deg
error), harsher than a typical player.

**3. Pickups under fire are unreliable (S05, low ammunition).** Recruit
(1.7 s), ODST (0.7 s) and Spartan in runs A (1.8 s) picked up the weapon
beside them. Marine (run B) never scored it (scavenge 0.00 for 15 s: it
apparently never registered the weapon as known or wanted) and fought with 4
rounds instead. Spartan in run D chose scavenge (0.95, over fight 0.68),
routed to it, then logged `gave up weapon ... (taken or gone)` 25 ticks
later, dropped to fight and never returned to it. The weapon is a loose
map weapon moved by the fixture; whether it was still there (netgame item
respawn, physics) or the bot's check misjudged it is not yet known: log the
pickup weapon's position per tick before concluding.

**4. Aim and reaction scale with skill as designed (S01, 5 units, a still
target).**

| Skill | Sight to first shot | Mean aim error in sight | Damage in 12 s (2.0 kills) | Time to kill |
| --- | ---: | ---: | ---: | ---: |
| Recruit | 0.90 s | 11.9 deg | 0.53 | - |
| Marine | 0.50 s | 4.6 deg | 1.20 | - |
| ODST | 0.30 s | 2.3 deg | 1.97 | 9.8 s |
| Spartan | 0.17 s | 1.0 deg | 1.84-2.00 | 3.5 s (A), 9.0 s (C), timeout (B) |

Reactions equal each skill's `reaction_seconds` exactly (the bot is alert
from the start). Spartan time-to-kill at 5 units ranged 3.5 s to over 12 s
against an unarmed, still target with near 1-degree aim: damage is landed
in bursts with long gaps (15 damage ticks in 3.5 s, or 14 in 12 s). The gaps
(burst pauses, reloads with 60-round magazines, strafing with a moving-aim
penalty) are worth a look: a 1-degree aim on a still target at 5 units
should kill in about 2-3 s with an assault rifle. At 10 units the Spartan
dealt 1.71 of 2.0 in 12 s and did not kill.

**5. Line-of-sight memory and pursuit work (S02, all skills).** Every
skill lost the target behind the wall, walked to its last known place (24-26
units walked by Marine to Spartan; 8.5 by the Recruit, which first saw it
only at 2.6 s) and reacquired it after 3.7-10.4 s unseen. Its idea of the
target never moved while it was unseen (`hidden_updates` 0), as it should.

**6. Routing round the wall is solid but not the shortest way (S03).**
Every skill arrived in 10.4 s, route found within 0.17 s, 0.07 s stalled; it
walked 22.0 units for an 18-unit straight line (efficiency 0.82), the same
path every time: the routes are deterministic.

**7. Team sightings (S06, Spartan): instant.** Bot A's callout reached bot
B on the first tick (`shared` at 0.00 s); B turned and saw the second target
for itself a tick later. Shared sightings act at once, with no delay a
spoken callout would have. Lower skills were not run (run B stopped).

**8. Strafing (S07, Spartan, still target).** 11 direction changes in 15 s
(44 a minute of fighting; run D: 7, 28 a minute), 2.1-2.8 s crouched, 6-10
jumps, 3.8-4.7 hits a firing second. Lower skills were not run.

**9. Crowd cost (S10, 8 Spartans, 30 s).** AI time 0.33-0.35 ms a tick on
average, worst 2.4-2.7 ms (Apple M2 Pro class machine); 4-5 kills, 3 s of
stalls among eight bots.

**Fixture issues found and fixed while building these runs** (relevant when
reading the earliest run directories): a team's shared sighting from the
previous trial reached the next trial's bot (now `bots_scenario_forget` at
each start); the step flags were tested as bit numbers, so targets never
fired; S09's bot turned while patrolling and saw its attacker before being
shot (it now keeps watch until hurt); "reacquired" counted wall-edge
flicker (sight is now lost only after a second unseen); bodies and weapons
from earlier trials littered the arena (now cleared). The rasterizer's
immediate debug triangles draw nothing at the world-geometry point of the
frame on the GL renderer (`platform.c`'s fallback board has the same
problem); the arena is drawn as buffered debug geometry, flushed before the
overlay text.

## Telemetry

`debug.bot_scenario_log` (the runner: `data/telemetry.jsonl`) is JSON lines,
one object each, in the data root (a `.jsonl` name is required):

| `type` | When | Fields |
| --- | --- | --- |
| `run` | first | `map`, `center` (the floor's center), `seed`, `trials`, `skills`, `scenarios`, `boxes` (`name`, `min`, `max` in world units) |
| `trial` | each start | `trial`, `scenario`, `name`, `skill`, `variant`, `variant_name`, `seed`, `tick`, `duration_ticks`, `roles` (`slot`, `kind` subject/target, `team`, arena `x`/`y`, `yaw`, `shield`, `sustain`, `ammunition`), `goal`, `pickup_weapon` |
| `tick` | each running tick, each subject | `t` (ticks since the start), `slot`, `pos`, `yaw`/`pitch` (degrees), `leaf`, `scores` (retreat, fight, roam, scavenge, vehicle, objective), `confidence`, `target` (slot, -1 none), `source` (0 none, 1 seen, 2 attacker, 3 remembered, 4 shared), `visible`, `aware`, `belief` (where it thinks the target is), `truth` (where the target's chest is), `aim_error` (degrees, while seen), `look`, `move` ([yaw, speed] after steering), `fire`, `crouch`, `jump`, `strafe`, `route` ([searching, following, waypoint, points]), `nav` (goal walks: the navigation result), `shield`, `body` |
| `target` | each running tick, each scripted target | `slot`, `chest`, `step`, `fire` |
| `hit`, `death` | damage or a death (attributed to the unit's last attacker) | `t`, `attacker` and `victim` slots, `amount` (shields + body, 0 to 2) |
| `perf` | each tick of S10 | `ms`: the bots' CPU time that tick |
| `result` | each end | `reason`, `ticks`, `ai_ms_mean`, `ai_ms_max`, `subjects`: first sight, on target (within 5 degrees), first shot, first hit, shared sighting, retreat, sight lost (unseen for a second), reacquired, arrival, death, kill (ticks, null if never); counts of visible, firing, crouched, retreating, stalled and route ticks, hits, kills, jumps, strafe and behavior changes, `hidden_updates` and `unseen_exact_ticks`; damage dealt and taken, the highest retreat score, distance walked, mean aim error |

The same seed gives the bots the same random numbers; the physics, the
timing of route searches and the targets' aim error still vary a little, so
compare distributions, not single trials.

## Settings

| Setting | Default | Variable | Meaning |
| --- | --- | --- | --- |
| `debug.bot_scenario` | `""` | `HALO_BOT_SCENARIO` | `S01,S02` or `all`: the scenarios to run (empty: off) |
| `debug.bot_scenario_skills` | `"spartan"` | `HALO_BOT_SCENARIO_SKILLS` | `recruit,marine,odst,spartan` or `all` |
| `debug.bot_scenario_trials` | `1` | `HALO_BOT_SCENARIO_TRIALS` | Trials of each scenario at each skill (1-100) |
| `debug.bot_scenario_seed` | `1` | `HALO_BOT_SCENARIO_SEED` | The run's seed |
| `debug.bot_scenario_log` | `""` | `HALO_BOT_SCENARIO_LOG` | The telemetry, a `.jsonl` file in the data root |
| `debug.bot_scenario_camera` | `"follow"` | `HALO_BOT_SCENARIO_CAMERA` | `follow`, `overview`, `first_person` or `off` |
| `debug.bot_scenario_origin` | `""` | `HALO_BOT_SCENARIO_ORIGIN` | The arena's center `x,y` (and probe height `x,y,z`) on maps other than Blood Gulch |
| `debug.bot_scenario_crowd` | `8` | `HALO_BOT_SCENARIO_CROWD` | S10's bots (2-24) |
| `debug.bot_scenario_exit` | `false` | `HALO_BOT_SCENARIO_EXIT` | Quit once the last trial ends |

The scenario run decides the bot count (`bots.count` is not used) and plays
Team Slayer: subjects on even slots (team 0), targets on odd ones (team 1).
The runner sets the network test's score limit to 999 so that trial kills do
not end the game.

## Limits

- Not a compiled `.map`: the arena exists only on the host that builds it,
  only while the fixture runs, and only on 64-bit desktop builds.
- Scripted targets walk straight and do not path; their scripts' paths are
  clear of the walls by design. They are bots driven by the fixture, not
  people: their aim is near perfect and their movement simple.
- The floor's cells under the walls remain polygons; routes through them are
  refused by the capsule check, not by the graph.
- The camera follows the first subject; the overlay shows its numbers only.
- Single-trial verdicts are noisy. S08 (CTF) is not implemented.

Tests: `python3 -m pytest -q tools/test_bot_scenarios.py` (asset-free: the
report's checks on synthetic telemetry, truncated and incomplete runs, the
runner's list parsing and settings).
