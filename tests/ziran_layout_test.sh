#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/zi/layout.zi" "$work/layout.zi"
cat > "$work/use_layout.zi" <<'EOF'
#module "use_layout"
#import "layout"
Answer :: () -> i32 #export {
    centered: CenteredColumnLayout = CenteredColumnFor(800, 600, 50)
    compact: CenteredColumnLayout = CenteredColumnFor(200, 500, 20)
    if centered.x != 100 || centered.width != 600 { return 0 }
    if compact.x != 20 || compact.width != 160 { return 0 }
    if PageSidePaddingFor(300) != 12 { return 0 }
    if PageSidePaddingFor(2000) != 24 { return 0 }
    if DesktopWidthThresholdFor(0.0) != 500 { return 0 }
    if DesktopWidthThresholdFor(1.5) != 750 { return 0 }
    if IsDesktopWidth(749, 1.5) { return 0 }
    if !IsDesktopWidth(750, 1.5) { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/layout.zi" "$work/use_layout.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/layout.zi" "$work/use_layout.zi"

for input in source ir; do
    if test "$input" = source; then
        layout="$work/layout.zi"
        use_layout="$work/use_layout.zi"
    else
        layout="$work/ir/layout.zir"
        use_layout="$work/ir/use_layout.zir"
    fi
    "$ziran" build --target=c --strict --root "$work" \
        -o "$work/c-$input" "$layout" "$use_layout"
    cat > "$work/c-$input/main.c" <<'EOF'
#include "use_layout.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
    ${CC:-cc} -I"$ziran_include" -I"$work/c-$input" \
        "$work/c-$input/layout.c" \
        "$work/c-$input/use_layout.c" "$work/c-$input/main.c" \
        -o "$work/c-$input/app"
    "$work/c-$input/app"

    "$ziran" build --target=cpp --strict --root "$work" \
        -o "$work/cpp-$input" "$layout" "$use_layout"
    cat > "$work/cpp-$input/main.cpp" <<'EOF'
#include "use_layout.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
EOF
    ${CXX:-c++} -I"$ziran_include" -I"$work/cpp-$input" \
        "$work/cpp-$input/layout.cpp" \
        "$work/cpp-$input/use_layout.cpp" "$work/cpp-$input/main.cpp" \
        -o "$work/cpp-$input/app"
    "$work/cpp-$input/app"

    "$ziran" build --target=go --strict --pkg main --root "$work" \
        -o "$work/go-$input" "$layout" "$use_layout"
    if grep -Fq 'github.com/waozixyz/kryon/go/kryon' \
        "$work/go-$input/layout.go" "$work/go-$input/use_layout.go"; then
        echo 'ordinary imported layout code pulled in the legacy Go runtime' >&2
        exit 1
    fi
    cat > "$work/go-$input/main.go" <<'EOF'
package main
func main() { if UseLayout_Answer() != 42 { panic("wrong layout result") } }
EOF
    GO111MODULE=off go run "$work/go-$input/layout.go" \
        "$work/go-$input/use_layout.go" "$work/go-$input/main.go"
done
