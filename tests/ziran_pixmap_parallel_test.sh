#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-parallel"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
sh "$repo/tests/pixmap_blend_test.sh"
for target in c cpp; do
    exe=--exe; if test "$target" = cpp; then exe=; fi
    "$ziran" build --project --root "$repo/tests" --target="$target" \
        --entry pixmap_parallel_test:main $exe -o "$work/$target" "$repo/tests/pixmap_parallel_test.zi"
    if test "$target" = cpp; then
        "${CXX:-c++}" -std=c++17 -O2 -I"$ziran_root/include" "$work/$target"/*.cpp -pthread -lm -o "$work/$target/pixmap_parallel_test"
    fi
    binary="$work/$target/pixmap_parallel_test"
    ZIRAN_PAR_THREADS=1 "$binary" > "$work/serial-$target.txt"
    env -u ZIRAN_PAR_THREADS "$binary" > "$work/default-$target.txt"
    ZIRAN_PAR_THREADS=8 "$binary" > "$work/eight-$target.txt"
    test "$(head -1 "$work/serial-$target.txt")" = 'workers 1'
    test "$(head -1 "$work/default-$target.txt")" = 'workers 4'
    test "$(head -1 "$work/eight-$target.txt")" = 'workers 8'
    test "$(tail -1 "$work/serial-$target.txt")" = "$(tail -1 "$work/default-$target.txt")"
    test "$(tail -1 "$work/serial-$target.txt")" = "$(tail -1 "$work/eight-$target.txt")"
done
test "$(tail -1 "$work/default-c.txt")" = "$(tail -1 "$work/default-cpp.txt")"
# Saved portable IR has no native thread capability and keeps identical pixels.
"$ziran" ir --project --root "$repo/tests" --entry pixmap_parallel_test:main \
    -o "$work/portable-ir" "$repo/tests/pixmap_parallel_test.zi"
"$ziran" build --target=c --root "$work/portable-ir" --entry pixmap_parallel_test:main \
    --exe -o "$work/portable" "$work/portable-ir/pixmap_parallel_test.zir"
"$work/portable/pixmap_parallel_test" > "$work/portable.txt"
test "$(head -1 "$work/portable.txt")" = 'workers 1'
test "$(tail -1 "$work/portable.txt")" = "$(tail -1 "$work/default-c.txt")"
printf 'Native parallel and portable rasters match serial pixels, including overlapping image storage.\n'
