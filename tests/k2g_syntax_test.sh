#!/bin/sh
# k2g syntax test — verifies the Kir-based .kry->Go pipeline output.
set -eu

k2g=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2g build/*/bin/k2g 2>/dev/null | head -1)}
work=${TMPDIR:-/tmp}/kryon-k2g-syntax-test.$$
root=$(pwd)

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -f "$k2g" ]; then
    echo "k2g not found: $k2g" >&2
    exit 1
fi

mkdir -p "$work/src" "$work/out"

cat > "$work/src/valid.kry" <<'EOF'
#import "kryon.h"

ANSWER :: #run 21 * 2
#assert ANSWER == 42, "k2g #run assertion failed"
#assert 1 + 1 == 2, "k2g fixture assertion failed"
query_jobs :: (since: long, limit: int) -> int #extern "smoke.QueryJobs"
label_text :: (i: int) -> char* #extern "smoke.LabelText"
tab_labels :: () -> char** #extern "smoke.TabLabels"

TabMode :: enum {
    TAB_OVERVIEW = 0,
    TAB_NETWORK,
    TAB_JOBS = 5,
    TAB_AFTER
}

state {
    scroll_off: int = 0
    tab: int = TAB_OVERVIEW
    check: int = 0
    pick: int = 1
    slider_val: int = 50
    toggle_val: int = 0
    lines_y: int = 0
    field_text: [64] char = ""
    field_cursor: int = 0
    area_text: [128] char = ""
    area_cursor: int = 0
    area_scroll: int = 0
    canvas_scroll_x: int = 0
    canvas_scroll_y: int = 0
    canvas_zoom: float = 1.0f
}

app "Smoke" {
    size 320 240
    fps 60
    frame main
}

frame main {
    BeginFrame()
    ClearBackground(GetThemeBackground())
    if tab == TAB_JOBS {
        Text(label_text(tab), ScaleUIPx(10), ScaleUIPx(20), Text16, GetThemeText())
    } else {
        Text("hello", ScaleUIPx(10), ScaleUIPx(20), Text16, GetThemeText())
    }
    Text("small", ScaleUIPx(10), ScaleUIPx(38), Text14, GetThemeText())
    Text("large", ScaleUIPx(10), ScaleUIPx(56), Text20, GetThemeText())
    switch tab {
        case TAB_OVERVIEW: {
            scroll_off = 0
        }
        case TAB_NETWORK:
            scroll_off = scroll_off + 1
        default:
            scroll_off = 1
    }
    for int i = 0; i < 3; i++ {
        Rect(ScaleUIPx(4), ScaleUIPx(8), ScaleUIPx(2), ScaleUIPx(2), GetThemeText())
    }
    guard tab >= 0 {
        return
    }
    TabBar((Rectangle){ScaleUIPx(4), ScaleUIPx(4), ScaleUIPx(200), ScaleUIPx(30)}, tab_labels(), &tab, NULL)
    Checkbox(0, ScaleUIPx(4), ScaleUIPx(60), "Check", &check)
    Dropdown(1, ScaleUIPx(4), ScaleUIPx(80), ScaleUIPx(120), ScaleUIPx(30), "a;b;c", &pick)
    Progress((Rectangle){ScaleUIPx(4), ScaleUIPx(120), ScaleUIPx(100), ScaleUIPx(10)}, 0, 100, query_jobs(0, 10), "")
    Scroll(ScaleUIPx(4), ScaleUIPx(8), ScaleUIPx(200), ScaleUIPx(100), ScaleUIPx(400), &scroll_off)
    DrawCircleV((Vector2){ScaleUIPx(120), ScaleUIPx(120)}, ScaleUIPx(30), (Color){0x2d, 0x4d, 0x7b, 0xff})
    DrawRing((Vector2){ScaleUIPx(120), ScaleUIPx(120)}, ScaleUIPx(36), ScaleUIPx(40), 0.0f, 360.0f, 0, (Color){0x70, 0x90, 0xc0, 0xff})
    EndScroll()
    TextInRect("in rect", (Rectangle){ScaleUIPx(4), ScaleUIPx(130), ScaleUIPx(160), ScaleUIPx(20)}, Text16, GetThemeText())
    TextLines("one;two;three", 3, ScaleUIPx(4), &lines_y, Text16, ScaleUIPx(18), GetThemeText())
    Bevel(ScaleUIPx(10), ScaleUIPx(10), ScaleUIPx(60), ScaleUIPx(20), GetThemeSurface(), GetThemeButton())
    IconTexture(2, ScaleUIPx(200), ScaleUIPx(10), ScaleUIPx(24), 3, WHITE)
    Picture((PictureProps){"tiles/tile.png", (Rectangle){ScaleUIPx(4), ScaleUIPx(150), ScaleUIPx(96), ScaleUIPx(96)}, (Rectangle){0, 0, 0, 0}, (Vector2){0, 0}, 0.0f, WHITE, PICTURE_FIT_CONTAIN})
    Paragraph((ParagraphSpec){.text = "Rich text", .icon_type = 1, .icon_size = ScaleUIPx(16), .width = ScaleUIPx(200), .font = Text16, .line_gap = ScaleUIPx(4), .color = GetThemeText()}, ScaleUIPx(4), &lines_y)
    IconButton((IconButtonProps){.bounds = {ScaleUIPx(210), ScaleUIPx(60), ScaleUIPx(36), ScaleUIPx(36)}, .icon_type = 2, .focus_id = 3})
    Href((HrefProps){.bounds = {ScaleUIPx(210), ScaleUIPx(110), ScaleUIPx(90), ScaleUIPx(24)}, .text = "docs", .href = "https://example.com", .font = Text16, .color = GetThemeLink()})
    Slider(9, ScaleUIPx(4), ScaleUIPx(170), ScaleUIPx(180), "S", 0, 100, &slider_val, "%", nil)
    Toggle(10, ScaleUIPx(200), ScaleUIPx(170), ScaleUIPx(120), ScaleUIPx(32), &toggle_val, "Off", "On")
    Stack((ColumnProps){.bounds = {ScaleUIPx(4), ScaleUIPx(190), ScaleUIPx(100), ScaleUIPx(40)}, .key = Key("smoke-stack")})
    Rect(ScaleUIPx(4), ScaleUIPx(190), ScaleUIPx(100), ScaleUIPx(40), Fade(GetThemeSurface(), 0.5f), GetThemeButton())
    End()
    Row((ColumnProps){.bounds = {ScaleUIPx(120), ScaleUIPx(190), ScaleUIPx(100), ScaleUIPx(40)}})
    End()
    Modal("Title", "Message", "Cancel", "OK")
    TitleBar("Smoke", ScaleUIPx(32))
    TopNav((TopNavProps){.id = 2, .x = 0, .y = 0, .width = ScaleUIPx(320), .height = ScaleUIPx(36), .title = "Top", .options = "x;y", .option_count = 2, .selected_index = &pick})
    Toolbar((ToolbarProps){.id = 1, .x = 0, .y = ScaleUIPx(40), .width = ScaleUIPx(300), .height = ScaleUIPx(36), .draw_menu = 1, .options = "a;b", .option_count = 2})
    BottomNav((BottomNavProps){.view_width = ScaleUIPx(320), .view_height = ScaleUIPx(240), .count = 0, .height = ScaleUIPx(56)})
    scalar: int = 5
    nums: [4] int = {1, 2, 3, 4}
    choices: [3] const char * = {"Alpha","Beta","Gamma"}
    Button((ButtonProps){.bounds = {ScaleUIPx(150), ScaleUIPx(8), ScaleUIPx(90), ScaleUIPx(28)}, .label = "GB", .style = ButtonStyleSecondary, .font = Text16, .id = 20})
    Button((ButtonProps){.bounds = {ScaleUIPx(150), ScaleUIPx(40), ScaleUIPx(90), ScaleUIPx(28)}, .label = "TB", .style = ButtonStyleSecondary, .font = Text16, .id = 21})
    Dropdown(22, ScaleUIPx(150), ScaleUIPx(70), ScaleUIPx(90), ScaleUIPx(24), choices, 3, &pick)
    frame_box: FrameBox = BeginFrameBox((Rectangle){ScaleUIPx(4), ScaleUIPx(392), ScaleUIPx(160), ScaleUIPx(80)}, ScaleUIPx(8), ScaleUIPx(8), ScaleUIPx(4))
    packed: Rectangle = FramePack(&frame_box, SideTop, ScaleUIPx(24))
    layout_grid: Grid = {frame_box.bounds, 2, 2, ScaleUIPx(4), ScaleUIPx(4), ScaleUIPx(0), ScaleUIPx(0)}
    grid_cell: Rectangle = GridCell(layout_grid, 1, 1, 1, 1)
    placed: Rectangle = Place(packed, ScaleUIPx(4), ScaleUIPx(4), ScaleUIPx(24), ScaleUIPx(12))
    CanvasGrid(grid_cell, 8, GetThemeIcon())
    CanvasGrid(placed, 4, GetThemeButton())
    canvas_result: CanvasResult = BeginCanvas((Canvas){{ScaleUIPx(180), ScaleUIPx(392), ScaleUIPx(100), ScaleUIPx(64)}, &canvas_scroll_x, &canvas_scroll_y, &canvas_zoom})
    DrawCircleV(canvas_result.world, ScaleUIPx(3), GetThemeSurface())
    EndCanvas((Canvas){{ScaleUIPx(180), ScaleUIPx(392), ScaleUIPx(100), ScaleUIPx(64)}, &canvas_scroll_x, &canvas_scroll_y, &canvas_zoom})
    Slider(23, ScaleUIPx(250), ScaleUIPx(8), ScaleUIPx(60), "", 0, 10, &slider_val, "", nil)
    CanvasGrid((Rectangle){ScaleUIPx(4), ScaleUIPx(230), ScaleUIPx(60), ScaleUIPx(40)}, 8, GetThemeIcon())
    SelectableText("select me", ScaleUIPx(150), ScaleUIPx(100), Text16, GetThemeText())
    ShowToast("toast from kry")
    TextField((TextFieldProps){.bounds = {ScaleUIPx(150), ScaleUIPx(124), ScaleUIPx(90), ScaleUIPx(24)}, .text = field_text, .text_size = sizeof(field_text), .cursor_position = &field_cursor, .focused = NULL, .max_codepoints = 63, .font = Text16, .focus_id = 30})
    TextArea((TextAreaProps){.bounds = {ScaleUIPx(250), ScaleUIPx(124), ScaleUIPx(90), ScaleUIPx(48)}, .text = area_text, .text_size = sizeof(area_text), .cursor_position = &area_cursor, .focused = NULL, .scroll_y = &area_scroll, .max_codepoints = 127, .font = Text16, .line_gap = ScaleUIPx(4), .focus_id = 31, .placeholder = "Notes", .syntax = SyntaxNone})
    Text("ro", ScaleUIPx(150), ScaleUIPx(152), Text16, GetThemeText())
    Radio((RadioButtonProps){{ScaleUIPx(4), ScaleUIPx(270), ScaleUIPx(120), ScaleUIPx(24)}, "one", 1, pick == 1, 0})
    Spinbox((SpinboxProps){{ScaleUIPx(140), ScaleUIPx(270), ScaleUIPx(90), ScaleUIPx(28)}, 24, 0, 10, 1, &slider_val, 0, ""})
    Combobox((ComboboxProps){{ScaleUIPx(240), ScaleUIPx(270), ScaleUIPx(70), ScaleUIPx(28)}, 25, choices, 3, &pick, 0})
    LabelFrame((LabelFrameProps){.bounds = {ScaleUIPx(4), ScaleUIPx(300), ScaleUIPx(120), ScaleUIPx(50)}, .title = "frame"})
    Notebook((NotebookProps){.bounds = {ScaleUIPx(140), ScaleUIPx(300), ScaleUIPx(120), ScaleUIPx(50)}, .tabs = choices[:], .selected_index = &pick})
    ListBox((ListBoxProps){.bounds = {ScaleUIPx(280), ScaleUIPx(300), ScaleUIPx(60), ScaleUIPx(50)}, .id = 26, .items = choices[:], .selected_index = &pick})
    Collapsible((CollapsibleProps){.bounds = {ScaleUIPx(4), ScaleUIPx(360), ScaleUIPx(120), ScaleUIPx(30)}, .label = "sect", .open = NULL})
    SetThemeDarkMode(1)
    SetCurrentTheme(0, 1)
    Dropdown(11, ScaleUIPx(4), ScaleUIPx(210), ScaleUIPx(120), ScaleUIPx(24), choices, 3, &pick)
    Progress((Rectangle){ScaleUIPx(140), ScaleUIPx(210), ScaleUIPx(100), ScaleUIPx(10)}, 0, 100, nums[0] + scalar, "")
    TextLines("one;two;three", 3, ScaleUIPx(4), &lines_y, Text16, ScaleUIPx(18), GetThemeText())
    attempts: int = 0
retry:
    attempts += 1
    if attempts < 3 {
        goto retry
    }
    EndFrame()
}
EOF

"$k2g" --root "$work" -o "$work/out" "$work/src/valid.kry"
out=$(find "$work/out" -name "*.go" | head -1)

[ -f "$out" ] || { echo "k2g produced no output" >&2; exit 1; }
sh "$root/tests/check_clean_generated_output.sh" "$work/out"

if "$k2g" --runtime github.com/waozixyz/kryon/go/kryui \
    --root "$work" -o "$work/out" "$work/src/valid.kry" \
    2>"$work/runtime_override.err"; then
    echo "k2g accepted --runtime override; generated Go must target the native kryon runtime" >&2
    exit 1
fi
grep -q 'usage: k2g' "$work/runtime_override.err"

# Structural assertions: the declarative subset must translate fully.
grep -q 'package krygen' "$out"
grep -q 'import kryon "github.com/waozixyz/kryon/go/kryon"' "$out"
grep -q 'ScrollOff int32' "$out"
grep -q 'func main()' "$out"
grep -q 'BeginFrame()' "$out"
grep -q '&st.ScrollOff' "$out"
grep -q 'kryon.NewVector2(float32(kryon.ScaleUIPx(120)), float32(kryon.ScaleUIPx(120)))' "$out"
grep -q 'kryon.Color{R: 0x2d, G: 0x4d, B: 0x7b, A: 0xff}' "$out"
grep -q '0.0, 360.0' "$out"   # C float suffixes stripped
if grep -q '0\.0f' "$out"; then
    echo "k2g left a C float suffix in Go output" >&2
    exit 1
fi
if grep -q 'TODO k2g' "$out"; then
    echo "k2g left a TODO lowering in Go output:" >&2
    grep 'TODO k2g' "$out" >&2
    exit 1
fi
unqualified_runtime_calls="$(
    rg -n '^\t+(BeginFrame|EndFrame|Text|Button|TextField|TextArea|Row|Column|Stack|Dropdown|Progress|Rect|Scroll|EndScroll|Open|Close)\(' "$out" || true
)"
if [ -n "$unqualified_runtime_calls" ]; then
    echo "k2g emitted unqualified runtime calls; generated Go must use kryon.<Name>:" >&2
    echo "$unqualified_runtime_calls" >&2
    exit 1
fi

# '#extern' host bridge: interface, setter, converted calls.
grep -q 'type ValidHost interface' "$out"
grep -q 'func SetValidHost(host ValidHost)' "$out"
grep -q 'QueryJobs(Since int64, Limit int32) int32' "$out"
grep -q 'LabelText(I int32) string' "$out"
grep -q 'TabLabels() \[\]string' "$out"
grep -q 'validHost.QueryJobs(int64(0), int32(10))' "$out"
grep -q 'validHost.LabelText(int32(st.Tab))' "$out"

# enums: typed constants with C counter semantics, rewritten at use sites.
grep -q 'type TabMode int32' "$out"
grep -q 'TabModeTAB_OVERVIEW = 0' "$out"
grep -q 'TabModeTAB_NETWORK = 1' "$out"
grep -q 'TabModeTAB_JOBS = 5' "$out"
grep -q 'TabModeTAB_AFTER = 6' "$out"
grep -q 'Tab: TabModeTAB_OVERVIEW' "$out"
grep -q 'st.Tab == TabModeTAB_JOBS' "$out"

# switch/case/default, C-style for headers, guard.
grep -q 'case TabModeTAB_OVERVIEW:' "$out"
grep -q 'case TabModeTAB_NETWORK:' "$out"
grep -q 'default:' "$out"
grep -q 'for i := int32(0); i < 3; i++' "$out"
grep -q 'if st.Tab >= 0 {' "$out"

# widget surface used by declarative apps.
grep -q 'TabBar(' "$out"
grep -q 'Checkbox(' "$out"
grep -q 'Dropdown(' "$out"
grep -q 'Progress(' "$out"
grep -q 'Rect(' "$out"
grep -q 'kryon.Text("small".*kryon.Text14.*kryon.GetThemeText())' "$out"
grep -q 'kryon.Text("large".*kryon.Text20.*kryon.GetThemeText())' "$out"

# full whitelisted widget surface: every widget statement must lower and
# compile against the clean package API.
grep -q 'TextInRect(' "$out"
grep -q 'TextLines(' "$out"
grep -q 'Bevel(' "$out"
grep -q 'IconTexture(' "$out"
grep -q 'kryon.Picture(kryon.PictureProps{AssetPath: "tiles/tile.png"' "$out"
grep -q 'kryon.Paragraph(kryon.ParagraphSpec{Text: "Rich text"' "$out"
grep -q 'kryon.IconButton(kryon.IconButtonProps{' "$out"
grep -q 'FocusID: 3' "$out"
grep -q 'kryon.Href(kryon.HrefProps{' "$out"
grep -q 'Slider(9,' "$out"
grep -q 'Toggle(10,' "$out"
grep -q 'kryon.Stack(kryon.ColumnProps{' "$out"
grep -q 'Key("smoke-stack")' "$out"
grep -q 'kryon.Row(kryon.ColumnProps{' "$out"
grep -q 'Modal("Title"' "$out"
grep -q 'TitleBar("Smoke"' "$out"
grep -q 'kryon.TopNav(kryon.TopNavProps{' "$out"
grep -q 'kryon.Toolbar(kryon.ToolbarProps{' "$out"
grep -q 'kryon.BottomNav(kryon.BottomNavProps{' "$out"
grep -q 'Fade(' "$out"
grep -q 'GetThemeSurface()' "$out"

# Go-parity surface: the remaining widget families lower and compile
grep -q 'kryon.Button(kryon.ButtonProps{Bounds: kryon.NewRectangle.*Label: "GB"' "$out"
grep -q 'kryon.Button(kryon.ButtonProps{Bounds: kryon.NewRectangle.*Label: "TB"' "$out"
grep -q 'Dropdown(22,' "$out"
grep -q 'kryon.BeginFrameBox(kryon.NewRectangle' "$out"
grep -q 'kryon.FramePack(&frame_box, kryon.SideTop' "$out"
grep -q 'kryon.GridCell(layout_grid, 1, 1, 1, 1)' "$out"
grep -q 'kryon.Place(packed,' "$out"
grep -q 'kryon.BeginCanvas(kryon.Canvas{' "$out"
grep -q 'kryon.EndCanvas(kryon.Canvas{' "$out"
grep -q 'CanvasGrid(' "$out"
grep -q 'SelectableText(' "$out"
grep -q 'ShowToast("toast from kry")' "$out"
grep -q 'kryon.TextField(kryon.TextFieldProps{' "$out"
grep -q 'kryon.TextArea(kryon.TextAreaProps{.*Syntax: kryon.SyntaxNone' "$out"
grep -q 'kryon.Radio(kryon.RadioButtonProps{' "$out"
grep -q 'kryon.Spinbox(kryon.SpinboxProps{' "$out"
grep -q 'kryon.Combobox(kryon.ComboboxProps{' "$out"
grep -q 'kryon.LabelFrame(kryon.LabelFrameProps{' "$out"
grep -q 'kryon.Notebook(kryon.NotebookProps{' "$out"
grep -q 'kryon.ListBox(kryon.ListBoxProps{' "$out"
grep -q 'kryon.Collapsible(kryon.CollapsibleProps{' "$out"
grep -q 'SetThemeDarkMode(1' "$out"
grep -q 'SetCurrentTheme(0, 1)' "$out"

# typed declarations, arrays, and goto/labels lower for real now
grep -q 'var scalar int32 = 5' "$out"
grep -q 'var nums = \[4\]int32{1, 2, 3, 4}' "$out"
grep -q 'var choices = \[3\]string{"Alpha","Beta","Gamma"}' "$out"
grep -q 'kryon.Dropdown(11, kryon.ScaleUIPx(4), kryon.ScaleUIPx(210), kryon.ScaleUIPx(120), kryon.ScaleUIPx(24), choices, 3, &st.Pick)' "$out"
grep -q 'retry:$' "$out"
grep -q 'goto retry' "$out"

# The generated source must compile against Kryon's native Go runtime. Textual
# greps alone previously allowed syntactically invalid Go to pass unnoticed.
cat > "$work/out/go.mod" <<EOF
module kryon-generated-smoke

go 1.25.0

require github.com/waozixyz/kryon/go/kryon v0.0.0
replace github.com/waozixyz/kryon/go/kryon => $root/go/kryon
EOF
(cd "$work/out" && GOCACHE="${GOCACHE:-$work/go-cache}" go test ./...)

cat > "$work/src/assert_fail.kry" <<'EOF'
#import "kryon.h"
#assert 2 * 2 == 5, "k2g constant assertion failed"
EOF

if "$k2g" --root "$work" -o "$work/out" "$work/src/assert_fail.kry" 2>"$work/assert_fail.err"; then
    echo "false constant #assert did not fail during k2g parsing" >&2
    exit 1
fi
grep -q 'k2g constant assertion failed' "$work/assert_fail.err"

cat > "$work/src/assert_unknown.kry" <<'EOF'
#import "kryon.h"
WEB :: #defined(PLATFORM_WEB)
#assert WEB, "k2g unresolved assertion"
EOF

if "$k2g" --root "$work" -o "$work/out" "$work/src/assert_unknown.kry" 2>"$work/assert_unknown.err"; then
    echo "unresolved #assert did not fail in k2g" >&2
    exit 1
fi
grep -q 'unresolved #assert is not supported by the Go backend' "$work/assert_unknown.err"

echo "k2g syntax ok"
