#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-cell-text"
mkdir -p "$work"
source="$repo/tests/pixmap_cell_text_test.zi"
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in ${PIXMAP_CELL_TARGETS:-c go}; do
    executable=--exe
    package=; if test "$target" = go; then package='--pkg main'; fi
    "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_cell_text_test:main $package $executable -o "$work/$target" "$source"
    if test "$target" = go; then
        env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$work/$target"/*.go
    else
        env -u DISPLAY -u WAYLAND_DISPLAY "$work/$target/pixmap_cell_text_test"
    fi
done
if test "${PIXMAP_CELL_BUNDLES:-1}" = 0; then exit 0; fi
"$ziran" bundle --project --root "$repo/tests" --entry pixmap_cell_text_test:main -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry pixmap_cell_text_test:main -o "$work/saved.zx" "$work/ir/pixmap_cell_text_test.zir"
test "$(env -u DISPLAY -u WAYLAND_DISPLAY "$ziran" run "$work/source.zx" | tail -1)" = 0
test "$(env -u DISPLAY -u WAYLAND_DISPLAY "$ziran" run "$work/saved.zx" | tail -1)" = 0
