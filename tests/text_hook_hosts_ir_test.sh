#!/bin/sh
# Every host that installs a #reads text hook (typeface selector, emphasis or
# italic raster) must check against that promise, including the threaded
# raylib frame path Android uses. IR only, so this runs in seconds.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$repo"
"$ziran" ir --project --define BUNDLE_NATIVE_RENDERER --root tools \
    -o "$work/raylib" tools/zx_raylib.zi
"$ziran" ir --project --define BUNDLE_NATIVE_RENDERER --define KRYON_FRAME_THREADS \
    --root tools -o "$work/raylib-threads" tools/zx_raylib.zi
"$ziran" ir --project --entry android_frame_input_test:main --bind frame_entry:ProjectFrame=android_frame_app:Frame --define ANDROID_BUILD --define __aarch64__ --root tests \
    --module-path tests/android_frame_input --module-path tests \
    -o "$work/android" tests/android_frame_input_test.zi tests/android_frame_input/android_frame_app.zi
"$ziran" ir --project --target-os web --target-cpu wasm32 \
    --define PLATFORM_WEB --define BUNDLE_NATIVE_RENDERER --root tools \
    -o "$work/canvas" tools/zx_canvas.zi src/backend/canvas_raster.zi
printf '%s\n' 'Text hook hosts: raylib, threaded raylib, Android and canvas checked'
