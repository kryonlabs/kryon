#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# This list only shrinks as the remaining hosts move to current Ziran.
expected='examples/desktop_host.c
src/backend/composition_host.c
src/backend/font_metrics_host.c
src/backend/image_host.c
src/backend/image_software.c
src/backend/kss_string_host.c
src/backend/pointer_host.c
src/backend/raster_host.c
tools/frame_replay.c'

actual=$(cd "$repo" &&
    rg --files src examples tools -g '*.c' -g '*.cc' -g '*.cpp' -g '*.go' |
    LC_ALL=C sort)

if test "$actual" != "$expected"; then
    printf 'Non-Ziran source inventory changed. Remove migrated paths from the\n' >&2
    printf 'expected list; do not add new handwritten implementation files.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected" "$actual" >&2
    exit 1
fi
