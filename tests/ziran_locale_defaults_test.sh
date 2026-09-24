#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'EOF'
#import "locale_defaults"

#program_export
Answer :: () -> s32 {
    if LocaleDefaultCount() != 29 { return 0 }
    first: LocaleDefaultEntry = LocaleDefaultAt(0)
    if first.key != "theme_style_label" || first.value != "Style" { return 0 }
    if LocaleDefaultKeyAt(6) != "theme_style_liquid_glass" { return 0 }
    if LocaleDefaultValueAt(6) != "Liquid Glass" { return 0 }
    if LocaleDefaultKeyAt(14) != "theme_color_label" { return 0 }
    if LocaleDefaultValueAt(28) != "Sweet" { return 0 }
    if LocaleDefaultKeyAt(-1) != "" || LocaleDefaultValueAt(29) != "" { return 0 }
    return 42
}
EOF

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        module_dir=$repo/src/ui
    else
        module=$work/ir/app.zir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --strict --root "$work" \
            --module-path "$module_dir" -o "$output" "$module"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/locale_test.go" <<'GO'
package ziran
import "testing"
func TestLocaleDefaults(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("locale defaults") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
