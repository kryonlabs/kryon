#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/application-entry"
mkdir -p "$work"
for platform in native web; do
    define_args=""
    if test "$platform" = web; then define_args="--define PLATFORM_WEB"; fi
    "$ziran" ir --project --root "$repo/tests" $define_args -o "$work/$platform-ir" "$repo/tests/application_entry_test.zi"
    for form in source saved; do
        source="$repo/tests/application_entry_test.zi"
        root="$repo/tests"
        if test "$form" = saved; then
            source="$work/$platform-ir/application_entry_test.zir"
            root="$work/$platform-ir"
        fi
        output="$work/$platform-$form"
        "$ziran" build --project --target=c --root "$root" $define_args -o "$output" "$source"
        "${CC:-cc}" -std=c11 -O1 -I"$output" "$output"/*.c -o "$output/test"
        "$output/test" 'first argument' --second
    done
done
echo 'Application entry: native arguments and browser empty arguments passed from source and saved IR'
