"""Run Forge's file reader (port/linux/game/forge.c) against hostile files.

The reader's functions are taken from the source, not copied, and compiled
with AddressSanitizer and UndefinedBehaviorSanitizer when available."""

import argparse
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BEGIN = "/* a JSON reader for the file Forge writes"
END = "/* one placement or addition: its fields */"


def reader_source():
    source = (ROOT / "port/linux/game/forge.c").read_text()
    start = source.index(BEGIN)
    return source[start:source.index(END, start)]


def run(cc, sanitize=True):
    fixture = (ROOT / "tools/test_forge_json.c.in").read_text()
    fixture = fixture.replace("/* PRODUCTION_BODIES */", reader_source())
    with tempfile.TemporaryDirectory(prefix="forge-json-") as directory:
        path = Path(directory) / "test.c"
        executable = Path(directory) / "test"
        path.write_text(fixture)
        command = [cc, "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                   str(path), "-lm", "-o", str(executable)]
        if sanitize:
            command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                        "-fno-omit-frame-pointer"]
        subprocess.run(command, check=True, timeout=60)
        subprocess.run([str(executable)], check=True, timeout=60)


def test_forge_json():
    run(os.environ.get("CC", "clang"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "clang"))
    parser.add_argument("--no-sanitize", action="store_true")
    args = parser.parse_args()
    run(args.cc, not args.no_sanitize)
