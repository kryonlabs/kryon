#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'EOF'
#import "window_flags"
#import "window_policy"

using NativeWindowFlags;

#program_export
Answer :: () -> s32 {
    point: WindowPoint = WindowInitialPosition(11, 12, 100, 50,
        0, 0, 0, 1920, 1080)
    if point.x != 11 || point.y != 12 { return 0 }
    point = WindowInitialPosition(5, 7, 100, 50,
        NATIVE_WINDOW_TOP_RIGHT, 0, 0, 1920, 1080)
    if point.x != 1815 || point.y != 7 { return 0 }
    point = WindowInitialPosition(0, 0, 200, 100,
        NATIVE_WINDOW_CENTER, 10, 20, 1000, 800)
    if point.x != 410 || point.y != 370 { return 0 }
    point = WindowInitialPosition(0, 100, 1, 1,
        NATIVE_WINDOW_TOP_RIGHT, 2147483647, 2147483647, 1000, 10)
    if point.x != 2147483647 || point.y != 2147483647 { return 0 }
    if WindowDragMoved(3, 0) != 0 { return 0 }
    if WindowDragMoved(3, 1) != 1 { return 0 }
    if WindowDragMoved(4, 0) != 1 { return 0 }
    point = WindowDragPosition(5, 8, -20, -30, 0, 0, 200, 100)
    if point.x != 0 || point.y != 0 { return 0 }
    point = WindowDragPosition(180, 80, 5, 5, 0, 0, 200, 100)
    if point.x != 176 || point.y != 76 { return 0 }
    return 42
}

#program_export
main :: () -> s32 {
    if Answer() != 42 { return 1 }
    return 0
}
EOF

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        module_root=$work
        module_dir=$repo/src/ui
    else
        module=$work/ir/app.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe \
                --entry app:main --root "$module_root" \
                --module-path "$module_dir" \
                --module-path "$ziran_root/std" -o "$output" "$module"
            env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target="$target" --root "$module_root" \
                --module-path "$module_dir" \
                --module-path "$ziran_root/std" -o "$output" "$module"
            if test "$target" = c; then
                "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$output" \
                    "$output"/*.c -o "$output/app"
            else
                "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" \
                    -I"$output" "$output"/*.cpp -o "$output/app"
            fi
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        fi
    done
done
