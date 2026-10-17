#!/bin/sh
# The test driver lets a test process read an app's widget tree and press,
# type and drag through the app's own input path, natively in C and C++.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_test_driver_test.zi

for target in c cpp; do
    "$ziran" build --target="$target" --no-main \
        --root "$repo/tests" --module-path "$repo/src/ui" \
        --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
        -o "$work/$target" "$source"
    suffix=c
    compiler=${CC:-cc}
    standard=c11
    if [ "$target" = cpp ]; then
        suffix=cpp
        compiler=${CXX:-c++}
        standard=c++17
    fi
    "$compiler" -std="$standard" -O1 -I"$ziran_root/include" -I"$work/$target" \
        "$work/$target"/*."$suffix" -lm -o "$work/$target/run"
    mkdir "$work/$target-files"
    status=0
    KRYON_TEST_DRIVER="$work/$target-files" "$work/$target/run" || status=$?
    if [ "$status" -ne 0 ]; then
        echo "test driver ($target) failed with $status" >&2
        exit 1
    fi
    # Without KRYON_TEST_DRIVER the app's input is its own (status 100).
    status=0
    env -u KRYON_TEST_DRIVER "$work/$target/run" || status=$?
    if [ "$status" -ne 100 ]; then
        echo "test driver ($target) acted without KRYON_TEST_DRIVER: $status" >&2
        exit 1
    fi
done
echo 'Test driver finds, presses, types, drags and waits for settled frames'
