#!/bin/sh
# k2g syntax test — verifies the Kir-based .kry->Go pipeline output.
set -eu

k2g=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2g build/*/bin/k2g 2>/dev/null | head -1)}
work=${TMPDIR:-/tmp}/kryon-k2g-syntax-test.$$
root=$(pwd)

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -f "$k2g" ]; then
    echo "k2g not found: $k2g" >&2
    exit 1
fi

mkdir -p "$work/src" "$work/out"

cat > "$work/src/valid.kry" <<'EOF'
#import "kryon.h"

state {
    scroll_off: int = 0
}

app "Smoke" {
    size 320 240
    fps 60
    frame main
}

frame main {
    BeginDrawing()
    ClearBackground(GetThemeBackground())
    Text("hello", ScaleUIPx(10), ScaleUIPx(20), UI_TEXT_16, GetThemeText())
    Scroll(ScaleUIPx(4), ScaleUIPx(8), ScaleUIPx(200), ScaleUIPx(100), ScaleUIPx(400), &scroll_off)
    DrawCircleV((Vector2){ScaleUIPx(120), ScaleUIPx(120)}, ScaleUIPx(30), (Color){0x2d, 0x4d, 0x7b, 0xff})
    DrawRing((Vector2){ScaleUIPx(120), ScaleUIPx(120)}, ScaleUIPx(36), ScaleUIPx(40), 0.0f, 360.0f, 0, (Color){0x70, 0x90, 0xc0, 0xff})
    EndScroll()
    EndDrawing()
}
EOF

"$k2g" --root "$work" -o "$work/out" "$work/src/valid.kry"
out=$(find "$work/out" -name "*.go" | head -1)

[ -f "$out" ] || { echo "k2g produced no output" >&2; exit 1; }

# Structural assertions: the declarative subset must translate fully.
grep -q 'package krygen' "$out"
grep -q 'ScrollOff int32' "$out"
grep -q 'func main()' "$out"
grep -q 'rt.BeginDrawing()' "$out"
grep -q '&st.ScrollOff' "$out"
grep -q 'kryruntime.NewVector2(float32(rt.ScaleUIPx(120)), float32(rt.ScaleUIPx(120)))' "$out"
grep -q 'kryruntime.Color{R: 0x2d, G: 0x4d, B: 0x7b, A: 0xff}' "$out"
grep -q '0.0, 360.0' "$out"   # C float suffixes stripped
if grep -q '0\.0f' "$out"; then
    echo "k2g left a C float suffix in Go output" >&2
    exit 1
fi

# The generated source must compile against Kryon's real Go runtime. Textual
# greps alone previously allowed syntactically invalid Go to pass unnoticed.
cat > "$work/out/go.mod" <<EOF
module kryon-generated-smoke

go 1.25.0

require github.com/waozixyz/kryon/go/kryui v0.0.0
replace github.com/waozixyz/kryon/go/kryui => $root/go/kryui
EOF
(cd "$work/out" && GOCACHE="${GOCACHE:-$work/go-cache}" go test ./...)

echo "k2g syntax ok"
