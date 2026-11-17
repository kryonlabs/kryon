#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/android-present-damage"
mkdir -p "$out"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
"$ziran" ir $kryon_project_args --root "$repo/tests" --define ANDROID_BUILD --entry android_present_damage_test:main -o "$out/ir" "$repo/tests/android_present_damage_test.zi"
for form in source saved; do
    input="$repo/tests/android_present_damage_test.zi"; root="$repo/tests"
    if test "$form" = saved; then input="$out/ir/android_present_damage_test.zir"; root="$out/ir"; fi
    output="$out/$form"
    "$ziran" build $kryon_project_args --target=c --root "$root" --define ANDROID_BUILD --entry android_present_damage_test:main --prune-stale -o "$output" "$input"
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
        $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" -Wl,--gc-sections -ldl -lm -lpthread -o "$output/test"
    "$output/test"
done
