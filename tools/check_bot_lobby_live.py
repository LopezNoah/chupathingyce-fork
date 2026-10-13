"""Drive the real Xbox or PC/XML Bot Match menu and verify local gameplay.

Needs Xbox maps and a --bots build. Runs in a hidden window with isolated data
and saves, never on the user's desktop. Each run keeps a fresh artifact folder.
"""
import argparse
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(binary: Path, maps: Path, output: Path, menus: str = "xbox") -> Path:
    binary = binary.resolve(strict=True)
    maps = maps.resolve(strict=True)
    for name in ("ui", "bloodgulch", "beavercreek"):
        if not (maps / f"{name}.map").is_file():
            raise ValueError(f"Missing Xbox map: {maps / (name + '.map')}")
    output.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix="bot-lobby-", dir=output.resolve()))
    data, saves = artifacts / "data", artifacts / "saves"
    data.mkdir()
    saves.mkdir()
    (data / "maps").symlink_to(maps, target_is_directory=True)
    # Fresh saves start on Campaign. Select Multiplayer, then its fifth row.
    # Real keyboard arrows choose Battle Creek, 1 friendly + 2 enemies, ODST,
    # and a custom Assault Rifle / Magnum loadout; controller Start launches.
    buttons = (
        "wait wait wait wait wait down wait a wait down wait down wait down "
        "wait down wait a wait key:Right wait key:Down wait key:Down wait "
        "key:Right wait key:Down wait key:Right wait key:Down wait key:Right "
        "wait key:Down wait key:Right wait start"
    )
    if menus == "pc":
        # Start on Multiplayer: six selectable rows precede BOT MATCH.
        buttons = (
            "wait wait wait wait wait down wait down wait down wait down wait down "
            "wait down wait a wait key:Right wait key:Down wait key:Down wait "
            "key:Right wait key:Down wait key:Right wait key:Down wait key:Right "
            "wait key:Down wait key:Right wait start"
        )
    env = {key: value for key, value in os.environ.items() if not key.startswith("HALO_")}
    env.update(
        HALO_DATA_ROOT=str(data), HALO_SAVE_ROOT=str(saves), HALO_MENUS=menus,
        HALO_NET_ONLINE="false", HALO_NET_PUBLIC_LOBBY="false", HALO_NET_HOST_PUBLIC="false",
        HALO_NET_REPORT_EVENTS="false", HALO_NET_REPORT_GAMES="false",
        HALO_NET_LIST_GAMES="false", HALO_NET_ALLOW_UPNP="false", HALO_UPDATE_AUTO="false",
        HALO_HIDDEN_WINDOW="1", HALO_EXIT_AFTER="55", HALO_WINDOW_SCALE="1",
        HALO_BOTS="0", HALO_TEST_INPUT=f"menu:{buttons}",
    )
    if menus == "pc":
        env["HALO_MENU_OPEN"] = "main_menu/multiplayer_type_select/multiplayer_type_select_screen"
    print(f"Artifacts: {artifacts}", flush=True)
    with (artifacts / "process.log").open("w") as log:
        subprocess.run([str(binary)], cwd=ROOT, env=env, stdout=log,
                       stderr=subprocess.STDOUT, check=True, timeout=80)
    log = (data / "debug.txt").read_text(errors="replace")
    assert "starting precaching of map 'beavercreek'" in log, "Start did not load Battle Creek"
    assert log.count("joined (odst)") == 3, "Bot count or difficulty was not applied"
    teams = re.findall(r"bots: bot (\d+) assigned team (\d+) lane", log)
    assert teams == [("1", "0"), ("2", "1"), ("3", "1")], f"Wrong friendly/enemy teams: {teams}"
    assert "Assertion failed" not in log and "### ASSERTION" not in log, "Engine assertion"
    print("PASS: real arrow-key setup loaded Battle Creek with 1 friendly + 2 enemy ODST bots")
    return artifacts


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/macos/halo")
    parser.add_argument("--maps", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path(tempfile.gettempdir()) / "bot-lobby-checks")
    parser.add_argument("--menus", choices=("xbox", "pc"), default="xbox")
    args = parser.parse_args()
    run(args.binary, args.maps, args.output, args.menus)
