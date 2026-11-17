"""Invalidate CMake configuration when a codec's resolved inputs change."""
import json
from pathlib import Path
import shutil
import sys


def main():
    build = Path(sys.argv[1])
    inputs = dict(argument.split("=", 1) for argument in sys.argv[2:])
    build.mkdir(parents=True, exist_ok=True)
    stamp = build / "kryon-config.json"
    contents = json.dumps(inputs, sort_keys=True, indent=2) + "\n"
    cache = build / "CMakeCache.txt"
    home = "CMAKE_HOME_DIRECTORY:INTERNAL=" + inputs["source"]
    changed = not stamp.is_file() or stamp.read_text() != contents
    if cache.is_file() and home not in cache.read_text().splitlines():
        changed = True
    if changed:
        # Preserve codec archives until the new configuration rebuilds them.
        # CMake cannot reuse either a different top-level or add_subdirectory
        # source tree in one cache, even when both contain the same revision.
        cache.unlink(missing_ok=True)
        shutil.rmtree(build / "CMakeFiles", ignore_errors=True)
        stamp.write_text(contents)


if __name__ == "__main__":
    main()
