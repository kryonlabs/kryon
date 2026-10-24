#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

cp "$repo/tests/widget_test_session.zi" "$work/"

cat > "$work/app.zi" <<'ZI'
#import "session"
#import "widget_test_session"

#import "geometry"
#import "router"
#import "router_props"
#import "tree"
#import "widget_kind"

using WidgetKind;

Matches :: (bytes: []u8, length: s32, text: string) -> bool {
    if length != cast(s32)text.count { return false }
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
    initial_hash: RouterResult = Router(TestSession(), props, state, routes[:],
        "#/docs/api", "/app", 1)
    if initial_hash.route != 2 || initial_hash.changed ||
        initial_hash.write_url { return -11 }
    TreeStart(TestSession(), cast(u64)7, bounds)
    result: RouterResult = Router(TestSession(), props, state, routes[:], "", "/app", 1)
    if !TreeFinish(TestSession()) || !result.state.initialized || result.route != 1 ||
        result.route_index != 0 || result.changed || !result.write_url ||
        result.push_url || result.node != 1 ||
        TreeNodeAt(TestSession(), 1).kind != WidgetKindRouter { return -2 }
    url: [64]u8
    used: s32 = RouterFormatUrl(result, url[:])
    if !Matches(url[:], used, "/app#/home") ||
        RouterFormatUrl(result, url[:3]) != -1 { return -3 }

    state = RouterAcknowledgeVersion(result.state, 2)
    state = RouterNavigate(state, 2)
    TreeStart(TestSession(), cast(u64)7, bounds)
    result = Router(TestSession(), props, state, routes[:], "#/home", "/app", 2)
    if !TreeFinish(TestSession()) || result.route != 2 ||
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
    result = Router(TestSession(), props, state, routes[:], "#/docs/api?tab=1", "/app", 4)
    if !result.changed || result.route != 2 || result.push_url ||
        result.state.route_version != 4 { return -8 }
    state = RouterNavigate(result.state, 999)
    result = Router(TestSession(), props, state, routes[:], "#/docs/api", "/app", 4)
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

answer_go_file=router_test.go
cat > "$work/answer_test.go" <<'GO'
package ziran
import "testing"
func TestRouter(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("router") }
}
GO
. "$repo/tests/answer_matrix.sh"
