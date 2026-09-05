#!/bin/sh
# k2go syntax test — verifies the Kir-based .kry->Go pipeline output.
set -eu

k2go=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2go build/*/bin/k2go 2>/dev/null | head -1)}
work=${TMPDIR:-/tmp}/kryon-k2go-syntax-test.$$
root=$(pwd)

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -f "$k2go" ]; then
    echo "k2go not found: $k2go" >&2
    exit 1
fi

mkdir -p "$work/src" "$work/out" "$work/hierarchy-out" "$work/pure-out"

cat > "$work/src/valid.kry" <<'EOF'
#import "kryon.h"

ANSWER :: #run 21 * 2
#assert ANSWER == 42, "k2go #run assertion failed"
#assert 1 + 1 == 2, "k2go fixture assertion failed"
query_jobs :: (since: long, limit: int) -> int #extern "smoke.QueryJobs"
label_text :: (i: int) -> char* #extern "smoke.LabelText"
tab_labels :: () -> char** #extern "smoke.TabLabels"
store_secret :: (secret: const char*, site: const char*, login: const char*, a: int, b: int, c: int, d: int, e: int, f: int, exclude: const char*) -> int #extern "smoke.StoreSecret"
direct_scale :: (value: int) -> int #extern "github.com/waozixyz/kryon/go/kryon.ScaleUIPx"
direct_queue_text :: (value: const char*) #extern "github.com/waozixyz/kryon/go/kryon.QueueText"
c_abs :: (value: int) -> int #extern "c.abs"

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
    menu_open: int = -1
    context_open: int = 0
    context_x: int = 0
    context_y: int = 0
    selected_row: int = 0
    feature_flags: int = 1
    drag_float_min: float = 2.0f
    drag_float_max: float = 8.0f
    drag_int_min: int = 2
    drag_int_max: int = 8
}

app "Smoke" {
    size 320 240
    fps 60
}

local_value :: () -> int {
    return ScaleUIPx(5)
}

relay_text :: (value: [64] char) {
    direct_queue_text(value)
}

App :: () #ui {
    Screen root: {
    ClearBackground(GetThemeBackground())
    menu_items: [2] MenuItem = {{MenuCommand,"Open","Ctrl+O",46,0,0,NULL,0},{MenuCheck,"Grid",NULL,47,0,1,NULL,0}}
    menus: [1] Menu = {{(Rectangle){0,0,0,0},"File",menu_items,2}}
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
    Progress((ProgressBarProps){{ScaleUIPx(4), ScaleUIPx(120), ScaleUIPx(100), ScaleUIPx(10)}, 0, 100, query_jobs(0, 10), ""})
    Scroll(ScaleUIPx(4), ScaleUIPx(8), ScaleUIPx(200), ScaleUIPx(100), ScaleUIPx(400), &scroll_off)
    DrawCircleV((Vector2){ScaleUIPx(120), ScaleUIPx(120)}, ScaleUIPx(30), (Color){0x2d, 0x4d, 0x7b, 0xff})
    DrawRing((Vector2){ScaleUIPx(120), ScaleUIPx(120)}, ScaleUIPx(36), ScaleUIPx(40), 0.0f, 360.0f, 0, (Color){0x70, 0x90, 0xc0, 0xff})
    EndScroll()
    TextInRect("in rect", (Rectangle){ScaleUIPx(4), ScaleUIPx(130), ScaleUIPx(160), ScaleUIPx(20)}, Text16, GetThemeText())
    TextLines("one;two;three", 3, ScaleUIPx(4), &lines_y, Text16, ScaleUIPx(18), GetThemeText())
    Bevel(ScaleUIPx(10), ScaleUIPx(10), ScaleUIPx(60), ScaleUIPx(20), GetThemeSurface(), GetThemeButton())
    Icon(2, ScaleUIPx(200), ScaleUIPx(10), ScaleUIPx(24), 3, WHITE)
    Picture((PictureProps){"tiles/tile.png", (Rectangle){ScaleUIPx(4), ScaleUIPx(150), ScaleUIPx(96), ScaleUIPx(96)}, (Rectangle){0, 0, 0, 0}, (Vector2){0, 0}, 0.0f, WHITE, PICTURE_FIT_CONTAIN})
    Paragraph((ParagraphSpec){.text = "Rich text", .icon_type = 1, .icon_size = ScaleUIPx(16), .width = ScaleUIPx(200), .font = Text16, .line_gap = ScaleUIPx(4), .color = GetThemeText()}, ScaleUIPx(4), &lines_y)
    IconButton((IconButtonProps){.bounds = {ScaleUIPx(210), ScaleUIPx(60), ScaleUIPx(36), ScaleUIPx(36)}, .icon_type = 2, .focus_id = 3})
    Href((HrefProps){.bounds = {ScaleUIPx(210), ScaleUIPx(110), ScaleUIPx(90), ScaleUIPx(24)}, .text = "docs", .href = "https://example.com", .font = Text16, .color = GetThemeLink()})
    SetPageTitle("Kryon Page")
    SetPageDescription("Generated page")
    SetPageCanonicalURL("https://example.com/page")
    SetPageThemeColor((Color){0x11, 0x22, 0x33, 0xff})
    ReplaceRoute("/page#top")
    route_version: int = GetRouteVersion()
    Progress((ProgressBarProps){{ScaleUIPx(188), ScaleUIPx(204), ScaleUIPx(60), ScaleUIPx(10)}, 0, 10, route_version, ""})
    Page((PageProps){.title = "Kryon Page", .description = "Generated page", .canonical_url = "https://example.com/page", .theme_color = (Color){0x11, 0x22, 0x33, 0xff}, .background = GetThemeBackground(), .gap = ScaleUIPx(6), .padding = ScaleUIPx(8)})
    Heading((HeadingProps){.text = "Welcome", .level = 1, .font = Text24, .color = GetThemeText()})
    ParagraphText((ParagraphTextProps){.bounds = {0, 0, ScaleUIPx(160), 0}, .text = "Body", .font = Text16, .color = GetThemeText(), .line_gap = ScaleUIPx(4)})
    Link((LinkProps){.bounds = {0, 0, ScaleUIPx(90), ScaleUIPx(24)}, .text = "More", .href = "/more", .font = Text16, .color = GetThemeLink()})
    PagePicture((PictureProps){"hero.png", (Rectangle){0, 0, ScaleUIPx(96), ScaleUIPx(48)}, (Rectangle){0, 0, 0, 0}, (Vector2){0, 0}, 0.0f, WHITE, PICTURE_FIT_COVER}, "Hero")
    End()
    Section((SectionProps){.label = "Details", .gap = ScaleUIPx(4), .padding = ScaleUIPx(4)})
    Heading((HeadingProps){.text = "Details", .level = 2})
    End()
    Flow((FlowProps){.bounds = {ScaleUIPx(4), ScaleUIPx(176), ScaleUIPx(180), ScaleUIPx(24)}, .gap = ScaleUIPx(4)})
    Text("flow", 0, 0, Text16, GetThemeText())
    End()
    PageGrid((GridProps){.bounds = {ScaleUIPx(4), ScaleUIPx(204), ScaleUIPx(180), ScaleUIPx(40)}, .columns = 2, .gap = ScaleUIPx(4), .padding = ScaleUIPx(4)})
    Text("g1", 0, 0, Text16, GetThemeText())
    Text("g2", 0, 0, Text16, GetThemeText())
    End()
    Slider(9, ScaleUIPx(4), ScaleUIPx(170), ScaleUIPx(180), "S", 0, 100, &slider_val, "%", nil)
    Toggle(10, ScaleUIPx(200), ScaleUIPx(170), ScaleUIPx(120), ScaleUIPx(32), &toggle_val, "Off", "On")
    Stack smoke_stack: {
        bounds = {ScaleUIPx(4), ScaleUIPx(190), ScaleUIPx(100), ScaleUIPx(40)}
        key = Key("smoke-stack")
        Rect(ScaleUIPx(4), ScaleUIPx(190), ScaleUIPx(100), ScaleUIPx(40), Fade(GetThemeSurface(), 0.5f), GetThemeButton())
    }
    Row smoke_row: {
        bounds = {ScaleUIPx(120), ScaleUIPx(190), ScaleUIPx(100), ScaleUIPx(40)}
    }
    Modal("Title", "Message", "Cancel", "OK")
    TitleBar("Smoke", ScaleUIPx(32))
    TopNav((TopNavProps){.id = 2, .x = 0, .y = 0, .width = ScaleUIPx(320), .height = ScaleUIPx(36), .title = "Top", .options = "x;y", .option_count = 2, .selected_index = &pick})
    Toolbar((ToolbarProps){.id = 1, .x = 0, .y = ScaleUIPx(40), .width = ScaleUIPx(300), .height = ScaleUIPx(36), .draw_menu = 1, .options = "a;b", .option_count = 2})
    BottomNav((BottomNavProps){.view_width = ScaleUIPx(320), .view_height = ScaleUIPx(240), .count = 0, .height = ScaleUIPx(56)})
    scalar: int = 5
    nums: [4] int = {1, 2, 3, 4}
    plot_values: [4] float = {0.0f, 0.25f, 1.0f, 0.5f}
    plot_doubles: [2] double = {1.0, 2.0}
    edit_color: [4] float = {0.2f, 0.4f, 0.6f, 0.8f}
    choices: [3] const char * = {"Alpha","Beta","Gamma"}
    tree_items: [2] UITreeItem = {{"Root",0,1,1,0},{"Leaf",1,2,0,1}}
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
    store_secret(field_text, area_text, "literal", 1, 2, 3, 4, 5, 6, area_text)
    Text("ro", ScaleUIPx(150), ScaleUIPx(152), Text16, GetThemeText())
    Radio((RadioButtonProps){{ScaleUIPx(4), ScaleUIPx(270), ScaleUIPx(120), ScaleUIPx(24)}, "one", 1, pick == 1, 0})
    Spinbox((SpinboxProps){{ScaleUIPx(140), ScaleUIPx(270), ScaleUIPx(90), ScaleUIPx(28)}, 24, 0, 10, 1, &slider_val, 0, ""})
    Combobox((ComboboxProps){{ScaleUIPx(240), ScaleUIPx(270), ScaleUIPx(70), ScaleUIPx(28)}, 25, choices, 3, &pick, 0})
    LabelFrame((LabelFrameProps){.bounds = {ScaleUIPx(4), ScaleUIPx(300), ScaleUIPx(120), ScaleUIPx(50)}, .title = "frame"})
    Notebook((NotebookProps){.bounds = {ScaleUIPx(140), ScaleUIPx(300), ScaleUIPx(120), ScaleUIPx(50)}, .tabs = choices[:], .selected_index = &pick})
    ListBox((ListBoxProps){.bounds = {ScaleUIPx(280), ScaleUIPx(300), ScaleUIPx(60), ScaleUIPx(50)}, .id = 26, .items = choices[:], .selected_index = &pick})
    TreeView((TreeViewProps){.bounds = {ScaleUIPx(280), ScaleUIPx(356), ScaleUIPx(80), ScaleUIPx(50)}, .id = 27, .items = tree_items, .item_count = 2, .selected_id = &pick})
    Collapsible((CollapsibleProps){.bounds = {ScaleUIPx(4), ScaleUIPx(360), ScaleUIPx(120), ScaleUIPx(30)}, .label = "sect", .open = NULL})
    SetThemeDarkMode(1)
    SetCurrentTheme(0, 1)
    Dropdown(11, ScaleUIPx(4), ScaleUIPx(210), ScaleUIPx(120), ScaleUIPx(24), choices, 3, &pick)
    Progress((ProgressBarProps){{ScaleUIPx(140), ScaleUIPx(210), ScaleUIPx(100), ScaleUIPx(10)}, 0, 100, nums[0] + scalar, ""})
    PlotLines((PlotProps){.bounds = {ScaleUIPx(250), ScaleUIPx(210), ScaleUIPx(100), ScaleUIPx(40)}, .label = "Lines", .values = plot_values, .value_count = 4, .scale_min = 0.0f, .scale_max = 1.0f})
    PlotHistogram((PlotProps){.bounds = {ScaleUIPx(250), ScaleUIPx(254), ScaleUIPx(100), ScaleUIPx(40)}, .label = "Bars", .values = plot_values, .value_count = 4, .offset = 1})
    DragFloat((DragFloatProps){.bounds = {ScaleUIPx(250), ScaleUIPx(298), ScaleUIPx(100), ScaleUIPx(28)}, .id = 28, .label = "Float", .values = plot_values, .value_count = 2, .speed = 0.1f, .min = 0.0f, .max = 1.0f})
    DragInt((DragIntProps){.bounds = {ScaleUIPx(250), ScaleUIPx(330), ScaleUIPx(100), ScaleUIPx(28)}, .id = 29, .label = "Int", .values = nums, .value_count = 2, .speed = 1.0f, .min = 0, .max = 10})
    DragFloatRange2((DragFloatRange2Props){.bounds = {ScaleUIPx(250),ScaleUIPx(346),ScaleUIPx(100),ScaleUIPx(28)}, .id = 52, .label = "Float range", .current_min = &drag_float_min, .current_max = &drag_float_max, .speed = 0.1f, .min = 0.0f, .max = 10.0f, .format_max = "max %.1f"})
    DragIntRange2((DragIntRange2Props){.bounds = {ScaleUIPx(250),ScaleUIPx(378),ScaleUIPx(100),ScaleUIPx(28)}, .id = 53, .label = "Int range", .current_min = &drag_int_min, .current_max = &drag_int_max, .min = 0, .max = 10, .format_max = "max %d"})
    SliderFloat((SliderFloatProps){.bounds = {ScaleUIPx(250), ScaleUIPx(362), ScaleUIPx(100), ScaleUIPx(28)}, .id = 30, .label = "Slider float", .values = plot_values, .value_count = 2, .min = 0.0f, .max = 1.0f})
    SliderInt((SliderIntProps){.bounds = {ScaleUIPx(250), ScaleUIPx(394), ScaleUIPx(100), ScaleUIPx(28)}, .id = 31, .label = "Slider int", .values = nums, .value_count = 2, .min = 0, .max = 10})
    VSliderFloat((SliderFloatProps){.bounds = {ScaleUIPx(362), ScaleUIPx(298), ScaleUIPx(28), ScaleUIPx(100)}, .id = 32, .values = plot_values, .value_count = 1, .min = 0.0f, .max = 1.0f})
    VSliderInt((SliderIntProps){.bounds = {ScaleUIPx(394), ScaleUIPx(298), ScaleUIPx(28), ScaleUIPx(100)}, .id = 33, .values = nums, .value_count = 1, .min = 0, .max = 10})
    SliderAngle((SliderAngleProps){.bounds = {ScaleUIPx(250), ScaleUIPx(426), ScaleUIPx(100), ScaleUIPx(28)}, .id = 34, .value = &plot_values[0], .min_degrees = -180.0f, .max_degrees = 180.0f})
    InputFloat((InputFloatProps){.bounds = {ScaleUIPx(250), ScaleUIPx(458), ScaleUIPx(100), ScaleUIPx(28)}, .id = 35, .values = plot_values, .value_count = 2, .step = 0.1f, .step_fast = 1.0f})
    InputInt((InputIntProps){.bounds = {ScaleUIPx(250), ScaleUIPx(490), ScaleUIPx(100), ScaleUIPx(28)}, .id = 36, .values = nums, .value_count = 2, .step = 1, .step_fast = 10})
    InputDouble((InputDoubleProps){.bounds = {ScaleUIPx(250), ScaleUIPx(522), ScaleUIPx(100), ScaleUIPx(28)}, .id = 37, .values = plot_doubles, .value_count = 2, .step = 0.01, .step_fast = 1.0})
    SmallButton((ButtonProps){.bounds = {ScaleUIPx(250), ScaleUIPx(554), ScaleUIPx(70), ScaleUIPx(24)}, .label = "Small", .id = 38})
    InvisibleButton((InvisibleButtonProps){.bounds = {ScaleUIPx(324), ScaleUIPx(554), ScaleUIPx(30), ScaleUIPx(24)}, .id = 39})
    ArrowButton((ArrowButtonProps){.bounds = {ScaleUIPx(358), ScaleUIPx(554), ScaleUIPx(30), ScaleUIPx(24)}, .id = 40, .direction = 1})
    Bullet((Rectangle){ScaleUIPx(392), ScaleUIPx(554), ScaleUIPx(20), ScaleUIPx(20)})
    Separator((Rectangle){ScaleUIPx(250), ScaleUIPx(582), ScaleUIPx(160), ScaleUIPx(4)}, 0)
    ColorEdit3((ColorEditProps){.bounds = {ScaleUIPx(250), ScaleUIPx(590), ScaleUIPx(160), ScaleUIPx(28)}, .id = 41, .values = edit_color, .value_count = 3})
    ColorEdit4((ColorEditProps){.bounds = {ScaleUIPx(250), ScaleUIPx(622), ScaleUIPx(160), ScaleUIPx(28)}, .id = 42, .values = edit_color, .value_count = 4})
    ColorPicker3((ColorEditProps){.bounds = {ScaleUIPx(250), ScaleUIPx(654), ScaleUIPx(70), ScaleUIPx(130)}, .id = 43, .values = edit_color, .value_count = 3})
    ColorPicker4((ColorEditProps){.bounds = {ScaleUIPx(324), ScaleUIPx(654), ScaleUIPx(70), ScaleUIPx(130)}, .id = 44, .values = edit_color, .value_count = 4})
    ColorButton((ColorButtonProps){.bounds = {ScaleUIPx(398), ScaleUIPx(654), ScaleUIPx(60), ScaleUIPx(28)}, .id = 45, .label = "Tint", .color = (Color){51,102,153,204}})
    TextColored("colored", ScaleUIPx(250), ScaleUIPx(690), Text16, (Color){220,60,80,255})
    TextDisabled("disabled", ScaleUIPx(250), ScaleUIPx(714), Text16)
    TextWrapped("wrapped helper text", (Rectangle){ScaleUIPx(250),ScaleUIPx(738),ScaleUIPx(160),ScaleUIPx(40)}, Text16, GetThemeText())
    LabelText("Status", "Ready", (Rectangle){ScaleUIPx(250),ScaleUIPx(782),ScaleUIPx(160),ScaleUIPx(20)}, Text16, GetThemeText())
    BulletText("bullet text", (Rectangle){ScaleUIPx(250),ScaleUIPx(806),ScaleUIPx(160),ScaleUIPx(20)}, Text16, GetThemeText())
    MenuBar(46, (Rectangle){ScaleUIPx(4),ScaleUIPx(834),ScaleUIPx(220),ScaleUIPx(30)}, menus, 1, &menu_open)
    PopupMenu(47, ScaleUIPx(4), ScaleUIPx(868), menu_items, 2)
    ContextMenu((ContextMenuProps){.id = 48, .trigger = {ScaleUIPx(230),ScaleUIPx(834),ScaleUIPx(100),ScaleUIPx(60)}, .items = menu_items, .item_count = 2, .open = &context_open, .x = &context_x, .y = &context_y})
    Tooltip((TooltipProps){.trigger = {ScaleUIPx(230),ScaleUIPx(900),ScaleUIPx(100),ScaleUIPx(30)}, .text = "Helpful text", .font = Text14, .max_width = ScaleUIPx(160)})
    choice_picture: PictureProps = {"tiles/tile.png",(Rectangle){ScaleUIPx(250),ScaleUIPx(934),ScaleUIPx(48),ScaleUIPx(32)},(Rectangle){0,0,0,0},(Vector2){0,0},0.0f,WHITE,PICTURE_FIT_CONTAIN}
    Selectable((SelectableProps){.bounds = {ScaleUIPx(4),ScaleUIPx(934),ScaleUIPx(120),ScaleUIPx(28)}, .id = 49, .label = "Choice", .selected = &selected_row})
    CheckboxFlags((CheckboxFlagsProps){.bounds = {ScaleUIPx(4),ScaleUIPx(966),ScaleUIPx(160),ScaleUIPx(28)}, .id = 50, .label = "Feature", .flags = &feature_flags, .flags_value = 4})
    ImageWithBg((ImageWithBgProps){.picture = choice_picture, .background = GetThemeSurface()})
    ImageButton((ImageButtonProps){.picture = choice_picture, .background = GetThemeButton(), .id = 51})
    SeparatorText((SeparatorTextProps){.bounds = {ScaleUIPx(250),ScaleUIPx(1000),ScaleUIPx(160),ScaleUIPx(24)}, .label = "Section", .font = Text14})
    Progress((ProgressBarProps){{ScaleUIPx(140), ScaleUIPx(224), ScaleUIPx(100), ScaleUIPx(10)}, 0, 100, direct_scale(16), ""})
    Progress((ProgressBarProps){{ScaleUIPx(140), ScaleUIPx(238), ScaleUIPx(100), ScaleUIPx(10)}, 0, 100, helper_value(), ""})
    Progress((ProgressBarProps){{ScaleUIPx(140), ScaleUIPx(252), ScaleUIPx(100), ScaleUIPx(10)}, 0, 100, c_abs(-8), ""})
    Progress((ProgressBarProps){{ScaleUIPx(140), ScaleUIPx(266), ScaleUIPx(100), ScaleUIPx(10)}, 0, 100, local_value(), ""})
    relay_text(field_text)
    TextLines("one;two;three", 3, ScaleUIPx(4), &lines_y, Text16, ScaleUIPx(18), GetThemeText())
    attempts: int = 0
retry:
    attempts += 1
    if attempts < 3 {
        goto retry
    }
    }
}
EOF

cat > "$work/src/hierarchy.kry" <<'EOF'
#import "kryon.h"

app "Hierarchy" {
    size 320 240
}

Main :: (viewport: Rectangle) #ui {
    Screen root: {
        bounds = viewport
        padding = 8

        Column body: {
            gap = 4
            Text("Hello", 0, 0, Text16, GetThemeText())
        }
    }
}
EOF

cat > "$work/src/helper.kry" <<'EOF'
#import "kryon.h"

helper_value :: () -> int {
    return ScaleUIPx(7)
}
EOF

"$k2go" --root "$work" -o "$work/out" "$work/src/valid.kry" "$work/src/helper.kry"
"$k2go" --root "$work" -o "$work/hierarchy-out" "$work/src/hierarchy.kry"
"$k2go" --root "$work" -o "$work/pure-out" "$work/src/hierarchy.kry"
out="$work/out/valid.go"
cgo="$work/out/valid_cgo.go"
helper="$work/out/helper.go"
hier="$work/hierarchy-out/hierarchy.go"
pure="$work/pure-out/hierarchy.go"

[ -f "$out" ] || { echo "k2go produced no output" >&2; exit 1; }
[ -f "$cgo" ] || { echo "k2go produced no cgo output" >&2; exit 1; }
[ -f "$helper" ] || { echo "k2go produced no helper output" >&2; exit 1; }
[ -f "$hier" ] || { echo "k2go produced no hierarchy output" >&2; exit 1; }
[ -f "$pure" ] || { echo "k2go produced no pure output" >&2; exit 1; }
sh "$root/tests/check_clean_generated_output.sh" "$work/out"
sh "$root/tests/check_clean_generated_output.sh" "$work/hierarchy-out"
sh "$root/tests/check_clean_generated_output.sh" "$work/pure-out"
if [ -e "$work/pure-out/hierarchy_cgo.go" ] || grep -q 'import "C"' "$pure"; then
    echo "pure generated Go unexpectedly imports cgo" >&2
    exit 1
fi

if "$k2go" --runtime github.com/waozixyz/kryon/go/kryui \
    --root "$work" -o "$work/out" "$work/src/valid.kry" \
    2>"$work/runtime_override.err"; then
    echo "k2go accepted --runtime override; generated Go must target the native kryon runtime" >&2
    exit 1
fi
grep -q 'usage: k2go' "$work/runtime_override.err"

# Structural assertions: the declarative subset must translate fully.
grep -q 'package krygen' "$out"
grep -q 'import kryon "github.com/waozixyz/kryon/go/kryon"' "$out"
grep -q 'import kryonpkg "github.com/waozixyz/kryon/go/kryon"' "$out"
grep -q 'ScrollOff int32' "$out"
grep -q 'func main()' "$out"
grep -q 'kryon.BeginFrame()' "$out"
grep -q '&st.ScrollOff' "$out"
grep -q 'kryon.NewVector2(float32(kryon.ScaleUIPx(120)), float32(kryon.ScaleUIPx(120)))' "$out"
grep -q 'kryon.Color{R: 0x2d, G: 0x4d, B: 0x7b, A: 0xff}' "$out"
grep -q '0.0, 360.0' "$out"   # C float suffixes stripped
if grep -q '0\.0f' "$out"; then
    echo "k2go left a C float suffix in Go output" >&2
    exit 1
fi
if grep -q 'TODO k2go' "$out"; then
    echo "k2go left a TODO lowering in Go output:" >&2
    grep 'TODO k2go' "$out" >&2
    exit 1
fi
unqualified_runtime_calls="$(
    rg -n '^\t+(BeginFrame|EndFrame|Text|Button|TextField|TextArea|Row|Column|Stack|Dropdown|Progress|Rect|Scroll|EndScroll|Open|Close)\(' "$out" || true
)"
if [ -n "$unqualified_runtime_calls" ]; then
    echo "k2go emitted unqualified runtime calls; generated Go must use kryon.<Name>:" >&2
    echo "$unqualified_runtime_calls" >&2
    exit 1
fi

# '#extern' host bridge: interface, setter, converted calls.
grep -q 'type ValidHost interface' "$out"
grep -q 'func SetValidHost(host ValidHost)' "$out"
grep -q 'QueryJobs(Since int64, Limit int32) int32' "$out"
grep -q 'LabelText(I int32) string' "$out"
grep -q 'TabLabels() \[\]string' "$out"
grep -q 'StoreSecret(Secret string, Site string, Login string, A int32, B int32, C int32, D int32, E int32, F int32, Exclude string) int32' "$out"
if grep -q 'ScaleUIPx(Value int32)' "$out"; then
    echo "k2go placed a direct Go extern in the host interface" >&2
    exit 1
fi
grep -q 'import "C"' "$cgo"
grep -q 'extern int abs(int value);' "$cgo"
grep -q 'func k2goCValidCAbs(value int32) int32' "$cgo"
grep -q 'return int32(C.abs(C.int(value)))' "$cgo"
grep -q 'validHost.QueryJobs(int64(0), int32(10))' "$out"
grep -q 'validHost.LabelText(int32(st.Tab))' "$out"
grep -q 'validHost.StoreSecret(kryon.CString(st.FieldText\[:\]), kryon.CString(st.AreaText\[:\]), "literal", int32(1), int32(2), int32(3), int32(4), int32(5), int32(6), kryon.CString(st.AreaText\[:\]))' "$out"
grep -q 'kryonpkg.ScaleUIPx(int32(16))' "$out"
grep -q 'k2goCValidCAbs(int32(-8))' "$out"
grep -q 'Helper_HelperValue()' "$out"
grep -q 'Valid_LocalValue(st)' "$out"
grep -q 'kryonpkg.QueueText(kryon.CString(value\[:\]))' "$out"

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
grep -q 'func Hierarchy_Main(viewport kryon.Rectangle)' "$hier"
grep -q 'kryon.Screen(kryon.ColumnProps{Bounds: viewport, Padding: 8, Key: kryon.Key("Main/root")})' "$hier"
grep -q 'kryon.Column(kryon.ColumnProps{Gap: 4, Key: kryon.Key("Main/root/body")})' "$hier"
grep -q 'viewport := kryon.Rectangle{Width: float32(kryon.GetScreenWidth()), Height: float32(kryon.GetScreenHeight())}' "$hier"
grep -q 'Hierarchy_Main(viewport)' "$hier"
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
grep -q 'Icon(' "$out"
grep -q 'kryon.Picture(kryon.PictureProps{AssetPath: "tiles/tile.png"' "$out"
grep -q 'kryon.Paragraph(kryon.ParagraphSpec{Text: "Rich text"' "$out"
grep -q 'kryon.IconButton(kryon.IconButtonProps{' "$out"
grep -q 'FocusID: 3' "$out"
grep -q 'kryon.Href(kryon.HrefProps{' "$out"
grep -q 'kryon.SetPageTitle("Kryon Page")' "$out"
grep -q 'kryon.ReplaceRoute("/page#top")' "$out"
grep -q 'var route_version int32 = kryon.GetRouteVersion()' "$out"
grep -q 'kryon.Page(kryon.PageProps{' "$out"
grep -q 'CanonicalURL: "https://example.com/page"' "$out"
grep -q 'kryon.Heading(kryon.HeadingProps{' "$out"
grep -q 'kryon.ParagraphText(kryon.ParagraphTextProps{' "$out"
grep -q 'kryon.Link(kryon.LinkProps{' "$out"
grep -q 'kryon.PagePicture(kryon.PictureProps{AssetPath: "hero.png"' "$out"
grep -q 'kryon.Section(kryon.SectionProps{' "$out"
grep -q 'kryon.Flow(kryon.FlowProps{' "$out"
grep -q 'kryon.PageGrid(kryon.GridProps{' "$out"
grep -q 'Slider(9,' "$out"
grep -q 'Toggle(10,' "$out"
grep -q 'kryon.Stack(kryon.ColumnProps{' "$out"
grep -q 'Key("smoke-stack")' "$out"
grep -q 'kryon.Row(kryon.RowProps{' "$out"
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
grep -q 'kryon.Radio(kryon.RadioButtonProps{.*Label: "one".*Checked: st.Pick == 1' "$out"
grep -q 'kryon.Spinbox(kryon.SpinboxProps{.*Value: &st.SliderVal' "$out"
grep -q 'kryon.Combobox(kryon.ComboboxProps{.*Options: choices\[:\].*SelectedIndex: &st.Pick' "$out"
grep -q 'kryon.LabelFrame(kryon.LabelFrameProps{' "$out"
grep -q 'kryon.Notebook(kryon.NotebookProps{' "$out"
grep -q 'kryon.ListBox(kryon.ListBoxProps{' "$out"
grep -q 'kryon.TreeView(kryon.TreeViewProps{.*Items: tree_items\[:\].*SelectedID: &st.Pick' "$out"
grep -q 'kryon.PlotLines(kryon.PlotProps{.*Values: plot_values\[:\].*ValueCount: 4' "$out"
grep -q 'kryon.PlotHistogram(kryon.PlotProps{.*Values: plot_values\[:\].*Offset: 1' "$out"
grep -q 'kryon.DragFloat(kryon.DragFloatProps{.*Values: plot_values\[:\].*ValueCount: 2' "$out"
grep -q 'kryon.DragInt(kryon.DragIntProps{.*Values: nums\[:\].*ValueCount: 2' "$out"
grep -q 'kryon.DragFloatRange2(kryon.DragFloatRange2Props{.*CurrentMin: &st.DragFloatMin.*CurrentMax: &st.DragFloatMax.*FormatMax: "max %.1f"' "$out"
grep -q 'kryon.DragIntRange2(kryon.DragIntRange2Props{.*CurrentMin: &st.DragIntMin.*CurrentMax: &st.DragIntMax.*FormatMax: "max %d"' "$out"
grep -q 'kryon.SliderFloat(kryon.SliderFloatProps{.*Values: plot_values\[:\].*ValueCount: 2' "$out"
grep -q 'kryon.SliderInt(kryon.SliderIntProps{.*Values: nums\[:\].*ValueCount: 2' "$out"
grep -q 'kryon.VSliderFloat(kryon.SliderFloatProps{' "$out"
grep -q 'kryon.VSliderInt(kryon.SliderIntProps{' "$out"
grep -q 'kryon.SliderAngle(kryon.SliderAngleProps{.*Value: &plot_values\[0\]' "$out"
grep -q 'kryon.InputFloat(kryon.InputFloatProps{.*Values: plot_values\[:\].*ValueCount: 2' "$out"
grep -q 'kryon.InputInt(kryon.InputIntProps{.*Values: nums\[:\].*ValueCount: 2' "$out"
grep -q 'kryon.InputDouble(kryon.InputDoubleProps{.*Values: plot_doubles\[:\].*ValueCount: 2' "$out"
grep -q 'kryon.SmallButton(kryon.ButtonProps{' "$out"
grep -q 'kryon.InvisibleButton(kryon.InvisibleButtonProps{' "$out"
grep -q 'kryon.ArrowButton(kryon.ArrowButtonProps{.*Direction: 1' "$out"
grep -q 'kryon.Bullet(kryon.NewRectangle' "$out"
grep -q 'kryon.Separator(kryon.NewRectangle.*0)' "$out"
grep -q 'kryon.ColorEdit3(kryon.ColorEditProps{.*Values: edit_color\[:\].*ValueCount: 3' "$out"
grep -q 'kryon.ColorEdit4(kryon.ColorEditProps{.*Values: edit_color\[:\].*ValueCount: 4' "$out"
grep -q 'kryon.ColorPicker3(kryon.ColorEditProps{' "$out"
grep -q 'kryon.ColorPicker4(kryon.ColorEditProps{' "$out"
grep -q 'kryon.ColorButton(kryon.ColorButtonProps{.*Color: kryon.Color{R: 51, G: 102, B: 153, A: 204}' "$out"
grep -q 'kryon.TextColored("colored"' "$out"
grep -q 'kryon.TextDisabled("disabled"' "$out"
grep -q 'kryon.TextWrapped("wrapped helper text"' "$out"
grep -q 'kryon.LabelText("Status", "Ready"' "$out"
grep -q 'kryon.BulletText("bullet text"' "$out"
grep -q 'kryon.MenuBar(46, kryon.NewRectangle.*menus\[:\], 1, &st.MenuOpen)' "$out"
grep -q 'kryon.PopupMenu(47, .*menu_items\[:\], 2)' "$out"
grep -q 'kryon.ContextMenu(kryon.ContextMenuProps{.*Items: menu_items\[:\].*Open: &st.ContextOpen' "$out"
grep -q 'kryon.Tooltip(kryon.TooltipProps{.*Text: "Helpful text".*MaxWidth: kryon.ScaleUIPx(160)' "$out"
grep -q 'kryon.Selectable(kryon.SelectableProps{.*Selected: &st.SelectedRow' "$out"
grep -q 'kryon.CheckboxFlags(kryon.CheckboxFlagsProps{.*Flags: &st.FeatureFlags.*FlagsValue: 4' "$out"
grep -q 'kryon.ImageWithBg(kryon.ImageWithBgProps{Picture: choice_picture' "$out"
grep -q 'kryon.ImageButton(kryon.ImageButtonProps{Picture: choice_picture.*ID: 51' "$out"
grep -q 'kryon.SeparatorText(kryon.SeparatorTextProps{.*Label: "Section".*Font: kryon.Text14' "$out"
grep -q 'kryon.Collapsible(kryon.CollapsibleProps{' "$out"
grep -q 'SetThemeDarkMode(1' "$out"
grep -q 'SetCurrentTheme(0, 1)' "$out"

# typed declarations, arrays, and goto/labels lower for real now
grep -q 'var scalar int32 = 5' "$out"
grep -q 'var nums = \[4\]int32{1,2,3,4}' "$out"
grep -q 'var choices = \[3\]string{"Alpha","Beta","Gamma"}' "$out"
grep -q 'kryon.Dropdown(11, kryon.ScaleUIPx(4), kryon.ScaleUIPx(210), kryon.ScaleUIPx(120), kryon.ScaleUIPx(24), choices\[:\], 3, &st.Pick)' "$out"
grep -q 'retry:$' "$out"
grep -q 'goto retry' "$out"

# The generated source must compile against Kryon's native Go runtime. Textual
# greps alone previously allowed syntactically invalid Go to pass unnoticed.
cat > "$work/out/go.mod" <<EOF
module kryon-generated-smoke

go 1.25.0

require (
	github.com/waozixyz/kryon/go/kryon v0.0.0
	golang.org/x/image v0.45.0 // indirect
	golang.org/x/sys v0.47.0 // indirect
	golang.org/x/text v0.41.0 // indirect
)
replace github.com/waozixyz/kryon/go/kryon => $root/go/kryon
EOF
cp "$root/go/kryon/go.sum" "$work/out/go.sum"
(cd "$work/out" && GOCACHE="${GOCACHE:-$work/go-cache}" go test ./...)

cat > "$work/src/assert_fail.kry" <<'EOF'
#import "kryon.h"
#assert 2 * 2 == 5, "k2go constant assertion failed"
EOF

if "$k2go" --root "$work" -o "$work/out" "$work/src/assert_fail.kry" 2>"$work/assert_fail.err"; then
    echo "false constant #assert did not fail during k2go parsing" >&2
    exit 1
fi
grep -q 'k2go constant assertion failed' "$work/assert_fail.err"

cat > "$work/src/assert_unknown.kry" <<'EOF'
#import "kryon.h"
WEB :: #defined(PLATFORM_WEB)
#assert WEB, "k2go unresolved assertion"
EOF

if "$k2go" --root "$work" -o "$work/out" "$work/src/assert_unknown.kry" 2>"$work/assert_unknown.err"; then
    echo "unresolved #assert did not fail in k2go" >&2
    exit 1
fi
grep -q 'unresolved #assert is not supported by the Go backend' "$work/assert_unknown.err"

echo "k2go syntax ok"
