#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-surface"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
source="$repo/tests/pixmap_surface_test.zi"
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in c go; do
    for input in source saved; do
        executable=--exe
        package=; if test "$target" = go; then package='--pkg main'; fi
        if test "$input" = source; then
            "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_surface_test:main $package $executable -o "$work/$target-$input" "$source"
        else
            "$ziran" build --root "$work/ir" --module-path "$work/ir" --target="$target" --entry pixmap_surface_test:main $package $executable -o "$work/$target-$input" "$work/ir/pixmap_surface_test.zir"
        fi
        if test "$target" = go; then
            test "$(env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$work/$target-$input"/*.go)" = 'Pixmap caller-owned surfaces passed'
        else
            env -u DISPLAY -u WAYLAND_DISPLAY "$work/$target-$input/pixmap_surface_test"
        fi
    done
done
"$ziran" bundle --project --root "$repo/tests" --entry pixmap_surface_test:main -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry pixmap_surface_test:main -o "$work/saved.zx" "$work/ir/pixmap_surface_test.zir"
test "$(env -u DISPLAY -u WAYLAND_DISPLAY "$ziran" run "$work/source.zx" | tail -1)" = 0
test "$(env -u DISPLAY -u WAYLAND_DISPLAY "$ziran" run "$work/saved.zx" | tail -1)" = 0
