#!/bin/sh
# Smooth images through raylib in a private Xvfb window: once its Lanczos
# copy is made from the texture, a picture is drawn from it pixel for
# pixel, the same as Pillow's Image.resize(LANCZOS, box) over the frame.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
. "$repo/tests/image_packages.sh"
work="$repo/build/scratch/raylib-lanczos"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
rm -rf "$work/fixtures"
mkdir -p "$work/fixtures"
# The archives Kryon's raylib projects build (mk/ziran-project.mk).
archive=${RAYLIB_A:-"$repo/build/raylib-ziran/libraylib.a"}
webp=${WEBP_A:-"$repo/build/webp-ziran/libkryonwebp.a"}
test -f "$archive" && test -f "$webp" || { echo "build a raylib project first, or set RAYLIB_A and WEBP_A" >&2; exit 1; }
raylib_libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm}
python3 "$repo/tests/lanczos_goldens.py" fixtures "$work/fixtures"
# Kryon's modules come before std, whose image module has another job.
"$ziran" build --target=c --define POSIX_THREADS --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    --module-path "avif=$image_avif/src" --module-path "$image_avif/src" \
    --module-path "$image_avif/src/bridge" --module-path "webp=$image_webp/src" \
    --module-path "image_codec=$image_codec/src" --module-path "std=$ziran_root/std" \
    --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/raylib_lanczos_test.zi"
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$work/c" "$work/c"/*.c "$archive" "$webp" "${AVIF_A:-$repo/build/avif-ziran/avif/libavif.a}" "${AOM_A:-$repo/build/avif-ziran/aom/libaom.a}" \
    $raylib_libs -o "$work/test"
cd "$repo"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
    -u SESSION_MANAGER -u GDK_DISPLAY YUE_DESKTOP_RECOVERY=0 \
    timeout --kill-after=2s 60s xvfb-run -a -n 302 \
    -s '-screen 0 640x480x24' dbus-run-session -- "$work/test" "$work/fixtures" > "$work/run.out"
cat "$work/run.out"
python3 "$repo/tests/lanczos_goldens.py" compare "$work/fixtures" "$work/run.out"
