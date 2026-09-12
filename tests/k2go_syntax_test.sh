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
tab_label_text :: (i: int) -> char* #extern "smoke.TabLabelText"
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
    drag_scalar_min: float = 2.0f
    drag_scalar_max: float = 8.0f
    drag_whole_min: int = 2
    drag_whole_max: int = 8
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
    Background(GetThemeBackground())
    menu_items: [2] MenuItem = {{MenuCommand,"Open","Ctrl+O",46,0,0,NULL,0},{MenuCheck,"Grid",NULL,47,0,1,NULL,0}}
    menus: [1] Menu = {{(Rectangle){0,0,0,0},"File",menu_items,2}}
    rich_tabs: [2] Tab = {{"One",(Texture2D){0,0,0,0,0},0,0,(Color){0},0,1},{"Two",(Texture2D){0,0,0,0,0},0,0,(Color){0},0,1}}
    if tab == TAB_JOBS {
        Text((TextProps){.bounds={Scale(10), Scale(20), 0, 0}, .text=tab_label_text(tab), .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
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
    Checkbox((CheckboxProps){.bounds = {Scale(4), Scale(60), Scale(110), Scale(34)}, .id = 0, .label = "Check", .value = &check})
    dropdown_options: [3] const char * = {"a","b","c"}
    Dropdown((DropdownProps){.bounds = {Scale(4), Scale(80), Scale(120), Scale(30)}, .id = 1, .options = dropdown_options, .option_count = 3, .selected_index = &pick})
    Progress((ProgressProps){{Scale(4), Scale(120), Scale(100), Scale(10)}, 0, 100, query_jobs(0, 10), ""})
    Scroll(Scale(4), Scale(8), Scale(200), Scale(100), Scale(400), &scroll_off)
    Circle(Scale(120), Scale(120), Scale(30), (Color){0x2d, 0x4d, 0x7b, 0xff})
    Ring(Scale(120), Scale(120), Scale(36), Scale(40), (Color){0x70, 0x90, 0xc0, 0xff})
    EndScroll()
    Text((TextProps){.bounds = {Scale(4), Scale(130), Scale(160), Scale(20)}, .text = "in rect", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone, .align = TextAlignCenter, .vertical_align = TextAlignCenter})
    Text((TextProps){.bounds = {Scale(4), lines_y, 0, 0}, .text = "one", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(4), lines_y + Scale(18), 0, 0}, .text = "two", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(4), lines_y + Scale(36), 0, 0}, .text = "three", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone})
    lines_y += Scale(54)
    Bevel(Scale(10), Scale(10), Scale(60), Scale(20), GetThemeSurface(), GetThemeButton())
    Icon(2, Scale(200), Scale(10), Scale(24), 3, WHITE)
    Image((ImageProps){"tiles/tile.png", (Rectangle){Scale(4), Scale(150), Scale(96), Scale(96)}, (Rectangle){0, 0, 0, 0}, (Vector2){0, 0}, 0.0f, WHITE, IMAGE_FIT_CONTAIN, (ImageStyle){false,(Color){0},(Color){0},(Color){0},(Color){0},(Color){0},(Color){0},0.0f,0,0,0}})
    Paragraph((ParagraphSpec){.text = "Rich text", .icon_type = 1, .icon_size = Scale(16), .width = Scale(200), .font = Text16, .line_gap = Scale(4), .color = GetThemeText(), .align = TextAlignCenter}, Scale(4), &lines_y)
    Button((ButtonProps){.bounds = {Scale(210), Scale(60), Scale(36), Scale(36)}, .icon_type = 2, .icon_only = true, .id = 3})
    Link((LinkProps){.bounds = {Scale(210), Scale(110), Scale(90), Scale(24)}, .text = "docs", .link = "https://example.com", .font = Text16, .color = GetThemeLink()})
    SetPageTitle("Kryon Page")
    SetPageDescription("Generated page")
    SetPageCanonicalURL("https://example.com/page")
    SetPageThemeColor((Color){0x11, 0x22, 0x33, 0xff})
    ReplaceRoute("/page#top")
    route_version: int = GetRouteVersion()
    Progress((ProgressProps){{Scale(188), Scale(204), Scale(60), Scale(10)}, 0, 10, route_version, ""})
    Page((PageProps){.title = "Kryon Page", .description = "Generated page", .canonical_url = "https://example.com/page", .theme_color = (Color){0x11, 0x22, 0x33, 0xff}, .background = GetThemeBackground(), .gap = Scale(6), .padding = Scale(8)})
    Heading((HeadingProps){.text = "Welcome", .level = 1, .font = Text24, .color = GetThemeText()})
    ParagraphText((ParagraphTextProps){.bounds = {0, 0, Scale(160), 0}, .text = "Body", .font = Text16, .color = GetThemeText(), .line_gap = Scale(4)})
    Link((LinkProps){.bounds = {0, 0, Scale(90), Scale(24)}, .text = "More", .link = "/more", .font = Text16, .color = GetThemeLink()})
    PageImage((ImageProps){"hero.png", (Rectangle){0, 0, Scale(96), Scale(48)}, (Rectangle){0, 0, 0, 0}, (Vector2){0, 0}, 0.0f, WHITE, IMAGE_FIT_COVER, (ImageStyle){false,(Color){0},(Color){0},(Color){0},(Color){0},(Color){0},(Color){0},0.0f,0,0,0}}, "Hero")
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
    slider_values: [1] int = {slider_val}
    Slider((SliderProps){.bounds = {Scale(4), Scale(170), Scale(180), Scale(56)}, .id = 9, .label = "S", .kind = 1, .int_values = slider_values, .value_count = 1, .min = 0.0, .max = 100.0, .format = "%"})
    Toggle((ToggleProps){.bounds = {Scale(200), Scale(170), Scale(120), Scale(34)}, .id = 10, .value = &toggle_val, .off_label = "Off", .on_label = "On"})
    Stack smoke_stack: {
        bounds = {Scale(4), Scale(190), Scale(100), Scale(40)}
        key = Key("smoke-stack")
        Rect(Scale(4), Scale(190), Scale(100), Scale(40), Fade(GetThemeSurface(), 0.5f), GetThemeButton())
    }
    Row smoke_row: {
        bounds = {Scale(120), Scale(190), Scale(100), Scale(40)}
    }
    modal_actions: [2] ModalAction = {{"Cancel",ButtonToneNeutral,ButtonEmphasisSoft,0},{"OK",ButtonToneAccent,ButtonEmphasisFilled,0}}
    Modal((ModalProps){.title = "Title", .message = "Message", .actions = modal_actions, .action_count = 2, .max_width = 360})
    TitleBar((TitleBarProps){.title = "Smoke", .height = Scale(32)})
    Toolbar((ToolbarProps){.id = 2, .x = 0, .y = 0, .width = Scale(320), .height = Scale(36), .draw_menu = 1, .options = "x;y", .option_count = 2, .selected_index = &pick})
    Toolbar((ToolbarProps){.id = 1, .x = 0, .y = Scale(40), .width = Scale(300), .height = Scale(36), .draw_menu = 1, .options = "a;b", .option_count = 2})
    NavigationBar((NavigationBarProps){.view_width = Scale(320), .view_height = Scale(240), .count = 0, .height = Scale(56)})
    scalar: int = 5
    nums: [4] int = {1, 2, 3, 4}
    plot_values: [4] float = {0.0f, 0.25f, 1.0f, 0.5f}
    plot_doubles: [2] double = {1.0, 2.0}
    edit_color: [4] float = {0.2f, 0.4f, 0.6f, 0.8f}
    choices: [3] const char * = {"Alpha","Beta","Gamma"}
    tree_items: [2] TreeItem = {{"Root",0,1,1,0},{"Leaf",1,2,0,1}}
    Button((ButtonProps){.bounds = {Scale(150), Scale(8), Scale(90), Scale(28)}, .label = "GB", .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft, .font = Text16, .id = 20})
    Button((ButtonProps){.bounds = {Scale(150), Scale(40), Scale(90), Scale(28)}, .label = "TB", .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft, .font = Text16, .id = 21})
    Dropdown((DropdownProps){.bounds = {Scale(150), Scale(70), Scale(90), Scale(24)}, .id = 22, .options = choices, .option_count = 3, .selected_index = &pick})
    frame_box: FrameBox = BeginFrameBox((Rectangle){Scale(4), Scale(392), Scale(160), Scale(80)}, Scale(8), Scale(8), Scale(4))
    packed: Rectangle = FramePack(&frame_box, SideTop, Scale(24))
    layout_grid: GridFrame = {frame_box.bounds, 2, 2, Scale(4), Scale(4), Scale(0), Scale(0)}
    grid_cell: Rectangle = GridCell(layout_grid, 1, 1, 1, 1)
    placed: Rectangle = Place(packed, Scale(4), Scale(4), Scale(24), Scale(12))
    CanvasGrid(grid_cell, 8, GetThemeIcon())
    CanvasGrid(placed, 4, GetThemeButton())
    canvas_result: CanvasResult = BeginCanvas((Canvas){{Scale(180), Scale(392), Scale(100), Scale(64)}, &canvas_scroll_x, &canvas_scroll_y, &canvas_zoom})
    Circle((int)canvas_result.world.x, (int)canvas_result.world.y, Scale(3), GetThemeSurface())
    EndCanvas((Canvas){{Scale(180), Scale(392), Scale(100), Scale(64)}, &canvas_scroll_x, &canvas_scroll_y, &canvas_zoom})
    Slider((SliderProps){.bounds = {Scale(250), Scale(8), Scale(60), Scale(56)}, .id = 23, .kind = 1, .int_values = nums, .value_count = 1, .min = 0.0, .max = 10.0})
    CanvasGrid((Rectangle){Scale(4), Scale(230), Scale(60), Scale(40)}, 8, GetThemeIcon())
    SelectableText("select me", Scale(150), Scale(100), Text16, GetThemeText())
    ShowToast("toast from kry")
    TextField((TextFieldProps){.bounds = {Scale(150), Scale(124), Scale(90), Scale(24)}, .text = field_text, .text_size = sizeof(field_text), .cursor_position = &field_cursor, .focused = NULL, .max_codepoints = 63, .font = Text16, .focus_id = 30})
    TextArea((TextAreaProps){.bounds = {Scale(250), Scale(124), Scale(90), Scale(48)}, .text = area_text, .text_size = sizeof(area_text), .cursor_position = &area_cursor, .focused = NULL, .scroll_y = &area_scroll, .max_codepoints = 127, .font = Text16, .line_gap = Scale(4), .focus_id = 31, .placeholder = "Notes", .syntax = SyntaxNone})
    store_secret(field_text, area_text, "literal", 1, 2, 3, 4, 5, 6, area_text)
    Text((TextProps){.bounds={Scale(150), Scale(152), 0, 0}, .text="ro", .font=Text16, .color=GetThemeText(), .wrap=TextWrapNone})
    Radio((RadioProps){.bounds = {Scale(4), Scale(270), Scale(120), Scale(24)}, .label = "one", .id = 1, .checked = pick == 1})
    Spinbox((SpinboxProps){{Scale(140), Scale(270), Scale(90), Scale(28)}, 24, 0, 10, 1, &slider_val, 0, ""})
    Dropdown((DropdownProps){.bounds = {Scale(240), Scale(270), Scale(70), Scale(28)}, .id = 25, .options = choices, .option_count = 3, .selected_index = &pick})
    Fieldset((FieldsetProps){.bounds = {Scale(4), Scale(300), Scale(120), Scale(50)}, .title = "frame"})
    tab_result: int = TabBar((TabBarProps){.bounds = {Scale(140), Scale(300), Scale(120), Scale(30)}, .tabs = rich_tabs, .count = 2, .selected_index = pick, .id = 260})
    if tab_result >= 0 { pick = tab_result }
    ListBox((ListBoxProps){.bounds = {Scale(280), Scale(300), Scale(60), Scale(50)}, .id = 26, .items = choices[:], .selected_index = &pick})
    TreeView((TreeViewProps){.bounds = {Scale(280), Scale(356), Scale(80), Scale(50)}, .id = 27, .items = tree_items, .item_count = 2, .selected_id = &pick})
    Collapsible((CollapsibleProps){.bounds = {Scale(4), Scale(360), Scale(120), Scale(30)}, .label = "sect", .open = NULL})
    SetThemeDarkMode(1)
    SetCurrentTheme(0, 1)
    Dropdown((DropdownProps){.bounds = {Scale(4), Scale(210), Scale(120), Scale(24)}, .id = 11, .options = choices, .option_count = 3, .selected_index = &pick})
    Progress((ProgressProps){{Scale(140), Scale(210), Scale(100), Scale(10)}, 0, 100, nums[0] + scalar, ""})
    Plot((PlotProps){.bounds = {Scale(250), Scale(210), Scale(100), Scale(40)}, .label = "Lines", .values = plot_values, .value_count = 4, .scale_min = 0.0f, .scale_max = 1.0f})
    Plot((PlotProps){.bounds = {Scale(250), Scale(254), Scale(100), Scale(40)}, .label = "Bars", .values = plot_values, .value_count = 4, .offset = 1, .mode = 1})
    Drag((DragProps){.bounds = {Scale(250), Scale(298), Scale(100), Scale(28)}, .id = 28, .label = "Float", .kind = 0, .float_values = plot_values, .value_count = 2, .speed = 0.1f, .min = 0.0, .max = 1.0})
    Drag((DragProps){.bounds = {Scale(250), Scale(330), Scale(100), Scale(28)}, .id = 29, .label = "Int", .kind = 1, .int_values = nums, .value_count = 2, .speed = 1.0f, .min = 0.0, .max = 10.0})
    Drag((DragProps){.bounds = {Scale(250),Scale(346),Scale(100),Scale(28)}, .id = 52, .label = "Float range", .kind = 0, .mode = 1, .float_min = &drag_scalar_min, .float_max = &drag_scalar_max, .speed = 0.1f, .min = 0.0, .max = 10.0, .format_max = "max %.1f"})
    Drag((DragProps){.bounds = {Scale(250),Scale(378),Scale(100),Scale(28)}, .id = 53, .label = "Int range", .kind = 1, .mode = 1, .int_min = &drag_whole_min, .int_max = &drag_whole_max, .min = 0.0, .max = 10.0, .format_max = "max %d"})
    Slider((SliderProps){.bounds = {Scale(250), Scale(362), Scale(100), Scale(28)}, .id = 30, .label = "Slider float", .kind = 0, .float_values = plot_values, .value_count = 2, .min = 0.0, .max = 1.0})
    Slider((SliderProps){.bounds = {Scale(250), Scale(394), Scale(100), Scale(28)}, .id = 31, .label = "Slider int", .kind = 1, .int_values = nums, .value_count = 2, .min = 0.0, .max = 10.0})
    Slider((SliderProps){.bounds = {Scale(362), Scale(298), Scale(28), Scale(100)}, .id = 32, .kind = 0, .float_values = plot_values, .value_count = 1, .min = 0.0, .max = 1.0, .vertical = true})
    Slider((SliderProps){.bounds = {Scale(394), Scale(298), Scale(28), Scale(100)}, .id = 33, .kind = 1, .int_values = nums, .value_count = 1, .min = 0.0, .max = 10.0, .vertical = true})
    Slider((SliderProps){.bounds = {Scale(250), Scale(426), Scale(100), Scale(28)}, .id = 34, .float_value = &plot_values[0], .min = -180.0, .max = 180.0, .angle = true})
    Input((InputProps){.bounds = {Scale(250), Scale(458), Scale(100), Scale(28)}, .id = 35, .kind = 0, .float_values = plot_values, .value_count = 2, .step = 0.1, .step_fast = 1.0})
    Input((InputProps){.bounds = {Scale(250), Scale(490), Scale(100), Scale(28)}, .id = 36, .kind = 1, .int_values = nums, .value_count = 2, .step = 1.0, .step_fast = 10.0})
    Input((InputProps){.bounds = {Scale(250), Scale(522), Scale(100), Scale(28)}, .id = 37, .kind = 2, .double_values = plot_doubles, .value_count = 2, .step = 0.01, .step_fast = 1.0})
    Button((ButtonProps){.bounds = {Scale(250), Scale(554), Scale(70), Scale(24)}, .label = "Small", .size = ControlSizeSmall, .id = 38})
    InvisibleButton((InvisibleButtonProps){.bounds = {Scale(324), Scale(554), Scale(30), Scale(24)}, .id = 39})
    Button((ButtonProps){.bounds = {Scale(358), Scale(554), Scale(30), Scale(24)}, .id = 40, .arrow = true, .direction = 1})
    Bullet((Rectangle){Scale(392), Scale(554), Scale(20), Scale(20)})
    Separator((SeparatorProps){.bounds = {Scale(250), Scale(582), Scale(160), Scale(4)}})
    ColorPicker((ColorPickerProps){.bounds = {Scale(250), Scale(590), Scale(160), Scale(28)}, .id = 41, .values = edit_color, .value_count = 3})
    ColorPicker((ColorPickerProps){.bounds = {Scale(250), Scale(622), Scale(160), Scale(28)}, .id = 42, .values = edit_color, .value_count = 4})
    ColorPicker((ColorPickerProps){.bounds = {Scale(250), Scale(654), Scale(70), Scale(130)}, .id = 43, .values = edit_color, .value_count = 3, .picker = true})
    ColorPicker((ColorPickerProps){.bounds = {Scale(324), Scale(654), Scale(70), Scale(130)}, .id = 44, .values = edit_color, .value_count = 4, .picker = true})
    Button((ButtonProps){.bounds = {Scale(398), Scale(654), Scale(60), Scale(28)}, .id = 45, .label = "Tint", .swatch = true, .swatch_color = (Color){51,102,153,204}})
    Text((TextProps){.bounds = {Scale(250), Scale(690), 0, 0}, .text = "colored", .font = Text16, .color = {220,60,80,255}, .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(250), Scale(714), 0, 0}, .text = "disabled", .font = Text16, .wrap = TextWrapNone, .disabled = 1})
    Text((TextProps){.bounds = {Scale(250),Scale(738),Scale(160),Scale(40)}, .text = "wrapped text", .font = Text16, .color = GetThemeText()})
    Text((TextProps){.bounds = {Scale(250),Scale(782),0,0}, .text = "Status", .font = Text16, .color = Fade(GetThemeText(), 0.72f), .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(310),Scale(782),0,0}, .text = "Ready", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone})
    Bullet((Rectangle){Scale(250),Scale(806),Scale(16),Scale(20)})
    Text((TextProps){.bounds = {Scale(270),Scale(806),0,0}, .text = "bullet text", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone})
    enabled_text: const char* = "false"
    if check != 0 { enabled_text = "true" }
    Text((TextProps){.bounds = {Scale(250),Scale(1030),0,0}, .text = TextFormat("Enabled: %s", enabled_text), .font = Text14, .color = GetThemeText(), .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(250),Scale(1054),0,0}, .text = TextFormat("Count: %d", scalar), .font = Text14, .color = GetThemeText(), .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(250),Scale(1078),0,0}, .text = TextFormat("Mask: %u", 42), .font = Text14, .color = GetThemeText(), .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(250),Scale(1102),0,0}, .text = TextFormat("Rate: %.1f", plot_values[0]), .font = Text14, .color = GetThemeText(), .wrap = TextWrapNone})
    MenuBar(46, (Rectangle){Scale(4),Scale(834),Scale(220),Scale(30)}, menus, 1, &menu_open)
    PopupMenu(47, Scale(4), Scale(868), menu_items, 2)
    ContextMenu((ContextMenuProps){.id = 48, .trigger = {Scale(230),Scale(834),Scale(100),Scale(60)}, .items = menu_items, .item_count = 2, .open = &context_open, .x = &context_x, .y = &context_y})
    if BeginPopup((PopupProps){.bounds={Scale(220),Scale(934),Scale(176),Scale(42)},.id=58,.trigger={Scale(230),Scale(900),Scale(100),Scale(30)},.flags=PopupTooltip}) {
        Text((TextProps){.bounds={Scale(228),Scale(944),Scale(160),Scale(20)},.text="Helpful text",.font=Text14,.color=GetThemeText(),.wrap=TextWrapNone})
        EndPopup()
    }
    choice_image: ImageProps = {"tiles/tile.png",(Rectangle){Scale(250),Scale(934),Scale(48),Scale(32)},(Rectangle){0,0,0,0},(Vector2){0,0},0.0f,WHITE,IMAGE_FIT_CONTAIN,(ImageStyle){false,(Color){0},(Color){0},(Color){0},(Color){0},(Color){0},(Color){0},0.0f,0,0,0}}
    Selectable((SelectableProps){.bounds = {Scale(4),Scale(934),Scale(120),Scale(28)}, .id = 49, .label = "Choice", .selected = &selected_row})
    Checkbox((CheckboxProps){.bounds = {Scale(4),Scale(966),Scale(160),Scale(28)}, .id = 50, .label = "Feature", .flags = &feature_flags, .flags_value = 4})
    choice_image.style = (ImageStyle){.enabled = true, .background = GetThemeSurface()}
    Image(choice_image)
    Button((ButtonProps){.bounds = choice_image.bounds, .image_asset_path = choice_image.asset_path, .image_bounds = choice_image.bounds, .image_source = choice_image.source, .image_origin = choice_image.origin, .image_rotation = choice_image.rotation, .image_tint = choice_image.tint, .image_fit = choice_image.fit, .image_background = GetThemeButton(), .id = 51})
    Separator((SeparatorProps){.bounds = {Scale(250),Scale(1000),Scale(160),Scale(24)}, .label = "Section", .font = Text14})
    Button((ButtonProps){.bounds = {Scale(250),Scale(1130),Scale(60),Scale(28)}, .id = 54, .label = "+", .font = Text14, .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisGhost})
    tab = TabBar((TabBarProps){.bounds = {Scale(314),Scale(1130),Scale(180),Scale(28)}, .tabs = rich_tabs, .count = 2, .selected_index = tab, .font = Text14, .closed_index = &closed_tab, .min_tab_width = Scale(90), .max_tab_width = Scale(90)})
    DragDropSource((DragDropSourceProps){.bounds = {Scale(250),Scale(1162),Scale(80),Scale(28)}, .id = 55, .type = "TEXT", .data = field_text, .data_size = 64})
    DragDropTarget((DragDropTargetProps){.bounds = {Scale(334),Scale(1162),Scale(120),Scale(28)}, .id = 56, .type = "TEXT", .output = area_text, .output_size = 128, .accepted_size = &accepted_size})
    MultiSelectList((MultiSelectListProps){.bounds = {Scale(250),Scale(1194),Scale(180),Scale(84)}, .id = 57, .items = choices, .item_count = 3, .selected = nums, .selected_count = &multi_count, .anchor = &multi_anchor, .row_height = 28})
    Progress((ProgressProps){{Scale(140), Scale(224), Scale(100), Scale(10)}, 0, 100, direct_scale(16), ""})
    Progress((ProgressProps){{Scale(140), Scale(238), Scale(100), Scale(10)}, 0, 100, helper_value(), ""})
    Progress((ProgressProps){{Scale(140), Scale(252), Scale(100), Scale(10)}, 0, 100, c_abs(-8), ""})
    Progress((ProgressProps){{Scale(140), Scale(266), Scale(100), Scale(10)}, 0, 100, local_value(), ""})
    relay_text(field_text)
    Text((TextProps){.bounds = {Scale(4), lines_y, 0, 0}, .text = "one", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(4), lines_y + Scale(18), 0, 0}, .text = "two", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone})
    Text((TextProps){.bounds = {Scale(4), lines_y + Scale(36), 0, 0}, .text = "three", .font = Text16, .color = GetThemeText(), .wrap = TextWrapNone})
    lines_y += Scale(54)
    attempts: int = 0
retry:
    attempts += 1
    if attempts < 3 {
        goto retry
    }
    }
}

DisabledSmoke :: () #ui {
    Disabled scoped_disabled: {
        Button((ButtonProps){.bounds = {Scale(150), Scale(8), Scale(90), Scale(28)}, .label = "GB", .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft, .font = Text16, .id = 120})
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
grep -q 'import kr "github.com/waozixyz/kryon/go/kryon"' "$out"
if grep -q 'import kryonpkg "github.com/waozixyz/kryon/go/kryon"' "$out"; then
    echo "k2go should reuse the kr runtime import for Kryon direct externs" >&2
    exit 1
fi
grep -q 'ScrollOff int32' "$out"
grep -q 'func main()' "$out"
grep -q 'kr.BeginFrame()' "$out"
grep -q '&st.ScrollOff' "$out"
grep -q 'kr.Circle(kr.Scale(120), kr.Scale(120), kr.Scale(30)' "$out"
grep -q 'kr.Ring(kr.Scale(120), kr.Scale(120), kr.Scale(36), kr.Scale(40)' "$out"
grep -q 'kr.Color{R: uint8(0x2d), G: uint8(0x4d), B: uint8(0x7b), A: uint8(0xff)}' "$out"
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
    rg -n '^\t+(BeginFrame|EndFrame|Text|Button|TextField|TextArea|Row|Column|Stack|Dropdown|Progress|Rect|Circle|Ring|Scroll|EndScroll|Open|Close)\(' "$out" || true
)"
if [ -n "$unqualified_runtime_calls" ]; then
    echo "k2go emitted unqualified runtime calls; generated Go must use kr.<Name>:" >&2
    echo "$unqualified_runtime_calls" >&2
    exit 1
fi

# '#extern' host bridge: interface, setter, converted calls.
grep -q 'type ValidHost interface' "$out"
grep -q 'func SetValidHost(host ValidHost)' "$out"
grep -q 'QueryJobs(Since int64, Limit int32) int32' "$out"
grep -q 'TabLabelText(I int32) string' "$out"
grep -q 'TabLabels() \[\]string' "$out"
grep -q 'StoreSecret(Secret string, Site string, Login string, A int32, B int32, C int32, D int32, E int32, F int32, Exclude string) int32' "$out"
if grep -q 'Scale(Value int32)' "$out"; then
    echo "k2go placed a direct Go extern in the host interface" >&2
    exit 1
fi
grep -q 'validHost.QueryJobs(int64(0), int32(10))' "$out"
grep -q 'validHost.TabLabelText(int32(st.Tab))' "$out"
grep -q 'validHost.StoreSecret(kr.CString(st.FieldText\[:\]), kr.CString(st.AreaText\[:\]), "literal", int32(1), int32(2), int32(3), int32(4), int32(5), int32(6), kr.CString(st.AreaText\[:\]))' "$out"
grep -q 'kr.Scale(int32(16))' "$out"
grep -q 'validHost.Abs(int32(-8))' "$out"
grep -q 'Helper_HelperValue()' "$out"
grep -q 'Valid_LocalValue(st)' "$out"
grep -q 'kr.QueueText(kr.CString(value\[:\]))' "$out"

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
grep -q 'func Hierarchy_Main(viewport kr.Rectangle)' "$hier"
grep -q 'kr.Screen(kr.ColumnProps{Bounds: viewport, Padding: 8, Key: kr.Key("Main/root")})' "$hier"
grep -q 'kr.Column(kr.ColumnProps{Gap: 4, Key: kr.Key("Main/root/body")})' "$hier"
grep -q 'viewport := kr.Rectangle{Width: float32(kr.GetScreenWidth()), Height: float32(kr.GetScreenHeight())}' "$hier"
grep -q 'Hierarchy_Main(viewport)' "$hier"
grep -q 'Checkbox(' "$out"
grep -q 'Dropdown(' "$out"
grep -q 'Progress(' "$out"
grep -q 'Rect(' "$out"
grep -q 'Text: "small".*Font: kr.Text14.*Color: kr.GetThemeText().*Wrap: kr.TextWrapNone' "$out"
grep -q 'Text: "large".*Font: kr.Text20.*Color: kr.GetThemeText().*Wrap: kr.TextWrapNone' "$out"

# full whitelisted widget surface: every widget statement must lower and
# compile against the clean package API.
grep -q 'kr.Text(kr.TextProps{' "$out"
grep -q 'kr.Text(kr.TextProps{.*Text: "one"' "$out"
grep -q 'Bevel(' "$out"
grep -q 'Icon(' "$out"
grep -q 'kr.Image(kr.ImageProps{AssetPath: "tiles/tile.png"' "$out"
grep -q 'kr.Paragraph(kr.ParagraphSpec{Text: "Rich text"' "$out"
grep -q 'Align: kr.TextAlignCenter' "$out"
grep -q 'kr.Button(kr.ButtonProps{' "$out"
grep -q 'FocusID: 3' "$out"
grep -q 'kr.Link(kr.LinkProps{' "$out"
grep -q 'kr.SetPageTitle("Kryon Page")' "$out"
grep -q 'kr.ReplaceRoute("/page#top")' "$out"
grep -q 'var route_version int32 = kr.GetRouteVersion()' "$out"
grep -q 'kr.Page(kr.PageProps{' "$out"
grep -q 'CanonicalURL: "https://example.com/page"' "$out"
grep -q 'kr.Heading(kr.HeadingProps{' "$out"
grep -q 'kr.ParagraphText(kr.ParagraphTextProps{' "$out"
grep -q 'kr.Link(kr.LinkProps{' "$out"
grep -q 'kr.PageImage(kr.ImageProps{AssetPath: "hero.png"' "$out"
grep -q 'kr.Section(kr.SectionProps{' "$out"
grep -q 'kr.Flow(kr.FlowProps{' "$out"
grep -q 'kr.Grid(kr.GridProps{' "$out"
grep -q 'kr.Slider(kr.SliderProps{.*ID: 9.*IntValues: slider_values\[:\].*Format: "%"' "$out"
grep -q 'kr.Toggle(kr.ToggleProps{' "$out"
grep -q 'ID: 10' "$out"
grep -q 'Value: &st.ToggleVal' "$out"
grep -q 'OffLabel: "Off"' "$out"
grep -q 'OnLabel: "On"' "$out"
grep -q 'kr.Stack(kr.ColumnProps{' "$out"
grep -q 'Key("smoke-stack")' "$out"
grep -q 'kr.Row(kr.RowProps{' "$out"
grep -q 'kr.Modal(kr.ModalProps{.*Title: "Title".*Actions: modal_actions\[:\].*ActionCount: 2' "$out"
grep -q 'kr.TitleBar(kr.TitleBarProps{Title: "Smoke"' "$out"
grep -q 'kr.Toolbar(kr.ToolbarProps{' "$out"
grep -q 'kr.NavigationBar(kr.NavigationBarProps{' "$out"
grep -q 'Fade(' "$out"
grep -q 'GetThemeSurface()' "$out"

# Go-parity surface: the remaining widget families lower and compile
grep -q 'kr.Button(kr.ButtonProps{Bounds: kr.Rectangle{.*Label: "GB"' "$out"
grep -q 'kr.BeginDisabled(true)' "$out"
grep -q 'kr.EndDisabled()' "$out"
grep -q 'kr.Button(kr.ButtonProps{Bounds: kr.Rectangle{.*Label: "TB"' "$out"
grep -q 'kr.Dropdown(kr.DropdownProps{.*ID: 22.*Options: choices\[:\].*SelectedIndex: &st.Pick' "$out"
grep -q 'kr.BeginFrameBox(kr.Rectangle{' "$out"
grep -q 'kr.FramePack(&frame_box, kr.SideTop' "$out"
grep -q 'kr.GridCell(layout_grid, 1, 1, 1, 1)' "$out"
grep -q 'kr.Place(packed,' "$out"
grep -q 'kr.BeginCanvas(kr.Canvas{' "$out"
grep -q 'kr.EndCanvas(kr.Canvas{' "$out"
grep -q 'CanvasGrid(' "$out"
grep -q 'SelectableText(' "$out"
grep -q 'ShowToast("toast from kry")' "$out"
grep -q 'kr.TextField(kr.TextFieldProps{' "$out"
grep -q 'kr.TextArea(kr.TextAreaProps{.*Syntax: kr.SyntaxNone' "$out"
grep -q 'kr.Radio(kr.RadioProps{' "$out"
grep -q 'Label: "one"' "$out"
grep -q 'Checked: st.Pick == 1' "$out"
grep -q 'kr.Spinbox(kr.SpinboxProps{.*Value: &st.SliderVal' "$out"
grep -q 'kr.Dropdown(kr.DropdownProps{.*ID: 25.*Options: choices\[:\].*SelectedIndex: &st.Pick' "$out"
grep -q 'kr.Fieldset(kr.FieldsetProps{' "$out"
grep -q 'kr.TabBar(kr.TabBarProps{.*ID: 260' "$out"
grep -q 'kr.ListBox(kr.ListBoxProps{' "$out"
grep -q 'kr.TreeView(kr.TreeViewProps{.*Items: tree_items\[:\].*SelectedID: &st.Pick' "$out"
grep -q 'kr.Plot(kr.PlotProps{.*Values: plot_values\[:\].*ValueCount: 4' "$out"
grep -q 'kr.Plot(kr.PlotProps{.*Values: plot_values\[:\].*Offset: 1.*Mode: 1' "$out"
grep -q 'kr.Drag(kr.DragProps{.*Kind: 0.*FloatValues: plot_values\[:\].*ValueCount: 2' "$out"
grep -q 'kr.Drag(kr.DragProps{.*Kind: 1.*IntValues: nums\[:\].*ValueCount: 2' "$out"
grep -q 'kr.Drag(kr.DragProps{.*Mode: 1.*FloatMin: &st.DragScalarMin.*FloatMax: &st.DragScalarMax.*FormatMax: "max %.1f"' "$out"
grep -q 'kr.Drag(kr.DragProps{.*Mode: 1.*IntMin: &st.DragWholeMin.*IntMax: &st.DragWholeMax.*FormatMax: "max %d"' "$out"
grep -q 'kr.Slider(kr.SliderProps{.*Kind: 0.*FloatValues: plot_values\[:\].*ValueCount: 2' "$out"
grep -q 'kr.Slider(kr.SliderProps{.*Kind: 1.*IntValues: nums\[:\].*ValueCount: 2' "$out"
grep -q 'kr.Slider(kr.SliderProps{.*Vertical: true' "$out"
grep -q 'kr.Slider(kr.SliderProps{.*FloatValue: &plot_values\[0\].*Angle: true' "$out"
grep -q 'kr.Input(kr.InputProps{.*Kind: 0.*FloatValues: plot_values\[:\].*ValueCount: 2' "$out"
grep -q 'kr.Input(kr.InputProps{.*Kind: 1.*IntValues: nums\[:\].*ValueCount: 2' "$out"
grep -q 'kr.Input(kr.InputProps{.*Kind: 2.*DoubleValues: plot_doubles\[:\].*ValueCount: 2' "$out"
grep -q 'Size: kr.ControlSizeSmall' "$out"
grep -q 'kr.InvisibleButton(kr.InvisibleButtonProps{' "$out"
grep -q 'kr.Button(kr.ButtonProps{.*Arrow: true' "$out"
grep -q 'Direction: int32(1)' "$out"
grep -q 'kr.Bullet(kr.Rectangle{' "$out"
grep -q 'kr.Separator(kr.SeparatorProps{Bounds: kr.Rectangle{' "$out"
grep -q 'kr.ColorPicker(kr.ColorPickerProps{.*Values: edit_color\[:\].*ValueCount: 3' "$out"
grep -q 'kr.ColorPicker(kr.ColorPickerProps{.*Values: edit_color\[:\].*ValueCount: 4' "$out"
grep -q 'kr.ColorPicker(kr.ColorPickerProps{.*Picker: true' "$out"
grep -q 'kr.Button(kr.ButtonProps{.*Label: "Tint".*Swatch: true.*SwatchColor: kr.Color{R: uint8(51), G: uint8(102), B: uint8(153), A: uint8(204)}' "$out"
grep -q 'Text: "colored"' "$out"
grep -q 'Text: "disabled".*Disabled:' "$out"
grep -q 'Text: "wrapped text"' "$out"
grep -q 'kr.Text(kr.TextProps{.*Text: "Status"' "$out"
grep -q 'kr.Bullet(kr.Rectangle' "$out"
grep -q 'kr.TextFormat("Enabled: %s"' "$out"
grep -q 'kr.TextFormat("Count: %d"' "$out"
grep -q 'kr.TextFormat("Mask: %u"' "$out"
grep -q 'kr.TextFormat("Rate: %.1f"' "$out"
grep -q 'kr.MenuBar(46, kr.Rectangle{.*menus\[:\], 1, &st.MenuOpen)' "$out"
grep -q 'kr.PopupMenu(47, .*menu_items\[:\], 2)' "$out"
grep -q 'kr.ContextMenu(kr.ContextMenuProps{.*Items: menu_items\[:\].*Open: &st.ContextOpen' "$out"
grep -q 'kr.BeginPopup(kr.PopupProps{.*ID: 58.*Flags: kr.PopupTooltip' "$out"
grep -q 'Text: "Helpful text"' "$out"
grep -q 'kr.Selectable(kr.SelectableProps{.*Selected: &st.SelectedRow' "$out"
grep -q 'kr.Checkbox(kr.CheckboxProps{.*Flags: &st.FeatureFlags.*FlagsValue: 4' "$out"
grep -q 'choice_image.Style = kr.ImageStyle{Enabled: true, Background: kr.GetThemeSurface()}' "$out"
grep -q 'kr.Image(choice_image)' "$out"
grep -q 'kr.Button(kr.ButtonProps{.*ImageAssetPath: choice_image.AssetPath.*ImageBounds: choice_image.Bounds.*ID: int32(51)' "$out"
grep -q 'kr.Separator(kr.SeparatorProps{.*Label: "Section".*Font: kr.Text14' "$out"
grep -q 'kr.Button(kr.ButtonProps{.*Label: "+".*Font: int32(kr.Text14).*Tone: kr.ButtonToneNeutral.*Emphasis: kr.ButtonEmphasisGhost' "$out"
grep -q 'kr.TabBar(kr.TabBarProps{.*Tabs: rich_tabs\[:\].*SelectedIndex: st.Tab.*ClosedIndex: &st.ClosedTab' "$out"
grep -q 'kr.DragDropSource(kr.DragDropSourceProps{.*Data: st.FieldText\[:\]' "$out"
grep -q 'kr.DragDropTarget(kr.DragDropTargetProps{.*Output: st.AreaText\[:\].*AcceptedSize: &st.AcceptedSize' "$out"
grep -q 'kr.MultiSelectList(kr.MultiSelectListProps{.*Items: choices\[:\].*Selected: nums\[:\].*SelectedCount: &st.MultiCount.*Anchor: &st.MultiAnchor' "$out"
grep -q 'kr.Collapsible(kr.CollapsibleProps{' "$out"
grep -q 'SetThemeDarkMode(1' "$out"
grep -q 'SetCurrentTheme(0, 1)' "$out"

# typed declarations, arrays, and goto/labels lower for real now
grep -q 'var scalar int32 = 5' "$out"
grep -q 'var nums = \[4\]int32{1,2,3,4}' "$out"
grep -q 'var choices = \[3\]string{"Alpha","Beta","Gamma"}' "$out"
grep -q 'kr.Dropdown(kr.DropdownProps{.*ID: 11.*Options: choices\[:\].*SelectedIndex: &st.Pick' "$out"
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
    kr "github.com/waozixyz/kryon/go/kryon"
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
    if got.Fields != kr.StyleBackgroundEnd || got.BackgroundEnd != (kr.Color{R: 17, G: 34, B: 51, A: 0}) {
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
