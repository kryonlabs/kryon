#include "ui_internal.h"
#include "ui_style_internal.h"
#include "ui_tk.h"
#include "ui_numeric_input_internal.h"
#include "ui_popup_input_internal.h"
#include "toolkit_store.h"
#include "runtime/canvas.h"
#include "runtime/instance.h"
#include "runtime/canvas_grid.h"
#include "runtime/checkbox.h"
#include "runtime/collapsible.h"
#include "runtime/color_picker.h"
#include "runtime/drag_drop.h"
#include "runtime/drag.h"
#include "runtime/input.h"
#include "runtime/fieldset.h"
#include "runtime/focus.h"
#include "runtime/list_box.h"
#include "runtime/menu.h"
#include "runtime/list_box_multi.h"
#include "runtime/paned_view.h"
#include "runtime/plot.h"
#include "runtime/progress.h"
#include "runtime/radio.h"
#include "runtime/selectable.h"
#include "runtime/separator.h"
#include "runtime/slider.h"
#include "runtime/spinbox.h"
#include "runtime/table_view.h"
#include "runtime/tree_view.h"
#include <limits.h>

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Vector2 kryon_zero_vector2;

static StyleFrame ui_tk_simple_style_frame_role(ButtonTone tone,
                                                ButtonState state,
                                                int disabled, int selected,
                                                int style_kind, int role);

static StyleFrame
ui_tk_checkbox_style_frame(ButtonTone tone, ButtonState state, int disabled,
                           int selected, int class_name, int role)
{
    ButtonProps props = {0};
    props.class_name = class_name;
    props.tone = tone;
    props.emphasis = tone == ButtonToneAccent
                       ? ButtonEmphasisFilled
                       : ButtonEmphasisOutline;
    props.size = ControlSizeMedium;
    props.disabled = disabled;
    props.selected = selected;
    return ui_control_style_frame_role_kind(props, state, 0, 0.0f, 0.0f, 0.0f,
                                            StyleKindCheckbox(), role);
}

static ButtonState
ui_tk_checkbox_button_state(int hovered, int down, int focused, int disabled)
{
    if(disabled)
        return ButtonStateDisabled;
    if(down)
        return ButtonStatePressed;
    if(focused)
        return ButtonStateFocus;
    if(hovered)
        return ButtonStateHover;
    return ButtonStateNormal;
}

static StyleFrame
ui_tk_radio_style_frame(ButtonTone tone, ButtonState state, int disabled,
                        int selected, int class_name, int role)
{
    ButtonProps props = {0};
    props.class_name = class_name;
    props.tone = tone;
    props.emphasis = tone == ButtonToneAccent
                       ? ButtonEmphasisFilled
                       : ButtonEmphasisOutline;
    props.size = ControlSizeMedium;
    props.disabled = disabled;
    props.selected = selected;
    return ui_control_style_frame_role_kind(props, state, 0, 0.0f, 0.0f, 0.0f,
                                            StyleKindRadio(), role);
}

static StyleFrame
ui_tk_simple_style_frame(ButtonTone tone, ButtonState state, int disabled,
                         int selected, int style_kind)
{
    return ui_tk_simple_style_frame_role(tone, state, disabled, selected,
                                         style_kind, StyleAny());
}

static StyleFrame
ui_tk_simple_style_frame_role(ButtonTone tone, ButtonState state, int disabled,
                              int selected, int style_kind, int role)
{
    ButtonProps props = {0};
    props.tone = tone;
    props.emphasis = tone == ButtonToneAccent
                       ? ButtonEmphasisFilled
                       : ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    props.pill = 1;
    props.disabled = disabled;
    props.selected = selected;
    return ui_control_style_frame_role_kind(props, state, 0, 0.0f, 0.0f,
                                            0.0f, style_kind, role);
}

static StyleFrame
ui_tk_simple_style_frame_class_role(ButtonTone tone, ButtonState state,
                                    int disabled, int selected,
                                    int class_name, int style_kind, int role)
{
    ButtonProps props = {0};
    props.class_name = class_name;
    props.tone = tone;
    props.emphasis = tone == ButtonToneAccent
                       ? ButtonEmphasisFilled
                       : ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    props.pill = 1;
    props.disabled = disabled;
    props.selected = selected;
    return ui_control_style_frame_role_kind(props, state, 0, 0.0f, 0.0f,
                                            0.0f, style_kind, role);
}

static void
ui_tk_draw_style_frame(Rectangle bounds, Rectangle surface_bounds,
                       StyleFrame frame, int hovered, int pressed,
                       int disabled, int focused)
{
    StyleFrame styled = ui_style_apply_effects_frame(frame);
    Style style = ui_unpack_style(styled.value);

    ui_draw_material(bounds, surface_bounds,
                     style.background, style.border, style.border,
                     style.radius, style.border_width,
                     hovered ? 1.0f : 0.0f, pressed ? 1.0f : 0.0f,
                     disabled, style.focus, focused ? 1.0f : 0.0f,
                     style.opacity, ui_style_apply_effects_fill(styled.fill),
                     style.material);
}

static const TextInputStyle kryon_zero_text_input_style;

static void
ui_tk_draw_slider_paint(SliderPaint paint, int hovered, int active,
                        int disabled)
{
    StyleFrame track_frame = ui_style_apply_effects_frame(paint.track);
    Style track_style = ui_unpack_style(track_frame.value);
    StyleFrame active_frame = ui_style_apply_effects_frame(paint.active_track);
    Style active_style = ui_unpack_style(active_frame.value);

    ui_draw_material(paint.track_bounds, (Rectangle){0},
                     track_style.background, track_style.border,
                     track_style.border, track_style.radius,
                     track_style.border_width,
                     hovered ? 1.0f : 0.0f, active ? 1.0f : 0.0f,
                     disabled, track_style.focus, 0.0f,
                     track_style.opacity,
                     ui_style_apply_effects_fill(track_frame.fill),
                     track_style.material);
    if(paint.active_bounds.width > 0.0f && paint.active_bounds.height > 0.0f) {
        ui_draw_material(paint.active_bounds, paint.track_bounds,
                         active_style.background, active_style.border,
                         active_style.border, active_style.radius,
                         active_style.border_width,
                         hovered ? 1.0f : 0.0f, active ? 1.0f : 0.0f,
                         disabled, active_style.focus, 0.0f,
                         active_style.opacity,
                         ui_style_apply_effects_fill(active_frame.fill),
                         active_style.material);
    }
    if((active || hovered) && paint.glow_radius > 0.0f)
        DrawCircle((int)paint.thumb_x, (int)paint.thumb_y,
                   paint.glow_radius, GetColor(paint.glow_color));
    DrawCircle((int)paint.thumb_x, (int)(paint.thumb_y + Scale(2)),
               paint.thumb_radius + (float)Scale(1),
               GetColor(paint.thumb_shadow_color));
    DrawCircle((int)paint.thumb_x, (int)paint.thumb_y,
               paint.thumb_radius, GetColor(paint.thumb_fill_color));
    DrawCircle((int)(paint.thumb_x - Scale(3)),
               (int)(paint.thumb_y - Scale(4)),
               paint.thumb_radius * 0.45f,
               GetColor(paint.thumb_highlight_color));
    DrawCircleLines((int)paint.thumb_x, (int)paint.thumb_y,
                    paint.thumb_radius, GetColor(paint.thumb_edge_color));
}


#define UI_TK_MENU_MAX 8
#define UI_TK_MENU_DEPTH_MAX 8
#define UI_TK_CONTEXT_MENU_MAX_ITEMS 64
#define UI_RADIO_ANIM_MAX 128
#define UI_INSTANCE_BUCKETS 512
#define UI_DRAG_DROP_DATA_MAX 1024
#define UI_NUMERIC_INPUT_BUCKETS 128
typedef struct UINumericClickState {
    int valid;
    int kind;
    int widget_id;
    int component;
    Vector2 position;
    double time;
} UINumericClickState;
typedef struct UIDragDropState {
    int active;
    int source_id;
    char type[32];
    unsigned char data[UI_DRAG_DROP_DATA_MAX];
    int data_size;
} UIDragDropState;
static int ui_slider_scalar(SliderScalarProps slider, int vertical);
typedef struct UIMenuOverlayState {
    int active;
    int bar_id;
    int menu_id;
    int x;
    int y;
    int item_count;
    MenuItem items[UI_TK_CONTEXT_MENU_MAX_ITEMS];
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

typedef struct UIMenuNavigation {
    int focus_id;
    int top;
    int depth;
    int path[UI_TK_MENU_DEPTH_MAX];
    int key_handled;
    unsigned long key_frame;
} UIMenuNavigation;

typedef struct UIRadioAnimState {
    unsigned int key;
    float selected;
    float press;
    unsigned long frame_seen;
} UIRadioAnimState;

typedef struct UITreeHeaderNav {
    int id;
    int depth;
} UITreeHeaderNav;

typedef struct InstanceEntry {
    uint64_t key;
    const char *type;
    size_t size;
    void *value;
    unsigned long frame_seen;
    struct InstanceEntry *next;
} InstanceEntry;

struct ToolkitStore {
    int drag_active;
    float drag_last_x;
    UIPopupInputOwner drag_owner;
    int slider_active;
    UIPopupInputOwner slider_owner;
    UINumericClickState numeric_click;
    UIDragDropState drag_drop;
    int canvas_depth;
    int canvas_mode_depth;
    UIRadioAnimState radio_anim[UI_RADIO_ANIM_MAX];
    InstanceEntry *instances[UI_INSTANCE_BUCKETS];
    unsigned long instance_frame;
    int last_table_id;
    int last_table_row;
    int last_table_column;
    double last_table_click_time;
    int resize_table_id;
    int resize_column;
    int resize_start_x;
    int resize_start_width;
    UIPopupInputOwner resize_owner;
    int *active_split;
    UIPopupInputOwner active_split_owner;
    UINumericInputState *numeric_inputs[UI_NUMERIC_INPUT_BUCKETS];
    int numeric_next_token;
    UITreeHeaderNav *tree_headers;
    UITreeHeaderNav *tree_previous;
    int tree_header_count;
    int tree_previous_count;
    int tree_header_capacity;
    int tree_previous_capacity;
    unsigned long tree_header_frame;
    unsigned long tree_key_frame;
    int open_id;
    int submenu_id;
    Rectangle panel_bounds;
    int panel_valid;
    UIMenuOverlayState overlay;
    UIContextMenuOverlayState context_overlay;
    int pending_bar_id;
    int pending_activated;
    int pending_closed_bar_id;
    int context_open_id;
    int context_pending_id;
    int context_pending_activated;
    int context_pending_closed_id;
    UIMenuNavigation navigation;
};

static ToolkitStore fallback_toolkit_store = {
    .last_table_row = -1,
    .last_table_column = -1,
    .resize_column = -1,
    .numeric_next_token = 0x60000000
};
static ToolkitStore *current_toolkit_store = &fallback_toolkit_store;

ToolkitStore *
toolkit_store_new(void)
{
    ToolkitStore *store = calloc(1, sizeof(*store));

    if(store == NULL)
        abort();
    store->last_table_row = -1;
    store->last_table_column = -1;
    store->resize_column = -1;
    store->numeric_next_token = 0x60000000;
    return store;
}

void
toolkit_store_free(ToolkitStore *store)
{
    if(store == NULL)
        return;
    if(store == current_toolkit_store || store == &fallback_toolkit_store)
        abort();
    for(int i = 0; i < UI_INSTANCE_BUCKETS; i++) {
        InstanceEntry *entry = store->instances[i];
        while(entry != NULL) {
            InstanceEntry *next = entry->next;
            free(entry->value);
            free(entry);
            entry = next;
        }
    }
    for(int i = 0; i < UI_NUMERIC_INPUT_BUCKETS; i++) {
        UINumericInputState *state = store->numeric_inputs[i];

        while(state != NULL) {
            UINumericInputState *next = state->next;

            free(state);
            state = next;
        }
    }
    free(store->tree_headers);
    free(store->tree_previous);
    free(store);
}

ToolkitStore *
toolkit_store_swap(ToolkitStore *store)
{
    ToolkitStore *previous = current_toolkit_store;

    current_toolkit_store = store != NULL ? store : &fallback_toolkit_store;
    return previous;
}

ToolkitStore *
toolkit_store_current(void)
{
    return current_toolkit_store;
}

void
toolkit_store_frame(ToolkitStore *store)
{
    if(store == NULL)
        return;
    unsigned long frame = ++store->instance_frame;
    for(int i = 0; i < UI_INSTANCE_BUCKETS; i++) {
        InstanceEntry **link = &store->instances[i];
        while(*link != NULL) {
            InstanceEntry *entry = *link;
            if(InstanceExpired((int64_t)(frame - entry->frame_seen))) {
                *link = entry->next;
                free(entry->value);
                free(entry);
            } else {
                link = &entry->next;
            }
        }
    }
}

void *
InstanceState(const char *type, uint64_t key, size_t size)
{
    ToolkitStore *store = current_toolkit_store;
    unsigned int bucket = key % UI_INSTANCE_BUCKETS;
    for(InstanceEntry *entry = store->instances[bucket]; entry != NULL;
        entry = entry->next) {
        if(entry->key == key && strcmp(entry->type, type) == 0) {
            if(entry->size != size)
                abort();
            entry->frame_seen = store->instance_frame;
            return entry->value;
        }
    }
    InstanceEntry *entry = calloc(1, sizeof(*entry));
    if(entry == NULL)
        abort();
    entry->value = calloc(1, size);
    if(entry->value == NULL)
        abort();
    entry->key = key;
    entry->type = type;
    entry->size = size;
    entry->frame_seen = store->instance_frame;
    entry->next = store->instances[bucket];
    store->instances[bucket] = entry;
    return entry->value;
}

static ToolkitStore *
toolkit_state(void)
{
    return current_toolkit_store;
}

static int
ui_contains(Rectangle bounds, Vector2 point)
{
    return CheckCollisionPointRec(point, bounds);
}

static int
ui_hot(Rectangle bounds)
{
    Vector2 mouse = ui_mouse_world();
    return ui_contains(bounds, mouse) && !InputCapturesClick(mouse);
}

/* Keep every button-like widget on one focus/activation contract even when
 * its paint is not a button. The caller remains responsible for drawing the
 * focused presentation that fits its shape. */
int
ui_focusable_pressed(Rectangle bounds, int id, int disabled, int *focused)
{
    Vector2 mouse = ui_mouse_world();
    int enabled = !disabled && !UIContentDisabled();
    int inside = ui_contains(bounds, mouse);
    int captured = InputCapturesClick(mouse);
    int hot = enabled && inside && !captured;
    int active = 0;

    *focused = 0;
    if(enabled && id > 0)
        *focused = RegisterFocus(id, bounds) &&
                   !ui_popup_input_focus_captures(id);
    if(hot)
        MarkClickable();
    else if(inside && !captured && !enabled)
        MarkDisabled();
    if(mouse_release_activates_rect(bounds, mouse, hot)) {
        ConsumeRelease();
        if(id > 0)
            SetFocus(id);
        *focused = id > 0;
        active = 1;
    }
    if(enabled && id > 0 && IsFocusActivatePressed(id))
        active = 1;
    return active;
}

static int
ui_menu_bar_owns_open_menu(int id, int menu_count)
{
    ToolkitStore *state = toolkit_state();

    return state->open_id >= id + 1 && state->open_id <= id + menu_count;
}

static int
ui_row_text_y(Rectangle bounds, int font)
{
    return (int)bounds.y + ((int)bounds.height - TextLineHeight(font)) / 2;
}

static StyleFrame
ui_canvas_frame(void)
{
    return ui_tk_simple_style_frame(ButtonToneNeutral, ButtonStateNormal, 0, 0,
                                    StyleKindCanvas());
}

static void
ui_draw_menu_panel(Rectangle bounds)
{
    StyleFrame frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, StyleKindMenu(), 2);
    Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
    ui_default_elevation(bounds, style.radius, 2);
    ui_tk_draw_style_frame(bounds, (Rectangle){0}, frame, 0, 0, 0, 0);
}

static void
ui_menu_track_panel(Rectangle bounds)
{
    ToolkitStore *state = toolkit_state();
    float x1;
    float y1;
    float x2;
    float y2;

    if(!state->panel_valid) {
        state->panel_bounds = bounds;
        state->panel_valid = 1;
        return;
    }

    x1 = state->panel_bounds.x < bounds.x ? state->panel_bounds.x : bounds.x;
    y1 = state->panel_bounds.y < bounds.y ? state->panel_bounds.y : bounds.y;
    x2 = state->panel_bounds.x + state->panel_bounds.width;
    if(bounds.x + bounds.width > x2)
        x2 = bounds.x + bounds.width;
    y2 = state->panel_bounds.y + state->panel_bounds.height;
    if(bounds.y + bounds.height > y2)
        y2 = bounds.y + bounds.height;
    state->panel_bounds = (Rectangle){x1, y1, x2 - x1, y2 - y1};
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
    if(!ui_contains(bounds, mouse) || InputCapturesClick(mouse))
        return max_scroll;

    wheel = GetMouseWheelMove();
    if(wheel != 0.0f) {
        int step = row_h > 0 ? row_h * 3 : Scale(90);
        *scroll_offset -= (int)(wheel * (float)step);
        if(*scroll_offset < 0)
            *scroll_offset = 0;
        if(*scroll_offset > max_scroll)
            *scroll_offset = max_scroll;
    }
    return max_scroll;
}

static void
ui_render_separator_line(Rectangle bounds, int vertical, int class_name)
{
    SeparatorLine paint;
    StyleFrame frame;
    if(!IsWindowReady())
        return;
    frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, class_name, StyleKindSeparator(), 7);
    paint = SeparatorLineFor(bounds, vertical != 0, frame);
    if(vertical)
        DrawLine((int)paint.line.x, (int)paint.line.y,
                 (int)paint.line.x,
                 (int)(paint.line.y + paint.line.height),
                 GetColor(paint.color));
    else
        DrawLine((int)paint.line.x, (int)paint.line.y,
                 (int)(paint.line.x + paint.line.width),
                 (int)paint.line.y, GetColor(paint.color));
}

void
RenderSeparator(SeparatorProps separator)
{
    const char *label = separator.label != NULL ? separator.label : "";
    StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        separator.disabled ? ButtonStateDisabled : ButtonStateNormal,
        separator.disabled, 0, separator.class_name, StyleKindSeparator(), 6);
    Style label_style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
    int font = separator.font > 0 ? separator.font :
        (label_style.font_size > 0.0f
            ? (int)(label_style.font_size + 0.5f)
            : GetSmallFontSize());
    int text_width = TextWidth(label, font);
    int text_y = ui_row_text_y(separator.bounds, font);
    StyleFrame line_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        separator.disabled ? ButtonStateDisabled : ButtonStateNormal,
        separator.disabled, 0, separator.class_name, StyleKindSeparator(), 7);
    SeparatorLabelPaint paint = SeparatorLabelPaintFor(
        separator.bounds, (float)text_width, label[0] != '\0', font,
        (float)Scale(1000) / 1000.0f, frame);
    paint.line_color = line_frame.value.background;

    if(label[0] == '\0') {
        ui_render_separator_line(separator.bounds, separator.vertical,
                                 separator.class_name);
        return;
    }
    if(!IsWindowReady())
        return;
    if(paint.show_text)
        RenderText(label, (int)paint.text.x, text_y, font,
                   Fade(GetColor(paint.text_color), label_style.opacity));
    if(paint.show_line)
        DrawLine((int)paint.line.x, (int)paint.line.y,
                 (int)(paint.line.x + paint.line.width),
                 (int)paint.line.y, GetColor(paint.line_color));
}

int
RenderDragDrop(DragDropProps drag_drop)
{
    ToolkitStore *toolkit = toolkit_state();
    Vector2 mouse = ui_mouse_world();
    int disabled = drag_drop.disabled || UIContentDisabled();
    int hot;
    int matches;
    int valid;

    if(drag_drop.role == DragDropRoleSource) {
        if(DragDropShouldClearSource(toolkit->drag_drop.active,
           toolkit->drag_drop.source_id, drag_drop.id,
           IsMouseButtonDown(MOUSE_BUTTON_LEFT),
           IsMouseButtonReleased(MOUSE_BUTTON_LEFT)))
            toolkit->drag_drop = (UIDragDropState){0};
        valid = DragDropSourceValid(drag_drop.disabled, UIContentDisabled(),
                                    drag_drop.type != NULL &&
                                    drag_drop.type[0] != '\0',
                                    drag_drop.data_size, UI_DRAG_DROP_DATA_MAX,
                                    drag_drop.data != NULL);
        if(!valid)
            return 0;
        hot = ui_hot(drag_drop.bounds);
        if(hot)
            MarkClickable();
        if(DragDropSourceStarts(valid, hot,
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            toolkit->drag_drop = (UIDragDropState){0};
            toolkit->drag_drop.active = 1;
            toolkit->drag_drop.source_id = drag_drop.id;
            snprintf(toolkit->drag_drop.type, sizeof(toolkit->drag_drop.type),
                     "%s", drag_drop.type);
            toolkit->drag_drop.data_size = drag_drop.data_size;
            if(drag_drop.data_size > 0)
                memcpy(toolkit->drag_drop.data, drag_drop.data,
                       (size_t)drag_drop.data_size);
        }
        return DragDropSourceReturnsActive(toolkit->drag_drop.active,
            toolkit->drag_drop.source_id, drag_drop.id,
            IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    }

    hot = CheckCollisionPointRec(mouse, drag_drop.bounds) &&
              !disabled && !InspectInputCapturesClick(mouse) &&
              !ui_input_captures_click_internal(mouse, 0);
    matches = DragDropTargetMatches(toolkit->drag_drop.active,
                  drag_drop.type != NULL && drag_drop.type[0] != '\0',
                  drag_drop.type != NULL &&
                  strcmp(toolkit->drag_drop.type, drag_drop.type) == 0);

    if(drag_drop.accepted_size != NULL)
        *drag_drop.accepted_size = 0;
    if(matches && IsWindowReady()) {
        StyleFrame frame = ui_tk_simple_style_frame(ButtonToneNeutral,
            disabled ? ButtonStateDisabled :
            (hot ? ButtonStateHover : ButtonStateNormal),
            disabled, hot, StyleKindDragDropTarget());
        ui_tk_draw_style_frame(drag_drop.bounds, (Rectangle){0}, frame, hot, 0,
                               disabled, 0);
    }
    if(!DragDropTargetAccepts(drag_drop.disabled, UIContentDisabled(), matches,
       hot, IsMouseButtonReleased(MOUSE_BUTTON_LEFT)))
        return 0;
    if(drag_drop.output != NULL && drag_drop.output_size > 0) {
        int copied = DragDropCopySize(toolkit->drag_drop.data_size,
                                      drag_drop.output_size);
        memcpy(drag_drop.output, toolkit->drag_drop.data, (size_t)copied);
        if(drag_drop.accepted_size != NULL)
            *drag_drop.accepted_size = copied;
    }
    toolkit->drag_drop = (UIDragDropState){0};
    ConsumeRelease();
    return 1;
}

static void
ui_list_box_multi_apply(ListBoxProps list, int index, int control,
                      int shift, int range_anchor)
{
    int anchor = list.anchor != NULL ? *list.anchor : -1;
    for(int row = 0; row < list.item_count; row++) {
        list.selected[row] = ListBoxMultiSelectionForRow(row,
            list.selected[row] != 0, index, list.item_count, anchor,
            control != 0, shift != 0, range_anchor) ? 1 : 0;
    }
    if(list.anchor != NULL)
        *list.anchor = ListBoxMultiAnchorAfterClick(anchor, index,
            list.item_count, control != 0, shift != 0, range_anchor);
}

int
RenderListBoxMulti(ListBoxProps list)
{
    Vector2 mouse = ui_mouse_world();
    int row_height = ListBoxMultiRowHeight(list.row_height,
        (float)Scale(1000) / 1000.0f);
    int paint = IsWindowReady();
    int clicked = -1;
    int disabled = list.disabled || UIContentDisabled();
    StyleFrame default_item_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        list.class_name, StyleKindListBoxMultiItem(), StyleAny());
    Style default_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_item_frame).value);
    int default_item_font = default_item_style.font_size > 0.0f
        ? (int)(default_item_style.font_size + 0.5f)
        : GetSmallFontSize();
    int control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    int focused;
    int range_anchor = -1;

    if(list.items == NULL || list.selected == NULL || list.item_count <= 0)
        return -1;
    focused = !disabled && list.id > 0 && RegisterFocus(list.id,list.bounds) &&
              !ui_popup_input_focus_captures(list.id);
    if(focused)
        SetFocusTextInputActive(0);
    if(focused) {
        int cursor = -1;
        int selected_first = -1;
        ListBoxMultiNavResult nav;
        if(list.anchor != NULL && *list.anchor >= 0 &&
           *list.anchor < list.item_count)
            cursor = *list.anchor;
        else
            for(int i = 0; i < list.item_count; i++)
                if(list.selected[i]) { selected_first = i; break; }
        cursor = ListBoxMultiFocusedRow(cursor, selected_first,
                                       list.item_count);
        nav = ListBoxMultiNavigate(list.item_count, cursor, control != 0,
            shift != 0, IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END),
            IsKeyPressed(KEY_UP), IsKeyPressed(KEY_DOWN),
            IsKeyPressed(KEY_SPACE),
            IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
        clicked = nav.clicked;
        control = nav.control ? 1 : 0;
        shift = nav.shift ? 1 : 0;
        range_anchor = nav.range_anchor;
        if(nav.anchor_changed && list.anchor != NULL) {
            *list.anchor = nav.anchor;
        }
    }
    if(clicked >= 0)
        ui_list_box_multi_apply(list,clicked,control,shift,range_anchor);
    if(paint) {
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
            list.class_name, StyleKindListBoxMulti(), StyleAny());
        ui_tk_draw_style_frame(list.bounds, (Rectangle){0}, frame, 0, 0,
                               disabled, focused);
        BeginClip((int)list.bounds.x, (int)list.bounds.y,
                  (int)list.bounds.width, (int)list.bounds.height);
    }
    for(int i = 0; i < list.item_count; i++) {
        Rectangle row = ListBoxMultiRowBounds(list.bounds, i, row_height);
        int hot = CheckCollisionPointRec(mouse, row) &&
                  !InputCapturesClick(mouse);
        int selected = list.selected[i] != 0;
        ButtonState item_state = disabled ? ButtonStateDisabled :
            (hot ? ButtonStateHover :
             (selected ? ButtonStateSelected : ButtonStateNormal));
        StyleFrame item_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            item_state, disabled, selected, list.class_name,
            StyleKindListBoxMultiItem(), StyleAny());
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        if(paint) {
            int font = item_style.font_size > 0.0f
                ? (int)(item_style.font_size + 0.5f)
                : default_item_font;
            int label_inset = item_style.padding_x > 0.0f
                ? (int)(item_style.padding_x + 0.5f)
                : Scale(8);
            if(selected || hot || disabled)
                ui_tk_draw_style_frame(row, list.bounds, item_frame, hot, 0,
                                       disabled, 0);
            RenderText(list.items[i] != NULL ? list.items[i] : "",
                       (int)row.x + label_inset,
                       ui_row_text_y(row, font),
                       font, Fade(item_style.foreground, item_style.opacity));
        }
        if(hot)
            disabled ? MarkDisabled() : MarkClickable();
        if(hot && !disabled &&
           IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            ConsumeRelease();
            if(list.id > 0) {
                SetFocus(list.id);
                focused = 1;
            }
            clicked = i;
            ui_list_box_multi_apply(list,clicked,control,shift,-1);
        }
    }
    if(paint)
        EndClip();
    if(paint && focused)
        RenderFocus(list.bounds);
    if(list.selected_count != NULL) {
        int count = 0;
        for(int i = 0; i < list.item_count; i++)
            count += list.selected[i] != 0;
        *list.selected_count = count;
    }
    return clicked;
}

void
RenderBullet(Rectangle bounds)
{
    BulletPaint paint;
    StyleFrame frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, StyleKindSeparator(), 8);
    if(!IsWindowReady())
        return;
    paint = BulletPaintFor(bounds, frame);
    DrawCircleV(paint.center, paint.radius, GetColor(paint.color));
}

int
RenderSelectable(SelectableProps selectable)
{
    Vector2 mouse = ui_mouse_world();
    int selected = selectable.selected != NULL && *selectable.selected;
    int disabled = selectable.disabled || UIContentDisabled();
    int focused = 0;
    int pressed = ui_focusable_pressed(selectable.bounds, selectable.id,
                                       selectable.disabled, &focused);
    int hot = !disabled && ui_contains(selectable.bounds, mouse) &&
              !InputCapturesClick(mouse);
    ButtonState state = ButtonStateNormal;
    if(disabled)
        state = ButtonStateDisabled;
    else if(pressed)
        state = ButtonStatePressed;
    else if(hot)
        state = ButtonStateHover;
    else if(selected)
        state = ButtonStateSelected;
    StyleFrame face = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        state, disabled, selected, selectable.class_name,
        StyleKindSelectable(), StyleAny());
    float label_inset = face.value.padding_x > 0.0f
        ? face.value.padding_x
        : (float)Scale(8);
    int font = face.value.font_size > 0.0f
        ? (int)(face.value.font_size + 0.5f)
        : GetFontSize();
    SelectablePaint paint = SelectablePaintFor((SelectableSpec){
        .bounds = selectable.bounds,
        .selected = selected,
        .hovered = hot,
        .pressed = pressed,
        .disabled = disabled,
        .face = face,
        .label_inset = label_inset
    });

    if(IsWindowReady() && paint.draw_fill)
        DrawRectangleRec(paint.bounds, GetColor(Opacity(paint.fill_color,
                                                        face.value.opacity)));
    if(IsWindowReady())
        RenderText(selectable.label != NULL ? selectable.label : "",
                   (int)paint.label_x,
                   ui_row_text_y(selectable.bounds, font),
                   font, GetColor(Opacity(paint.text_color,
                                          face.value.opacity)));
    if(focused && IsWindowReady())
        RenderFocus(selectable.bounds);
    if(pressed) {
        if(selectable.selected != NULL)
            *selectable.selected = !*selectable.selected;
        return 1;
    }
    return 0;
}

int
RenderCheckbox(CheckboxProps checkbox)
{
    int disabled = checkbox.disabled || UIContentDisabled() ||
        (checkbox.value == NULL && checkbox.flags == NULL);
    int focused = 0;
    int pressed = ui_focusable_pressed(checkbox.bounds, checkbox.id,
                                       disabled, &focused);
    int changed = 0;
    int checked = 0;

    if(checkbox.flags != NULL) {
        CheckboxFlagResult state = CheckboxFlagApply(
            (uint32_t)*checkbox.flags, (uint32_t)checkbox.flags_value,
            pressed);
        checked = state.checked;
        changed = state.changed;
        if(state.changed)
            *checkbox.flags = (int)state.flags;
    } else if(checkbox.value != NULL) {
        checked = *checkbox.value != 0;
        if(pressed) {
            checked = !checked;
            *checkbox.value = checked;
            changed = 1;
        }
    }

    if(IsWindowReady()) {
        float runtime_scale = (float)Scale(1000) / 1000.0f;
        int hovered = !disabled && ui_contains(checkbox.bounds, ui_mouse_world()) &&
                      !InputCapturesClick(ui_mouse_world()) &&
                      HoverEffectsEnabled();
        int down = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        ButtonState state = ui_tk_checkbox_button_state(hovered, down,
                                                        focused, disabled);
        CheckboxPaint paint;

        paint = CheckboxPaintFor((CheckboxSpec){
            .bounds = checkbox.bounds,
            .checked = checked,
            .enabled = !disabled,
            .hovered = hovered,
            .pressed = down,
            .focused = focused,
            .scale = runtime_scale,
            .box = ui_tk_checkbox_style_frame(ButtonToneNeutral, state,
                                              disabled, checked,
                                              checkbox.class_name, 9),
            .active = ui_tk_checkbox_style_frame(ButtonToneAccent, state,
                                                 disabled, checked,
                                                 checkbox.class_name, 10)
        });
        Style label_style = ui_unpack_style(ui_style_apply_effects_frame(
            ui_tk_checkbox_style_frame(ButtonToneNeutral, state, disabled,
                                       checked, checkbox.class_name, 6)).value);
        int label_font = label_style.font_size > 0.0f
            ? (int)(label_style.font_size + 0.5f)
            : GetFontSize();
        paint.label_color = ColorToInt(label_style.foreground);

        if(paint.show_state)
            DrawRectangleRounded(paint.state_bounds, paint.state_radius, 8,
                                 GetColor(paint.state_color));
        if(paint.show_focus)
            DrawRectangleRoundedLinesEx(paint.focus_bounds, paint.focus_radius, 8,
                                        paint.border_width,
                                        GetColor(paint.focus_color));
        if(paint.show_fill)
            DrawRectangleRounded(paint.box_bounds, paint.radius, 8,
                                 GetColor(paint.fill_color));
        DrawRectangleRoundedLinesEx(paint.box_bounds, paint.radius, 8,
                                    paint.border_width,
                                    GetColor(paint.border_color));
        if(paint.show_mark) {
            DrawLineEx(paint.check_start, paint.check_middle, paint.mark_width,
                       GetColor(paint.mark_color));
            DrawLineEx(paint.check_middle, paint.check_end, paint.mark_width,
                       GetColor(paint.mark_color));
        }
        RenderText(checkbox.label != NULL ? checkbox.label : "",
                   (int)paint.slot_bounds.x + CheckboxSlotSize(runtime_scale) + Scale(10),
                   ui_row_text_y(checkbox.bounds, label_font),
                   label_font, Fade(GetColor(paint.label_color),
                                    label_style.opacity));
    }
    return changed;
}

static int
ui_color_edit(ColorPickerProps edit, int channels)
{
    if(edit.values == NULL || edit.value_count < channels)
        return 0;
    return ui_slider_scalar((SliderScalarProps){edit.bounds, edit.id, edit.label,
                                               edit.values, channels, 0.0f, 1.0f,
                                               "%.3f", edit.disabled, 0}, 0);
}

static Color
ui_float_color(const float *values, int channels)
{
    float component[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for(int i = 0; i < channels; i++) {
        component[i] = values[i];
    }
    return ColorPickerColorFor(component[0], component[1], component[2],
                               component[3], channels);
}

static int
ui_color_picker_float(ColorPickerProps picker, int channels)
{
    int changed = 0;
    float scale = (float)Scale(1000) / 1000.0f;
    ColorPickerLayout layout;
    const char *labels[4] = {"R", "G", "B", "A"};

    if(picker.values == NULL || picker.value_count < channels)
        return 0;
    layout = ColorPickerLayoutFor(picker.bounds, channels, scale);
    for(int i = 0; i < channels; i++) {
        Rectangle row = ColorPickerChannelBounds(picker.bounds, i, channels,
                                                 scale);
        SliderScalarProps channel = {
            row,
            picker.id * 8 + i + 1, labels[i], &picker.values[i], 1,
            0.0f, 1.0f, "%.3f", picker.disabled
        };
        changed |= ui_slider_scalar(channel, 0);
    }
    if(IsWindowReady()) {
        Rectangle swatch = layout.swatch_bounds;
        StyleFrame frame = ui_tk_simple_style_frame(ButtonToneNeutral,
            picker.disabled ? ButtonStateDisabled : ButtonStateNormal,
            picker.disabled, 0, StyleKindColorPickerSwatch());
        Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
        int font = style.font_size > 0.0f
            ? (int)(style.font_size + 0.5f)
            : GetSmallFontSize();
        int label_inset = style.padding_x > 0.0f
            ? (int)(style.padding_x + 0.5f)
            : Scale(6);
        DrawRectangleRec(swatch, ui_float_color(picker.values, channels));
        ui_tk_draw_style_frame(swatch, picker.bounds, frame, 0, 0,
                               picker.disabled, 0);
        if(picker.label != NULL)
            RenderText(picker.label, (int)swatch.x + label_inset,
                       ui_row_text_y(swatch, font),
                       font, Fade(style.foreground, style.opacity));
    }
    return changed;
}

int
RenderColorPicker(ColorPickerProps picker)
{
    int channels = picker.value_count >= 4 ? 4 : 3;

    if(picker.picker)
        return ui_color_picker_float(picker, channels);
    return ui_color_edit(picker, channels);
}

static int
menu_item_at(const MenuItem *items, int item_count, int start, int direction)
{
    int index = start;
    if(items == NULL || item_count <= 0) return -1;
    for(int i = 0; i < item_count; i++) {
        index = (index + direction + item_count) % item_count;
        if(items[index].kind != MenuSeparator && !items[index].disabled)
            return index;
    }
    return -1;
}

static int menu_first_item(const MenuItem *items, int count)
{ return menu_item_at(items,count,-1,1); }
static int menu_last_item(const MenuItem *items, int count)
{ return menu_item_at(items,count,0,-1); }

static void
menu_navigation_begin_frame(void)
{
    ToolkitStore *state = toolkit_state();

    if(state->navigation.key_frame == g_ui_frame_serial)
        return;
    state->navigation.key_frame = g_ui_frame_serial;
    state->navigation.key_handled = 0;
}

static void
menu_navigation_reset(int focus_id, const MenuItem *items, int item_count)
{
    ToolkitStore *state = toolkit_state();

    state->navigation.focus_id = focus_id;
    state->navigation.top = 0;
    state->navigation.depth = 0;
    for(int i = 0; i < UI_TK_MENU_DEPTH_MAX; i++)
        state->navigation.path[i] = -1;
    state->navigation.path[0] = menu_first_item(items,item_count);
}

static int
draw_menu_items(int x, int y, const MenuItem *items, int item_count,
                int focus_id, int depth)
{
    ToolkitStore *state = toolkit_state();
    StyleFrame base_item_frame = ui_tk_simple_style_frame(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, StyleKindMenuItem());
    Style base_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(base_item_frame).value);
    int font = base_item_style.font_size > 0.0f
        ? (int)(base_item_style.font_size + 0.5f)
        : GetFontSize();
    MenuMetrics metrics = MenuMetricsFor((float)Scale(1000) / 1000.0f);
    int w = metrics.panel_min_width;
    int activated = 0;
    Rectangle panel;
    Vector2 mouse;
    int can_draw = IsWindowReady();
    int keyboard;

    if(items == NULL || item_count <= 0)
        return 0;

    keyboard = !UIContentDisabled() && focus_id > 0 && IsFocusActive(focus_id) &&
               IsKeyboardInputEnabled() &&
               !ui_popup_input_focus_captures(focus_id);
    if(keyboard && state->navigation.focus_id != focus_id)
        menu_navigation_reset(focus_id,items,item_count);
    if(keyboard && depth < UI_TK_MENU_DEPTH_MAX) {
        int selected;
        if(state->navigation.path[depth] < 0 ||
           state->navigation.path[depth] >= item_count ||
           items[state->navigation.path[depth]].kind == MenuSeparator ||
           items[state->navigation.path[depth]].disabled)
            state->navigation.path[depth] = menu_first_item(items,item_count);
        selected = state->navigation.path[depth];
        if(!state->navigation.key_handled &&
           state->navigation.depth == depth && selected >= 0) {
            if(IsKeyPressed(KEY_UP)) {
                state->navigation.path[depth] =
                    menu_item_at(items,item_count,selected,-1);
                state->navigation.key_handled = 1;
            } else if(IsKeyPressed(KEY_DOWN)) {
                state->navigation.path[depth] =
                    menu_item_at(items,item_count,selected,1);
                state->navigation.key_handled = 1;
            } else if(IsKeyPressed(KEY_HOME)) {
                state->navigation.path[depth] = menu_first_item(items,item_count);
                state->navigation.key_handled = 1;
            } else if(IsKeyPressed(KEY_END)) {
                state->navigation.path[depth] = menu_last_item(items,item_count);
                state->navigation.key_handled = 1;
            } else if(IsKeyPressed(KEY_LEFT) && depth > 0) {
                state->navigation.depth = depth-1;
                state->navigation.path[depth] = -1;
                state->navigation.key_handled = 1;
            } else if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER) ||
                      IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE)) {
                const MenuItem *selected_item =
                    &items[state->navigation.path[depth]];
                state->navigation.key_handled = 1;
                if(selected_item->kind == MenuSubmenu &&
                   selected_item->submenu != NULL &&
                   selected_item->submenu_count > 0 &&
                   depth+1 < UI_TK_MENU_DEPTH_MAX) {
                    state->submenu_id = selected_item->id;
                    state->navigation.depth = depth+1;
                    state->navigation.path[depth+1] =
                        menu_first_item(selected_item->submenu,
                                        selected_item->submenu_count);
                } else if(selected_item->kind != MenuSeparator &&
                          !selected_item->disabled) {
                    state->open_id = 0;
                    return selected_item->id;
                }
            }
        }
    }

    for(int i = 0; i < item_count; i++) {
        int text_w = items[i].label != NULL ? TextWidth(items[i].label, font) : 0;
        int accel = items[i].accelerator != NULL ? TextWidth(items[i].accelerator, font) : 0;
        w = MenuPanelWidthStep(w, text_w, accel,
                               items[i].accelerator != NULL, metrics);
    }

    panel = MenuPanelBounds(x, y, w, item_count, metrics);
    if(can_draw)
        ui_draw_menu_panel(panel);
    ui_menu_track_panel(panel);
    PushInputCapture(panel, 1);
    mouse = ui_mouse_world();
    if(ui_contains(panel, mouse))
        MarkCursor(MOUSE_CURSOR_DEFAULT);

    for(int i = 0; i < item_count; i++) {
        Rectangle row = MenuRowBounds(panel, i, metrics);
        const MenuItem *item = &items[i];
        int row_hot = !UIContentDisabled() && ui_contains(row, mouse) &&
                      item->kind != MenuSeparator;
        int hot = row_hot && !item->disabled;
        int selected = keyboard && depth < UI_TK_MENU_DEPTH_MAX &&
                       state->navigation.path[depth] == i;
        ButtonState item_state = item->disabled ? ButtonStateDisabled :
            ((hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ?
                 ButtonStatePressed :
             (hot ? ButtonStateHover :
              (selected ? ButtonStateSelected : ButtonStateNormal)));
        StyleFrame item_frame = ui_tk_simple_style_frame(ButtonToneNeutral,
            item_state, item->disabled, selected, StyleKindMenuItem());
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        int item_font = item_style.font_size > 0.0f
            ? (int)(item_style.font_size + 0.5f)
            : font;
        Color item_text = Fade(item_style.foreground, item_style.opacity);

        if(item->kind == MenuSeparator) {
            if(can_draw)
                RenderSeparator((SeparatorProps){.bounds = row});
            continue;
        }

        if(hot) {
            if(state->navigation.focus_id == focus_id &&
               depth < UI_TK_MENU_DEPTH_MAX) {
                state->navigation.path[depth] = i;
                state->navigation.depth = depth;
                for(int child = depth+1; child < UI_TK_MENU_DEPTH_MAX; child++)
                state->navigation.path[child] = -1;
            }
            if(can_draw)
                ui_tk_draw_style_frame(row, panel, item_frame, 1,
                                       IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                                       item->disabled, 0);
            MarkClickable();
        }
        if(selected && !hot && can_draw)
            ui_tk_draw_style_frame(row, panel, item_frame, 0, 0,
                                   item->disabled, 0);
        if(item->disabled && can_draw)
            ui_tk_draw_style_frame(row, panel, item_frame, 0, 0, 1, 0);
        if(item->disabled && row_hot)
            MarkDisabled();
        if(can_draw && item->checked)
            RenderText("*", (int)row.x + Scale(8),
                       ui_row_text_y(row, item_font),
                       item_font, item_text);
        if(can_draw)
            RenderText(item->label != NULL ? item->label : "",
                       (int)row.x + Scale(28),
                       ui_row_text_y(row, item_font),
                       item_font, item_text);
        if(can_draw && item->accelerator != NULL) {
            int accel_text_w = TextWidth(item->accelerator, item_font);
            RenderText(item->accelerator,
                       (int)(row.x + row.width - accel_text_w -
                             metrics.panel_padding),
                       ui_row_text_y(row, item_font),
                       item_font, item_text);
        }
        if(can_draw && item->kind == MenuSubmenu)
            RenderText(">", (int)(row.x + row.width - Scale(18)),
                       ui_row_text_y(row, item_font),
                       item_font, item_text);
        if(hot && item->kind == MenuSubmenu)
            state->submenu_id = item->id;
        if(item->kind == MenuSubmenu &&
           (keyboard
                ? selected && state->navigation.depth > depth
                : state->submenu_id == item->id) &&
           item->submenu != NULL && item->submenu_count > 0) {
            int sub = draw_menu_items((int)(row.x + row.width), (int)row.y,
                                      item->submenu,
                                      item->submenu_count, focus_id, depth+1);
            if(sub != 0)
                activated = sub;
        }
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            ConsumeRelease();
            if(state->navigation.focus_id != focus_id)
                menu_navigation_reset(focus_id,items,item_count);
            if(depth < UI_TK_MENU_DEPTH_MAX) {
                state->navigation.path[depth] = i;
                state->navigation.depth = depth;
            }
            SetFocus(focus_id);
            if(item->kind == MenuSubmenu)
                state->submenu_id = item->id;
            else {
                activated = item->id;
                state->open_id = 0;
            }
        }
    }

    return activated;
}

static Rectangle
menu_items_panel_bounds(int x, int y, const MenuItem *items, int item_count)
{
    StyleFrame frame = ui_tk_simple_style_frame(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, StyleKindMenuItem());
    Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
    int font = style.font_size > 0.0f
        ? (int)(style.font_size + 0.5f)
        : GetFontSize();
    MenuMetrics metrics = MenuMetricsFor((float)Scale(1000) / 1000.0f);
    int w = metrics.panel_min_width;

    if(items == NULL || item_count <= 0)
        return (Rectangle){(float)x, (float)y, 0.0f, 0.0f};

    for(int i = 0; i < item_count; i++) {
        int text_w = items[i].label != NULL ? TextWidth(items[i].label, font) : 0;
        int accel = items[i].accelerator != NULL ? TextWidth(items[i].accelerator, font) : 0;
        w = MenuPanelWidthStep(w, text_w, accel,
                               items[i].accelerator != NULL, metrics);
    }
    return MenuPanelBounds(x, y, w, item_count, metrics);
}

static int
copy_menu_items(MenuItem *arena, int *used, const MenuItem *items,
                int item_count, int *copied_count)
{
    int start;
    int count;

    *copied_count = 0;
    if(items == NULL || item_count <= 0 ||
       *used >= UI_TK_CONTEXT_MENU_MAX_ITEMS)
        return -1;
    count = item_count;
    if(count > UI_TK_CONTEXT_MENU_MAX_ITEMS-*used)
        count = UI_TK_CONTEXT_MENU_MAX_ITEMS-*used;
    start = *used;
    *used += count;
    *copied_count = count;
    for(int i = 0; i < count; i++)
        arena[start+i] = items[i];
    for(int i = 0; i < count; i++) {
        MenuItem *copy = &arena[start+i];
        int child_count = 0;
        int child_start;
        if(copy->kind != MenuSubmenu || copy->submenu == NULL ||
           copy->submenu_count <= 0) {
            copy->submenu = NULL;
            copy->submenu_count = 0;
            continue;
        }
        child_start = copy_menu_items(arena,used,copy->submenu,
                                      copy->submenu_count,&child_count);
        copy->submenu = child_start >= 0 ? &arena[child_start] : NULL;
        copy->submenu_count = child_count;
    }
    return start;
}

static void
queue_context_menu_overlay(MenuProps menu, int suppress_close)
{
    ToolkitStore *state = toolkit_state();
    int count = 0;
    int used = 0;

    if(menu.items == NULL || menu.item_count <= 0) {
        state->context_overlay.active = 0;
        return;
    }
    copy_menu_items(state->context_overlay.items,&used,menu.items,
                    menu.item_count,&count);

    state->context_overlay.active = 1;
    state->context_overlay.id = menu.id;
    state->context_overlay.x = menu.x != NULL ? *menu.x : 0;
    state->context_overlay.y = menu.y != NULL ? *menu.y : 0;
    state->context_overlay.item_count = count;
    state->context_overlay.suppress_close = suppress_close;
}

MenuResult
RenderMenuGroups(int id, Rectangle bounds, const MenuGroup *menus, int menu_count, int *open_index)
{
    ToolkitStore *state = toolkit_state();
    MenuResult result = {0, -1};
    StyleFrame base_item_frame = ui_tk_simple_style_frame(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, StyleKindMenuItem());
    Style base_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(base_item_frame).value);
    int font = base_item_style.font_size > 0.0f
        ? (int)(base_item_style.font_size + 0.5f)
        : GetFontSize();
    MenuMetrics metrics = MenuMetricsFor((float)Scale(1000) / 1000.0f);
    int x = (int)bounds.x + Scale(4);
    Vector2 mouse = ui_mouse_world();
    int skip_external_open = 0;
    int bar_capture_pushed = 0;
    int can_draw = IsWindowReady();
    int focused;

    if(state->pending_bar_id == id) {
        result.activated_id = state->pending_activated;
        state->pending_bar_id = 0;
        state->pending_activated = 0;
    }
    if(state->pending_closed_bar_id == id) {
        state->pending_closed_bar_id = 0;
        state->open_id = 0;
        state->submenu_id = 0;
        skip_external_open = 1;
        if(open_index != NULL)
            *open_index = -1;
    }
    if(menu_count > UI_TK_MENU_MAX)
        menu_count = UI_TK_MENU_MAX;
    focused = !UIContentDisabled() && id > 0 && RegisterFocus(id,bounds) &&
              !ui_popup_input_focus_captures(id);
    if(state->navigation.focus_id != id)
        menu_navigation_reset(id,NULL,0);
    if(state->navigation.top < 0 || state->navigation.top >= menu_count)
        state->navigation.top = 0;
    menu_navigation_begin_frame();
    if(!skip_external_open && open_index != NULL && *open_index >= 0)
        state->open_id = id + 1 + *open_index;
    if(focused && menu_count > 0) {
        int current = ui_menu_bar_owns_open_menu(id,menu_count)
            ? state->open_id-id-1 : -1;
        if(current < 0) {
            if(IsKeyPressed(KEY_LEFT))
                state->navigation.top =
                    (state->navigation.top+menu_count-1)%menu_count;
            else if(IsKeyPressed(KEY_RIGHT))
                state->navigation.top = (state->navigation.top+1)%menu_count;
            else if(IsKeyPressed(KEY_HOME)) state->navigation.top = 0;
            else if(IsKeyPressed(KEY_END)) state->navigation.top = menu_count-1;
            else if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
                    IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_DOWN)) {
                current = state->navigation.top;
                state->open_id = id+1+current;
                menu_navigation_reset(id,menus[current].items,
                                       menus[current].item_count);
                state->navigation.top = current;
                state->navigation.key_handled = 1;
            }
        } else if(IsKeyPressed(KEY_ESCAPE)) {
            state->open_id = 0;
            state->submenu_id = 0;
            state->navigation.depth = 0;
            state->navigation.path[0] = -1;
        } else if(state->navigation.depth == 0 && IsKeyPressed(KEY_LEFT)) {
            current = (current+menu_count-1)%menu_count;
            state->open_id = id+1+current;
            menu_navigation_reset(id,menus[current].items,
                                   menus[current].item_count);
            state->navigation.top = current;
            state->navigation.key_handled = 1;
        } else if(state->navigation.depth == 0 && IsKeyPressed(KEY_RIGHT)) {
            int selected = state->navigation.path[0];
            int opens_submenu = selected >= 0 &&
                selected < menus[current].item_count &&
                menus[current].items[selected].kind == MenuSubmenu &&
                !menus[current].items[selected].disabled;
            if(!opens_submenu) {
                current = (current+1)%menu_count;
                state->open_id = id+1+current;
                menu_navigation_reset(id,menus[current].items,
                                       menus[current].item_count);
                state->navigation.top = current;
                state->navigation.key_handled = 1;
            }
        }
    }
    if(ui_menu_bar_owns_open_menu(id, menu_count)) {
        PushInputCapture(bounds, 1);
        bar_capture_pushed = 1;
    }
    state->overlay.active = 0;
    if(can_draw) {
        StyleFrame bar_frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
            ButtonStateNormal, 0, 0, StyleKindMenu(), 1);
        ui_tk_draw_style_frame(bounds, (Rectangle){0}, bar_frame, 0, 0, 0, 0);
    }

    for(int i = 0; i < menu_count; i++) {
        int w = MenuGroupItemWidth(TextWidth(menus[i].label != NULL ? menus[i].label : "", font), metrics);
        Rectangle item = MenuGroupItemBounds(x, bounds, w, metrics);
        int menu_id = id + 1 + i;
        int open = state->open_id == menu_id;
        int hot = !UIContentDisabled() && ui_hot(item);
        ButtonState item_state = open ? ButtonStateSelected :
            (hot ? ButtonStateHover : ButtonStateNormal);
        StyleFrame item_frame = ui_tk_simple_style_frame(ButtonToneNeutral,
            item_state, 0, open, StyleKindMenuItem());
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        int item_font = item_style.font_size > 0.0f
            ? (int)(item_style.font_size + 0.5f)
            : font;
        Color item_text = Fade(item_style.foreground, item_style.opacity);
        if(can_draw && (hot || open))
            ui_tk_draw_style_frame(item, bounds, item_frame, hot, 0, 0,
                                   focused && state->navigation.top == i);
        if(hot)
            MarkClickable();
        if(can_draw)
            RenderText(menus[i].label != NULL ? menus[i].label : "",
                       x + Scale(12), ui_row_text_y(item, item_font),
                       item_font, item_text);
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            ConsumeRelease();
            SetFocus(id);
            state->open_id = open ? 0 : menu_id;
            if(state->open_id == 0)
                state->submenu_id = 0;
            else {
                state->navigation.top = i;
                menu_navigation_reset(id,menus[i].items,menus[i].item_count);
                state->navigation.top = i;
            }
            open = state->open_id == menu_id;
        }
        if(hot && state->open_id != 0 && !open) {
            state->open_id = menu_id;
            state->submenu_id = 0;
        }
        open = state->open_id == menu_id;
        if(open) {
            result.open_index = i;
            state->overlay.active = 1;
            state->overlay.bar_id = id;
            state->overlay.menu_id = id + i;
            state->overlay.x = x;
            state->overlay.y = (int)(bounds.y + bounds.height);
            int used = 0;
            copy_menu_items(state->overlay.items,&used,menus[i].items,
                            menus[i].item_count,&state->overlay.item_count);
        }
        x += w + metrics.bar_item_gap;
    }
    if(state->open_id != 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !ui_contains(bounds, mouse) &&
       (!state->panel_valid || !ui_contains(state->panel_bounds, mouse))) {
        ConsumeRelease();
        state->open_id = 0;
        state->submenu_id = 0;
        result.open_index = -1;
    }
    if(state->open_id != 0 && !bar_capture_pushed &&
       ui_menu_bar_owns_open_menu(id, menu_count))
        PushInputCapture(bounds, 1);
    if(open_index != NULL)
        *open_index = result.open_index;
    if(can_draw && focused)
        RenderFocus(bounds);
    return result;
}

void
ui_draw_menu_overlays(void)
{
    ToolkitStore *state = toolkit_state();
    int activated;

    if(state->overlay.active && state->open_id != 0) {
        state->panel_valid = 0;
        activated = draw_menu_items(state->overlay.x,
                                    state->overlay.y,
                                    state->overlay.items,
                                    state->overlay.item_count,
                                    state->overlay.bar_id, 0);
        if(activated != 0) {
            state->pending_bar_id = state->overlay.bar_id;
            state->pending_activated = activated;
            state->pending_closed_bar_id = state->overlay.bar_id;
        }
    }
    state->overlay.active = 0;

    if(state->context_overlay.active &&
       state->context_open_id == state->context_overlay.id) {
        Vector2 mouse = ui_mouse_world();

        state->panel_valid = 0;
        activated = draw_menu_items(state->context_overlay.x,
                                    state->context_overlay.y,
                                    state->context_overlay.items,
                                    state->context_overlay.item_count,
                                    state->context_overlay.id, 0);
        if(activated != 0) {
            state->context_pending_id = state->context_overlay.id;
            state->context_pending_activated = activated;
            state->context_open_id = 0;
        } else if(!state->context_overlay.suppress_close &&
                  IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                  (!state->panel_valid ||
                   !ui_contains(state->panel_bounds, mouse))) {
            ConsumeRelease();
            state->context_pending_closed_id = state->context_overlay.id;
            state->context_open_id = 0;
        }
    }
    state->context_overlay.active = 0;
}

int
RenderPopupMenu(int id, int x, int y, const MenuItem *items, int item_count)
{
    Rectangle panel = menu_items_panel_bounds(x,y,items,item_count);
    int focused = !UIContentDisabled() && id > 0 && RegisterFocus(id,panel);
    if(focused && !ui_popup_input_focus_captures(id) &&
       IsKeyPressed(KEY_ESCAPE)) {
        menu_navigation_reset(0,NULL,0);
        SetFocus(0);
        return 0;
    }
    menu_navigation_begin_frame();
    return draw_menu_items(x,y,items,item_count,id,0);
}

int
RenderContextMenu(MenuProps menu)
{
    ToolkitStore *state = toolkit_state();
    Vector2 mouse = ui_mouse_world();
    int open_local = 0;
    int x_local = 0;
    int y_local = 0;
    Rectangle panel;
    int suppress_close = 0;
    int focused;

    if(menu.open == NULL)
        menu.open = &open_local;
    if(menu.x == NULL)
        menu.x = &x_local;
    if(menu.y == NULL)
        menu.y = &y_local;
    if(state->context_pending_id == menu.id) {
        int activated = state->context_pending_activated;

        state->context_pending_id = 0;
        state->context_pending_activated = 0;
        *menu.open = 0;
        return activated;
    }
    if(state->context_pending_closed_id == menu.id) {
        state->context_pending_closed_id = 0;
        *menu.open = 0;
        return 0;
    }
    if(!UIContentDisabled() && ui_contains(menu.trigger, mouse) &&
       !InputCapturesClick(mouse) &&
       IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        *menu.open = 1;
        *menu.x = (int)mouse.x;
        *menu.y = (int)mouse.y;
        SetFocus(menu.id);
        menu_navigation_reset(menu.id,menu.items,menu.item_count);
        suppress_close = 1;
    }
    if(!*menu.open) {
        if(state->context_open_id == menu.id)
            state->context_open_id = 0;
        return 0;
    }

    if(ui_contains(menu.trigger, mouse) &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        suppress_close = 1;
    state->context_open_id = menu.id;
    panel = menu_items_panel_bounds(*menu.x, *menu.y,
                                    menu.items, menu.item_count);
    focused = !UIContentDisabled() && menu.id > 0 && RegisterFocus(menu.id,panel) &&
              !ui_popup_input_focus_captures(menu.id);
    if(focused && IsKeyPressed(KEY_ESCAPE)) {
        *menu.open = 0;
        state->context_open_id = 0;
        state->submenu_id = 0;
        menu_navigation_reset(0,NULL,0);
        return 0;
    }
    menu_navigation_begin_frame();
    ui_menu_track_panel(panel);
    PushInputCapture(panel, 1);
    if(ui_contains(panel, mouse))
        MarkCursor(MOUSE_CURSOR_DEFAULT);
    queue_context_menu_overlay(menu, suppress_close);
    return 0;
}

MenuResult
RenderMenu(MenuProps menu)
{
    MenuResult result = {0, -1};
    if(menu.mode == MenuModeBar) {
        return RenderMenuGroups(menu.id, menu.bounds, menu.menus,
                                menu.menu_count, menu.open_index);
    }
    if(menu.mode == MenuModePopup) {
        result.activated_id = RenderPopupMenu(menu.id, (int)menu.bounds.x,
                                              (int)menu.bounds.y, menu.items,
                                              menu.item_count);
        return result;
    }
    result.activated_id = RenderContextMenu(menu);
    return result;
}

int
RenderRadio(RadioProps radio)
{
    ToolkitStore *toolkit = toolkit_state();
    ButtonState initial_state = radio.disabled ? ButtonStateDisabled
                                               : ButtonStateNormal;
    StyleFrame label_frame = ui_tk_radio_style_frame(ButtonToneNeutral,
                                                     initial_state,
                                                     radio.disabled,
                                                     radio.checked,
                                                     radio.class_name, 6);
    Style label_style = ui_unpack_style(ui_style_apply_effects_frame(
        label_frame).value);
    int font = label_style.font_size > 0.0f
        ? (int)(label_style.font_size + 0.5f)
        : GetFontSize();
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    RadioPaint paint = RadioPaintFor((RadioSpec){
        .bounds = radio.bounds,
        .checked = radio.checked,
        .disabled = radio.disabled,
        .default_style = ui_default_style(),
        .selected_amount = radio.checked ? 1.0f : 0.0f,
        .scale = runtime_scale,
        .frame = ui_tk_radio_style_frame(ButtonToneNeutral,
                                         initial_state,
                                         radio.disabled, radio.checked,
                                         radio.class_name, 11),
        .selected = ui_tk_radio_style_frame(ButtonToneAccent,
                                            initial_state,
                                            radio.disabled, radio.checked,
                                            radio.class_name, 10)
    });
    paint.label_color = ColorToInt(label_style.foreground);
    int hot;
    int down;
    int focused = 0;
    int activated;

    activated = ui_focusable_pressed(paint.hit_bounds, radio.id, radio.disabled,
                                     &focused);
    hot = ui_hot(paint.hit_bounds) && !radio.disabled && !UIContentDisabled();
    down = hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if(hot)
        MarkClickable();
    if(radio.disabled)
        MarkDisabled();
    if(!IsWindowReady())
        return activated ? radio.id : 0;
    if(ui_default_style()) {
        UIRadioAnimState *anim;
        Rectangle state_bounds = {
            paint.center.x - paint.touch / 2.0f,
            paint.center.y - paint.touch / 2.0f,
            paint.touch,
            paint.touch
        };
        unsigned int key = 2166136261u;
        float target = radio.checked ? 1.0f : 0.0f;
        float dt;
        float selected;
        float press;
        Color ring;
        Color fill;
        Color label;
        const char *text = radio.label != NULL ? radio.label : "";

        key = (key ^ (unsigned int)radio.id) * 16777619u;
        key = (key ^ (unsigned int)(int)radio.bounds.x) * 16777619u;
        key = (key ^ (unsigned int)(int)radio.bounds.y) * 16777619u;
        key = (key ^ (unsigned int)(int)radio.bounds.width) * 16777619u;
        key = (key ^ (unsigned int)(int)radio.bounds.height) * 16777619u;
        while(*text != '\0')
            key = (key ^ (unsigned char)*text++) * 16777619u;
        anim = &toolkit->radio_anim[key % UI_RADIO_ANIM_MAX];
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
        paint = RadioPaintFor((RadioSpec){
            .bounds = radio.bounds,
            .checked = radio.checked,
            .disabled = radio.disabled,
            .default_style = 1,
            .selected_amount = selected,
            .scale = runtime_scale,
            .frame = ui_tk_radio_style_frame(ButtonToneNeutral,
                                             ui_tk_checkbox_button_state(hot,
                                                 down, focused,
                                                 radio.disabled),
                                             radio.disabled, radio.checked,
                                             radio.class_name, 11),
            .selected = ui_tk_radio_style_frame(ButtonToneAccent,
                                                ui_tk_checkbox_button_state(hot,
                                                    down, focused,
                                                    radio.disabled),
                                                radio.disabled, radio.checked,
                                                radio.class_name, 10)
        });
        label_frame = ui_tk_radio_style_frame(ButtonToneNeutral,
                                              ui_tk_checkbox_button_state(hot, down,
                                                                         focused,
                                                                         radio.disabled),
                                              radio.disabled, radio.checked,
                                              radio.class_name, 6);
        label_style = ui_unpack_style(ui_style_apply_effects_frame(
            label_frame).value);
        if(label_style.font_size > 0.0f)
            font = (int)(label_style.font_size + 0.5f);
        paint.label_color = ColorToInt(label_style.foreground);

        ring = GetColor(paint.ring_color);
        fill = GetColor(paint.fill_color);
        label = Fade(GetColor(paint.label_color), label_style.opacity);
        if(radio.disabled) {
            press = 0.0f;
        }
        if(hot && HoverEffectsEnabled()) {
            Color layer = ring;
            layer.a = (unsigned char)(20 + 11 * press);
            DrawCircleV(paint.center, paint.touch / 2.0f, layer);
        } else if(down) {
            Color layer = ring;
            layer.a = 31;
            DrawCircleV(paint.center, paint.touch / 2.0f, layer);
        }
        ui_default_ripple(state_bounds, ring, (int)key, down);
        if(paint.fill_radius > 0.2f)
            DrawCircleV(paint.center, paint.fill_radius, fill);
        if(paint.stroke_width > 0.0f)
            DrawRing(paint.center, paint.outer_radius - paint.stroke_width,
                     paint.outer_radius, 0.0f, 360.0f, 48, ring);
        RenderText(radio.label != NULL ? radio.label : "",
                   (int)paint.label_x,
                   ui_row_text_y(radio.bounds, font), font, label);
    } else {
        if(paint.stroke_width > 0.0f)
            DrawCircleLines((int)paint.center.x, (int)paint.center.y,
                            paint.outer_radius, GetColor(paint.ring_color));
        if(radio.checked)
            DrawCircleV(paint.center, paint.fill_radius,
                        GetColor(paint.fill_color));
        RenderText(radio.label != NULL ? radio.label : "", (int)paint.label_x,
                   ui_row_text_y(radio.bounds, font), font,
                   Fade(GetColor(paint.label_color), label_style.opacity));
    }
    if(focused && IsWindowReady())
        RenderFocus(paint.hit_bounds);
    if(activated) {
        return radio.id;
    }
    return 0;
}

void
RenderProgress(ProgressProps progress)
{
    ProgressPaint paint;
    Rectangle fill;
    const char *label = progress.label;
    StyleFrame text = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, progress.class_name, StyleKindProgress(), 6);
    Style text_style = ui_unpack_style(ui_style_apply_effects_frame(text).value);
    int font = text_style.font_size > 0.0f
        ? (int)(text_style.font_size + 0.5f)
        : GetSmallFontSize();
    int label_w = label != NULL ? TextWidth(label, font) : 0;
    int pad = Scale(6);
    StyleFrame track = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, progress.class_name, StyleKindProgress(), 4);
    StyleFrame active = ui_tk_simple_style_frame_class_role(ButtonToneAccent,
        ButtonStateNormal, 0, 1, progress.class_name, StyleKindProgress(), 5);

    paint = ProgressPaintFor(progress.bounds, progress.min, progress.max,
                             progress.value, (float)label_w, (float)pad,
                             (float)Scale(1000) / 1000.0f, track, active,
                             text);
    fill = paint.layout.fill_bounds;
    if(!IsWindowReady())
        return;
    if(ui_modern_style() || ui_default_style()) {
        float radius = progress.bounds.height > 0.0f
            ? paint.radius / progress.bounds.height
            : 0.0f;
        if(radius < 0.0f)
            radius = 0.0f;
        if(radius > 1.0f)
            radius = 1.0f;
        DrawRectangleRounded(progress.bounds, radius, 12,
                             GetColor(paint.track_color));
        if(fill.width > 0.0f)
            DrawRectangleRounded(fill, radius, 12,
                                 GetColor(paint.fill_color));
        if(paint.border_width > 0.0f)
            DrawRectangleRoundedLinesEx(progress.bounds, radius, 12,
                                        paint.border_width,
                                        GetColor(paint.border_color));
    } else {
        DrawRectangleRec(progress.bounds, GetColor(paint.track_color));
        DrawRectangleRec(fill, GetColor(paint.fill_color));
        if(paint.border_width > 0.0f)
            DrawRectangleLinesEx(progress.bounds, paint.border_width,
                                 GetColor(paint.border_color));
    }
    if(label != NULL) {
        int text_x = (int)paint.layout.label_x;
        int text_y = GetUIControlTextY(label, (int)progress.bounds.y,
                                       (int)progress.bounds.height, font);
        Color text_color = GetColor(paint.layout.label_on_fill
            ? paint.filled_label_color
            : paint.label_color);
        RenderText(label, text_x, text_y, font,
                   Fade(text_color, text_style.opacity));
    }
}

static void
ui_plot(PlotProps plot, int histogram)
{
    float min_value;
    float max_value;
    PlotRange range;
    int count = plot.value_count;
    int offset;
    StyleFrame plot_frame;
    StyleFrame mark_frame;
    Style plot_style;

    if(!IsWindowReady())
        return;
    plot_frame = ui_tk_simple_style_frame(ButtonToneNeutral, ButtonStateNormal,
                                          0, 0, StyleKindPlot());
    mark_frame = ui_tk_simple_style_frame(ButtonToneAccent, ButtonStateSelected,
                                          0, 1, StyleKindPlotMark());
    plot_style = ui_unpack_style(ui_style_apply_effects_frame(plot_frame).value);
    DrawRectangleRec(plot.bounds, plot_style.background);
    DrawRectangleLinesEx(plot.bounds, plot_style.border_width,
                         plot_style.border);
    if(plot.values == NULL || count <= 0)
        return;
    offset = PlotOffset(count, plot.offset);
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
    range = PlotRangeFor(plot.scale_min, plot.scale_max, min_value, max_value);
    BeginClip((int)plot.bounds.x, (int)plot.bounds.y,
                (int)plot.bounds.width, (int)plot.bounds.height);
    if(histogram) {
        for(int i = 0; i < count; i++) {
            float value = plot.values[(offset + i) % count];
            PlotMark bar = PlotHistogramBar(plot.bounds, i, count, value,
                                             range, mark_frame);
            DrawRectangleRec(bar.bounds, GetColor(bar.color));
        }
    } else if(count == 1) {
        PlotMark line = PlotSingleLine(plot.bounds, plot.values[offset],
                                       range, mark_frame);
        DrawLine((int)line.bounds.x, (int)line.bounds.y,
                 (int)(line.bounds.x + line.bounds.width),
                 (int)line.bounds.y, GetColor(line.color));
    } else {
        for(int i = 1; i < count; i++) {
            float a = plot.values[(offset + i - 1) % count];
            float b = plot.values[(offset + i) % count];
            PlotMark line = PlotLineSegment(plot.bounds, i, count, a, b,
                                            range, mark_frame);
            DrawLine((int)line.bounds.x, (int)line.bounds.y,
                     (int)(line.bounds.x + line.bounds.width),
                     (int)(line.bounds.y + line.bounds.height),
                     GetColor(line.color));
        }
    }
    EndClip();
    {
        Style plot_style = ui_unpack_style(
            ui_style_apply_effects_frame(plot_frame).value);
        int font = plot_style.font_size > 0.0f
            ? (int)(plot_style.font_size + 0.5f)
            : GetSmallFontSize();
        float runtime_scale = (float)Scale(1000) / 1000.0f;
        float label_width = plot.label != NULL ? (float)TextWidth(plot.label, font) : 0.0f;
        float overlay_width = plot.overlay != NULL ? (float)TextWidth(plot.overlay, font) : 0.0f;
        PlotTextPaint text = PlotTextPaintFor(plot.bounds, label_width,
            overlay_width, runtime_scale, plot_frame,
            plot.label != NULL, plot.overlay != NULL);
        if(text.show_label)
            RenderText(plot.label, (int)text.label_bounds.x,
                       (int)text.label_bounds.y, font,
                       Fade(GetColor(text.text_color), plot_style.opacity));
        if(text.show_overlay)
            RenderText(plot.overlay, (int)text.overlay_bounds.x,
                       (int)text.overlay_bounds.y, font,
                       Fade(GetColor(text.text_color), plot_style.opacity));
    }
}

void
RenderPlotLines(PlotProps plot)
{
    ui_plot(plot, 0);
}

void
RenderPlotHistogram(PlotProps plot)
{
    ui_plot(plot, 1);
}

int
ui_numeric_focus_id(int id, int component, int integer)
{
    unsigned int token;

    if(id <= 0)
        return 0;
    if(component == 0)
        return id;
    token = (integer ? 0x50000000u : 0x40000000u) ^
            ((unsigned int)id << 4) ^ (unsigned int)(component + 1);
    token &= (unsigned int)INT_MAX;
    return token != 0 ? (int)token : id;
}

static int ui_slider_keyboard_direction(int vertical);
static int ui_numeric_input_filter(int codepoint, void *user_data);
static UINumericInputState *ui_numeric_input_find(int kind, int widget_id,
                                                   int component);

enum {
    UI_NUMERIC_EDIT_DRAG_FLOAT = 3,
    UI_NUMERIC_EDIT_DRAG_INT,
    UI_NUMERIC_EDIT_SLIDER_FLOAT,
    UI_NUMERIC_EDIT_SLIDER_INT
};

static int
ui_numeric_temp_edit(Rectangle bounds, int kind, int widget_id, int component,
                     int focus_id, void *value, const char *format,
                     int disabled, int integer, int *editing)
{
    ToolkitStore *toolkit = toolkit_state();
    int enabled = !disabled && !UIContentDisabled();
    int control = IsKeyDown(KEY_LEFT_CONTROL) ||
                  IsKeyDown(KEY_RIGHT_CONTROL);
    int pressed = enabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                  ui_hot(bounds);
    Vector2 mouse = ui_mouse_world();
    double now = GetTime();
    int slop = Scale(6);
    float dx = mouse.x - toolkit->numeric_click.position.x;
    float dy = mouse.y - toolkit->numeric_click.position.y;
    int double_click = pressed && toolkit->numeric_click.valid &&
        toolkit->numeric_click.kind == kind &&
        toolkit->numeric_click.widget_id == widget_id &&
        toolkit->numeric_click.component == component &&
        now - toolkit->numeric_click.time <= 0.30 &&
        dx >= -slop && dx <= slop && dy >= -slop && dy <= slop;
    int activate = pressed && (control || double_click);
    UINumericInputState *state =
        ui_numeric_input_find(kind, widget_id, component);
    int commit = 0;
    int changed = 0;

    if(pressed) {
        toolkit->numeric_click = (UINumericClickState){
            1, kind, widget_id, component, mouse, now
        };
        if(activate)
            toolkit->numeric_click.valid = 0;
    }

    if(state == NULL && !activate) {
        *editing = 0;
        return 0;
    }
    if(state == NULL)
        state = ui_numeric_input_state(kind, widget_id, component);
    if(!enabled && state->focused) {
        state->focused = 0;
        ClearTextInputFocus();
    }
    if(activate) {
        if(integer)
            snprintf(state->text, sizeof(state->text),
                     format != NULL ? format : "%d", *(int *)value);
        else
            snprintf(state->text, sizeof(state->text),
                     format != NULL ? format : "%.3f", *(float *)value);
        state->cursor = (int)strlen(state->text);
        state->focused = 1;
        SetFocus(focus_id);
        toolkit->drag_active = 0;
        toolkit->slider_active = 0;
    }
    if(!state->focused) {
        *editing = 0;
        return 0;
    }

    BeginDisabled(!enabled);
    if(ui_text_field_render_filtered((TextFieldProps){
            .bounds = bounds,
            .text = state->text,
            .text_size = sizeof(state->text),
            .cursor_position = &state->cursor,
            .focused = &state->focused,
            .max_codepoints = 63,
            .font = 0,
            .focus_id = focus_id,
            .style = kryon_zero_text_input_style,
            .commit_pressed = &commit,
            .read_only = !enabled
        }, ui_numeric_input_filter, NULL)) {
        char *end = NULL;
        if(integer) {
            long parsed = strtol(state->text, &end, 0);
            if(end != state->text && *end == '\0' &&
               parsed >= INT_MIN && parsed <= INT_MAX &&
               *(int *)value != (int)parsed) {
                *(int *)value = (int)parsed;
                changed = 1;
            }
        } else {
            float parsed = strtof(state->text, &end);
            if(end != state->text && *end == '\0' &&
               isfinite(parsed) && *(float *)value != parsed) {
                *(float *)value = parsed;
                changed = 1;
            }
        }
    }
    EndDisabled();
    if(commit) {
        state->focused = 0;
        ClearTextInputFocus();
        SetFocus(focus_id);
    }
    *editing = state->focused;
    return changed;
}

static int
ui_drag_delta(int token, int focus_id, Rectangle bounds, int disabled,
              float *delta)
{
    ToolkitStore *toolkit = toolkit_state();
    Vector2 mouse = ui_mouse_world();
    disabled = disabled || UIContentDisabled();
    int hot = !disabled && ui_hot(bounds);

    *delta = 0.0f;
    if(toolkit->drag_active && ui_popup_input_owner_captures(toolkit->drag_owner))
        toolkit->drag_active = 0;
    if(disabled && toolkit->drag_active == token)
        toolkit->drag_active = 0;
    if(hot)
        MarkClickable();
    if(hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        toolkit->drag_active = token;
        toolkit->drag_last_x = mouse.x;
        toolkit->drag_owner = ui_popup_input_owner();
        if(focus_id > 0)
            SetFocus(focus_id);
    }
    if(!disabled && toolkit->drag_active == token && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        *delta = mouse.x - toolkit->drag_last_x;
        toolkit->drag_last_x = mouse.x;
    }
    if(toolkit->drag_active == token && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        toolkit->drag_active = 0;
    return *delta != 0.0f;
}

static int
ui_update_drag_scalar_keyboard(int focus_id, float speed, float minimum,
                              float maximum, float *value)
{
    int direction;
    DragScalarStep step;

    if(focus_id <= 0 || !IsFocusActive(focus_id) ||
       !IsKeyboardInputEnabled() || ui_popup_input_focus_captures(focus_id))
        return 0;
    direction = ui_slider_keyboard_direction(0);
    step = DragScalarKeyboardValue(*value, speed, minimum, maximum, direction,
        IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    if(!step.changed) return 0;
    *value = step.value;
    return 1;
}

static int
ui_update_drag_whole_keyboard(int focus_id, float speed, int minimum,
                            int maximum, int *value)
{
    int direction;
    DragWholeStep step;

    if(focus_id <= 0 || !IsFocusActive(focus_id) ||
       !IsKeyboardInputEnabled() || ui_popup_input_focus_captures(focus_id))
        return 0;
    direction = ui_slider_keyboard_direction(0);
    step = DragWholeKeyboardValue(*value, speed, minimum, maximum, direction,
        IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    if(!step.changed) return 0;
    *value = step.value;
    return 1;
}

int
ui_update_drag_scalar(DragScalarProps drag)
{
    int count = drag.value_count;
    int changed = 0;
    float speed = DragEffectiveSpeed(drag.speed);

    if(drag.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int focus_id = ui_numeric_focus_id(drag.id,i,0);
        Rectangle cell = {drag.bounds.x + drag.bounds.width * i / count,
                          drag.bounds.y, drag.bounds.width / count,
                          drag.bounds.height};
        float delta;
        int enabled = !drag.disabled && !UIContentDisabled();
        int editing = 0;
        if(enabled && focus_id > 0) RegisterFocus(focus_id,cell);
        changed |= ui_numeric_temp_edit(cell, UI_NUMERIC_EDIT_DRAG_FLOAT,
            drag.id, i, focus_id, &drag.values[i], drag.format,
            drag.disabled, 0, &editing);
        if(editing)
            continue;
        if(enabled && ui_update_drag_scalar_keyboard(focus_id,speed,
                drag.min,drag.max,&drag.values[i]))
            changed = 1;
        if(ui_drag_delta((int)(((unsigned int)drag.id << 4) ^
                               (unsigned int)(i + 1)), focus_id, cell,
                         drag.disabled, &delta)) {
            DragScalarStep step = DragScalarDeltaValue(drag.values[i], delta,
                                                     speed, drag.min,
                                                     drag.max);
            if(step.changed) {
                drag.values[i] = step.value;
                changed = 1;
            }
        }
    }
    return changed;
}

int
ui_update_drag_whole(DragWholeProps drag)
{
    int count = drag.value_count;
    int changed = 0;
    float speed = DragEffectiveSpeed(drag.speed);

    if(drag.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int focus_id = ui_numeric_focus_id(drag.id,i,1);
        Rectangle cell = {drag.bounds.x + drag.bounds.width * i / count,
                          drag.bounds.y, drag.bounds.width / count,
                          drag.bounds.height};
        float delta;
        int enabled = !drag.disabled && !UIContentDisabled();
        int editing = 0;
        if(enabled && focus_id > 0) RegisterFocus(focus_id,cell);
        changed |= ui_numeric_temp_edit(cell, UI_NUMERIC_EDIT_DRAG_INT,
            drag.id, i, focus_id, &drag.values[i], drag.format,
            drag.disabled, 1, &editing);
        if(editing)
            continue;
        if(enabled && ui_update_drag_whole_keyboard(focus_id,speed,
                drag.min,drag.max,&drag.values[i]))
            changed = 1;
        if(ui_drag_delta((int)(((unsigned int)drag.id << 4) ^
                               (unsigned int)(i + 1)), focus_id, cell,
                         drag.disabled, &delta)) {
            DragWholeStep step = DragWholeDeltaValue(drag.values[i], delta,
                                                 speed, drag.min, drag.max);
            if(step.changed) {
                drag.values[i] = step.value;
                changed = 1;
            }
        }
    }
    return changed;
}

static void
ui_paint_drag_cell(Rectangle bounds, const char *text, int disabled, int focused)
{
    ButtonState state = disabled ? ButtonStateDisabled : ButtonStateNormal;
    StyleFrame frame = ui_tk_simple_style_frame(ButtonToneNeutral, state,
                                                disabled, 0,
                                                StyleKindDragValue());
    Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
    int font = style.font_size > 0.0f
        ? (int)(style.font_size + 0.5f)
        : GetSmallFontSize();

    ui_tk_draw_style_frame(bounds, bounds, frame, 0, 0, disabled, focused);
    RenderText(text, (int)bounds.x + Scale(6),
               ui_row_text_y(bounds, font),
               font, Fade(style.foreground, style.opacity));
}

static void
ui_paint_drag_label(Rectangle bounds, const char *label)
{
    if(label != NULL) {
        StyleFrame frame = ui_tk_simple_style_frame(ButtonToneNeutral,
                                                    ButtonStateNormal, 0, 0,
                                                    StyleKindDrag());
        Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
        int font = style.font_size > 0.0f
            ? (int)(style.font_size + 0.5f)
            : GetSmallFontSize();
        RenderText(label, (int)bounds.x + Scale(6),
                   (int)bounds.y - font - Scale(2),
                   font, Fade(style.foreground, style.opacity));
    }
}

void
ui_paint_drag_scalar(DragScalarProps drag)
{
    if(!IsWindowReady() || drag.values == NULL || drag.value_count <= 0)
        return;
    for(int i = 0; i < drag.value_count; i++) {
        Rectangle cell = {drag.bounds.x + drag.bounds.width*i/drag.value_count,
                          drag.bounds.y, drag.bounds.width/drag.value_count,
                          drag.bounds.height};
        char text[64];
        int focus_id = ui_numeric_focus_id(drag.id,i,0);
        UINumericInputState *state = ui_numeric_input_find(
            UI_NUMERIC_EDIT_DRAG_FLOAT, drag.id, i);
        int disabled = drag.disabled || UIContentDisabled();
        if(state != NULL && state->focused)
            continue;
        int focused = !disabled && focus_id > 0 && IsFocusActive(focus_id) &&
                      !ui_popup_input_focus_captures(focus_id);
        snprintf(text, sizeof(text), drag.format != NULL ? drag.format : "%.3f",
                 drag.values[i]);
        ui_paint_drag_cell(cell,text,disabled,focused);
    }
    ui_paint_drag_label(drag.bounds, drag.label);
}

void
ui_paint_drag_whole(DragWholeProps drag)
{
    if(!IsWindowReady() || drag.values == NULL || drag.value_count <= 0)
        return;
    for(int i = 0; i < drag.value_count; i++) {
        Rectangle cell = {drag.bounds.x + drag.bounds.width*i/drag.value_count,
                          drag.bounds.y, drag.bounds.width/drag.value_count,
                          drag.bounds.height};
        char text[64];
        int focus_id = ui_numeric_focus_id(drag.id,i,1);
        UINumericInputState *state = ui_numeric_input_find(
            UI_NUMERIC_EDIT_DRAG_INT, drag.id, i);
        int disabled = drag.disabled || UIContentDisabled();
        if(state != NULL && state->focused)
            continue;
        int focused = !disabled && focus_id > 0 && IsFocusActive(focus_id) &&
                      !ui_popup_input_focus_captures(focus_id);
        snprintf(text, sizeof(text), drag.format != NULL ? drag.format : "%d",
                 drag.values[i]);
        ui_paint_drag_cell(cell,text,disabled,focused);
    }
    ui_paint_drag_label(drag.bounds, drag.label);
}

static int
ui_slider_ratio(int token, int focus_id, Rectangle bounds, int disabled,
                int vertical, float *ratio)
{
    ToolkitStore *toolkit = toolkit_state();
    Vector2 mouse = ui_mouse_world();
    disabled = disabled || UIContentDisabled();
    int hot = !disabled && ui_hot(bounds);
    int pressed = hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if(toolkit->slider_active &&
       ui_popup_input_owner_captures(toolkit->slider_owner))
        toolkit->slider_active = 0;
    if(disabled && toolkit->slider_active == token)
        toolkit->slider_active = 0;
    if(hot)
        MarkClickable();
    if(pressed) {
        toolkit->slider_active = token;
        toolkit->slider_owner = ui_popup_input_owner();
        if(focus_id > 0)
            SetFocus(focus_id);
    }
    if(!disabled && toolkit->slider_active == token &&
       (pressed || IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
        float span = vertical ? bounds.height : bounds.width;
        float position = vertical ? bounds.y + bounds.height - mouse.y
                                  : mouse.x - bounds.x;
        *ratio = span > 0.0f ? position / span : 0.0f;
        if(*ratio < 0.0f) *ratio = 0.0f;
        if(*ratio > 1.0f) *ratio = 1.0f;
        return 1;
    }
    if(toolkit->slider_active == token && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        toolkit->slider_active = 0;
    return 0;
}

static int
ui_slider_keyboard_direction(int vertical)
{
    if(vertical) {
        if(IsKeyPressed(KEY_UP)) return 1;
        if(IsKeyPressed(KEY_DOWN)) return -1;
    } else {
        if(IsKeyPressed(KEY_RIGHT)) return 1;
        if(IsKeyPressed(KEY_LEFT)) return -1;
    }
    return 0;
}

static int
ui_update_slider_scalar_keyboard(int focus_id, int vertical, float minimum,
                                float maximum, float *value)
{
    int direction;
    SliderScalarStep step;

    if(focus_id <= 0 || !IsFocusActive(focus_id) ||
       !IsKeyboardInputEnabled() || ui_popup_input_focus_captures(focus_id))
        return 0;
    direction = ui_slider_keyboard_direction(vertical);
    step = SliderScalarKeyboardValue(*value, minimum, maximum, direction,
        IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    if(!step.changed)
        return 0;
    *value = step.value;
    return 1;
}

static int
ui_update_slider_whole_keyboard(int focus_id, int vertical, int minimum,
                              int maximum, int *value)
{
    int direction;
    SliderWholeStep step;

    if(focus_id <= 0 || !IsFocusActive(focus_id) ||
       !IsKeyboardInputEnabled() || ui_popup_input_focus_captures(focus_id))
        return 0;
    direction = ui_slider_keyboard_direction(vertical);
    step = SliderWholeKeyboardValue(*value, minimum, maximum, direction,
        IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    if(!step.changed)
        return 0;
    *value = step.value;
    return 1;
}

static void
ui_draw_slider_cell(Rectangle cell, float ratio, const char *text,
                    int disabled, int vertical, int focused, int class_name)
{
    if(!IsWindowReady())
        return;
    Vector2 mouse = ui_mouse_world();
    int hovered = CheckCollisionPointRec(mouse, cell) &&
                  !disabled &&
                  !InputCapturesClick(mouse);
    ButtonState state = disabled ? ButtonStateDisabled :
                        (focused ? ButtonStateFocus :
                         (hovered ? ButtonStateHover : ButtonStateNormal));
    StyleFrame track = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        state, disabled, 0, class_name, StyleKindSlider(), 4);
    StyleFrame active = ui_tk_simple_style_frame_class_role(ButtonToneAccent,
        state, disabled, 1, class_name, StyleKindSlider(), 5);
    StyleFrame thumb = ui_tk_simple_style_frame_class_role(ButtonToneAccent,
        state, disabled, 1, class_name, StyleKindSliderThumb(), StyleAny());
    StyleFrame label = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        state, disabled, 0, class_name, StyleKindSlider(), 6);
    Style label_style = ui_unpack_style(ui_style_apply_effects_frame(label).value);
    int label_font = label_style.font_size > 0.0f
        ? (int)(label_style.font_size + 0.5f)
        : GetSmallFontSize();

    ui_tk_draw_slider_paint(SliderPaintFor((SliderSpec){
        .bounds = cell,
        .ratio = ratio,
        .vertical = vertical != 0,
        .active = focused,
        .hovered = hovered,
        .disabled = disabled,
        .scale = (float)Scale(1000) / 1000.0f,
        .track = track,
        .active_track = active,
        .thumb = thumb
    }), hovered, focused, disabled);
    RenderText(text, (int)cell.x + Scale(6),
               ui_row_text_y(cell, label_font),
               label_font, Fade(label_style.foreground, label_style.opacity));
    if(focused)
        RenderFocus(cell);
}

static void
ui_draw_slider_label(Rectangle bounds, const char *label, int class_name)
{
    if(IsWindowReady() && label != NULL) {
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            ButtonStateNormal, 0, 0, class_name, StyleKindSlider(), 6);
        Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
        int font = style.font_size > 0.0f
            ? (int)(style.font_size + 0.5f)
            : GetSmallFontSize();
        RenderText(label, (int)bounds.x + Scale(6),
                   (int)bounds.y - font - Scale(2),
                   font, Fade(style.foreground, style.opacity));
    }
}

int
ui_update_slider_scalar(SliderScalarProps slider, int vertical)
{
    int changed = 0;
    int count = slider.value_count;

    if(slider.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int focus_id = ui_numeric_focus_id(slider.id,i,0);
        Rectangle cell = {slider.bounds.x + slider.bounds.width * i / count,
                          slider.bounds.y, slider.bounds.width / count,
                          slider.bounds.height};
        float ratio = SliderScalarRatio(slider.values[i], slider.min,
                                       slider.max);
        int enabled = !slider.disabled && !UIContentDisabled();
        int editing = 0;
        if(enabled && focus_id > 0)
            RegisterFocus(focus_id,cell);
        changed |= ui_numeric_temp_edit(cell, UI_NUMERIC_EDIT_SLIDER_FLOAT,
            slider.id, i, focus_id, &slider.values[i], slider.format,
            slider.disabled, 0, &editing);
        if(editing)
            continue;
        if(enabled && ui_update_slider_scalar_keyboard(focus_id,vertical,
                    slider.min,slider.max,&slider.values[i])) {
            ratio = SliderScalarRatio(slider.values[i], slider.min,
                                     slider.max);
            changed = 1;
        }
        if(slider.max > slider.min && ui_slider_ratio((int)(0x40000000u ^
                                           ((unsigned int)slider.id << 4) ^
                                           (unsigned int)(i + 1)),
                                           focus_id, cell, slider.disabled,
                                           vertical, &ratio)) {
            float value = SliderScalarValue(slider.min, slider.max, ratio);
            if(value != slider.values[i]) {
                slider.values[i] = value;
                changed = 1;
            }
        }
    }
    return changed;
}

int
ui_update_slider_whole(SliderWholeProps slider, int vertical)
{
    int changed = 0;
    int count = slider.value_count;

    if(slider.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int focus_id = ui_numeric_focus_id(slider.id,i,1);
        Rectangle cell = {slider.bounds.x + slider.bounds.width * i / count,
                          slider.bounds.y, slider.bounds.width / count,
                          slider.bounds.height};
        float ratio = SliderWholeRatio(slider.values[i], slider.min,
                                     slider.max);
        int enabled = !slider.disabled && !UIContentDisabled();
        int editing = 0;
        if(enabled && focus_id > 0)
            RegisterFocus(focus_id,cell);
        changed |= ui_numeric_temp_edit(cell, UI_NUMERIC_EDIT_SLIDER_INT,
            slider.id, i, focus_id, &slider.values[i], slider.format,
            slider.disabled, 1, &editing);
        if(editing)
            continue;
        if(enabled && ui_update_slider_whole_keyboard(focus_id,vertical,
                    slider.min,slider.max,&slider.values[i])) {
            ratio = SliderWholeRatio(slider.values[i], slider.min,
                                   slider.max);
            changed = 1;
        }
        if(slider.max > slider.min && ui_slider_ratio((int)(0x50000000u ^
                                        ((unsigned int)slider.id << 4) ^
                                        (unsigned int)(i + 1)),
                                        focus_id, cell, slider.disabled,
                                        vertical, &ratio)) {
            int value = SliderWholeValue(slider.min, slider.max, ratio);
            if(value != slider.values[i]) {
                slider.values[i] = value;
                changed = 1;
            }
        }
    }
    return changed;
}

void
ui_paint_slider_scalar(SliderScalarProps slider, int vertical)
{
    if(!IsWindowReady() || slider.values == NULL || slider.value_count <= 0) return;
    slider.disabled |= UIContentDisabled();
    for(int i = 0; i < slider.value_count; i++) {
        Rectangle cell = {slider.bounds.x + slider.bounds.width*i/slider.value_count,
                          slider.bounds.y, slider.bounds.width/slider.value_count,
                          slider.bounds.height};
        float ratio = SliderScalarRatio(slider.values[i], slider.min,
                                       slider.max);
        char text[64];
        int focus_id = ui_numeric_focus_id(slider.id,i,0);
        UINumericInputState *state = ui_numeric_input_find(
            UI_NUMERIC_EDIT_SLIDER_FLOAT, slider.id, i);
        if(state != NULL && state->focused)
            continue;
        int focused = !slider.disabled && focus_id > 0 &&
                      IsFocusActive(focus_id) &&
                      !ui_popup_input_focus_captures(focus_id);
        snprintf(text,sizeof(text),slider.format != NULL ? slider.format : "%.3f",slider.values[i]);
        ui_draw_slider_cell(cell,ratio,text,slider.disabled,vertical,focused,
                            slider.class_name);
    }
    ui_draw_slider_label(slider.bounds,slider.label,slider.class_name);
}

void
ui_paint_slider_whole(SliderWholeProps slider, int vertical)
{
    if(!IsWindowReady() || slider.values == NULL || slider.value_count <= 0) return;
    slider.disabled |= UIContentDisabled();
    for(int i = 0; i < slider.value_count; i++) {
        Rectangle cell = {slider.bounds.x + slider.bounds.width*i/slider.value_count,
                          slider.bounds.y, slider.bounds.width/slider.value_count,
                          slider.bounds.height};
        float ratio = SliderWholeRatio(slider.values[i], slider.min,
                                     slider.max);
        char text[64];
        int focus_id = ui_numeric_focus_id(slider.id,i,1);
        UINumericInputState *state = ui_numeric_input_find(
            UI_NUMERIC_EDIT_SLIDER_INT, slider.id, i);
        if(state != NULL && state->focused)
            continue;
        int focused = !slider.disabled && focus_id > 0 &&
                      IsFocusActive(focus_id) &&
                      !ui_popup_input_focus_captures(focus_id);
        snprintf(text,sizeof(text),slider.format != NULL ? slider.format : "%d",slider.values[i]);
        ui_draw_slider_cell(cell,ratio,text,slider.disabled,vertical,focused,
                            slider.class_name);
    }
    ui_draw_slider_label(slider.bounds,slider.label,slider.class_name);
}

static int
ui_slider_scalar(SliderScalarProps slider, int vertical)
{
    int changed = ui_update_slider_scalar(slider,vertical);
    ui_paint_slider_scalar(slider,vertical);
    return changed;
}

int
ui_update_slider_angle(SliderAngleProps slider)
{
    const float radians_to_degrees = 57.295779513082320876f;
    const float degrees_to_radians = 0.01745329251994329577f;
    float degrees;
    SliderScalarProps value_slider;
    int changed;

    if(slider.value == NULL)
        return 0;
    degrees = *slider.value * radians_to_degrees;
    value_slider = (SliderScalarProps){slider.bounds, slider.id, slider.label,
                                      &degrees, 1, slider.min_degrees,
                                      slider.max_degrees, slider.format,
                                      slider.disabled, slider.class_name};
    changed = ui_update_slider_scalar(value_slider, 0);
    if(changed)
        *slider.value = degrees * degrees_to_radians;
    return changed;
}

void
ui_paint_slider_angle(SliderAngleProps slider)
{
    if(slider.value == NULL)
        return;
    float degrees = *slider.value * 57.295779513082320876f;
    SliderScalarProps value_slider = {
        slider.bounds, slider.id, slider.label, &degrees, 1,
        slider.min_degrees, slider.max_degrees, slider.format, slider.disabled
    };
    ui_paint_slider_scalar(value_slider, 0);
}

static UINumericInputState *
ui_numeric_input_find(int kind, int widget_id, int component)
{
    ToolkitStore *toolkit = toolkit_state();
    unsigned bucket = ((unsigned)widget_id * 31u + (unsigned)component * 17u +
                       (unsigned)kind) % UI_NUMERIC_INPUT_BUCKETS;
    for(UINumericInputState *state = toolkit->numeric_inputs[bucket];
        state != NULL; state = state->next)
        if(state->kind == kind && state->widget_id == widget_id &&
           state->component == component)
            return state;
    return NULL;
}

UINumericInputState *
ui_numeric_input_state(int kind, int widget_id, int component)
{
    ToolkitStore *toolkit = toolkit_state();
    unsigned bucket = ((unsigned)widget_id * 31u + (unsigned)component * 17u +
                       (unsigned)kind) % UI_NUMERIC_INPUT_BUCKETS;
    UINumericInputState *existing =
        ui_numeric_input_find(kind, widget_id, component);
    if(existing != NULL)
        return existing;

    UINumericInputState *state = calloc(1, sizeof(*state));
    if(state == NULL)
        abort();
    if(toolkit->numeric_next_token > INT_MAX - 3)
        abort();
    state->token = toolkit->numeric_next_token;
    toolkit->numeric_next_token += 3; /* field, decrement, increment */
    state->kind = kind;
    state->widget_id = widget_id;
    state->component = component;
    state->next = toolkit->numeric_inputs[bucket];
    toolkit->numeric_inputs[bucket] = state;
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

    if(values == NULL || count <= 0)
        return 0;
    BeginDisabled(disabled);
    for(int i = 0; i < count; i++) {
        UINumericInputState *state = ui_numeric_input_state(kind, id, i);
        int token = state->token;
        Rectangle cell = {bounds.x + bounds.width * i / count, bounds.y,
                          bounds.width / count, bounds.height};
        Rectangle field_bounds = cell;
        Rectangle minus = cell;
        Rectangle plus = cell;
        int commit = 0;
        double old_value = ui_numeric_value(values, i, kind);

        if(UIContentDisabled() && state->focused)
            while(GetCharPressed() != 0) {}

        if(!state->focused) {
            ui_numeric_format(state->text, sizeof(state->text), format, kind,
                              old_value);
            state->cursor = (int)strlen(state->text);
        }
        if(step != 0.0) {
            int button_w = Scale(24);
            field_bounds.width -= button_w * 2;
            minus.x = field_bounds.x + field_bounds.width;
            minus.width = button_w;
            plus.x = minus.x + minus.width;
            plus.width = button_w;
        }
        if(ui_text_field_render_filtered((TextFieldProps){
                .bounds = field_bounds,
                .text = state->text,
                .text_size = sizeof(state->text),
                .cursor_position = &state->cursor,
                .focused = &state->focused,
                .max_codepoints = 63,
                .font = 0,
                .focus_id = token,
                .style = kryon_zero_text_input_style,
                .commit_pressed = &commit,
                .secure = 0,
                .read_only = disabled
            }, ui_numeric_input_filter, NULL)) {
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
        if(step != 0.0) {
            int fast = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            int minus_pressed = Button((ButtonProps){.bounds = minus, .label = "-",
                .id = token + 1, .disabled = disabled});
            int plus_pressed = Button((ButtonProps){.bounds = plus, .label = "+",
                .id = token + 2, .disabled = disabled});
            if(minus_pressed || plus_pressed) {
                int direction = plus_pressed ? 1 : -1;
                double value = ui_numeric_value(values, i, kind);
                if(kind == 0) {
                    InputScalarStep result = InputScalarStepValue((float)value,
                        (float)step, (float)step_fast, direction, fast);
                    value = result.value;
                } else if(kind == 1) {
                    InputWholeStep result = InputWholeStepValue((int)value,
                        (int)step, (int)step_fast, direction, fast);
                    value = (double)result.value;
                } else {
                    InputDoubleStep result = InputDoubleStepValue(value, step,
                        step_fast, direction, fast);
                    value = result.value;
                }
                ui_numeric_set_value(values, i, kind, value);
                ui_numeric_format(state->text, sizeof(state->text), format,
                                  kind, value);
                state->cursor = (int)strlen(state->text);
                changed = 1;
            }
        }
        (void)commit;
    }
    ui_draw_slider_label(bounds, label, 0);
    EndDisabled();
    return changed;
}

int
RenderInputScalar(InputScalarProps input)
{
    return ui_numeric_input(input.bounds, input.id, input.label, input.values,
                            input.value_count, input.step, input.step_fast,
                            input.format, input.disabled, 0);
}

int
RenderInputWhole(InputWholeProps input)
{
    return ui_numeric_input(input.bounds, input.id, input.label, input.values,
                            input.value_count, input.step, input.step_fast,
                            input.format, input.disabled, 1);
}

int
RenderInputPrecise(InputPreciseProps input)
{
    return ui_numeric_input(input.bounds, input.id, input.label, input.values,
                            input.value_count, input.step, input.step_fast,
                            input.format, input.disabled, 2);
}

int
RenderSpinbox(SpinboxProps spinbox)
{
    SpinboxLayout layout = SpinboxLayoutFor(spinbox.bounds, Scale(28));
    int changed = 0;
    int disabled = spinbox.disabled || UIContentDisabled();
    char value_text[32];
    Rectangle left = layout.left;
    Rectangle right = layout.right;
    Rectangle text = layout.text;

    spinbox.step = SpinboxEffectiveStep(spinbox.step);

    if(disabled)
        MarkDisabled();
    if(spinbox.value_text != NULL)
        snprintf(value_text, sizeof(value_text), "%s", spinbox.value_text);
    else
        snprintf(value_text, sizeof(value_text), "%d", spinbox.value != NULL ? *spinbox.value : 0);
    if(IsWindowReady()) {
        StyleFrame frame = ui_tk_simple_style_frame(ButtonToneNeutral,
            disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
            StyleKindSpinbox());
        StyleFrame value_frame = ui_tk_simple_style_frame(ButtonToneNeutral,
            disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
            StyleKindSpinboxValue());
        Style value_style = ui_unpack_style(
            ui_style_apply_effects_frame(value_frame).value);
        int value_font = value_style.font_size > 0.0f
            ? (int)(value_style.font_size + 0.5f)
            : GetFontSize();
        ui_tk_draw_style_frame(spinbox.bounds, (Rectangle){0}, frame, 0, 0,
                               disabled, 0);
        ui_tk_draw_style_frame(text, spinbox.bounds, value_frame, 0, 0,
                               disabled, 0);
        DrawCenteredUIText(value_text, (int)(text.x + text.width / 2),
                           (int)(text.y + text.height / 2), value_font,
                           Fade(value_style.foreground, value_style.opacity));
    }
    if(ui_button_render((ButtonSpec){.props = {.bounds = left, .label = "-",
        .id = spinbox.id * 10 + 1, .disabled = disabled},
        .style_resolved = 1, .surface_bounds = spinbox.bounds,
        .style_kind = StyleKindButton()}) &&
       spinbox.value != NULL) {
        SpinboxStepResult step = SpinboxStepValue(
            *spinbox.value, spinbox.min, spinbox.max, spinbox.step, -1,
            spinbox.wrap != 0);

        *spinbox.value = step.value;
        changed |= step.changed;
    }
    if(ui_button_render((ButtonSpec){.props = {.bounds = right, .label = "+",
        .id = spinbox.id * 10 + 2, .disabled = disabled},
        .style_resolved = 1, .surface_bounds = spinbox.bounds,
        .style_kind = StyleKindButton()}) &&
       spinbox.value != NULL) {
        SpinboxStepResult step = SpinboxStepValue(
            *spinbox.value, spinbox.min, spinbox.max, spinbox.step, 1,
            spinbox.wrap != 0);

        *spinbox.value = step.value;
        changed |= step.changed;
    }
    return changed;
}

void
RenderFieldset(FieldsetProps frame)
{
    int font = GetSmallFontSize();
    const char *title = frame.title != NULL ? frame.title : "";
    StyleFrame style = ui_tk_simple_style_frame(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, StyleKindFieldset());
    if(style.value.font_size > 0.0f)
        font = (int)(style.value.font_size + 0.5f);
    int title_width = title[0] != '\0' ? TextWidth(title, font) : 0;
    FieldsetPaint paint = FieldsetPaintFor(
        frame.bounds, (float)title_width, title[0] != '\0',
        (float)Scale(1000) / 1000.0f, style);

    ui_tk_draw_style_frame(paint.frame, (Rectangle){0}, paint.face, 0, 0, 0, 0);
    if(paint.show_title) {
        DrawRectangleRec(paint.title_background,
                         GetColor(paint.background_color));
        RenderText(title, (int)paint.title_text.x, (int)paint.title_text.y,
                   font, GetColor(Opacity(paint.text_color,
                                          style.value.opacity)));
    }
}

int
RenderListBox(ListBoxProps list)
{
    int paint = IsWindowReady();
    int disabled = list.disabled || UIContentDisabled();
    StyleFrame default_item_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        list.class_name, StyleKindListBoxItem(), StyleAny());
    Style default_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_item_frame).value);
    int font = default_item_style.font_size > 0.0f
        ? (int)(default_item_style.font_size + 0.5f)
        : GetFontSize();
    int selected = list.selected_index != NULL ? *list.selected_index : -1;
    int row_h = ListBoxRowHeight(list.row_height > 0
        ? Scale(list.row_height) : Scale(30));
    ListBoxLayout layout;
    int scroll_y;
    int first;
    int visible;
    int max_scroll;
    int changed = 0;

    max_scroll = ui_update_scroll(list.bounds, list.item_count * row_h,
                                  disabled ? NULL : list.scroll_offset, row_h);
    layout = ListBoxLayoutFor(list.bounds, list.item_count, row_h, 0,
                              list.scroll_offset != NULL ? *list.scroll_offset : 0);
    layout.max_scroll = max_scroll;
    layout.scroll = ListBoxClampScroll(layout.scroll, max_scroll);
    int focused = !disabled && list.id > 0 &&
                  RegisterFocus(list.id, list.bounds);
    if(focused) SetFocusTextInputActive(0);
    if(focused && list.selected_index != NULL && list.item_count > 0 &&
       !ui_popup_input_focus_captures(list.id)) {
        int key = 0;
        if(IsKeyPressed(KEY_HOME)) key = 1;
        else if(IsKeyPressed(KEY_END)) key = 2;
        else if(IsKeyPressed(KEY_UP)) key = 3;
        else if(IsKeyPressed(KEY_DOWN)) key = 4;
        if(key != 0) {
            ListBoxNavigation nav = ListBoxNavigate(selected, list.item_count,
                                                    key, layout.scroll, row_h,
                                                    list.bounds.height,
                                                    max_scroll);
            if(nav.changed) {
                *list.selected_index = nav.selected;
                selected = nav.selected;
                changed = 1;
            }
            if(list.scroll_offset != NULL) {
                *list.scroll_offset = nav.scroll;
                layout.scroll = nav.scroll;
            }
        }
    }
    scroll_y = list.scroll_offset != NULL ? *list.scroll_offset : layout.scroll;
    layout = ListBoxLayoutFor(list.bounds, list.item_count, row_h, 0, scroll_y);
    first = layout.first_row;
    visible = layout.visible_rows;
    if(paint) {
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
            list.class_name, StyleKindListBox(), StyleAny());
        ui_tk_draw_style_frame(list.bounds, (Rectangle){0}, frame, 0, 0,
                               disabled, focused);
        BeginClip((int)list.bounds.x, (int)list.bounds.y,
                    (int)list.bounds.width, (int)list.bounds.height);
    }
    for(int i = 0; i <= visible && first + i < list.item_count; i++) {
        int index = first + i;
        Rectangle row = ListBoxRowBounds(list.bounds, i, layout);
        int hot = !disabled && ui_hot(row);
        ButtonState item_state = disabled ? ButtonStateDisabled :
            (hot ? ButtonStateHover :
             (index == selected ? ButtonStateSelected : ButtonStateNormal));
        StyleFrame item_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            item_state, disabled, index == selected, list.class_name,
            StyleKindListBoxItem(), StyleAny());
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        if(paint && (index == selected || hot || disabled))
            ui_tk_draw_style_frame(row, list.bounds, item_frame, hot, 0,
                                   disabled, 0);
        if(hot)
            MarkClickable();
        if(paint) {
            int item_font = item_style.font_size > 0.0f
                ? (int)(item_style.font_size + 0.5f)
                : font;
            int label_inset = item_style.padding_x > 0.0f
                ? (int)(item_style.padding_x + 0.5f)
                : Scale(8);
            RenderText(list.items != NULL && list.items[index] != NULL ? list.items[index] : "",
                       (int)row.x + label_inset,
                       ui_row_text_y(row, item_font), item_font,
                       Fade(item_style.foreground, item_style.opacity));
        }
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && list.selected_index != NULL) {
            ConsumeRelease();
            *list.selected_index = index;
            changed = 1;
        }
    }
    if(paint)
        EndClip();
    if(paint && list.scroll_offset != NULL && max_scroll > 0)
        ui_scrollbar((int)(list.bounds.x + list.bounds.width - Scale(8)),
                        (int)list.bounds.y, (int)list.bounds.height,
                        list.item_count * row_h, list.scroll_offset, max_scroll, 0);
    if(paint && focused)
        RenderFocus(list.bounds);
    return changed;
}

int
RenderTreeView(TreeViewProps tree)
{
    int paint = IsWindowReady();
    StyleFrame default_item_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        tree.disabled ? ButtonStateDisabled : ButtonStateNormal,
        tree.disabled, 0, tree.class_name, StyleKindTreeViewItem(), StyleAny());
    Style default_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_item_frame).value);
    int font = default_item_style.font_size > 0.0f
        ? (int)(default_item_style.font_size + 0.5f)
        : GetFontSize();
    TreeViewMetrics metrics = TreeViewMetricsFor((float)GetScale());
    int row_h = TreeViewRowHeight(tree.row_height, (float)GetScale(), metrics);
    int content_h = TreeViewContentHeight(tree.item_count, row_h);
    TreeViewScrollLayout scroll_layout;
    int scroll_y;
    int first;
    int y_offset;
    int visible = TreeViewVisibleRows((int)tree.bounds.height, row_h);
    int max_scroll;
    int changed = 0;

    max_scroll = ui_update_scroll(tree.bounds, content_h,
                                  tree.disabled ? NULL : tree.scroll_offset, row_h);
    scroll_y = tree.scroll_offset != NULL ? *tree.scroll_offset : 0;
    scroll_layout = TreeViewScrollFor(scroll_y, row_h);
    first = scroll_layout.first;
    y_offset = scroll_layout.y_offset;
    if(paint) {
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            tree.disabled ? ButtonStateDisabled : ButtonStateNormal,
            tree.disabled, 0, tree.class_name, StyleKindTreeView(), StyleAny());
        ui_tk_draw_style_frame(tree.bounds, (Rectangle){0}, frame, 0, 0,
                               tree.disabled, 0);
        BeginClip((int)tree.bounds.x, (int)tree.bounds.y,
                    (int)tree.bounds.width, (int)tree.bounds.height);
    }
    for(int i = 0; i < visible && first + i < tree.item_count; i++) {
        int index = first + i;
        const TreeItem *item = &tree.items[index];
        Rectangle row = TreeViewRowBounds(tree.bounds, i, row_h, y_offset);
        Rectangle marker_bounds = TreeViewMarkerBounds(row, item->depth, metrics);
        Rectangle text_bounds = TreeViewTextBounds(row, item->depth, metrics);
        int hot = !tree.disabled && ui_hot(row);
        int selected = tree.selected_id != NULL && *tree.selected_id == item->id;
        ButtonState item_state = tree.disabled ? ButtonStateDisabled :
            (hot ? ButtonStateHover :
             (selected ? ButtonStateSelected : ButtonStateNormal));
        StyleFrame item_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            item_state, tree.disabled, selected, tree.class_name,
            StyleKindTreeViewItem(), StyleAny());
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        int item_font = item_style.font_size > 0.0f
            ? (int)(item_style.font_size + 0.5f)
            : font;
        Color item_text = Fade(item_style.foreground, item_style.opacity);
        if(paint && (selected || hot || tree.disabled))
            ui_tk_draw_style_frame(row, tree.bounds, item_frame, hot, 0,
                                   tree.disabled, 0);
        if(paint) {
            if(item->expanded)
                RenderText("v", (int)marker_bounds.x,
                           ui_row_text_y(marker_bounds, item_font), item_font,
                           item_text);
            else
                RenderText(">", (int)marker_bounds.x,
                           ui_row_text_y(marker_bounds, item_font), item_font,
                           item_text);
            RenderText(item->label != NULL ? item->label : "",
                       (int)text_bounds.x, ui_row_text_y(text_bounds, item_font),
                       item_font, item_text);
        }
        if(hot)
            MarkClickable();
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && item->selectable && tree.selected_id != NULL) {
            ConsumeRelease();
            *tree.selected_id = item->id;
            changed = 1;
        }
    }
    if(paint)
        EndClip();
    if(paint && tree.scroll_offset != NULL && max_scroll > 0)
        ui_scrollbar((int)(tree.bounds.x + tree.bounds.width - Scale(8)),
                        (int)tree.bounds.y, (int)tree.bounds.height,
                        content_h, tree.scroll_offset, max_scroll, 0);
    return changed;
}

static int
ui_table_display_column(TableViewProps table, int slot)
{
    if(table.column_order == NULL) {
        if(slot < 0 || slot >= table.column_count)
            return -1;
        if(table.column_enabled != NULL && table.column_enabled[slot] == 0)
            return -1;
        return slot;
    }

    int ordinal = 0;
    for(int order_slot = 0; order_slot < table.column_count; order_slot++) {
        int column = table.column_order[order_slot];
        int duplicate = 0;
        if(column < 0 || column >= table.column_count)
            continue;
        for(int previous = 0; previous < order_slot; previous++)
            if(table.column_order[previous] == column)
                duplicate = 1;
        if(duplicate || (table.column_enabled != NULL && table.column_enabled[column] == 0))
            continue;
        if(ordinal++ == slot)
            return column;
    }
    for(int column = 0; column < table.column_count; column++) {
        int requested = 0;
        for(int order_slot = 0; order_slot < table.column_count; order_slot++)
            if(table.column_order[order_slot] == column)
                requested = 1;
        if(requested || (table.column_enabled != NULL && table.column_enabled[column] == 0))
            continue;
        if(ordinal++ == slot)
            return column;
    }
    return -1;
}

static int
ui_table_visible_columns(TableViewProps table)
{
    int count = 0;

    for(int slot = 0; slot < table.column_count; slot++)
        if(ui_table_display_column(table, slot) >= 0)
            count++;
    return count;
}

static int
ui_table_column_width(TableViewProps table, int column, int default_width)
{
    return table.column_widths != NULL && table.column_widths[column] > 0
        ? table.column_widths[column] : default_width;
}

static int
ui_table_column_x(TableViewProps table, int column, int default_width)
{
    int x = (int)table.bounds.x;

    for(int slot = 0; slot < table.column_count; slot++) {
        int current = ui_table_display_column(table, slot);
        if(current == column)
            return x;
        if(current >= 0)
            x += ui_table_column_width(table, current, default_width);
    }
    return x;
}

static int
ui_table_separator_at_x(TableViewProps table, int x, int tolerance,
                        int default_width, int *separator_x)
{
    int cursor = (int)table.bounds.x;

    for(int slot = 0; slot < table.column_count; slot++) {
        int column = ui_table_display_column(table, slot);
        if(column < 0)
            continue;
        cursor += ui_table_column_width(table, column, default_width);
        if(x >= cursor - tolerance && x <= cursor + tolerance) {
            if(separator_x != NULL)
                *separator_x = cursor;
            return column;
        }
    }
    return -1;
}

static float
ui_table_header_shift(TableViewProps table, float y)
{
    float angle = table.header_angle;
    if(!isfinite(angle) || angle == 0) return 0;
    angle = fmaxf(-89,fminf(89,angle));
    float height = Scale(table.header_height > 30 ? table.header_height : 30);
    return -(height-(y-table.bounds.y))/tanf(angle*3.14159265358979323846f/180);
}

static int
ui_table_mod_key_down(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
           IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

static const char *
ui_table_cell_text(TableViewProps table, int row, int column)
{
    if(table.rows == NULL || row < 0 || row >= table.row_count || column < 0 ||
       column >= table.column_count || table.rows[row].cells == NULL ||
       column >= table.rows[row].cell_count || table.rows[row].cells[column] == NULL)
        return "";
    return table.rows[row].cells[column];
}

static char *
ui_table_clipboard_text(TableViewProps table, int row, int column)
{
    size_t size = 1;
    int count = 0;

    if(table.copy_text != NULL) {
        size = strlen(table.copy_text)+1;
        char *copy = malloc(size);
        if(copy == NULL) return NULL;
        memcpy(copy,table.copy_text,size);
        return copy;
    }
    if(row >= 0 && row < table.row_count && column >= 0 && column < table.column_count)
        count = 1;
    else if(row >= 0 && row < table.row_count)
        count = table.column_count;
    else if(column >= 0 && column < table.column_count)
        count = table.row_count;
    if(count < 1) return NULL;

    for(int i = 0; i < count; i++) {
        const char *cell = ui_table_cell_text(table,
            row >= 0 ? row : i,column >= 0 ? column : i);
        size_t length = strlen(cell);
        if(length > SIZE_MAX-size-(i > 0 ? 1u : 0u)) return NULL;
        size += length+(i > 0 ? 1u : 0u);
    }
    char *copy = malloc(size);
    if(copy == NULL) return NULL;
    char *out = copy;
    for(int i = 0; i < count; i++) {
        const char *cell = ui_table_cell_text(table,
            row >= 0 ? row : i,column >= 0 ? column : i);
        size_t length = strlen(cell);
        if(i > 0) *out++ = row >= 0 ? '\t' : '\n';
        memcpy(out,cell,length);
        out += length;
    }
    *out = '\0';
    return copy;
}

static int
ui_table_handle_keys(TableViewProps table, int row_h, int header_h,
                     int frozen_rows, int max_scroll)
{
    int row, column, column_slot = 0, selection_changed = 0, changed = 0;
    int visible_columns = ui_table_visible_columns(table);

    if(table.disabled || table.selected_row == NULL || table.row_count < 1 ||
       visible_columns < 1 || table.id <= 0 ||
       !IsFocusActive(table.id) || !IsKeyboardInputEnabled() ||
       ui_popup_input_focus_captures(table.id))
        return 0;

    row = *table.selected_row >= 0
        ? (*table.selected_row < table.row_count ? *table.selected_row : table.row_count-1)
        : 0;
    column = ui_table_display_column(table,0);
    if(table.selected_column != NULL) {
        for(int slot = 0; slot < visible_columns; slot++) {
            int candidate = ui_table_display_column(table,slot);
            if(candidate == *table.selected_column) {
                column_slot = slot;
                column = candidate;
                break;
            }
        }
    }

    if(IsKeyPressed(KEY_UP)) {
        if(row > 0) row--;
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_DOWN)) {
        if(row < table.row_count-1) row++;
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_LEFT)) {
        if(column_slot > 0) column_slot--;
        column = ui_table_display_column(table,column_slot);
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_RIGHT)) {
        if(column_slot < visible_columns-1) column_slot++;
        column = ui_table_display_column(table,column_slot);
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_TAB)) {
        if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            if(column_slot > 0) column_slot--;
            else {
                column_slot = visible_columns-1;
                if(row > 0) row--;
            }
        } else if(column_slot < visible_columns-1) column_slot++;
        else {
            column_slot = 0;
            if(row < table.row_count-1) row++;
        }
        column = ui_table_display_column(table,column_slot);
        ui_consume_focus_tab();
        selection_changed = changed = 1;
    }
    if(ui_table_mod_key_down() &&
       (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_X))) {
        char *copy = ui_table_clipboard_text(table,*table.selected_row,
            table.selected_column != NULL ? *table.selected_column : -1);
        if(copy != NULL) {
            SetClipboardTextValue(copy);
            free(copy);
            changed = 1;
        }
    }
    if(ui_table_mod_key_down() && IsKeyPressed(KEY_V) &&
       table.pasted_text != NULL) {
        *table.pasted_text = GetClipboardTextValue();
        if(table.pasted_row != NULL) *table.pasted_row = *table.selected_row;
        if(table.pasted_column != NULL)
            *table.pasted_column = table.selected_column != NULL
                ? *table.selected_column : -1;
        changed = 1;
    }
    if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
       IsKeyPressed(KEY_F2)) {
        if(table.activated_row != NULL) *table.activated_row = row;
        if(table.activated_column != NULL) *table.activated_column = column;
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_ESCAPE) &&
       (*table.selected_row >= 0 ||
        (table.selected_column != NULL && *table.selected_column >= 0))) {
        row = -1;
        column = -1;
        selection_changed = changed = 1;
    }

    if(selection_changed) {
        *table.selected_row = row;
        if(table.selected_column != NULL) *table.selected_column = column;
        if(row >= frozen_rows && table.scroll_offset != NULL) {
            int view_h = (int)table.bounds.height-header_h-frozen_rows*row_h;
            int top = (row-frozen_rows)*row_h;
            int bottom = top+row_h;
            if(view_h > 0) {
                if(top < *table.scroll_offset) *table.scroll_offset = top;
                else if(bottom > *table.scroll_offset+view_h)
                    *table.scroll_offset = bottom-view_h;
                if(*table.scroll_offset < 0) *table.scroll_offset = 0;
                if(*table.scroll_offset > max_scroll) *table.scroll_offset = max_scroll;
            }
        }
    }
    return changed;
}

Rectangle
BeginTableCell(TableViewProps table, int row, int column)
{
    Rectangle cell = {0,0,0,0}, clip = {0,0,0,0};
    int visible = ui_table_visible_columns(table), found = 0;
    for(int slot = 0; slot < table.column_count; slot++)
        if(ui_table_display_column(table,slot) == column) found = 1;
    if(found && visible > 0 && row >= 0 && row < table.row_count) {
        TableViewMetrics metrics = TableViewMetricsFor((float)GetScale());
        TableViewLayout layout = TableViewLayoutFor(table.bounds, table.row_count,
            table.row_height, table.header_height, table.freeze_rows,
            (float)GetScale(), metrics);
        int row_h = layout.row_height;
        int header_h = layout.header_height;
        int frozen = layout.frozen_rows;
        int scroll = table.scroll_offset != NULL ? *table.scroll_offset : 0;
        int default_width = (int)table.bounds.width/visible;
        Rectangle row_bounds = {
            table.bounds.x,
            row < frozen
                ? table.bounds.y + header_h + row * row_h
                : table.bounds.y + header_h + frozen * row_h +
                    (row - frozen) * row_h - scroll,
            table.bounds.width,
            (float)row_h
        };
        cell = TableViewCellBounds(row_bounds,
            ui_table_column_x(table,column,default_width),
            ui_table_column_width(table,column,default_width));
        Rectangle viewport = TableViewViewport(table.bounds, layout,
            row >= frozen);
        clip = GetCollisionRec(cell,viewport);
    }
    BeginDisabled(table.disabled);
    (void)BeginScroll(clip,(int)clip.height,NULL);
    return cell;
}

void
EndTableCell(void)
{
    EndScroll();
    EndDisabled();
}

int
RenderTableView(TableViewProps table)
{
    ToolkitStore *toolkit = toolkit_state();
    int paint = IsWindowReady();
    TableViewMetrics metrics = TableViewMetricsFor((float)GetScale());
    TableViewLayout layout;
    TableViewScrollLayout scroll_layout;
    int row_h;
    int header_h;
    int default_col_w;
    int scroll_y;
    int first;
    int visible;
    int frozen_rows;
    int scroll_body_h;
    int max_scroll;
    int changed = 0;
    Style text_style = {0};
    Style selection_style = {0};
    Style divider_style = {0};
    Style header_style = {0};
    int cell_font;
    int header_font;

    table.disabled = table.disabled || UIContentDisabled();
    StyleFrame default_header_frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
        table.disabled ? ButtonStateDisabled : ButtonStateNormal,
        table.disabled, 0, StyleKindTableView(), 13);
    StyleFrame default_cell_frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
        table.disabled ? ButtonStateDisabled : ButtonStateNormal,
        table.disabled, 0, StyleKindTableView(), 22);
    header_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_header_frame).value);
    text_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_cell_frame).value);
    header_font = header_style.font_size > 0.0f
        ? (int)(header_style.font_size + 0.5f)
        : GetSmallFontSize();
    cell_font = text_style.font_size > 0.0f
        ? (int)(text_style.font_size + 0.5f)
        : header_font;

    if(toolkit->resize_column >= 0 &&
       ui_popup_input_owner_captures(toolkit->resize_owner)) {
        toolkit->resize_table_id = 0;
        toolkit->resize_column = -1;
    }

    if(toolkit->resize_column >= 0 &&
       (UIContentDisabled() || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       (UIContentDisabled() || toolkit->resize_table_id != table.id || table.disabled || !table.resizable ||
        table.column_widths == NULL)) {
        toolkit->resize_table_id = 0;
        toolkit->resize_column = -1;
    }
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
    if(table.pasted_text != NULL)
        *table.pasted_text = NULL;
    if(table.pasted_row != NULL)
        *table.pasted_row = -1;
    if(table.pasted_column != NULL)
        *table.pasted_column = -1;
    int visible_columns = ui_table_visible_columns(table);
    if(visible_columns < 1)
        return 0;
    layout = TableViewLayoutFor(table.bounds, table.row_count, table.row_height,
                                table.header_height, table.freeze_rows,
                                (float)GetScale(), metrics);
    row_h = layout.row_height;
    header_h = layout.header_height;
    frozen_rows = layout.frozen_rows;
    scroll_body_h = layout.scroll_body_height;
    default_col_w = TableViewDefaultColumnWidth((int)table.bounds.width,
                                                visible_columns);
    if(!UIContentDisabled() && !table.disabled && table.resizable && table.column_widths != NULL) {
        Vector2 mouse = ui_mouse_world();
        Rectangle header = {table.bounds.x, table.bounds.y,
                            table.bounds.width, (float)header_h};
        if(toolkit->resize_column < 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           ui_contains(header, mouse)) {
            int separator_x = 0;
            int column = ui_table_separator_at_x(table, (int)(mouse.x-ui_table_header_shift(table,mouse.y)),
                                                  Scale(5), default_col_w,
                                                  &separator_x);
            if(column >= 0) {
                toolkit->resize_table_id = table.id;
                toolkit->resize_column = column;
                toolkit->resize_start_x = (int)mouse.x;
                toolkit->resize_start_width = ui_table_column_width(table, column,
                                                           default_col_w);
                toolkit->resize_owner = ui_popup_input_owner();
                MarkClickable();
            }
        }
        if(toolkit->resize_column >= 0 && toolkit->resize_table_id == table.id) {
            int minimum = TableViewMinimumColumnWidth(table.min_column_width,
                                                      (float)GetScale(),
                                                      metrics);
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                int width = toolkit->resize_start_width + (int)mouse.x - toolkit->resize_start_x;
                if(width < minimum)
                    width = minimum;
                if(table.column_widths[toolkit->resize_column] != width) {
                    table.column_widths[toolkit->resize_column] = width;
                    changed = 1;
                }
                MarkClickable();
            }
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                ConsumeRelease();
                toolkit->resize_table_id = 0;
                toolkit->resize_column = -1;
            }
        }
    }
    max_scroll = ui_update_scroll(TableViewViewport(table.bounds, layout, true),
                                  (table.row_count - frozen_rows) * row_h,
                                  table.disabled ? NULL : table.scroll_offset, row_h);
    int focused = !table.disabled && table.id > 0 &&
                  RegisterFocus(table.id,table.bounds);
    if(focused) SetFocusTextInputActive(0);
    if(!table.custom_cells)
        changed |= ui_table_handle_keys(table,row_h,header_h,frozen_rows,max_scroll);
    scroll_y = table.scroll_offset != NULL ? *table.scroll_offset : 0;
    scroll_layout = TableViewScrollFor(scroll_y, frozen_rows, row_h,
                                       scroll_body_h);
    first = scroll_layout.first;
    visible = scroll_layout.visible_rows;
    if(paint) {
        StyleFrame surface_frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
            table.disabled ? ButtonStateDisabled : ButtonStateNormal,
            table.disabled, 0, StyleKindTableView(), 2);
        StyleFrame text_frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
            table.disabled ? ButtonStateDisabled : ButtonStateNormal,
            table.disabled, 0, StyleKindTableView(), 22);
        StyleFrame selection_frame = ui_tk_simple_style_frame_role(ButtonToneAccent,
            table.disabled ? ButtonStateDisabled : ButtonStateSelected,
            table.disabled, 1, StyleKindTableView(), 23);
        StyleFrame divider_frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
            table.disabled ? ButtonStateDisabled : ButtonStateNormal,
            table.disabled, 0, StyleKindTableView(), 18);
        text_style = ui_unpack_style(ui_style_apply_effects_frame(text_frame).value);
        selection_style = ui_unpack_style(ui_style_apply_effects_frame(selection_frame).value);
        divider_style = ui_unpack_style(ui_style_apply_effects_frame(divider_frame).value);
        if(text_style.font_size > 0.0f)
            cell_font = (int)(text_style.font_size + 0.5f);
        ui_tk_draw_style_frame(table.bounds, (Rectangle){0}, surface_frame, 0, 0,
                               table.disabled, focused);
    }

    for(int slot = 0; slot < table.column_count; slot++) {
        int c = ui_table_display_column(table, slot);
        if(c < 0)
            continue;
        int x = ui_table_column_x(table, c, default_col_w);
        int col_w = ui_table_column_width(table, c, default_col_w);
        Rectangle head = TableViewHeaderBounds(table.bounds, x, col_w, header_h);
        Vector2 header_mouse = ui_mouse_world();
        Vector2 local_mouse = {header_mouse.x-ui_table_header_shift(table,header_mouse.y),header_mouse.y};
        Rectangle all_headers = {table.bounds.x,table.bounds.y,table.bounds.width,(float)header_h};
        int header_hot = !table.disabled && ui_contains(all_headers,header_mouse) &&
            ui_contains(head,local_mouse) && !InputCapturesClick(header_mouse);
        if(paint) {
            int selected_header = (table.selected_column != NULL &&
                                   *table.selected_column == c) ||
                                  (table.sort_column != NULL &&
                                   *table.sort_column == c);
            ButtonState header_state = table.disabled ? ButtonStateDisabled :
                (header_hot ? ButtonStateHover :
                 (selected_header ? ButtonStateSelected : ButtonStateNormal));
            StyleFrame header_frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
                header_state, table.disabled, selected_header,
                StyleKindTableView(), 13);
            Style header_paint = ui_unpack_style(
                ui_style_apply_effects_frame(header_frame).value);
            Color header_color = header_paint.background;
            Color text_color = Fade(header_paint.foreground,
                                    header_paint.opacity);
            int render_header_font = header_paint.font_size > 0.0f
                ? (int)(header_paint.font_size + 0.5f)
                : header_font;
            float shift = ui_table_header_shift(table,head.y);
            if(shift != 0) {
                Vector2 a = {head.x+shift,head.y}, b = {head.x+head.width+shift,head.y};
                Vector2 c = {head.x+head.width,head.y+head.height}, d = {head.x,head.y+head.height};
                BeginClip((int)table.bounds.x,(int)table.bounds.y,(int)table.bounds.width,header_h);
                DrawTriangle(a,d,c,header_color); DrawTriangle(a,c,b,header_color);
                EndClip();
            } else {
                ui_tk_draw_style_frame(head, table.bounds, header_frame,
                                       header_hot, 0, table.disabled, 0);
            }
            const char *label = table.columns != NULL && table.columns[c] != NULL ? table.columns[c] : "";
            float angle = isfinite(table.header_angle) ? fmaxf(-89, fminf(89,table.header_angle)) : 0;
            if(angle != 0) {
                /* Scissor one raster row at a time to clip glyphs to the slanted
                   cell without a backend-specific stencil or offscreen target. */
                for(int row = 0; row < header_h; row++) {
                    float left = head.x + ui_table_header_shift(table,head.y+row+0.5f);
                    int x0 = (int)ceilf(fmaxf(table.bounds.x,left)-0.5f);
                    int x1 = (int)ceilf(fminf(table.bounds.x+table.bounds.width,left+head.width)-0.5f);
                    if(x1 <= x0) continue;
                    BeginClip(x0,(int)head.y+row,x1-x0,1);
                    DrawTextPro(GetTextFont(), label,
                            (Vector2){head.x + (angle > 0 ? shift : 0) + Scale(6), angle < 0 ? head.y + head.height - Scale(6) : head.y + Scale(6)},
                            (Vector2){0,0}, angle, render_header_font, 1, text_color);
                    EndClip();
                }
            } else RenderText(label, (int)head.x + Scale(6), ui_row_text_y(head, render_header_font), render_header_font, text_color);
            if(table.resizable && table.column_widths != NULL) {
                BeginClip((int)table.bounds.x,(int)head.y,(int)table.bounds.width,header_h);
                DrawLine((int)(head.x + head.width + shift) - 1, (int)head.y,
                         (int)(head.x + head.width) - 1,
                         (int)(head.y + head.height),
                         divider_style.border);
                EndClip();
            }
        }
        if(!table.disabled && ui_contains(all_headers,header_mouse) && ui_contains(head,local_mouse) &&
           !InputCapturesClick(header_mouse) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && table.sort_column != NULL) {
            int previous_sort_column = *table.sort_column;
            if(table.selected_row != NULL)
                *table.selected_row = -1;
            if(table.selected_column != NULL)
                *table.selected_column = c;
            *table.sort_column = c;
            if(table.sort_direction != NULL) {
                if(previous_sort_column != c || *table.sort_direction == 0)
                    *table.sort_direction = 1;
                else if(*table.sort_direction > 0)
                    *table.sort_direction = -1;
                else
                    *table.sort_direction = 0;
            }
            changed = 1;
        }
    }

    if(paint)
        BeginClip((int)table.bounds.x, (int)(table.bounds.y + header_h),
                    (int)table.bounds.width, (int)(table.bounds.height - header_h));
    for(int draw_index = 0; draw_index < frozen_rows + visible + 1; draw_index++) {
        int scrolling = draw_index >= frozen_rows;
        if(paint && draw_index == frozen_rows) {
            EndClip();
            BeginClip((int)table.bounds.x,
                        (int)table.bounds.y + header_h + frozen_rows * row_h,
                        (int)table.bounds.width, scroll_body_h);
        }
        int i = draw_index - frozen_rows;
        int r = scrolling ? first + i : draw_index;
        if(r < 0 || r >= table.row_count)
            continue;
        Rectangle row = TableViewRowBounds(table.bounds, layout, r,
                                           draw_index, scroll_layout,
                                           scrolling != 0);
        Rectangle viewport = TableViewViewport(table.bounds, layout,
                                               scrolling != 0);
        int hot = !table.disabled && !table.custom_cells && ui_contains(viewport, ui_mouse_world()) && ui_hot(row);
        if(paint && (r % 2) == 1) {
            StyleFrame row_frame = ui_tk_simple_style_frame_role(ButtonToneNeutral,
                table.disabled ? ButtonStateDisabled : ButtonStateNormal,
                table.disabled, 0, StyleKindTableView(), 21);
            ui_tk_draw_style_frame(row, table.bounds, row_frame, 0, 0,
                                   table.disabled, 0);
        }
        if(paint && ((table.selected_row != NULL && *table.selected_row == r) || hot)) {
            int selected = table.selected_row != NULL && *table.selected_row == r;
            ButtonState row_state = hot ? ButtonStateHover : ButtonStateSelected;
            StyleFrame row_frame = ui_tk_simple_style_frame_role(ButtonToneAccent,
                row_state, table.disabled, selected, StyleKindTableView(), 23);
            selection_style = ui_unpack_style(ui_style_apply_effects_frame(row_frame).value);
            ui_tk_draw_style_frame(row, table.bounds, row_frame, hot, 0,
                                   table.disabled, 0);
        }
        if(hot)
            MarkClickable();
        for(int slot = 0; slot < table.column_count; slot++) {
            int c = ui_table_display_column(table, slot);
            if(c < 0)
                continue;
            int x = ui_table_column_x(table, c, default_col_w);
            int col_w = ui_table_column_width(table, c, default_col_w);
            const char *text = "";
            if(table.rows != NULL && table.rows[r].cells != NULL && c < table.rows[r].cell_count)
                text = table.rows[r].cells[c] != NULL ? table.rows[r].cells[c] : "";
            if(paint) {
                if(table.rows != NULL && table.rows[r].background_colors != NULL &&
                   c < table.rows[r].cell_count && table.rows[r].background_colors[c].a != 0)
                    DrawRectangleRec((Rectangle){(float)x, row.y, (float)col_w, row.height},
                                     table.disabled
                                         ? DarkenColor(table.rows[r].background_colors[c], 38)
                                         : table.rows[r].background_colors[c]);
                BeginClip(x, (int)row.y, col_w, (int)row.height);
                Color text_color = text_style.foreground;
                float text_opacity = text_style.opacity;
                int render_font = cell_font;
                if(table.rows != NULL && table.rows[r].text_colors != NULL &&
                   c < table.rows[r].cell_count && table.rows[r].text_colors[c].a != 0)
                    text_color = table.rows[r].text_colors[c];
                else if((table.selected_row != NULL && *table.selected_row == r) || hot) {
                    text_color = selection_style.foreground;
                    text_opacity = selection_style.opacity;
                    if(selection_style.font_size > 0.0f)
                        render_font = (int)(selection_style.font_size + 0.5f);
                } else {
                    text_color = text_style.foreground;
                }
                RenderText(text, x + Scale(6), ui_row_text_y(row, render_font),
                           render_font, Fade(text_color, text_opacity));
                EndClip();
            }
        }
        if(!table.disabled && hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && table.selected_row != NULL) {
            int clicked_col = -1;
            double now = GetTime();
            Vector2 mouse = ui_mouse_world();
            ConsumeRelease();
            *table.selected_row = r;
            for(int slot = 0; slot < table.column_count; slot++) {
                int c = ui_table_display_column(table, slot);
                if(c < 0)
                    continue;
                int x = ui_table_column_x(table, c, default_col_w);
                int col_w = ui_table_column_width(table, c, default_col_w);
                if(mouse.x >= (float)x && mouse.x < (float)(x + col_w)) {
                    clicked_col = c;
                    break;
                }
            }
            if(clicked_col >= 0 && table.selected_column != NULL)
                *table.selected_column = clicked_col;
            if(clicked_col >= 0 && toolkit->last_table_id == table.id &&
               toolkit->last_table_row == r && toolkit->last_table_column == clicked_col &&
               now - toolkit->last_table_click_time <= 0.45) {
                if(table.activated_row != NULL)
                    *table.activated_row = r;
                if(table.activated_column != NULL)
                    *table.activated_column = clicked_col;
            }
            toolkit->last_table_id = table.id;
            toolkit->last_table_row = r;
            toolkit->last_table_column = clicked_col;
            toolkit->last_table_click_time = now;
            changed = 1;
        }
        if(!table.disabled && hot && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            int clicked_col = -1;
            Vector2 mouse = ui_mouse_world();
            for(int slot = 0; slot < table.column_count; slot++) {
                int c = ui_table_display_column(table, slot);
                if(c < 0)
                    continue;
                int x = ui_table_column_x(table, c, default_col_w);
                int col_w = ui_table_column_width(table, c, default_col_w);
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
        EndClip();
    if(paint && table.scroll_offset != NULL && max_scroll > 0)
        ui_scrollbar((int)(table.bounds.x + table.bounds.width - Scale(8)),
                        (int)(table.bounds.y + header_h + frozen_rows * row_h),
                        scroll_body_h,
                        (table.row_count - frozen_rows) * row_h,
                        table.scroll_offset, max_scroll, 0);
    if(paint && focused)
        RenderFocus(table.bounds);
    return changed;
}

Vector2
CanvasToScreen(Canvas canvas, Vector2 point)
{
    return CanvasPointToScreen(canvas.bounds, point,
                               canvas.scroll_x != NULL ? *canvas.scroll_x : 0,
                               canvas.scroll_y != NULL ? *canvas.scroll_y : 0,
                               canvas.zoom != NULL ? *canvas.zoom : 1.0f);
}

Rectangle
CanvasRectToScreen(Canvas canvas, Rectangle rect)
{
    return CanvasRectToScreenBounds(canvas.bounds, rect,
                                    canvas.scroll_x != NULL ? *canvas.scroll_x : 0,
                                    canvas.scroll_y != NULL ? *canvas.scroll_y : 0,
                                    canvas.zoom != NULL ? *canvas.zoom : 1.0f);
}

CanvasResult
BeginCanvas(Canvas canvas)
{
    ToolkitStore *toolkit = toolkit_state();
    CanvasResult result = {0};
    Vector2 mouse = ui_mouse_world();
    CanvasPolicyResult policy;

    ui_tk_draw_style_frame(canvas.bounds, canvas.bounds, ui_canvas_frame(),
                           0, 0, 0, 0);
    policy = CanvasBeginResultFor(canvas.bounds, mouse,
                                  canvas.scroll_x != NULL ? *canvas.scroll_x : 0,
                                  canvas.scroll_y != NULL ? *canvas.scroll_y : 0,
                                  canvas.zoom != NULL ? *canvas.zoom : 1.0f,
                                  IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    result.active = policy.active;
    result.dragging = policy.dragging;
    result.world = policy.world;
    BeginClip((int)canvas.bounds.x, (int)canvas.bounds.y,
                (int)canvas.bounds.width, (int)canvas.bounds.height);
    toolkit->canvas_depth++;
    if(canvas.scroll_x != NULL || canvas.scroll_y != NULL ||
       (canvas.zoom != NULL && *canvas.zoom > 0.01f && *canvas.zoom != 1.0f)) {
        Camera2D camera = {0};
        camera.target = (Vector2){canvas.bounds.x + (canvas.scroll_x != NULL ? (float)*canvas.scroll_x : 0.0f),
                                  canvas.bounds.y + (canvas.scroll_y != NULL ? (float)*canvas.scroll_y : 0.0f)};
        camera.offset = (Vector2){canvas.bounds.x, canvas.bounds.y};
        camera.rotation = 0.0f;
        camera.zoom = canvas.zoom != NULL && *canvas.zoom > 0.01f ? *canvas.zoom : 1.0f;
        BeginMode2D(camera);
        toolkit->canvas_mode_depth++;
    }
    return result;
}

void
EndCanvas(Canvas canvas)
{
    ToolkitStore *toolkit = toolkit_state();

    if(toolkit->canvas_depth > 0) {
        if(toolkit->canvas_mode_depth > 0) {
            toolkit->canvas_mode_depth--;
            EndMode2D();
        }
        toolkit->canvas_depth--;
        EndClip();
    }
    Style style = ui_unpack_style(
        ui_style_apply_effects_frame(ui_canvas_frame()).value);
    DrawRectangleLinesEx(canvas.bounds, 1.0f, style.border);
}

void
RenderCanvasGrid(Rectangle bounds, int step, Color color)
{
    int spacing = CanvasGridSpacing(Scale(step), 4);
    int vertical_count = CanvasGridLineCount(bounds.width, spacing);
    int horizontal_count = CanvasGridLineCount(bounds.height, spacing);
    uint32_t packed = ColorToInt(color);

    for(int i = 0; i < vertical_count; i++) {
        CanvasGridLine line = CanvasGridVerticalLine(bounds, i, spacing, packed);
        DrawLine((int)line.bounds.x, (int)line.bounds.y,
                 (int)line.bounds.x,
                 (int)(line.bounds.y + line.bounds.height),
                 GetColor(line.color));
    }
    for(int i = 0; i < horizontal_count; i++) {
        CanvasGridLine line = CanvasGridHorizontalLine(bounds, i, spacing, packed);
        DrawLine((int)line.bounds.x, (int)line.bounds.y,
                 (int)(line.bounds.x + line.bounds.width),
                 (int)line.bounds.y, GetColor(line.color));
    }
}

int
CanvasHitTest(Vector2 point, Rectangle *items, int item_count)
{
    if(items == NULL)
        return -1;
    for(int i = item_count - 1; i >= 0; i--) {
        int hit = CanvasHitTestStep(point, items[i], i, -1);
        if(hit >= 0)
            return hit;
    }
    return -1;
}

int
RenderPanedView(PanedViewProps panes)
{
    ToolkitStore *toolkit = toolkit_state();
    PanedViewMetrics metrics = PanedViewMetricsFor((float)GetScale());
    int changed = 0;
    int size = PanedViewSize(panes.bounds, panes.vertical != 0);
    int limit = PanedViewLimit(size, panes.min_first, panes.min_second);
    int split = panes.split != NULL ? *panes.split : limit / 2;
    split = PanedViewClampSplit(split, panes.min_first, limit);
    Rectangle handle = PanedViewHandleFor(panes.bounds, panes.vertical != 0,
                                         split, metrics);
    if(toolkit->active_split != NULL &&
       ui_popup_input_owner_captures(toolkit->active_split_owner))
        toolkit->active_split = NULL;
    if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        toolkit->active_split = NULL;
    if(UIContentDisabled() && toolkit->active_split == panes.split)
        toolkit->active_split = NULL;
    if(!UIContentDisabled() && ui_hot(handle)) {
        MarkClickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            toolkit->active_split = panes.split;
            toolkit->active_split_owner = ui_popup_input_owner();
        }
    }
    if(toolkit->active_split != NULL && toolkit->active_split == panes.split) {
        Vector2 mouse = ui_mouse_world();
        int next = PanedViewPointerSplit(panes.bounds, panes.vertical != 0,
                                         mouse.x, mouse.y);
        split = PanedViewClampSplit(next, panes.min_first, limit);
    }
    if(panes.split != NULL && *panes.split != split) {
        *panes.split = split;
        changed = 1;
    }
    handle = PanedViewHandleFor(panes.bounds, panes.vertical != 0,
                                split, metrics);
    if(IsWindowReady()) {
        StyleFrame frame = ui_tk_simple_style_frame_role(
            ButtonToneNeutral, ButtonStateNormal, 0, 0, StyleKindPanedView(),
            12);
        ui_tk_draw_style_frame(handle, (Rectangle){0}, frame, 0, 0, 0, 0);
    }
    return changed;
}

static void
ui_tree_header_register(CollapsibleProps section, int enabled)
{
    ToolkitStore *toolkit = toolkit_state();

    if(toolkit->tree_header_frame != g_ui_frame_serial) {
        UITreeHeaderNav *swap = toolkit->tree_previous;
        int capacity = toolkit->tree_previous_capacity;
        toolkit->tree_previous = toolkit->tree_headers;
        toolkit->tree_previous_count =
            toolkit->tree_header_frame + 1 == g_ui_frame_serial
                ? toolkit->tree_header_count : 0;
        toolkit->tree_previous_capacity = toolkit->tree_header_capacity;
        toolkit->tree_headers = swap;
        toolkit->tree_header_capacity = capacity;
        toolkit->tree_header_count = 0;
        toolkit->tree_header_frame = g_ui_frame_serial;
    }
    if(!enabled || !section.tree || section.id <= 0) return;
    if(toolkit->tree_header_count == toolkit->tree_header_capacity) {
        int capacity = toolkit->tree_header_capacity
            ? toolkit->tree_header_capacity * 2 : 32;
        UITreeHeaderNav *items = realloc(toolkit->tree_headers,
                                        sizeof(*items) * capacity);
        if(items == NULL) return;
        toolkit->tree_headers = items;
        toolkit->tree_header_capacity = capacity;
    }
    toolkit->tree_headers[toolkit->tree_header_count++] =
        (UITreeHeaderNav){section.id, section.depth > 0 ? section.depth : 0};
}

static int
ui_tree_header_target(int id, int key)
{
    ToolkitStore *toolkit = toolkit_state();

    for(int i = 0; i < toolkit->tree_previous_count; i++) {
        if(toolkit->tree_previous[i].id != id) continue;
        if(key == KEY_DOWN && i + 1 < toolkit->tree_previous_count)
            return toolkit->tree_previous[i+1].id;
        if(key == KEY_UP && i > 0)
            return toolkit->tree_previous[i-1].id;
        if(key == KEY_RIGHT && i + 1 < toolkit->tree_previous_count &&
           toolkit->tree_previous[i+1].depth >
               toolkit->tree_previous[i].depth)
            return toolkit->tree_previous[i+1].id;
        if(key == KEY_LEFT)
            for(int j = i - 1; j >= 0; j--)
                if(toolkit->tree_previous[j].depth <
                   toolkit->tree_previous[i].depth)
                    return toolkit->tree_previous[j].id;
        break;
    }
    return id;
}

int
RenderCollapsible(CollapsibleProps section)
{
    ToolkitStore *toolkit = toolkit_state();
    int enabled = !section.disabled && !UIContentDisabled();
    ButtonState default_state = !enabled ? ButtonStateDisabled
                              : section.selected ? ButtonStateSelected
                              : ButtonStateNormal;
    StyleFrame default_item_frame = ui_tk_simple_style_frame_role(
        ButtonToneNeutral, default_state, !enabled, section.selected,
        StyleKindCollapsible(), section.tree ? 14 : 13);
    Style default_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_item_frame).value);
    int font = default_item_style.font_size > 0.0f
        ? (int)(default_item_style.font_size + 0.5f)
        : GetFontSize();
    int changed = 0;
    CollapsibleMetrics metrics = CollapsibleMetricsFor((float)GetScale());
    CollapsibleLayout layout;
    Rectangle header;
    Rectangle body;
    Rectangle close_bounds;
    int close_hover = 0;
    int closed = 0;
    if(section.visible != NULL && !*section.visible) return 0;
    layout = CollapsibleLayoutFor(section.bounds, section.tree != 0,
                                  section.depth, section.visible != NULL,
                                  metrics);
    header = layout.header;
    body = layout.body;
    close_bounds = layout.close_bounds;
    ui_tree_header_register(section, enabled);
    int focused = enabled && section.id > 0 && RegisterFocus(section.id, header);
    if(focused) SetFocusTextInputActive(0);
    if(section.visible != NULL &&
       HandleClick(close_bounds, !enabled, &close_hover)) {
        *section.visible = false;
        changed = closed = 1;
    }
    if(enabled && !closed && ui_hot(body)) {
        MarkClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            ConsumeRelease();
            if(section.id > 0) SetFocus(section.id);
            if(!section.leaf && section.open != NULL) {
                *section.open = !*section.open;
                changed = 1;
            }
        }
    }
    if(focused && !ui_popup_input_focus_captures(section.id) &&
       toolkit->tree_key_frame != g_ui_frame_serial) {
        int key = IsKeyPressed(KEY_DOWN) ? KEY_DOWN : IsKeyPressed(KEY_UP) ? KEY_UP :
                  IsKeyPressed(KEY_RIGHT) ? KEY_RIGHT : IsKeyPressed(KEY_LEFT) ? KEY_LEFT : 0;
        int open = section.open != NULL && *section.open;
        if(section.tree && (key == KEY_DOWN || key == KEY_UP ||
           (key == KEY_RIGHT && open && !section.leaf) || (key == KEY_LEFT && (!open || section.leaf)))) {
            SetFocus(ui_tree_header_target(section.id,key));
            toolkit->tree_key_frame = g_ui_frame_serial;
        } else if(!section.leaf && section.open != NULL) {
            if(key == KEY_RIGHT) *section.open = true;
            if(key == KEY_LEFT) *section.open = false;
            if(IsFocusActivatePressed(section.id)) *section.open = !*section.open;
            changed |= open != *section.open;
            if(key != 0 || IsFocusActivatePressed(section.id))
                toolkit->tree_key_frame = g_ui_frame_serial;
        }
    }
    focused = enabled && section.id > 0 && IsFocusActive(section.id);
    if(IsWindowReady()) {
        ButtonState state = !enabled ? ButtonStateDisabled
                          : focused ? ButtonStateFocus
                          : section.selected ? ButtonStateSelected
                          : ButtonStateNormal;
        StyleFrame item_frame = ui_tk_simple_style_frame_role(
            ButtonToneNeutral, state, !enabled, section.selected,
            StyleKindCollapsible(), section.tree ? 14 : 13);
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        font = item_style.font_size > 0.0f
            ? (int)(item_style.font_size + 0.5f)
            : font;
        StyleFrame link_frame = ui_tk_simple_style_frame_role(
            ButtonToneNeutral, close_hover ? ButtonStateHover : ButtonStateNormal,
            !enabled, 0, StyleKindCollapsible(), 15);
        Style link_style = ui_unpack_style(
            ui_style_apply_effects_frame(link_frame).value);
        int close_font = link_style.font_size > 0.0f
            ? (int)(link_style.font_size + 0.5f)
            : font;
        Color text = Fade(item_style.foreground, item_style.opacity);
        Color icon = text;
        Color close_text = Fade(link_style.foreground, link_style.opacity);
        int marker = CollapsibleMarkerFor(section.open != NULL && *section.open,
                                          section.leaf != 0);
        if(!section.tree || section.selected)
            ui_tk_draw_style_frame(header, header, item_frame, 0, 0,
                                   !enabled, focused);
        if(!section.tree && item_style.border.a != 0)
            DrawRectangleLinesEx(header, 1.0f, item_style.border);
        RenderText(CollapsibleMarkerText(marker),
                   (int)header.x + metrics.icon_offset,
                   ui_row_text_y(header, font), font, icon);
        BeginClip((int)header.x + metrics.text_offset, (int)header.y,
                    (int)fmaxf(0.0f, body.width - (float)metrics.text_offset),
                    (int)header.height);
        RenderText(section.label != NULL ? section.label : "",
                   (int)header.x + metrics.text_offset,
                   ui_row_text_y(header, font), font, text);
        EndClip();
        if(section.visible != NULL)
            RenderText("x",
                       (int)(close_bounds.x +
                             (close_bounds.width - TextWidth("x", close_font)) * 0.5f),
                       ui_row_text_y(close_bounds, close_font), close_font,
                       close_text);
        if(focused) RenderFocus(header);
    }
    return changed;
}

int
AcceleratorPressed(Accelerator accelerator)
{
    if(!IsKeyboardInputEnabled())
        return 0;
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
RenderFocusDebugOverlay(const AccessibilityNode *nodes, int count)
{
    if(nodes == NULL)
        return;
    for(int i = 0; i < count; i++) {
        ButtonState state = nodes[i].focused ? ButtonStateFocus
                                             : ButtonStateNormal;
        Style box = ui_unpack_style(ui_control_style_frame_role_kind(
            (ButtonProps){0}, state, 0, 0.0f, 0.0f,
            nodes[i].focused ? 1.0f : 0.0f, StyleKindFocus(), 9).value);
        Style label = ui_unpack_style(ui_control_style_frame_role_kind(
            (ButtonProps){0}, state, 0, 0.0f, 0.0f,
            nodes[i].focused ? 1.0f : 0.0f, StyleKindFocus(), 6).value);
        int font = label.font_size > 0.0f
            ? (int)(label.font_size + 0.5f)
            : GetSmallFontSize();
        FocusDebugOverlayPaint paint =
            FocusDebugOverlayPaintFor(nodes[i].bounds, TextLineHeight(font),
                                      nodes[i].label != NULL);
        DrawRectangleLinesEx(paint.outline, (float)paint.stroke_width,
                             Fade(box.border, box.opacity));
        if(paint.label_visible)
            RenderText(nodes[i].label, (int)paint.label_position.x,
                       (int)paint.label_position.y, font,
                       Fade(label.foreground, label.opacity));
    }
}
