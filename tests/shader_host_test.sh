#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/shader-host"
mkdir -p "$out"
python3 "$repo/tests/image_pixels_fixtures.py"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
libs="$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -ldl -lm -lpthread"
"$ziran" build --project --module-path "$repo/tests" --root "$repo/tests" --target=c --define POSIX_THREADS --define SHADER_CAIRO --no-main -o "$out/cairo" "$repo/tests/shader_host_test.zi"
"${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections -I"$ziran_root/include" -iquote "$out/cairo" "$out/cairo"/*.c $libs -o "$out/cairo/shader_host_test"
LIBGL_ALWAYS_SOFTWARE=1 timeout 45s xvfb-run -a -s '-screen 0 640x480x24 -nolisten tcp' "$out/cairo/shader_host_test"
printf 'Real private-Xvfb Cairo shader image: clip, alpha, uniforms, samplers and context recreation passed\n'
