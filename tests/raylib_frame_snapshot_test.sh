#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY DESKTOP_SESSION XDG_SESSION_ID XDG_SESSION_TYPE KRYON_CAPTURE_PATH KRYON_TEST_DRIVER
export YUE_DESKTOP_RECOVERY=0
# Test-owned, ignored output; never replace shared compilers or host binaries.
work="$repo/work/frame-input"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
set -- --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std"
# Exercise the real WidgetHostCall and VM array transport with real raylib data.
guest="$repo/tests/frame_snapshot_guest.zi"
"$ziran" ir "$@" --define KRYON_HOSTED_UI -o "$work/guest-ir" "$guest"
"$ziran" bundle "$@" --define KRYON_HOSTED_UI --entry frame_snapshot_guest:main -o "$work/frame-snapshot.zx" "$guest"
"$ziran" bundle "$@" --define KRYON_HOSTED_UI --entry frame_snapshot_guest:main -o "$work/guest-saved.zx" "$work/guest-ir/frame_snapshot_guest.zir"
cmp "$work/frame-snapshot.zx" "$work/guest-saved.zx"
archive=${RAYLIB_A:-"$repo/build/raylib-ziran/libraylib.a"}
webp=${WEBP_A:-"$repo/build/webp-ziran/libkryonwebp.a"}
test -f "$archive" && test -f "$webp" || { echo 'Set RAYLIB_A and WEBP_A to the existing desktop SDL raylib and kryonwebp archives' >&2; exit 1; }
for mode in serial threaded; do
    defines=
    wraps=
    if [ "$mode" = threaded ]; then
        defines='--define KRYON_FRAME_THREADS'
        for symbol in BeginDrawing EndDrawing ClearBackground BeginMode2D EndMode2D BeginScissorMode EndScissorMode SetTargetFPS GetFrameTime CloseWindow; do
            wraps="$wraps -Wl,--wrap=$symbol"
        done
    fi
    # Reuse the test's disposable output for each backend configuration.
    # shellcheck disable=SC2086
    "$ziran" build --target=c "$@" $defines \
        -o "$work/host-c" "$repo/tests/raylib_frame_snapshot_test.zi"
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$work/host-c" \
        "$work/host-c"/*.c "$archive" "$webp" "${AVIF_A:-$repo/build/avif-ziran/avif/libavif.a}" "${AOM_A:-$repo/build/avif-ziran/aom/libaom.a}" "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" \
        -Wl,--gc-sections -Wl,--wrap=GetTouchPointCount -Wl,--wrap=GetTouchPosition \
        -Wl,--wrap=GetMouseX -Wl,--wrap=GetMouseY $wraps \
        $(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm -o "$work/host-c/test"
    (cd "$work" && timeout --kill-after=2s 45s xvfb-run -a -n 370 -s '-screen 0 640x480x24' ./host-c/test)
    printf '%s\n' "Frame snapshots: $mode touch bounds/coordinates/ownership and source/saved hosted VM boundary passed"
done
