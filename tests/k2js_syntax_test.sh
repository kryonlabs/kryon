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

mkdir -p "$work/src" "$work/out" "$work/styles/kryon" "$work/styles/acme"

cat > "$work/src/valid.kry" <<'EOF'
#import "kryon.h"
#style <material> as material
#style "brand.kss" as brand
#style <brand> as brand_pack
#style <acme.dark> as acme_dark

ANSWER :: #run 21 * 2
#assert ANSWER == 42, "k2js #run assertion failed"
#assert 1 + 1 == 2, "k2js fixture assertion failed"
host_value :: (value: int) -> int #extern "smoke.HostValue"

state {
    count: int = 0
    viewport_width: float = 0
    viewport_height: float = 0
    label: [64] char = "hello"
    selected: int = 0
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
note_before_input :: (value: string) -> int {
    unused value
    count += 2
    return count
}
note_change :: (value: string) -> int {
    unused value
    count += 100
    return count
}
note_select :: (value: string) -> int {
    unused value
    count += 200
    return count
}
note_key :: (value: string) -> int {
    unused value
    count += 1000
    return count
}
invalid_search :: (value: string) -> int {
    unused value
    count += 10000000
    return count
}
scroll_search :: (value: i32) -> int {
    count += value
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
toggle_search :: () -> int {
    count += 3000000
    return count
}
close_search :: () -> int {
    count += 4000000
    return count
}
cancel_search :: () -> int {
    count += 5000000
    return count
}

DirectAction :: (x: float) -> bool {
    return Button((ButtonProps){.bounds={x, 100, 80, 32}, .label="Action"})
}
StoredAction :: (x: float) -> bool {
    activated: bool = Button((ButtonProps){.bounds={x, 100, 80, 32}, .label="Action"})
    return activated
}
InferredAction :: (x: float) -> bool {
    activated := Button((ButtonProps){.bounds={x, 100, 80, 32}, .label="Action"})
    return activated
}
AssignedAction :: (x: float) -> bool {
    activated: bool = false
    activated = Button((ButtonProps){.bounds={x, 100, 80, 32}, .label="Action"})
    return activated
}
CompoundAction :: (x: float) -> int {
    count = 0
    count += Button((ButtonProps){.bounds={x, 100, 80, 32}, .label="Action"})
    return count
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
    values: [2] float = {0.0f, 1.0f}
    value_changed: bool = false
    button_bounds: Rectangle = {left, 50 + count, widths[0] + widths[1], 28}
    button_bounds = (Rectangle){left, 50 + count, widths[0] + widths[1], 28}
    Screen root: {
        Text((TextProps){.bounds={Scale(10), Scale(20), 0, 0}, .text="hello", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
        Button tap: {
            bounds = button_bounds
            label = "Tap"
            style = (ControlStyle){.normal = (Style){.fields = StyleRadius, .radius = (float)6}}
            dom = "button"
            dom_ref = "primary-action"
            dom_id = "tap-button"
            dom_value = "tap-value"
            data_tracking_id = "tap-1"
            class = "primary action"
            title = "Tap details"
            tab_index = 3
            role = "button"
            aria_label = "Tap the action"
            aria_description = "Runs the host action"
            aria_controls = "search-box"
            aria_owns = "search-box"
            aria_current = "page"
            aria_pressed = false
            popover_target = "Scene/root/search_label"
            popover_target_action = "toggle"
            attr_fetchpriority = "high"
            html_attr_part = "primary-action"
            on_click = call_host
        }
        TextField search: {
            bounds = {10, 90, 180, 32}
            text = label
            dom = "input"
            dom_ref = "search-box"
            dom_id = "search-field"
            dom_name = "q"
            dom_data_role = "search"
            dom_type = "search"
            draggable = "true"
            spellcheck = false
            content_editable = "plaintext-only"
            autofocus = true
            inert = true
            auto_capitalize = "words"
            enter_key_hint = "search"
            form_no_validate = true
            readonly = true
            required = true
            dom_min = 1
            dom_max = 100
            dom_step = 1
            min_length = 2
            max_length = 64
            pattern = "needle.*"
            accept = ".txt"
            multiple = true
            input_mode = "search"
            class = "field"
            placeholder = "Search terms"
            aria_label = "Search"
            aria_labelledby = "Scene/root/search_label"
            aria_activedescendant = "Scene/root/search_label"
            aria_describedby = "primary-action"
            on_input = note_input
            on_before_input = note_before_input
            on_change = note_change
            on_select = note_select
            on_key = note_key
            on_invalid = invalid_search
            on_scroll = scroll_search
            on_submit = submit_search
            on_focus = focus_search
            on_blur = blur_search
        }
        Text search_label: {
            text = "Search"
            dom = "label"
            dom_for = "search-box"
            popover = "manual"
            hidden = true
            on_toggle = toggle_search
            on_close = close_search
            on_cancel = cancel_search
        }
        if Selectable((SelectableProps){.bounds={10, 132, 180, 28}, .id=500, .label="Choice", .selected=&selected}) {
            count += 4
        }
        value_changed = Input((InputProps){.bounds={10, 168, 180, 28}, .id=501, .label="Value", .kind=NumericFloat, .float_values=values, .value_count=2, .step=0.1f, .step_fast=1.0f})
        Input((InputProps){.bounds={10, 204, 180, 28}, .id=502, .label="Standalone", .kind=NumericFloat, .float_values=values, .value_count=1, .step=0.1f, .step_fast=1.0f})
        if value_changed {
            count += 8
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
@pack material;
@layer components;
Button {
  radius: 8;
}
EOF

cat > "$work/src/brand.kss" <<'EOF'
@pack local.brand;
@layer app;
Button.primary {
  background: #203040;
}
EOF

cat > "$work/styles/brand.kss" <<'EOF'
@pack brand;
@layer app;
Button.secondary {
  background: #405060;
}
EOF

cat > "$work/styles/acme/dark.kss" <<'EOF'
@pack acme.dark;
@layer app;
Text {
  foreground: #f4f4f4;
}
EOF

mkdir -p "$work/vendor/theme"
cat > "$work/styles/packages.kssmap" <<'EOF'
# Optional package registry for KSS packs that do not live under styles/.
registered.theme = vendor/theme/main.kss
EOF

cat > "$work/vendor/theme/main.kss" <<'EOF'
@pack registered.theme;
@layer app;
Button {
  foreground: #abcdef;
}
EOF

cat > "$work/src/registry_styles.kry" <<'EOF'
#import "kryon.h"
#style <registered.theme> as registered_theme
app "Registry Styles" {
    size 80 60
}
RegistryScreen :: () #ui {
    Button((ButtonProps){.label="Mapped"})
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
grep -q 'kryon.widget(\$rt, "Selectable"' "$out"
grep -q 'kryon.widget(\$rt, "Input"' "$out"
awk '/kryon.widget\(\$rt, "Text"/ && /"sourcePath": "src\/valid.kry"/ { found=1 } END { exit !found }' "$out"
grep -q '"nodeName": "tap"' "$out"
grep -q '"path": "Scene/root/tap"' "$out"
grep -q '"parentPath": "Scene/root"' "$out"
grep -Eq '"path": "Scene/root/Selectable@[0-9]+(-[0-9]+)?"' "$out"
grep -q '"parentPath": "Scene/root"' "$out"
grep -q '"sourceColumn":' "$out"
grep -q '"tag": "button"' "$out"
grep -q '"domValue": "tap-value"' "$out"
grep -q '"data": {"tracking-id": "tap-1"}' "$out"
grep -q '"class": "primary action"' "$out"
grep -q '"title": "Tap details"' "$out"
grep -q '"tabIndex": 3' "$out"
grep -q '"onClick": "call_host"' "$out"
grep -q '"ariaDescription": "Runs the host action"' "$out"
grep -q '"ariaControls": "search-box"' "$out"
grep -q '"ariaOwns": "search-box"' "$out"
grep -q '"ariaLabelledBy": "Scene/root/search_label"' "$out"
grep -q '"ariaActiveDescendant": "Scene/root/search_label"' "$out"
grep -q '"popoverTarget": "Scene/root/search_label"' "$out"
grep -q '"aria": {"current": "page", "pressed": false}' "$out"
grep -q '"onInput": "note_input"' "$out"
grep -q '"onBeforeInput": "note_before_input"' "$out"
grep -q '"onChange": "note_change"' "$out"
grep -q '"onSelect": "note_select"' "$out"
grep -q '"onKey": "note_key"' "$out"
grep -q '"onInvalid": "invalid_search"' "$out"
grep -q '"onScroll": "scroll_search"' "$out"
grep -q '"onSubmit": "submit_search"' "$out"
grep -q '"onFocus": "focus_search"' "$out"
grep -q '"onBlur": "blur_search"' "$out"
grep -q '"domName": "q"' "$out"
grep -q '"data": {"role": "search"}' "$out"
grep -q '"inputType": "search"' "$out"
grep -q '"draggable": "true"' "$out"
grep -q '"spellCheck": false' "$out"
grep -q '"contentEditable": "plaintext-only"' "$out"
grep -q '"autoFocus": true' "$out"
grep -q '"inert": true' "$out"
grep -q '"autoCapitalize": "words"' "$out"
grep -q '"enterKeyHint": "search"' "$out"
grep -q '"formNoValidate": true' "$out"
grep -q '"readOnly": true' "$out"
grep -q '"required": true' "$out"
grep -q '"min": 1' "$out"
grep -q '"max": 100' "$out"
grep -q '"step": 1' "$out"
grep -q '"minLength": 2' "$out"
grep -q '"maxLength": 64' "$out"
grep -q '"pattern": "needle.*"' "$out"
grep -q '"accept": ".txt"' "$out"
grep -q '"multiple": true' "$out"
grep -q '"inputMode": "search"' "$out"
grep -q '"placeholder": "Search terms"' "$out"
grep -q '"ariaDescribedBy": "primary-action"' "$out"
grep -q '"htmlFor": "search-box"' "$out"
grep -q '"hidden": true' "$out"
if grep -q 'kryon.widget(\$rt, "End"' "$out"; then
    echo "k2js emitted a synthetic End widget" >&2
    exit 1
fi
grep -q '\$state.count += 1' "$out"
grep -Eq 'kryon.hostCall\(\$host \|\| moduleHost, "HostValue", \[(value_[0-9]+|\$state.count)\]\)' "$out"
if grep -q 'TODO k2js' "$out"; then
    echo "k2js left a TODO lowering in JS output:" >&2
    grep 'TODO k2js' "$out" >&2
    exit 1
fi

node "$root/tests/k2js_syntax_test_runner.mjs" "$work/out/src/valid.js" "$work/out/kryon-runtime.js"

"$k2js" --root "$work" -o "$work/out" "$work/src/registry_styles.kry"
registry_out="$work/out/src/registry_styles.js"
grep -q 'target: "registered.theme"' "$registry_out"
grep -q 'alias: "registered_theme"' "$registry_out"
grep -q '@pack registered.theme;' "$registry_out"

cat > "$work/src/anon_refs.kry" <<'EOF'
#import "kryon.h"

pointer_enter :: () -> int {
    return 1
}
pointer_leave :: () -> int {
    return 2
}
pointer_move :: () -> int {
    return 20
}
pointer_down :: () -> int {
    return 3
}
pointer_up :: () -> int {
    return 4
}
pointer_wheel :: (value: i32) -> int {
    return value
}
drag_value :: (value: string) -> int {
    unused value
    return 5
}
drag_marker :: () -> int {
    return 6
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
            download = "docs.html"
            on_pointer_enter = pointer_enter
            on_pointer_leave = pointer_leave
            on_pointer_move = pointer_move
            on_pointer_down = pointer_down
            on_pointer_up = pointer_up
            on_wheel = pointer_wheel
            on_drag_start = drag_value
            on_drag_end = drag_value
            on_drag_over = drag_marker
            on_drop = drag_value
            on_copy = drag_value
            on_cut = drag_value
            on_paste = drag_value
        }
        Column contact: {
            dom = "form"
            dom_action = "/contact"
            dom_method = "post"
            dom_enctype = "multipart/form-data"
            autocomplete = "off"
            no_validate = true
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
grep -q '"download": "docs.html"' "$anon_out"
grep -q '"onMouseEnter": "pointer_enter"' "$anon_out"
grep -q '"onMouseLeave": "pointer_leave"' "$anon_out"
grep -q '"onMouseMove": "pointer_move"' "$anon_out"
grep -q '"onMouseDown": "pointer_down"' "$anon_out"
grep -q '"onMouseUp": "pointer_up"' "$anon_out"
grep -q '"onWheel": "pointer_wheel"' "$anon_out"
grep -q '"onDragStart": "drag_value"' "$anon_out"
grep -q '"onDragEnd": "drag_value"' "$anon_out"
grep -q '"onDragOver": "drag_marker"' "$anon_out"
grep -q '"onDrop": "drag_value"' "$anon_out"
grep -q '"onCopy": "drag_value"' "$anon_out"
grep -q '"onCut": "drag_value"' "$anon_out"
grep -q '"onPaste": "drag_value"' "$anon_out"
grep -q '"formAction": "/contact"' "$anon_out"
grep -q '"formMethod": "post"' "$anon_out"
grep -q '"formEncType": "multipart/form-data"' "$anon_out"
grep -q '"autoComplete": "off"' "$anon_out"
grep -q '"noValidate": true' "$anon_out"
grep -q '"onReset": "pointer_leave"' "$anon_out"

cat > "$work/src/conditional_widgets.kry" <<'EOF'
#import "kryon.h"
Conditional :: () #ui {
    if Button((ButtonProps){.label="First"}) {
    } else if Toggle((ToggleProps){.label="Second"}) {
    }
    if Button(
        (ButtonProps){
            .label = "Wrapped"
        }
    ) {
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/conditional_widgets.kry"
conditional_out="$work/out/src/conditional_widgets.js"
grep -Eq '"path": "Conditional/Button@[0-9]+(-[0-9]+)?"' "$conditional_out"
grep -Eq '"path": "Conditional/Toggle@[0-9]+(-[0-9]+)?"' "$conditional_out"
grep -q '"path": "Conditional/Toggle@4-2", "sourcePath": "src/conditional_widgets.kry", "sourceLine": 4, "sourceColumn": 7, "sourceEndLine": 4, "sourceEndColumn": 55' "$conditional_out"
grep -q '"path": "Conditional/Button@6-3"' "$conditional_out"
grep -q '"sourceLine": 6' "$conditional_out"
grep -q '"sourceColumn": 5' "$conditional_out"
grep -q '"sourceEndLine": 10' "$conditional_out"
grep -q '"sourceEndColumn": 8' "$conditional_out"

cat > "$work/src/multiline_widget_metadata.kry" <<'EOF'
#import "kryon.h"
Multiline :: () #ui {
    Screen root: {
        Button(
            (ButtonProps){
                .label = "Wrapped"
            }
        )
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/multiline_widget_metadata.kry"
multiline_out="$work/out/src/multiline_widget_metadata.js"
grep -q '"path": "Multiline/root/Button@4"' "$multiline_out"
grep -q '"sourcePath": "src/multiline_widget_metadata.kry"' "$multiline_out"
grep -q '"sourceLine": 4' "$multiline_out"
grep -q '"sourceColumn": 9' "$multiline_out"
grep -q '"sourceEndLine": 8' "$multiline_out"
grep -q '"sourceEndColumn": 10' "$multiline_out"

cat > "$work/src/multiline_expression_widget_metadata.kry" <<'EOF'
#import "kryon.h"
MultilineExpression :: () #ui {
    first := Button(
        (ButtonProps){
            .label = "Declared"
        }
    )
    first = Toggle(
        (ToggleProps){
            .label = "Assigned"
        }
    )
    return Card(
        (CardProps){
            .bounds = {0, 0, 10, 10}
        }
    )
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/multiline_expression_widget_metadata.kry"
multiline_expr_out="$work/out/src/multiline_expression_widget_metadata.js"
grep -q '"path": "MultilineExpression/Button@3"' "$multiline_expr_out"
grep -q '"sourceLine": 3' "$multiline_expr_out"
grep -q '"sourceColumn": 5' "$multiline_expr_out"
grep -q '"sourceEndLine": 7' "$multiline_expr_out"
grep -q '"sourceEndColumn": 6' "$multiline_expr_out"
grep -q '"path": "MultilineExpression/Toggle@8-2"' "$multiline_expr_out"
grep -q '"sourceLine": 8' "$multiline_expr_out"
grep -q '"sourceColumn": 5' "$multiline_expr_out"
grep -q '"sourceEndLine": 12' "$multiline_expr_out"
grep -q '"sourceEndColumn": 6' "$multiline_expr_out"
grep -q '"path": "MultilineExpression/Card@13-3"' "$multiline_expr_out"
grep -q '"sourceLine": 13' "$multiline_expr_out"
grep -q '"sourceColumn": 5' "$multiline_expr_out"
grep -q '"sourceEndLine": 17' "$multiline_expr_out"
grep -q '"sourceEndColumn": 6' "$multiline_expr_out"

cat > "$work/src/direct_web_nodes.kry" <<'EOF'
#import "kryon.h"
DirectWebNodes :: () #ui {
    Page((PageProps){.title="Docs"})
    Section((SectionProps){.label="Intro"})
    Heading((HeadingProps){.text="Welcome", .level=1})
    ParagraphText((ParagraphTextProps){.text="Body"})
    Link((LinkProps){.text="Read more", .href="/docs"})
    Flow((FlowProps){.gap=4})
    Grid((GridProps){.columns=2})
    Fieldset((FieldsetProps){.legend="Options"})
    Scroll viewport: {
        bounds = (Rectangle){0, 0, 100, 100}
        content_height = 200
        scroll_offset = 0
        visible_width: float = viewport.width
        unused visible_width
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/direct_web_nodes.kry"
direct_web_out="$work/out/src/direct_web_nodes.js"
for widget in Page Section Heading ParagraphText Link Flow Grid Fieldset; do
    grep -Eq "\"path\": \"DirectWebNodes/${widget}@[0-9]+(-[0-9]+)?\"" "$direct_web_out"
done
grep -q '"nodeName": "viewport"' "$direct_web_out"
grep -q '"path": "DirectWebNodes/viewport"' "$direct_web_out"
grep -q 'const \$bounds = kryon.copyValue' "$direct_web_out"
grep -q 'let visible_width = kryon.copyValue(viewport.width)' "$direct_web_out"

cat > "$work/src/canvas_block_nodes.kry" <<'EOF'
#import "kryon.h"
state {
    sx: int = 0
    sy: int = 0
    zoom: float = 1
}
CanvasBlockNodes :: () #ui {
    Canvas drawing: {
        bounds = {0, 0, 100, 100}
        scroll_x = &sx
        scroll_y = &sy
        zoom = &zoom
        if drawing.active {
            sx += 1
        }
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/canvas_block_nodes.kry"
canvas_block_out="$work/out/src/canvas_block_nodes.js"
grep -q 'kryon.widget(\$rt, "Canvas"' "$canvas_block_out"
grep -q '"nodeName": "drawing"' "$canvas_block_out"
grep -q '"path": "CanvasBlockNodes/drawing"' "$canvas_block_out"
grep -q 'const \$canvas = kryon.Canvas(drawing_spec)' "$canvas_block_out"
grep -q 'if (drawing.active)' "$canvas_block_out"

cat > "$work/src/table_cell_block_nodes.kry" <<'EOF'
#import "kryon.h"
TableCellBlockNodes :: () #ui {
    labels: [2] const char * = {"Action", "State"}
    rows: [1] TableRow = {{.cells = nil, .cell_count = 0}}
    table: TableViewProps = (TableViewProps){.bounds = {0, 0, 200, 80}, .columns = labels, .column_count = 2, .rows = rows, .row_count = 1, .custom_cells = 1}
    TableView(table)
    TableCell action_cell: {
        table = table
        row = 0
        column = 0
        cell_width: float = action_cell.width
        unused cell_width
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/table_cell_block_nodes.kry"
table_cell_block_out="$work/out/src/table_cell_block_nodes.js"
grep -q 'kryon.widget(\$rt, "TableCell"' "$table_cell_block_out"
grep -q '"nodeName": "action_cell"' "$table_cell_block_out"
grep -q '"path": "TableCellBlockNodes/action_cell"' "$table_cell_block_out"
grep -q 'const \$bounds = kryon.recordValue("Rectangle", \[0, 0, 0, 0\])' "$table_cell_block_out"
grep -q 'let cell_width = kryon.copyValue(action_cell.width)' "$table_cell_block_out"

cat > "$work/src/popup_block_nodes.kry" <<'EOF'
#import "kryon.h"
state {
    popup_open: bool = true
    closed_open: bool = false
}
PopupBlockNodes :: () #ui {
    Popup tools: {
        bounds = {20, 20, 120, 80}
        id = 600
        open = &popup_open
        role = "dialog"
        Text label: {
            text = "Tools"
        }
    }
    Popup closed: {
        bounds = {200, 20, 120, 80}
        id = 601
        open = &closed_open
        Text hidden_label: {
            text = "Hidden"
        }
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/popup_block_nodes.kry"
popup_block_out="$work/out/src/popup_block_nodes.js"
grep -q 'kryon.widget(\$rt, "Popup"' "$popup_block_out"
grep -q '"nodeName": "tools"' "$popup_block_out"
grep -q '"path": "PopupBlockNodes/tools"' "$popup_block_out"
grep -q '"role": "dialog"' "$popup_block_out"
node --input-type=module - "$popup_block_out" "$work/out/kryon-runtime.js" <<'EOF'
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";
const module = await import(pathToFileURL(process.argv[2]).href);
const runtime = await import(pathToFileURL(process.argv[3]).href);
const state = module.createState();
const rt = runtime.createRuntime({});
module.PopupBlockNodes_PopupBlockNodes(rt, state, {});
const paths = runtime.webDocumentFrame(rt).nodes.map((node) => node.path);
assert(paths.includes("PopupBlockNodes/tools"));
assert(paths.includes("PopupBlockNodes/tools/label"));
assert(paths.includes("PopupBlockNodes/closed"));
assert(!paths.includes("PopupBlockNodes/closed/hidden_label"));
EOF

cat > "$work/src/disabled_block_nodes.kry" <<'EOF'
#import "kryon.h"
DisabledBlockNodes :: () #ui {
    Disabled locked: {
        when = true
        Button child: {
            label = "Locked"
        }
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/disabled_block_nodes.kry"
disabled_block_out="$work/out/src/disabled_block_nodes.js"
grep -q 'kryon.widget(\$rt, "Disabled", (true) ? 1 : 0, \$state' "$disabled_block_out"
grep -q 'kryon.widget(\$rt, "Disabled", "end", \$state, null)' "$disabled_block_out"
grep -q '"nodeName": "locked"' "$disabled_block_out"
grep -q '"path": "DisabledBlockNodes/locked"' "$disabled_block_out"
node --input-type=module - "$disabled_block_out" "$work/out/kryon-runtime.js" <<'EOF'
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";
const module = await import(pathToFileURL(process.argv[2]).href);
const runtime = await import(pathToFileURL(process.argv[3]).href);
const state = module.createState();
const rt = runtime.createRuntime({});
module.DisabledBlockNodes_DisabledBlockNodes(rt, state, {});
const frame = runtime.webDocumentFrame(rt);
const disabled = frame.nodes.filter((node) => node.kind === "Disabled");
assert.equal(disabled.length, 1);
assert.equal(disabled[0].path, "DisabledBlockNodes/locked");
assert.equal(disabled[0].tag, "fieldset");
assert.equal(disabled[0].state.disabled, true);
assert(frame.nodes.some((node) => node.path === "DisabledBlockNodes/locked/child"));
EOF

cat > "$work/src/declared_widget_block_nodes.kry" <<'EOF'
#import "kryon.h"
PanelProps :: struct {
    label: string
}
Panel :: (props: PanelProps) #ui {
    Text((TextProps){.text=props.label})
}
DeclaredWidgetBlockNodes :: () #ui {
    Panel card: {
        label = "Composite"
        class = "surface"
    }
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/declared_widget_block_nodes.kry"
declared_widget_block_out="$work/out/src/declared_widget_block_nodes.js"
grep -q 'kryon.widget(\$rt, "Panel"' "$declared_widget_block_out"
grep -q '"nodeName": "card"' "$declared_widget_block_out"
grep -q '"path": "DeclaredWidgetBlockNodes/card"' "$declared_widget_block_out"
grep -q '"class": "surface"' "$declared_widget_block_out"
grep -Eq 'DeclaredWidgetBlockNodes_Panel\(\$rt, \$state, \$host, widget_value_[0-9]+\)' "$declared_widget_block_out"
node --input-type=module - "$declared_widget_block_out" "$work/out/kryon-runtime.js" <<'EOF'
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";
const module = await import(pathToFileURL(process.argv[2]).href);
const runtime = await import(pathToFileURL(process.argv[3]).href);
const state = module.createState();
const rt = runtime.createRuntime({});
module.DeclaredWidgetBlockNodes_DeclaredWidgetBlockNodes(rt, state, {});
const frame = runtime.webDocumentFrame(rt);
const panel = frame.nodes.find((node) => node.path === "DeclaredWidgetBlockNodes/card");
assert(panel);
assert.equal(panel.kind, "Panel");
assert.deepEqual(panel.classes, ["surface"]);
assert(frame.nodes.some((node) => node.kind === "Text" && node.text === "Composite" &&
  node.path === "DeclaredWidgetBlockNodes/card/Text@6" &&
  node.parentPath === "DeclaredWidgetBlockNodes/card"));
assert.equal(runtime.webNodeQuery(rt, "Panel > Text").path,
  "DeclaredWidgetBlockNodes/card/Text@6");
EOF

cat > "$work/src/expression_widget_nodes.kry" <<'EOF'
#import "kryon.h"
state {
    pressed: bool = false
}
ExpressionWidgetNodes :: () #ui {
    local: bool = Button((ButtonProps){.label="Declare"})
    pressed = Button((ButtonProps){.label="Assign"})
    unused local
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/expression_widget_nodes.kry"
expression_widget_out="$work/out/src/expression_widget_nodes.js"
grep -q '"path": "ExpressionWidgetNodes/Button@6"' "$expression_widget_out"
grep -q '"path": "ExpressionWidgetNodes/Button@7-2"' "$expression_widget_out"
node --input-type=module - "$expression_widget_out" "$work/out/kryon-runtime.js" <<'EOF'
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";
const module = await import(pathToFileURL(process.argv[2]).href);
const runtime = await import(pathToFileURL(process.argv[3]).href);
const state = module.createState();
const rt = runtime.createRuntime({});
module.ExpressionWidgetNodes_ExpressionWidgetNodes(rt, state, {});
const paths = runtime.webDocumentFrame(rt).nodes.map((node) => node.path);
assert.deepEqual(paths, [
  "ExpressionWidgetNodes/Button@6",
  "ExpressionWidgetNodes/Button@7-2"
]);
EOF

cat > "$work/src/direct_runtime_nodes.kry" <<'EOF'
#import "kryon.h"
DirectRuntimeNodes :: () #ui {
    AppBackground()
    Background()
    Text((TextProps){.text="Text"})
    Paragraph()
    Box()
    Line()
    Bevel()
    Icon()
    Image()
    Button((ButtonProps){.label="Button"})
    Card((CardProps){.bounds={0,0,1,1}})
    Selectable()
    Bullet()
    Separator()
    Link()
    TextField()
    TextArea()
    Dropdown()
    SegmentedControl()
    Slider()
    Menu()
    Toggle()
    Checkbox()
    Radio()
    Progress()
    Plot()
    Drag()
    Input()
    Spinbox()
    DragDrop()
    Screen()
    Page()
    Section()
    Heading()
    ParagraphText()
    Column()
    Row()
    Stack()
    Flow()
    Grid()
    Modal()
    TitleBar()
    TabBar()
    NavigationBar()
    Toolbar()
    Toast()
    Fieldset()
    PanedView()
    Collapsible()
    ListBox()
    TreeView()
    TableView()
    ColorPicker()
    CanvasGrid()
}
EOF
"$k2js" --no-main --root "$work" -o "$work/out" "$work/src/direct_runtime_nodes.kry"
direct_runtime_out="$work/out/src/direct_runtime_nodes.js"
for widget in AppBackground Background Text Paragraph Box Line Bevel Icon Image Button Card Selectable Bullet Separator Link TextField TextArea Dropdown SegmentedControl Slider Menu Toggle Checkbox Radio Progress Plot Drag Input Spinbox DragDrop Screen Page Section Heading ParagraphText Column Row Stack Flow Grid Modal TitleBar TabBar NavigationBar Toolbar Toast Fieldset PanedView Collapsible ListBox TreeView TableView ColorPicker CanvasGrid; do
    grep -Eq "\"path\": \"DirectRuntimeNodes/${widget}@[0-9]+(-[0-9]+)?\"" "$direct_runtime_out"
done
awk '/kryon\.widget\(\$rt,/ && $0 !~ /"path": "DirectRuntimeNodes\// { missing=1 } END { exit missing }' "$direct_runtime_out"

cat > "$work/src/form_owner.kry" <<'EOF'
#import "kryon.h"

app "Form Owner" {
    size 80 60
}

FormOwnerScreen :: () #ui {
    Screen root: {
        Column contact: {
            dom = "form"
            dom_id = "contact-form"
        }
        TextField external_email: {
            text = "x@example.test"
            dom_name = "email"
            part = "external-control"
            slot = "contact-extra"
            form = "contact"
        }
    }
}
EOF
"$k2js" --root "$work" -o "$work/out" "$work/src/form_owner.kry"
form_owner_out="$work/out/src/form_owner.js"
grep -q '"tag": "form"' "$form_owner_out"
grep -q '"id": "contact-form"' "$form_owner_out"
grep -q '"domName": "email"' "$form_owner_out"
grep -q '"part": "external-control"' "$form_owner_out"
grep -q '"slot": "contact-extra"' "$form_owner_out"
grep -q '"formOwner": "contact"' "$form_owner_out"

cat > "$work/src/table_headers.kry" <<'EOF'
#import "kryon.h"

app "Table Headers" {
    size 80 60
}

TableHeaderScreen :: () #ui {
    Screen root: {
        Text price_header: {
            text = "Price"
            dom = "th"
            dom_id = "price-header"
            scope = "col"
            aria_sort = "ascending"
            aria_colindex = 1
            aria_colcount = 2
        }
        Text price_cell: {
            text = "$12"
            dom = "td"
            headers = "price_header"
            colspan = 2
            rowspan = 1
            aria_rowindex = 2
            aria_col_index = 1
            aria_row_count = 4
        }
    }
}
EOF
"$k2js" --root "$work" -o "$work/out" "$work/src/table_headers.kry"
table_headers_out="$work/out/src/table_headers.js"
grep -q '"tag": "th"' "$table_headers_out"
grep -q '"scope": "col"' "$table_headers_out"
grep -q '"ariaSort": "ascending"' "$table_headers_out"
grep -q '"ariaColIndex": 1' "$table_headers_out"
grep -q '"ariaColCount": 2' "$table_headers_out"
grep -q '"tag": "td"' "$table_headers_out"
grep -q '"headers": "price_header"' "$table_headers_out"
grep -q '"colSpan": 2' "$table_headers_out"
grep -q '"rowSpan": 1' "$table_headers_out"
grep -q '"ariaRowIndex": 2' "$table_headers_out"
grep -q '"ariaColIndex": 1' "$table_headers_out"
grep -q '"ariaRowCount": 4' "$table_headers_out"

cat > "$work/src/menu_semantics.kry" <<'EOF'
#import "kryon.h"

app "Menu Semantics" {
    size 80 60
}

MenuSemanticScreen :: () #ui {
    Screen root: {
        Column choices: {
            role = "menu"
            aria_orientation = "vertical"
            aria_multiselectable = true
        }
        Button choice_one: {
            label = "One"
            role = "menuitem"
            aria_level = 2
            aria_posinset = 1
            aria_setsize = 3
            aria_haspopup = "menu"
        }
    }
}
EOF
"$k2js" --root "$work" -o "$work/out" "$work/src/menu_semantics.kry"
menu_semantics_out="$work/out/src/menu_semantics.js"
grep -q '"ariaOrientation": "vertical"' "$menu_semantics_out"
grep -q '"ariaMultiSelectable": true' "$menu_semantics_out"
grep -q '"ariaLevel": 2' "$menu_semantics_out"
grep -q '"ariaPosInSet": 1' "$menu_semantics_out"
grep -q '"ariaSetSize": 3' "$menu_semantics_out"
grep -q '"ariaHasPopup": "menu"' "$menu_semantics_out"

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
    path "/docs/:section/:slug"
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
  { id: "about", title: "About", group: "Pages", page: "About", path: "/docs/:section/:slug" }
]);

const state = module.createState();
const rt = runtime.createRuntime({ app: module.app });
runtime.ReplaceRoute("/");
let snap = module.frame(rt, state);
assert.equal(state.visits, 1);
assert.equal(runtime.webDocumentFrame(rt).nodes[0].path, "Home/home");
assert.equal(snap.frame[1].args.text, "Home");

runtime.ReplaceRoute("/docs/api/install");
snap = module.frame(rt, state);
assert.equal(state.visits, 101);
assert.equal(runtime.webDocumentFrame(rt).nodes[0].path, "About/about");
assert.equal(snap.frame[1].args.text, "About");
assert.deepEqual(runtime.GetRouteParams(), { section: "api", slug: "install" });
assert.equal(runtime.GetRouteParam("section"), "api");

runtime.ReplaceRoute("/missing");
snap = module.frame(rt, state);
assert.equal(state.visits, 102);
assert.equal(runtime.webDocumentFrame(rt).nodes[0].path, "Home/home");
assert.equal(snap.frame[1].args.text, "Home");
EOF

"$k2js" --strict --root "$root" -o "$work/out" \
    runtime/*.kry
cp "$root"/web/*.js "$work/out/"
node "$root/tests/style_policy_test.mjs" "$work/out/runtime/style.js" \
    "$work/out/runtime/button.js" "$work/out/runtime/theme.js"

"$k2js" --strict --root "$root" -o "$work/out" \
    tests/fixtures/module_calls.kry tests/fixtures/modules/counter.kry
cp "$root"/web/*.js "$work/out/"
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
