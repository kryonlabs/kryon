#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=${PIXMAP_CELL_RUN_WORK:-"$repo/build/scratch/pixmap-cell-run"}
mkdir -p "$work"
source="$repo/tests/pixmap_cell_run_test.zi"
entry=pixmap_cell_run_test:main
"$ziran" ir --project --offline --root "$repo/tests" -o "$work/ir" "$source"
for target in ${PIXMAP_CELL_RUN_TARGETS:-c go}; do
    for mode in source saved; do
        dest="$work/$target-$mode"
        executable=--exe
        package=; if test "$target" = go; then package='--pkg main'; fi
        if test "$mode" = source; then
            "$ziran" build --project --offline --root "$repo/tests" --target="$target" \
                --entry "$entry" $package $executable -o "$dest" "$source"
        else
            "$ziran" build --root "$work/ir" --module-path "$work/ir" --target="$target" \
                --entry "$entry" $package $executable -o "$dest" "$work/ir/pixmap_cell_run_test.zir"
        fi
        if test "$target" = go; then
            env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS GO111MODULE=off go run "$dest"/*.go
        else
            env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS "$dest/pixmap_cell_run_test"
        fi
    done
done
"$ziran" bundle --project --offline --root "$repo/tests" --entry "$entry" -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry "$entry" -o "$work/saved.zx" "$work/ir/pixmap_cell_run_test.zir"
for mode in source saved; do
    env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
        "$ziran" run "$work/$mode.zx" > "$work/$mode-vm.log"
    test "$(tail -1 "$work/$mode-vm.log")" = 0
done
