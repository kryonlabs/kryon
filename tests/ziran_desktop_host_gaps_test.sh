#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/desktop-host-gaps"
runtime="$work/runtime"
mkdir -p "$work" "$runtime"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
unset SESSION_MANAGER DESKTOP_SESSION XDG_SESSION_ID XDG_SESSION_TYPE
unset SDL_WINDOWID WAYLAND_SOCKET
export YUE_DESKTOP_RECOVERY=0
export TMPDIR="$work" XDG_RUNTIME_DIR="$runtime"
export SDL_VIDEODRIVER=x11 SDL_RENDER_DRIVER=software
# The test closes and reopens its window; keep the private server unreset.
xvfb="-noreset -screen 0 640x480x24"
libs="$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0) -lm -lpthread"
LDLIBS="$libs" "$ziran" build --project --root "$repo/tests" \
    --target=c --entry desktop_host_gaps_test:main --exe \
    -o "$work/c" "$repo/tests/desktop_host_gaps_test.zi"
xvfb-run -a -s "$xvfb" "$work/c/desktop_host_gaps_test"
echo "Desktop host gaps: source native descriptors, input, rate, exit key, middle button, options, fullscreen and glyphs passed"

"$ziran" ir --project --root "$repo/tests" \
    -o "$work/ir" "$repo/tests/desktop_host_gaps_test.zi"
LDLIBS="$libs" "$ziran" build --target=c --root "$work/ir" \
    --entry desktop_host_gaps_test:main --exe \
    -o "$work/saved" "$work/ir/desktop_host_gaps_test.zir"
xvfb-run -a -s "$xvfb" "$work/saved/desktop_host_gaps_test"
echo "Desktop host gaps: saved-IR native passed"
