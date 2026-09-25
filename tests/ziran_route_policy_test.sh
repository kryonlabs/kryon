#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "route"

#program_export
Answer :: () -> s32 {
    routes: [6]s32 = .[1, 2, 2, 3, 4, 0]
    view: []s32 = routes[:]
    allowed: [2]s32 = .[2, 4]
    if RouteSanitizeSet(view, 5, allowed[:], -1) != 2 ||
        routes[0] != 2 || routes[1] != 4 ||
        routes[2] != -1 || routes[5] != -1 { return -1 }
    if !RouteMove(view, 2, 0, 1) ||
        routes[0] != 4 || routes[1] != 2 ||
        RouteMove(view, 2, 0, 2) { return -2 }
    candidates: [3]s32 = .[4, 2, 5]
    if RouteFirstUnused(view, 2, candidates[:], 9) != 5 ||
        !RouteContains(view, 2, 2) ||
        RouteContains(view, 2, 3) ||
        RouteCount(view, 99) != 6 { return -3 }

    filtered: [5]s32 = .[1, 2, 2, 3, 4]
    mask: [5]bool = .[false, true, false, true, false]
    if RouteSanitizeMask(filtered[:], 5, mask[:], -1) != 2 ||
        filtered[0] != 2 || filtered[1] != 3 ||
        filtered[4] != -1 { return -4 }

    stack: [3]s32
    step: RouteStackStep = RouteStackInit(stack[:], 10)
    if !step.accepted || step.count != 1 ||
        step.current != 10 { return -5 }
    step = RouteStackPush(stack[:], step.count, 10, 20)
    if !step.accepted || step.count != 2 ||
        step.current != 20 { return -6 }
    step = RouteStackPush(stack[:], step.count, 10, 20)
    if !step.accepted || step.count != 2 { return -7 }
    step = RouteStackPush(stack[:], step.count, 10, 30)
    if !step.accepted || step.count != 3 ||
        step.current != 30 { return -8 }
    step = RouteStackPush(stack[:], step.count, 10, 40)
    if step.accepted || step.count != 3 ||
        step.current != 30 { return -9 }
    step = RouteStackPop(stack[:], step.count, 10)
    if !step.accepted || step.count != 2 ||
        step.current != 20 { return -10 }
    step = RouteStackReset(stack[:], 11)
    if !step.accepted || step.count != 1 ||
        step.current != 11 || stack[0] != 11 { return -11 }
    empty: []s32 = stack[:0]
    step = RouteStackInit(empty, 7)
    if step.accepted || step.count != 0 ||
        step.current != 7 { return -12 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/app.zi
        root=$work
        module_path=$repo/src/ui
    else
        source=$work/ir/app.zir
        root=$work/ir
        module_path=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$module_path" \
        --entry app:Answer -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" \
            --root "$root" --module-path "$module_path" \
            -o "$output" "$source"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/route_test.go" <<'GO'
package ziran
import "testing"
func TestRoutePolicy(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("route policy") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
