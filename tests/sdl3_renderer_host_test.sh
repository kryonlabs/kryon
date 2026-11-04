#!/bin/sh
# Real SDL >= 3.4 GPU-renderer integration, never the owner's display.
# Normally invoked by make sdl3-test, which prepares the SDK.
# KRYON_SDL3_ARTIFACT names the verified native-sdl3 package artifact directory.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
. "$repo/tests/toolchain.sh"
git -C "$repo" check-ignore -q work/sdl3-renderer
work="$repo/work/sdl3-renderer"
output="$repo/build/scratch/sdl3-renderer-host"
mkdir -p "$work" "$output"
exec 9>"$work/lock"
flock 9
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
unset SDL_RENDER_DRIVER SDL_GPU_DRIVER SDL_VIDEO_DRIVER SDL_VIDEODRIVER
export YUE_DESKTOP_RECOVERY=0 SDL_VIDEO_DRIVER=x11
export MAKEFLAGS=-j1 CMAKE_BUILD_PARALLEL_LEVEL=1 ZIRAN_PAR_THREADS=1
export TMPDIR="$work"
artifact=${KRYON_SDL3_ARTIFACT:-}
if test -z "$artifact" || ! test -f "$artifact/lib/pkgconfig/sdl3.pc"; then
    echo 'SDL3 SDK unavailable: set KRYON_SDL3_ARTIFACT to the native-sdl3 package artifact directory.' >&2
    exit 2
fi
pc="$artifact/lib/pkgconfig/sdl3.pc"
PYTHONPATH="$ziran_root/scripts${PYTHONPATH:+:$PYTHONPATH}" python3 - "$artifact" "$work/sdk" <<'PY'
from pathlib import Path
import sys
import tarfile
from ziran_artifacts import Artifact
artifact = Artifact(Path(sys.argv[1]).resolve() / 'artifact.tsv',
                    artifact='native-sdl3', platform='linux-x86_64-gnu')
sdk = Path(sys.argv[2])
sdk.mkdir(exist_ok=True)
with tarfile.open(artifact.output('resources/SDL3-headers.tar')) as archive:
    archive.extractall(sdk, filter='data')
print('Verified pinned native-sdl3 artifact and complete SDK header archive')
PY
pkg-config --atleast-version=3.4.0 "$pc"
export CFLAGS="${CFLAGS:--O2} -I$work/sdk"
export LDLIBS="$(pkg-config --static --libs "$pc")"
source="$repo/tests/sdl3_renderer_host_test.zi"
set -- --bind raster:RasterLine=pixmap:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=pixmap:RasterText \
    --bind raster_text:RasterTextClipped=pixmap:RasterTextClipped \
    --bind raster_text:RasterTextRotated=pixmap:RasterTextRotated \
    --bind raster_triangles:RasterTriangles=pixmap:RasterTriangles
"$ziran" ir --project --root "$repo/tests" --module-path "$repo/tests" \
    -o "$output/ir" "$source"
for form in source saved; do
    input=$source
    if test "$form" = source; then
        "$ziran" build --project --root "$repo/tests" --module-path "$repo/tests" \
            --target=c --entry sdl3_renderer_host_test:main "$@" --exe -o "$output/c" "$input"
    else
        input="$output/ir/sdl3_renderer_host_test.zir"
        "$ziran" build --root "$output/ir" --module-path "$output/ir" \
            --target=c --entry sdl3_renderer_host_test:main "$@" --exe -o "$output/c" "$input"
    fi
    # An inherited/software render preference must not change the explicit gpu
    # request. The selected GPU implementation may still be software: metadata
    # is evidence, renderer/API names alone are not a hardware classification.
    log="$work/$form.log"
    if env SDL_RENDER_DRIVER=software xvfb-run -a -s '-screen 0 800x600x24 -nolisten tcp' \
        "$output/c/sdl3_renderer_host_test" >"$log" 2>&1; then
        sed -n '1,90p' "$log"
    else
        status=$?; sed -n '1,90p' "$log"; exit "$status"
    fi
    # Invalid GPU API must fail; SDL's software renderer remains available but
    # must never be used as a fallback by CreatePixmapSdl3Renderer.
    log="$work/$form-failure.log"
    if env SDL_GPU_DRIVER=kryon-intentionally-missing SDL_RENDER_DRIVER=software \
        KRYON_SDL3_EXPECT_FAILURE=1 xvfb-run -a -s '-screen 0 800x600x24 -nolisten tcp' \
        "$output/c/sdl3_renderer_host_test" >"$log" 2>&1; then
        sed -n '1,30p' "$log"
    else
        status=$?; sed -n '1,90p' "$log"; exit "$status"
    fi
    echo "SDL3 renderer host: $form real private-display local tests passed"
done
