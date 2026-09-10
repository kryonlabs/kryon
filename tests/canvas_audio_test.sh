#!/bin/sh
# Canvas audio smoke test: compile the WebAudio backend to wasm and run it in
# node with a fake AudioContext. Requires emcc + node.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/kryon-canvas-audio-test.$$
cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if ! command -v emcc >/dev/null 2>&1; then
    if [ -x "$HOME/emsdk/upstream/emscripten/emcc" ]; then
        PATH="$HOME/emsdk/upstream/emscripten:$PATH"
        export PATH
    fi
fi
if ! command -v emcc >/dev/null 2>&1; then
    echo "canvas audio test: emcc not found (source ~/emsdk/emsdk_env.sh) - skipping" >&2
    exit 0
fi
if ! command -v node >/dev/null 2>&1; then
    echo "canvas audio test: node not found - skipping" >&2
    exit 0
fi

mkdir -p "$work"
cp "$root/tests/canvas_audio_main.c" "$work/main.c"
cp "$root/tests/canvas_audio_run.js" "$work/pre.js"

# kryon.h includes the generated ui_icon_types.h; pass the generated include
# dirs from the last completed build (same discovery as canvas_backend_test.sh).
generated=""
for d in "$root"/build/*/generated; do
    [ -f "$d/include/ui_icon_types.h" ] && generated=$d
done
if [ -n "$generated" ]; then
    gen_inc="-I$generated/include -I$generated/src"
else
    gen_inc=""
fi

emcc -I"$root/include" $gen_inc -Wall -Wextra -O1 \
    -sASYNCIFY -sENVIRONMENT=node,web -sEXIT_RUNTIME=1 \
    --pre-js "$work/pre.js" \
    -o "$work/canvas_audio_smoke.js" \
    "$work/main.c" \
    "$root/src/backend/canvas_audio.c" \
    "$root/src/backend/canvas_os.c" \
    -lm

(cd "$work" && node canvas_audio_smoke.js)
echo "canvas audio test ok"
