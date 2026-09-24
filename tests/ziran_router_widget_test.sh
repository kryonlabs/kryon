#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "geometry"
#import "router"
#import "router_props"
#import "tree"
#import "widget_kind"

Matches :: (bytes: []u8, length: s32, text: string) -> bool {
    if length != text.length { return false }
    index: s32 = 0
    while index < length {
        if bytes[index] != text[index] { return false }
        index += 1
    }
    return true
}

#program_export
Answer :: () -> s32 {
    routes: [3]RouterRoute
    routes[0] = RouterRoute.{1, -1, "home", "Home", ""}
    routes[1] = RouterRoute.{2, -1, "##/docs/api", "Docs", ""}
    routes[2] = RouterRoute.{3, -1, "", "Empty", ""}
    bounds: Rectangle = Rectangle.{0.0, 0.0, 200.0, 100.0}
    props: RouterProps
    props.bounds = bounds
    props.key = cast(u64)91
    props.initial_route = 1
    props.sync_url = true
    props.replace_on_init = true
    if RouterFindRoute(routes[:], 2) != 1 ||
        RouterFindRoute(routes[:], 99) != -1 ||
        !RouterHashMatches("  ##/docs/api?tab=1", "##/docs/api") ||
        RouterHashMatches("#/docs/other", "docs/api") ||
        RouterFindHash(routes[:], "#/docs/api&x") != 1 { return -1 }

    state: RouterState
    TreeStart(cast(u64)7, bounds)
    result: RouterResult = Router(props, state, routes[:], "", "/app", 1)
    if !TreeFinish() || !result.state.initialized || result.route != 1 ||
        result.route_index != 0 || result.changed || !result.write_url ||
        result.push_url || result.node != 1 ||
        TreeNodeAt(1).kind != WidgetKindRouter { return -2 }
    url: [64]u8
    used: s32 = RouterFormatUrl(result, url[:])
    if !Matches(url[:], used, "/app#/home") ||
        RouterFormatUrl(result, url[:3]) != -1 { return -3 }

    state = RouterAcknowledgeVersion(result.state, 2)
    state = RouterNavigate(state, 2)
    TreeStart(cast(u64)7, bounds)
    result = Router(props, state, routes[:], "#/home", "/app", 2)
    if !TreeFinish() || result.route != 2 ||
        result.previous_route != 1 || !result.changed ||
        !result.write_url || !result.push_url ||
        result.state.generation != cast(u32)1 ||
        result.state.has_request { return -4 }
    used = RouterFormatUrl(result, url[:])
    if !Matches(url[:], used, "/app#/docs/api") { return -5 }

    result = RouterSetRoute(props, result.state, routes[:], 1,
        false, "", 2)
    if !result.changed || result.route != 1 || result.push_url ||
        !result.write_url || result.state.generation != cast(u32)2 {
        return -6
    }
    used = RouterFormatUrl(result, url[:])
    if !Matches(url[:], used, "/#/home") { return -7 }

    state = RouterAcknowledgeVersion(result.state, 3)
    result = Router(props, state, routes[:], "#/docs/api?tab=1", "/app", 4)
    if !result.changed || result.route != 2 || result.push_url ||
        result.state.route_version != 4 { return -8 }
    state = RouterNavigate(result.state, 999)
    result = Router(props, state, routes[:], "#/docs/api", "/app", 4)
    if result.changed || result.route != 2 || result.state.has_request ||
        result.write_url { return -9 }

    props.sync_url = false
    result = RouterSetRoute(props, result.state, routes[:], 3,
        true, "/app", 4)
    if !result.changed || result.write_url ||
        RouterFormatUrl(result, url[:]) != -1 { return -10 }
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
        "$ziran" build --target="$target" --strict \
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
            cat > "$output/router_test.go" <<'GO'
package ziran
import "testing"
func TestRouter(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("router") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
