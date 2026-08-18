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
}

app "Smoke" {
    size 320 240
    fps 60
    frame main
}

frame main {
    BeginDrawing()
    ClearBackground(GetThemeBackground())
    if tab == TAB_JOBS {
        Text(label_text(tab), ScaleUIPx(10), ScaleUIPx(20), UI_TEXT_16, GetThemeText())
    } else {
        Text("hello", ScaleUIPx(10), ScaleUIPx(20), UI_TEXT_16, GetThemeText())
    }
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
    TextInRect("in rect", (Rectangle){ScaleUIPx(4), ScaleUIPx(130), ScaleUIPx(160), ScaleUIPx(20)}, UI_TEXT_16, GetThemeText())
    TextLines("one;two;three", 3, ScaleUIPx(4), &lines_y, UI_TEXT_16, ScaleUIPx(18), GetThemeText())
    Bevel(ScaleUIPx(10), ScaleUIPx(10), ScaleUIPx(60), ScaleUIPx(20), GetThemeSurface(), GetThemeButton())
    IconTexture(2, ScaleUIPx(200), ScaleUIPx(10), ScaleUIPx(24), 3, WHITE)
    Picture((PictureProps){"tiles/tile.png", (Rectangle){ScaleUIPx(4), ScaleUIPx(150), ScaleUIPx(96), ScaleUIPx(96)}, (Rectangle){0, 0, 0, 0}, (Vector2){0, 0}, 0.0f, WHITE, UI_PICTURE_FIT_CONTAIN})
    Paragraph((UIParagraphSpec){.text = "Rich text", .icon_type = 1, .icon_size = ScaleUIPx(16), .width = ScaleUIPx(200), .font = UI_TEXT_16, .line_gap = ScaleUIPx(4), .color = GetThemeText()}, ScaleUIPx(4), &lines_y)
    IconButton((IconButtonProps){.bounds = {ScaleUIPx(210), ScaleUIPx(60), ScaleUIPx(36), ScaleUIPx(36)}, .icon_type = 2, .focus_id = 3})
    Href((HrefProps){.bounds = {ScaleUIPx(210), ScaleUIPx(110), ScaleUIPx(90), ScaleUIPx(24)}, .text = "docs", .href = "https://example.com", .font = UI_TEXT_16, .color = GetThemeLink()})
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
    EndDrawing()
}
EOF

"$k2g" --root "$work" -o "$work/out" "$work/src/valid.kry"
out=$(find "$work/out" -name "*.go" | head -1)

[ -f "$out" ] || { echo "k2g produced no output" >&2; exit 1; }

# Structural assertions: the declarative subset must translate fully.
grep -q 'package krygen' "$out"
grep -q 'ScrollOff int32' "$out"
grep -q 'func main()' "$out"
grep -q 'rt.BeginDrawing()' "$out"
grep -q '&st.ScrollOff' "$out"
grep -q 'kryruntime.NewVector2(float32(rt.ScaleUIPx(120)), float32(rt.ScaleUIPx(120)))' "$out"
grep -q 'kryruntime.Color{R: 0x2d, G: 0x4d, B: 0x7b, A: 0xff}' "$out"
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
grep -q 'rt.TabBar(' "$out"
grep -q 'rt.Checkbox(' "$out"
grep -q 'rt.Dropdown(' "$out"
grep -q 'rt.Progress(' "$out"
grep -q 'rt.Rect(' "$out"

# full whitelisted widget surface: every widget statement must lower and
# compile against the Runtime interface.
grep -q 'rt.TextInRect(' "$out"
grep -q 'rt.TextLines(' "$out"
grep -q 'rt.Bevel(' "$out"
grep -q 'rt.IconTexture(' "$out"
grep -q 'rt.Picture(kryruntime.PictureProps{AssetPath: "tiles/tile.png"' "$out"
grep -q 'rt.Paragraph(kryruntime.UIParagraphSpec{Text: "Rich text"' "$out"
grep -q 'rt.IconButton(kryruntime.IconButtonProps{' "$out"
grep -q 'FocusID: 3' "$out"
grep -q 'rt.Href(kryruntime.HrefProps{' "$out"
grep -q 'rt.Slider(9,' "$out"
grep -q 'rt.Toggle(10,' "$out"
grep -q 'rt.Stack(kryruntime.ColumnProps{' "$out"
grep -q 'rt.Key("smoke-stack")' "$out"
grep -q 'rt.Row(kryruntime.ColumnProps{' "$out"
grep -q 'rt.Modal("Title"' "$out"
grep -q 'rt.TitleBar("Smoke"' "$out"
grep -q 'rt.TopNav(kryruntime.TopNavProps{' "$out"
grep -q 'rt.Toolbar(kryruntime.ToolbarProps{' "$out"
grep -q 'rt.BottomNav(kryruntime.BottomNavProps{' "$out"
grep -q 'rt.Fade(' "$out"
grep -q 'rt.GetThemeSurface()' "$out"

# The generated source must compile against Kryon's real Go runtime. Textual
# greps alone previously allowed syntactically invalid Go to pass unnoticed.
cat > "$work/out/go.mod" <<EOF
module kryon-generated-smoke

go 1.25.0

require github.com/waozixyz/kryon/go/kryui v0.0.0
replace github.com/waozixyz/kryon/go/kryui => $root/go/kryui
EOF
(cd "$work/out" && GOCACHE="${GOCACHE:-$work/go-cache}" go test ./...)

echo "k2g syntax ok"
