#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
env -u DISPLAY -u WAYLAND_DISPLAY make -C "$repo/examples" test
