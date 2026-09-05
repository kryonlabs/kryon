#include "ui_internal.h"
#include "ui_tk.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Vector2 kryon_zero_vector2;
static const TextInputStyle kryon_zero_text_input_style;


#define UI_TK_MENU_MAX 8
#define UI_TK_CONTEXT_MENU_MAX_ITEMS 64
#define UI_RADIO_ANIM_MAX 128
static int g_menu_open_id = 0;
static int g_menu_submenu_id = 0;
static Rectangle g_menu_panel_bounds = {0};
static int g_menu_panel_valid = 0;
static int g_drag_active = 0;
static float g_drag_last_x = 0.0f;
static int g_slider_active = 0;
static int ui_slider_float(SliderFloatProps slider, int vertical);
typedef struct UIMenuOverlayState {
    int active;
    int bar_id;
    int menu_id;
    int x;
    int y;
    const MenuItem *items;
    int item_count;
} UIMenuOverlayState;

typedef struct UIContextMenuOverlayState {
    int active;
    int id;
    int x;
    int y;
    int item_count;
    int suppress_close;
    MenuItem items[UI_TK_CONTEXT_MENU_MAX_ITEMS];
} UIContextMenuOverlayState;

static UIMenuOverlayState g_menu_overlay = {0};
static UIContextMenuOverlayState g_context_menu_overlay = {0};
static int g_menu_pending_bar_id = 0;
static int g_menu_pending_activated = 0;
static int g_menu_pending_closed_bar_id = 0;
static int g_context_menu_open_id = 0;
static int g_context_menu_pending_id = 0;
static int g_context_menu_pending_activated = 0;
static int g_context_menu_pending_closed_id = 0;
static int g_canvas_depth = 0;
static int g_canvas_mode_depth = 0;

typedef struct UIRadioAnimState {
    unsigned int key;
    float selected;
    float press;
    unsigned long frame_seen;
} UIRadioAnimState;

static UIRadioAnimState g_ui_radio_anim[UI_RADIO_ANIM_MAX];

static int
ui_contains(Rectangle bounds, Vector2 point)
{
    return CheckCollisionPointRec(point, bounds);
}

static int
ui_hot(Rectangle bounds)
{
    Vector2 mouse = ui_mouse_world();
    return ui_contains(bounds, mouse) && !UIInputCapturesClick(mouse);
}

static int
ui_clicked(Rectangle bounds)
{
    return ui_hot(bounds) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static int
ui_menu_bar_owns_open_menu(int id, int menu_count)
{
    return g_menu_open_id >= id + 1 && g_menu_open_id <= id + menu_count;
}

static int
ui_row_text_y(Rectangle bounds, int font)
{
    return (int)bounds.y + ((int)bounds.height - TextLineHeight(font)) / 2;
}

static Color
ui_panel_color(int amount)
{
    int lum = ((int)c_bg.r + (int)c_bg.g + (int)c_bg.b) / 3;
    return lum < 96 ? LightenUIColor(c_bg, amount) : DarkenUIColor(c_bg, amount);
}

static void
ui_draw_panel(Rectangle bounds)
{
    DrawRectangleRec(bounds, c_surface);
    DrawRectangleLinesEx(bounds, 1.0f, c_button);
}

static void
ui_draw_menu_panel(Rectangle bounds)
{
    Color surface = GetThemeSurface();
    Color border = GetThemeButton();
    Color shadow = Fade(GetThemeText(), 0.16f);

    DrawRectangleRec((Rectangle){bounds.x + 2.0f, bounds.y + 2.0f,
                                 bounds.width, bounds.height}, shadow);
    DrawRectangleRec(bounds, surface);
    DrawRectangleLinesEx(bounds, 1.0f, border);
}

static void
ui_menu_track_panel(Rectangle bounds)
{
    float x1;
    float y1;
    float x2;
    float y2;

    if(!g_menu_panel_valid) {
        g_menu_panel_bounds = bounds;
        g_menu_panel_valid = 1;
        return;
    }

    x1 = g_menu_panel_bounds.x < bounds.x ? g_menu_panel_bounds.x : bounds.x;
    y1 = g_menu_panel_bounds.y < bounds.y ? g_menu_panel_bounds.y : bounds.y;
    x2 = g_menu_panel_bounds.x + g_menu_panel_bounds.width;
    if(bounds.x + bounds.width > x2)
        x2 = bounds.x + bounds.width;
    y2 = g_menu_panel_bounds.y + g_menu_panel_bounds.height;
    if(bounds.y + bounds.height > y2)
        y2 = bounds.y + bounds.height;
    g_menu_panel_bounds = (Rectangle){x1, y1, x2 - x1, y2 - y1};
}

static int
ui_scroll_max(int content_h, int viewport_h)
{
    if(content_h <= viewport_h)
        return 0;
    return content_h - viewport_h;
}

static int
ui_update_scroll(Rectangle bounds, int content_h, int *scroll_offset, int row_h)
{
    int max_scroll;
    float wheel;
    Vector2 mouse;

    if(scroll_offset == NULL)
        return 0;
    max_scroll = ui_scroll_max(content_h, (int)bounds.height);
    if(*scroll_offset < 0)
        *scroll_offset = 0;
    if(*scroll_offset > max_scroll)
        *scroll_offset = max_scroll;

    mouse = ui_mouse_world();
    if(!ui_contains(bounds, mouse) || UIInputCapturesClick(mouse))
        return max_scroll;

    wheel = GetMouseWheelMove();
    if(wheel != 0.0f) {
        int step = row_h > 0 ? row_h * 3 : ScaleUIPx(90);
        *scroll_offset -= (int)(wheel * (float)step);
        if(*scroll_offset < 0)
            *scroll_offset = 0;
        if(*scroll_offset > max_scroll)
            *scroll_offset = max_scroll;
    }
    return max_scroll;
}

FrameBox
BeginFrameBox(Rectangle bounds, int pad_x, int pad_y, int gap)
{
    FrameBox frame;
    frame.bounds = bounds;
    frame.pad_x = ScaleUIPx(pad_x);
    frame.pad_y = ScaleUIPx(pad_y);
    frame.gap = ScaleUIPx(gap);
    frame.cursor_x = (int)bounds.x + frame.pad_x;
    frame.cursor_y = (int)bounds.y + frame.pad_y;
    return frame;
}

Rectangle
FramePack(FrameBox *frame, Side side, int size)
{
    Rectangle item = {0};
    int scaled = ScaleUIPx(size);

    if(frame == NULL)
        return item;

    item = frame->bounds;
    item.x += frame->pad_x;
    item.y += frame->pad_y;
    item.width -= frame->pad_x * 2;
    item.height -= frame->pad_y * 2;

    if(side == SideTop) {
        item.y = frame->cursor_y;
        item.height = scaled;
        frame->cursor_y += scaled + frame->gap;
    } else if(side == SideBottom) {
        item.y = frame->bounds.y + frame->bounds.height - frame->pad_y - scaled;
        item.height = scaled;
        frame->bounds.height -= scaled + frame->gap;
    } else if(side == SideLeft) {
        item.x = frame->cursor_x;
        item.width = scaled;
        frame->cursor_x += scaled + frame->gap;
    } else {
        item.x = frame->bounds.x + frame->bounds.width - frame->pad_x - scaled;
        item.width = scaled;
        frame->bounds.width -= scaled + frame->gap;
    }

    return item;
}

Rectangle
GridCell(Grid grid, int row, int col, int row_span, int col_span)
{
    float gx = (float)ScaleUIPx(grid.gap_x);
    float gy = (float)ScaleUIPx(grid.gap_y);
    float px = (float)ScaleUIPx(grid.pad_x);
    float py = (float)ScaleUIPx(grid.pad_y);
    float cell_w;
    float cell_h;

    if(grid.rows < 1)
        grid.rows = 1;
    if(grid.cols < 1)
        grid.cols = 1;
    if(row_span < 1)
        row_span = 1;
    if(col_span < 1)
        col_span = 1;

    cell_w = (grid.bounds.width - px * 2.0f - gx * (float)(grid.cols - 1)) / (float)grid.cols;
    cell_h = (grid.bounds.height - py * 2.0f - gy * (float)(grid.rows - 1)) / (float)grid.rows;

    return (Rectangle){
        grid.bounds.x + px + (float)col * (cell_w + gx),
        grid.bounds.y + py + (float)row * (cell_h + gy),
        cell_w * (float)col_span + gx * (float)(col_span - 1),
        cell_h * (float)row_span + gy * (float)(row_span - 1)
    };
}

Rectangle
Place(Rectangle parent, int x, int y, int w, int h)
{
    return (Rectangle){parent.x + ScaleUIPx(x), parent.y + ScaleUIPx(y),
                       ScaleUIPx(w), ScaleUIPx(h)};
}

void
DrawUISeparator(Rectangle bounds, int vertical)
{
    if(!IsWindowReady())
        return;
    if(vertical)
        DrawLine((int)(bounds.x + bounds.width / 2), (int)bounds.y,
                 (int)(bounds.x + bounds.width / 2), (int)(bounds.y + bounds.height), c_button);
    else
        DrawLine((int)bounds.x, (int)(bounds.y + bounds.height / 2),
                 (int)(bounds.x + bounds.width), (int)(bounds.y + bounds.height / 2), c_button);
}

void
DrawUISeparatorText(SeparatorTextProps separator)
{
    const char *label = separator.label != NULL ? separator.label : "";
    int font = separator.font > 0 ? separator.font : GetUISmallFontSize();
    int text_width = TextWidth(label, font);
    int text_y = ui_row_text_y(separator.bounds, font);
    int line_y = (int)(separator.bounds.y + separator.bounds.height * 0.5f);
    int line_x = (int)separator.bounds.x;
    Color color = separator.disabled ? Fade(c_text, 0.45f) : c_text;

    if(!IsWindowReady())
        return;
    if(label[0] != '\0') {
        DrawUIText(label, (int)separator.bounds.x, text_y, font, color);
        line_x += text_width + ScaleUIPx(12);
    }
    if(line_x < (int)(separator.bounds.x + separator.bounds.width))
        DrawLine(line_x, line_y,
                 (int)(separator.bounds.x + separator.bounds.width), line_y,
                 separator.disabled ? Fade(c_button, 0.45f) : c_button);
}

int
DrawUISmallButton(ButtonProps button)
{
    if(!IsWindowReady())
        return 0;
    ButtonSpec spec = {button.bounds, button.label, GetUISmallFontSize(),
                       button.id, button.disabled, {0}, {0}, {0}, {0}, 0.0f};
    return RenderButton(spec);
}

int
DrawUIInvisibleButton(InvisibleButtonProps button)
{
    if(!IsWindowReady())
        return 0;
    ButtonSpec spec = {button.bounds, "", GetUISmallFontSize(), button.id,
                       button.disabled, {0}, {0}, {0}, {0}, 0.0f};
    return HandleButton(spec);
}

int
DrawUIArrowButton(ArrowButtonProps button)
{
    if(!IsWindowReady())
        return 0;
    const char *label = "<";
    if(button.direction == ARROW_RIGHT) label = ">";
    else if(button.direction == ARROW_UP) label = "^";
    else if(button.direction == ARROW_DOWN) label = "v";
    return RenderButton((ButtonSpec){button.bounds, label, GetUISmallFontSize(),
                                     button.id, button.disabled,
                                     {0}, {0}, {0}, {0}, 0.0f});
}

void
DrawUIBullet(Rectangle bounds)
{
    if(!IsWindowReady())
        return;
    float radius = bounds.width < bounds.height ? bounds.width * 0.25f
                                                 : bounds.height * 0.25f;
    DrawCircleV((Vector2){bounds.x + bounds.width * 0.5f,
                          bounds.y + bounds.height * 0.5f},
                radius, GetThemeText());
}

int
DrawUISelectable(SelectableProps selectable)
{
    Vector2 mouse = ui_mouse_world();
    int selected = selectable.selected != NULL && *selectable.selected;
    int hot = ui_contains(selectable.bounds, mouse) &&
              !UIInputCapturesClick(mouse);

    if(IsWindowReady() && (selected || hot))
        DrawRectangleRec(selectable.bounds,
                         selected ? GetThemeButton() : GetThemeButtonHover());
    if(hot)
        selectable.disabled ? MarkUIDisabled() : MarkUIClickable();
    if(IsWindowReady())
        DrawUIText(selectable.label != NULL ? selectable.label : "",
                   (int)selectable.bounds.x + ScaleUIPx(8),
                   ui_row_text_y(selectable.bounds, GetUIFontSize()),
                   GetUIFontSize(), selectable.disabled
                       ? Fade(GetThemeText(), 0.45f) : GetThemeText());
    if(hot && !selectable.disabled &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        UIConsumeRelease();
        if(selectable.selected != NULL)
            *selectable.selected = !*selectable.selected;
        return 1;
    }
    return 0;
}

int
DrawUICheckboxFlags(CheckboxFlagsProps checkbox)
{
    Vector2 mouse = ui_mouse_world();
    int checked = checkbox.flags != NULL &&
                  ((*checkbox.flags & checkbox.flags_value) == checkbox.flags_value);
    int box_size = ScaleUIPx(20);
    Rectangle box = {checkbox.bounds.x,
                     checkbox.bounds.y + (checkbox.bounds.height - box_size) * 0.5f,
                     (float)box_size, (float)box_size};
    int hot = ui_contains(checkbox.bounds, mouse) &&
              !UIInputCapturesClick(mouse);

    if(IsWindowReady()) {
        DrawRectangleLinesEx(box, 1.0f, GetThemeButton());
        if(checked)
            DrawRectangle((int)box.x + ScaleUIPx(4),
                          (int)box.y + ScaleUIPx(4),
                          box_size - ScaleUIPx(8), box_size - ScaleUIPx(8),
                          GetThemeCircle());
        DrawUIText(checkbox.label != NULL ? checkbox.label : "",
                   (int)box.x + box_size + ScaleUIPx(8),
                   ui_row_text_y(checkbox.bounds, GetUIFontSize()),
                   GetUIFontSize(), checkbox.disabled
                       ? Fade(GetThemeText(), 0.45f) : GetThemeText());
    }
    if(hot)
        checkbox.disabled ? MarkUIDisabled() : MarkUIClickable();
    if(hot && !checkbox.disabled && checkbox.flags != NULL &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        UIConsumeRelease();
        if(checked)
            *checkbox.flags &= ~checkbox.flags_value;
        else
            *checkbox.flags |= checkbox.flags_value;
        return 1;
    }
    return 0;
}

static int
ui_color_edit(ColorEditProps edit, int channels)
{
    if(edit.values == NULL || edit.value_count < channels)
        return 0;
    return ui_slider_float((SliderFloatProps){edit.bounds, edit.id, edit.label,
                                               edit.values, channels, 0.0f, 1.0f,
                                               "%.3f", edit.disabled}, 0);
}

static Color
ui_float_color(const float *values, int channels)
{
    float component[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for(int i = 0; i < channels; i++) {
        component[i] = values[i];
        if(component[i] < 0.0f) component[i] = 0.0f;
        if(component[i] > 1.0f) component[i] = 1.0f;
    }
    return (Color){(unsigned char)(component[0] * 255.0f + 0.5f),
                   (unsigned char)(component[1] * 255.0f + 0.5f),
                   (unsigned char)(component[2] * 255.0f + 0.5f),
                   (unsigned char)(component[3] * 255.0f + 0.5f)};
}

static int
ui_color_picker_float(ColorEditProps picker, int channels)
{
    int changed = 0;
    float swatch_h = ScaleUIPx(36);
    float gap = ScaleUIPx(4);
    float row_h = (picker.bounds.height - swatch_h - gap) / channels;
    const char *labels[4] = {"R", "G", "B", "A"};

    if(picker.values == NULL || picker.value_count < channels)
        return 0;
    if(row_h < ScaleUIPx(20))
        row_h = ScaleUIPx(28);
    for(int i = 0; i < channels; i++) {
        SliderFloatProps channel = {
            {picker.bounds.x, picker.bounds.y + row_h * i,
             picker.bounds.width, row_h - ScaleUIPx(2)},
            picker.id * 8 + i + 1, labels[i], &picker.values[i], 1,
            0.0f, 1.0f, "%.3f", picker.disabled
        };
        changed |= ui_slider_float(channel, 0);
    }
    if(IsWindowReady()) {
        Rectangle swatch = {picker.bounds.x,
                            picker.bounds.y + row_h * channels + gap,
                            picker.bounds.width, swatch_h};
        DrawRectangleRec(swatch, ui_float_color(picker.values, channels));
        DrawRectangleLinesEx(swatch, 1.0f, c_button);
        if(picker.label != NULL)
            DrawUIText(picker.label, (int)swatch.x + ScaleUIPx(6),
                       ui_row_text_y(swatch, GetUISmallFontSize()),
                       GetUISmallFontSize(), c_text);
    }
    return changed;
}

int DrawUIColorEdit3(ColorEditProps edit) { return ui_color_edit(edit, 3); }
int DrawUIColorEdit4(ColorEditProps edit) { return ui_color_edit(edit, 4); }
int DrawUIColorPicker3(ColorEditProps picker) { return ui_color_picker_float(picker, 3); }
int DrawUIColorPicker4(ColorEditProps picker) { return ui_color_picker_float(picker, 4); }

int
DrawUIColorButton(ColorButtonProps button)
{
    int pressed;
    if(!IsWindowReady())
        return 0;
    pressed = HandleButton((ButtonSpec){button.bounds, "", GetUISmallFontSize(),
                                        button.id, button.disabled,
                                        {0}, {0}, {0}, {0}, 0.0f});
    DrawRectangleRec(button.bounds, (Color){180, 180, 180, 255});
    DrawRectangle((int)button.bounds.x, (int)button.bounds.y,
                  (int)(button.bounds.width / 2), (int)(button.bounds.height / 2),
                  (Color){220, 220, 220, 255});
    DrawRectangle((int)(button.bounds.x + button.bounds.width / 2),
                  (int)(button.bounds.y + button.bounds.height / 2),
                  (int)(button.bounds.width / 2), (int)(button.bounds.height / 2),
                  (Color){220, 220, 220, 255});
    DrawRectangleRec(button.bounds, button.color);
    DrawRectangleLinesEx(button.bounds, 1.0f,
                         ui_hot(button.bounds) ? c_button_hover : c_button);
    if(button.label != NULL)
        DrawUIText(button.label, (int)button.bounds.x + ScaleUIPx(6),
                   ui_row_text_y(button.bounds, GetUISmallFontSize()),
                   GetUISmallFontSize(), c_text);
    return pressed;
}

int
DrawUITooltip(TooltipProps tooltip)
{
    Vector2 mouse = ui_mouse_world();
    int font = tooltip.font > 0 ? tooltip.font : GetUISmallFontSize();
    int max_width = tooltip.max_width > 0 ? tooltip.max_width : ScaleUIPx(240);
    int padding = ScaleUIPx(8);
    int content_width;
    int content_height;
    int y;
    Rectangle panel;
    ParagraphSpec paragraph = {0};

    if(tooltip.disabled || tooltip.text == NULL ||
       !ui_contains(tooltip.trigger, mouse))
        return 0;
    content_width = TextWidth(tooltip.text, font);
    if(content_width > max_width)
        content_width = max_width;
    if(content_width < ScaleUIPx(24))
        content_width = ScaleUIPx(24);
    paragraph.text = tooltip.text;
    paragraph.width = content_width;
    paragraph.font = font;
    paragraph.line_gap = ScaleUIPx(2);
    paragraph.color = GetThemeText();
    content_height = ui_paragraph_height(paragraph);
    panel = (Rectangle){mouse.x + ScaleUIPx(12), mouse.y + ScaleUIPx(16),
                        content_width + padding * 2,
                        content_height + padding * 2};
    if(panel.x + panel.width > GetUIViewWidth())
        panel.x = GetUIViewWidth() - panel.width - ScaleUIPx(4);
    if(panel.y + panel.height > GetUIViewHeight())
        panel.y = mouse.y - panel.height - ScaleUIPx(8);
    if(!IsWindowReady())
        return 1;
    DrawRectangleRec((Rectangle){panel.x + ScaleUIPx(2),
                                 panel.y + ScaleUIPx(2),
                                 panel.width, panel.height},
                     Fade(GetThemeText(), 0.18f));
    DrawRectangleRec(panel, GetThemeSurface());
    DrawRectangleLinesEx(panel, 1.0f, GetThemeButton());
    y = (int)panel.y + padding;
    ui_draw_paragraph(paragraph, (int)panel.x + padding, &y);
    return 1;
}

static int
draw_menu_items(int id, int x, int y, const MenuItem *items, int item_count)
{
    int font = GetUIFontSize();
    int row_h = ScaleUIPx(30);
    int pad = ScaleUIPx(12);
    int accel_w = ScaleUIPx(88);
    int w = ScaleUIPx(180);
    int activated = 0;
    Rectangle panel;
    Vector2 mouse;
    int can_draw = IsWindowReady();

    (void)id;
    if(items == NULL || item_count <= 0)
        return 0;

    for(int i = 0; i < item_count; i++) {
        int text_w = items[i].label != NULL ? TextWidth(items[i].label, font) : 0;
        int accel = items[i].accelerator != NULL ? TextWidth(items[i].accelerator, font) + accel_w : 0;
        if(text_w + accel + pad * 2 > w)
            w = text_w + accel + pad * 2;
    }

    panel = (Rectangle){(float)x, (float)y, (float)w, (float)(row_h * item_count + ScaleUIPx(8))};
    if(can_draw)
        ui_draw_menu_panel(panel);
    ui_menu_track_panel(panel);
    PushUIInputCapture(panel, 1);
    mouse = ui_mouse_world();
    if(ui_contains(panel, mouse))
        MarkUICursor(MOUSE_CURSOR_DEFAULT);

    for(int i = 0; i < item_count; i++) {
        Rectangle row = {(float)x + 4, (float)y + 4 + (float)(i * row_h),
                         (float)w - 8, (float)row_h};
        const MenuItem *item = &items[i];
        int row_hot = ui_contains(row, mouse) && item->kind != MenuSeparator;
        int hot = row_hot && !item->disabled;

        if(item->kind == MenuSeparator) {
            if(can_draw)
                DrawUISeparator(row, 0);
            continue;
        }

        if(hot) {
            if(can_draw)
                DrawRectangleRec(row, GetThemeButtonHover());
            MarkUIClickable();
        }
        if(item->disabled && row_hot)
            MarkUIDisabled();
        if(can_draw && item->checked)
            DrawUIText("*", (int)row.x + ScaleUIPx(8), ui_row_text_y(row, font), font, GetThemeIcon());
        if(can_draw)
            DrawUIText(item->label != NULL ? item->label : "",
                       (int)row.x + ScaleUIPx(28), ui_row_text_y(row, font),
                       font, item->disabled ? GetThemeButton()
                                            : GetThemeText());
        if(can_draw && item->accelerator != NULL)
            DrawUIText(item->accelerator, (int)(row.x + row.width - accel_w),
                       ui_row_text_y(row, font),
                       font, item->disabled ? GetThemeButton() : GetThemeIcon());
        if(can_draw && item->kind == MenuSubmenu)
            DrawUIText(">", (int)(row.x + row.width - ScaleUIPx(18)),
                       ui_row_text_y(row, font),
                       font, item->disabled ? GetThemeButton() : GetThemeIcon());
        if(hot && item->kind == MenuSubmenu)
            g_menu_submenu_id = item->id;
        if(item->kind == MenuSubmenu && g_menu_submenu_id == item->id &&
           item->submenu != NULL && item->submenu_count > 0) {
            int sub = draw_menu_items(item->id, (int)(row.x + row.width), (int)row.y,
                                      item->submenu, item->submenu_count);
            if(sub != 0)
                activated = sub;
        }
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && item->kind != MenuSubmenu) {
            UIConsumeRelease();
            activated = item->id;
            g_menu_open_id = 0;
        }
    }

    return activated;
}

static Rectangle
menu_items_panel_bounds(int x, int y, const MenuItem *items, int item_count)
{
    int font = GetUIFontSize();
    int row_h = ScaleUIPx(30);
    int pad = ScaleUIPx(12);
    int accel_w = ScaleUIPx(88);
    int w = ScaleUIPx(180);

    if(items == NULL || item_count <= 0)
        return (Rectangle){(float)x, (float)y, 0.0f, 0.0f};

    for(int i = 0; i < item_count; i++) {
        int text_w = items[i].label != NULL ? TextWidth(items[i].label, font) : 0;
        int accel = items[i].accelerator != NULL ? TextWidth(items[i].accelerator, font) + accel_w : 0;
        if(text_w + accel + pad * 2 > w)
            w = text_w + accel + pad * 2;
    }
    return (Rectangle){(float)x, (float)y, (float)w,
                       (float)(row_h * item_count + ScaleUIPx(8))};
}

static void
queue_context_menu_overlay(ContextMenuProps menu, int suppress_close)
{
    int count = menu.item_count;

    if(menu.items == NULL || count <= 0) {
        g_context_menu_overlay.active = 0;
        return;
    }
    if(count > UI_TK_CONTEXT_MENU_MAX_ITEMS)
        count = UI_TK_CONTEXT_MENU_MAX_ITEMS;

    g_context_menu_overlay.active = 1;
    g_context_menu_overlay.id = menu.id;
    g_context_menu_overlay.x = menu.x != NULL ? *menu.x : 0;
    g_context_menu_overlay.y = menu.y != NULL ? *menu.y : 0;
    g_context_menu_overlay.item_count = count;
    g_context_menu_overlay.suppress_close = suppress_close;
    for(int i = 0; i < count; i++)
        g_context_menu_overlay.items[i] = menu.items[i];
}

MenuBarResult
DrawUIMenuBar(int id, Rectangle bounds, const Menu *menus, int menu_count, int *open_index)
{
    MenuBarResult result = {0, -1};
    int font = GetUIFontSize();
    int x = (int)bounds.x + ScaleUIPx(4);
    Vector2 mouse = ui_mouse_world();
    int skip_external_open = 0;
    int bar_capture_pushed = 0;
    int can_draw = IsWindowReady();

    if(g_menu_pending_bar_id == id) {
        result.activated_id = g_menu_pending_activated;
        g_menu_pending_bar_id = 0;
        g_menu_pending_activated = 0;
    }
    if(g_menu_pending_closed_bar_id == id) {
        g_menu_pending_closed_bar_id = 0;
        g_menu_open_id = 0;
        g_menu_submenu_id = 0;
        skip_external_open = 1;
        if(open_index != NULL)
            *open_index = -1;
    }
    if(menu_count > UI_TK_MENU_MAX)
        menu_count = UI_TK_MENU_MAX;
    if(!skip_external_open && open_index != NULL && *open_index >= 0)
        g_menu_open_id = id + 1 + *open_index;
    if(ui_menu_bar_owns_open_menu(id, menu_count)) {
        PushUIInputCapture(bounds, 1);
        bar_capture_pushed = 1;
    }
    g_menu_overlay.active = 0;
    if(can_draw) {
        DrawRectangleRec(bounds, c_surface);
        DrawRectangleLinesEx(bounds, 1.0f, c_button);
    }

    for(int i = 0; i < menu_count; i++) {
        int w = TextWidth(menus[i].label != NULL ? menus[i].label : "", font) + ScaleUIPx(24);
        Rectangle item = {(float)x, bounds.y + ScaleUIPx(3), (float)w, bounds.height - ScaleUIPx(6)};
        int menu_id = id + 1 + i;
        int open = g_menu_open_id == menu_id;
        int hot = ui_hot(item);
        if(can_draw && (hot || open))
            DrawRectangleRec(item, open ? c_button : c_button_hover);
        if(hot)
            MarkUIClickable();
        if(can_draw)
            DrawUIText(menus[i].label != NULL ? menus[i].label : "",
                       x + ScaleUIPx(12), ui_row_text_y(item, font), font,
                       c_text);
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            UIConsumeRelease();
            g_menu_open_id = open ? 0 : menu_id;
            if(g_menu_open_id == 0)
                g_menu_submenu_id = 0;
            open = g_menu_open_id == menu_id;
        }
        if(hot && g_menu_open_id != 0 && !open) {
            g_menu_open_id = menu_id;
            g_menu_submenu_id = 0;
        }
        open = g_menu_open_id == menu_id;
        if(open) {
            result.open_index = i;
            g_menu_overlay.active = 1;
            g_menu_overlay.bar_id = id;
            g_menu_overlay.menu_id = id + i;
            g_menu_overlay.x = x;
            g_menu_overlay.y = (int)(bounds.y + bounds.height);
            g_menu_overlay.items = menus[i].items;
            g_menu_overlay.item_count = menus[i].item_count;
        }
        x += w + ScaleUIPx(2);
    }
    if(g_menu_open_id != 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !ui_contains(bounds, mouse) &&
       (!g_menu_panel_valid || !ui_contains(g_menu_panel_bounds, mouse))) {
        UIConsumeRelease();
        g_menu_open_id = 0;
        g_menu_submenu_id = 0;
        result.open_index = -1;
    }
    if(g_menu_open_id != 0 && !bar_capture_pushed &&
       ui_menu_bar_owns_open_menu(id, menu_count))
        PushUIInputCapture(bounds, 1);
    if(open_index != NULL)
        *open_index = result.open_index;
    return result;
}

void
ui_draw_menu_overlays(void)
{
    int activated;

    if(g_menu_overlay.active && g_menu_open_id != 0) {
        g_menu_panel_valid = 0;
        activated = draw_menu_items(g_menu_overlay.menu_id,
                                    g_menu_overlay.x,
                                    g_menu_overlay.y,
                                    g_menu_overlay.items,
                                    g_menu_overlay.item_count);
        if(activated != 0) {
            g_menu_pending_bar_id = g_menu_overlay.bar_id;
            g_menu_pending_activated = activated;
            g_menu_pending_closed_bar_id = g_menu_overlay.bar_id;
        }
    }
    g_menu_overlay.active = 0;

    if(g_context_menu_overlay.active &&
       g_context_menu_open_id == g_context_menu_overlay.id) {
        Vector2 mouse = ui_mouse_world();

        g_menu_panel_valid = 0;
        activated = draw_menu_items(g_context_menu_overlay.id,
                                    g_context_menu_overlay.x,
                                    g_context_menu_overlay.y,
                                    g_context_menu_overlay.items,
                                    g_context_menu_overlay.item_count);
        if(activated != 0) {
            g_context_menu_pending_id = g_context_menu_overlay.id;
            g_context_menu_pending_activated = activated;
            g_context_menu_open_id = 0;
        } else if(!g_context_menu_overlay.suppress_close &&
                  IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                  (!g_menu_panel_valid ||
                   !ui_contains(g_menu_panel_bounds, mouse))) {
            UIConsumeRelease();
            g_context_menu_pending_closed_id = g_context_menu_overlay.id;
            g_context_menu_open_id = 0;
        }
    }
    g_context_menu_overlay.active = 0;
}

int
DrawUIPopupMenu(int id, int x, int y, const MenuItem *items, int item_count)
{
    return draw_menu_items(id, x, y, items, item_count);
}

int
DrawUIContextMenu(ContextMenuProps menu)
{
    Vector2 mouse = ui_mouse_world();
    int open_local = 0;
    int x_local = 0;
    int y_local = 0;
    Rectangle panel;
    int suppress_close = 0;

    if(menu.open == NULL)
        menu.open = &open_local;
    if(menu.x == NULL)
        menu.x = &x_local;
    if(menu.y == NULL)
        menu.y = &y_local;
    if(g_context_menu_pending_id == menu.id) {
        int activated = g_context_menu_pending_activated;

        g_context_menu_pending_id = 0;
        g_context_menu_pending_activated = 0;
        *menu.open = 0;
        return activated;
    }
    if(g_context_menu_pending_closed_id == menu.id) {
        g_context_menu_pending_closed_id = 0;
        *menu.open = 0;
        return 0;
    }
    if(ui_contains(menu.trigger, mouse) &&
       !UIInputCapturesClick(mouse) &&
       IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        *menu.open = 1;
        *menu.x = (int)mouse.x;
        *menu.y = (int)mouse.y;
        suppress_close = 1;
    }
    if(!*menu.open) {
        if(g_context_menu_open_id == menu.id)
            g_context_menu_open_id = 0;
        return 0;
    }

    if(ui_contains(menu.trigger, mouse) &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        suppress_close = 1;
    g_context_menu_open_id = menu.id;
    panel = menu_items_panel_bounds(*menu.x, *menu.y,
                                    menu.items, menu.item_count);
    ui_menu_track_panel(panel);
    PushUIInputCapture(panel, 1);
    if(ui_contains(panel, mouse))
        MarkUICursor(MOUSE_CURSOR_DEFAULT);
    queue_context_menu_overlay(menu, suppress_close);
    return 0;
}

int
DrawUIRadioButton(RadioButtonProps radio)
{
    int font = GetUIFontSize();
    int diameter = ScaleUIPx(20);
    int touch = ScaleUIPx(40);
    Rectangle hit_bounds = radio.bounds;
    Vector2 center = {radio.bounds.x + touch / 2.0f,
                      radio.bounds.y + radio.bounds.height / 2.0f};
    int hot;
    int down;

    if(ui_material_style() && hit_bounds.height < touch &&
       radio.bounds.height >= touch) {
        hit_bounds.y -= ((float)touch - hit_bounds.height) * 0.5f;
        hit_bounds.height = (float)touch;
    }
    if(ui_material_style() && hit_bounds.width < touch)
        hit_bounds.width = (float)touch;
    hot = ui_hot(hit_bounds) && !radio.disabled;
    down = hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if(hot)
        MarkUIClickable();
    if(radio.disabled)
        MarkUIDisabled();
    if(ui_material_style()) {
        UIMaterialScheme scheme = ui_material_scheme();
        UIRadioAnimState *anim;
        Rectangle state_bounds = {
            center.x - (float)touch / 2.0f,
            center.y - (float)touch / 2.0f,
            (float)touch,
            (float)touch
        };
        unsigned int key = 2166136261u;
        float target = radio.checked ? 1.0f : 0.0f;
        float dt;
        float selected;
        float press;
        float outer = (float)diameter / 2.0f;
        float stroke = (float)ScaleUIPx(2);
        float fill_radius;
        Color ring;
        Color fill;
        Color label = radio.disabled ? scheme.disabled_content : scheme.on_surface;
        const char *text = radio.label != NULL ? radio.label : "";

        key = (key ^ (unsigned int)radio.id) * 16777619u;
        key = (key ^ (unsigned int)(int)radio.bounds.x) * 16777619u;
        key = (key ^ (unsigned int)(int)radio.bounds.y) * 16777619u;
        key = (key ^ (unsigned int)(int)radio.bounds.width) * 16777619u;
        key = (key ^ (unsigned int)(int)radio.bounds.height) * 16777619u;
        while(*text != '\0')
            key = (key ^ (unsigned char)*text++) * 16777619u;
        anim = &g_ui_radio_anim[key % UI_RADIO_ANIM_MAX];
        if(anim->key != key || g_ui_frame_serial - anim->frame_seen > 12) {
            memset(anim, 0, sizeof(*anim));
            anim->key = key;
            anim->selected = target;
        }
        anim->frame_seen = g_ui_frame_serial;
        dt = GetFrameTime();
        if(dt <= 0.0f || dt > 0.1f)
            dt = 1.0f / 60.0f;
        {
            float step = dt * 18.0f;
            float press_step = dt * 20.0f;
            if(step > 1.0f)
                step = 1.0f;
            if(press_step > 1.0f)
                press_step = 1.0f;
            anim->selected += (target - anim->selected) * step;
            anim->press += ((down ? 1.0f : 0.0f) - anim->press) * press_step;
        }
        selected = anim->selected;
        press = anim->press;

        ring = ColorLerp(scheme.on_surface_variant, scheme.primary, selected);
        fill = c_text;
        if(radio.checked)
            ring = c_text;
        if(radio.disabled) {
            ring = scheme.disabled_content;
            fill = scheme.disabled_content;
            selected = target;
            press = 0.0f;
        }
        if(stroke < 1.0f)
            stroke = 1.0f;
        if(hot && UIHoverEffectsEnabled()) {
            Color layer = ring;
            layer.a = (unsigned char)(20 + 11 * press);
            DrawCircleV(center, (float)touch / 2.0f, layer);
        } else if(down) {
            Color layer = ring;
            layer.a = 31;
            DrawCircleV(center, (float)touch / 2.0f, layer);
        }
        ui_material_ripple(state_bounds, ring, (int)key, down);
        fill_radius = radio.checked ? outer - stroke : (outer - stroke * 1.5f) * selected;
        if(fill_radius > 0.2f)
            DrawCircleV(center, fill_radius, fill);
        DrawRing(center, outer - stroke, outer, 0.0f, 360.0f, 48, ring);
        DrawUIText(radio.label != NULL ? radio.label : "",
                   (int)radio.bounds.x + touch + ScaleUIPx(4),
                   ui_row_text_y(radio.bounds, font), font, label);
    } else {
        center.x = radio.bounds.x + diameter / 2.0f;
        DrawCircleLines((int)center.x, (int)center.y, (float)diameter / 2.0f, radio.disabled ? c_button : c_icon);
        if(radio.checked)
            DrawCircleV(center, (float)diameter / 2.0f - (float)ScaleUIPx(3),
                        radio.disabled ? c_button : c_text);
        DrawUIText(radio.label != NULL ? radio.label : "", (int)radio.bounds.x + diameter + ScaleUIPx(8),
                   ui_row_text_y(radio.bounds, font), font, radio.disabled ? c_button : c_text);
    }
    if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        UIConsumeRelease();
        return radio.id;
    }
    return 0;
}

void
DrawUIProgressBar(ProgressBarProps progress)
{
    float t;
    Rectangle fill = progress.bounds;
    const char *label = progress.label;
    Color fill_color = c_button_hover;
    int font = GetUISmallFontSize();
    int center_x = (int)(progress.bounds.x + progress.bounds.width / 2);
    if(progress.max <= progress.min)
        progress.max = progress.min + 1;
    t = (float)(progress.value - progress.min) / (float)(progress.max - progress.min);
    if(t < 0.0f)
        t = 0.0f;
    if(t > 1.0f)
        t = 1.0f;
    if(!IsWindowReady())
        return;
    fill.width *= t;
    DrawRectangleRec(progress.bounds, ui_panel_color(10));
    DrawRectangleRec(fill, fill_color);
    DrawRectangleLinesEx(progress.bounds, 1.0f, c_button);
    if(label != NULL) {
        int pad = ScaleUIPx(6);
        int text_w = TextWidth(label, font);
        int text_x = center_x - text_w / 2;
        int text_y = GetUIControlTextY(label, (int)progress.bounds.y,
                                       (int)progress.bounds.height, font);
        Color text_color = c_text;
        float fill_end = fill.x + fill.width;
        float empty_w = progress.bounds.width - fill.width;

        if(empty_w >= (float)(text_w + pad * 2)) {
            text_x = (int)fill_end + pad;
            text_color = c_text;
        } else if(fill.width >= (float)(text_w + pad * 2)) {
            text_x = (int)fill_end - text_w - pad;
            text_color = ui_material_on_color(fill_color);
        }
        DrawUIText(label, text_x, text_y, font, text_color);
    }
}

static void
ui_plot(PlotProps plot, int histogram)
{
    float min_value;
    float max_value;
    float range;
    int count = plot.value_count;
    int offset;

    if(!IsWindowReady())
        return;
    DrawRectangleRec(plot.bounds, ui_panel_color(10));
    DrawRectangleLinesEx(plot.bounds, 1.0f, c_button);
    if(plot.values == NULL || count <= 0)
        return;
    offset = plot.offset % count;
    if(offset < 0)
        offset += count;
    min_value = plot.scale_min;
    max_value = plot.scale_max;
    if(min_value >= max_value) {
        min_value = max_value = plot.values[offset];
        for(int i = 1; i < count; i++) {
            float value = plot.values[(offset + i) % count];
            if(value < min_value)
                min_value = value;
            if(value > max_value)
                max_value = value;
        }
        if(min_value == max_value) {
            min_value -= 0.5f;
            max_value += 0.5f;
        }
    }
    range = max_value - min_value;
    BeginUIClip((int)plot.bounds.x, (int)plot.bounds.y,
                (int)plot.bounds.width, (int)plot.bounds.height);
    if(histogram) {
        float step = plot.bounds.width / (float)count;
        for(int i = 0; i < count; i++) {
            float value = plot.values[(offset + i) % count];
            float t = (value - min_value) / range;
            Rectangle bar;
            if(t < 0.0f) t = 0.0f;
            if(t > 1.0f) t = 1.0f;
            bar.x = plot.bounds.x + i * step + 1.0f;
            bar.width = step > 2.0f ? step - 2.0f : step;
            bar.height = t * plot.bounds.height;
            bar.y = plot.bounds.y + plot.bounds.height - bar.height;
            DrawRectangleRec(bar, c_button_hover);
        }
    } else if(count == 1) {
        float t = (plot.values[offset] - min_value) / range;
        int y;
        if(t < 0.0f) t = 0.0f;
        if(t > 1.0f) t = 1.0f;
        y = (int)(plot.bounds.y + (1.0f - t) * plot.bounds.height);
        DrawLine((int)plot.bounds.x, y,
                 (int)(plot.bounds.x + plot.bounds.width), y, c_button_hover);
    } else {
        for(int i = 1; i < count; i++) {
            float a = plot.values[(offset + i - 1) % count];
            float b = plot.values[(offset + i) % count];
            float ta = (a - min_value) / range;
            float tb = (b - min_value) / range;
            int x1 = (int)(plot.bounds.x + (float)(i - 1) * plot.bounds.width / (float)(count - 1));
            int x2 = (int)(plot.bounds.x + (float)i * plot.bounds.width / (float)(count - 1));
            int y1 = (int)(plot.bounds.y + (1.0f - ta) * plot.bounds.height);
            int y2 = (int)(plot.bounds.y + (1.0f - tb) * plot.bounds.height);
            DrawLine(x1, y1, x2, y2, c_button_hover);
        }
    }
    EndUIClip();
    if(plot.label != NULL)
        DrawUIText(plot.label, (int)plot.bounds.x + ScaleUIPx(6),
                   (int)plot.bounds.y + ScaleUIPx(4), GetUISmallFontSize(), c_text);
    if(plot.overlay != NULL) {
        int font = GetUISmallFontSize();
        int width = TextWidth(plot.overlay, font);
        DrawUIText(plot.overlay,
                   (int)(plot.bounds.x + plot.bounds.width) - width - ScaleUIPx(6),
                   (int)plot.bounds.y + ScaleUIPx(4), font, c_text);
    }
}

void
DrawUIPlotLines(PlotProps plot)
{
    ui_plot(plot, 0);
}

void
DrawUIPlotHistogram(PlotProps plot)
{
    ui_plot(plot, 1);
}

static int
ui_drag_delta(int token, Rectangle bounds, int disabled, float *delta)
{
    Vector2 mouse = ui_mouse_world();
    int hot = !disabled && ui_hot(bounds);

    *delta = 0.0f;
    if(hot)
        MarkUIClickable();
    if(hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_drag_active = token;
        g_drag_last_x = mouse.x;
    }
    if(g_drag_active == token && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        *delta = mouse.x - g_drag_last_x;
        g_drag_last_x = mouse.x;
    }
    if(g_drag_active == token && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_drag_active = 0;
    return *delta != 0.0f;
}

int
DrawUIDragFloat(DragFloatProps drag)
{
    int count = drag.value_count;
    int changed = 0;
    float speed = drag.speed != 0.0f ? drag.speed : 1.0f;

    if(drag.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        Rectangle cell = {drag.bounds.x + drag.bounds.width * i / count,
                          drag.bounds.y, drag.bounds.width / count,
                          drag.bounds.height};
        float delta;
        if(ui_drag_delta((drag.id << 4) ^ (i + 1), cell,
                         drag.disabled, &delta)) {
            float value = drag.values[i] + delta * speed;
            if(drag.min < drag.max) {
                if(value < drag.min) value = drag.min;
                if(value > drag.max) value = drag.max;
            }
            if(value != drag.values[i]) {
                drag.values[i] = value;
                changed = 1;
            }
        }
        if(IsWindowReady()) {
            char text[64];
            snprintf(text, sizeof(text), drag.format != NULL ? drag.format : "%.3f",
                     drag.values[i]);
            DrawRectangleRec(cell, drag.disabled ? c_surface : c_button);
            DrawRectangleLinesEx(cell, 1.0f, c_button_hover);
            DrawUIText(text, (int)cell.x + ScaleUIPx(6),
                       ui_row_text_y(cell, GetUISmallFontSize()),
                       GetUISmallFontSize(), drag.disabled ? c_icon : c_text);
        }
    }
    if(IsWindowReady() && drag.label != NULL)
        DrawUIText(drag.label, (int)drag.bounds.x + ScaleUIPx(6),
                   (int)drag.bounds.y - GetUISmallFontSize() - ScaleUIPx(2),
                   GetUISmallFontSize(), c_text);
    return changed;
}

int
DrawUIDragInt(DragIntProps drag)
{
    int count = drag.value_count;
    int changed = 0;
    float speed = drag.speed != 0.0f ? drag.speed : 1.0f;

    if(drag.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        Rectangle cell = {drag.bounds.x + drag.bounds.width * i / count,
                          drag.bounds.y, drag.bounds.width / count,
                          drag.bounds.height};
        float delta;
        if(ui_drag_delta((drag.id << 4) ^ (i + 1), cell,
                         drag.disabled, &delta)) {
            float scaled = delta * speed;
            int step = (int)(scaled + (scaled < 0.0f ? -0.5f : 0.5f));
            int value = drag.values[i] + step;
            if(drag.min < drag.max) {
                if(value < drag.min) value = drag.min;
                if(value > drag.max) value = drag.max;
            }
            if(value != drag.values[i]) {
                drag.values[i] = value;
                changed = 1;
            }
        }
        if(IsWindowReady()) {
            char text[64];
            snprintf(text, sizeof(text), drag.format != NULL ? drag.format : "%d",
                     drag.values[i]);
            DrawRectangleRec(cell, drag.disabled ? c_surface : c_button);
            DrawRectangleLinesEx(cell, 1.0f, c_button_hover);
            DrawUIText(text, (int)cell.x + ScaleUIPx(6),
                       ui_row_text_y(cell, GetUISmallFontSize()),
                       GetUISmallFontSize(), drag.disabled ? c_icon : c_text);
        }
    }
    if(IsWindowReady() && drag.label != NULL)
        DrawUIText(drag.label, (int)drag.bounds.x + ScaleUIPx(6),
                   (int)drag.bounds.y - GetUISmallFontSize() - ScaleUIPx(2),
                   GetUISmallFontSize(), c_text);
    return changed;
}

int
DrawUIDragFloatRange2(DragFloatRange2Props drag)
{
    Rectangle low_bounds = drag.bounds;
    Rectangle high_bounds = drag.bounds;
    DragFloatProps low;
    DragFloatProps high;
    int changed;

    if(drag.current_min == NULL || drag.current_max == NULL)
        return 0;
    low_bounds.width *= 0.5f;
    high_bounds.x += low_bounds.width;
    high_bounds.width -= low_bounds.width;
    low = (DragFloatProps){low_bounds, drag.id * 2, NULL, drag.current_min, 1,
                           drag.speed, drag.min, *drag.current_max,
                           drag.format, drag.disabled};
    high = (DragFloatProps){high_bounds, drag.id * 2 + 1, NULL,
                            drag.current_max, 1, drag.speed,
                            *drag.current_min, drag.max,
                            drag.format_max != NULL ? drag.format_max : drag.format,
                            drag.disabled};
    changed = DrawUIDragFloat(low);
    changed |= DrawUIDragFloat(high);
    if(*drag.current_min > *drag.current_max)
        *drag.current_min = *drag.current_max;
    if(IsWindowReady() && drag.label != NULL)
        DrawUIText(drag.label, (int)drag.bounds.x + ScaleUIPx(6),
                   (int)drag.bounds.y - GetUISmallFontSize() - ScaleUIPx(2),
                   GetUISmallFontSize(), c_text);
    return changed;
}

int
DrawUIDragIntRange2(DragIntRange2Props drag)
{
    Rectangle low_bounds = drag.bounds;
    Rectangle high_bounds = drag.bounds;
    DragIntProps low;
    DragIntProps high;
    int changed;

    if(drag.current_min == NULL || drag.current_max == NULL)
        return 0;
    low_bounds.width *= 0.5f;
    high_bounds.x += low_bounds.width;
    high_bounds.width -= low_bounds.width;
    low = (DragIntProps){low_bounds, drag.id * 2, NULL, drag.current_min, 1,
                         drag.speed, drag.min, *drag.current_max,
                         drag.format, drag.disabled};
    high = (DragIntProps){high_bounds, drag.id * 2 + 1, NULL,
                          drag.current_max, 1, drag.speed,
                          *drag.current_min, drag.max,
                          drag.format_max != NULL ? drag.format_max : drag.format,
                          drag.disabled};
    changed = DrawUIDragInt(low);
    changed |= DrawUIDragInt(high);
    if(*drag.current_min > *drag.current_max)
        *drag.current_min = *drag.current_max;
    if(IsWindowReady() && drag.label != NULL)
        DrawUIText(drag.label, (int)drag.bounds.x + ScaleUIPx(6),
                   (int)drag.bounds.y - GetUISmallFontSize() - ScaleUIPx(2),
                   GetUISmallFontSize(), c_text);
    return changed;
}

static int
ui_slider_ratio(int token, Rectangle bounds, int disabled, int vertical,
                float *ratio)
{
    Vector2 mouse = ui_mouse_world();
    int hot = !disabled && ui_hot(bounds);
    int pressed = hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if(hot)
        MarkUIClickable();
    if(pressed)
        g_slider_active = token;
    if(g_slider_active == token &&
       (pressed || IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
        float span = vertical ? bounds.height : bounds.width;
        float position = vertical ? bounds.y + bounds.height - mouse.y
                                  : mouse.x - bounds.x;
        *ratio = span > 0.0f ? position / span : 0.0f;
        if(*ratio < 0.0f) *ratio = 0.0f;
        if(*ratio > 1.0f) *ratio = 1.0f;
        return 1;
    }
    if(g_slider_active == token && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_slider_active = 0;
    return 0;
}

static void
ui_draw_slider_cell(Rectangle cell, float ratio, const char *text,
                    int disabled, int vertical)
{
    if(!IsWindowReady())
        return;
    Color fill = disabled ? c_surface : c_button;
    Color accent = disabled ? c_icon : c_button_hover;
    DrawRectangleRec(cell, fill);
    if(vertical) {
        Rectangle progress = {cell.x, cell.y + cell.height * (1.0f - ratio),
                              cell.width, cell.height * ratio};
        DrawRectangleRec(progress, accent);
        float knob_y = cell.y + cell.height * (1.0f - ratio);
        DrawRectangle((int)cell.x, (int)(knob_y - ScaleUIPx(2)),
                      (int)cell.width, ScaleUIPx(4), c_text);
    } else {
        Rectangle progress = {cell.x, cell.y, cell.width * ratio, cell.height};
        DrawRectangleRec(progress, accent);
        float knob_x = cell.x + cell.width * ratio;
        DrawRectangle((int)(knob_x - ScaleUIPx(2)), (int)cell.y,
                      ScaleUIPx(4), (int)cell.height, c_text);
    }
    DrawRectangleLinesEx(cell, 1.0f, c_button_hover);
    DrawUIText(text, (int)cell.x + ScaleUIPx(6),
               ui_row_text_y(cell, GetUISmallFontSize()),
               GetUISmallFontSize(), disabled ? c_icon : c_text);
}

static void
ui_draw_slider_label(Rectangle bounds, const char *label)
{
    if(IsWindowReady() && label != NULL)
        DrawUIText(label, (int)bounds.x + ScaleUIPx(6),
                   (int)bounds.y - GetUISmallFontSize() - ScaleUIPx(2),
                   GetUISmallFontSize(), c_text);
}

static int
ui_slider_float(SliderFloatProps slider, int vertical)
{
    int changed = 0;
    int count = slider.value_count;
    float range = slider.max - slider.min;

    if(slider.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        Rectangle cell = {slider.bounds.x + slider.bounds.width * i / count,
                          slider.bounds.y, slider.bounds.width / count,
                          slider.bounds.height};
        float ratio = range > 0.0f ? (slider.values[i] - slider.min) / range : 0.0f;
        if(ratio < 0.0f) ratio = 0.0f;
        if(ratio > 1.0f) ratio = 1.0f;
        if(range > 0.0f && ui_slider_ratio(0x40000000 ^ (slider.id << 4) ^ (i + 1),
                                           cell, slider.disabled, vertical, &ratio)) {
            float value = slider.min + ratio * range;
            if(value != slider.values[i]) {
                slider.values[i] = value;
                changed = 1;
            }
        }
        char text[64];
        snprintf(text, sizeof(text), slider.format != NULL ? slider.format : "%.3f",
                 slider.values[i]);
        ui_draw_slider_cell(cell, ratio, text, slider.disabled, vertical);
    }
    ui_draw_slider_label(slider.bounds, slider.label);
    return changed;
}

static int
ui_slider_int(SliderIntProps slider, int vertical)
{
    int changed = 0;
    int count = slider.value_count;
    int range = slider.max - slider.min;

    if(slider.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        Rectangle cell = {slider.bounds.x + slider.bounds.width * i / count,
                          slider.bounds.y, slider.bounds.width / count,
                          slider.bounds.height};
        float ratio = range > 0 ? (float)(slider.values[i] - slider.min) / range : 0.0f;
        if(ratio < 0.0f) ratio = 0.0f;
        if(ratio > 1.0f) ratio = 1.0f;
        if(range > 0 && ui_slider_ratio(0x50000000 ^ (slider.id << 4) ^ (i + 1),
                                        cell, slider.disabled, vertical, &ratio)) {
            int value = slider.min + (int)(ratio * range + 0.5f);
            if(value != slider.values[i]) {
                slider.values[i] = value;
                changed = 1;
            }
        }
        char text[64];
        snprintf(text, sizeof(text), slider.format != NULL ? slider.format : "%d",
                 slider.values[i]);
        ui_draw_slider_cell(cell, ratio, text, slider.disabled, vertical);
    }
    ui_draw_slider_label(slider.bounds, slider.label);
    return changed;
}

int
DrawUISliderFloat(SliderFloatProps slider)
{
    return ui_slider_float(slider, 0);
}

int
DrawUISliderInt(SliderIntProps slider)
{
    return ui_slider_int(slider, 0);
}

int
DrawUIVSliderFloat(SliderFloatProps slider)
{
    return ui_slider_float(slider, 1);
}

int
DrawUIVSliderInt(SliderIntProps slider)
{
    return ui_slider_int(slider, 1);
}

int
DrawUISliderAngle(SliderAngleProps slider)
{
    const float radians_to_degrees = 57.295779513082320876f;
    const float degrees_to_radians = 0.01745329251994329577f;
    float degrees;
    SliderFloatProps value_slider;
    int changed;

    if(slider.value == NULL)
        return 0;
    degrees = *slider.value * radians_to_degrees;
    value_slider = (SliderFloatProps){slider.bounds, slider.id, slider.label,
                                      &degrees, 1, slider.min_degrees,
                                      slider.max_degrees, slider.format,
                                      slider.disabled};
    changed = ui_slider_float(value_slider, 0);
    if(changed)
        *slider.value = degrees * degrees_to_radians;
    return changed;
}

#define UI_NUMERIC_INPUT_SLOTS 128
typedef struct {
    int token;
    char text[64];
    int cursor;
    int focused;
} UINumericInputState;
static UINumericInputState g_numeric_inputs[UI_NUMERIC_INPUT_SLOTS];

static UINumericInputState *
ui_numeric_input_state(int token)
{
    unsigned int key = (unsigned int)token;
    UINumericInputState *state = &g_numeric_inputs[key % UI_NUMERIC_INPUT_SLOTS];
    if(state->token != token) {
        memset(state, 0, sizeof(*state));
        state->token = token;
    }
    return state;
}

static int
ui_numeric_input_filter(int codepoint, void *user_data)
{
    (void)user_data;
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '-' ||
           codepoint == '+' || codepoint == '.' || codepoint == 'e' ||
           codepoint == 'E';
}

static double
ui_numeric_value(const void *values, int index, int kind)
{
    if(kind == 0) return ((const float *)values)[index];
    if(kind == 1) return ((const int *)values)[index];
    return ((const double *)values)[index];
}

static void
ui_numeric_set_value(void *values, int index, int kind, double value)
{
    if(kind == 0) ((float *)values)[index] = (float)value;
    else if(kind == 1) ((int *)values)[index] = (int)value;
    else ((double *)values)[index] = value;
}

static void
ui_numeric_format(char *text, size_t text_size, const char *format,
                  int kind, double value)
{
    if(kind == 0)
        snprintf(text, text_size, format != NULL ? format : "%.3f", (float)value);
    else if(kind == 1)
        snprintf(text, text_size, format != NULL ? format : "%d", (int)value);
    else
        snprintf(text, text_size, format != NULL ? format : "%.6f", value);
}

static int
ui_numeric_input(Rectangle bounds, int id, const char *label, void *values,
                 int count, double step, double step_fast, const char *format,
                 int disabled, int kind)
{
    int changed = 0;
    int paint = IsWindowReady();

    if(values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int token = (0x60000000 + kind * 0x08000000) ^ (id << 4) ^ (i + 1);
        UINumericInputState *state = ui_numeric_input_state(token);
        Rectangle cell = {bounds.x + bounds.width * i / count, bounds.y,
                          bounds.width / count, bounds.height};
        Rectangle field_bounds = cell;
        Rectangle minus = cell;
        Rectangle plus = cell;
        int commit = 0;
        double old_value = ui_numeric_value(values, i, kind);

        if(!state->focused) {
            ui_numeric_format(state->text, sizeof(state->text), format, kind,
                              old_value);
            state->cursor = (int)strlen(state->text);
        }
        if(step != 0.0) {
            int button_w = ScaleUIPx(24);
            field_bounds.width -= button_w * 2;
            minus.x = field_bounds.x + field_bounds.width;
            minus.width = button_w;
            plus.x = minus.x + minus.width;
            plus.width = button_w;
        }
        if(paint && RenderTextField((TextFieldProps){field_bounds, state->text,
                                                     sizeof(state->text), &state->cursor,
                                                     &state->focused, 63,
                                                     GetUISmallFontSize(), token,
                                                     kryon_zero_text_input_style,
                                                     ui_numeric_input_filter, NULL,
                                                     &commit, 0, disabled})) {
            char *end = NULL;
            double value = strtod(state->text, &end);
            if(end != state->text && *end == '\0') {
                if(kind == 1)
                    value = value < 0.0 ? (double)((int)(value - 0.5))
                                        : (double)((int)(value + 0.5));
                if(value != old_value) {
                    ui_numeric_set_value(values, i, kind, value);
                    changed = 1;
                }
            }
        }
        if(paint && step != 0.0) {
            int fast = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            double increment = fast && step_fast != 0.0 ? step_fast : step;
            int minus_pressed = RenderButton((ButtonSpec){minus, "-", GetUISmallFontSize(),
                token + 1, disabled, c_button, c_button_hover, c_text, c_button, 0.0f});
            int plus_pressed = RenderButton((ButtonSpec){plus, "+", GetUISmallFontSize(),
                token + 2, disabled, c_button, c_button_hover, c_text, c_button, 0.0f});
            if(minus_pressed || plus_pressed) {
                double value = ui_numeric_value(values, i, kind) +
                               (plus_pressed ? increment : -increment);
                if(kind == 1)
                    value = value < 0.0 ? (double)((int)(value - 0.5))
                                        : (double)((int)(value + 0.5));
                ui_numeric_set_value(values, i, kind, value);
                ui_numeric_format(state->text, sizeof(state->text), format,
                                  kind, value);
                state->cursor = (int)strlen(state->text);
                changed = 1;
            }
        }
        (void)commit;
    }
    ui_draw_slider_label(bounds, label);
    return changed;
}

int
DrawUIInputFloat(InputFloatProps input)
{
    return ui_numeric_input(input.bounds, input.id, input.label, input.values,
                            input.value_count, input.step, input.step_fast,
                            input.format, input.disabled, 0);
}

int
DrawUIInputInt(InputIntProps input)
{
    return ui_numeric_input(input.bounds, input.id, input.label, input.values,
                            input.value_count, input.step, input.step_fast,
                            input.format, input.disabled, 1);
}

int
DrawUIInputDouble(InputDoubleProps input)
{
    return ui_numeric_input(input.bounds, input.id, input.label, input.values,
                            input.value_count, input.step, input.step_fast,
                            input.format, input.disabled, 2);
}

int
DrawUISpinbox(SpinboxProps spinbox)
{
    int button_w = ScaleUIPx(28);
    int changed = 0;
    char value_text[32];
    Rectangle left = spinbox.bounds;
    Rectangle right = spinbox.bounds;
    Rectangle text = spinbox.bounds;

    if(spinbox.step <= 0)
        spinbox.step = 1;
    left.width = button_w;
    right.x = spinbox.bounds.x + spinbox.bounds.width - button_w;
    right.width = button_w;
    text.x += button_w;
    text.width -= button_w * 2;

    if(spinbox.disabled)
        MarkUIDisabled();
    DrawRectangleRec(text, c_surface);
    DrawRectangleLinesEx(spinbox.bounds, 1.0f, c_button);
    if(spinbox.value_text != NULL)
        snprintf(value_text, sizeof(value_text), "%s", spinbox.value_text);
    else
        snprintf(value_text, sizeof(value_text), "%d", spinbox.value != NULL ? *spinbox.value : 0);
    DrawCenteredUIText(value_text, (int)(text.x + text.width / 2), (int)(text.y + text.height / 2),
                       GetUIFontSize(), c_text);
    if(RenderButton((ButtonSpec){left, "-", GetUIFontSize(), spinbox.id * 10 + 1, spinbox.disabled,
                               c_button, c_button_hover, c_text, c_button, 0.0f}) &&
       spinbox.value != NULL) {
        if(*spinbox.value > spinbox.min) {
            *spinbox.value -= spinbox.step;
            changed = 1;
        } else if(spinbox.wrap && *spinbox.value <= spinbox.min) {
            *spinbox.value = spinbox.max;
            changed = 1;
        }
        if(*spinbox.value < spinbox.min)
            *spinbox.value = spinbox.min;
    }
    if(RenderButton((ButtonSpec){right, "+", GetUIFontSize(), spinbox.id * 10 + 2, spinbox.disabled,
                               c_button, c_button_hover, c_text, c_button, 0.0f}) &&
       spinbox.value != NULL) {
        if(*spinbox.value < spinbox.max) {
            *spinbox.value += spinbox.step;
            changed = 1;
        } else if(spinbox.wrap && *spinbox.value >= spinbox.max) {
            *spinbox.value = spinbox.min;
            changed = 1;
        }
        if(*spinbox.value > spinbox.max)
            *spinbox.value = spinbox.max;
    }
    return changed;
}

int
DrawUICombobox(ComboboxProps combo)
{
    if(combo.disabled)
        MarkUIDisabled();
    return DrawUIDropdown(combo.id, (int)combo.bounds.x, (int)combo.bounds.y,
                          (int)combo.bounds.width, (int)combo.bounds.height,
                          combo.options, combo.option_count,
                          combo.selected_index);
}

void
DrawUILabelFrame(LabelFrameProps frame)
{
    int font = GetUISmallFontSize();
    DrawRectangleLinesEx(frame.bounds, 1.0f, c_button);
    if(frame.title != NULL) {
        int pad = ScaleUIPx(8);
        int w = TextWidth(frame.title, font) + pad * 2;
        DrawRectangle((int)frame.bounds.x + pad, (int)frame.bounds.y - ScaleUIPx(8),
                      w, ScaleUIPx(18), c_bg);
        DrawUIText(frame.title, (int)frame.bounds.x + pad * 2,
                   (int)frame.bounds.y - ScaleUIPx(9), font, c_text);
    }
}

void
DrawUIImageBox(ImageBoxProps image)
{
    DrawRectangleRec(image.bounds, c_surface);
    if(image.texture.id != 0)
        DrawTexturePro(image.texture,
                       (Rectangle){0, 0, (float)image.texture.width, (float)image.texture.height},
                       image.bounds, (Vector2){0, 0}, 0.0f,
                       image.tint.a == 0 ? WHITE : image.tint);
    DrawRectangleLinesEx(image.bounds, 1.0f, c_button);
}

int
DrawUIListBox(ListBoxProps list)
{
    int paint = IsWindowReady();
    int font = GetUIFontSize();
    int selected = list.selected_index != NULL ? *list.selected_index : -1;
    int row_h = list.row_height > 0 ? ScaleUIPx(list.row_height) : ScaleUIPx(30);
    int scroll_y;
    int first;
    int y_offset;
    int visible = (int)list.bounds.height / row_h;
    int max_scroll;
    int changed = 0;

    max_scroll = ui_update_scroll(list.bounds, list.item_count * row_h,
                                  list.scroll_offset, row_h);
    scroll_y = list.scroll_offset != NULL ? *list.scroll_offset : 0;
    first = scroll_y / row_h;
    y_offset = scroll_y % row_h;
    if(paint) {
        ui_draw_panel(list.bounds);
        BeginUIClip((int)list.bounds.x, (int)list.bounds.y,
                    (int)list.bounds.width, (int)list.bounds.height);
    }
    for(int i = 0; i <= visible && first + i < list.item_count; i++) {
        int index = first + i;
        Rectangle row = {list.bounds.x, list.bounds.y + (float)(i * row_h - y_offset),
                         list.bounds.width, (float)row_h};
        int hot = ui_hot(row);
        if(paint && index == selected)
            DrawRectangleRec(row, c_button);
        else if(paint && hot)
            DrawRectangleRec(row, c_button_hover);
        if(hot)
            MarkUIClickable();
        if(paint)
            DrawUIText(list.items != NULL && list.items[index] != NULL ? list.items[index] : "",
                       (int)row.x + ScaleUIPx(8), ui_row_text_y(row, font), font, c_text);
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && list.selected_index != NULL) {
            UIConsumeRelease();
            *list.selected_index = index;
            changed = 1;
        }
    }
    if(paint)
        EndUIClip();
    if(paint && list.scroll_offset != NULL && max_scroll > 0)
        DrawUIScrollbar((int)(list.bounds.x + list.bounds.width - ScaleUIPx(8)),
                        (int)list.bounds.y, (int)list.bounds.height,
                        list.item_count * row_h, list.scroll_offset, max_scroll);
    return changed;
}

int
DrawUITreeView(TreeViewProps tree)
{
    int paint = IsWindowReady();
    int font = GetUIFontSize();
    int row_h = tree.row_height > 0 ? ScaleUIPx(tree.row_height) : ScaleUIPx(28);
    int scroll_y;
    int first;
    int y_offset;
    int visible = (int)tree.bounds.height / row_h;
    int max_scroll;
    int changed = 0;

    max_scroll = ui_update_scroll(tree.bounds, tree.item_count * row_h,
                                  tree.scroll_offset, row_h);
    scroll_y = tree.scroll_offset != NULL ? *tree.scroll_offset : 0;
    first = scroll_y / row_h;
    y_offset = scroll_y % row_h;
    if(paint) {
        ui_draw_panel(tree.bounds);
        BeginUIClip((int)tree.bounds.x, (int)tree.bounds.y,
                    (int)tree.bounds.width, (int)tree.bounds.height);
    }
    for(int i = 0; i <= visible && first + i < tree.item_count; i++) {
        int index = first + i;
        const UITreeItem *item = &tree.items[index];
        Rectangle row = {tree.bounds.x, tree.bounds.y + (float)(i * row_h - y_offset),
                         tree.bounds.width, (float)row_h};
        int hot = ui_hot(row);
        int x = (int)row.x + ScaleUIPx(8 + item->depth * 18);
        if(paint && tree.selected_id != NULL && *tree.selected_id == item->id)
            DrawRectangleRec(row, c_button);
        else if(paint && hot)
            DrawRectangleRec(row, c_button_hover);
        if(paint) {
            if(item->expanded)
                DrawUIText("v", x, ui_row_text_y(row, font), font, c_icon);
            else
                DrawUIText(">", x, ui_row_text_y(row, font), font, c_icon);
            DrawUIText(item->label != NULL ? item->label : "",
                       x + ScaleUIPx(18), ui_row_text_y(row, font), font,
                       c_text);
        }
        if(hot)
            MarkUIClickable();
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && item->selectable && tree.selected_id != NULL) {
            UIConsumeRelease();
            *tree.selected_id = item->id;
            changed = 1;
        }
    }
    if(paint)
        EndUIClip();
    if(paint && tree.scroll_offset != NULL && max_scroll > 0)
        DrawUIScrollbar((int)(tree.bounds.x + tree.bounds.width - ScaleUIPx(8)),
                        (int)tree.bounds.y, (int)tree.bounds.height,
                        tree.item_count * row_h, tree.scroll_offset, max_scroll);
    return changed;
}

static int
ui_tree_expanded_index(UICascadingTreeExpansion expansion, int id)
{
    int count = expansion.count != NULL ? *expansion.count : 0;

    for(int i = 0; i < count; i++) {
        if(expansion.ids != NULL && expansion.ids[i] == id)
            return i;
    }
    return -1;
}

static int
ui_tree_is_expanded(UICascadingTreeExpansion expansion, int id)
{
    return ui_tree_expanded_index(expansion, id) >= 0;
}

static void
ui_tree_toggle_expanded(UICascadingTreeExpansion *expansion, int id)
{
    int index;
    int count;

    if(expansion == NULL || expansion->ids == NULL ||
       expansion->count == NULL || expansion->capacity <= 0)
        return;
    count = *expansion->count;
    index = ui_tree_expanded_index(*expansion, id);
    if(index >= 0) {
        for(int i = index; i + 1 < count; i++)
            expansion->ids[i] = expansion->ids[i + 1];
        if(count > 0)
            expansion->ids[count - 1] = 0;
        *expansion->count = count - 1;
        return;
    }
    if(count >= expansion->capacity)
        return;
    expansion->ids[count++] = id;
    *expansion->count = count;
}

static void
ui_tree_remove_expanded(UICascadingTreeExpansion *expansion, int id)
{
    int index;
    int count;

    if(expansion == NULL || expansion->ids == NULL || expansion->count == NULL)
        return;
    count = *expansion->count;
    index = ui_tree_expanded_index(*expansion, id);
    if(index < 0)
        return;
    for(int i = index; i + 1 < count; i++)
        expansion->ids[i] = expansion->ids[i + 1];
    if(count > 0)
        expansion->ids[count - 1] = 0;
    *expansion->count = count - 1;
}

static void
ui_tree_collapse_item_and_children(const UICascadingTreeItem *items,
                                   int item_count, int index,
                                   UICascadingTreeExpansion *expansion)
{
    int depth;

    if(items == NULL || index < 0 || index >= item_count || expansion == NULL)
        return;
    depth = items[index].depth;
    ui_tree_remove_expanded(expansion, items[index].id);
    for(int i = index + 1; i < item_count; i++) {
        if(items[i].depth <= depth)
            break;
        if(items[i].is_dir)
            ui_tree_remove_expanded(expansion, items[i].id);
    }
}

static int
ui_tree_item_visible(const UICascadingTreeItem *items, int index,
                     UICascadingTreeExpansion expansion)
{
    int depth;

    if(items == NULL || index < 0)
        return 0;
    depth = items[index].depth;
    for(int i = index - 1; i >= 0; i--) {
        if(items[i].depth >= depth)
            continue;
        if(items[i].is_dir && !ui_tree_is_expanded(expansion, items[i].id))
            return 0;
        depth = items[i].depth;
        if(depth <= 0)
            break;
    }
    return 1;
}

static void
ui_draw_tree_text(const char *text, Rectangle rect, int font, Color color)
{
    const char *value = text != NULL ? text : "";
    int y;

    if(rect.width <= 0 || rect.height <= 0)
        return;
    y = TextBaselineY(value, (int)rect.y, (int)rect.height, font);
    BeginUIClip((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
    DrawUIText(value, (int)rect.x, y, font, color);
    EndUIClip();
}

static void
ui_draw_tree_disclosure(Rectangle box, int expanded, int hot)
{
    Color bg = hot ? GetThemeButtonHover() : GetThemeSurface();
    Color border = hot ? GetThemeIcon() : GetThemeButton();
    Color mark = GetThemeIcon();
    int cx = (int)(box.x + box.width / 2.0f);
    int cy = (int)(box.y + box.height / 2.0f);
    int pad = ScaleUIPx(4);

    DrawRectangleRec(box, bg);
    DrawRectangleLinesEx(box, 1.0f, border);
    DrawLine((int)box.x + pad, cy, (int)(box.x + box.width) - pad, cy,
             mark);
    if(!expanded)
        DrawLine(cx, (int)box.y + pad, cx, (int)(box.y + box.height) - pad,
                 mark);
}

static void
ui_draw_tree_file_mark(Rectangle box, int hot)
{
    Color border = hot ? GetThemeIcon() : GetThemeButton();
    int inset = ScaleUIPx(3);

    DrawRectangleLines((int)box.x + inset, (int)box.y + inset,
                       (int)box.width - inset * 2,
                       (int)box.height - inset * 2, border);
    DrawLine((int)(box.x + box.width) - inset * 2,
             (int)box.y + inset,
             (int)(box.x + box.width) - inset,
             (int)box.y + inset * 2, border);
}

int
DrawUICascadingTreeView(CascadingTreeViewProps tree)
{
    int font = GetUIFontSize();
    int row_h = tree.row_height > 0 ? ScaleUIPx(tree.row_height) : ScaleUIPx(28);
    Vector2 mouse = ui_mouse_world();
    int blocked = UIInputCapturesClick(mouse);
    int contains;
    int scroll_y;
    int max_scroll;
    int visible_count = 0;
    int changed = 0;

    if(row_h <= 0)
        row_h = 1;
    for(int i = 0; i < tree.item_count; i++) {
        if(ui_tree_item_visible(tree.items, i, tree.expanded))
            visible_count++;
    }
    contains = ui_contains(tree.bounds, mouse);
    max_scroll = ui_update_scroll(tree.bounds, visible_count * row_h,
                                  tree.scroll_offset, row_h);
    scroll_y = tree.scroll_offset != NULL ? *tree.scroll_offset : 0;
    if(contains && !blocked) {
        PushUIInputCapture(tree.bounds, 1);
    }

    ui_draw_panel(tree.bounds);
    BeginUIClip((int)tree.bounds.x, (int)tree.bounds.y,
                (int)tree.bounds.width, (int)tree.bounds.height);
    visible_count = 0;
    for(int i = 0; i < tree.item_count; i++) {
        const UICascadingTreeItem *item;
        Rectangle row;
        Rectangle text_rect;
        Rectangle mark;
        int hot;
        int expanded;
        int x;
        int y;
        int mark_size = ScaleUIPx(16);
        int gap = ScaleUIPx(8);

        if(!ui_tree_item_visible(tree.items, i, tree.expanded))
            continue;
        item = &tree.items[i];
        y = (int)tree.bounds.y + visible_count * row_h - scroll_y;
        visible_count++;
        if(y + row_h < (int)tree.bounds.y ||
           y > (int)(tree.bounds.y + tree.bounds.height))
            continue;

        row = (Rectangle){tree.bounds.x, (float)y, tree.bounds.width,
                          (float)row_h};
        hot = ui_contains(row, mouse) && !blocked;
        expanded = item->is_dir && ui_tree_is_expanded(tree.expanded, item->id);
        x = (int)row.x + ScaleUIPx(8 + item->depth * 18);
        if(tree.selected_id != NULL && *tree.selected_id == item->id)
            DrawRectangleRec(row, GetThemeButton());
        else if(hot)
            DrawRectangleRec(row, GetThemeButtonHover());
        if(hot)
            MarkUIClickable();

        mark = (Rectangle){
            (float)x,
            row.y + ((float)row_h - (float)mark_size) * 0.5f,
            (float)mark_size,
            (float)mark_size
        };
        if(item->is_dir)
            ui_draw_tree_disclosure(mark, expanded, hot);
        else
            ui_draw_tree_file_mark(mark, hot);
        text_rect = (Rectangle){
            mark.x + mark.width + (float)gap,
            row.y,
            tree.bounds.x + tree.bounds.width - (float)ScaleUIPx(10) -
                (mark.x + mark.width + (float)gap),
            row.height
        };
        ui_draw_tree_text(item->label != NULL ? item->label : "", text_rect,
                          font, item->is_dir ? GetThemeText()
                                             : Fade(GetThemeText(), 0.72f));

        if(hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(item->is_dir) {
                if(expanded)
                    ui_tree_collapse_item_and_children(tree.items,
                                                       tree.item_count, i,
                                                       &tree.expanded);
                else
                    ui_tree_toggle_expanded(&tree.expanded, item->id);
                changed = 1;
            } else if(item->selectable && tree.selected_id != NULL) {
                *tree.selected_id = item->id;
                if(tree.activated_id != NULL)
                    *tree.activated_id = item->id;
                changed = 1;
            }
        }
    }
    EndUIClip();
    if(tree.scroll_offset != NULL && max_scroll > 0)
        DrawUIScrollbar((int)(tree.bounds.x + tree.bounds.width - ScaleUIPx(8)),
                        (int)tree.bounds.y, (int)tree.bounds.height,
                        visible_count * row_h, tree.scroll_offset, max_scroll);
    return changed;
}

static int
ui_source_line_count(const char *text)
{
    int count = 1;

    if(text == NULL || text[0] == '\0')
        return 0;
    for(const char *p = text; *p != '\0'; p++) {
        if(*p == '\n')
            count++;
    }
    return count;
}

static int
ui_source_expand_line(char *dst, size_t dst_size, const char *text, int len,
                      int tab_width)
{
    int col = 0;
    int out = 0;

    if(dst == NULL || dst_size == 0)
        return 0;
    if(text == NULL || len <= 0) {
        dst[0] = '\0';
        return 0;
    }
    if(tab_width <= 0)
        tab_width = 4;
    for(int i = 0; i < len && out < (int)dst_size - 1; i++) {
        unsigned char c = (unsigned char)text[i];

        if(c == '\t') {
            int spaces = tab_width - (col % tab_width);
            while(spaces-- > 0 && out < (int)dst_size - 1) {
                dst[out++] = ' ';
                col++;
            }
        } else if(c == '\r') {
            continue;
        } else if(c < 32) {
            dst[out++] = ' ';
            col++;
        } else {
            dst[out++] = (char)c;
            col++;
        }
    }
    dst[out] = '\0';
    return out;
}

static int
ui_source_line_width(const char *text, int len, int font)
{
    char line[1024];
    int n;

    if(text == NULL)
        return 0;
    n = ui_source_expand_line(line, sizeof(line), text, len, 4);
    line[n] = '\0';
    return TextWidth(line, font);
}

static int
ui_source_max_line_width(const char *text, int font)
{
    int max_w = 0;
    const char *line;

    if(text == NULL)
        return 0;
    line = text;
    while(*line != '\0') {
        const char *end = strchr(line, '\n');
        int width;

        if(end == NULL)
            end = line + strlen(line);
        width = ui_source_line_width(line, (int)(end - line), font);
        if(width > max_w)
            max_w = width;
        if(*end == '\0')
            break;
        line = end + 1;
    }
    return max_w;
}

static void
ui_draw_source_line(const char *text, int len, int x, int y, int font,
                    Color color)
{
    char line[1024];
    int n;

    if(text == NULL)
        return;
    n = ui_source_expand_line(line, sizeof(line), text, len, 4);
    line[n] = '\0';
    DrawUIText(line, x, y, font, color);
}

int
DrawUISourceView(SourceViewProps source)
{
    const char *text = source.text != NULL ? source.text : "";
    int font = source.font_size > 0 ? source.font_size : GetUISmallFontSize();
    int line_h = source.line_height > 0 ? ScaleUIPx(source.line_height)
                                        : TextLineHeight(font) + ScaleUIPx(4);
    int pad = ScaleUIPx(12);
    int gutter_w = source.show_line_numbers ? ScaleUIPx(58) : 0;
    Rectangle view;
    int line_count = ui_source_line_count(text);
    int content_h;
    int content_w;
    int max_scroll_y;
    int max_scroll_x = 0;
    int scroll_y = source.scroll_y != NULL ? *source.scroll_y : 0;
    int scroll_x = source.scroll_x != NULL ? *source.scroll_x : 0;
    int first_line;
    int y_offset;
    int y;
    int line_no = 1;
    const char *line;
    Vector2 mouse;

    if(line_h <= 0)
        line_h = 1;
    ui_draw_panel(source.bounds);
    view = (Rectangle){source.bounds.x + (float)pad,
                       source.bounds.y + (float)pad,
                       source.bounds.width - (float)(pad * 2),
                       source.bounds.height - (float)(pad * 2)};
    if(view.width <= 0.0f || view.height <= 0.0f)
        return 0;
    mouse = ui_mouse_world();
    if(ui_contains(view, mouse)) {
        PushUIInputCapture(view, 1);
        MarkUIClickable();
    }

    content_h = line_count * line_h;
    content_w = gutter_w + ui_source_max_line_width(text, font) + ScaleUIPx(24);
    max_scroll_y = ui_update_scroll(view, content_h, source.scroll_y, line_h);
    if(source.scroll_y != NULL)
        scroll_y = *source.scroll_y;
    if(source.scroll_x != NULL) {
        max_scroll_x = ui_scroll_max(content_w, (int)view.width);
        if(*source.scroll_x < 0)
            *source.scroll_x = 0;
        if(*source.scroll_x > max_scroll_x)
            *source.scroll_x = max_scroll_x;
        scroll_x = *source.scroll_x;
    }

    first_line = scroll_y / line_h;
    y_offset = scroll_y % line_h;
    y = (int)view.y - y_offset;
    line = text;
    while(*line != '\0' && line_no <= first_line) {
        const char *next = strchr(line, '\n');
        if(next == NULL)
            break;
        line = next + 1;
        line_no++;
    }

    BeginUIClip((int)view.x, (int)view.y, (int)view.width, (int)view.height);
    while(*line != '\0' && y < (int)(view.y + view.height)) {
        const char *end = strchr(line, '\n');
        int len;

        if(end == NULL)
            end = line + strlen(line);
        len = (int)(end - line);
        if(source.show_line_numbers) {
            DrawUIText(TextFormat("%d", line_no), (int)view.x, y, font,
                       GetThemeIcon());
        }
        BeginUIClip((int)view.x + gutter_w, y,
                    (int)view.width - gutter_w - ScaleUIPx(10), line_h);
        ui_draw_source_line(line, len,
                            (int)view.x + gutter_w - scroll_x, y, font,
                            GetThemeText());
        EndUIClip();
        if(*end == '\0')
            break;
        line = end + 1;
        line_no++;
        y += line_h;
    }
    EndUIClip();

    if(source.scroll_y != NULL && max_scroll_y > 0)
        DrawUIScrollbar((int)(source.bounds.x + source.bounds.width -
                              ScaleUIPx(8)),
                        (int)source.bounds.y, (int)source.bounds.height,
                        content_h, source.scroll_y, max_scroll_y);
    return max_scroll_x > 0 || max_scroll_y > 0;
}

int
DrawUITableView(TableViewProps table)
{
    static int last_table_id = 0;
    static int last_table_row = -1;
    static int last_table_column = -1;
    static double last_table_click_time = 0.0;
    int paint = IsWindowReady();
    int font = GetUISmallFontSize();
    int row_h = table.row_height > 0 ? ScaleUIPx(table.row_height) : ScaleUIPx(28);
    int header_h = ScaleUIPx(30);
    int default_col_w;
    int scroll_y;
    int first;
    int y_offset;
    int visible;
    int max_scroll;
    int changed = 0;

    if(table.column_count < 1)
        return 0;
    if(table.activated_row != NULL)
        *table.activated_row = -1;
    if(table.activated_column != NULL)
        *table.activated_column = -1;
    if(table.right_clicked_row != NULL)
        *table.right_clicked_row = -1;
    if(table.right_clicked_column != NULL)
        *table.right_clicked_column = -1;
    default_col_w = (int)table.bounds.width / table.column_count;
    max_scroll = ui_update_scroll((Rectangle){table.bounds.x, table.bounds.y + header_h,
                                              table.bounds.width, table.bounds.height - header_h},
                                  table.row_count * row_h, table.scroll_offset, row_h);
    scroll_y = table.scroll_offset != NULL ? *table.scroll_offset : 0;
    first = scroll_y / row_h;
    y_offset = scroll_y % row_h;
    visible = (int)(table.bounds.height - header_h) / row_h;
    if(paint)
        ui_draw_panel(table.bounds);

    for(int c = 0; c < table.column_count; c++) {
        int x = (int)table.bounds.x;
        int col_w = table.column_widths != NULL ? table.column_widths[c] : default_col_w;
        for(int prev = 0; prev < c; prev++)
            x += table.column_widths != NULL ? table.column_widths[prev] : default_col_w;
        Rectangle head = {(float)x, table.bounds.y, (float)col_w, (float)header_h};
        if(paint) {
            DrawRectangleRec(head, DarkenUIColor(c_bg, 10));
            DrawRectangleLinesEx(head, 1.0f, DarkenUIColor(c_bg, 28));
            DrawUIText(table.columns != NULL && table.columns[c] != NULL ? table.columns[c] : "",
                       (int)head.x + ScaleUIPx(6), ui_row_text_y(head, font), font, c_text);
        }
        if(ui_clicked(head) && table.sort_column != NULL) {
            if(table.selected_row != NULL)
                *table.selected_row = -1;
            if(table.selected_column != NULL)
                *table.selected_column = c;
            *table.sort_column = c;
            changed = 1;
        }
    }

    if(paint)
        BeginUIClip((int)table.bounds.x, (int)(table.bounds.y + header_h),
                    (int)table.bounds.width, (int)(table.bounds.height - header_h));
    for(int i = 0; i <= visible && first + i < table.row_count; i++) {
        int r = first + i;
        Rectangle row = {table.bounds.x, table.bounds.y + header_h + (float)(i * row_h - y_offset),
                         table.bounds.width, (float)row_h};
        int hot = ui_hot(row);
        if(paint && table.selected_row != NULL && *table.selected_row == r)
            DrawRectangleRec(row, DarkenUIColor(c_bg, 18));
        else if(paint && hot)
            DrawRectangleRec(row, c_button_hover);
        if(hot)
            MarkUIClickable();
        for(int c = 0; c < table.column_count; c++) {
            int x = (int)table.bounds.x;
            int col_w = table.column_widths != NULL ? table.column_widths[c] : default_col_w;
            const char *text = "";
            for(int prev = 0; prev < c; prev++)
                x += table.column_widths != NULL ? table.column_widths[prev] : default_col_w;
            if(table.rows != NULL && table.rows[r].cells != NULL && c < table.rows[r].cell_count)
                text = table.rows[r].cells[c] != NULL ? table.rows[r].cells[c] : "";
            if(paint) {
                BeginUIClip(x, (int)row.y, col_w, (int)row.height);
                DrawUIText(text, x + ScaleUIPx(6), ui_row_text_y(row, font), font, c_text);
                EndUIClip();
            }
        }
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && table.selected_row != NULL) {
            int clicked_col = -1;
            double now = GetTime();
            Vector2 mouse = GetMousePosition();
            UIConsumeRelease();
            *table.selected_row = r;
            for(int c = 0; c < table.column_count; c++) {
                int x = (int)table.bounds.x;
                int col_w = table.column_widths != NULL ? table.column_widths[c] : default_col_w;
                for(int prev = 0; prev < c; prev++)
                    x += table.column_widths != NULL ? table.column_widths[prev] : default_col_w;
                if(mouse.x >= (float)x && mouse.x < (float)(x + col_w)) {
                    clicked_col = c;
                    break;
                }
            }
            if(clicked_col >= 0 && table.selected_column != NULL)
                *table.selected_column = clicked_col;
            if(clicked_col >= 0 && last_table_id == table.id &&
               last_table_row == r && last_table_column == clicked_col &&
               now - last_table_click_time <= 0.45) {
                if(table.activated_row != NULL)
                    *table.activated_row = r;
                if(table.activated_column != NULL)
                    *table.activated_column = clicked_col;
            }
            last_table_id = table.id;
            last_table_row = r;
            last_table_column = clicked_col;
            last_table_click_time = now;
            changed = 1;
        }
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            int clicked_col = -1;
            Vector2 mouse = GetMousePosition();
            for(int c = 0; c < table.column_count; c++) {
                int x = (int)table.bounds.x;
                int col_w = table.column_widths != NULL ? table.column_widths[c] : default_col_w;
                for(int prev = 0; prev < c; prev++)
                    x += table.column_widths != NULL ? table.column_widths[prev] : default_col_w;
                if(mouse.x >= (float)x && mouse.x < (float)(x + col_w)) {
                    clicked_col = c;
                    break;
                }
            }
            if(table.right_clicked_row != NULL)
                *table.right_clicked_row = r;
            if(table.right_clicked_column != NULL)
                *table.right_clicked_column = clicked_col;
            changed = 1;
        }
    }
    if(paint)
        EndUIClip();
    if(paint && table.scroll_offset != NULL && max_scroll > 0)
        DrawUIScrollbar((int)(table.bounds.x + table.bounds.width - ScaleUIPx(8)),
                        (int)(table.bounds.y + header_h),
                        (int)(table.bounds.height - header_h),
                        table.row_count * row_h, table.scroll_offset, max_scroll);
    return changed;
}

Vector2
CanvasToScreen(Canvas canvas, Vector2 point)
{
    float zoom = canvas.zoom != NULL && *canvas.zoom > 0.01f ? *canvas.zoom : 1.0f;
    if(canvas.scroll_x != NULL)
        point.x -= (float)*canvas.scroll_x;
    if(canvas.scroll_y != NULL)
        point.y -= (float)*canvas.scroll_y;
    point.x = canvas.bounds.x + (point.x - canvas.bounds.x) * zoom;
    point.y = canvas.bounds.y + (point.y - canvas.bounds.y) * zoom;
    return point;
}

Rectangle
CanvasRectToScreen(Canvas canvas, Rectangle rect)
{
    float zoom = canvas.zoom != NULL && *canvas.zoom > 0.01f ? *canvas.zoom : 1.0f;
    Vector2 p = CanvasToScreen(canvas, (Vector2){rect.x, rect.y});
    return (Rectangle){p.x, p.y, rect.width * zoom, rect.height * zoom};
}

CanvasResult
BeginCanvas(Canvas canvas)
{
    CanvasResult result = {0};
    Vector2 mouse = ui_mouse_world();

    ui_draw_panel(canvas.bounds);
    result.active = ui_contains(canvas.bounds, mouse);
    result.world = mouse;
    if(canvas.scroll_x != NULL)
        result.world.x += (float)*canvas.scroll_x;
    if(canvas.scroll_y != NULL)
        result.world.y += (float)*canvas.scroll_y;
    if(canvas.zoom != NULL && *canvas.zoom > 0.01f) {
        result.world.x = canvas.bounds.x + (result.world.x - canvas.bounds.x) / *canvas.zoom;
        result.world.y = canvas.bounds.y + (result.world.y - canvas.bounds.y) / *canvas.zoom;
    }
    if(result.active && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        result.dragging = 1;
    BeginUIClip((int)canvas.bounds.x, (int)canvas.bounds.y,
                (int)canvas.bounds.width, (int)canvas.bounds.height);
    g_canvas_depth++;
    if(canvas.scroll_x != NULL || canvas.scroll_y != NULL ||
       (canvas.zoom != NULL && *canvas.zoom > 0.01f && *canvas.zoom != 1.0f)) {
        Camera2D camera = {0};
        camera.target = (Vector2){canvas.bounds.x + (canvas.scroll_x != NULL ? (float)*canvas.scroll_x : 0.0f),
                                  canvas.bounds.y + (canvas.scroll_y != NULL ? (float)*canvas.scroll_y : 0.0f)};
        camera.offset = (Vector2){canvas.bounds.x, canvas.bounds.y};
        camera.rotation = 0.0f;
        camera.zoom = canvas.zoom != NULL && *canvas.zoom > 0.01f ? *canvas.zoom : 1.0f;
        BeginMode2D(camera);
        g_canvas_mode_depth++;
    }
    return result;
}

void
EndCanvas(Canvas canvas)
{
    if(g_canvas_depth > 0) {
        if(g_canvas_mode_depth > 0) {
            g_canvas_mode_depth--;
            EndMode2D();
        }
        g_canvas_depth--;
        EndUIClip();
    }
    DrawRectangleLinesEx(canvas.bounds, 1.0f, c_button);
}

void
DrawUICanvasGrid(Rectangle bounds, int step, Color color)
{
    int scaled = ScaleUIPx(step);
    if(scaled < 4)
        scaled = 4;
    for(int x = (int)bounds.x; x < (int)(bounds.x + bounds.width); x += scaled)
        DrawLine(x, (int)bounds.y, x, (int)(bounds.y + bounds.height), color);
    for(int y = (int)bounds.y; y < (int)(bounds.y + bounds.height); y += scaled)
        DrawLine((int)bounds.x, y, (int)(bounds.x + bounds.width), y, color);
}

int
CanvasHitTest(Vector2 point, Rectangle *items, int item_count)
{
    if(items == NULL)
        return -1;
    for(int i = item_count - 1; i >= 0; i--) {
        if(ui_contains(items[i], point))
            return i;
    }
    return -1;
}

int
DrawUINotebook(NotebookProps notebook)
{
    int font = GetUIFontSize();
    int changed = 0;
    int x = (int)notebook.bounds.x;
    int tab_h = ScaleUIPx(34);

    for(int i = 0; i < notebook.tab_count; i++) {
        int w = TextWidth(notebook.tabs[i], font) + ScaleUIPx(28);
        Rectangle tab = {(float)x, notebook.bounds.y, (float)w, (float)tab_h};
        int selected = notebook.selected_index != NULL && *notebook.selected_index == i;
        int hot = ui_hot(tab);
        DrawRectangleRec(tab, selected ? c_surface : c_button);
        if(hot)
            DrawRectangleRec(tab, c_button_hover);
        DrawRectangleLinesEx(tab, 1.0f, c_button);
        DrawUIText(notebook.tabs[i], x + ScaleUIPx(14), ui_row_text_y(tab, font), font, c_text);
        if(hot)
            MarkUIClickable();
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && notebook.selected_index != NULL) {
            UIConsumeRelease();
            *notebook.selected_index = i;
            changed = 1;
        }
        x += w;
    }
    DrawRectangleLinesEx((Rectangle){notebook.bounds.x, notebook.bounds.y + tab_h,
                                     notebook.bounds.width, notebook.bounds.height - tab_h},
                         1.0f, c_button);
    return changed;
}

int
DrawUIPanedView(PanedViewProps panes)
{
    int changed = 0;
    int split = panes.split != NULL ? *panes.split : (panes.vertical ? (int)panes.bounds.width / 2 : (int)panes.bounds.height / 2);
    int grip = ScaleUIPx(8);
    Rectangle handle;

    if(panes.vertical)
        handle = (Rectangle){panes.bounds.x + split - grip / 2, panes.bounds.y, (float)grip, panes.bounds.height};
    else
        handle = (Rectangle){panes.bounds.x, panes.bounds.y + split - grip / 2, panes.bounds.width, (float)grip};
    DrawRectangleRec(handle, c_button);
    if(ui_hot(handle)) {
        MarkUIClickable();
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && panes.split != NULL) {
            Vector2 mouse = ui_mouse_world();
            split = panes.vertical ? (int)(mouse.x - panes.bounds.x) : (int)(mouse.y - panes.bounds.y);
            if(split < panes.min_first)
                split = panes.min_first;
            if(panes.vertical && split > (int)panes.bounds.width - panes.min_second)
                split = (int)panes.bounds.width - panes.min_second;
            if(!panes.vertical && split > (int)panes.bounds.height - panes.min_second)
                split = (int)panes.bounds.height - panes.min_second;
            *panes.split = split;
            changed = 1;
        }
    }
    return changed;
}

int
DrawUICollapsible(CollapsibleProps section)
{
    int font = GetUIFontSize();
    Rectangle header = section.bounds;
    header.height = ScaleUIPx(32);
    DrawRectangleRec(header, c_button);
    DrawRectangleLinesEx(header, 1.0f, c_button_hover);
    DrawUIText(section.open != NULL && *section.open ? "v" : ">",
               (int)header.x + ScaleUIPx(8), ui_row_text_y(header, font), font, c_icon);
    DrawUIText(section.label != NULL ? section.label : "",
               (int)header.x + ScaleUIPx(28), ui_row_text_y(header, font), font, c_text);
    if(ui_hot(header)) {
        MarkUIClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && section.open != NULL) {
            UIConsumeRelease();
            *section.open = !*section.open;
            return 1;
        }
    }
    return 0;
}

int
DrawUIMessageDialog(MessageDialogProps dialog)
{
    const ModalAction action = {dialog.ok_label != NULL ? dialog.ok_label : "OK",
                                  ButtonStylePrimary, 0};
    ModalProps props;

    memset(&props, 0, sizeof(props));
    props.title = dialog.title;
    props.message = dialog.message;
    props.actions = &action;
    props.action_count = 1;
    props.close_icon = g_ui_x_icon;
    props.max_width = ScaleUIPx(420);
    return DrawUIActionModal(props);
}

int
DrawUIConfirmDialog(ConfirmDialogProps dialog)
{
    ModalAction actions[2] = {
        {dialog.cancel_label != NULL ? dialog.cancel_label : "Cancel", ButtonStyleSecondary, 0},
        {dialog.confirm_label != NULL ? dialog.confirm_label : "OK", ButtonStylePrimary, 0}
    };
    ModalProps props;

    memset(&props, 0, sizeof(props));
    props.title = dialog.title;
    props.message = dialog.message;
    props.actions = actions;
    props.action_count = 2;
    props.close_icon = g_ui_x_icon;
    props.max_width = ScaleUIPx(460);
    return DrawUIActionModal(props);
}

int
DrawUIPromptDialog(PromptDialogProps dialog)
{
    int result;
    int commit_pressed = 0;
    ModalAction actions[2] = {
        {dialog.cancel_label != NULL ? dialog.cancel_label : "Cancel", ButtonStyleSecondary, 0},
        {dialog.confirm_label != NULL ? dialog.confirm_label : "OK", ButtonStylePrimary, 0}
    };
    ModalProps props;
    TextFieldProps field_props;

    memset(&props, 0, sizeof(props));
    props.title = dialog.title;
    props.message = "";
    props.actions = actions;
    props.action_count = 2;
    props.close_icon = g_ui_x_icon;
    props.max_width = ScaleUIPx(460);
    result = DrawUIActionModal(props);
    if(dialog.text != NULL && dialog.cursor_position != NULL && dialog.focused != NULL) {
        Rectangle field = {(float)(ui_view_width / 2 - ScaleUIPx(190)),
                           (float)(ui_view_height / 2 - ScaleUIPx(4)),
                           (float)ScaleUIPx(380), (float)ScaleUIPx(38)};
        memset(&field_props, 0, sizeof(field_props));
        field_props.bounds = field;
        field_props.text = dialog.text;
        field_props.text_size = (size_t)dialog.text_size;
        field_props.cursor_position = dialog.cursor_position;
        field_props.focused = dialog.focused;
        field_props.max_codepoints = dialog.text_size - 1;
        field_props.font = GetUIFontSize();
        field_props.focus_id = 7301;
        field_props.commit_pressed = &commit_pressed;
        RenderTextField(field_props);
        if(result == 0 && commit_pressed)
            result = 2;
        if(result == 0 && IsKeyPressed(KEY_ESCAPE))
            result = 1;
    }
    return result;
}

int
DrawUITextPopover(TextPopoverProps popover)
{
    int result = 0;
    int font = GetUIFontSize();
    int small_font = GetUISmallFontSize();
    int pad = ScaleUIPx(10);
    int gap = ScaleUIPx(8);
    int close_size = ScaleUIPx(22);
    int field_h = ScaleUIPx(34);
    int popover_w = popover.width > 0 ? ScaleUIPx(popover.width) : ScaleUIPx(300);
    int popover_h = pad * 2 + field_h;
    int screen_pad = ScaleUIPx(6);
    int label_w;
    int field_x;
    int field_w;
    int popover_x;
    int popover_y;
    int anchor_center;
    int commit_pressed = 0;
    int focused_before;
    Rectangle panel;
    Rectangle field_bounds;
    Rectangle close_bounds;
    Vector2 mouse = ui_mouse_world();
    TextFieldProps field;
    TextInputStyle field_style;
    Color surface = ui_modern_style() ? c_surface : ui_panel_color(14);
    Color border = c_link;
    Color shadow = Fade(GetThemeText(), 0.14f);

    if(popover.text == NULL || popover.text_size <= 0 ||
       popover.cursor_position == NULL || popover.focused == NULL)
        return 1;

    if(popover_w < ScaleUIPx(220))
        popover_w = ScaleUIPx(220);
    if(popover_w > ui_view_width - screen_pad * 2)
        popover_w = ui_view_width - screen_pad * 2;

    anchor_center = (int)(popover.anchor.x + popover.anchor.width / 2.0f);
    popover_x = anchor_center - popover_w / 2;
    popover_x = ui_clampi(popover_x, screen_pad,
                          ui_view_width - screen_pad - popover_w);
    popover_y = (int)(popover.anchor.y + popover.anchor.height) + ScaleUIPx(6);
    if(popover_y + popover_h > ui_view_height - screen_pad)
        popover_y = (int)popover.anchor.y - popover_h - ScaleUIPx(6);
    popover_y = ui_clampi(popover_y, screen_pad,
                          ui_view_height - screen_pad - popover_h);

    panel = (Rectangle){(float)popover_x, (float)popover_y,
                        (float)popover_w, (float)popover_h};
    SetUIModalCapture(panel);
    if((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !CheckCollisionPointRec(mouse, panel)) {
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            UIConsumeRelease();
        return 1;
    }

    DrawRectangleRec((Rectangle){panel.x + 2.0f, panel.y + 2.0f,
                                 panel.width, panel.height}, shadow);
    DrawRectangleRec(panel, surface);
    DrawRectangleLinesEx(panel, 1.0f, border);
    if(anchor_center >= popover_x + ScaleUIPx(8) &&
       anchor_center <= popover_x + popover_w - ScaleUIPx(8)) {
        Vector2 tip = {(float)anchor_center, panel.y - (float)ScaleUIPx(6)};
        Vector2 left = {(float)(anchor_center - ScaleUIPx(7)), panel.y};
        Vector2 right = {(float)(anchor_center + ScaleUIPx(7)), panel.y};

        DrawTriangle(tip, left, right, surface);
        DrawLine((int)tip.x, (int)tip.y, (int)left.x, (int)left.y, border);
        DrawLine((int)tip.x, (int)tip.y, (int)right.x, (int)right.y, border);
    }

    label_w = TextWidth(popover.title != NULL ? popover.title : "", small_font);
    if(label_w > 0) {
        DrawUIText(popover.title, popover_x + pad,
                   TextBaselineY(popover.title, popover_y + pad, field_h,
                                 small_font),
                   small_font, c_text);
        label_w += gap;
    }

    close_bounds = (Rectangle){
        (float)(popover_x + popover_w - pad - close_size),
        (float)(popover_y + pad + (field_h - close_size) / 2),
        (float)close_size,
        (float)close_size
    };
    field_x = popover_x + pad + label_w;
    field_w = (int)close_bounds.x - gap - field_x;
    if(field_w < ScaleUIPx(80))
        field_w = ScaleUIPx(80);
    field_bounds = (Rectangle){(float)field_x, (float)(popover_y + pad),
                               (float)field_w, (float)field_h};

    memset(&field_style, 0, sizeof(field_style));
    field_style.background = c_bg;
    field_style.border = c_button_hover;
    field_style.focus_border = c_link;
    field_style.text = c_text;
    field_style.cursor = c_link;
    field_style.radius = 0.03f;
    field_style.padding_x = ScaleUIPx(8);
    field_style.padding_y = ScaleUIPx(6);

    if(*popover.focused)
        SetUIFocus(popover.id > 0 ? popover.id : 8401);

    focused_before = *popover.focused;
    memset(&field, 0, sizeof(field));
    field.bounds = field_bounds;
    field.text = popover.text;
    field.text_size = (size_t)popover.text_size;
    field.cursor_position = popover.cursor_position;
    field.focused = popover.focused;
    field.max_codepoints = popover.max_codepoints > 0
                               ? popover.max_codepoints
                               : popover.text_size - 1;
    field.font = font;
    field.focus_id = popover.id > 0 ? popover.id : 8401;
    field.style = field_style;
    field.commit_pressed = &commit_pressed;
    RenderTextField(field);

    if(DrawUIPaddedIconBtn((int)close_bounds.x, (int)close_bounds.y,
                           ScaleUIPx(14), ScaleUIPx(4), g_ui_x_icon, NULL))
        result = 1;
    if(g_ui_x_icon.id == 0) {
        int hover = 0;
        if(UIHandleClick(close_bounds, 0, &hover))
            result = 1;
        DrawUIText("x",
                   (int)(close_bounds.x + (close_bounds.width -
                                           (float)TextWidth("x", small_font)) *
                                      0.5f),
                   TextBaselineY("x", (int)close_bounds.y,
                                 (int)close_bounds.height, small_font),
                   small_font, hover ? c_link : c_text);
    }

    if(result == 0 && commit_pressed)
        result = 2;
    if(result == 0 && focused_before && !*popover.focused)
        result = 1;
    return result;
}

int
DrawUIPickerDialog(PickerDialogProps picker)
{
    int pad = ScaleUIPx(14);
    int title_h = ScaleUIPx(34);
    int row_h = ScaleUIPx(40);
    int button_h = ScaleUIPx(36);
    int icon_size = row_h - ScaleUIPx(8);
    int max_w = picker.max_width > 0 ? ScaleUIPx(picker.max_width) : ScaleUIPx(300);
    int w;
    int panel_h;
    int x;
    int y;
    int i;
    Rectangle panel;
    Rectangle icon_src;
    Rectangle icon_dst;
    Color scrim = BLACK;
    ButtonSpec button;

    if(picker.labels == NULL || picker.option_count <= 0)
        return 0;

    w = max_w;
    if(w > ui_view_width - ScaleUIPx(32))
        w = ui_view_width - ScaleUIPx(32);
    panel_h = title_h + picker.option_count * row_h + button_h + pad * 2;
    x = (ui_view_width - w) / 2;
    y = (ui_view_height - panel_h) / 2;
    panel.x = (float)x;
    panel.y = (float)y;
    panel.width = (float)w;
    panel.height = (float)panel_h;

    scrim.a = 130;
    DrawRectangle(0, 0, ui_view_width, ui_view_height, scrim);
    SetUIModalCapture(panel);
    if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK) ||
       UIPointerReleaseOutside(panel)) {
        UIConsumeRelease();
        return -1;
    }

    DrawRectangleRounded(panel, 0.06f, 8, c_surface);
    DrawRectangleRoundedLines(panel, 0.06f, 8, DarkenUIColor(c_surface, 30));
    DrawUIText(picker.title != NULL ? picker.title : "",
               x + pad, y + pad, GetUIFontSize(), c_text);

    y += title_h;
    for(i = 0; i < picker.option_count; i++) {
        int has_icon = picker.icons != NULL && picker.icons[i].id != 0;
        int text_x = x + pad + ScaleUIPx(12);

        memset(&button, 0, sizeof(button));
        button.bounds.x = (float)(x + pad);
        button.bounds.y = (float)y;
        button.bounds.width = (float)(w - pad * 2);
        button.bounds.height = (float)row_h;
        button.label = "";
        button.font = GetUIFontSize();
        button.background = c_surface;
        button.hover_background = c_button_hover;
        button.text = c_text;
        button.border = c_button;
        button.radius = 0.08f;
        if(RenderButton(button)) {
            return i + 1;
        }
        if(has_icon) {
            icon_src.x = 0;
            icon_src.y = 0;
            icon_src.width = (float)picker.icons[i].width;
            icon_src.height = (float)picker.icons[i].height;
            icon_dst.x = (float)text_x;
            icon_dst.y = (float)(y + ScaleUIPx(4));
            icon_dst.width = (float)icon_size;
            icon_dst.height = (float)icon_size;
            DrawTexturePro(picker.icons[i], icon_src, icon_dst,
                           kryon_zero_vector2, 0.0f, WHITE);
            text_x += icon_size + ScaleUIPx(12);
        }
        DrawCenteredUIText(picker.labels[i] != NULL ? picker.labels[i] : "",
                           (text_x + x + w - pad) / 2, y + row_h / 2,
                           GetUIFontSize(), c_text);
        y += row_h;
    }

    y += pad;
    memset(&button, 0, sizeof(button));
    button.bounds.x = (float)(x + pad);
    button.bounds.y = (float)y;
    button.bounds.width = (float)(w - pad * 2);
    button.bounds.height = (float)button_h;
    button.label = picker.cancel_label != NULL ? picker.cancel_label : "Cancel";
    button.font = GetUIFontSize();
    button.background = c_surface;
    button.hover_background = c_button_hover;
    button.text = c_text;
    button.border = c_button;
    button.radius = 0.08f;
    if(RenderButton(button))
        return -1;
    return 0;
}

int
DrawUIColorPicker(Rectangle bounds, Color *color)
{
    int changed = 0;
    int r, g, b;
    if(color == NULL)
        return 0;
    r = color->r;
    g = color->g;
    b = color->b;
    changed |= ui_render_slider(8101, (int)bounds.x, (int)bounds.y,
                            (int)bounds.width, "R", 0, 255, &r, "", NULL);
    changed |= ui_render_slider(8102, (int)bounds.x, (int)bounds.y + ScaleUIPx(36),
                            (int)bounds.width, "G", 0, 255, &g, "", NULL);
    changed |= ui_render_slider(8103, (int)bounds.x, (int)bounds.y + ScaleUIPx(72),
                            (int)bounds.width, "B", 0, 255, &b, "", NULL);
    if(changed)
        *color = (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, color->a};
    DrawRectangle((int)bounds.x, (int)bounds.y + ScaleUIPx(112),
                  ScaleUIPx(80), ScaleUIPx(36), *color);
    DrawRectangleLines((int)bounds.x, (int)bounds.y + ScaleUIPx(112),
                       ScaleUIPx(80), ScaleUIPx(36), c_button);
    return changed;
}

int
AcceleratorPressed(Accelerator accelerator)
{
    if(accelerator.ctrl &&
       !(IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)))
        return 0;
    if(accelerator.shift &&
       !(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)))
        return 0;
    if(accelerator.alt &&
       !(IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))
        return 0;
    return IsKeyPressed(accelerator.key) ? accelerator.id : 0;
}

int
DispatchAccelerators(const Accelerator *accelerators, int count)
{
    int id;
    if(accelerators == NULL)
        return 0;
    for(int i = 0; i < count; i++) {
        id = AcceleratorPressed(accelerators[i]);
        if(id != 0)
            return id;
    }
    return 0;
}

void
DrawUIFocusDebugOverlay(const UIAccessibilityNode *nodes, int count)
{
    int font = GetUISmallFontSize();
    if(nodes == NULL)
        return;
    for(int i = 0; i < count; i++) {
        Color color = nodes[i].focused ? c_link : c_icon;
        DrawRectangleLinesEx(nodes[i].bounds, 1.0f, color);
        if(nodes[i].label != NULL)
            DrawUIText(nodes[i].label, (int)nodes[i].bounds.x,
                       (int)nodes[i].bounds.y - TextLineHeight(font), font, color);
    }
}
