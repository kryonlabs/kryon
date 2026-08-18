#!/bin/sh
# canvas backend test — builds a full kryon app with KRYON_BACKEND=canvas
# under emcc (no raylib) and runs it in node against a recording Canvas2D
# shim, asserting the draw call sequence. Requires emcc + node.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/kryon-canvas-test.$$
cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if ! command -v emcc >/dev/null 2>&1; then
    if [ -x "$HOME/emsdk/upstream/emscripten/emcc" ]; then
        PATH="$HOME/emsdk/upstream/emscripten:$PATH"
        export PATH
    fi
fi
if ! command -v emcc >/dev/null 2>&1; then
    echo "canvas test: emcc not found (source ~/emsdk/emsdk_env.sh) — skipping" >&2
    exit 0
fi
if ! command -v node >/dev/null 2>&1; then
    echo "canvas test: node not found — skipping" >&2
    exit 0
fi

mkdir -p "$work"
cp "$root/tests/canvas_smoke_main.c" "$work/main.c"
cp "$root/tests/canvas_smoke_run.js" "$work/run.js"

assets=""
for f in "$root"/build/*/embedded_asset_data.c; do
    [ -f "$f" ] && assets=$f
done
if [ -z "$assets" ]; then
    echo "canvas test: no embedded_asset_data.c (run 'make' once first) — skipping" >&2
    exit 0
fi

srcs=$(find "$root/src" -name '*.c' ! -path '*/ksync/*' | sort | tr '\n' ' ')

emcc -I"$root/include" -I"$root/vendor/box2d/include" -O1 \
    -sASYNCIFY -sENVIRONMENT=node,web -sINITIAL_MEMORY=64MB \
    -o "$work/canvas_smoke.js" "$work/main.c" $srcs "$assets"

(cd "$work" && node run.js)
echo "canvas backend test ok"
