#!/bin/sh
# Runs on Xvfb's software GL, so the comparison needs no GPU or real display.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-gpu"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SDL_RENDER_DRIVER SDL_FRAMEBUFFER_ACCELERATION
export YUE_DESKTOP_RECOVERY=0 SDL_VIDEODRIVER=x11
LDLIBS='-lSDL2' "$ziran" build --project --root "$repo/tests" --target=c --entry pixmap_gpu_test:main --exe \
    -o "$work/c" "$repo/tests/pixmap_gpu_test.zi"
KRYON_PIXMAP_GPU=1 xvfb-run -a "$work/c/pixmap_gpu_test"
# Software GL stays on the raster unless forced; the host keeps uploading pixels.
if xvfb-run -a "$work/c/pixmap_gpu_test" > "$work/unforced.log"; then echo "software GL chose GPU painting" >&2; exit 1; fi
grep -q "GPU recording unavailable" "$work/unforced.log"
if KRYON_PIXMAP_GPU=0 xvfb-run -a "$work/c/pixmap_gpu_test" > "$work/off.log"; then echo "KRYON_PIXMAP_GPU=0 was ignored" >&2; exit 1; fi
grep -q "GPU recording unavailable" "$work/off.log"
