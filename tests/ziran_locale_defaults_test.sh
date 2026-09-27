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
    index: s32 = 0
    while index < LocaleDefaultCount() {
        entry: LocaleDefaultEntry = LocaleDefaultAt(index)
        if LocaleDefaultKeyAt(index) != entry.key ||
            LocaleDefaultValueAt(index) != entry.value { return 0 }
        index += 1
    }
    return 42
}

#program_export
main :: () -> s32 {
    if Answer() != 42 { return 1 }
    return 0
}
EOF

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
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
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe --entry app:main \
                --root "$work" --module-path "$module_dir" \
                --module-path "$repo/../ziran/std" -o "$output" "$module"
            env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target="$target" --root "$work" \
                --module-path "$module_dir" \
                --module-path "$repo/../ziran/std" -o "$output" "$module"
        fi
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        elif test "$target" = cpp; then
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        fi
    done
done
