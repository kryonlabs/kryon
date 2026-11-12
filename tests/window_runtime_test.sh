#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/window-runtime"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export HARMONY_DESKTOP_RECOVERY=0
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$repo/tests/window_runtime_test.zi"
for form in source saved; do
    source="$repo/tests/window_runtime_test.zi"
    root="$repo/tests"
    if test "$form" = saved; then
        source="$work/ir/window_runtime_test.zir"
        root="$work/ir"
    fi
    output="$work/$form"
    "$ziran" build --project --target=c --root "$root" -o "$output" "$source"
    "${CC:-cc}" -std=c11 -O1 -I"$output" "$output"/*.c -o "$output/test"
    "$output/test"
done
echo 'Window lifecycle: native source and saved IR passed'
