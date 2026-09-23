#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'EOF'
#module "app"
#import "app_shell_layout"
#import "capability_layout"
#import "geometry"

Answer :: () -> i32 #export {
    spec: AppShellLayoutSpec
    spec.view_width = 360
    spec.view_height = 640
    spec.safe_left = 10
    spec.safe_top = 20
    spec.safe_right = 30
    spec.safe_bottom = 40
    spec.padding = 8
    spec.nav_height = 50
    spec.sidebar_breakpoint = 480
    spec.sidebar_width = 180
    spec.min_content_width = 100
    spec.max_content_width = 400
    shell: AppShellLayout = MeasureAppShell(spec)
    if !shell.compact || shell.sidebar_width != 0 ||
       shell.nav_x != 10 || shell.nav_y != 550 ||
       shell.nav_width != 320 || shell.nav_height != 50 ||
       shell.content_x != 18 || shell.content_y != 28 ||
       shell.content_width != 304 || shell.content_height != 514 { return 0 }

    spec.view_width = 1200
    spec.view_height = 800
    spec.safe_left = 0
    spec.safe_top = 0
    spec.safe_right = 0
    spec.safe_bottom = 0
    spec.padding = 20
    spec.nav_height = 80
    spec.sidebar_width = 200
    spec.max_content_width = 600
    shell = MeasureAppShell(spec)
    if shell.compact || shell.sidebar_width != 200 ||
       shell.content_x != 220 || shell.content_width != 600 ||
       shell.content_height != 680 || shell.nav_y != 720 { return 0 }

    spec.view_width = 10
    spec.view_height = 10
    spec.safe_left = 8
    spec.safe_top = 15
    spec.safe_right = 8
    spec.padding = -4
    spec.nav_height = 20
    spec.sidebar_width = 0
    shell = MeasureAppShell(spec)
    if !shell.compact || shell.nav_y != 15 || shell.nav_width != 0 ||
       shell.content_height != 0 || shell.content_width != 100 { return 0 }

    available: u32 = (u32)FilePicker | (u32)Share
    if !CapabilitiesHas(available, (Capability)Share) ||
       CapabilitiesHas(available, (Capability)Clipboard) { return 0 }
    if CapabilityName((Capability)FilePicker) != "file-picker" ||
       CapabilityName((Capability)0) != "unknown" { return 0 }
    viewport: ViewportSpec
    viewport.width = 100
    viewport.height = 80
    viewport.safe_area.left = 10
    viewport.safe_area.top = 5
    viewport.safe_area.right = 15
    viewport.safe_area.bottom = 10
    viewport.padding = 8
    viewport.reserved_top = 4
    viewport.reserved_bottom = 6
    viewport.min_content_width = 90
    content: Rectangle = SafeContentRect(viewport)
    if content.x != 18.0 || content.y != 17.0 ||
       content.width != 90.0 || content.height != 39.0 { return 0 }
    viewport.height = 10
    content = SafeContentRect(viewport)
    if content.height != 0.0 { return 0 }
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
        input_dir=$work
        module_dir=$repo/src/ui
        extension=zi
    else
        input_dir=$work/ir
        module_dir=$work/ir
        extension=zir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --strict --pkg main --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/app.$extension"
            cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("app shell layout") } }
GO
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --strict --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/app.$extension"
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --strict --root "$work" \
                --module-path "$module_dir" -o "$output" \
                "$input_dir/app.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        fi
    done
done
