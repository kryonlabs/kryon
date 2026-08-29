#!/bin/sh
# DOM backend test - builds a Kryon app with KRYON_BACKEND=dom under emcc
# and runs it in node against a tiny DOM shim.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/kryon-dom-test.$$
cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if ! command -v emcc >/dev/null 2>&1; then
    if [ -x "$HOME/emsdk/upstream/emscripten/emcc" ]; then
        PATH="$HOME/emsdk/upstream/emscripten:$PATH"
        export PATH
    fi
fi
if ! command -v emcc >/dev/null 2>&1; then
    echo "dom test: emcc not found (source ~/emsdk/emsdk_env.sh) - skipping" >&2
    exit 0
fi
if ! command -v node >/dev/null 2>&1; then
    echo "dom test: node not found - skipping" >&2
    exit 0
fi

mkdir -p "$work"
cp "$root/tests/dom_smoke_main.c" "$work/main.c"
cp "$root/tests/dom_smoke_run.js" "$work/run.js"

assets=""
for f in "$root"/build/*/embedded_asset_data.c; do
    [ -f "$f" ] && assets=$f
done
if [ -z "$assets" ]; then
    echo "dom test: no embedded_asset_data.c (run 'make' once first) - skipping" >&2
    exit 0
fi
null_backend="$root/build/generated/kryon_null_backend.c"
if [ ! -f "$null_backend" ]; then
    echo "dom test: no generated null backend (run 'make' once first) - skipping" >&2
    exit 0
fi

srcs=$(find "$root/src" -name '*.c' \
    ! -path '*/ksync/*' \
    ! -path '*/platform/plan9/*' \
    ! -path '*/scene/physics_world.c' \
    ! -path '*/scene/node_body2d.c' \
    ! -path '*/scene/node_area2d.c' \
    ! -path '*/scene/node_collision_shape2d.c' \
    ! -name 'canvas_*.c' \
    ! -name 'libdraw_*.c' \
    ! -name 'termi_*.c' \
    | sort | tr '\n' ' ')

emcc -I"$root/include" -DKRYON_WITH_PHYSICS=0 -DKRYON_BACKEND_DOM -O1 \
    -sASYNCIFY -sENVIRONMENT=node,web -sINITIAL_MEMORY=64MB \
    -o "$work/dom_smoke.js" "$work/main.c" $srcs "$assets" \
    "$null_backend"

(cd "$work" && node run.js)
echo "dom backend test ok"
