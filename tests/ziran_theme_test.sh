#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/theme.zi" "$work/theme.zi"
cat > "$work/use_theme.zi" <<'EOF'
#import "theme"

using ThemePolicy;

#program_export
Answer :: () -> s32 {
    if ResolveDark(cast(ThemePolicy)ThemePolicyLight, true) { return 0 }
    if !ResolveDark(cast(ThemePolicy)ThemePolicySystem, true) { return 0 }
    light: Palette = DefaultPalette(false)
    dark: Palette = DefaultPalette(true)
    if light.background != PackedColor(cast(u32)0xF8FBFF, cast(u32)255) { return 0 }
    if dark.background != PackedColor(cast(u32)0x001D38, cast(u32)255) { return 0 }
    if light.focus == dark.focus { return 0 }
    if OnColor(cast(u32)0x000000FF) != cast(u32)0xFFFFFFFF { return 0 }
    if OnColor(cast(u32)0xFFFFFFFF) != cast(u32)0x1D1B20FF { return 0 }
    scheme: Scheme = SchemeFor(light.background, light.surface,
        light.text, light.accent, light.info, false, cast(u8)96)
    if scheme.disabled_content != ((light.text & ~cast(u32)255) | cast(u32)96) {
        return 0
    }
    metrics: Metrics = DefaultMetrics()
    if metrics.control_height_medium != 40.0 || metrics.disabled_opacity <= 0.0 {
        return 0
    }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/theme.zi" "$work/use_theme.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/theme.zi" "$work/use_theme.zi"
"$ziran" bundle --root "$work" --entry use_theme:Answer \
    -o "$work/source.zib" "$work/theme.zi" "$work/use_theme.zi"
"$ziran" bundle --root "$work" --entry use_theme:Answer \
    -o "$work/ir.zib" "$work/ir/theme.zir" "$work/ir/use_theme.zir"
cmp "$work/source.zib" "$work/ir.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/ir.zib")" = 42

for input in source ir; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
    else
        extension=zir
        input_dir=$work/ir
    fi
    "$ziran" build --target=c --root "$work" -o "$work/c-$input" \
        "$input_dir/theme.$extension" "$input_dir/use_theme.$extension"
    cat > "$work/c-$input/main.c" <<'C'
#include "use_theme.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    ${CC:-cc} -I"$ziran_include" -I"$work/c-$input" \
        "$work/c-$input/theme.c" "$work/c-$input/use_theme.c" \
        "$work/c-$input/main.c" -o "$work/c-$input/app"
    "$work/c-$input/app"

    "$ziran" build --target=cpp --root "$work" -o "$work/cpp-$input" \
        "$input_dir/theme.$extension" "$input_dir/use_theme.$extension"
    cat > "$work/cpp-$input/main.cpp" <<'CPP'
#include "use_theme.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    ${CXX:-c++} -I"$ziran_include" -I"$work/cpp-$input" \
        "$work/cpp-$input/theme.cpp" "$work/cpp-$input/use_theme.cpp" \
        "$work/cpp-$input/main.cpp" -o "$work/cpp-$input/app"
    "$work/cpp-$input/app"

    "$ziran" build --target=go --pkg main --root "$work" \
        -o "$work/go-$input" \
        "$input_dir/theme.$extension" "$input_dir/use_theme.$extension"
    if rg -q 'github.com/waozixyz/kryon' "$work/go-$input"; then
        echo 'theme imported the old Kryon Go runtime' >&2
        exit 1
    fi
    cat > "$work/go-$input/main.go" <<'GO'
package main
func main() { if UseTheme_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$work/go-$input/theme.go" \
        "$work/go-$input/use_theme.go" "$work/go-$input/main.go"
done
