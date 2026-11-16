#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/window-runtime"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
for host in desktop android; do
    test_name=window_runtime_test
    define=PLATFORM_DESKTOP
    if test "$host" = android; then test_name=window_runtime_android_test; define=ANDROID_BUILD; fi
    "$ziran" ir $kryon_project_args --root "$repo/tests" --define "$define" -o "$work/$host-ir" "$repo/tests/$test_name.zi"
    for form in source saved; do
        source="$repo/tests/$test_name.zi"
        root="$repo/tests"
        if test "$form" = saved; then source="$work/$host-ir/$test_name.zir"; root="$work/$host-ir"; fi
        output="$work/$host-$form"
        "$ziran" build $kryon_project_args --target=c --root "$root" --define "$define" --prune-stale -o "$output" "$source"
        libraries='-lpthread -ldl -lm'
        if test "$host" = desktop; then
            libraries="$libraries $(pkg-config --libs sdl2 cairo freetype2 fontconfig pangocairo)"
        fi
        includes=$(find "$output" -type d -printf ' -iquote %p')
        # shellcheck disable=SC2046,SC2086
        "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
            $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" -Wl,--gc-sections $libraries -o "$output/test"
        if test "$host" = desktop; then xvfb-run -a "$output/test"; else "$output/test"; fi
    done
done
echo 'Window lifecycle: SDL/Cairo pixels, resize, alpha/clip, visibility, close; NativeActivity pixels, pause/resume, close drain passed (source/saved IR, no raylib)'
