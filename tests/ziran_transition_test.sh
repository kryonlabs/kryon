#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/zi/transition.zi" "$work/transition.zi"
cat > "$work/use_transition.zi" <<'EOF'
#module "use_transition"
#import "transition"

Answer :: () -> i32 #export {
    if TransitionClampProgress(-2.0) != 0.0 { return 0 }
    if TransitionClampProgress(2.0) != 1.0 { return 0 }
    if TransitionSmoothProgress(0.5) != 0.5 { return 0 }
    if TransitionDuration(-1.0) <= 0.0 { return 0 }
    if TransitionDelta(-1.0) != 0.0 { return 0 }
    if TransitionReverseElapsed(1.0, 2.0) != 0.0 { return 0 }
    if TransitionAlpha(false, TransitionOut, 0.5, 1.0) != 0.0 { return 0 }
    if TransitionAlpha(true, TransitionOut, 0.5, 1.0) != 0.5 { return 0 }
    if TransitionAlpha(true, TransitionIn, 0.5, 1.0) != 0.5 { return 0 }
    if TransitionAlpha(true, TransitionNone, 0.5, 1.0) != 0.0 { return 0 }
    if TransitionFadeAlphaByte(true, TransitionOut, 0.5, 1.0) != 127 { return 0 }
    if TransitionApplyAlpha(200, 128) != 100 { return 0 }
    if TransitionApplyAlpha(300, 300) != 255 { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/transition.zi" \
    "$work/use_transition.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/transition.zi" "$work/use_transition.zi"
"$ziran" bundle --root "$work" --entry use_transition:Answer \
    -o "$work/source.zib" "$work/transition.zi" \
    "$work/use_transition.zi"
"$ziran" bundle --root "$work" --entry use_transition:Answer \
    -o "$work/ir.zib" "$work/ir/transition.zir" \
    "$work/ir/use_transition.zir"
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
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --strict --pkg main --root "$work" \
                -o "$output" "$input_dir/transition.$extension" \
                "$input_dir/use_transition.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseTransition_Answer() != 42 { panic("wrong transition result") } }
GO
            GO111MODULE=off go run "$output/transition.go" \
                "$output/use_transition.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --strict --root "$work" \
                -o "$output" "$input_dir/transition.$extension" \
                "$input_dir/use_transition.$extension"
            cat > "$output/main.c" <<'C'
#include "use_transition.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/transition.c" "$output/use_transition.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --strict --root "$work" \
                -o "$output" "$input_dir/transition.$extension" \
                "$input_dir/use_transition.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_transition.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/transition.cpp" "$output/use_transition.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
