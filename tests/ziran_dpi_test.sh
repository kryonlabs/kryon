#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/dpi.zi" "$work/dpi.zi"
cat > "$work/use_dpi.zi" <<'EOF'
#import "dpi"

#program_export
Answer :: () -> s32 {
    state: DPIState = InitialDPI()
    state = ResolveDPI(state, 720, 1400,
        cast(DPIPlatform)DPIPlatformDesktop, 0.0, 1.0, 1.0)
    if state.ui_scale != 1.0 || state.layout_width != 720 ||
        state.layout_height != 1400 { return 0 }
    state = InvalidateDPI(state)
    state = ResolveDPI(state, 720, 1400,
        cast(DPIPlatform)DPIPlatformAndroid, 0.0, 1.0, 1.0)
    if state.ui_scale != 2.5 || state.layout_width != 288 ||
        state.layout_height != 560 { return 0 }
    state = InvalidateDPI(state)
    state = ResolveDPI(state, 720, 1400,
        cast(DPIPlatform)DPIPlatformDesktop, 1.75, 1.0, 1.0)
    if state.ui_scale != 1.75 || state.layout_width != 411 ||
        state.layout_height != 800 { return 0 }
    state = OffscreenDPI(state, 320, 560, 2.0,
        cast(DPIPlatform)DPIPlatformWeb, 0.0, 1.0, 1.0)
    if state.render_scale != 2.0 || state.layout_width != 160 ||
        state.layout_height != 280 { return 0 }
    if ScalePixels(5, 1.5) != 8 || ClampPixels(20, 0, 10, 1.5) != 15 {
        return 0
    }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/use_dpi.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/use_dpi.zi"
"$ziran" bundle --root "$work" --entry use_dpi:Answer \
    -o "$work/source.zib" "$work/use_dpi.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry use_dpi:Answer -o "$work/saved.zib" "$work/ir/use_dpi.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
    else
        extension=zir
        input_dir=$work/ir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$input_dir" \
                -o "$output" "$input_dir/use_dpi.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseDpi_Answer() != 42 { panic("wrong DPI result") } }
GO
            GO111MODULE=off go run "$output/dpi.go" \
                "$output/use_dpi.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$input_dir" \
                -o "$output" "$input_dir/use_dpi.$extension"
            cat > "$output/main.c" <<'C'
#include "use_dpi.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/dpi.c" "$output/use_dpi.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$input_dir" \
                -o "$output" "$input_dir/use_dpi.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_dpi.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/dpi.cpp" "$output/use_dpi.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
