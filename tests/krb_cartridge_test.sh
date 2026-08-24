#!/bin/sh
set -eu

k2b=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2b build/*/bin/k2b 2>/dev/null | head -1)}
walker=${2:-}
root=${3:-.}
work=${TMPDIR:-/tmp}/kryon-krb-cartridge-test.$$
src=$root/examples/02_buttons.kry
budget=2000000

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

# A #ui app body should emit a cartridge with stateful controls.
cat > "$work/frame.kry" <<'EOF'
#import "kryon.h"

ANSWER :: #run 6 * 7
#assert ANSWER == 42, "KRB fixture #run assertion failed"

state {
    cb_flag: int = 0
    combo_sel: int = 0
    dd_sel: int = 0
    choices: [3] const char * = {"Alpha","Beta","Gamma"}
}

app "Frame" {
    size 100 100
    fps 60
}

App :: () #ui {
    Screen root: {
        ClearBackground(GetThemeBackground())
        Background(GetThemeSurface())
        Text("hi", ScaleUIPx(4), ScaleUIPx(4), Text16, GetThemeText())
        Picture((PictureProps){"tiles/tile.png", (Rectangle){ScaleUIPx(8), ScaleUIPx(20), ScaleUIPx(16), ScaleUIPx(16)}, (Rectangle){0,0,0,0}, (Vector2){0,0}, 0.0f, WHITE, PICTURE_FIT_CONTAIN})
        Checkbox(1, ScaleUIPx(4), ScaleUIPx(40), "Flag", &cb_flag)
        Combobox((ComboboxProps){{6, 60, 80, 24}, 2, choices, 3, &combo_sel, 0})
        Dropdown(3, ScaleUIPx(6), ScaleUIPx(84), ScaleUIPx(80), ScaleUIPx(24), "x;y", &dd_sel)
    }
}
EOF
frame_out=$("$k2b" --no-main --root "$work" -o "$work" "$work/frame.kry" 2>&1)
if [ ! -f "$work/frame.krb" ]; then
    echo "#ui hierarchy did not emit a cartridge" >&2
    exit 1
fi
if echo "$frame_out" | grep -q 'Combobox'; then
    echo "k2b dropped the Combobox call: $frame_out" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "Gamma"; then
    echo "frame cartridge missing Combobox options (string-array state)" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "cb_flag"; then
    echo "frame cartridge missing Checkbox value path" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "Alpha"; then
    echo "frame cartridge missing Combobox options (string-array state)" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "combo_sel"; then
    echo "frame cartridge missing Combobox selected-index path" >&2
    exit 1
fi

cat > "$work/assert_unknown.kry" <<'EOF'
#import "kryon.h"
WEB :: #defined(PLATFORM_WEB)
#assert WEB, "KRB unresolved assertion"

BadAssert :: (viewport: Rectangle) #ui {
    Screen root: {
        bounds = viewport
        Background(GetThemeBackground())
    }
}
EOF

if "$k2b" --root "$work" -o "$work" "$work/assert_unknown.kry" 2>"$work/assert_unknown.err"; then
    echo "unresolved #assert did not fail in k2b" >&2
    exit 1
fi
grep -q 'unresolved #assert is not supported by KRB' "$work/assert_unknown.err"

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
