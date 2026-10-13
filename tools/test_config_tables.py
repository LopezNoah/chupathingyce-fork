"""The macOS Metal build's settings table (port/macos/metal/renderer/port_config.c)
is its own copy of port/linux/src/port_config.c's. Every shared setting must be
in both: a setting the Metal table lacks reads as unknown, so config_boolean()
answers false whatever its default. display.viewmodel_visible was missing, and
no first-person weapon was drawn on macOS."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LINUX = ROOT / "port/linux/src/port_config.c"
METAL = ROOT / "port/macos/metal/renderer/port_config.c"
ROW = re.compile(r'^\s*\{ "([a-z_]+\.[a-z_0-9]+)", (_config_\w+), ("(?:[^"\\]|\\.)*"|\w+)', re.M)


def settings(path):
    return {name: (kind, default) for name, kind, default in ROW.findall(path.read_text())}


def test_metal_table_has_every_shared_setting():
    linux, metal = settings(LINUX), settings(METAL)
    assert len(linux) > 100 and len(metal) > 100, "the table rows were not found"
    missing = sorted(set(linux) - set(metal))
    assert not missing, f"missing from {METAL.relative_to(ROOT)}: {missing}"


def test_shared_settings_agree():
    linux, metal = settings(LINUX), settings(METAL)
    different = sorted(name for name in set(linux) & set(metal) if linux[name] != metal[name])
    assert not different, f"type or default differs: {[(n, linux[n], metal[n]) for n in different]}"


if __name__ == "__main__":
    test_metal_table_has_every_shared_setting()
    test_shared_settings_agree()
    print("settings tables agree")
