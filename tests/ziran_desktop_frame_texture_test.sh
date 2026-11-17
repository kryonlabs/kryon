#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/desktop-frame-texture"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
"$ziran" ir $kryon_project_args --root "$repo/tests" --define PLATFORM_DESKTOP -o "$work/ir" "$repo/tests/desktop_frame_texture_test.zi"
for form in source saved; do
    input="$repo/tests/desktop_frame_texture_test.zi"; root="$repo/tests"
    if test "$form" = saved; then input="$work/ir/desktop_frame_texture_test.zir"; root="$work/ir"; fi
    output="$work/$form"
    "$ziran" build $kryon_project_args --target=c --root "$root" --define PLATFORM_DESKTOP --prune-stale -o "$output" "$input"
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
        $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" -Wl,--gc-sections \
        $(pkg-config --libs sdl2 cairo freetype2 fontconfig pangocairo) -ldl -lm -lpthread -o "$output/test"
    xvfb-run -a -s '-screen 0 800x600x24 -nolisten tcp' "$output/test"
done
echo 'Desktop frame textures passed source and saved IR on private Xvfb'
