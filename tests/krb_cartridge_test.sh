#!/bin/sh
set -eu

k2b=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2b build/*/bin/k2b 2>/dev/null | head -1)}
walker=${2:-}
root=${3:-.}
work=${TMPDIR:-/tmp}/kryon-krb-cartridge-test.$$
src=$root/examples/02_buttons.kry
budget=1024

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

if [ ! -f "$k2b" ]; then
    echo "k2b not found: $k2b" >&2
    exit 1
fi
if [ ! -f "$src" ]; then
    echo "example not found: $src" >&2
    exit 1
fi

mkdir -p "$work"
"$k2b" --root "$root/examples" -o "$work" "$src"

krb=$work/02_buttons.krb
if [ ! -f "$krb" ]; then
    echo "k2b did not write $krb" >&2
    exit 1
fi

size=$(wc -c < "$krb")
if [ "$size" -gt "$budget" ]; then
    echo "cartridge too large: $size bytes (budget $budget)" >&2
    exit 1
fi

magic=$(od -An -N4 -t x1 "$krb" | tr -d ' \n')
if [ "$magic" != "4b524200" ]; then
    echo "bad magic: $magic (want 4b524200 KRB\\0)" >&2
    exit 1
fi

if ! strings "$krb" | grep -q click_count; then
    echo "cartridge missing click_count path" >&2
    exit 1
fi
if ! strings "$krb" | grep -q last_action; then
    echo "cartridge missing last_action path" >&2
    exit 1
fi

# A hook-driven app: 'frame main {}' is a top-level function definition that
# the generated main() calls each loop. k2b must parse it (not reject it as an
# unknown top-level statement) and emit a cartridge from its body.
cat > "$work/frame.kry" <<'EOF'
#import "kryon.h"

state {
    cb_flag: int = 0
}

app "Frame" {
    size 100 100
    fps 60
    frame main
}

frame main {
    BeginDrawing()
    ClearBackground(GetThemeBackground())
    BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale())
    Background(GetThemeSurface())
    Text("hi", ScaleUIPx(4), ScaleUIPx(4), UI_TEXT_16, GetThemeText())
    Picture((PictureProps){"tiles/tile.png", (Rectangle){ScaleUIPx(8), ScaleUIPx(20), ScaleUIPx(16), ScaleUIPx(16)}, (Rectangle){0,0,0,0}, (Vector2){0,0}, 0.0f, WHITE, UI_PICTURE_FIT_CONTAIN})
    Checkbox(1, ScaleUIPx(4), ScaleUIPx(40), "Flag", &cb_flag)
    EndUIFocus()
    EndDrawing()
}
EOF
"$k2b" --no-main --root "$work" -o "$work" "$work/frame.kry"
if [ ! -f "$work/frame.krb" ]; then
    echo "frame main {} did not emit a cartridge" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "tiles/tile.png"; then
    echo "frame cartridge missing Picture asset path" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "cb_flag"; then
    echo "frame cartridge missing Checkbox value path" >&2
    exit 1
fi

if [ -n "$walker" ] && [ -x "$walker" ]; then
    "$walker" "$krb"
fi

if [ ! -f "$work/02_buttons.krb.c" ] || [ ! -f "$work/02_buttons.krb.h" ]; then
    echo "k2b did not write the C host" >&2
    exit 1
fi

host=$work/krb_host_click_test
cc -Wall -Wextra -Werror -DKRYON_KRB_NO_MAIN \
    -I"$work" -I"$root/include" \
    -o "$host" \
    "$root/tests/krb_host_click_test.c" \
    "$work/02_buttons.krb.c" \
    "$root/src/krb/krb.c" \
    "$root/src/backend/kry_backend.c"
"$host"

echo "krb ok size=$size"
