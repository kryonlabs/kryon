#!/bin/sh
# Native ABI/call observations through Ziran SDL3 mocks; no SDL installation,
# display, custom C, or GPU is needed. Run this script under build-queue.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
git -C "$repo" check-ignore -q work/sdl3-geometry
work="$repo/work/sdl3-geometry"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
source="$repo/tests/sdl3_geometry_sink_test.zi"
"$ziran" ir --root "$repo/tests" $kryon_ui_args \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    --entry sdl3_geometry_sink_test:main -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input="$work/ir/sdl3_geometry_sink_test.zir"; fi
    # Exported Ziran functions satisfy the foreign SDL3 symbols in this binary.
    LDLIBS= "$ziran" build --root "$repo/tests" $kryon_ui_args \
        --module-path "$repo/src/backend" --module-path "$ziran_root/std" --target=c \
        --entry sdl3_geometry_sink_test:main --exe -o "$work/c" "$input"
    "$work/c/sdl3_geometry_sink_test"
    echo "SDL3 geometry sink: $form native local tests passed"
done
