/* Bots' config.toml settings: rows of port_config.c's config_settings
table, included there when the feature is built (tools/features.py,
HALO_FEATURE_SETTINGS_<n>). A setting may be in any section; the file is
written grouped by section. */

	/* computer-controlled players (source/features/bots/bots.c; configure.py --bots) */
	{ "bots.count", _config_integer, "0", "HALO_BOTS", _environment_value, _platform_all,
		"Computer-controlled players (0 to 31) in FFA or Team Slayer games you\n"
		"host by yourself: a custom game nobody else is in. Bots never join a game\n"
		"another machine is in. They play as players do, with the same weapons,\n"
		"health and rules; experimental." },
	{ "bots.ctf", _config_boolean, "false", "HALO_BOT_CTF", _environment_value, _platform_all,
		"Opt-in experimental standard two-flag CTF objectives and lobby mode.\n"
		"Pickup, return and scoring checked on Blood Gulch and Battle Creek; off by default.\n"
		"No Assault, single-flag or manual-reset variants (docs/bot-objective-modes.md)." },
	{ "bots.team_spawns", _config_boolean, "true", "HALO_BOT_TEAM_SPAWNS", _environment_value, _platform_all,
		"In a team game with bots, each team spawns in its own half of the map,\n"
		"nearer its own base. False: Halo's own spawns, where both teams can\n"
		"start in the same base." },
	{ "bots.skill", _config_string, "\"marine\"", "HALO_BOT_SKILL", _environment_value, _platform_all,
		"How well the bots play: \"recruit\", \"marine\", \"odst\" or \"spartan\".\n"
		"Skill changes their reactions, aim and sight, never their health or damage." },
	{ "bots.human_callouts", _config_boolean, "true", "HALO_BOT_HUMAN_CALLOUTS", _environment_value, _platform_all,
		"Share local human POV contacts with teammate bots after one second of\n"
		"continuous sight. Nearby patrol bots also turn toward incoming-fire cues;\n"
		"Team Slayer only, no speech or hidden enemy tracking (docs/bot-testing.md)." },
	{ "debug.bot_decisions", _config_boolean, "false", "HALO_BOT_DECISIONS", _environment_set_is_true, _platform_all,
		"Log each bot's change of behavior with every behavior's utility score\n"
		"and its confidence (docs/bot_decisions.md); off in normal play." },
	{ "debug.nav_probe", _config_boolean, "false", "HALO_NAV_PROBE", _environment_set_is_true, _platform_all,
		"Opt-in BSP navigation walking probe controlling Bot 1 in a solo Slayer game.\n"
		"No teleport or physics bypass; off in normal play." },
	{ "debug.nav_dump", _config_string, "\"\"", "HALO_NAV_DUMP", _environment_value, _platform_all,
		"Diagnostic walkable BSP polygon dump path (derived local data, not a map edit)." },
	{ "debug.nav_goal", _config_string, "\"\"", "HALO_NAV_GOAL", _environment_value, _platform_all,
		"Optional navigation probe goal as x,y,z in CE world units; empty picks a nearby region." },
	{ "debug.map_anchors_dump", _config_string, "\"\"", "HALO_MAP_ANCHORS_DUMP", _environment_value, _platform_all,
		"Diagnostic JSON of the map's anchors (starting locations, netgame flags,\n"
		"weapon and powerup spawns) in world coordinates, each placed on the walkable\n"
		"BSP polygons; the path must end in .json (docs/navigation.md)." },
	{ "debug.bot_navigation_only", _config_boolean, "false", "HALO_BOT_NAVIGATION_ONLY", _environment_set_is_true, _platform_all,
		"Navigation test fixture: bots only patrol their team lanes and hold fire\n"
		"(no fighting, retreating or pickups), so every route can be checked to\n"
		"arrival (docs/bot-testing.md); off in normal play." },
	{ "debug.bot_sandbox", _config_boolean, "false", "HALO_BOT_SANDBOX", _environment_set_is_true, _platform_all,
		"Solo Team Slayer bot test fixture (requires three bots): seat the local\n"
		"player in a parked Warthog and place teammate Bot 2 near the gunner entrance\n"
		"once per map. The bot must approach and enter normally; off in normal play." },
