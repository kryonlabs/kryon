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
#include "runtime/popup_policy.h"
#include "runtime/progress.h"
#include "runtime/radio.h"
#include "runtime/scroll.h"
#include "runtime/selectable.h"
#include "runtime/separator.h"
#include "runtime/slider.h"
#include "runtime/spinbox.h"
#include "runtime/style.h"
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
    DrawCircle((int)paint.thumb_shadow_x, (int)paint.thumb_shadow_y,
               paint.thumb_shadow_radius,
               GetColor(paint.thumb_shadow_color));
    DrawCircle((int)paint.thumb_x, (int)paint.thumb_y,
               paint.thumb_radius, GetColor(paint.thumb_fill_color));
    DrawCircle((int)paint.thumb_highlight_x,
               (int)paint.thumb_highlight_y,
               paint.thumb_highlight_radius,
               GetColor(paint.thumb_highlight_color));
    DrawCircleLines((int)paint.thumb_x, (int)paint.thumb_y,
                    paint.thumb_radius, GetColor(paint.thumb_edge_color));
}


#define TK_MENU_MAX 8
#define TK_MENU_DEPTH_MAX 8
#define TK_CONTEXT_MENU_MAX_ITEMS 64
#define RADIO_ANIM_MAX 128
#define INSTANCE_BUCKETS 512
#define DRAG_DROP_DATA_MAX 1024
#define NUMERIC_INPUT_BUCKETS 128
typedef struct NumericClickState {
    int valid;
    int kind;
    int widget_id;
    int component;
    Vector2 position;
    double time;
} NumericClickState;
typedef struct DragDropState {
    int active;
    int source_id;
    char type[32];
    unsigned char data[DRAG_DROP_DATA_MAX];
    int data_size;
} DragDropState;
static int ui_slider_continuous(SliderContinuousProps slider, int vertical);
typedef struct MenuOverlayState {
    int active;
    int bar_id;
    int menu_id;
    int class_name;
    int x;
    int y;
    int item_count;
    MenuItem items[TK_CONTEXT_MENU_MAX_ITEMS];
} MenuOverlayState;

typedef struct ContextMenuOverlayState {
    int active;
    int id;
    int class_name;
    int x;
    int y;
    int item_count;
    int suppress_close;
    MenuItem items[TK_CONTEXT_MENU_MAX_ITEMS];
} ContextMenuOverlayState;

typedef struct MenuNavigationState {
    int focus_id;
    int top;
    int depth;
    int path[TK_MENU_DEPTH_MAX];
    int key_handled;
    unsigned long key_frame;
} MenuNavigationState;

typedef struct RadioAnimState {
    unsigned int key;
    float selected;
    float press;
    unsigned long frame_seen;
} RadioAnimState;

typedef struct TreeHeaderNavState {
    int id;
    int depth;
} TreeHeaderNavState;

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
    PopupInputOwner drag_owner;
    int slider_active;
    PopupInputOwner slider_owner;
    NumericClickState numeric_click;
    DragDropState drag_drop;
    int canvas_depth;
    int canvas_mode_depth;
    RadioAnimState radio_anim[RADIO_ANIM_MAX];
    InstanceEntry *instances[INSTANCE_BUCKETS];
    unsigned long instance_frame;
    int last_table_id;
    int last_table_row;
    int last_table_column;
    double last_table_click_time;
    int resize_table_id;
    int resize_column;
    int resize_start_x;
    int resize_start_width;
    PopupInputOwner resize_owner;
    int *active_split;
    PopupInputOwner active_split_owner;
    NumericInputState *numeric_inputs[NUMERIC_INPUT_BUCKETS];
    int numeric_next_token;
    TreeHeaderNavState *tree_headers;
    TreeHeaderNavState *tree_previous;
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
    MenuOverlayState overlay;
    ContextMenuOverlayState context_overlay;
    int pending_bar_id;
    int pending_activated;
    int pending_closed_bar_id;
    int context_open_id;
    int context_pending_id;
    int context_pending_activated;
    int context_pending_closed_id;
    MenuNavigationState navigation;
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
    for(int i = 0; i < INSTANCE_BUCKETS; i++) {
        InstanceEntry *entry = store->instances[i];
        while(entry != NULL) {
            InstanceEntry *next = entry->next;
            free(entry->value);
            free(entry);
            entry = next;
        }
    }
    for(int i = 0; i < NUMERIC_INPUT_BUCKETS; i++) {
        NumericInputState *state = store->numeric_inputs[i];

        while(state != NULL) {
            NumericInputState *next = state->next;

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
    for(int i = 0; i < INSTANCE_BUCKETS; i++) {
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
    unsigned int bucket = key % INSTANCE_BUCKETS;
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
    int enabled = !disabled && !ContentDisabled();
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

    return MenuBarOpenIndexFor(id, state->open_id, menu_count) >= 0;
}

static int
ui_row_text_y(Rectangle bounds, int font)
{
    return (int)bounds.y + ((int)bounds.height - TextLineHeight(font)) / 2;
}

static StyleFrame
ui_canvas_frame(int class_name)
{
    return ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, class_name, StyleKindCanvas(), StyleAny());
}

static void
ui_draw_menu_panel(Rectangle bounds, int class_name)
{
    StyleFrame frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenu(), MenuPopupRole());
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
        float scale = (float)Scale(1000) / 1000.0f;
        int step = ScrollRowWheelStepFor(row_h, scale);
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
        ButtonStateNormal, 0, 0, class_name, StyleKindSeparator(),
        SeparatorLineRole());
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
        separator.disabled, 0, separator.class_name, StyleKindSeparator(),
        SeparatorLabelRole());
    Style label_style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
    int font = ResolveFont(0, StyleFontValue(label_style.fields,
                                             label_style.font_size),
                           GetSmallFontSize());
    int text_width = TextWidth(label, font);
    int text_y = ui_row_text_y(separator.bounds, font);
    StyleFrame line_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        separator.disabled ? ButtonStateDisabled : ButtonStateNormal,
        separator.disabled, 0, separator.class_name, StyleKindSeparator(),
        SeparatorLineRole());
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
    int disabled = drag_drop.disabled || ContentDisabled();
    int hot;
    int matches;
    int valid;

    if(drag_drop.role == DragDropRoleSource) {
        DragDropSourceDecision decision;
        hot = ui_hot(drag_drop.bounds);
        decision = DragDropSourceDecisionFor(
            toolkit->drag_drop.active, toolkit->drag_drop.source_id,
            drag_drop.id, drag_drop.disabled, ContentDisabled(),
            drag_drop.type != NULL && drag_drop.type[0] != '\0',
            drag_drop.data_size, DRAG_DROP_DATA_MAX, drag_drop.data != NULL,
            hot != 0, IsMouseButtonPressed(MOUSE_BUTTON_LEFT) != 0,
            IsMouseButtonDown(MOUSE_BUTTON_LEFT) != 0,
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0);
        if(decision.clear_source)
            toolkit->drag_drop = (DragDropState){0};
        valid = decision.valid;
        if(!valid)
            return 0;
        if(hot)
            MarkClickable();
        if(decision.start_source) {
            toolkit->drag_drop = (DragDropState){0};
            toolkit->drag_drop.active = 1;
            toolkit->drag_drop.source_id = drag_drop.id;
            snprintf(toolkit->drag_drop.type, sizeof(toolkit->drag_drop.type),
                     "%s", drag_drop.type);
            toolkit->drag_drop.data_size = drag_drop.data_size;
            if(drag_drop.data_size > 0)
                memcpy(toolkit->drag_drop.data, drag_drop.data,
                       (size_t)drag_drop.data_size);
        }
        return decision.returns_active;
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
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            disabled ? ButtonStateDisabled :
            (hot ? ButtonStateHover : ButtonStateNormal),
            disabled, hot, drag_drop.class_name, StyleKindDragDropTarget(),
            StyleAny());
        ui_tk_draw_style_frame(drag_drop.bounds, (Rectangle){0}, frame, hot, 0,
                               disabled, 0);
    }
    {
        DragDropTargetDecision decision = DragDropTargetDecisionFor(
            drag_drop.disabled, ContentDisabled(), toolkit->drag_drop.active,
            drag_drop.type != NULL && drag_drop.type[0] != '\0',
            drag_drop.type != NULL &&
                strcmp(toolkit->drag_drop.type, drag_drop.type) == 0,
            hot != 0, IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0,
            toolkit->drag_drop.data_size, drag_drop.output_size);
        if(!decision.accepted)
            return 0;
        if(drag_drop.output != NULL && drag_drop.output_size > 0) {
            memcpy(drag_drop.output, toolkit->drag_drop.data,
                   (size_t)decision.copy_size);
            if(drag_drop.accepted_size != NULL)
                *drag_drop.accepted_size = decision.copy_size;
        }
        if(decision.clear_source)
            toolkit->drag_drop = (DragDropState){0};
        if(decision.consume_release)
            ConsumeRelease();
    }
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
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int paint = IsWindowReady();
    int clicked = -1;
    int disabled = list.disabled || ContentDisabled();
    StyleFrame default_item_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        list.class_name, StyleKindListBoxMultiItem(), StyleAny());
    int row_height = ListBoxMultiRowHeight(list.row_height, runtime_scale,
        default_item_frame);
    Style default_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_item_frame).value);
    int default_item_font = ResolveFont(
        0, StyleFontValue(default_item_style.fields,
                          default_item_style.font_size),
        GetSmallFontSize());
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
            int font = ResolveFont(
                0, StyleFontValue(item_style.fields, item_style.font_size),
                default_item_font);
            int label_inset = ListBoxMultiItemLabelInset(
                (float)Scale(1000) / 1000.0f, item_frame);
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
        ButtonStateNormal, 0, 0, StyleKindSeparator(),
        SeparatorBulletRole());
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
    int disabled = selectable.disabled || ContentDisabled();
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
    float label_inset = SelectableLabelInset(
        (float)Scale(1000) / 1000.0f, face);
    int font = ResolveFont(0, StyleFontValue(face.value.fields,
                                             face.value.font_size),
                           GetFontSize());
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
    int disabled = checkbox.disabled || ContentDisabled() ||
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
        StyleFrame label_frame = ui_tk_checkbox_style_frame(
            ButtonToneNeutral, state, disabled, checked, checkbox.class_name,
            CheckboxLabelRole());

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
                                              checkbox.class_name,
                                              CheckboxBoxRoleForTone(
                                                  ButtonToneNeutral)),
            .active = ui_tk_checkbox_style_frame(ButtonToneAccent, state,
                                                 disabled, checked,
                                                 checkbox.class_name,
                                                 CheckboxBoxRoleForTone(
                                                     ButtonToneAccent)),
            .label = label_frame
        });
        Style label_style = ui_unpack_style(ui_style_apply_effects_frame(
            label_frame).value);
        int label_font = ResolveFont(
            0, StyleFontValue(label_style.fields, label_style.font_size),
            GetFontSize());
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
        float label_x = CheckboxLabelXFor(paint.slot_bounds, runtime_scale,
                                          label_frame);
        float label_y = CheckboxLabelYFor(checkbox.bounds,
                                          (float)TextLineHeight(label_font));
        RenderText(checkbox.label != NULL ? checkbox.label : "",
                   (int)label_x, (int)label_y,
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
    return ui_slider_continuous((SliderContinuousProps){edit.bounds, edit.id,
                                               edit.label, edit.values, channels,
                                               0.0f, 1.0f,
                                               InputDefaultFormat(NumericFloat),
                                               edit.disabled, edit.class_name}, 0);
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
    StyleFrame picker_frame;
    const char *labels[4] = {"R", "G", "B", "A"};

    if(picker.values == NULL || picker.value_count < channels)
        return 0;
    picker_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        picker.disabled ? ButtonStateDisabled : ButtonStateNormal,
        picker.disabled, 0, picker.class_name, StyleKindColorPicker(),
        StyleAny());
    layout = ColorPickerLayoutFor(picker.bounds, channels, scale,
                                  picker_frame);
    for(int i = 0; i < channels; i++) {
        Rectangle row = ColorPickerChannelBounds(picker.bounds, i, channels,
                                                 scale, picker_frame);
        SliderContinuousProps channel = {
            row,
            picker.id * 8 + i + 1, labels[i], &picker.values[i], 1,
            0.0f, 1.0f, InputDefaultFormat(NumericFloat),
            picker.disabled, picker.class_name
        };
        changed |= ui_slider_continuous(channel, 0);
    }
    if(IsWindowReady()) {
        ColorPickerSwatchPaint paint;
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            picker.disabled ? ButtonStateDisabled : ButtonStateNormal,
            picker.disabled, 0, picker.class_name,
            StyleKindColorPickerSwatch(), StyleAny());
        Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
        int font = ResolveFont(0, StyleFontValue(style.fields,
                                                 style.font_size),
                               GetSmallFontSize());
        float label_inset = ColorPickerSwatchLabelInset(
            (float)Scale(1000) / 1000.0f, frame);
        paint = ColorPickerSwatchPaintFor(layout.swatch_bounds,
                                          label_inset,
                                          (float)TextLineHeight(font));
        DrawRectangleRec(paint.bounds, ui_float_color(picker.values, channels));
        ui_tk_draw_style_frame(paint.bounds, picker.bounds, frame, 0, 0,
                               picker.disabled, 0);
        if(picker.label != NULL)
            RenderText(picker.label, (int)paint.label_x, (int)paint.label_y,
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
    if(items == NULL || item_count <= 0)
        return -1;
    for(int i = 0; i < item_count; i++) {
        index = MenuWrappedItemIndex(index, direction, item_count);
        if(MenuItemSelectable((int)items[index].kind, items[index].disabled))
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
    for(int i = 0; i < TK_MENU_DEPTH_MAX; i++)
        state->navigation.path[i] = -1;
    state->navigation.path[0] = menu_first_item(items,item_count);
}

static int
draw_menu_items(int x, int y, const MenuItem *items, int item_count,
                int focus_id, int depth, int class_name)
{
    ToolkitStore *state = toolkit_state();
    StyleFrame base_item_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenuItem(), StyleAny());
    Style base_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(base_item_frame).value);
    StyleFrame panel_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenu(), MenuPopupRole());
    StyleFrame bar_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenu(), MenuBarRole());
    int font = ResolveFont(0, StyleFontValue(base_item_style.fields,
                                             base_item_style.font_size),
                           GetFontSize());
    MenuMetrics metrics = MenuMetricsFor((float)Scale(1000) / 1000.0f,
                                         panel_frame, base_item_frame,
                                         bar_frame);
    int w = metrics.panel_min_width;
    int activated = 0;
    Rectangle panel;
    Vector2 mouse;
    int can_draw = IsWindowReady();
    int keyboard;

    if(items == NULL || item_count <= 0)
        return 0;

    keyboard = !ContentDisabled() && focus_id > 0 && IsFocusActive(focus_id) &&
               IsKeyboardInputEnabled() &&
               !ui_popup_input_focus_captures(focus_id);
    if(keyboard && state->navigation.focus_id != focus_id)
        menu_navigation_reset(focus_id,items,item_count);
    if(keyboard && depth < TK_MENU_DEPTH_MAX) {
        int selected;
        if(state->navigation.path[depth] < 0 ||
           state->navigation.path[depth] >= item_count ||
           !MenuItemSelectable((int)items[state->navigation.path[depth]].kind,
                               items[state->navigation.path[depth]].disabled))
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
                int opens_submenu = MenuItemCanOpenSubmenu(
                    (int)selected_item->kind, selected_item->disabled,
                    selected_item->submenu != NULL,
                    selected_item->submenu_count, depth, TK_MENU_DEPTH_MAX);
                state->navigation.key_handled = 1;
                if(opens_submenu) {
                    state->submenu_id = selected_item->id;
                    state->navigation.depth = depth+1;
                    state->navigation.path[depth+1] =
                        menu_first_item(selected_item->submenu,
                                        selected_item->submenu_count);
                } else if(MenuItemKeyboardActivates((int)selected_item->kind,
                                                     selected_item->disabled,
                                                     opens_submenu)) {
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
        ui_draw_menu_panel(panel, class_name);
    ui_menu_track_panel(panel);
    PushInputCapture(panel, 1);
    mouse = ui_mouse_world();
    if(ui_contains(panel, mouse))
        MarkCursor(MOUSE_CURSOR_DEFAULT);

    for(int i = 0; i < item_count; i++) {
        Rectangle row = MenuRowBounds(panel, i, metrics);
        const MenuItem *item = &items[i];
        int row_hot = !ContentDisabled() && ui_contains(row, mouse) &&
                      !MenuItemIsSeparator((int)item->kind);
        int hot = row_hot && !item->disabled;
        int selected = keyboard && depth < TK_MENU_DEPTH_MAX &&
                       state->navigation.path[depth] == i;
        ButtonState item_state = item->disabled ? ButtonStateDisabled :
            ((hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ?
                 ButtonStatePressed :
             (hot ? ButtonStateHover :
              (selected ? ButtonStateSelected : ButtonStateNormal)));
        StyleFrame item_frame = ui_tk_simple_style_frame_class_role(
            ButtonToneNeutral, item_state, item->disabled, selected,
            class_name, StyleKindMenuItem(), StyleAny());
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        int item_font = ResolveFont(0, StyleFontValue(item_style.fields,
                                                      item_style.font_size),
                                    font);
        Color item_text = Fade(item_style.foreground, item_style.opacity);

        if(MenuItemIsSeparator((int)item->kind)) {
            if(can_draw) {
                StyleFrame separator_frame = ui_tk_simple_style_frame_class_role(
                    ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
                    StyleKindMenuSeparator(), StyleAny());
                Style separator_style = ui_unpack_style(
                    ui_style_apply_effects_frame(separator_frame).value);
                MenuLine line = MenuSeparatorLineFor(row, metrics);
                Line(line.x1, line.y1, line.x2, line.y2,
                     separator_style.border);
            }
            continue;
        }

        if(hot) {
            if(state->navigation.focus_id == focus_id &&
               depth < TK_MENU_DEPTH_MAX) {
                state->navigation.path[depth] = i;
                state->navigation.depth = depth;
                for(int child = depth+1; child < TK_MENU_DEPTH_MAX; child++)
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
            RenderText("*", MenuCheckedMarkX(row, metrics),
                       MenuTextY(row, TextLineHeight(item_font)),
                       item_font, item_text);
        if(can_draw)
            RenderText(item->label != NULL ? item->label : "",
                       MenuLabelX(row, metrics),
                       MenuTextY(row, TextLineHeight(item_font)),
                       item_font, item_text);
        if(can_draw && item->accelerator != NULL) {
            int accel_text_w = TextWidth(item->accelerator, item_font);
            RenderText(item->accelerator,
                       MenuAcceleratorX(row, accel_text_w, metrics),
                       MenuTextY(row, TextLineHeight(item_font)),
                       item_font, item_text);
        }
        if(can_draw && MenuItemShowsSubmenu((int)item->kind))
            RenderText(">", MenuSubmenuIndicatorX(row, metrics),
                       MenuTextY(row, TextLineHeight(item_font)),
                       item_font, item_text);
        if(hot && MenuItemShowsSubmenu((int)item->kind))
            state->submenu_id = item->id;
        if(MenuItemCanOpenSubmenu((int)item->kind, item->disabled,
                                  item->submenu != NULL, item->submenu_count,
                                  depth, TK_MENU_DEPTH_MAX) &&
           (keyboard
                ? selected && state->navigation.depth > depth
                : state->submenu_id == item->id)) {
            Vector2 origin = MenuSubmenuOrigin(row);
            int sub = draw_menu_items((int)origin.x, (int)origin.y,
                                      item->submenu, item->submenu_count,
                                      focus_id, depth+1, class_name);
            if(sub != 0)
                activated = sub;
        }
        if(hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            ConsumeRelease();
            if(state->navigation.focus_id != focus_id)
                menu_navigation_reset(focus_id,items,item_count);
            if(depth < TK_MENU_DEPTH_MAX) {
                state->navigation.path[depth] = i;
                state->navigation.depth = depth;
            }
            SetFocus(focus_id);
            if(MenuItemShowsSubmenu((int)item->kind))
                state->submenu_id = item->id;
            else if(MenuItemPointerActivates((int)item->kind,
                                             item->disabled)) {
                activated = item->id;
                state->open_id = 0;
            }
        }
    }

    return activated;
}

static Rectangle
menu_items_panel_bounds(int x, int y, const MenuItem *items, int item_count,
                        int class_name)
{
    StyleFrame frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenuItem(), StyleAny());
    Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
    StyleFrame panel_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenu(), MenuPopupRole());
    StyleFrame bar_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenu(), MenuBarRole());
    int font = ResolveFont(0, StyleFontValue(style.fields, style.font_size),
                           GetFontSize());
    MenuMetrics metrics = MenuMetricsFor((float)Scale(1000) / 1000.0f,
                                         panel_frame, frame, bar_frame);
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
       *used >= TK_CONTEXT_MENU_MAX_ITEMS)
        return -1;
    count = item_count;
    if(count > TK_CONTEXT_MENU_MAX_ITEMS-*used)
        count = TK_CONTEXT_MENU_MAX_ITEMS-*used;
    start = *used;
    *used += count;
    *copied_count = count;
    for(int i = 0; i < count; i++)
        arena[start+i] = items[i];
    for(int i = 0; i < count; i++) {
        MenuItem *copy = &arena[start+i];
        int child_count = 0;
        int child_start;
        if(!MenuItemShowsSubmenu((int)copy->kind) || copy->submenu == NULL ||
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
    state->context_overlay.class_name = menu.class_name;
    state->context_overlay.x = menu.x != NULL ? *menu.x : 0;
    state->context_overlay.y = menu.y != NULL ? *menu.y : 0;
    state->context_overlay.item_count = count;
    state->context_overlay.suppress_close = suppress_close;
}

MenuResult
RenderMenuGroups(int id, int class_name, Rectangle bounds, const MenuGroup *menus,
                 int menu_count, int *open_index)
{
    ToolkitStore *state = toolkit_state();
    MenuResult result = {0, -1};
    StyleFrame base_item_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenuItem(), StyleAny());
    Style base_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(base_item_frame).value);
    StyleFrame bar_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenu(), MenuBarRole());
    StyleFrame panel_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, class_name,
        StyleKindMenu(), MenuPopupRole());
    int font = ResolveFont(0, StyleFontValue(base_item_style.fields,
                                             base_item_style.font_size),
                           GetFontSize());
    MenuMetrics metrics = MenuMetricsFor((float)Scale(1000) / 1000.0f,
                                         panel_frame, base_item_frame,
                                         bar_frame);
    int x = MenuBarFirstItemX(bounds, metrics);
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
    menu_count = MenuBarCountFor(menu_count, TK_MENU_MAX);
    focused = !ContentDisabled() && id > 0 && RegisterFocus(id,bounds) &&
              !ui_popup_input_focus_captures(id);
    if(state->navigation.focus_id != id)
        menu_navigation_reset(id,NULL,0);
    state->navigation.top = MenuBarTopIndexFor(state->navigation.top,
                                               menu_count);
    menu_navigation_begin_frame();
    if(!skip_external_open && open_index != NULL && *open_index >= 0)
        state->open_id = MenuBarOpenIdFor(id, *open_index, menu_count);
    if(focused && menu_count > 0) {
        int current = MenuBarOpenIndexFor(id, state->open_id, menu_count);
        if(current < 0) {
            if(IsKeyPressed(KEY_LEFT))
                state->navigation.top =
                    MenuBarMoveTopIndex(state->navigation.top, menu_count, -1);
            else if(IsKeyPressed(KEY_RIGHT))
                state->navigation.top =
                    MenuBarMoveTopIndex(state->navigation.top, menu_count, 1);
            else if(IsKeyPressed(KEY_HOME)) state->navigation.top = 0;
            else if(IsKeyPressed(KEY_END)) state->navigation.top = menu_count-1;
            else if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
                    IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_DOWN)) {
                current = state->navigation.top;
                state->open_id = MenuBarOpenIdFor(id, current, menu_count);
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
            current = MenuBarMoveTopIndex(current, menu_count, -1);
            state->open_id = MenuBarOpenIdFor(id, current, menu_count);
            menu_navigation_reset(id,menus[current].items,
                                   menus[current].item_count);
            state->navigation.top = current;
            state->navigation.key_handled = 1;
        } else if(state->navigation.depth == 0 && IsKeyPressed(KEY_RIGHT)) {
            int selected = state->navigation.path[0];
            int opens_submenu = selected >= 0 &&
                selected < menus[current].item_count &&
                MenuItemCanOpenSubmenu(
                    (int)menus[current].items[selected].kind,
                    menus[current].items[selected].disabled,
                    menus[current].items[selected].submenu != NULL,
                    menus[current].items[selected].submenu_count,
                    state->navigation.depth, TK_MENU_DEPTH_MAX);
            if(!opens_submenu) {
                current = MenuBarMoveTopIndex(current, menu_count, 1);
                state->open_id = MenuBarOpenIdFor(id, current, menu_count);
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
        ui_tk_draw_style_frame(bounds, (Rectangle){0}, bar_frame, 0, 0, 0, 0);
    }

    for(int i = 0; i < menu_count; i++) {
        int w = MenuGroupItemWidth(TextWidth(menus[i].label != NULL ? menus[i].label : "", font), metrics);
        Rectangle item = MenuGroupItemBounds(x, bounds, w, metrics);
        int menu_id = MenuBarOpenIdFor(id, i, menu_count);
        int open = state->open_id == menu_id;
        int hot = !ContentDisabled() && ui_hot(item);
        ButtonState item_state = open ? ButtonStateSelected :
            (hot ? ButtonStateHover : ButtonStateNormal);
        StyleFrame item_frame = ui_tk_simple_style_frame_class_role(
            ButtonToneNeutral, item_state, 0, open, class_name,
            StyleKindMenuItem(), StyleAny());
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        int item_font = ResolveFont(0, StyleFontValue(item_style.fields,
                                                      item_style.font_size),
                                    font);
        Color item_text = Fade(item_style.foreground, item_style.opacity);
        if(can_draw && (hot || open))
            ui_tk_draw_style_frame(item, bounds, item_frame, hot, 0, 0,
                                   focused && state->navigation.top == i);
        if(hot)
            MarkClickable();
        if(can_draw)
            RenderText(menus[i].label != NULL ? menus[i].label : "",
                       MenuBarLabelX(item, metrics),
                       MenuBarLabelY(item, TextLineHeight(item_font)),
                       item_font, item_text);
        MenuGroupPointerDecision pointer_decision =
            MenuGroupPointerDecisionFor(menu_id, i, state->open_id, hot,
                                        IsMouseButtonReleased(
                                            MOUSE_BUTTON_LEFT));
        if(pointer_decision.consume_release)
            ConsumeRelease();
        if(pointer_decision.set_focus)
            SetFocus(id);
        if(pointer_decision.changed_open)
            state->open_id = pointer_decision.next_open_id;
        if(pointer_decision.clear_submenu)
            state->submenu_id = 0;
        if(pointer_decision.reset_navigation) {
            state->navigation.top = pointer_decision.navigation_top;
            menu_navigation_reset(id,menus[i].items,menus[i].item_count);
            state->navigation.top = pointer_decision.navigation_top;
        }
        open = state->open_id == menu_id;
        if(open) {
            Vector2 origin = PopupMenuBarOrigin(item, bounds);
            result.open_index = i;
            state->overlay.active = 1;
            state->overlay.bar_id = id;
            state->overlay.menu_id = id + i;
            state->overlay.class_name = class_name;
            state->overlay.x = (int)origin.x;
            state->overlay.y = (int)origin.y;
            int used = 0;
            copy_menu_items(state->overlay.items,&used,menus[i].items,
                            menus[i].item_count,&state->overlay.item_count);
        }
        x = MenuBarNextItemX(x, w, metrics);
    }
    MenuOutsideCloseDecision close_decision = MenuOutsideCloseDecisionFor(
        state->open_id, IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        ui_contains(bounds, mouse), state->panel_valid,
        ui_contains(state->panel_bounds, mouse));
    if(close_decision.close_open) {
        if(close_decision.consume_release)
            ConsumeRelease();
        state->open_id = 0;
        if(close_decision.clear_submenu)
            state->submenu_id = 0;
        result.open_index = close_decision.open_index;
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
                                    state->overlay.bar_id, 0,
                                    state->overlay.class_name);
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
                                    state->context_overlay.id, 0,
                                    state->context_overlay.class_name);
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
RenderPopupMenu(int id, int class_name, int x, int y, const MenuItem *items,
                int item_count)
{
    Rectangle panel = menu_items_panel_bounds(x,y,items,item_count, class_name);
    int focused = !ContentDisabled() && id > 0 && RegisterFocus(id,panel);
    if(focused && !ui_popup_input_focus_captures(id) &&
       IsKeyPressed(KEY_ESCAPE)) {
        menu_navigation_reset(0,NULL,0);
        SetFocus(0);
        return 0;
    }
    menu_navigation_begin_frame();
    return draw_menu_items(x,y,items,item_count,id,0, class_name);
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
    PopupContextActivation activation;

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
    activation = PopupContextActivationFor(PopupDecisionFor(PopupContext, 0),
                                           menu.trigger, mouse,
                                           ContentDisabled() != 0,
                                           InputCapturesClick(mouse) != 0,
                                           IsMouseButtonReleased(
                                               MOUSE_BUTTON_RIGHT) != 0);
    if(activation.open) {
        *menu.open = 1;
        *menu.x = (int)activation.origin.x;
        *menu.y = (int)activation.origin.y;
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
                                    menu.items, menu.item_count,
                                    menu.class_name);
    focused = !ContentDisabled() && menu.id > 0 && RegisterFocus(menu.id,panel) &&
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
        return RenderMenuGroups(menu.id, menu.class_name, menu.bounds, menu.menus,
                                menu.menu_count, menu.open_index);
    }
    if(menu.mode == MenuModePopup) {
        result.activated_id = RenderPopupMenu(menu.id, menu.class_name,
                                              (int)menu.bounds.x,
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
                                                     radio.class_name,
                                                     RadioLabelRole());
    Style label_style = ui_unpack_style(ui_style_apply_effects_frame(
        label_frame).value);
    int font = ResolveFont(0, StyleFontValue(label_style.fields,
                                             label_style.font_size),
                           GetFontSize());
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    RadioPaint paint = RadioPaintFor((RadioSpec){
        .bounds = radio.bounds,
        .checked = radio.checked,
        .disabled = radio.disabled,
        .selected_amount = radio.checked ? 1.0f : 0.0f,
        .scale = runtime_scale,
        .frame = ui_tk_radio_style_frame(ButtonToneNeutral,
                                         initial_state,
                                         radio.disabled, radio.checked,
                                         radio.class_name, RadioRingRole()),
        .selected = ui_tk_radio_style_frame(ButtonToneAccent,
                                            initial_state,
                                            radio.disabled, radio.checked,
                                            radio.class_name, RadioMarkRole())
    });
    paint.label_color = ColorToInt(label_style.foreground);
    int hot;
    int down;
    int focused = 0;
    int activated;

    activated = ui_focusable_pressed(paint.hit_bounds, radio.id, radio.disabled,
                                     &focused);
    hot = ui_hot(paint.hit_bounds) && !radio.disabled && !ContentDisabled();
    down = hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if(hot)
        MarkClickable();
    if(radio.disabled)
        MarkDisabled();
    if(!IsWindowReady())
        return activated ? radio.id : 0;
    if(ui_default_style()) {
        RadioAnimState *anim;
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
        anim = &toolkit->radio_anim[key % RADIO_ANIM_MAX];
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
            .selected_amount = selected,
            .scale = runtime_scale,
            .frame = ui_tk_radio_style_frame(ButtonToneNeutral,
                                             ui_tk_checkbox_button_state(hot,
                                                 down, focused,
                                                 radio.disabled),
                                             radio.disabled, radio.checked,
                                             radio.class_name, RadioRingRole()),
            .selected = ui_tk_radio_style_frame(ButtonToneAccent,
                                                ui_tk_checkbox_button_state(hot,
                                                    down, focused,
                                                    radio.disabled),
                                                radio.disabled, radio.checked,
                                                radio.class_name, RadioMarkRole())
        });
        label_frame = ui_tk_radio_style_frame(ButtonToneNeutral,
                                              ui_tk_checkbox_button_state(hot, down,
                                                                         focused,
                                                                         radio.disabled),
                                              radio.disabled, radio.checked,
                                              radio.class_name,
                                              RadioLabelRole());
        label_style = ui_unpack_style(ui_style_apply_effects_frame(
            label_frame).value);
        font = ResolveFont(0, StyleFontValue(label_style.fields,
                                             label_style.font_size), font);
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
        ButtonStateNormal, 0, 0, progress.class_name, StyleKindProgress(),
        ProgressLabelRole());
    Style text_style = ui_unpack_style(ui_style_apply_effects_frame(text).value);
    int font = ResolveFont(0, StyleFontValue(text_style.fields,
                                             text_style.font_size),
                           GetSmallFontSize());
    int label_w = label != NULL ? TextWidth(label, font) : 0;
    int label_h = label != NULL ? TextLineHeight(font) : 0;
    StyleFrame track = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, progress.class_name, StyleKindProgress(),
        ProgressTrackRole());
    StyleFrame active = ui_tk_simple_style_frame_class_role(ButtonToneAccent,
        ButtonStateNormal, 0, 1, progress.class_name, StyleKindProgress(),
        ProgressFillRole());

    paint = ProgressPaintFor(progress.bounds, progress.min, progress.max,
                             progress.value, (float)label_w, (float)label_h,
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
        int text_y = (int)paint.layout.label_y;
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
    plot_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral, ButtonStateNormal,
                                                0, 0, plot.class_name,
                                                StyleKindPlot(), StyleAny());
    mark_frame = ui_tk_simple_style_frame_class_role(ButtonToneAccent, ButtonStateSelected,
                                                0, 1, plot.class_name,
                                                StyleKindPlotMark(), StyleAny());
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
        int font = ResolveFont(0, StyleFontValue(plot_style.fields,
                                                 plot_style.font_size),
                               GetSmallFontSize());
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
static NumericInputState *ui_numeric_input_find(int kind, int widget_id,
                                                   int component);

enum {
    NUMERIC_EDIT_DRAG_FLOAT = 3,
    NUMERIC_EDIT_DRAG_INT,
    NUMERIC_EDIT_SLIDER_FLOAT,
    NUMERIC_EDIT_SLIDER_INT
};

static int
ui_numeric_temp_edit(Rectangle bounds, int kind, int widget_id, int component,
                     int focus_id, void *value, const char *format,
                     int disabled, int integer, int *editing)
{
    ToolkitStore *toolkit = toolkit_state();
    int enabled = !disabled && !ContentDisabled();
    int control = IsKeyDown(KEY_LEFT_CONTROL) ||
                  IsKeyDown(KEY_RIGHT_CONTROL);
    int pressed = enabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                  ui_hot(bounds);
    Vector2 mouse = ui_mouse_world();
    double now = GetTime();
    float scale = (float)Scale(1000) / 1000.0f;
    int slop = InputDoubleClickSlopFor(scale);
    float dx = mouse.x - toolkit->numeric_click.position.x;
    float dy = mouse.y - toolkit->numeric_click.position.y;
    int double_click = pressed && toolkit->numeric_click.valid &&
        toolkit->numeric_click.kind == kind &&
        toolkit->numeric_click.widget_id == widget_id &&
        toolkit->numeric_click.component == component &&
        now - toolkit->numeric_click.time <= 0.30 &&
        dx >= -slop && dx <= slop && dy >= -slop && dy <= slop;
    int activate = pressed && (control || double_click);
    NumericInputState *state =
        ui_numeric_input_find(kind, widget_id, component);
    int commit = 0;
    int changed = 0;

    if(pressed) {
        toolkit->numeric_click = (NumericClickState){
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
        const char *default_format = InputDefaultFormat(
            integer ? NumericInt : NumericFloat);
        if(integer)
            snprintf(state->text, sizeof(state->text),
                     format != NULL ? format : default_format, *(int *)value);
        else
            snprintf(state->text, sizeof(state->text),
                     format != NULL ? format : default_format,
                     *(float *)value);
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

    DisabledScope(!enabled);
    if(ui_text_field_render_filtered((TextFieldProps){
            .bounds = bounds,
            .text = state->text,
            .text_size = sizeof(state->text),
            .cursor_position = &state->cursor,
            .focused = &state->focused,
            .max_codepoints = 63,
            .focus_id = focus_id,
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
    DisabledEndScope();
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
    disabled = disabled || ContentDisabled();
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
ui_update_drag_continuous_keyboard(int focus_id, float speed, float minimum,
                              float maximum, float *value)
{
    int direction;
    DragStep step;

    if(focus_id <= 0 || !IsFocusActive(focus_id) ||
       !IsKeyboardInputEnabled() || ui_popup_input_focus_captures(focus_id))
        return 0;
    direction = ui_slider_keyboard_direction(0);
    step = DragKeyboardValue(*value, speed, minimum, maximum, direction,
        IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    if(!step.changed) return 0;
    *value = step.value;
    return 1;
}

static int
ui_update_drag_discrete_keyboard(int focus_id, float speed, int minimum,
                            int maximum, int *value)
{
    int direction;
    DragDiscreteStep step;

    if(focus_id <= 0 || !IsFocusActive(focus_id) ||
       !IsKeyboardInputEnabled() || ui_popup_input_focus_captures(focus_id))
        return 0;
    direction = ui_slider_keyboard_direction(0);
    step = DragDiscreteKeyboardValue(*value, speed, minimum, maximum, direction,
        IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    if(!step.changed) return 0;
    *value = step.value;
    return 1;
}

int
ui_update_drag_continuous(DragContinuousProps drag)
{
    int count = drag.value_count;
    int changed = 0;
    float speed = DragEffectiveSpeed(drag.speed);

    if(drag.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int focus_id = ui_numeric_focus_id(drag.id,i,0);
        Rectangle cell = DragCellBoundsFor(drag.bounds, count, i);
        float delta;
        int enabled = !drag.disabled && !ContentDisabled();
        int editing = 0;
        if(enabled && focus_id > 0) RegisterFocus(focus_id,cell);
        changed |= ui_numeric_temp_edit(cell, NUMERIC_EDIT_DRAG_FLOAT,
            drag.id, i, focus_id, &drag.values[i], drag.format,
            drag.disabled, 0, &editing);
        if(editing)
            continue;
        if(enabled && ui_update_drag_continuous_keyboard(focus_id,speed,
                drag.min,drag.max,&drag.values[i]))
            changed = 1;
        if(ui_drag_delta((int)(((unsigned int)drag.id << 4) ^
                               (unsigned int)(i + 1)), focus_id, cell,
                         drag.disabled, &delta)) {
            DragStep step = DragDeltaValue(drag.values[i], delta,
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
ui_update_drag_discrete(DragDiscreteProps drag)
{
    int count = drag.value_count;
    int changed = 0;
    float speed = DragEffectiveSpeed(drag.speed);

    if(drag.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int focus_id = ui_numeric_focus_id(drag.id,i,1);
        Rectangle cell = DragCellBoundsFor(drag.bounds, count, i);
        float delta;
        int enabled = !drag.disabled && !ContentDisabled();
        int editing = 0;
        if(enabled && focus_id > 0) RegisterFocus(focus_id,cell);
        changed |= ui_numeric_temp_edit(cell, NUMERIC_EDIT_DRAG_INT,
            drag.id, i, focus_id, &drag.values[i], drag.format,
            drag.disabled, 1, &editing);
        if(editing)
            continue;
        if(enabled && ui_update_drag_discrete_keyboard(focus_id,speed,
                drag.min,drag.max,&drag.values[i]))
            changed = 1;
        if(ui_drag_delta((int)(((unsigned int)drag.id << 4) ^
                               (unsigned int)(i + 1)), focus_id, cell,
                         drag.disabled, &delta)) {
            DragDiscreteStep step = DragDiscreteDeltaValue(drag.values[i], delta,
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
ui_paint_drag_cell(Rectangle bounds, const char *text, int disabled,
                   int focused, int class_name)
{
    ButtonState state = disabled ? ButtonStateDisabled : ButtonStateNormal;
    StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral, state,
                                                      disabled, 0, class_name,
                                                      StyleKindDragValue(), StyleAny());
    Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
    int font = ResolveFont(0, StyleFontValue(style.fields, style.font_size),
                           GetSmallFontSize());
    float scale = (float)Scale(1000) / 1000.0f;
    DragTextPaint paint = DragCellTextPaintFor(bounds, DragTextInsetFor(scale),
                                               (float)TextLineHeight(font));

    ui_tk_draw_style_frame(bounds, bounds, frame, 0, 0, disabled, focused);
    RenderText(text, (int)paint.text_x, (int)paint.text_y,
               font, Fade(style.foreground, style.opacity));
}

static void
ui_paint_drag_label(Rectangle bounds, const char *label, int class_name)
{
    if(label != NULL) {
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
                                                          ButtonStateNormal, 0, 0,
                                                          class_name,
                                                          StyleKindDrag(),
                                                          StyleAny());
        Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
        int font = ResolveFont(0, StyleFontValue(style.fields,
                                                 style.font_size),
                               GetSmallFontSize());
        float scale = (float)Scale(1000) / 1000.0f;
        DragTextPaint paint = DragLabelTextPaintFor(bounds,
                                                    DragTextInsetFor(scale),
                                                    font,
                                                    DragLabelGapFor(scale));
        RenderText(label, (int)paint.text_x, (int)paint.text_y,
                   font, Fade(style.foreground, style.opacity));
    }
}

void
ui_paint_drag_continuous(DragContinuousProps drag)
{
    if(!IsWindowReady() || drag.values == NULL || drag.value_count <= 0)
        return;
    for(int i = 0; i < drag.value_count; i++) {
        Rectangle cell = DragCellBoundsFor(drag.bounds, drag.value_count, i);
        char text[64];
        int focus_id = ui_numeric_focus_id(drag.id,i,0);
        NumericInputState *state = ui_numeric_input_find(
            NUMERIC_EDIT_DRAG_FLOAT, drag.id, i);
        int disabled = drag.disabled || ContentDisabled();
        if(state != NULL && state->focused)
            continue;
        int focused = !disabled && focus_id > 0 && IsFocusActive(focus_id) &&
                      !ui_popup_input_focus_captures(focus_id);
        snprintf(text, sizeof(text),
                 drag.format != NULL ? drag.format :
                    InputDefaultFormat(NumericFloat),
                 drag.values[i]);
        ui_paint_drag_cell(cell,text,disabled,focused,drag.class_name);
    }
    ui_paint_drag_label(drag.bounds, drag.label, drag.class_name);
}

void
ui_paint_drag_discrete(DragDiscreteProps drag)
{
    if(!IsWindowReady() || drag.values == NULL || drag.value_count <= 0)
        return;
    for(int i = 0; i < drag.value_count; i++) {
        Rectangle cell = DragCellBoundsFor(drag.bounds, drag.value_count, i);
        char text[64];
        int focus_id = ui_numeric_focus_id(drag.id,i,1);
        NumericInputState *state = ui_numeric_input_find(
            NUMERIC_EDIT_DRAG_INT, drag.id, i);
        int disabled = drag.disabled || ContentDisabled();
        if(state != NULL && state->focused)
            continue;
        int focused = !disabled && focus_id > 0 && IsFocusActive(focus_id) &&
                      !ui_popup_input_focus_captures(focus_id);
        snprintf(text, sizeof(text),
                 drag.format != NULL ? drag.format :
                    InputDefaultFormat(NumericInt),
                 drag.values[i]);
        ui_paint_drag_cell(cell,text,disabled,focused,drag.class_name);
    }
    ui_paint_drag_label(drag.bounds, drag.label, drag.class_name);
}

static int
ui_slider_ratio(int token, int focus_id, Rectangle bounds, int disabled,
                int vertical, float *ratio)
{
    ToolkitStore *toolkit = toolkit_state();
    Vector2 mouse = ui_mouse_world();
    disabled = disabled || ContentDisabled();
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
ui_update_slider_continuous_keyboard(int focus_id, int vertical, float minimum,
                                float maximum, float *value)
{
    int direction;
    SliderStep step;

    if(focus_id <= 0 || !IsFocusActive(focus_id) ||
       !IsKeyboardInputEnabled() || ui_popup_input_focus_captures(focus_id))
        return 0;
    direction = ui_slider_keyboard_direction(vertical);
    step = SliderKeyboardValue(*value, minimum, maximum, direction,
        IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    if(!step.changed)
        return 0;
    *value = step.value;
    return 1;
}

static int
ui_update_slider_discrete_keyboard(int focus_id, int vertical, int minimum,
                              int maximum, int *value)
{
    int direction;
    SliderDiscreteStep step;

    if(focus_id <= 0 || !IsFocusActive(focus_id) ||
       !IsKeyboardInputEnabled() || ui_popup_input_focus_captures(focus_id))
        return 0;
    direction = ui_slider_keyboard_direction(vertical);
    step = SliderDiscreteKeyboardValue(*value, minimum, maximum, direction,
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
        state, disabled, 0, class_name, StyleKindSlider(),
        SliderTrackRole());
    StyleFrame active = ui_tk_simple_style_frame_class_role(ButtonToneAccent,
        state, disabled, 1, class_name, StyleKindSlider(),
        SliderFillRole());
    StyleFrame thumb = ui_tk_simple_style_frame_class_role(ButtonToneAccent,
        state, disabled, 1, class_name, StyleKindSliderThumb(), StyleAny());
    StyleFrame label = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        state, disabled, 0, class_name, StyleKindSlider(),
        SliderLabelRole());
    Style label_style = ui_unpack_style(ui_style_apply_effects_frame(label).value);
    int label_font = ResolveFont(0, StyleFontValue(label_style.fields,
                                                   label_style.font_size),
                                 GetSmallFontSize());
    float scale = (float)Scale(1000) / 1000.0f;
    SliderTextPaint text_paint = SliderCellTextPaintFor(
        cell, SliderLabelInsetForStyle(label, scale),
        (float)TextLineHeight(label_font));

    ui_tk_draw_slider_paint(SliderPaintFor((SliderSpec){
        .bounds = cell,
        .ratio = ratio,
        .vertical = vertical != 0,
        .active = focused,
        .hovered = hovered,
        .disabled = disabled,
        .scale = scale,
        .track = track,
        .active_track = active,
        .thumb = thumb
    }), hovered, focused, disabled);
    RenderText(text, (int)text_paint.text_x, (int)text_paint.text_y,
               label_font, Fade(label_style.foreground, label_style.opacity));
    if(focused)
        RenderFocus(cell);
}

static void
ui_draw_slider_label(Rectangle bounds, const char *label, int class_name)
{
    if(IsWindowReady() && label != NULL) {
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            ButtonStateNormal, 0, 0, class_name, StyleKindSlider(),
            SliderLabelRole());
        Style style = ui_unpack_style(ui_style_apply_effects_frame(frame).value);
        int font = ResolveFont(0, StyleFontValue(style.fields,
                                                 style.font_size),
                               GetSmallFontSize());
        float scale = (float)Scale(1000) / 1000.0f;
        SliderTextPaint paint = SliderLabelTextPaintFor(
            bounds, SliderLabelInsetForStyle(frame, scale), font,
            SliderLabelGapForStyle(frame, scale));
        RenderText(label, (int)paint.text_x, (int)paint.text_y,
                   font, Fade(style.foreground, style.opacity));
    }
}

int
ui_update_slider_continuous(SliderContinuousProps slider, int vertical)
{
    int changed = 0;
    int count = slider.value_count;

    if(slider.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int focus_id = ui_numeric_focus_id(slider.id,i,0);
        Rectangle cell = SliderCellBoundsFor(slider.bounds, count, i);
        float ratio = SliderRatio(slider.values[i], slider.min,
                                       slider.max);
        int enabled = !slider.disabled && !ContentDisabled();
        int editing = 0;
        if(enabled && focus_id > 0)
            RegisterFocus(focus_id,cell);
        changed |= ui_numeric_temp_edit(cell, NUMERIC_EDIT_SLIDER_FLOAT,
            slider.id, i, focus_id, &slider.values[i], slider.format,
            slider.disabled, 0, &editing);
        if(editing)
            continue;
        if(enabled && ui_update_slider_continuous_keyboard(focus_id,vertical,
                    slider.min,slider.max,&slider.values[i])) {
            ratio = SliderRatio(slider.values[i], slider.min,
                                     slider.max);
            changed = 1;
        }
        if(slider.max > slider.min && ui_slider_ratio((int)(0x40000000u ^
                                           ((unsigned int)slider.id << 4) ^
                                           (unsigned int)(i + 1)),
                                           focus_id, cell, slider.disabled,
                                           vertical, &ratio)) {
            float value = SliderValue(slider.min, slider.max, ratio);
            if(value != slider.values[i]) {
                slider.values[i] = value;
                changed = 1;
            }
        }
    }
    return changed;
}

int
ui_update_slider_discrete(SliderDiscreteProps slider, int vertical)
{
    int changed = 0;
    int count = slider.value_count;

    if(slider.values == NULL || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        int focus_id = ui_numeric_focus_id(slider.id,i,1);
        Rectangle cell = SliderCellBoundsFor(slider.bounds, count, i);
        float ratio = SliderDiscreteRatio(slider.values[i], slider.min,
                                     slider.max);
        int enabled = !slider.disabled && !ContentDisabled();
        int editing = 0;
        if(enabled && focus_id > 0)
            RegisterFocus(focus_id,cell);
        changed |= ui_numeric_temp_edit(cell, NUMERIC_EDIT_SLIDER_INT,
            slider.id, i, focus_id, &slider.values[i], slider.format,
            slider.disabled, 1, &editing);
        if(editing)
            continue;
        if(enabled && ui_update_slider_discrete_keyboard(focus_id,vertical,
                    slider.min,slider.max,&slider.values[i])) {
            ratio = SliderDiscreteRatio(slider.values[i], slider.min,
                                   slider.max);
            changed = 1;
        }
        if(slider.max > slider.min && ui_slider_ratio((int)(0x50000000u ^
                                        ((unsigned int)slider.id << 4) ^
                                        (unsigned int)(i + 1)),
                                        focus_id, cell, slider.disabled,
                                        vertical, &ratio)) {
            int value = SliderDiscreteValue(slider.min, slider.max, ratio);
            if(value != slider.values[i]) {
                slider.values[i] = value;
                changed = 1;
            }
        }
    }
    return changed;
}

void
ui_paint_slider_continuous(SliderContinuousProps slider, int vertical)
{
    if(!IsWindowReady() || slider.values == NULL || slider.value_count <= 0) return;
    slider.disabled |= ContentDisabled();
    for(int i = 0; i < slider.value_count; i++) {
        Rectangle cell = SliderCellBoundsFor(slider.bounds, slider.value_count, i);
        float ratio = SliderRatio(slider.values[i], slider.min,
                                       slider.max);
        char text[64];
        int focus_id = ui_numeric_focus_id(slider.id,i,0);
        NumericInputState *state = ui_numeric_input_find(
            NUMERIC_EDIT_SLIDER_FLOAT, slider.id, i);
        if(state != NULL && state->focused)
            continue;
        int focused = !slider.disabled && focus_id > 0 &&
                      IsFocusActive(focus_id) &&
                      !ui_popup_input_focus_captures(focus_id);
        snprintf(text, sizeof(text),
                 slider.format != NULL ? slider.format :
                    InputDefaultFormat(NumericFloat),
                 slider.values[i]);
        ui_draw_slider_cell(cell,ratio,text,slider.disabled,vertical,focused,
                            slider.class_name);
    }
    ui_draw_slider_label(slider.bounds,slider.label,slider.class_name);
}

void
ui_paint_slider_discrete(SliderDiscreteProps slider, int vertical)
{
    if(!IsWindowReady() || slider.values == NULL || slider.value_count <= 0) return;
    slider.disabled |= ContentDisabled();
    for(int i = 0; i < slider.value_count; i++) {
        Rectangle cell = SliderCellBoundsFor(slider.bounds, slider.value_count, i);
        float ratio = SliderDiscreteRatio(slider.values[i], slider.min,
                                     slider.max);
        char text[64];
        int focus_id = ui_numeric_focus_id(slider.id,i,1);
        NumericInputState *state = ui_numeric_input_find(
            NUMERIC_EDIT_SLIDER_INT, slider.id, i);
        if(state != NULL && state->focused)
            continue;
        int focused = !slider.disabled && focus_id > 0 &&
                      IsFocusActive(focus_id) &&
                      !ui_popup_input_focus_captures(focus_id);
        snprintf(text, sizeof(text),
                 slider.format != NULL ? slider.format :
                    InputDefaultFormat(NumericInt),
                 slider.values[i]);
        ui_draw_slider_cell(cell,ratio,text,slider.disabled,vertical,focused,
                            slider.class_name);
    }
    ui_draw_slider_label(slider.bounds,slider.label,slider.class_name);
}

static int
ui_slider_continuous(SliderContinuousProps slider, int vertical)
{
    int changed = ui_update_slider_continuous(slider,vertical);
    ui_paint_slider_continuous(slider,vertical);
    return changed;
}

int
ui_update_slider_angle(SliderAngleProps slider)
{
    const float radians_to_degrees = 57.295779513082320876f;
    const float degrees_to_radians = 0.01745329251994329577f;
    float degrees;
    SliderContinuousProps value_slider;
    int changed;

    if(slider.value == NULL)
        return 0;
    degrees = *slider.value * radians_to_degrees;
    value_slider = (SliderContinuousProps){slider.bounds, slider.id, slider.label,
                                      &degrees, 1, slider.min_degrees,
                                      slider.max_degrees, slider.format,
                                      slider.disabled, slider.class_name};
    changed = ui_update_slider_continuous(value_slider, 0);
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
    SliderContinuousProps value_slider = {
        slider.bounds, slider.id, slider.label, &degrees, 1,
        slider.min_degrees, slider.max_degrees, slider.format,
        slider.disabled, slider.class_name
    };
    ui_paint_slider_continuous(value_slider, 0);
}

static NumericInputState *
ui_numeric_input_find(int kind, int widget_id, int component)
{
    ToolkitStore *toolkit = toolkit_state();
    unsigned bucket = ((unsigned)widget_id * 31u + (unsigned)component * 17u +
                       (unsigned)kind) % NUMERIC_INPUT_BUCKETS;
    for(NumericInputState *state = toolkit->numeric_inputs[bucket];
        state != NULL; state = state->next)
        if(state->kind == kind && state->widget_id == widget_id &&
           state->component == component)
            return state;
    return NULL;
}

NumericInputState *
ui_numeric_input_state(int kind, int widget_id, int component)
{
    ToolkitStore *toolkit = toolkit_state();
    unsigned bucket = ((unsigned)widget_id * 31u + (unsigned)component * 17u +
                       (unsigned)kind) % NUMERIC_INPUT_BUCKETS;
    NumericInputState *existing =
        ui_numeric_input_find(kind, widget_id, component);
    if(existing != NULL)
        return existing;

    NumericInputState *state = calloc(1, sizeof(*state));
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
    const char *resolved_format =
        format != NULL ? format : InputDefaultFormat((NumericValueKind)kind);

    value = InputRoundValueForKind((NumericValueKind)kind, value);
    if(InputKindIsInt((NumericValueKind)kind))
        snprintf(text, text_size, resolved_format, (int)value);
    else if(InputKindIsFloat((NumericValueKind)kind))
        snprintf(text, text_size, resolved_format, (float)value);
    else
        snprintf(text, text_size, resolved_format, value);
}

static int
ui_numeric_input(Rectangle bounds, int id, const char *label, void *values,
                 int count, double step, double step_fast, const char *format,
                 int disabled, int kind)
{
    int changed = 0;
    int step_button_width = InputDefaultStepButtonWidth(
        (float)Scale(1000) / 1000.0f);

    if(values == NULL || count <= 0)
        return 0;
    DisabledScope(disabled);
    for(int i = 0; i < count; i++) {
        NumericInputState *state = ui_numeric_input_state(kind, id, i);
        int token = state->token;
        InputCellLayout layout = InputCellLayoutFor(
            bounds, count, i, step_button_width, step != 0.0);
        Rectangle field_bounds = layout.field;
        Rectangle minus = layout.minus;
        Rectangle plus = layout.plus;
        int commit = 0;
        double old_value = ui_numeric_value(values, i, kind);

        if(ContentDisabled() && state->focused)
            while(GetCharPressed() != 0) {}

        if(!state->focused) {
            ui_numeric_format(state->text, sizeof(state->text), format, kind,
                              old_value);
            state->cursor = (int)strlen(state->text);
        }
        if(ui_text_field_render_filtered((TextFieldProps){
                .bounds = field_bounds,
                .text = state->text,
                .text_size = sizeof(state->text),
                .cursor_position = &state->cursor,
                .focused = &state->focused,
                .max_codepoints = 63,
                .focus_id = token,
                .commit_pressed = &commit,
                .secure = 0,
                .read_only = disabled
            }, ui_numeric_input_filter, NULL)) {
            char *end = NULL;
            double value = strtod(state->text, &end);
            if(end != state->text && *end == '\0') {
                value = InputRoundValueForKind((NumericValueKind)kind, value);
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
                InputStep result = InputStepValueForKind(
                    (NumericValueKind)kind, value, step, step_fast, direction,
                    fast);
                value = result.value;
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
    DisabledEndScope();
    return changed;
}

int
RenderInputContinuous(InputContinuousProps input)
{
    return ui_numeric_input(input.bounds, input.id, input.label, input.values,
                            input.value_count, input.step, input.step_fast,
                            input.format, input.disabled, 0);
}

int
RenderInputDiscrete(InputDiscreteProps input)
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
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    SpinboxLayout layout = SpinboxLayoutFor(
        spinbox.bounds, SpinboxDefaultButtonWidth(runtime_scale));
    int changed = 0;
    int disabled = spinbox.disabled || ContentDisabled();
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
        snprintf(value_text, sizeof(value_text), InputDefaultFormat(NumericInt),
                 spinbox.value != NULL ? *spinbox.value : 0);
    if(IsWindowReady()) {
        StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
            spinbox.class_name, StyleKindSpinbox(), StyleAny());
        StyleFrame value_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
            spinbox.class_name, StyleKindSpinboxValue(), StyleAny());
        Style value_style = ui_unpack_style(
            ui_style_apply_effects_frame(value_frame).value);
        int value_font = ResolveFont(
            0, StyleFontValue(value_style.fields, value_style.font_size),
            GetFontSize());
        ui_tk_draw_style_frame(spinbox.bounds, (Rectangle){0}, frame, 0, 0,
                               disabled, 0);
        ui_tk_draw_style_frame(text, spinbox.bounds, value_frame, 0, 0,
                               disabled, 0);
        DrawCenteredText(value_text, (int)(text.x + text.width / 2),
                           (int)(text.y + text.height / 2), value_font,
                           Fade(value_style.foreground, value_style.opacity));
    }
    if(ui_button_render((ButtonSpec){.props = {.bounds = left, .label = "-",
        .id = spinbox.id * 10 + 1, .class_name = spinbox.class_name,
        .disabled = disabled},
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
        .id = spinbox.id * 10 + 2, .class_name = spinbox.class_name,
        .disabled = disabled},
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
    StyleFrame style = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        ButtonStateNormal, 0, 0, frame.class_name, StyleKindFieldset(),
        StyleAny());
    font = ResolveFont(0, StyleFontValue(style.value.fields,
                                         style.value.font_size), font);
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
    int disabled = list.disabled || ContentDisabled();
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    StyleFrame frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        list.class_name, StyleKindListBox(), StyleAny());
    StyleFrame default_item_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        list.class_name, StyleKindListBoxItem(), StyleAny());
    Style default_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_item_frame).value);
    int font = ResolveFont(0, StyleFontValue(default_item_style.fields,
                                             default_item_style.font_size),
                           GetFontSize());
    int selected = list.selected_index != NULL ? *list.selected_index : -1;
    int row_h = ListBoxRowHeight(list.row_height > 0
        ? Scale(list.row_height) : 0, runtime_scale, default_item_frame);
    ListBoxLayout layout;
    int scroll_y;
    int first;
    int visible;
    int max_scroll;
    int changed = 0;

    max_scroll = ui_update_scroll(list.bounds, list.item_count * row_h,
                                  disabled ? NULL : list.scroll_offset, row_h);
    layout = ListBoxLayoutFor(list.bounds, list.item_count, row_h, 0,
                              list.scroll_offset != NULL ? *list.scroll_offset : 0,
                              runtime_scale, default_item_frame);
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
                                                    max_scroll, runtime_scale,
                                                    default_item_frame);
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
    layout = ListBoxLayoutFor(list.bounds, list.item_count, row_h, 0, scroll_y,
                              runtime_scale, default_item_frame);
    first = layout.first_row;
    visible = layout.visible_rows;
    if(paint) {
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
            int item_font = ResolveFont(
                0, StyleFontValue(item_style.fields, item_style.font_size),
                font);
            int label_inset = ListBoxItemLabelInset(runtime_scale, item_frame);
            ListBoxItemPaint item_paint =
                ListBoxItemPaintFor(row, label_inset,
                                    TextLineHeight(item_font));
            RenderText(list.items != NULL && list.items[index] != NULL ? list.items[index] : "",
                       item_paint.text_x, item_paint.text_y, item_font,
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
    if(paint && list.scroll_offset != NULL && max_scroll > 0) {
        Rectangle scrollbar = ListBoxScrollbarBoundsFor(list.bounds,
            runtime_scale, frame);
        ui_scrollbar((int)scrollbar.x, (int)scrollbar.y,
                     (int)scrollbar.height,
                     list.item_count * row_h, list.scroll_offset,
                     max_scroll, 0);
    }
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
    StyleFrame panel_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        tree.disabled ? ButtonStateDisabled : ButtonStateNormal,
        tree.disabled, 0, tree.class_name, StyleKindTreeView(), StyleAny());
    int font = ResolveFont(0, StyleFontValue(default_item_style.fields,
                                             default_item_style.font_size),
                           GetFontSize());
    TreeViewMetrics metrics = TreeViewMetricsFor((float)GetScale(),
                                                 panel_frame,
                                                 default_item_frame);
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
        ui_tk_draw_style_frame(tree.bounds, (Rectangle){0}, panel_frame, 0, 0,
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
        int item_font = ResolveFont(0, StyleFontValue(item_style.fields,
                                                      item_style.font_size),
                                    font);
        TreeViewTextPaint text_paint = TreeViewTextPaintFor(
            marker_bounds, text_bounds, TextLineHeight(item_font));
        Color item_text = Fade(item_style.foreground, item_style.opacity);
        if(paint && (selected || hot || tree.disabled))
            ui_tk_draw_style_frame(row, tree.bounds, item_frame, hot, 0,
                                   tree.disabled, 0);
        if(paint) {
            if(item->expanded)
                RenderText("v", text_paint.marker_x, text_paint.marker_y,
                           item_font, item_text);
            else
                RenderText(">", text_paint.marker_x, text_paint.marker_y,
                           item_font, item_text);
            RenderText(item->label != NULL ? item->label : "",
                       text_paint.text_x, text_paint.text_y,
                       item_font, item_text);
        }
        if(hot)
            MarkClickable();
        TreeViewRowDecision decision = TreeViewRowDecisionFor(
            hot, IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0,
            item->selectable != 0, tree.selected_id != NULL,
            item->id);
        if(decision.consume_release)
            ConsumeRelease();
        if(decision.select) {
            *tree.selected_id = decision.selected_id;
        }
        if(decision.changed)
            changed = 1;
    }
    if(paint)
        EndClip();
    if(paint && tree.scroll_offset != NULL && max_scroll > 0) {
        Rectangle scrollbar = TreeViewScrollbarBoundsFor(tree.bounds,
                                                         metrics.scrollbar_width);
        ui_scrollbar((int)scrollbar.x, (int)scrollbar.y,
                     (int)scrollbar.height,
                     content_h, tree.scroll_offset, max_scroll, 0);
    }
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

static TableViewMetrics
ui_table_metrics(TableViewProps table)
{
    ButtonState state = table.disabled ? ButtonStateDisabled : ButtonStateNormal;
    StyleFrame table_frame = {0};
    StyleFrame header_frame = {0};
    StyleFrame cell_frame = {0};
    StyleFrame divider_frame = {0};
    table_frame.value = ResolveActiveStyle((StyleData){0},
        StyleControlRoleFacts(StyleKindTableView(), 0, table.class_name,
            StyleAny(), ButtonToneNeutral, ButtonEmphasisSoft,
            ControlSizeMedium, state), state);
    header_frame.value = ResolveActiveStyle((StyleData){0},
        StyleControlRoleFacts(StyleKindTableView(), 0, table.class_name,
            TableViewHeaderRole(), ButtonToneNeutral, ButtonEmphasisSoft,
            ControlSizeMedium, state), state);
    cell_frame.value = ResolveActiveStyle((StyleData){0},
        StyleControlRoleFacts(StyleKindTableView(), 0, table.class_name,
            TableViewCellRole(), ButtonToneNeutral, ButtonEmphasisSoft,
            ControlSizeMedium, state), state);
    divider_frame.value = ResolveActiveStyle((StyleData){0},
        StyleControlRoleFacts(StyleKindTableView(), 0, table.class_name,
            TableViewDividerRole(), ButtonToneNeutral, ButtonEmphasisSoft,
            ControlSizeMedium, state), state);
    return TableViewMetricsFor((float)GetScale(), table_frame, header_frame,
                               cell_frame, divider_frame);
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
        ? TableViewSelectedRowFor(*table.selected_row, table.row_count)
        : TableViewSelectedRowFor(-1, table.row_count);
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
        row = TableViewSelectionMoveRow(row, table.row_count, -1);
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_DOWN)) {
        row = TableViewSelectionMoveRow(row, table.row_count, 1);
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_LEFT)) {
        column_slot = TableViewSelectionMoveColumn(column_slot, visible_columns,
                                                   -1);
        column = ui_table_display_column(table,column_slot);
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_RIGHT)) {
        column_slot = TableViewSelectionMoveColumn(column_slot, visible_columns,
                                                   1);
        column = ui_table_display_column(table,column_slot);
        selection_changed = changed = 1;
    }
    if(IsKeyPressed(KEY_TAB)) {
        TableViewSelection selection = TableViewSelectionTab(
            row, column_slot, table.row_count, visible_columns,
            IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
        row = selection.row;
        column_slot = selection.column_slot;
        column = ui_table_display_column(table,column_slot);
        ui_consume_focus_tab();
        selection_changed = changed = 1;
    }
    TableViewClipboardDecision clipboard_decision =
        TableViewClipboardDecisionFor(
            ui_table_mod_key_down() != 0,
            IsKeyPressed(KEY_C) != 0,
            IsKeyPressed(KEY_X) != 0,
            IsKeyPressed(KEY_V) != 0,
            table.pasted_text != NULL);
    if(clipboard_decision.copy_selection) {
        char *copy = ui_table_clipboard_text(table,*table.selected_row,
            table.selected_column != NULL ? *table.selected_column : -1);
        if(copy != NULL) {
            SetClipboardTextValue(copy);
            free(copy);
            changed = 1;
        }
    }
    if(clipboard_decision.paste) {
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
    if(IsKeyPressed(KEY_ESCAPE)) {
        TableViewSelectionClearDecision clear_decision =
            TableViewSelectionClearFor(
                *table.selected_row,
                table.selected_column != NULL ? *table.selected_column : -1);
        if(clear_decision.changed) {
            row = clear_decision.row;
            column = clear_decision.column;
            selection_changed = changed = 1;
        }
    }

    if(selection_changed) {
        *table.selected_row = row;
        if(table.selected_column != NULL) *table.selected_column = column;
        if(row >= frozen_rows && table.scroll_offset != NULL) {
            int view_h = (int)table.bounds.height-header_h-frozen_rows*row_h;
            *table.scroll_offset = TableViewSelectionScrollOffset(
                row, frozen_rows, row_h, view_h, *table.scroll_offset,
                max_scroll);
        }
    }
    return changed;
}

Rectangle
TableCellScope(TableViewProps table, int row, int column)
{
    Rectangle cell = {0,0,0,0}, clip = {0,0,0,0};
    int visible = ui_table_visible_columns(table), found = 0;
    for(int slot = 0; slot < table.column_count; slot++)
        if(ui_table_display_column(table,slot) == column) found = 1;
    if(found && visible > 0 && row >= 0 && row < table.row_count) {
        TableViewMetrics metrics = ui_table_metrics(table);
        TableViewLayout layout = TableViewLayoutFor(table.bounds, table.row_count,
            table.row_height, table.header_height, table.freeze_rows,
            (float)GetScale(), metrics);
        int row_h = layout.row_height;
        int frozen = layout.frozen_rows;
        int scroll = table.scroll_offset != NULL ? *table.scroll_offset : 0;
        TableViewScrollLayout scroll_layout = TableViewScrollFor(
            scroll, frozen, row_h, layout.scroll_body_height);
        int default_width = (int)table.bounds.width/visible;
        Rectangle row_bounds = TableViewRowBounds(
            table.bounds, layout, row,
            row < frozen ? row : frozen + row - scroll_layout.first,
            scroll_layout, row >= frozen);
        cell = TableViewCellBounds(row_bounds,
            ui_table_column_x(table,column,default_width),
            ui_table_column_width(table,column,default_width));
        Rectangle viewport = TableViewViewport(table.bounds, layout,
            row >= frozen);
        clip = GetCollisionRec(cell,viewport);
    }
    DisabledScope(table.disabled);
    (void)ScrollScope(clip,(int)clip.height,NULL);
    return cell;
}

void
TableCellEndScope(void)
{
    ScrollEndScope();
    DisabledEndScope();
}

int
RenderTableView(TableViewProps table)
{
    ToolkitStore *toolkit = toolkit_state();
    int paint = IsWindowReady();
    TableViewMetrics metrics;
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

    table.disabled = table.disabled || ContentDisabled();
    metrics = ui_table_metrics(table);
    StyleFrame default_header_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        table.disabled ? ButtonStateDisabled : ButtonStateNormal,
        table.disabled, 0, table.class_name, StyleKindTableView(), TableViewHeaderRole());
    StyleFrame default_cell_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
        table.disabled ? ButtonStateDisabled : ButtonStateNormal,
        table.disabled, 0, table.class_name, StyleKindTableView(), TableViewCellRole());
    header_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_header_frame).value);
    text_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_cell_frame).value);
    header_font = ResolveFont(0, StyleFontValue(header_style.fields,
                                                header_style.font_size),
                              GetSmallFontSize());
    cell_font = ResolveFont(0, StyleFontValue(text_style.fields,
                                              text_style.font_size),
                            header_font);

    TableViewResizeClearDecision resize_clear = TableViewResizeClearFor(
        toolkit->resize_column >= 0,
        ui_popup_input_owner_captures(toolkit->resize_owner) != 0,
        ContentDisabled() != 0,
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0,
        toolkit->resize_table_id == table.id,
        table.disabled != 0,
        table.resizable != 0,
        table.column_widths != NULL);
    if(resize_clear.clear) {
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
    if(!ContentDisabled() && !table.disabled && table.resizable && table.column_widths != NULL) {
        Vector2 mouse = ui_mouse_world();
        Rectangle header = {table.bounds.x, table.bounds.y,
                            table.bounds.width, (float)header_h};
        if(toolkit->resize_column < 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           ui_contains(header, mouse)) {
            int separator_x = 0;
            int column = ui_table_separator_at_x(table, (int)(mouse.x-ui_table_header_shift(table,mouse.y)),
                                                  metrics.resize_tolerance, default_col_w,
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
                int width = TableViewResizeColumnWidthFor(
                    toolkit->resize_start_width, toolkit->resize_start_x,
                    (int)mouse.x, minimum);
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
        StyleFrame surface_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            table.disabled ? ButtonStateDisabled : ButtonStateNormal,
            table.disabled, 0, table.class_name, StyleKindTableView(), TableViewPanelRole());
        StyleFrame text_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            table.disabled ? ButtonStateDisabled : ButtonStateNormal,
            table.disabled, 0, table.class_name, StyleKindTableView(), TableViewCellRole());
        StyleFrame selection_frame = ui_tk_simple_style_frame_class_role(ButtonToneAccent,
            table.disabled ? ButtonStateDisabled : ButtonStateSelected,
            table.disabled, 1, table.class_name, StyleKindTableView(), TableViewSelectionRole());
        StyleFrame divider_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
            table.disabled ? ButtonStateDisabled : ButtonStateNormal,
            table.disabled, 0, table.class_name, StyleKindTableView(), TableViewDividerRole());
        text_style = ui_unpack_style(ui_style_apply_effects_frame(text_frame).value);
        selection_style = ui_unpack_style(ui_style_apply_effects_frame(selection_frame).value);
        divider_style = ui_unpack_style(ui_style_apply_effects_frame(divider_frame).value);
        cell_font = ResolveFont(0, StyleFontValue(text_style.fields,
                                                  text_style.font_size),
                                cell_font);
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
            StyleFrame header_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
                header_state, table.disabled, selected_header,
                table.class_name, StyleKindTableView(), TableViewHeaderRole());
            Style header_paint = ui_unpack_style(
                ui_style_apply_effects_frame(header_frame).value);
            Color header_color = header_paint.background;
            Color text_color = Fade(header_paint.foreground,
                                    header_paint.opacity);
            int render_header_font = ResolveFont(
                0, StyleFontValue(header_paint.fields,
                                  header_paint.font_size),
                header_font);
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
                            (Vector2){head.x + (angle > 0 ? shift : 0) + metrics.header_text_pad_x, angle < 0 ? head.y + head.height - metrics.header_text_pad_x : head.y + metrics.header_text_pad_x},
                            (Vector2){0,0}, angle, render_header_font, 1, text_color);
                    EndClip();
                }
            } else RenderText(label, (int)head.x + metrics.header_text_pad_x, ui_row_text_y(head, render_header_font), render_header_font, text_color);
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
            StyleFrame row_frame = ui_tk_simple_style_frame_class_role(ButtonToneNeutral,
                table.disabled ? ButtonStateDisabled : ButtonStateNormal,
                table.disabled, 0, table.class_name, StyleKindTableView(), TableViewRowRole());
            ui_tk_draw_style_frame(row, table.bounds, row_frame, 0, 0,
                                   table.disabled, 0);
        }
        if(paint && ((table.selected_row != NULL && *table.selected_row == r) || hot)) {
            int selected = table.selected_row != NULL && *table.selected_row == r;
            ButtonState row_state = hot ? ButtonStateHover : ButtonStateSelected;
            StyleFrame row_frame = ui_tk_simple_style_frame_class_role(ButtonToneAccent,
                row_state, table.disabled, selected, table.class_name,
                StyleKindTableView(), TableViewSelectionRole());
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
                BeginClip(x, (int)row.y, col_w, (int)row.height);
                Color text_color = text_style.foreground;
                float text_opacity = text_style.opacity;
                int render_font = cell_font;
                if((table.selected_row != NULL && *table.selected_row == r) || hot) {
                    text_color = selection_style.foreground;
                    text_opacity = selection_style.opacity;
                    render_font = ResolveFont(
                        0, StyleFontValue(selection_style.fields,
                                          selection_style.font_size),
                        render_font);
                } else {
                    text_color = text_style.foreground;
                }
                RenderText(text, x + metrics.header_text_pad_x, ui_row_text_y(row, render_font),
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
    if(paint && table.scroll_offset != NULL && max_scroll > 0) {
        Rectangle scrollbar = TableViewScrollbarBoundsFor(table.bounds, layout,
                                                          metrics.scrollbar_width);
        ui_scrollbar((int)scrollbar.x, (int)scrollbar.y,
                        (int)scrollbar.height,
                        (table.row_count - frozen_rows) * row_h,
                        table.scroll_offset, max_scroll, 0);
    }
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
CanvasScope(Canvas canvas)
{
    ToolkitStore *toolkit = toolkit_state();
    CanvasResult result = {0};
    Vector2 mouse = ui_mouse_world();
    CanvasPolicyResult policy;

    ui_tk_draw_style_frame(canvas.bounds, canvas.bounds,
                           ui_canvas_frame(canvas.class_name), 0, 0, 0, 0);
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
CanvasEndScope(Canvas canvas)
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
        ui_style_apply_effects_frame(ui_canvas_frame(canvas.class_name)).value);
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
    StyleFrame normal_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, 0, 0, panes.class_name,
        StyleKindPanedView(), PanedViewHandleRole());
    PanedViewMetrics metrics = PanedViewMetricsFor((float)GetScale(),
                                                   normal_frame);
    int changed = 0;
    PanedViewLayout layout = PanedViewLayoutFor(
        panes.bounds, panes.vertical != 0,
        panes.split != NULL ? *panes.split : 0, panes.split != NULL,
        panes.min_first, panes.min_second, metrics);
    int split = layout.split;
    Rectangle handle = layout.handle;
    int handle_hot = !ContentDisabled() && ui_hot(handle);
    PanedViewDragDecision drag = PanedViewDragFor(
        toolkit->active_split != NULL, toolkit->active_split == panes.split,
        toolkit->active_split != NULL &&
            ui_popup_input_owner_captures(toolkit->active_split_owner),
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) != 0,
        ContentDisabled() != 0, panes.split != NULL, handle_hot != 0,
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) != 0);
    if(drag.clear_active)
        toolkit->active_split = NULL;
    if(handle_hot) {
        MarkClickable();
        if(drag.start_drag) {
            toolkit->active_split = panes.split;
            toolkit->active_split_owner = ui_popup_input_owner();
        }
    }
    if(drag.drag_active) {
        Vector2 mouse = ui_mouse_world();
        split = PanedViewPointerSplitFor(panes.bounds, panes.vertical != 0,
                                         mouse.x, mouse.y, panes.min_first,
                                         panes.min_second);
    }
    if(PanedViewChanged(panes.split != NULL ? *panes.split : 0, split,
                        panes.split != NULL)) {
        *panes.split = split;
        changed = 1;
    }
    handle = PanedViewHandleFor(panes.bounds, panes.vertical != 0,
                                split, metrics);
    if(IsWindowReady()) {
        ui_tk_draw_style_frame(handle, (Rectangle){0}, normal_frame, 0, 0, 0, 0);
    }
    return changed;
}

static void
ui_tree_header_register(CollapsibleProps section, int enabled)
{
    ToolkitStore *toolkit = toolkit_state();

    if(toolkit->tree_header_frame != g_ui_frame_serial) {
        TreeHeaderNavState *swap = toolkit->tree_previous;
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
        TreeHeaderNavState *items = realloc(toolkit->tree_headers,
                                        sizeof(*items) * capacity);
        if(items == NULL) return;
        toolkit->tree_headers = items;
        toolkit->tree_header_capacity = capacity;
    }
    toolkit->tree_headers[toolkit->tree_header_count++] =
        (TreeHeaderNavState){section.id, section.depth > 0 ? section.depth : 0};
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
    int enabled = !section.disabled && !ContentDisabled();
    ButtonState default_state = !enabled ? ButtonStateDisabled
                              : section.selected ? ButtonStateSelected
                              : ButtonStateNormal;
    StyleFrame default_item_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, default_state, !enabled, section.selected,
        section.class_name, StyleKindCollapsible(),
        CollapsibleHeaderRoleFor(section.tree != 0));
    Style default_item_style = ui_unpack_style(
        ui_style_apply_effects_frame(default_item_frame).value);
    int font = ResolveFont(0, StyleFontValue(default_item_style.fields,
                                             default_item_style.font_size),
                           GetFontSize());
    StyleFrame tree_item_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, default_state, !enabled, section.selected,
        section.class_name, StyleKindCollapsible(),
        CollapsibleTreeHeaderRole());
    StyleFrame close_default_frame = ui_tk_simple_style_frame_class_role(
        ButtonToneNeutral, ButtonStateNormal, !enabled, 0,
        section.class_name, StyleKindCollapsible(), CollapsibleCloseRole());
    int changed = 0;
    CollapsibleMetrics metrics = CollapsibleMetricsFor((float)GetScale(),
        default_item_frame, tree_item_frame, close_default_frame);
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
        StyleFrame item_frame = ui_tk_simple_style_frame_class_role(
            ButtonToneNeutral, state, !enabled, section.selected,
            section.class_name, StyleKindCollapsible(),
            CollapsibleHeaderRoleFor(section.tree != 0));
        Style item_style = ui_unpack_style(
            ui_style_apply_effects_frame(item_frame).value);
        font = ResolveFont(0, StyleFontValue(item_style.fields,
                                             item_style.font_size), font);
        StyleFrame link_frame = ui_tk_simple_style_frame_class_role(
            ButtonToneNeutral, close_hover ? ButtonStateHover : ButtonStateNormal,
            !enabled, 0, section.class_name, StyleKindCollapsible(),
            CollapsibleCloseRole());
        Style link_style = ui_unpack_style(
            ui_style_apply_effects_frame(link_frame).value);
        int close_font = ResolveFont(0, StyleFontValue(link_style.fields,
                                                       link_style.font_size),
                                     font);
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
            nodes[i].focused ? 1.0f : 0.0f, StyleKindFocus(),
            FocusBoxRole()).value);
        Style label = ui_unpack_style(ui_control_style_frame_role_kind(
            (ButtonProps){0}, state, 0, 0.0f, 0.0f,
            nodes[i].focused ? 1.0f : 0.0f, StyleKindFocus(),
            FocusLabelRole()).value);
        int font = ResolveFont(0, StyleFontValue(label.fields, label.font_size),
                               GetSmallFontSize());
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
