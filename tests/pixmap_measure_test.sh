#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-measure"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
source="$repo/tests/pixmap_measure_test.zi"
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in ${PIXMAP_TEST_TARGETS:-c cpp go rust py}; do
    for input in source saved; do
        executable=--exe; if test "$target" = cpp; then executable=; fi
        package=; if test "$target" = go; then package='--pkg main'; fi
        if test "$input" = source; then
            "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_measure_test:main $package $executable -o "$work/$target-$input" "$source"
        else
            "$ziran" build --root "$work/ir" --module-path "$work/ir" --target="$target" --entry pixmap_measure_test:main $package $executable -o "$work/$target-$input" "$work/ir/pixmap_measure_test.zir"
        fi
        if test "$target" = cpp; then "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$work/$target-$input"/*.cpp -lm -o "$work/$target-$input/pixmap_measure_test"; fi
        if test "$target" = go; then
            test "$(env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$work/$target-$input"/*.go)" = 'Pixmap measurement and coverage parity passed'
        elif test "$target" = py; then
            python3 "$work/$target-$input/__main__.py"
        elif test "$target" = rust; then
            CARGO_TARGET_DIR="$work/rust-target-$input" cargo run --offline --quiet \
                --manifest-path "$work/$target-$input/Cargo.toml"
        else
            env -u DISPLAY -u WAYLAND_DISPLAY "$work/$target-$input/pixmap_measure_test"
        fi
    done
done
"$ziran" bundle --project --root "$repo/tests" --entry pixmap_measure_test:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry pixmap_measure_test:main -o "$work/saved.zib" "$work/ir/pixmap_measure_test.zir"
test "$(env -u DISPLAY -u WAYLAND_DISPLAY "$ziran" run "$work/source.zib" | tail -1)" = 0
test "$(env -u DISPLAY -u WAYLAND_DISPLAY "$ziran" run "$work/saved.zib" | tail -1)" = 0
