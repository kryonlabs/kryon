#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
output_root="$repo/build/scratch/android-process"
mkdir -p "$output_root/files-absent" "$output_root/files-present/kryon-test-driver"
test ! -e "$output_root/files-absent/kryon-test-driver"
for mode in normal driver; do
    defines="--define ANDROID_BUILD --define __x86_64__"
    if [ "$mode" = driver ]; then
        defines="$defines --define KRYON_TEST_DRIVER"
    fi
    for form in source saved; do
        input="$repo/tests/android_process_behavior.zi"
        if [ "$form" = saved ]; then
            # shellcheck disable=SC2086
            "$ziran" ir $kryon_project_args $defines --root "$repo/tests" \
                -o "$output_root/$mode/ir" "$input"
            input="$output_root/$mode/ir/android_process_behavior.zir"
        fi
        output="$output_root/$mode/$form"
        # shellcheck disable=SC2086
        "$ziran" build $kryon_project_args $defines --target=c --root "$repo/tests" \
            --prune-stale -o "$output" "$input"
        includes=$(find "$output" -type d -printf ' -iquote %p')
        # shellcheck disable=SC2046,SC2086
        "${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
            -I"$ziran_root/include" $includes $(find "$output" -name '*.c') \
            "$ziran_root/build/libziran.a" -Wl,--gc-sections -lm -lpthread -ldl \
            -o "$output/test"
        for folder in absent present; do
            expected_driver=""
            if [ "$mode" = driver ] && [ "$folder" = present ]; then
                expected_driver="$output_root/files-$folder/kryon-test-driver"
            fi
            env -u KRYON_TEST_DRIVER -u TMPDIR \
                KRYON_PROCESS_PRIVATE_FILES="$output_root/files-$folder" \
                KRYON_PROCESS_EXPECT_TMPDIR="$output_root/files-$folder/tmp" \
                KRYON_PROCESS_EXPECT_DRIVER="$expected_driver" "$output/test"
        done
    done
done
echo 'Android process: normal/driver builds with absent/leftover folders passed (source and saved IR)'
