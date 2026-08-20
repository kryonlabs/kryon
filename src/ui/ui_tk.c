#include "ui_internal.h"
#include "ui_tk.h"

#define UI_TK_MENU_MAX 8
#define UI_RADIO_ANIM_MAX 128
/* Large enough that the editor's source buffer (512 KiB) is the real ceiling,
 * not this. Matches the raylib SDL read buffer set via RAY_RAYLIB_CONFIG so
 * copy and paste caps stay symmetric. */
#define UI_TK_CLIPBOARD_MAX (1024 * 1024)

static int g_menu_open_id = 0;
static int g_menu_submenu_id = 0;
static Rectangle g_menu_panel_bounds = {0};
static int g_menu_panel_valid = 0;
typedef struct UIMenuOverlayState {
    int active;
    int bar_id;
    int menu_id;
    int x;
    int y;
    const UIMenuItem *items;
    int item_count;
} UIMenuOverlayState;

static UIMenuOverlayState g_menu_overlay = {0};
static int g_menu_pending_bar_id = 0;
static int g_menu_pending_activated = 0;
static int g_menu_pending_closed_bar_id = 0;
static int g_canvas_depth = 0;
static char g_clipboard_text[UI_TK_CLIPBOARD_MAX];
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
    return (int)bounds.y + ((int)bounds.height - GetUITextLineHeight(font)) / 2;
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
    if(ui_aero_style()) {
        ui_aero_paint_panel(bounds, c_surface,
                            ui_radius_px(bounds, GetUIStyleTokens().panel_radius),
                            1);
        return;
    }
    DrawRectangleRec(bounds, c_surface);
    DrawRectangleLinesEx(bounds, 1.0f, c_button);
}

static void
ui_draw_menu_panel(Rectangle bounds)
{
    Color surface = GetThemeSurface();
    Color border = GetThemeButton();
    Color shadow = Fade(GetThemeText(), 0.16f);

    if(ui_aero_style()) {
        ui_aero_paint_panel(bounds, surface,
                            ui_radius_px(bounds, GetUIStyleTokens().panel_radius),
                            2);
        return;
    }
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

UIFrame
BeginUIFrameBox(Rectangle bounds, int pad_x, int pad_y, int gap)
{
    UIFrame frame;
    frame.bounds = bounds;
    frame.pad_x = ScaleUIPx(pad_x);
    frame.pad_y = ScaleUIPx(pad_y);
    frame.gap = ScaleUIPx(gap);
    frame.cursor_x = (int)bounds.x + frame.pad_x;
    frame.cursor_y = (int)bounds.y + frame.pad_y;
    return frame;
}

Rectangle
UIFramePack(UIFrame *frame, UISide side, int size)
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

    if(side == UI_SIDE_TOP) {
        item.y = frame->cursor_y;
        item.height = scaled;
        frame->cursor_y += scaled + frame->gap;
    } else if(side == UI_SIDE_BOTTOM) {
        item.y = frame->bounds.y + frame->bounds.height - frame->pad_y - scaled;
        item.height = scaled;
        frame->bounds.height -= scaled + frame->gap;
    } else if(side == UI_SIDE_LEFT) {
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
UIGridCell(UIGrid grid, int row, int col, int row_span, int col_span)
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
UIPlace(Rectangle parent, int x, int y, int w, int h)
{
    return (Rectangle){parent.x + ScaleUIPx(x), parent.y + ScaleUIPx(y),
                       ScaleUIPx(w), ScaleUIPx(h)};
}

void
DrawUISeparator(Rectangle bounds, int vertical)
{
    if(vertical)
        DrawLine((int)(bounds.x + bounds.width / 2), (int)bounds.y,
                 (int)(bounds.x + bounds.width / 2), (int)(bounds.y + bounds.height), c_button);
    else
        DrawLine((int)bounds.x, (int)(bounds.y + bounds.height / 2),
                 (int)(bounds.x + bounds.width), (int)(bounds.y + bounds.height / 2), c_button);
}

static int
draw_menu_items(int id, int x, int y, const UIMenuItem *items, int item_count)
{
    int font = GetUIFontSize();
    int row_h = ScaleUIPx(30);
    int pad = ScaleUIPx(12);
    int accel_w = ScaleUIPx(88);
    int w = ScaleUIPx(180);
    int activated = 0;
    Rectangle panel;
    Vector2 mouse;

    (void)id;
    if(items == NULL || item_count <= 0)
        return 0;

    for(int i = 0; i < item_count; i++) {
        int text_w = items[i].label != NULL ? MeasureUIText(items[i].label, font) : 0;
        int accel = items[i].accelerator != NULL ? MeasureUIText(items[i].accelerator, font) + accel_w : 0;
        if(text_w + accel + pad * 2 > w)
            w = text_w + accel + pad * 2;
    }

    panel = (Rectangle){(float)x, (float)y, (float)w, (float)(row_h * item_count + ScaleUIPx(8))};
    ui_draw_menu_panel(panel);
    ui_menu_track_panel(panel);
    PushUIInputCapture(panel, 1);
    mouse = ui_mouse_world();
    if(ui_contains(panel, mouse))
        MarkUICursor(MOUSE_CURSOR_DEFAULT);

    for(int i = 0; i < item_count; i++) {
        Rectangle row = {(float)x + 4, (float)y + 4 + (float)(i * row_h),
                         (float)w - 8, (float)row_h};
        const UIMenuItem *item = &items[i];
        int row_hot = ui_contains(row, mouse) && item->kind != UI_MENU_SEPARATOR;
        int hot = row_hot && !item->disabled;

        if(item->kind == UI_MENU_SEPARATOR) {
            DrawUISeparator(row, 0);
            continue;
        }

        if(hot) {
            DrawRectangleRec(row, GetThemeButtonHover());
            MarkUIClickable();
        }
        if(item->disabled && row_hot)
            MarkUIDisabled();
        if(item->checked)
            DrawUIText("*", (int)row.x + ScaleUIPx(8), ui_row_text_y(row, font), font, GetThemeIcon());
        DrawUIText(item->label != NULL ? item->label : "", (int)row.x + ScaleUIPx(28),
                   ui_row_text_y(row, font),
                   font, item->disabled ? GetThemeButton() : GetThemeText());
        if(item->accelerator != NULL)
            DrawUIText(item->accelerator, (int)(row.x + row.width - accel_w),
                       ui_row_text_y(row, font),
                       font, item->disabled ? GetThemeButton() : GetThemeIcon());
        if(item->kind == UI_MENU_SUBMENU)
            DrawUIText(">", (int)(row.x + row.width - ScaleUIPx(18)),
                       ui_row_text_y(row, font),
                       font, item->disabled ? GetThemeButton() : GetThemeIcon());
        if(hot && item->kind == UI_MENU_SUBMENU)
            g_menu_submenu_id = item->id;
        if(item->kind == UI_MENU_SUBMENU && g_menu_submenu_id == item->id &&
           item->submenu != NULL && item->submenu_count > 0) {
            int sub = draw_menu_items(item->id, (int)(row.x + row.width), (int)row.y,
                                      item->submenu, item->submenu_count);
            if(sub != 0)
                activated = sub;
        }
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && item->kind != UI_MENU_SUBMENU) {
            UIConsumeRelease();
            activated = item->id;
            g_menu_open_id = 0;
        }
    }

    return activated;
}

UIMenuBarResult
DrawUIMenuBar(int id, Rectangle bounds, const UIMenu *menus, int menu_count, int *open_index)
{
    UIMenuBarResult result = {0, -1};
    int font = GetUIFontSize();
    int x = (int)bounds.x + ScaleUIPx(4);
    Vector2 mouse = ui_mouse_world();
    int skip_external_open = 0;
    int bar_capture_pushed = 0;

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
    DrawRectangleRec(bounds, c_surface);
    DrawRectangleLinesEx(bounds, 1.0f, c_button);

    for(int i = 0; i < menu_count; i++) {
        int w = MeasureUIText(menus[i].label != NULL ? menus[i].label : "", font) + ScaleUIPx(24);
        Rectangle item = {(float)x, bounds.y + ScaleUIPx(3), (float)w, bounds.height - ScaleUIPx(6)};
        int menu_id = id + 1 + i;
        int open = g_menu_open_id == menu_id;
        int hot = ui_hot(item);
        if(hot || open)
            DrawRectangleRec(item, open ? c_button : c_button_hover);
        if(hot)
            MarkUIClickable();
        DrawUIText(menus[i].label != NULL ? menus[i].label : "", x + ScaleUIPx(12),
                   ui_row_text_y(item, font), font, c_text);
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

    if(!g_menu_overlay.active || g_menu_open_id == 0)
        return;

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
    g_menu_overlay.active = 0;
}

int
DrawUIPopupMenu(int id, int x, int y, const UIMenuItem *items, int item_count)
{
    return draw_menu_items(id, x, y, items, item_count);
}

int
DrawUIContextMenu(UIContextMenu menu)
{
    Vector2 mouse = ui_mouse_world();
    int open_local = 0;
    int x_local = 0;
    int y_local = 0;
    int activated;

    if(menu.open == NULL)
        menu.open = &open_local;
    if(menu.x == NULL)
        menu.x = &x_local;
    if(menu.y == NULL)
        menu.y = &y_local;
    if(ui_contains(menu.trigger, mouse) &&
       !UIInputCapturesClick(mouse) &&
       IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        *menu.open = 1;
        *menu.x = (int)mouse.x;
        *menu.y = (int)mouse.y;
    }
    if(!*menu.open)
        return 0;

    activated = draw_menu_items(menu.id, *menu.x, *menu.y,
                                menu.items, menu.item_count);
    if(activated != 0) {
        *menu.open = 0;
        return activated;
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       (!g_menu_panel_valid || !ui_contains(g_menu_panel_bounds, mouse))) {
        UIConsumeRelease();
        *menu.open = 0;
    }
    return 0;
}

int
DrawUIRadioButton(RadioButtonProps radio)
{
    int font = GetUIFontSize();
    int diameter = ScaleUIPx(20);
    int touch = ScaleUIPx(40);
    Vector2 center = {radio.bounds.x + touch / 2.0f,
                      radio.bounds.y + radio.bounds.height / 2.0f};
    int hot = ui_hot(radio.bounds) && !radio.disabled;
    int down = hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

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
        float dot_radius;
        Color ring;
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
        if(radio.disabled) {
            ring = scheme.disabled_content;
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
        DrawRing(center, outer - stroke, outer, 0.0f, 360.0f, 48, ring);
        dot_radius = (float)ScaleUIPx(5) * selected;
        if(dot_radius > 0.2f)
            DrawCircleV(center, dot_radius, ring);
        DrawUIText(radio.label != NULL ? radio.label : "",
                   (int)radio.bounds.x + touch + ScaleUIPx(4),
                   ui_row_text_y(radio.bounds, font), font, label);
    } else if(ui_aero_style()) {
        UIAeroScheme scheme = ui_aero_scheme();
        Color ring = radio.checked ? scheme.glow : scheme.inset_border;

        center.x = radio.bounds.x + diameter / 2.0f;
        if(radio.disabled)
            ring = ColorLerp(ring, c_bg, 0.5f);
        if(hot && !radio.disabled) {
            Color halo = scheme.glow;

            halo.a = 60;
            DrawCircleV(center, (float)diameter / 2.0f + ScaleUIPx(3), halo);
        }
        DrawCircleLines((int)center.x, (int)center.y, (float)diameter / 2.0f, ring);
        if(radio.checked) {
            Color dot = radio.disabled ? ColorLerp(scheme.glow, c_bg, 0.5f)
                                       : scheme.glow;

            DrawCircleV(center, (float)ScaleUIPx(5), dot);
        }
        DrawUIText(radio.label != NULL ? radio.label : "", (int)radio.bounds.x + diameter + ScaleUIPx(8),
                   ui_row_text_y(radio.bounds, font), font, radio.disabled ? c_button : c_text);
    } else {
        center.x = radio.bounds.x + diameter / 2.0f;
        DrawCircleLines((int)center.x, (int)center.y, (float)diameter / 2.0f, radio.disabled ? c_button : c_icon);
        if(radio.checked)
            DrawCircleV(center, (float)ScaleUIPx(5), radio.disabled ? c_button : c_icon);
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
    if(progress.max <= progress.min)
        progress.max = progress.min + 1;
    t = (float)(progress.value - progress.min) / (float)(progress.max - progress.min);
    if(t < 0.0f)
        t = 0.0f;
    if(t > 1.0f)
        t = 1.0f;
    fill.width *= t;
    if(ui_aero_style()) {
        float radius = ui_radius_px(progress.bounds,
                                    GetUIStyleTokens().control_radius);

        ui_aero_paint_inset(progress.bounds, BLANK, radius);
        if(fill.width > ScaleUIPx(2))
            ui_aero_paint_track(fill, LightenUIColor(c_circle, 22),
                                DarkenUIColor(c_circle, 10), radius, 1);
    } else {
        DrawRectangleRec(progress.bounds, ui_panel_color(10));
        DrawRectangleRec(fill, c_icon);
        DrawRectangleLinesEx(progress.bounds, 1.0f, c_button);
    }
    if(progress.label != NULL)
        DrawCenteredUIText(progress.label, (int)(progress.bounds.x + progress.bounds.width / 2),
                           (int)(progress.bounds.y + progress.bounds.height / 2),
                           GetUISmallFontSize(), c_text);
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
    if(ui_aero_style())
        ui_aero_paint_inset(spinbox.bounds, c_surface,
                            ui_radius_px(spinbox.bounds,
                                         GetUIStyleTokens().control_radius));
    else {
        DrawRectangleRec(text, c_surface);
        DrawRectangleLinesEx(spinbox.bounds, 1.0f, c_button);
    }
    if(spinbox.value_text != NULL)
        snprintf(value_text, sizeof(value_text), "%s", spinbox.value_text);
    else
        snprintf(value_text, sizeof(value_text), "%d", spinbox.value != NULL ? *spinbox.value : 0);
    DrawCenteredUIText(value_text, (int)(text.x + text.width / 2), (int)(text.y + text.height / 2),
                       GetUIFontSize(), c_text);
    if(DrawUIButton((UIButtonSpec){left, "-", GetUIFontSize(), spinbox.id * 10 + 1, spinbox.disabled,
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
    if(DrawUIButton((UIButtonSpec){right, "+", GetUIFontSize(), spinbox.id * 10 + 2, spinbox.disabled,
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
    if(ui_aero_style()) {
        UIAeroScheme scheme = ui_aero_scheme();
        int pad = ScaleUIPx(8);

        ui_aero_paint_panel(frame.bounds, scheme.glass,
                            ui_radius_px(frame.bounds,
                                         GetUIStyleTokens().panel_radius),
                            1);
        if(frame.title != NULL) {
            int w = MeasureUIText(frame.title, font) + pad * 2;

            DrawRectangle((int)frame.bounds.x + pad,
                          (int)frame.bounds.y - ScaleUIPx(8),
                          w, ScaleUIPx(18), scheme.glass);
            DrawUIText(frame.title, (int)frame.bounds.x + pad * 2,
                       (int)frame.bounds.y - ScaleUIPx(9), font, c_text);
        }
        return;
    }
    DrawRectangleLinesEx(frame.bounds, 1.0f, c_button);
    if(frame.title != NULL) {
        int pad = ScaleUIPx(8);
        int w = MeasureUIText(frame.title, font) + pad * 2;
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
    ui_draw_panel(list.bounds);
    BeginUIClip((int)list.bounds.x, (int)list.bounds.y, (int)list.bounds.width, (int)list.bounds.height);
    for(int i = 0; i <= visible && first + i < list.item_count; i++) {
        int index = first + i;
        Rectangle row = {list.bounds.x, list.bounds.y + (float)(i * row_h - y_offset),
                         list.bounds.width, (float)row_h};
        int hot = ui_hot(row);
        if(index == selected)
            DrawRectangleRec(row, c_button);
        else if(hot)
            DrawRectangleRec(row, c_button_hover);
        if(hot)
            MarkUIClickable();
        DrawUIText(list.items != NULL && list.items[index] != NULL ? list.items[index] : "",
                   (int)row.x + ScaleUIPx(8), ui_row_text_y(row, font), font, c_text);
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && list.selected_index != NULL) {
            UIConsumeRelease();
            *list.selected_index = index;
            changed = 1;
        }
    }
    EndUIClip();
    if(list.scroll_offset != NULL && max_scroll > 0)
        DrawUIScrollbar((int)(list.bounds.x + list.bounds.width - ScaleUIPx(8)),
                        (int)list.bounds.y, (int)list.bounds.height,
                        list.item_count * row_h, list.scroll_offset, max_scroll);
    return changed;
}

int
DrawUITreeView(TreeViewProps tree)
{
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
    ui_draw_panel(tree.bounds);
    BeginUIClip((int)tree.bounds.x, (int)tree.bounds.y, (int)tree.bounds.width, (int)tree.bounds.height);
    for(int i = 0; i <= visible && first + i < tree.item_count; i++) {
        int index = first + i;
        const UITreeItem *item = &tree.items[index];
        Rectangle row = {tree.bounds.x, tree.bounds.y + (float)(i * row_h - y_offset),
                         tree.bounds.width, (float)row_h};
        int hot = ui_hot(row);
        int x = (int)row.x + ScaleUIPx(8 + item->depth * 18);
        if(tree.selected_id != NULL && *tree.selected_id == item->id)
            DrawRectangleRec(row, c_button);
        else if(hot)
            DrawRectangleRec(row, c_button_hover);
        if(item->expanded)
            DrawUIText("v", x, ui_row_text_y(row, font), font, c_icon);
        else
            DrawUIText(">", x, ui_row_text_y(row, font), font, c_icon);
        DrawUIText(item->label != NULL ? item->label : "", x + ScaleUIPx(18),
                   ui_row_text_y(row, font), font, c_text);
        if(hot)
            MarkUIClickable();
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && item->selectable && tree.selected_id != NULL) {
            UIConsumeRelease();
            *tree.selected_id = item->id;
            changed = 1;
        }
    }
    EndUIClip();
    if(tree.scroll_offset != NULL && max_scroll > 0)
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
    y = GetUITextY(value, (int)rect.y, (int)rect.height, font);
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
    return MeasureUIText(line, font);
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
                                        : GetUITextLineHeight(font) + ScaleUIPx(4);
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
    default_col_w = (int)table.bounds.width / table.column_count;
    max_scroll = ui_update_scroll((Rectangle){table.bounds.x, table.bounds.y + header_h,
                                              table.bounds.width, table.bounds.height - header_h},
                                  table.row_count * row_h, table.scroll_offset, row_h);
    scroll_y = table.scroll_offset != NULL ? *table.scroll_offset : 0;
    first = scroll_y / row_h;
    y_offset = scroll_y % row_h;
    visible = (int)(table.bounds.height - header_h) / row_h;
    ui_draw_panel(table.bounds);

    for(int c = 0; c < table.column_count; c++) {
        int x = (int)table.bounds.x;
        int col_w = table.column_widths != NULL ? table.column_widths[c] : default_col_w;
        for(int prev = 0; prev < c; prev++)
            x += table.column_widths != NULL ? table.column_widths[prev] : default_col_w;
        Rectangle head = {(float)x, table.bounds.y, (float)col_w, (float)header_h};
        DrawRectangleRec(head, DarkenUIColor(c_bg, 10));
        DrawRectangleLinesEx(head, 1.0f, DarkenUIColor(c_bg, 28));
        DrawUIText(table.columns != NULL && table.columns[c] != NULL ? table.columns[c] : "",
                   (int)head.x + ScaleUIPx(6), ui_row_text_y(head, font), font, c_text);
        if(ui_clicked(head) && table.sort_column != NULL) {
            *table.sort_column = c;
            changed = 1;
        }
    }

    BeginUIClip((int)table.bounds.x, (int)(table.bounds.y + header_h),
                (int)table.bounds.width, (int)(table.bounds.height - header_h));
    for(int i = 0; i <= visible && first + i < table.row_count; i++) {
        int r = first + i;
        Rectangle row = {table.bounds.x, table.bounds.y + header_h + (float)(i * row_h - y_offset),
                         table.bounds.width, (float)row_h};
        int hot = ui_hot(row);
        if(table.selected_row != NULL && *table.selected_row == r)
            DrawRectangleRec(row, DarkenUIColor(c_bg, 18));
        else if(hot)
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
            BeginUIClip(x, (int)row.y, col_w, (int)row.height);
            DrawUIText(text, x + ScaleUIPx(6), ui_row_text_y(row, font), font, c_text);
            EndUIClip();
        }
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && table.selected_row != NULL) {
            UIConsumeRelease();
            *table.selected_row = r;
            changed = 1;
        }
    }
    EndUIClip();
    if(table.scroll_offset != NULL && max_scroll > 0)
        DrawUIScrollbar((int)(table.bounds.x + table.bounds.width - ScaleUIPx(8)),
                        (int)(table.bounds.y + header_h),
                        (int)(table.bounds.height - header_h),
                        table.row_count * row_h, table.scroll_offset, max_scroll);
    return changed;
}

Vector2
UICanvasToScreen(UICanvas canvas, Vector2 point)
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
UICanvasRectToScreen(UICanvas canvas, Rectangle rect)
{
    float zoom = canvas.zoom != NULL && *canvas.zoom > 0.01f ? *canvas.zoom : 1.0f;
    Vector2 p = UICanvasToScreen(canvas, (Vector2){rect.x, rect.y});
    return (Rectangle){p.x, p.y, rect.width * zoom, rect.height * zoom};
}

UICanvasResult
BeginUICanvas(UICanvas canvas)
{
    UICanvasResult result = {0};
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
EndUICanvas(UICanvas canvas)
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
UICanvasHitTest(Vector2 point, Rectangle *items, int item_count)
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
        int w = MeasureUIText(notebook.tabs[i], font) + ScaleUIPx(28);
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
    const UIModalAction action = {dialog.ok_label != NULL ? dialog.ok_label : "OK",
                                  UI_BUTTON_STYLE_PRIMARY, 0};
    return DrawUIActionModal((ModalProps){dialog.title, dialog.message, &action, 1,
                                           g_ui_x_icon, ScaleUIPx(420)});
}

int
DrawUIConfirmDialog(ConfirmDialogProps dialog)
{
    UIModalAction actions[2] = {
        {dialog.cancel_label != NULL ? dialog.cancel_label : "Cancel", UI_BUTTON_STYLE_SECONDARY, 0},
        {dialog.confirm_label != NULL ? dialog.confirm_label : "OK", UI_BUTTON_STYLE_PRIMARY, 0}
    };
    return DrawUIActionModal((ModalProps){dialog.title, dialog.message, actions, 2,
                                           g_ui_x_icon, ScaleUIPx(460)});
}

int
DrawUIPromptDialog(PromptDialogProps dialog)
{
    int result;
    UIModalAction actions[2] = {
        {dialog.cancel_label != NULL ? dialog.cancel_label : "Cancel", UI_BUTTON_STYLE_SECONDARY, 0},
        {dialog.confirm_label != NULL ? dialog.confirm_label : "OK", UI_BUTTON_STYLE_PRIMARY, 0}
    };
    result = DrawUIActionModal((ModalProps){dialog.title, "", actions, 2,
                                             g_ui_x_icon, ScaleUIPx(460)});
    if(dialog.text != NULL && dialog.cursor_position != NULL && dialog.focused != NULL) {
        Rectangle field = {(float)(ui_view_width / 2 - ScaleUIPx(190)),
                           (float)(ui_view_height / 2 - ScaleUIPx(4)),
                           (float)ScaleUIPx(380), (float)ScaleUIPx(38)};
        DrawUITextField((TextFieldProps){
            .bounds = field,
            .text = dialog.text,
            .text_size = (size_t)dialog.text_size,
            .cursor_position = dialog.cursor_position,
            .focused = dialog.focused,
            .max_codepoints = dialog.text_size - 1,
            .font = GetUIFontSize(),
            .focus_id = 7301
        });
    }
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
    Color scrim = BLACK;

    if(picker.labels == NULL || picker.option_count <= 0)
        return 0;

    w = max_w;
    if(w > ui_view_width - ScaleUIPx(32))
        w = ui_view_width - ScaleUIPx(32);
    panel_h = title_h + picker.option_count * row_h + button_h + pad * 2;
    x = (ui_view_width - w) / 2;
    y = (ui_view_height - panel_h) / 2;
    panel = (Rectangle){(float)x, (float)y, (float)w, (float)panel_h};

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

        if(DrawUIButton((UIButtonSpec){
            .bounds = {(float)(x + pad), (float)y, (float)(w - pad * 2), (float)row_h},
            .label = "",
            .font = GetUIFontSize(),
            .background = c_surface,
            .hover_background = c_button_hover,
            .text = c_text,
            .border = c_button,
            .radius = 0.08f
        })) {
            return i + 1;
        }
        if(has_icon) {
            DrawTexturePro(picker.icons[i],
                           (Rectangle){0, 0, (float)picker.icons[i].width,
                                       (float)picker.icons[i].height},
                           (Rectangle){(float)text_x, (float)(y + ScaleUIPx(4)),
                                       (float)icon_size, (float)icon_size},
                           (Vector2){0}, 0.0f, WHITE);
            text_x += icon_size + ScaleUIPx(12);
        }
        DrawCenteredUIText(picker.labels[i] != NULL ? picker.labels[i] : "",
                           (text_x + x + w - pad) / 2, y + row_h / 2,
                           GetUIFontSize(), c_text);
        y += row_h;
    }

    y += pad;
    if(DrawUIButton((UIButtonSpec){
        .bounds = {(float)(x + pad), (float)y, (float)(w - pad * 2), (float)button_h},
        .label = picker.cancel_label != NULL ? picker.cancel_label : "Cancel",
        .font = GetUIFontSize(),
        .background = c_surface,
        .hover_background = c_button_hover,
        .text = c_text,
        .border = c_button,
        .radius = 0.08f
    }))
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
    changed |= DrawUISlider(8101, (int)bounds.x, (int)bounds.y,
                            (int)bounds.width, "R", 0, 255, &r, "", NULL);
    changed |= DrawUISlider(8102, (int)bounds.x, (int)bounds.y + ScaleUIPx(36),
                            (int)bounds.width, "G", 0, 255, &g, "", NULL);
    changed |= DrawUISlider(8103, (int)bounds.x, (int)bounds.y + ScaleUIPx(72),
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
UIAcceleratorPressed(UIAccelerator accelerator)
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
DispatchUIAccelerators(const UIAccelerator *accelerators, int count)
{
    int id;
    if(accelerators == NULL)
        return 0;
    for(int i = 0; i < count; i++) {
        id = UIAcceleratorPressed(accelerators[i]);
        if(id != 0)
            return id;
    }
    return 0;
}

int
SetUIClipboardTextValue(const char *text)
{
    if(text == NULL)
        text = "";
    snprintf(g_clipboard_text, sizeof(g_clipboard_text), "%s", text);
    SetClipboardText(g_clipboard_text);
    return 1;
}

const char *
GetUIClipboardTextValue(void)
{
    const char *text = GetClipboardText();
    if(text != NULL && text[0] != '\0')
        snprintf(g_clipboard_text, sizeof(g_clipboard_text), "%s", text);
    return g_clipboard_text;
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
                       (int)nodes[i].bounds.y - GetUITextLineHeight(font), font, color);
    }
}
