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

if ! strings "$krb" | grep -q menu_open; then
    echo "cartridge missing menu_open path" >&2
    exit 1
fi
if ! strings "$krb" | grep -q split_open; then
    echo "cartridge missing split_open path" >&2
    exit 1
fi

cat > "$work/click_host.kry" <<'EOF'
#import "kryon.h"

state {
    click_count: int = 0
    last_action: [64] char = "none"
}

app "Click Host" {
    size 240 120
    fps 60
}

ButtonsExample :: (viewport: Rectangle) #ui {
    Screen root: {
        bounds = viewport
        if Button((ButtonProps){.bounds={20,20,120,32}, .label="Primary Button", .id=1}) {
            click_count += 1
            snprintf(last_action,sizeof(last_action),"Primary Button clicked.")
        }
        if Button((ButtonProps){.bounds={20,64,120,32}, .label="Danger Button", .id=2}) {
            click_count += 1
            snprintf(last_action,sizeof(last_action),"Danger Button clicked.")
        }
    }
}
EOF
"$k2b" --root "$work" -o "$work" "$work/click_host.kry"

# A #ui app body should emit a cartridge with stateful controls.
cat > "$work/frame.kry" <<'EOF'
#import "kryon.h"

ANSWER :: #run 6 * 7
#assert ANSWER == 42, "KRB fixture #run assertion failed"

state {
    cb_flag: int = 0
    combo_sel: int = 0
    dd_sel: int = 0
    progress_value: int = 42
    radio_sel: int = 0
    choices: [3] const char * = {"Alpha","Beta","Gamma"}
}

app "Frame" {
    size 100 100
    fps 60
}

App :: () #ui {
    Screen root: {
        Background(GetThemeBackground())
        Background(GetThemeSurface())
        Text((TextProps){.bounds={Scale(4), Scale(4), 0, 0}, .text="hi", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
        Image((ImageProps){"tiles/tile.png", (Rectangle){Scale(8), Scale(20), Scale(16), Scale(16)}, (Rectangle){0,0,0,0}, (Vector2){0,0}, 0.0f, WHITE, IMAGE_FIT_CONTAIN})
        Checkbox((CheckboxProps){.bounds = {Scale(4), Scale(40), Scale(110), Scale(34)}, .id = 1, .label = "Flag", .value = &cb_flag})
        Radio((RadioProps){{Scale(4), Scale(56), Scale(80), Scale(20)}, "Pick", 0, radio_sel == 0, 0})
        Progress((ProgressProps){.bounds = {Scale(30), Scale(42), Scale(60), Scale(10)}, .min = 0, .max = 100, .value = progress_value, .label = "Load"})
        LabelFrame((LabelFrameProps){.bounds = {Scale(28), Scale(56), Scale(64), Scale(20)}, .title = "PanelTitle"})
        Dropdown((DropdownProps){{6, 60, 80, 24}, 2, choices, 3, &combo_sel, 0})
        Dropdown((DropdownProps){.bounds = {Scale(6), Scale(84), Scale(80), Scale(24)}, .id = 3, .options = "x;y", .option_count = 2, .selected_index = &dd_sel})
    }
}
EOF
frame_out=$("$k2b" --no-main --root "$work" -o "$work" "$work/frame.kry" 2>&1)
if [ ! -f "$work/frame.krb" ]; then
    echo "#ui hierarchy did not emit a cartridge" >&2
    exit 1
fi
if echo "$frame_out" | grep -q 'Dropdown'; then
    echo "k2b dropped the Dropdown call: $frame_out" >&2
    exit 1
fi
if echo "$frame_out" | grep -q 'Progress'; then
    echo "k2b dropped the Progress call: $frame_out" >&2
    exit 1
fi
if echo "$frame_out" | grep -q 'Radio'; then
    echo "k2b dropped the Radio call: $frame_out" >&2
    exit 1
fi
if echo "$frame_out" | grep -q 'LabelFrame'; then
    echo "k2b dropped the LabelFrame call: $frame_out" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "Gamma"; then
    echo "frame cartridge missing Dropdown options (string-array state)" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "cb_flag"; then
    echo "frame cartridge missing Checkbox value path" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "Alpha"; then
    echo "frame cartridge missing Dropdown options (string-array state)" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "combo_sel"; then
    echo "frame cartridge missing Dropdown selected-index path" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "progress_value"; then
    echo "frame cartridge missing Progress value path" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "Load"; then
    echo "frame cartridge missing Progress label" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "radio_sel"; then
    echo "frame cartridge missing Radio selected path" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "Pick"; then
    echo "frame cartridge missing Radio label" >&2
    exit 1
fi
if ! strings "$work/frame.krb" | grep -q "PanelTitle"; then
    echo "frame cartridge missing LabelFrame title" >&2
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

cat > "$work/unsupported.kry" <<'EOF'
#import "kryon.h"
Unsupported :: () #ui {
    PanedView((PanedViewProps){0})
}
EOF
if "$k2b" --root "$work" -o "$work" "$work/unsupported.kry" \
        2>"$work/unsupported.err"; then
    echo "unsupported KRB call did not fail by default" >&2
    exit 1
fi
grep -q 'unsupported calls: PanedView x1' "$work/unsupported.err"
"$k2b" --allow-unsupported --root "$work" -o "$work" \
    "$work/unsupported.kry" 2>"$work/unsupported_allowed.err"
test -f "$work/unsupported.krb"

if [ -n "$walker" ] && [ -x "$walker" ]; then
    "$walker" "$krb"
fi

if [ ! -f "$work/click_host.krb.c" ] || [ ! -f "$work/click_host.krb.h" ]; then
    echo "k2b did not write the C host" >&2
    exit 1
fi

host=$work/krb_host_click_test
cc -Wall -Wextra -Werror -DKRYON_KRB_NO_MAIN \
    -I"$work" -I"$root/include" \
    -o "$host" \
    "$root/tests/krb_host_click_test.c" \
    "$work/click_host.krb.c" \
    "$root/src/krb/krb.c" \
    "$root/src/backend/kry_backend.c"
"$host"

echo "krb ok size=$size"
