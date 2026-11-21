#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/desktop-pointer"
mkdir -p "$work/runtime"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
unset SESSION_MANAGER DESKTOP_SESSION XDG_SESSION_ID XDG_SESSION_TYPE
unset SDL_WINDOWID WAYLAND_SOCKET
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
export TMPDIR="$work" XDG_RUNTIME_DIR="$work/runtime"
export SDL_VIDEODRIVER=x11 SDL_RENDER_DRIVER=software
libs="$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0) -lX11 -lXtst -lm -lpthread"
source="$repo/tests/desktop_pointer_test.zi"
LDLIBS="$libs" "$ziran" build --project --root "$repo/tests" \
    --target=c --entry desktop_pointer_test:main --exe -o "$work/c" "$source"
xvfb-run -a -s "-noreset -screen 0 640x480x24" "$work/c/desktop_pointer_test"
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
LDLIBS="$libs" "$ziran" build --target=c --root "$work/ir" \
    --entry desktop_pointer_test:main --exe -o "$work/saved" "$work/ir/desktop_pointer_test.zir"
xvfb-run -a -s "-noreset -screen 0 640x480x24" "$work/saved/desktop_pointer_test"
echo "Desktop pointer: source and saved-IR native grab and click local tests passed"
