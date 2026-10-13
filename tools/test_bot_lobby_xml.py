"""Native bot widgets, generator idempotence and production build filters."""
import importlib.util
import os
import subprocess
import tempfile
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / "port/assets/menus"
BASE = "main_menu/multiplayer_type_select"


def generator():
    spec = importlib.util.spec_from_file_location("port_settings", ROOT / "tools/port_settings.py")
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_generated_bot_widgets(tmp_path: Path):
    module = generator()
    name = "main_menu.multiplayer_type_select.bots.xml"
    generated = "\n".join(module.multiplayer_files()[name])
    assert (MENUS / "ce" / name).read_text() == generated
    widgets = {widget.attrib["name"]: widget for widget in ET.fromstring(generated).findall("widget")}
    screen = widgets[f"{BASE}/bots/bot_screen"]
    assert screen.find("on[@run='port bot lobby open']") is not None
    assert screen.find("on[@run='port bot lobby start']") is not None
    assert all(widget.get("feature") == "bots" for widget in widgets.values())
    # Runtime text has no inline string list: rendering must not overwrite it.
    status = widgets[f"{BASE}/bots/bot_status"]
    assert "text" not in status.attrib and "string_list" not in status.attrib
    for row in range(8):
        widget = widgets[f"{BASE}/bots/bot_row_{row}"]
        assert "text" not in widget.attrib and "string_list" not in widget.attrib
        assert widget.find("on[@run='port bot lobby change']") is not None
    listing = ET.parse(MENUS / "ce/main_menu.multiplayer_type_select.xml").find(
        f"widget[@name='{BASE}/multiplayer_type_select_list']")
    assert listing is not None
    item = listing.find(f"child[@widget='{BASE}/multiplayer_type_bot_item']")
    assert item is not None and item.get("feature") == "bots"
    assert listing.find("data[@input='port bot menu update']") is not None
    # A scoped refresh must preserve unrelated screens and every manifest order.
    (tmp_path / "ce").mkdir()
    for relative in ("ce/main_menu.multiplayer_type_select.xml", "ce/strings.xml", "menus.json"):
        (tmp_path / relative).write_bytes((MENUS / relative).read_bytes())
    sentinel = tmp_path / "ce/unrelated.xml"
    sentinel.write_text("unchanged")
    module.refresh_bot_menus(tmp_path)
    first = {path.name: path.read_bytes() for path in tmp_path.rglob("*") if path.is_file()}
    module.refresh_bot_menus(tmp_path)
    assert first == {path.name: path.read_bytes() for path in tmp_path.rglob("*") if path.is_file()}
    assert sentinel.read_text() == "unchanged"


def production_filter(path):
    source = path.read_text()
    start = source.index("static int for_this_platform(")
    end = source.index("\n/* room for one more", start)
    return source[start:end]


def test_production_bot_feature_filter():
    for path in ("port/linux/src/menu_files.c", "port/macos/metal/renderer/menu_files.c"):
        for bots, browser, android in ((0, 0, 0), (1, 0, 0), (1, 1, 0), (1, 1, 1)):
            check_production_bot_feature_filter(path, bots, browser, android)


def check_production_bot_feature_filter(path: str, bots: int, browser: int, android: int):
    fixture = r'''
#include <assert.h>
#include <string.h>
typedef char XML_Char;
struct reader { int failed; };
static void reader_error(struct reader *reader, const char *format, ...) {
    (void)format; reader->failed=1;
}
/* FILTER */
int main(void) {
    struct reader reader={0};
    const char *empty[]={NULL};
    const char *bot[]={"feature","bots",NULL};
    const char *desktop[]={"platform","desktop",NULL};
    const char *both[]={"platform","desktop","feature","bots",NULL};
    const char *reverse[]={"feature","bots","platform","desktop",NULL};
    const char *invalid[]={"feature","unknown",NULL};
    const char *bad_platform[]={"platform","other","feature","bots",NULL};
    assert(for_this_platform(&reader,empty));
    assert(for_this_platform(&reader,bot)==BOT_ALLOWED);
    assert(for_this_platform(&reader,desktop)==DESKTOP);
    assert(for_this_platform(&reader,both)==(BOT_ALLOWED && DESKTOP));
    assert(for_this_platform(&reader,reverse)==(BOT_ALLOWED && DESKTOP));
    assert(!reader.failed);
    for_this_platform(&reader,invalid); assert(reader.failed);
    reader.failed=0; for_this_platform(&reader,bad_platform); assert(reader.failed);
    return 0;
}
'''.replace("/* FILTER */", production_filter(ROOT / path))
    flags = [f"-DBOT_ALLOWED={int(bots and browser)}", f"-DDESKTOP={int(not android)}"]
    flags += [f"-D{name}" for name, enabled in (
        ("HALO_FEATURE_BOTS", bots), ("HALO_GAME_BROWSER", browser), ("HALO_ANDROID", android)) if enabled]
    with tempfile.TemporaryDirectory(prefix="bot-menu-filter-") as directory:
        source = Path(directory) / "test.c"
        executable = Path(directory) / "test"
        source.write_text(fixture)
        subprocess.run([os.environ.get("CC", "clang"), "-std=gnu89", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", *flags, str(source), "-o", str(executable)],
                       check=True, timeout=60)
        subprocess.run([str(executable)], check=True, timeout=30)
