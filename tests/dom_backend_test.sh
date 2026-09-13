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
generated=""
for d in "$root"/build/*/generated; do
    [ -f "$d/kryon_null_backend.c" ] && [ -f "$d/include/ui_icon_types.h" ] && generated=$d
done
if [ -z "$generated" ]; then
    echo "dom test: no generated backend dir (run 'make' once first) - skipping" >&2
    exit 0
fi
null_backend="$generated/kryon_null_backend.c"

# Generated runtime C (widgets lowered from current runtime/*.kry such as
# button and dropdown) plus generated non-runtime support sources. Stale
# generated runtime files from deleted compatibility modules must not be linked.
generated_srcs=""
while IFS= read -r file; do
    case "$file" in
        "$generated"/src/runtime/*.c)
            base=${file##*/}
            module=${base%.c}
            [ -f "$root/runtime/$module.kry" ] || continue
            ;;
    esac
    generated_srcs="$generated_srcs $file"
done <<EOF
$(find "$generated/src" -name '*.c' 2>/dev/null | LC_ALL=C sort)
EOF

srcs=$(find "$root/src" -name '*.c' \
    ! -path '*/sync/*' \
    ! -path '*/platform/plan9/*' \
    ! -path '*/scene/physics_world.c' \
    ! -path '*/scene/node_body2d.c' \
    ! -path '*/scene/node_area2d.c' \
    ! -path '*/scene/node_collision_shape2d.c' \
    ! -name 'canvas_*.c' \
    ! -name 'libdraw_*.c' \
    ! -name 'termi_*.c' \
    | sort | tr '\n' ' ')

emcc -I"$root/include" -I"$generated/include" -I"$generated/src" \
    -DKRYON_WITH_PHYSICS=0 -DKRYON_BACKEND_DOM -O1 \
    -sASYNCIFY -sENVIRONMENT=node,web -sINITIAL_MEMORY=64MB \
    -o "$work/dom_smoke.js" "$work/main.c" $srcs "$assets" \
    $generated_srcs "$null_backend"

(cd "$work" && node run.js)
echo "dom backend test ok"
