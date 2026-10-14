"""Custom Edition campaign discovery shares the cache-header checks with the
existing multiplayer map-family list (port/linux/game/map_families.c)."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools import harness

ROOT = Path(__file__).resolve().parent.parent

STUB_CSERIES = r"""
#include <stddef.h>
#include <string.h>
#include <strings.h>
typedef unsigned char boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
"""

STUB_WINDOWS = r"""
#include <stdint.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
typedef void *HANDLE;
typedef struct { char cFileName[260]; } WIN32_FIND_DATAA;
#define GENERIC_READ 1
#define OPEN_EXISTING 1
#define FILE_ATTRIBUTE_NORMAL 0
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
HANDLE CreateFileA(char const *, unsigned long, unsigned long, void *, unsigned long, unsigned long, HANDLE);
int ReadFile(HANDLE, void *, unsigned long, unsigned long *, void *);
int CloseHandle(HANDLE);
HANDLE FindFirstFileA(char const *, WIN32_FIND_DATAA *);
int FindNextFileA(HANDLE, WIN32_FIND_DATAA *);
"""

HARNESS = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "halo_map_families.h"

struct fake_file
{
	char const *path;
	unsigned long version;
	unsigned long scenario_type;
	unsigned long bytes;
};

static struct fake_file files[] =
{
	{ "d:\\maps_ce\\campaign_one.map", 609, 0, 25 },
	{ "d:\\maps_ce\\campaign_two.map", 609, 0, 25 },
	{ "d:\\custom_maps\\campaign_one.map", 609, 0, 25 },
	{ "d:\\maps_ce\\multiplayer_one.map", 609, 1, 25 },
	{ "d:\\custom_maps\\multiplayer_one.map", 609, 1, 25 },
	{ "d:\\maps_ce\\unsupported_type.map", 609, 2, 25 },
	{ "d:\\maps_ce\\wrong_version.map", 7, 0, 25 },
	{ "d:\\maps_ce\\truncated.map", 609, 0, 10 },
	{ "d:\\maps_md\\halomd_one.map", 7, 1, 25 },
	{ "d:\\maps_pc\\retail_one.map", 7, 1, 25 },
};

struct fake_handle
{
	int find;
	long index;
};

static struct fake_handle file_handle, find_handle;
static char find_handle_folder[288];

struct network_game_server { int changed; };
struct game_variant { wchar_t human_readable_game_description[32]; };
struct player_profile { int unused; };
struct campaign_level_state { boolean available; };
struct campaign_state { short shown_level; struct campaign_level_state levels[10]; };
struct level_list_state { short kind; short chosen; short map_count; char const *const *map_names; };
static struct campaign_state campaign;
static struct level_list_state level_list;
static struct network_game_server fake_server;
static int server_available;
static short chosen_difficulty;
static char chosen_map[64];
static struct game_variant chosen_variant;
static long player_spawn_count;
static char current_campaign_map[64];
static char launched_campaign_map[64];
static long deferred_map_change_count;
static char *campaign_names[] = { "campaign_one@ce" };

#define MAP_KIND_SINGLEPLAYER 0
#define MAP_KIND_MULTIPLAYER 1
#define MAP_KIND_CUSTOM_SINGLEPLAYER 2
#define NUMBER_OF_SINGLE_PLAYER_LEVELS 10

const char *cache_files_map_directory(void)
{
	return "d:\\maps\\";
}

char const *config_string(char const *name)
{
	(void)name;
	return "";
}

static int find_next_match(long *index, char const *folder, WIN32_FIND_DATAA *data)
{
	long i;
	size_t folder_length = strlen(folder);

	for (i = *index; i < (long)(sizeof(files) / sizeof(files[0])); i++)
	{
		char const *name;
		if (strncmp(files[i].path, folder, folder_length))
			continue;
		name = files[i].path + folder_length;
		if (!strchr(name, '\\') && strlen(name) >= 4 && !strcmp(name + strlen(name) - 4, ".map"))
		{
			snprintf(data->cFileName, sizeof(data->cFileName), "%s", name);
			*index = i + 1;
			return 1;
		}
	}
	return 0;
}

HANDLE FindFirstFileA(char const *pattern, WIN32_FIND_DATAA *data)
{
	char folder[288];
	char const *wildcard = strchr(pattern, '*');
	if (!wildcard || (size_t)(wildcard - pattern) >= sizeof(folder))
		return INVALID_HANDLE_VALUE;
	memcpy(folder, pattern, (size_t)(wildcard - pattern));
	folder[wildcard - pattern] = 0;
	find_handle.find = 1;
	find_handle.index = 0;
	/* Keep the folder in a fixed test buffer for FindNextFileA. */
	memcpy(find_handle_folder, folder, strlen(folder) + 1);
	if (!find_next_match(&find_handle.index, folder, data))
		return INVALID_HANDLE_VALUE;
	return &find_handle;
}

int FindNextFileA(HANDLE handle, WIN32_FIND_DATAA *data)
{
	return handle == &find_handle && find_next_match(&find_handle.index, find_handle_folder, data);
}

HANDLE CreateFileA(char const *path, unsigned long access, unsigned long share, void *security,
	unsigned long creation, unsigned long attributes, HANDLE template_file)
{
	long i;
	(void)access; (void)share; (void)security; (void)creation; (void)attributes; (void)template_file;
	for (i = 0; i < (long)(sizeof(files) / sizeof(files[0])); i++)
	{
		if (!strcmp(path, files[i].path))
		{
			file_handle.find = 0;
			file_handle.index = i;
			return &file_handle;
		}
	}
	return INVALID_HANDLE_VALUE;
}

int ReadFile(HANDLE handle, void *buffer, unsigned long size, unsigned long *read, void *overlapped)
{
	unsigned long header[25];
	struct fake_file const *file;
	(void)overlapped;
	if (handle != &file_handle || file_handle.find)
		return 0;
	file = &files[file_handle.index];
	memset(header, 0, sizeof(header));
	header[0] = 'head';
	header[1] = file->version;
	header[24] = file->scenario_type;
	*read = file->bytes < size ? file->bytes * sizeof(unsigned long) : size;
	memcpy(buffer, header, *read);
	return 1;
}

int CloseHandle(HANDLE handle)
{
	(void)handle;
	return 1;
}

struct network_game_server *global_network_game_server_get(void)
{
	return server_available ? &fake_server : NULL;
}

short main_get_solo_level_from_name(char const *name)
{
	return name && !strcmp(name, "stock_campaign") ? 0 : -1;
}

short ui_map_campaign_find(char const *name)
{
	return name && !strcmp(name, "campaign_one@ce") ? 0x1000 : -1;
}

long ui_map_campaign_count(void)
{
	return 1;
}

char **ui_map_campaign_names(void)
{
	return campaign_names;
}

void main_set_difficulty(short difficulty)
{
	chosen_difficulty = difficulty;
}

char const *main_get_map_name(void)
{
	return current_campaign_map;
}

char const *main_get_solo_level_name(short level)
{
	(void)level;
	return "stock_campaign";
}

boolean campaign_profile(short controller, struct player_profile *profile)
{
	(void)controller;
	(void)profile;
	return TRUE;
}

boolean campaign_fail(void)
{
	return FALSE;
}

void main_set_map_name(char const *name)
{
	snprintf(launched_campaign_map, sizeof(launched_campaign_map), "%s", name);
}

void main_defer_map_map_change(void)
{
	deferred_map_change_count++;
}

short main_get_difficulty(void)
{
	return chosen_difficulty;
}

void player_ui_set_single_player_local_player_controller(short player, short controller)
{
	(void)player;
	(void)controller;
}

void game_connection_set(short connection)
{
	(void)connection;
}

void main_menu_switch_to_single_player(void)
{
}

void player_ui_remember_player1_profile(boolean remember)
{
	(void)remember;
}

void ui_play_audio_feedback_sound(short sound)
{
	(void)sound;
}

void main_set_multiplayer_map_name(char const *name)
{
	snprintf(chosen_map, sizeof(chosen_map), "%s", name);
}

void network_game_server_port_set_cooperative(struct network_game_server *server, short difficulty)
{
	server->changed++;
	chosen_difficulty = difficulty;
}

void network_game_server_change_map_name(struct network_game_server *server, char const *name)
{
	server->changed++;
	snprintf(chosen_map, sizeof(chosen_map), "%s", name);
}

void network_game_server_change_game_variant(struct network_game_server *server, struct game_variant const *variant)
{
	server->changed++;
	chosen_variant = *variant;
}

#define csmemset memset
#define ustrncpy wcsncpy
#define PIN(value, minimum, maximum) ((value) < (minimum) ? (minimum) : ((value) > (maximum) ? (maximum) : (value)))
#define SOUND_FORWARD 1
/* CAMPAIGN_START_FUNCTION */
/* DIFFICULTY_START_FUNCTION */
/* CUSTOM_CAMPAIGN_MAP_NAME_FUNCTION */
/* LEVEL_CHOOSE_FUNCTION */
/* COOPERATIVE_LAUNCH_FUNCTION */

static void emit(char const *file, void *context)
{
	printf("%s %s\n", (char const *)context, file);
}

static int cooperative_launch_test(void)
{
	server_available = 0;
	if (ui_widget_port_cooperative_level_choose("stock_campaign", 2))
		return 1;
	server_available = 1;
	if (!ui_widget_port_cooperative_level_choose("stock_campaign", 2) || strcmp(chosen_map, "stock_campaign"))
		return 2;
	fake_server.changed = 0;
	if (!ui_widget_port_cooperative_level_choose("campaign_one@ce", 3) ||
		strcmp(chosen_map, "campaign_one@ce") || chosen_difficulty != 3 || fake_server.changed != 3 ||
		wcscmp(chosen_variant.human_readable_game_description, L"Co-op"))
		return 3;
	fake_server.changed = 0;
	if (ui_widget_port_cooperative_level_choose("multiplayer_one@ce", 1) || fake_server.changed)
		return 4;
	if (ui_widget_port_cooperative_level_choose(NULL, 1))
		return 5;
	return 0;
}

static int solo_campaign_launch_test(void)
{
	snprintf(current_campaign_map, sizeof(current_campaign_map), "%s", "campaign_one@ce");
	if (!difficulty_start(9, 1) || strcmp(launched_campaign_map, "campaign_one@ce") || chosen_difficulty != 3)
		return 1;
	snprintf(current_campaign_map, sizeof(current_campaign_map), "%s", "multiplayer_one@ce");
	if (!difficulty_start(-4, 0) || strcmp(launched_campaign_map, "stock_campaign") || chosen_difficulty != 0)
		return 2;
	return 0;
}

static int map_kind_launch_test(void)
{
	char const *multiplayer_names[] = { "multiplayer_one@ce" };
	long deferred_before = deferred_map_change_count;

	campaign.shown_level = 0;
	campaign.levels[0].available = TRUE;
	level_list.kind = MAP_KIND_CUSTOM_SINGLEPLAYER;
	level_list.chosen = 0;
	if (!level_choose(1) || strcmp(launched_campaign_map, "campaign_one@ce") ||
		deferred_map_change_count != deferred_before + 1)
		return 1;
	level_list.chosen = 1;
	if (level_choose(1) || deferred_map_change_count != deferred_before + 1)
		return 2;
	level_list.kind = MAP_KIND_MULTIPLAYER;
	level_list.chosen = 0;
	level_list.map_count = 1;
	level_list.map_names = multiplayer_names;
	if (level_choose(0) || strcmp(launched_campaign_map, "multiplayer_one@ce"))
		return 3;
	return 0;
}

int main(void)
{
	map_family_list_custom_campaign(emit, "campaign");
	map_family_list(_map_family_custom_edition, emit, "ce-multiplayer");
	map_family_list(_map_family_halomd, emit, "halomd-multiplayer");
	map_family_list(_map_family_halo_pc, emit, "retail-multiplayer");
	if (cooperative_launch_test())
		return 1;
	puts("behavior cooperative");
	if (solo_campaign_launch_test())
		return 2;
	puts("behavior difficulty");
	if (map_kind_launch_test())
		return 3;
	puts("behavior map-kind");
	return 0;
}
"""


class TestCampaignMapDiscovery(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = shutil.which("clang") or shutil.which("cc") or shutil.which("gcc")
        if cc is None:
            raise unittest.SkipTest("no C compiler")
        cls.temporary_directory = tempfile.TemporaryDirectory(prefix="map-campaign-")
        cls.work = Path(cls.temporary_directory.name)
        (cls.work / "cseries").mkdir()
        (cls.work / "cseries.h").write_text(STUB_CSERIES)
        (cls.work / "cseries" / "cseries_windows.h").write_text(STUB_WINDOWS)
        menu_source = harness.read("port/linux/game/menu_functions.c")
        campaign_start_function = harness.function(menu_source, "campaign_start")
        difficulty_start_function = harness.function(menu_source, "difficulty_start")
        campaign_name_function = harness.function(menu_source, "custom_campaign_map_name")
        level_choose_function = harness.function(menu_source, "level_choose")
        cooperative_function = harness.function(
            harness.read("source/interface/ui_widget_event_handler_functions.c"),
            "ui_widget_port_cooperative_level_choose",
        )
        source = HARNESS.replace("/* CAMPAIGN_START_FUNCTION */", campaign_start_function)
        source = source.replace("/* DIFFICULTY_START_FUNCTION */", difficulty_start_function)
        source = source.replace("/* CUSTOM_CAMPAIGN_MAP_NAME_FUNCTION */", campaign_name_function)
        source = source.replace("/* LEVEL_CHOOSE_FUNCTION */", level_choose_function)
        (cls.work / "harness.c").write_text(source.replace("/* COOPERATIVE_LAUNCH_FUNCTION */", cooperative_function))
        shutil.copy(ROOT / "port/linux/include/halo_map_families.h", cls.work / "halo_map_families.h")
        binary = cls.work / "campaign-maps"
        subprocess.run([cc, "-std=gnu99", "-Wall", "-Werror", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-DHALO_CUSTOM_EDITION", "-I", str(cls.work),
                        str(cls.work / "harness.c"), str(ROOT / "port/linux/game/map_families.c"), "-o", str(binary)],
                       check=True)
        output = subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout.splitlines()
        cls.discovered = {
            label: sorted(name for prefix, name in (line.split() for line in output) if prefix == label)
            for label in ("campaign", "ce-multiplayer", "halomd-multiplayer", "retail-multiplayer")
        }
        cls.behaviors = {line.split()[1] for line in output if line.startswith("behavior ")}

    @classmethod
    def tearDownClass(cls):
        cls.temporary_directory.cleanup()

    def test_campaign_discovery_filters_scenario_type_and_cache_version(self):
        self.assertEqual(self.discovered["campaign"], ["campaign_one", "campaign_two"])

    def test_campaign_discovery_keeps_existing_multiplayer_family_lists(self):
        self.assertEqual(self.discovered["ce-multiplayer"], ["multiplayer_one"])
        self.assertEqual(self.discovered["halomd-multiplayer"], ["halomd_one"])
        self.assertEqual(self.discovered["retail-multiplayer"], ["retail_one"])

    def test_campaign_and_multiplayer_menu_choices_launch(self):
        self.assertIn("map-kind", self.behaviors)

    def test_custom_campaign_difficulty_and_fallback(self):
        self.assertIn("difficulty", self.behaviors)

    def test_campaign_and_stock_maps_are_valid_network_coop_choices(self):
        self.assertIn("cooperative", self.behaviors)


if __name__ == "__main__":
    unittest.main()
