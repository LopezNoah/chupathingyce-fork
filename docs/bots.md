# Solo custom-game bots: playable milestone

> **Feature flag:** bots are compiled in only by `python3 configure.py --bots`,
> and are off by default. Without the flag, the sources under
> `source/features/bots/` are excluded: no bot can join, and the `bots.*`,
> `debug.nav_*`, `debug.bot_sandbox`, `debug.bot_navigation_only` and
> `debug.map_anchors_dump` settings are not offered. Existing
> `config.toml` entries are kept and logged as unknown. The startup log reports
> `features: ... bots on|off`. The reusable `source/engine_ai/` library and its
> standalone tests are unaffected.

## Supported scope

Up to 31 computer-controlled players in **FFA Slayer or Team Slayer**, hosted
by yourself on one machine. Bots are disabled by default (`bots.count = 0`).
They are real Halo player datums with normal spawning, scoring, weapons, damage
and respawning; their inputs enter the same player-action pipeline as a human.
Campaign AI is unchanged.

Standard two-flag CTF is opt-in (`bots.ctf = true`, `HALO_BOT_CTF=true`), with
an explicitly experimental lobby entry. Hidden Battle Creek and Blood Gulch
matches have verified both teams' captures and friendly touch-returns. Bots take
and return flags, deliver while carrying, and guard or search the public enemy
stand to recover a carried friendly flag. They never read hidden carrier
positions or award their own scores. Other maps retain navigation limitations;
see [CTF validation and scope](bot-objective-modes.md). King, Oddball, Assault,
single-flag and manual-reset CTF remain unsupported.

Only a local game or network host with no remote distributed machines may run
bots. Bots leave when that gate stops permitting them, including postgame,
unsupported modes or a remote machine joining. They are **not replicated bots
for LAN or online matches**. Games bots have participated in are excluded from
game-list/Delta Stats reporting. This is not a substitute for disabling public
hosting/listing when running a private test.

A utility selector scores VEHICLE, RETREAT, SCAVENGE, FIGHT and lane PATROL from
the bot's confidence, target and opportunities. It uses hysteresis to avoid
thrashing and decides every eight ticks, staggered across bots. The combat and
movement outputs are independent: seeing a distant opponent makes the bot aim,
choose whether to pursue or retreat, and close distance; it does not fire past
its skill's fire range or stand still to fire. Only Spartan difficulty currently
shares enemy callouts: the host-local blackboard keeps the last observed
position, and the contact's freshness, distance and bot confidence determine
whether to investigate it. This is not live tracking or hidden information;
callouts are logged, not spoken or shown as HUD text.

Aim is deliberately imperfect and difficulty-scaled. Error grows with range and
while moving, then settles as the bot tracks; it never falls to zero. Recruits
turn slowly, turn worse while moving and drift clumsily instead of stopping to
shoot. Higher skills strafe more capably; Spartans sometimes crouch-strafe.
Sight range is separate from fire range, so a target can attract pursuit without
turning into perfect long-range fire. BSP navigation builds walkable surfaces
and uses collision-checked routes for lane patrol, scavenging and distant
pursuit. The spawn graph remains a fallback if BSP navigation is unavailable
for ten seconds. Three map-relative lanes, paired squads and near/far patrol
phases keep teammates spread while allowing them to rally on a strong contact.

Weapon input supports automatic fire, slower semi-auto taps, charging/releasing
against visible shielded targets at medium range, reloads, switching away from
empty ammunition weapons, and close-range melee. Charging releases on lost
sight or changed targets. Bots do not deliberately target teammates, and in
team games they hold fire while a teammate is in the firing lane: the line
along the facing actually submitted (after turn speed and aim error), out to
the skill's fire range, against each teammate's collision pill. The lane
widens with range by the weapon's own shot cone, read from its trigger tag
(the error that sustained fire ramps up to, plus pellet spread; at least 2
degrees, at most half a radian, and invalid tag values ignored). Teammates
riding in the bot's own vehicle do not block it (projectiles ignore their
owner's vehicle). A charging or charged trigger is held rather than let go
into a teammate, and a swap to the other weapon drops the charge unfired. A
grenade is not thrown when the bot is within 6 world units of where it is
aimed or of the target; when a teammate is, or will be within the next two
seconds at its present velocity (its vehicle's, riding); when a teammate is in
the throwing lane; or when a wall, rock or vehicle stands within 6 units in
front of the bot. A refused throw is retried after a second. This is
conservative geometry, not projectile flight: ricochets, blast tags, homing
and a teammate's turns are not predicted, a turret's lag behind its gunner's
facing is not modelled, and custom weapons whose charge discharges while held
are outside it. Driving is not covered: a bot driving a Warthog can still run
over a teammate.

Bots scan up to 256 nearby weapon, equipment and vehicle objects once per
second. Weapons and powerups must be within the bot's sight/field of view (or
arm's reach) and pass a bounded line-of-sight budget; a last-seen item location
is remembered for up to 30 seconds, not live-tracked. They seek usable weapons
for empty slots or clear upgrades, and prefer overshield, camouflage, speed and
vision; health is useful when hurt. One host-local claim per team and item
prevents teammates from choosing the same pickup; opposing teams may pursue it
independently. Claims never reserve the item from a human, who can still race a
bot. A bot abandons an item it cannot route
to for 15 seconds before reconsidering it. A nearby upgrade or powerup can take
priority during combat; distant weapon scavenging does not. Utility is a coarse
damage/rate estimate, not a complete range-aware loadout strategy.

If a target is visibly observed more than six metres from its last observed
position within two game ticks, the bot logs a teleport observation and uses
that ordinary new sighting for chase/retreat utility. It never reads an invisible
target's live destination. The AI does not yet plan a route through teleporter
pairs.

Bots seek empty seats in parked ground vehicles and fixed turrets, never evict
occupants, and prefer a gunner seat when a friendly driver is present. They can
drive supported ground vehicles, man turrets, and use an available passenger
seat. When a driver has no contact to pursue and an extra seat is open, it steers
toward a nearby teammate and stops so the teammate can board normally. There is
no supported horn input, so the invitation is behavioral/logged rather than an
audible signal. Aircraft are excluded; there is no vehicle navmesh, advanced
driving, obstacle avoidance or crew role assignment.

No objective play or cross-machine replication is claimed. Objective variants
are explicitly gated out, so callout-vs-objective ambition scoring is future
work; in the supported Slayer modes, callouts compete with lane patrol and
combat. The map-wide navigation, vehicle and combat policies still need broader
testing. Normal spawn selection remains Halo's own; teammate-proximate spawning
is not forced.

## Bot Match lobby

Build with `python3 configure.py --bots` (keep the default game browser enabled).
Open **Multiplayer → Bot Match** in either menu style. With
`display.menus = "pc"`, this is a native XML screen generated by
`tools/port_settings.py`: keyboard, controller and mouse use ordinary widgets.
With `display.menus = "xbox"`, the code-rendered setup uses Online Games'
blue overlay style, map artwork and a match/team preview. Both frontends share
`source/features/bots/bot_lobby.c`'s bounded settings and private launch path.
The XML entry and screen are omitted from builds without bots or the browser.
Objective modes remain gated out; [the source audit and implementation gates](bot-objective-modes.md)
explain what CTF, King and Oddball still require.

Choose one of the thirteen Xbox multiplayer maps, FFA or Team Slayer,
friendly/enemy bot counts (31 total at most), Recruit/Marine/ODST/Spartan,
and map-default or custom starting weapons. Custom loadouts apply to the
human and bots through the ordinary game variant options. In FFA the friendly
count is folded into the opponent count. In Team Slayer, friendly bots join
your player's team and enemies join the other; automatic team balancing is
disabled for this match so it does not undo the selected counts.

Keyboard arrow keys or the controller's d-pad select rows and change values;
held directions repeat every 250 ms, as in the other menus. WASD is not the
recommended menu control. Selector lists wrap left/right (Recruit, Marine,
ODST, Spartan); counts stop at their capacity limit. Inactive settings are
greyed out and have no arrows: friendlies in FFA and weapon choices with the
map-default loadout. Enter/A changes a value or activates START BOT MATCH.
Controller Start launches directly; Escape/B returns without applying anything.
Mouse/touch clicks on labels select only, and the switch's left/right halves
change the value. The footer also has clickable Select, Change and Start/Back.
At least one bot is required, and missing/incompatible maps are refused before
launch. The match is local, with one human player and no network server or
remote joins. Lobby choices override the next map's bot settings only; no
`config.toml` or debug-test settings are changed. Ordinary configured bots
retain their balanced-team policy. Auto-fill, objectives, online bots and
live training controls are not offered by this lobby.

Validation: `ninja macos` and `python3 tools/test_bot_lobby.py` (production
control, arrow indicators, post-admission team assignment and launch functions
under ASan/UBSan, including the real main-loop map-change flag). Start must
queue the shell's map transition: `main_defer_map_map_change()` cancels it and
previously left the player watching ui.map's ring with no menu.

The asset-backed macOS check drives real keyboard arrows through the Xbox
menus in a hidden window, then verifies that Battle Creek loads with one
friendly and two enemy ODST bots. It uses isolated saves/data and keeps each
run's logs in a fresh artifact directory:

```sh
python3 tools/check_bot_lobby_live.py --maps /path/to/Xbox/maps
```

Local player admission normally alternates teams; the bot lobby restores its
selected teams after admission and before spawning. The live check validates
this as well as the launch. Manual visual review of the overlay remains useful;
the game's own frame dumps precede overlay presentation. Android could not be
built on this host: the configured build has no Android target.

## Configuration and visible private demo

For the repository executable, a runnable script, all test flags and terminal
checks, see [Terminal bot tests](bot-testing.md) and `tools/run_bot_match.sh`.
For a 4v4 Rat Race setup (one host plus seven bots), run
`BOT_PRESET=4v4 sh tools/run_bot_match.sh`; it is an offline local-host fixture.

In `config.toml`:

```toml
[bots]
count = 3
skill = "marine"
```

Skills: `recruit`, `marine`, `odst`, `spartan`. Environment overrides:
`HALO_BOTS=3`, `HALO_BOT_SKILL=spartan`. Set `debug.bot_decisions = true`
(or `HALO_BOT_DECISIONS=1`) to log utility scores and behavior changes.

For the native macOS executable, run from a directory with the proper data/map
roots (prefer isolated save/data roots for testing):

```sh
env HALO_NET_ONLINE=false HALO_NET_PUBLIC_LOBBY=false \
  HALO_NET_HOST_PUBLIC=false HALO_NET_LIST_GAMES=false \
  HALO_NET_REPORT_GAMES=false HALO_NET_REPORT_EVENTS=false \
  HALO_NET_ALLOW_UPNP=false HALO_SOLO_GAME=1 \
  HALO_NETWORK_TEST=host:bloodgulch:team_slayer HALO_NETWORK_TEST_START=5 \
  HALO_NETWORK_TEST_LOADOUT=rifle_pistol \
  HALO_BOTS=3 HALO_BOT_SKILL=spartan \
  /path/to/ChupathingyCE.app/Contents/MacOS/halo
```

Use `host:bloodgulch:slayer` for FFA. Leave `HALO_HIDDEN_WINDOW` and
`HALO_NULL_RENDERER` unset for visible play. `HALO_EXIT_AFTER=95` optionally adds
a timed exit; its clock starts when the window opens, not when gameplay begins.

### Optional controlled gunner test (not normal spawning)

Add `HALO_BOT_SANDBOX=1` to the Team Slayer command with three bots. Once per
map, this **debug-only fixture** puts the host in a parked Warthog's driver seat
and positions teammate Bot 2 near the gunner entrance. Bot 2 must still approach,
select the gunner seat and enter through normal interaction. Keep the Warthog
stopped while it boards. The teleport is intentionally visible and is not a
same-side/team spawn preference. Leave the variable unset for normal games;
`debug.bot_sandbox` defaults to false.

`HALO_BOT_NAVIGATION_ONLY=1` (`debug.bot_navigation_only`, default false) is a
navigation fixture: bots only patrol their team lanes and hold fire, so every
route can be followed to arrival (`docs/bot-testing.md`).
`HALO_MAP_ANCHORS_DUMP=map_anchors.json` (`debug.map_anchors_dump`, default
empty; data-root-relative, must end in `.json`) writes the map's anchors in
world coordinates once the walkable polygons are built (`docs/navigation.md`).

For a controlled weapon pickup test, instead add
`HALO_NETWORK_TEST_PICKUP=5 HALO_NETWORK_TEST_PICKUP_WEAPON=sniper` without the
sandbox variable. This existing network-test fixture positions the last player
on a ground sniper rifle; the normal bot/pickup pipeline must acquire it.

`tools/system_link_bots.py` is a different protocol/load-test utility: it joins
remote stand-in machines that mostly stand still. It is not this gameplay AI.

## Verification for this milestone

- Native `ninja macos` build passes, including the bot and reusable AI objects.
- Utility, aim/claims, navigation, behavior-definition, traversal and
  fire-control standalone runners pass with sanitizers.
- Visible Blood Gulch FFA testing demonstrated movement, combat, kills, deaths
  and respawning. The latest FFA sample ran approximately 46 game seconds.
- Visible Blood Gulch Team Slayer sample ran 1,350 ticks (45 game seconds):
  human and Bot 2 on team 0, Bots 1 and 3 on team 1; nine logged target samples
  were opponent-only. Bot 1 scored a kill and Bot 2 died. No friendly-fire kills
  or crash/assertion were recorded. This is a smoke test, not full-team gameplay
  coverage; Team Slayer respawning and team score/endgame need longer testing.

To validate the sampled Team Slayer evidence from a network-test log:

```sh
python3 tools/check_bot_team_slayer_log.py /path/to/match.log
```

The checker requires this one-human/three-bot fixture, balanced teams, at least
30 game seconds, enemy-only target samples and a bot kill without friendly-fire
kills. It cannot verify every tick, charged projectiles or correct team totals.

For 24-bot lane/patrol coverage:

```sh
python3 tools/check_bot_navigation_log.py /path/to/match.log
```

It requires actual patrol goals and completed patrol routes in all three lanes
for both teams, with goals at least 10 metres apart; tactical routes alone do
not count.

Additional visible smoke tests for weapon/vehicle support:

- Controlled gunner fixture: Bot 2 sought the host's Warthog and entered seat 2
  through the normal interaction path; later status showed vehicle behavior.
- Pickup fixture: Bot 3 acquired the ground sniper rifle into inventory slot 1;
  Bot 1 also gained a second weapon. Bots 1 and 2 independently entered ground
  vehicles as drivers. This proves acquisition/entry, not good driving or turret
  accuracy. The initial sandbox orientation assertion was fixed by preserving
  biped orientation during position-only test teleports.

Validate the corresponding logs with:

```sh
python3 tools/check_bot_opportunity_log.py gunner /path/to/gunner.log
python3 tools/check_bot_opportunity_log.py weapon /path/to/pickup.log
```

Known follow-ups:

- BSP `bad-start`, `bad-goal` and `no-path` searches still prevent full lane
  coverage; movement recovery is not a route fix.
- The previous Rat Race smoke log predates claim-release logging, so live claim
  handoffs after abandonment, death and bot exit remain unverified. Test pickup
  races across maps and with a human collecting the same item.
- Aim helper tests pass, but live accuracy and difficulty differences need
  tuning. Validate visible teleport reacquisition on a teleporter map;
  teleporter route planning is not implemented.
- Instrument actual charge/release/projectile events; helper tests do not prove
  that every weapon's runtime charging behavior is correct.
- Investigate a reported brief movement/jump interruption under two-bot fire.
  One FFA log sample shows Halo's stunned-movement state while the human was
  badly hurt, but that is not a confirmed reproduction or a fix.
- Re-run the 24-bot lane checker after the current patrol/callout changes;
  validate turret combat and the stop-near-teammate boarding behavior; there is
  no horn control in the current player-input API.
- Longer Team Slayer lifecycle/endgame tests, other maps, live friendly-fire
  behavior (how often bots hold fire or refuse throws in team fights),
  vehicle obstacle avoidance, and supported non-macOS builds.

See [reusable primitive documentation](../source/engine_ai/README.md) for test
commands and ownership/bounds contracts.
