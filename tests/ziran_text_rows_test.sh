#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/text_rows.zi" "$work/text_rows.zi"
cat > "$work/use_text_rows.zi" <<'EOF'
#module "use_text_rows"
#import "text_rows"

Answer :: () -> i32 #export {
    text: string = "ab\r\ncd"
    first: TextLogicalLine = TextLogicalLineFor(text, 0)
    if !first.valid || first.start != 0 || first.end != 2 ||
        first.next != 4 { return 0 }
    second: TextLogicalLine = TextLogicalLineFor(text, first.next)
    if !second.valid || second.start != 4 || second.end != 6 { return 0 }
    if TextLogicalLineFor(text, 8).valid { return 0 }
    if TextRowFontFor("# Heading", 0, 9, 16, true) != 24 { return 0 }
    if TextRowFontFor("### Heading", 0, 11, 16, true) != 16 { return 0 }
    if TextRowNextStart(" \tword", 0, 6, true) != 2 { return 0 }
    if !TextRowHasCursor(2, 5, 5, 5) { return 0 }
    if !TextRowAtY(10, 20, 25, false) { return 0 }
    state: TextRowBreak
    state = TextRowAdvance(state, 0, 4, 8, false, 2.0, 10.0, true)
    if state.end != 4 || state.done { return 0 }
    state = TextRowAdvance(state, 0, 8, 8, false, 12.0, 10.0, true)
    if state.end != 4 || !state.done { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/text_rows.zi" \
    "$work/use_text_rows.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/text_rows.zi" "$work/use_text_rows.zi"
"$ziran" bundle --root "$work" --entry use_text_rows:Answer \
    -o "$work/source.zib" "$work/text_rows.zi" \
    "$work/use_text_rows.zi"
"$ziran" bundle --root "$work" --entry use_text_rows:Answer \
    -o "$work/ir.zib" "$work/ir/text_rows.zir" \
    "$work/ir/use_text_rows.zir"
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
                -o "$output" "$input_dir/text_rows.$extension" \
                "$input_dir/use_text_rows.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseTextRows_Answer() != 42 { panic("wrong text row result") } }
GO
            GO111MODULE=off go run "$output/text_rows.go" \
                "$output/use_text_rows.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --strict --root "$work" -o "$output" \
                "$input_dir/text_rows.$extension" \
                "$input_dir/use_text_rows.$extension"
            cat > "$output/main.c" <<'C'
#include "use_text_rows.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/text_rows.c" "$output/use_text_rows.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --strict --root "$work" -o "$output" \
                "$input_dir/text_rows.$extension" \
                "$input_dir/use_text_rows.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_text_rows.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/text_rows.cpp" "$output/use_text_rows.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done
