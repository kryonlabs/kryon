#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/scratch/session-validation
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/session_validation_test.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    root=$repo/tests
    module_args="--module-path $repo/src/ui"
    if test "$form" = saved; then
        input=$work/ir/session_validation_test.zir
        root=$work/ir
        module_args=""
    fi
    "$ziran" bundle --root "$root" $module_args --entry session_validation_test:main \
        -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 0
    for target in c cpp go; do
        output=$work/$form-$target
        if test "$target" = c; then
            "$ziran" build --target=c --root "$root" $module_args --entry session_validation_test:main --exe -o "$output" "$input"
            "$output/session_validation_test"
        elif test "$target" = cpp; then
            "$ziran" build --target=cpp --root "$root" $module_args --entry session_validation_test:main -o "$output" "$input"
            "${CXX:-c++}" -std=c++17 -O1 -I"$output" -I"$ziran_root/include" "$output"/*.cpp -o "$output/run"
            "$output/run"
        else
            "$ziran" build --target=go --pkg main --root "$root" $module_args --entry session_validation_test:main --exe -o "$output" "$input"
            env GO111MODULE=off go run "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Session validation preserves invalid slots, close/reopen generations, capacity and exhausted epochs'
