#!/bin/sh
# k2js syntax test - verifies the Kir-based .kry->JavaScript pipeline output.
set -eu

k2js=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2js build/*/bin/k2js 2>/dev/null | head -1)}
work=${TMPDIR:-/tmp}/kryon-k2js-syntax-test.$$
root=$(pwd)

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -f "$k2js" ]; then
    echo "k2js not found: $k2js" >&2
    exit 1
fi
if ! command -v node >/dev/null 2>&1; then
    echo "k2js syntax skipped: node not found"
    exit 0
fi

mkdir -p "$work/src" "$work/out" "$work/styles/kryon"

cat > "$work/src/valid.kry" <<'EOF'
#import "kryon.h"
#style <kryon.material> as material
#style "brand.kss" as brand

ANSWER :: #run 21 * 2
#assert ANSWER == 42, "k2js #run assertion failed"
#assert 1 + 1 == 2, "k2js fixture assertion failed"
host_value :: (value: int) -> int #extern "smoke.HostValue"

state {
    count: int = 0
    viewport_width: float = 0
    viewport_height: float = 0
    label: [64] char = "hello"
}

app "JS Smoke" {
    size 320 240
    fps 60
}

PreviewMode :: enum {
    PreviewLight = 1
    PreviewDark = 2
}

ApplyPreviewMode :: (value: int) -> int {
    SetThemeMode((PreviewMode)value)
    return (PreviewMode)(value + 1)
}

FractionalPreviewMode :: (value: float) -> int {
    return (PreviewMode)value
}

call_host :: () -> int {
    return host_value(count)
}
note_input :: (value: string) -> int {
    unused value
    count += 10
    return count
}
note_change :: (value: string) -> int {
    unused value
    count += 100
    return count
}
note_key :: (value: string) -> int {
    unused value
    count += 1000
    return count
}
submit_search :: () -> int {
    count += 10000
    return count
}
focus_search :: () -> int {
    count += 100000
    return count
}
blur_search :: () -> int {
    count += 1000000
    return count
}

DirectAction :: (x: float) -> bool {
    return Button((ButtonProps){.bounds={x, 100, 80, 32}, .label="Action"})
}
StoredAction :: (x: float) -> bool {
    activated: bool = Button((ButtonProps){.bounds={x, 100, 80, 32}, .label="Action"})
    return activated
}
AssignedAction :: (x: float) -> bool {
    activated: bool = false
    activated = Button((ButtonProps){.bounds={x, 100, 80, 32}, .label="Action"})
    return activated
}

PreviewProps :: struct {
    value: i32
}
Preview :: (props: PreviewProps) #ui {
    count += 1000
}

Scene :: (viewport: Rectangle) #ui {
    viewport_width = viewport.width
    viewport_height = viewport.height
    left: int = 10
    widths: [2] int = {100, 20}
    button_bounds: Rectangle = {left, 50 + count, widths[0] + widths[1], 28}
    button_bounds = (Rectangle){left, 50 + count, widths[0] + widths[1], 28}
    Screen root: {
        Text((TextProps){.bounds={Scale(10), Scale(20), 0, 0}, .text="hello", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
        Button tap: {
            bounds = button_bounds
            label = "Tap"
            style = (ControlStyle){.normal = (Style){.fields = StyleRadius, .radius = (float)6}}
            dom = "button"
            dom_id = "tap-button"
            data_tracking_id = "tap-1"
            class = "primary action"
            title = "Tap details"
            tab_index = 3
            role = "button"
            aria_label = "Tap the action"
            aria_description = "Runs the host action"
            aria_controls = "search-field"
            on_click = call_host
        }
        TextField search: {
            bounds = {10, 90, 180, 32}
            text = label
            dom = "input"
            dom_id = "search-field"
            dom_name = "q"
            dom_data_role = "search"
            dom_type = "search"
            readonly = true
            required = true
            class = "field"
            placeholder = "Search terms"
            aria_label = "Search"
            aria_describedby = "tap-button"
            on_input = note_input
            on_change = note_change
            on_key = note_key
            on_submit = submit_search
            on_focus = focus_search
            on_blur = blur_search
        }
        count += 1
    }
}

StyleCopies :: () #ui {
    source: ControlStyle = (ControlStyle){.normal=(Style){
        .fields=StyleFontSize | StyleContentOffset, .font_size=19,
        .content_offset=(Vector2){0, 1}}}
    declared := source
    assigned: ControlStyle = (ControlStyle){}
    assigned = declared
    assigned.normal.font_size = 32
    assigned.normal.content_offset.y = -1
    declared.normal.font_size = 24
    bounds: Rectangle = {10, 20, 30, 40}
    bounds.width = 50
    Button((ButtonProps){.id=1, .label="Source", .style=source, .bounds=bounds})
    Button((ButtonProps){.id=2, .label="Declared", .style=declared})
    Button((ButtonProps){.id=3, .label="Assigned", .style=assigned})
}
EOF

cat > "$work/styles/kryon/material.kss" <<'EOF'
@pack kryon.material;
@layer components;
Button {
  radius: 8;
}
EOF

cat > "$work/src/brand.kss" <<'EOF'
@pack brand;
@layer app;
Button.primary {
  background: #203040;
}
EOF

"$k2js" --root "$work" -o "$work/out" "$work/src/valid.kry"
out="$work/out/src/valid.js"

[ -f "$out" ] || { echo "k2js produced no output" >&2; exit 1; }
cp "$root"/web/*.js "$work/out/"
printf '%s\n' '{"type":"module"}' > "$work/out/package.json"

grep -q 'Code generated by k2js from src/valid.kry' "$out"
grep -q 'import \* as kryon from "../kryon-runtime.js"' "$out"
grep -q 'export function createState()' "$out"
grep -q 'export const moduleState = createState()' "$out"
grep -q 'export function setHost(host)' "$out"
grep -q 'export const app = {' "$out"
grep -q 'title: "JS Smoke"' "$out"
grep -q 'export function Valid_CallHost' "$out"
grep -q 'export function Valid_Scene' "$out"
grep -q 'export function frame' "$out"
grep -q 'export function main' "$out"
grep -q 'kryon.widget(\$rt, "Text"' "$out"
grep -q 'kryon.widget(\$rt, "Button"' "$out"
grep -q '"nodeName": "tap"' "$out"
grep -q '"path": "Scene/root/tap"' "$out"
grep -q '"parentPath": "Scene/root"' "$out"
grep -q '"tag": "button"' "$out"
grep -q '"data": {"tracking-id": "tap-1"}' "$out"
grep -q '"class": "primary action"' "$out"
grep -q '"title": "Tap details"' "$out"
grep -q '"tabIndex": 3' "$out"
grep -q '"onClick": "call_host"' "$out"
grep -q '"ariaDescription": "Runs the host action"' "$out"
grep -q '"ariaControls": "search-field"' "$out"
grep -q '"onInput": "note_input"' "$out"
grep -q '"onChange": "note_change"' "$out"
grep -q '"onKey": "note_key"' "$out"
grep -q '"onSubmit": "submit_search"' "$out"
grep -q '"onFocus": "focus_search"' "$out"
grep -q '"onBlur": "blur_search"' "$out"
grep -q '"domName": "q"' "$out"
grep -q '"data": {"role": "search"}' "$out"
grep -q '"inputType": "search"' "$out"
grep -q '"readOnly": true' "$out"
grep -q '"required": true' "$out"
grep -q '"placeholder": "Search terms"' "$out"
grep -q '"ariaDescribedBy": "tap-button"' "$out"
if grep -q 'kryon.widget(\$rt, "End"' "$out"; then
    echo "k2js emitted a synthetic End widget" >&2
    exit 1
fi
grep -q '\$state.count += 1' "$out"
grep -Eq 'kryon.hostCall\(\$host \|\| moduleHost, "HostValue", \[value_[0-9]+\]\)' "$out"
if grep -q 'TODO k2js' "$out"; then
    echo "k2js left a TODO lowering in JS output:" >&2
    grep 'TODO k2js' "$out" >&2
    exit 1
fi

node "$root/tests/k2js_syntax_test_runner.mjs" "$work/out/src/valid.js" "$work/out/kryon-runtime.js"

cat > "$work/src/anon_refs.kry" <<'EOF'
#import "kryon.h"

pointer_enter :: () -> int {
    return 1
}
pointer_leave :: () -> int {
    return 2
}
pointer_down :: () -> int {
    return 3
}
pointer_up :: () -> int {
    return 4
}

Anon :: () #ui {
    Screen root: {
        Text((TextProps){.text="first"})
        Text((TextProps){.text="second"})
        Link docs: {
            text = "Docs"
            dom_href = "/docs"
            dom_target = "_blank"
            dom_rel = "noopener"
            on_pointer_enter = pointer_enter
            on_pointer_leave = pointer_leave
            on_pointer_down = pointer_down
            on_pointer_up = pointer_up
        }
        Column contact: {
            dom = "form"
            dom_action = "/contact"
            dom_method = "post"
            dom_enctype = "multipart/form-data"
            autocomplete = "off"
            on_reset = pointer_leave
        }
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/anon_refs.kry"
anon_out="$work/out/src/anon_refs.js"
grep -Eq '"path": "Anon/root/Text@[0-9]+"' "$anon_out"
grep -Eq '"path": "Anon/root/Text@[0-9]+-2"' "$anon_out"
grep -q '"href": "/docs"' "$anon_out"
grep -q '"target": "_blank"' "$anon_out"
grep -q '"rel": "noopener"' "$anon_out"
grep -q '"onMouseEnter": "pointer_enter"' "$anon_out"
grep -q '"onMouseLeave": "pointer_leave"' "$anon_out"
grep -q '"onMouseDown": "pointer_down"' "$anon_out"
grep -q '"onMouseUp": "pointer_up"' "$anon_out"
grep -q '"formAction": "/contact"' "$anon_out"
grep -q '"formMethod": "post"' "$anon_out"
grep -q '"formEncType": "multipart/form-data"' "$anon_out"
grep -q '"autoComplete": "off"' "$anon_out"
grep -q '"onReset": "pointer_leave"' "$anon_out"

cat > "$work/src/state_arrays.kry" <<'EOF'
Counter :: struct {
    value: i32
}
state {
    counts: [3] i32
    flags: [2] bool
    wide: [2] i64
    grid: [2] [2] i32
    counters: [2] Counter
}
Read :: () -> i32 {
    return counts[0]
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/state_arrays.kry"
node --input-type=module - "$work/out/src/state_arrays.js" <<'EOF'
import assert from "node:assert/strict";
const module = await import(process.argv[2]);
const state = module.createState();
assert.deepEqual(state.counts, [0, 0, 0]);
assert.deepEqual(state.flags, [false, false]);
assert.deepEqual(state.wide, [0n, 0n]);
assert.deepEqual(state.grid, [[0, 0], [0, 0]]);
assert.deepEqual(state.counters, [{value: 0}, {value: 0}]);
state.grid[0][0] = 7;
state.counters[0].value = 9;
assert.equal(state.grid[1][0], 0);
assert.equal(state.counters[1].value, 0);
assert.equal(module.createState().counters[0].value, 0);
EOF

cat > "$work/src/routes.kry" <<'EOF'
#import "kryon.h"

state {
    visits: int = 0
}

app "Route Smoke" {
    size 480 320
    fps 60
}

route home {
    title "Home"
    group "Pages"
    page Home
}

route about {
    title "About"
    group "Pages"
    page About
}

Home :: (viewport: Rectangle) #ui {
    unused viewport
    visits += 1
    Screen home: {
        Text((TextProps){.bounds={0, 0, 0, 0}, .text="Home", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    }
}

About :: () #ui {
    visits += 100
    Screen about: {
        Text((TextProps){.bounds={0, 0, 0, 0}, .text="About", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    }
}
EOF
"$k2js" --root "$work" -o "$work/out" "$work/src/routes.kry"
node --input-type=module - "$work/out/src/routes.js" "$work/out/kryon-runtime.js" <<'EOF'
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";
const module = await import(pathToFileURL(process.argv[2]).href);
const runtime = await import(pathToFileURL(process.argv[3]).href);

assert.deepEqual(module.app.routes, [
  { id: "home", title: "Home", group: "Pages", page: "Home", path: "/" },
  { id: "about", title: "About", group: "Pages", page: "About", path: "/about" }
]);

const state = module.createState();
const rt = runtime.createRuntime({ app: module.app });
runtime.ReplaceRoute("/");
let snap = module.frame(rt, state);
assert.equal(state.visits, 1);
assert.equal(runtime.webDocumentFrame(rt).nodes[0].path, "Home/home");
assert.equal(snap.frame[1].args.text, "Home");

runtime.ReplaceRoute("/about");
snap = module.frame(rt, state);
assert.equal(state.visits, 101);
assert.equal(runtime.webDocumentFrame(rt).nodes[0].path, "About/about");
assert.equal(snap.frame[1].args.text, "About");

runtime.ReplaceRoute("/missing");
snap = module.frame(rt, state);
assert.equal(state.visits, 102);
assert.equal(runtime.webDocumentFrame(rt).nodes[0].path, "Home/home");
assert.equal(snap.frame[1].args.text, "Home");
EOF

"$k2js" --strict --root "$root" -o "$work/out" \
    runtime/*.kry
node "$root/tests/style_policy_test.mjs" "$work/out/runtime/style.js" \
    "$work/out/runtime/button.js" "$work/out/runtime/theme.js"

"$k2js" --strict --root "$root" -o "$work/out" \
    tests/fixtures/module_calls.kry tests/fixtures/modules/counter.kry
node "$root/tests/module_calls_test.mjs" "$work/out/tests/fixtures/module_calls.js" \
    "$work/out/tests/fixtures/modules/counter.js"

cat > "$work/src/assert_fail.kry" <<'EOF'
#import "kryon.h"
#assert 2 * 2 == 5, "k2js constant assertion failed"
EOF

if "$k2js" --root "$work" -o "$work/out" "$work/src/assert_fail.kry" 2>"$work/assert_fail.err"; then
    echo "false constant #assert did not fail during k2js parsing" >&2
    exit 1
fi
grep -q 'k2js constant assertion failed' "$work/assert_fail.err"

cat > "$work/src/assert_unknown.kry" <<'EOF'
#import "kryon.h"
WEB :: #defined(PLATFORM_WEB)
#assert WEB, "k2js unresolved assertion"
EOF

if "$k2js" --root "$work" -o "$work/out" "$work/src/assert_unknown.kry" 2>"$work/assert_unknown.err"; then
    echo "unresolved #assert did not fail in k2js" >&2
    exit 1
fi
grep -q 'unresolved #assert is not supported by the JS backend' "$work/assert_unknown.err"

echo "k2js syntax ok"
