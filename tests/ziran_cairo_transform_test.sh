#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/cairo-transform"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
"$ziran" ir $kryon_project_args --root "$repo/tests" --define PLATFORM_DESKTOP -o "$work/ir" "$repo/tests/cairo_transform_test.zi"
for form in source saved; do
    input="$repo/tests/cairo_transform_test.zi"; root="$repo/tests"
    if test "$form" = saved; then input="$work/ir/cairo_transform_test.zir"; root="$work/ir"; fi
    output="$work/$form"
    "$ziran" build $kryon_project_args --target=c --root "$root" --define PLATFORM_DESKTOP --prune-stale -o "$output" "$input"
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
        $(find "$output" -name '*.c') "$ziran_root/build/libziran.a" -Wl,--gc-sections \
        $(pkg-config --libs sdl2 cairo freetype2 fontconfig pangocairo) -ldl -lm -lpthread -o "$output/test"
    xvfb-run -a -s '-screen 0 1920x1080x24 -dpi 96 -nolisten tcp' "$output/test"
done
echo 'Cairo camera: negative-X rail, offset content without a strip, public widget pointer hit/click and resized camera passed (source/saved IR, private1920x1080@96dpi)'
