#!/bin/sh
# The runner owns a private display and tests source plus saved native modules.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec python3 "$repo/tests/pixmap_sdl_test.py"
