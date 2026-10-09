#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/desktop-cadence"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
    -o "$work/ir" "$repo/tests/desktop_cadence_test.zi"
for form in source saved; do
    source="$repo/tests/desktop_cadence_test.zi"
    source_root="$repo/tests"
    if test "$form" = saved; then
        source="$work/ir/desktop_cadence_test.zir"
        source_root="$work/ir"
    fi
    for target in c cpp go; do
        executable=--exe
        package=
        if test "$target" = cpp; then executable=; fi
        if test "$target" = go; then package='--pkg main'; fi
        "$ziran" build --root "$source_root" --module-path "$repo/src/backend" \
            --module-path "$work/ir" --target="$target" $executable $package \
            --entry desktop_cadence_test:main -o "$work/$form-$target" "$source"
        if test "$target" = cpp; then
            printf '#include "desktop_cadence_test.hpp"\nint main() { return desktop_cadence_test_main(); }\n' > "$work/$form-$target/entry.cpp"
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$work/$form-$target"/*.cpp \
                -o "$work/$form-$target/desktop_cadence_test"
        fi
        if test "$target" = go; then
            env GO111MODULE=off go run "$work/$form-$target"/*.go
        else
            "$work/$form-$target/desktop_cadence_test"
        fi
    done
    "$ziran" bundle --root "$source_root" --module-path "$repo/src/backend" \
        --module-path "$work/ir" --entry desktop_cadence_test:main \
        -o "$work/$form.zib" "$source"
    test "$("$ziran" run "$work/$form.zib")" = 0
done
printf '%s\n' 'Desktop cadence includes drawing cost, preserves deadlines and resets hidden windows'
