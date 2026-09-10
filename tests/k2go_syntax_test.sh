#!/bin/sh
# k2go syntax test — verifies the Kir-based .kry->Go pipeline output.
set -eu

k2go=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2go build/*/bin/k2go 2>/dev/null | head -1)}
work=${TMPDIR:-/tmp}/kryon-k2go-syntax-test.$$
root=$(pwd)

cleanup() { [ "${KEEP_K2GO_SYNTAX_WORK:-0}" = 1 ] || rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -f "$k2go" ]; then
    echo "k2go not found: $k2go" >&2
    exit 1
fi

mkdir -p "$work/src" "$work/out" "$work/hierarchy-out" "$work/pure-out"

cat > "$work/src/valid.kry" <<'EOF'
#import "kryon.h"
#import "src/helper"

ANSWER :: #run 21 * 2
cast_operand_index :: () -> int {
    return 2
}

cast_operand_value :: () -> float {
    values: [3] int = {10, 20, 30}
    point: Vector2 = (Vector2){5, 7}
    return (float)values[cast_operand_index() - 1] + (float)cast_operand_index() * 3.0f + (float)point.x
}

typed_geometry_value :: () -> float {
    values: [3] int = {10, 20, 30}
    point: Vector2 = {values[0], values[1] + 2}
    bounds: Rectangle = {values[0], values[1], values[2], cast_operand_index()}
    partial: Rectangle = {values[0], values[1]}
    empty: Vector2 = {}
    return point.x + point.y + bounds.width + bounds.height + partial.x + partial.y + partial.width + partial.height + empty.x + empty.y
}

gradient_end_style :: () -> Style {
    return (Style){StyleBackgroundEnd, BLANK, BLANK, BLANK, BLANK,
        0, 0, 1, 0, 0, 0, 0, 0, (Vector2){0, 0}, (Color){17, 34, 51, 0}}
}

Badge :: struct {
    count: i32
    label: const char*
    disabled: i32
    text_size: i32
    id: i32
}

source_badge :: () -> Badge {
    return (Badge){17, "Ready", 2, 23, 41}
}

designated_badge :: () -> Badge {
    return (Badge){.count=19, .label="Custom", .disabled=3, .text_size=29, .id=43}
}

updated_badge_id :: () -> i32 {
    badge: Badge = source_badge()
    badge.id += 2
    return badge.id
}
#assert ANSWER == 42, "k2go #run assertion failed"
#assert 1 + 1 == 2, "k2go fixture assertion failed"
query_jobs :: (since: long, limit: int) -> int #extern "smoke.QueryJobs"
label_text :: (i: int) -> char* #extern "smoke.LabelText"
tab_labels :: () -> char** #extern "smoke.TabLabels"
store_secret :: (secret: const char*, site: const char*, login: const char*, a: int, b: int, c: int, d: int, e: int, f: int, exclude: const char*) -> int #extern "smoke.StoreSecret"
direct_scale :: (value: int) -> int #extern "github.com/waozixyz/kryon/go/kryon.Scale"
direct_queue_text :: (value: const char*) #extern "github.com/waozixyz/kryon/go/kryon.QueueText"
c_abs :: (value: int) -> int #extern "host.Abs"

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
    accepted_size: int = 0
    multi_count: int = 0
    multi_anchor: int = -1
    selected_row: int = 0
    feature_flags: int = 1
    closed_tab: int = -1
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
    return Scale(5)
}

relay_text :: (value: [64] char) {
    direct_queue_text(value)
}

App :: () #ui {
    Screen root: {
    ClearBackground(GetThemeBackground())
    menu_items: [2] MenuItem = {{MenuCommand,"Open","Ctrl+O",46,0,0,NULL,0},{MenuCheck,"Grid",NULL,47,0,1,NULL,0}}
    menus: [1] Menu = {{(Rectangle){0,0,0,0},"File",menu_items,2}}
    rich_tabs: [2] Tab = {{"One",(Texture2D){0,0,0,0,0},0,0,(Color){0},0,1},{"Two",(Texture2D){0,0,0,0,0},0,0,(Color){0},0,1}}
    if tab == TAB_JOBS {
        Text((TextProps){.bounds={Scale(10), Scale(20), 0, 0}, .text=label_text(tab), .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    } else {
        Text((TextProps){.bounds={Scale(10), Scale(20), 0, 0}, .text="hello", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    }
    Text((TextProps){.bounds={Scale(10), Scale(38), 0, 0}, .text="small", .font=Text14, .color=GetThemeText(), .wrap=TextWrapNone})
    Text((TextProps){.bounds={Scale(10), Scale(56), 0, 0}, .text="large", .font=Text20, .color=GetThemeText(), .wrap=TextWrapNone})
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
        Rect(Scale(4), Scale(8), Scale(2), Scale(2), GetThemeText())
    }
    guard tab >= 0 {
        return
    }
    tab = TabBar((TabBarProps){.bounds = {Scale(4), Scale(4), Scale(200), Scale(30)}, .tabs = rich_tabs, .count = 2, .selected_index = tab, .id = 140})
    Checkbox(0, Scale(4), Scale(60), "Check", &check)
    Dropdown(1, Scale(4), Scale(80), Scale(120), Scale(30), "a;b;c", &pick)
    Progress((ProgressBarProps){{Scale(4), Scale(120), Scale(100), Scale(10)}, 0, 100, query_jobs(0, 10), ""})
    Scroll(Scale(4), Scale(8), Scale(200), Scale(100), Scale(400), &scroll_off)
    DrawCircleV((Vector2){Scale(120), Scale(120)}, Scale(30), (Color){0x2d, 0x4d, 0x7b, 0xff})
    DrawRing((Vector2){Scale(120), Scale(120)}, Scale(36), Scale(40), 0.0f, 360.0f, 0, (Color){0x70, 0x90, 0xc0, 0xff})
    EndScroll()
    Text((TextProps){.bounds = {Scale(4), Scale(130), Scale(160), Scale(20)}, .text = "in rect", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone, .align = TextAlignCenter, .vertical_align = TextAlignCenter})
    TextLines("one;two;three", 3, Scale(4), &lines_y, Text16, Scale(18), GetThemeText())
    Bevel(Scale(10), Scale(10), Scale(60), Scale(20), GetThemeSurface(), GetThemeButton())
    Icon(2, Scale(200), Scale(10), Scale(24), 3, WHITE)
    Picture((PictureProps){"tiles/tile.png", (Rectangle){Scale(4), Scale(150), Scale(96), Scale(96)}, (Rectangle){0, 0, 0, 0}, (Vector2){0, 0}, 0.0f, WHITE, PICTURE_FIT_CONTAIN})
    Paragraph((ParagraphSpec){.text = "Rich text", .icon_type = 1, .icon_size = Scale(16), .width = Scale(200), .font = Text16, .line_gap = Scale(4), .color = GetThemeText(), .align = TextAlignCenter}, Scale(4), &lines_y)
    IconButton((IconButtonProps){.bounds = {Scale(210), Scale(60), Scale(36), Scale(36)}, .icon_type = 2, .focus_id = 3})
    Href((HrefProps){.bounds = {Scale(210), Scale(110), Scale(90), Scale(24)}, .text = "docs", .href = "https://example.com", .font = Text16, .color = GetThemeLink()})
    SetPageTitle("Kryon Page")
    SetPageDescription("Generated page")
    SetPageCanonicalURL("https://example.com/page")
    SetPageThemeColor((Color){0x11, 0x22, 0x33, 0xff})
    ReplaceRoute("/page#top")
    route_version: int = GetRouteVersion()
    Progress((ProgressBarProps){{Scale(188), Scale(204), Scale(60), Scale(10)}, 0, 10, route_version, ""})
    Page((PageProps){.title = "Kryon Page", .description = "Generated page", .canonical_url = "https://example.com/page", .theme_color = (Color){0x11, 0x22, 0x33, 0xff}, .background = GetThemeBackground(), .gap = Scale(6), .padding = Scale(8)})
    Heading((HeadingProps){.text = "Welcome", .level = 1, .font = Text24, .color = GetThemeText()})
    ParagraphText((ParagraphTextProps){.bounds = {0, 0, Scale(160), 0}, .text = "Body", .font = Text16, .color = GetThemeText(), .line_gap = Scale(4)})
    Link((LinkProps){.bounds = {0, 0, Scale(90), Scale(24)}, .text = "More", .href = "/more", .font = Text16, .color = GetThemeLink()})
    PagePicture((PictureProps){"hero.png", (Rectangle){0, 0, Scale(96), Scale(48)}, (Rectangle){0, 0, 0, 0}, (Vector2){0, 0}, 0.0f, WHITE, PICTURE_FIT_COVER}, "Hero")
    End()
    Section((SectionProps){.label = "Details", .gap = Scale(4), .padding = Scale(4)})
    Heading((HeadingProps){.text = "Details", .level = 2})
    End()
    Flow((FlowProps){.bounds = {Scale(4), Scale(176), Scale(180), Scale(24)}, .gap = Scale(4)})
    Text((TextProps){.bounds={0, 0, 0, 0}, .text="flow", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    End()
    Grid((GridProps){.bounds = {Scale(4), Scale(204), Scale(180), Scale(40)}, .columns = 2, .gap = Scale(4), .padding = Scale(4)})
    Text((TextProps){.bounds={0, 0, 0, 0}, .text="g1", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    Text((TextProps){.bounds={0, 0, 0, 0}, .text="g2", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    End()
    Slider(9, Scale(4), Scale(170), Scale(180), "S", 0, 100, &slider_val, "%", nil)
    Toggle(10, Scale(200), Scale(170), Scale(120), Scale(32), &toggle_val, "Off", "On")
    Stack smoke_stack: {
        bounds = {Scale(4), Scale(190), Scale(100), Scale(40)}
        key = Key("smoke-stack")
        Rect(Scale(4), Scale(190), Scale(100), Scale(40), Fade(GetThemeSurface(), 0.5f), GetThemeButton())
    }
    Row smoke_row: {
        bounds = {Scale(120), Scale(190), Scale(100), Scale(40)}
    }
    Modal("Title", "Message", "Cancel", "OK")
    TitleBar("Smoke", Scale(32))
    TopNav((TopNavProps){.id = 2, .x = 0, .y = 0, .width = Scale(320), .height = Scale(36), .title = "Top", .options = "x;y", .option_count = 2, .selected_index = &pick})
    Toolbar((ToolbarProps){.id = 1, .x = 0, .y = Scale(40), .width = Scale(300), .height = Scale(36), .draw_menu = 1, .options = "a;b", .option_count = 2})
    BottomNav((BottomNavProps){.view_width = Scale(320), .view_height = Scale(240), .count = 0, .height = Scale(56)})
    scalar: int = 5
    nums: [4] int = {1, 2, 3, 4}
    plot_values: [4] float = {0.0f, 0.25f, 1.0f, 0.5f}
    plot_doubles: [2] double = {1.0, 2.0}
    edit_color: [4] float = {0.2f, 0.4f, 0.6f, 0.8f}
    choices: [3] const char * = {"Alpha","Beta","Gamma"}
    tree_items: [2] UITreeItem = {{"Root",0,1,1,0},{"Leaf",1,2,0,1}}
    BeginDisabled(1)
    BeginScroll((Rectangle){0,0,320,240}, 400, &scroll_off)
    Button((ButtonProps){.bounds = {Scale(150), Scale(8), Scale(90), Scale(28)}, .label = "GB", .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft, .font = Text16, .id = 20})
    EndDisabled()
    EndScroll()
    Button((ButtonProps){.bounds = {Scale(150), Scale(40), Scale(90), Scale(28)}, .label = "TB", .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft, .font = Text16, .id = 21})
    Dropdown(22, Scale(150), Scale(70), Scale(90), Scale(24), choices, 3, &pick)
    frame_box: FrameBox = BeginFrameBox((Rectangle){Scale(4), Scale(392), Scale(160), Scale(80)}, Scale(8), Scale(8), Scale(4))
    packed: Rectangle = FramePack(&frame_box, SideTop, Scale(24))
    layout_grid: GridFrame = {frame_box.bounds, 2, 2, Scale(4), Scale(4), Scale(0), Scale(0)}
    grid_cell: Rectangle = GridCell(layout_grid, 1, 1, 1, 1)
    placed: Rectangle = Place(packed, Scale(4), Scale(4), Scale(24), Scale(12))
    CanvasGrid(grid_cell, 8, GetThemeIcon())
    CanvasGrid(placed, 4, GetThemeButton())
    canvas_result: CanvasResult = BeginCanvas((Canvas){{Scale(180), Scale(392), Scale(100), Scale(64)}, &canvas_scroll_x, &canvas_scroll_y, &canvas_zoom})
    DrawCircleV(canvas_result.world, Scale(3), GetThemeSurface())
    EndCanvas((Canvas){{Scale(180), Scale(392), Scale(100), Scale(64)}, &canvas_scroll_x, &canvas_scroll_y, &canvas_zoom})
    Slider(23, Scale(250), Scale(8), Scale(60), "", 0, 10, &slider_val, "", nil)
    CanvasGrid((Rectangle){Scale(4), Scale(230), Scale(60), Scale(40)}, 8, GetThemeIcon())
    SelectableText("select me", Scale(150), Scale(100), Text16, GetThemeText())
    ShowToast("toast from kry")
    TextField((TextFieldProps){.bounds = {Scale(150), Scale(124), Scale(90), Scale(24)}, .text = field_text, .text_size = sizeof(field_text), .cursor_position = &field_cursor, .focused = NULL, .max_codepoints = 63, .font = Text16, .focus_id = 30})
    TextArea((TextAreaProps){.bounds = {Scale(250), Scale(124), Scale(90), Scale(48)}, .text = area_text, .text_size = sizeof(area_text), .cursor_position = &area_cursor, .focused = NULL, .scroll_y = &area_scroll, .max_codepoints = 127, .font = Text16, .line_gap = Scale(4), .focus_id = 31, .placeholder = "Notes", .syntax = SyntaxNone})
    store_secret(field_text, area_text, "literal", 1, 2, 3, 4, 5, 6, area_text)
    Text((TextProps){.bounds={Scale(150), Scale(152), 0, 0}, .text="ro", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    Radio((RadioButtonProps){{Scale(4), Scale(270), Scale(120), Scale(24)}, "one", 1, pick == 1, 0})
    Spinbox((SpinboxProps){{Scale(140), Scale(270), Scale(90), Scale(28)}, 24, 0, 10, 1, &slider_val, 0, ""})
    Combobox((ComboboxProps){{Scale(240), Scale(270), Scale(70), Scale(28)}, 25, choices, 3, &pick, 0})
    LabelFrame((LabelFrameProps){.bounds = {Scale(4), Scale(300), Scale(120), Scale(50)}, .title = "frame"})
    Notebook((NotebookProps){.bounds = {Scale(140), Scale(300), Scale(120), Scale(50)}, .tabs = choices[:], .selected_index = &pick})
    ListBox((ListBoxProps){.bounds = {Scale(280), Scale(300), Scale(60), Scale(50)}, .id = 26, .items = choices[:], .selected_index = &pick})
    TreeView((TreeViewProps){.bounds = {Scale(280), Scale(356), Scale(80), Scale(50)}, .id = 27, .items = tree_items, .item_count = 2, .selected_id = &pick})
    Collapsible((CollapsibleProps){.bounds = {Scale(4), Scale(360), Scale(120), Scale(30)}, .label = "sect", .open = NULL})
    SetThemeDarkMode(1)
    SetCurrentTheme(0, 1)
    Dropdown(11, Scale(4), Scale(210), Scale(120), Scale(24), choices, 3, &pick)
    Progress((ProgressBarProps){{Scale(140), Scale(210), Scale(100), Scale(10)}, 0, 100, nums[0] + scalar, ""})
    PlotLines((PlotProps){.bounds = {Scale(250), Scale(210), Scale(100), Scale(40)}, .label = "Lines", .values = plot_values, .value_count = 4, .scale_min = 0.0f, .scale_max = 1.0f})
    PlotHistogram((PlotProps){.bounds = {Scale(250), Scale(254), Scale(100), Scale(40)}, .label = "Bars", .values = plot_values, .value_count = 4, .offset = 1})
    DragFloat((DragFloatProps){.bounds = {Scale(250), Scale(298), Scale(100), Scale(28)}, .id = 28, .label = "Float", .values = plot_values, .value_count = 2, .speed = 0.1f, .min = 0.0f, .max = 1.0f})
    DragInt((DragIntProps){.bounds = {Scale(250), Scale(330), Scale(100), Scale(28)}, .id = 29, .label = "Int", .values = nums, .value_count = 2, .speed = 1.0f, .min = 0, .max = 10})
    DragFloatRange2((DragFloatRange2Props){.bounds = {Scale(250),Scale(346),Scale(100),Scale(28)}, .id = 52, .label = "Float range", .current_min = &drag_float_min, .current_max = &drag_float_max, .speed = 0.1f, .min = 0.0f, .max = 10.0f, .format_max = "max %.1f"})
    DragIntRange2((DragIntRange2Props){.bounds = {Scale(250),Scale(378),Scale(100),Scale(28)}, .id = 53, .label = "Int range", .current_min = &drag_int_min, .current_max = &drag_int_max, .min = 0, .max = 10, .format_max = "max %d"})
    SliderFloat((SliderFloatProps){.bounds = {Scale(250), Scale(362), Scale(100), Scale(28)}, .id = 30, .label = "Slider float", .values = plot_values, .value_count = 2, .min = 0.0f, .max = 1.0f})
    SliderInt((SliderIntProps){.bounds = {Scale(250), Scale(394), Scale(100), Scale(28)}, .id = 31, .label = "Slider int", .values = nums, .value_count = 2, .min = 0, .max = 10})
    VSliderFloat((SliderFloatProps){.bounds = {Scale(362), Scale(298), Scale(28), Scale(100)}, .id = 32, .values = plot_values, .value_count = 1, .min = 0.0f, .max = 1.0f})
    VSliderInt((SliderIntProps){.bounds = {Scale(394), Scale(298), Scale(28), Scale(100)}, .id = 33, .values = nums, .value_count = 1, .min = 0, .max = 10})
    SliderAngle((SliderAngleProps){.bounds = {Scale(250), Scale(426), Scale(100), Scale(28)}, .id = 34, .value = &plot_values[0], .min_degrees = -180.0f, .max_degrees = 180.0f})
    InputFloat((InputFloatProps){.bounds = {Scale(250), Scale(458), Scale(100), Scale(28)}, .id = 35, .values = plot_values, .value_count = 2, .step = 0.1f, .step_fast = 1.0f})
    InputInt((InputIntProps){.bounds = {Scale(250), Scale(490), Scale(100), Scale(28)}, .id = 36, .values = nums, .value_count = 2, .step = 1, .step_fast = 10})
    InputDouble((InputDoubleProps){.bounds = {Scale(250), Scale(522), Scale(100), Scale(28)}, .id = 37, .values = plot_doubles, .value_count = 2, .step = 0.01, .step_fast = 1.0})
    Button((ButtonProps){.bounds = {Scale(250), Scale(554), Scale(70), Scale(24)}, .label = "Small", .size = ControlSizeSmall, .id = 38})
    InvisibleButton((InvisibleButtonProps){.bounds = {Scale(324), Scale(554), Scale(30), Scale(24)}, .id = 39})
    ArrowButton((ArrowButtonProps){.bounds = {Scale(358), Scale(554), Scale(30), Scale(24)}, .id = 40, .direction = 1})
    Bullet((Rectangle){Scale(392), Scale(554), Scale(20), Scale(20)})
    Separator((Rectangle){Scale(250), Scale(582), Scale(160), Scale(4)}, 0)
    ColorEdit3((ColorEditProps){.bounds = {Scale(250), Scale(590), Scale(160), Scale(28)}, .id = 41, .values = edit_color, .value_count = 3})
    ColorEdit4((ColorEditProps){.bounds = {Scale(250), Scale(622), Scale(160), Scale(28)}, .id = 42, .values = edit_color, .value_count = 4})
    ColorPicker3((ColorEditProps){.bounds = {Scale(250), Scale(654), Scale(70), Scale(130)}, .id = 43, .values = edit_color, .value_count = 3})
    ColorPicker4((ColorEditProps){.bounds = {Scale(324), Scale(654), Scale(70), Scale(130)}, .id = 44, .values = edit_color, .value_count = 4})
    ColorButton((ColorButtonProps){.bounds = {Scale(398), Scale(654), Scale(60), Scale(28)}, .id = 45, .label = "Tint", .color = (Color){51,102,153,204}})
    Text((TextProps){.bounds = {Scale(250), Scale(690), 0, 0}, .text = "colored", .font = Text16, .color = {220,60,80,255}, .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(250), Scale(714), 0, 0}, .text = "disabled", .font = Text16, .wrap = TextWrapNone, .disabled = 1})
    Text((TextProps){.bounds = {Scale(250),Scale(738),Scale(160),Scale(40)}, .text = "wrapped text", .font = Text16, .color = GetThemeText()})
    LabelText("Status", "Ready", (Rectangle){Scale(250),Scale(782),Scale(160),Scale(20)}, Text16, GetThemeText())
    BulletText("bullet text", (Rectangle){Scale(250),Scale(806),Scale(160),Scale(20)}, Text16, GetThemeText())
    ValueBool("Enabled", check != 0, (Rectangle){Scale(250),Scale(1030),Scale(120),Scale(20)}, Text14, GetThemeText())
    ValueInt("Count", scalar, (Rectangle){Scale(250),Scale(1054),Scale(120),Scale(20)}, Text14, GetThemeText())
    ValueUInt("Mask", 42, (Rectangle){Scale(250),Scale(1078),Scale(120),Scale(20)}, Text14, GetThemeText())
    ValueFloat("Rate", plot_values[0], "%.1f", (Rectangle){Scale(250),Scale(1102),Scale(120),Scale(20)}, Text14, GetThemeText())
    MenuBar(46, (Rectangle){Scale(4),Scale(834),Scale(220),Scale(30)}, menus, 1, &menu_open)
    PopupMenu(47, Scale(4), Scale(868), menu_items, 2)
    ContextMenu((ContextMenuProps){.id = 48, .trigger = {Scale(230),Scale(834),Scale(100),Scale(60)}, .items = menu_items, .item_count = 2, .open = &context_open, .x = &context_x, .y = &context_y})
    if BeginPopup((PopupProps){.bounds={Scale(220),Scale(934),Scale(176),Scale(42)},.id=58,.trigger={Scale(230),Scale(900),Scale(100),Scale(30)},.flags=PopupTooltip}) {
        Text((TextProps){.bounds={Scale(228),Scale(944),Scale(160),Scale(20)},.text="Helpful text",.font=Text14,.color=GetThemeText(),.wrap=TextWrapNone})
        EndPopup()
    }
    choice_picture: PictureProps = {"tiles/tile.png",(Rectangle){Scale(250),Scale(934),Scale(48),Scale(32)},(Rectangle){0,0,0,0},(Vector2){0,0},0.0f,WHITE,PICTURE_FIT_CONTAIN}
    Selectable((SelectableProps){.bounds = {Scale(4),Scale(934),Scale(120),Scale(28)}, .id = 49, .label = "Choice", .selected = &selected_row})
    CheckboxFlags((CheckboxFlagsProps){.bounds = {Scale(4),Scale(966),Scale(160),Scale(28)}, .id = 50, .label = "Feature", .flags = &feature_flags, .flags_value = 4})
    ImageWithBg((ImageWithBgProps){.picture = choice_picture, .background = GetThemeSurface()})
    ImageButton((ImageButtonProps){.picture = choice_picture, .background = GetThemeButton(), .id = 51})
    SeparatorText((SeparatorTextProps){.bounds = {Scale(250),Scale(1000),Scale(160),Scale(24)}, .label = "Section", .font = Text14})
    TabItemButton((TabItemButtonProps){.bounds = {Scale(250),Scale(1130),Scale(60),Scale(28)}, .id = 54, .label = "+", .font = Text14})
    ClosableTabBar((ClosableTabBarProps){.bounds = {Scale(314),Scale(1130),Scale(180),Scale(28)}, .tabs = rich_tabs, .count = 2, .selected_index = &tab, .font = Text14, .closed_index = &closed_tab})
    DragDropSource((DragDropSourceProps){.bounds = {Scale(250),Scale(1162),Scale(80),Scale(28)}, .id = 55, .type = "TEXT", .data = field_text, .data_size = 64})
    DragDropTarget((DragDropTargetProps){.bounds = {Scale(334),Scale(1162),Scale(120),Scale(28)}, .id = 56, .type = "TEXT", .output = area_text, .output_size = 128, .accepted_size = &accepted_size})
    MultiSelectList((MultiSelectListProps){.bounds = {Scale(250),Scale(1194),Scale(180),Scale(84)}, .id = 57, .items = choices, .item_count = 3, .selected = nums, .selected_count = &multi_count, .anchor = &multi_anchor, .row_height = 28})
    Progress((ProgressBarProps){{Scale(140), Scale(224), Scale(100), Scale(10)}, 0, 100, direct_scale(16), ""})
    Progress((ProgressBarProps){{Scale(140), Scale(238), Scale(100), Scale(10)}, 0, 100, helper_value(), ""})
    Progress((ProgressBarProps){{Scale(140), Scale(252), Scale(100), Scale(10)}, 0, 100, c_abs(-8), ""})
    Progress((ProgressBarProps){{Scale(140), Scale(266), Scale(100), Scale(10)}, 0, 100, local_value(), ""})
    relay_text(field_text)
    TextLines("one;two;three", 3, Scale(4), &lines_y, Text16, Scale(18), GetThemeText())
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
            Text((TextProps){.bounds={0, 0, 0, 0}, .text="Hello", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
        }
    }
}
EOF

cat > "$work/src/helper.kry" <<'EOF'
#import "kryon.h"

helper_value :: () -> int {
    return Scale(7)
}
EOF

"$k2go" --root "$work" -o "$work/out" "$work/src/valid.kry" "$work/src/helper.kry"
"$k2go" --root "$work" -o "$work/hierarchy-out" "$work/src/hierarchy.kry"
"$k2go" --root "$work" -o "$work/pure-out" "$work/src/hierarchy.kry"
out="$work/out/valid.go"
helper="$work/out/helper.go"
hier="$work/hierarchy-out/hierarchy.go"
pure="$work/pure-out/hierarchy.go"

[ -f "$out" ] || { echo "k2go produced no output" >&2; exit 1; }
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

cat > "$work/src/c_abi.kry" <<'EOF'
abs :: (x: i32) -> i32 #extern "c.abs"
EOF
if "$k2go" --root "$work" -o "$work/c-abi-out" "$work/src/c_abi.kry" 2>"$work/c-abi.err"; then
    echo "native Go accepted a C ABI import" >&2
    exit 1
fi
grep -q 'c_abi.kry:1:.*native Go cannot import a C ABI symbol' "$work/c-abi.err"
if find "$work/out" -name '*_cgo.go' | grep -q .; then
    echo "generated Go contains a cgo bridge" >&2
    exit 1
fi

# Structural assertions: the declarative subset must translate fully.
grep -q 'package krygen' "$out"
grep -q 'import kryon "github.com/waozixyz/kryon/go/kryon"' "$out"
grep -q 'import kryonpkg "github.com/waozixyz/kryon/go/kryon"' "$out"
grep -q 'ScrollOff int32' "$out"
grep -q 'func main()' "$out"
grep -q 'kryon.BeginFrame()' "$out"
grep -q '&st.ScrollOff' "$out"
grep -q 'kryon.Vector2{X: float32(kryon.Scale(120)), Y: float32(kryon.Scale(120))}' "$out"
grep -q 'kryon.Color{R: uint8(0x2d), G: uint8(0x4d), B: uint8(0x7b), A: uint8(0xff)}' "$out"
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
if grep -q 'Scale(Value int32)' "$out"; then
    echo "k2go placed a direct Go extern in the host interface" >&2
    exit 1
fi
grep -q 'validHost.QueryJobs(int64(0), int32(10))' "$out"
grep -q 'validHost.LabelText(int32(st.Tab))' "$out"
grep -q 'validHost.StoreSecret(kryon.CString(st.FieldText\[:\]), kryon.CString(st.AreaText\[:\]), "literal", int32(1), int32(2), int32(3), int32(4), int32(5), int32(6), kryon.CString(st.AreaText\[:\]))' "$out"
grep -q 'kryonpkg.Scale(int32(16))' "$out"
grep -q 'validHost.Abs(int32(-8))' "$out"
grep -q 'Helper_HelperValue()' "$out"
grep -q 'Valid_LocalValue(st)' "$out"
grep -q 'kryonpkg.QueueText(kryon.CString(value\[:\]))' "$out"

# enums: typed constants with C counter semantics, rewritten at use sites.
grep -q 'type TabMode int32' "$out"
grep -q 'TabModeTAB_OVERVIEW = 0' "$out"
grep -q 'TabModeTAB_NETWORK = TabModeTAB_OVERVIEW + 1' "$out"
grep -q 'TabModeTAB_JOBS = 5' "$out"
grep -q 'TabModeTAB_AFTER = TabModeTAB_JOBS + 1' "$out"
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
grep -q 'Text: "small".*Font: kryon.Text14.*Color: kryon.GetThemeText().*Wrap: kryon.TextWrapNone' "$out"
grep -q 'Text: "large".*Font: kryon.Text20.*Color: kryon.GetThemeText().*Wrap: kryon.TextWrapNone' "$out"

# full whitelisted widget surface: every widget statement must lower and
# compile against the clean package API.
grep -q 'kryon.Text(kryon.TextProps{' "$out"
grep -q 'TextLines(' "$out"
grep -q 'Bevel(' "$out"
grep -q 'Icon(' "$out"
grep -q 'kryon.Picture(kryon.PictureProps{AssetPath: "tiles/tile.png"' "$out"
grep -q 'kryon.Paragraph(kryon.ParagraphSpec{Text: "Rich text"' "$out"
grep -q 'Align: kryon.TextAlignCenter' "$out"
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
grep -q 'kryon.Grid(kryon.GridProps{' "$out"
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
grep -q 'kryon.Button(kryon.ButtonProps{Bounds: kryon.Rectangle{.*Label: "GB"' "$out"
grep -q 'kryon.BeginDisabled((1) != 0)' "$out"
grep -q 'kryon.EndDisabled()' "$out"
grep -q 'kryon.Button(kryon.ButtonProps{Bounds: kryon.Rectangle{.*Label: "TB"' "$out"
grep -q 'Dropdown(22,' "$out"
grep -q 'kryon.BeginFrameBox(kryon.Rectangle{' "$out"
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
grep -q 'Size: kryon.ControlSizeSmall' "$out"
grep -q 'kryon.InvisibleButton(kryon.InvisibleButtonProps{' "$out"
grep -q 'kryon.ArrowButton(kryon.ArrowButtonProps{.*Direction: 1' "$out"
grep -q 'kryon.Bullet(kryon.Rectangle{' "$out"
grep -q 'kryon.Separator(kryon.Rectangle{.*0)' "$out"
grep -q 'kryon.ColorEdit3(kryon.ColorEditProps{.*Values: edit_color\[:\].*ValueCount: 3' "$out"
grep -q 'kryon.ColorEdit4(kryon.ColorEditProps{.*Values: edit_color\[:\].*ValueCount: 4' "$out"
grep -q 'kryon.ColorPicker3(kryon.ColorEditProps{' "$out"
grep -q 'kryon.ColorPicker4(kryon.ColorEditProps{' "$out"
grep -q 'kryon.ColorButton(kryon.ColorButtonProps{.*Color: kryon.Color{R: uint8(51), G: uint8(102), B: uint8(153), A: uint8(204)}' "$out"
grep -q 'Text: "colored"' "$out"
grep -q 'Text: "disabled".*Disabled:' "$out"
grep -q 'Text: "wrapped text"' "$out"
grep -q 'kryon.LabelText("Status", "Ready"' "$out"
grep -q 'kryon.BulletText("bullet text"' "$out"
grep -q 'kryon.ValueBool("Enabled", st.Check != 0' "$out"
grep -q 'kryon.ValueInt("Count", scalar' "$out"
grep -q 'kryon.ValueUInt("Mask", 42' "$out"
grep -q 'kryon.ValueFloat("Rate".*"%.1f"' "$out"
grep -q 'kryon.MenuBar(46, kryon.Rectangle{.*menus\[:\], 1, &st.MenuOpen)' "$out"
grep -q 'kryon.PopupMenu(47, .*menu_items\[:\], 2)' "$out"
grep -q 'kryon.ContextMenu(kryon.ContextMenuProps{.*Items: menu_items\[:\].*Open: &st.ContextOpen' "$out"
grep -q 'kryon.BeginPopup(kryon.PopupProps{.*ID: 58.*Flags: kryon.PopupTooltip' "$out"
grep -q 'Text: "Helpful text"' "$out"
grep -q 'kryon.Selectable(kryon.SelectableProps{.*Selected: &st.SelectedRow' "$out"
grep -q 'kryon.CheckboxFlags(kryon.CheckboxFlagsProps{.*Flags: &st.FeatureFlags.*FlagsValue: 4' "$out"
grep -q 'kryon.ImageWithBg(kryon.ImageWithBgProps{Picture: choice_picture' "$out"
grep -q 'kryon.ImageButton(kryon.ImageButtonProps{Picture: choice_picture.*ID: 51' "$out"
grep -q 'kryon.SeparatorText(kryon.SeparatorTextProps{.*Label: "Section".*Font: kryon.Text14' "$out"
grep -q 'kryon.TabItemButton(kryon.TabItemButtonProps{.*Label: "+".*Font: kryon.Text14' "$out"
grep -q 'kryon.ClosableTabBar(kryon.ClosableTabBarProps{.*Tabs: rich_tabs\[:\].*SelectedIndex: &st.Tab.*ClosedIndex: &st.ClosedTab' "$out"
grep -q 'kryon.DragDropSource(kryon.DragDropSourceProps{.*Data: st.FieldText\[:\]' "$out"
grep -q 'kryon.DragDropTarget(kryon.DragDropTargetProps{.*Output: st.AreaText\[:\].*AcceptedSize: &st.AcceptedSize' "$out"
grep -q 'kryon.MultiSelectList(kryon.MultiSelectListProps{.*Items: choices\[:\].*Selected: nums\[:\].*SelectedCount: &st.MultiCount.*Anchor: &st.MultiAnchor' "$out"
grep -q 'kryon.Collapsible(kryon.CollapsibleProps{' "$out"
grep -q 'SetThemeDarkMode(1' "$out"
grep -q 'SetCurrentTheme(0, 1)' "$out"

# typed declarations, arrays, and goto/labels lower for real now
grep -q 'var scalar int32 = 5' "$out"
grep -q 'var nums = \[4\]int32{1,2,3,4}' "$out"
grep -q 'var choices = \[3\]string{"Alpha","Beta","Gamma"}' "$out"
grep -q 'kryon.Dropdown(11, kryon.Scale(4), kryon.Scale(210), kryon.Scale(120), kryon.Scale(24), choices\[:\], 3, &st.Pick)' "$out"
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
cat > "$work/out/cast_operand_test.go" <<'EOF'
package krygen

import (
    "testing"
    "github.com/waozixyz/kryon/go/kryon"
)

func TestCastOperandPrecedence(t *testing.T) {
    if got := Valid_CastOperandValue(&ValidState{}); got != 31 {
        t.Fatalf("cast consumed the wrong operand: got %g, want 31", got)
    }
}

func TestTypedGeometryInitializers(t *testing.T) {
    if got := Valid_TypedGeometryValue(&ValidState{}); got != 94 {
        t.Fatalf("typed geometry lost field conversions: got %g, want 94", got)
    }
}

func TestPositionalGradientEndpoint(t *testing.T) {
    got := Valid_GradientEndStyle(&ValidState{})
    if got.Fields != kryon.StyleBackgroundEnd || got.BackgroundEnd != (kryon.Color{R: 17, G: 34, B: 51, A: 0}) {
        t.Fatalf("transparent gradient endpoint was dropped: %+v", got)
    }
}

func TestSourceRecordWithoutCompilerNameEntry(t *testing.T) {
    got := Valid_SourceBadge(&ValidState{})
    if got.Count != 17 || got.Label != "Ready" || got.Disabled != 2 || got.TextSize != 23 || got.ID != 41 {
        t.Fatalf("source-defined record fields were dropped: %+v", got)
    }
    named := Valid_DesignatedBadge(&ValidState{})
    if named.Count != 19 || named.Label != "Custom" || named.Disabled != 3 || named.TextSize != 29 || named.ID != 43 {
        t.Fatalf("source-defined fields inherited native widget rules: %+v", named)
    }
    if id := Valid_UpdatedBadgeId(&ValidState{}); id != 43 {
        t.Fatalf("source-defined field access disagrees with initialization: %d", id)
    }
}
EOF
(cd "$work/out" && GOCACHE="${GOCACHE:-$work/go-cache}" go test ./...)

cat > "$work/src/geometry_extra.kry" <<'EOF'
#import "kryon.h"
Main :: () {
    bounds: Rectangle = {1, 2, 3, 4, 5}
}
EOF
if "$k2go" --root "$work" -o "$work/out" "$work/src/geometry_extra.kry" 2>"$work/geometry_extra.err"; then
    echo "extra geometry fields were silently discarded" >&2
    exit 1
fi
grep -q 'no positional field 5 in Rectangle' "$work/geometry_extra.err"

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
