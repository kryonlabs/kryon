#include "ui_internal.h"
#include "ui_paint_layers_internal.h"
#include "ui_disabled_internal.h"
#include "ui_input_clip_internal.h"
#include "ui_popup_input_internal.h"
#include "ui_text.h"
#include "ui_tk.h"
#include "ui_style_internal.h"
#include "platform.h"
#include "theme.h"
#include "runtime/bevel.h"
#include "runtime/button.h"
#include "runtime/focus.h"
#include "runtime/icon.h"
#include "runtime/input.h"
#include "runtime/layout.h"
#include "runtime/link.h"
#include "runtime/paragraph.h"
#include "runtime/scroll.h"
#include "runtime/style.h"
#include "runtime/surface.h"
#include "runtime/text.h"
#include "runtime/text_input.h"
#include "kry_uri.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Vector2 kryon_zero_vector2;

static Color
ui_default_text_color(void)
{
    Style style = ui_unpack_style(ResolveActiveStyle(
        ui_pack_style_states((ControlStyle){.normal = {.opacity = 1}}).normal,
        StyleTextFacts(0, 0, StyleKindText(), ButtonStateNormal),
        ButtonStateNormal));
    return style.foreground.a != 0 ? style.foreground : c_text;
}

static Rectangle
ui_rect(float x, float y, float width, float height)
{
    Rectangle rect;

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    return rect;
}

static ScrollMetrics
ui_scope_scroll_metrics(void)
{
    StyleFrame track = {0};
    StyleFrame thumb = {0};
    return ScrollMetricsFor((float)GetScale(), track, thumb);
}

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

/* Global UI state */
int ui_view_width = 320;
int ui_view_height = 560;
Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon, c_link;
Camera2D g_ui_camera;
static int *g_ui_cursor_clickable = NULL;
static int *g_ui_cursor_disabled = NULL;
static int g_ui_cursor_priority = 0;
static int g_ui_cursor_current = MOUSE_CURSOR_DEFAULT;
static int g_ui_cursor_had_intent = 0;
Texture2D g_ui_gear_icon = {0};
Texture2D g_ui_x_icon = {0};
int g_ui_slider_active_id = 0;
static int g_ui_pointer_down = 0;
int g_ui_pointer_dragging = 0;
static int g_ui_pointer_dragged_this_click = 0;
static int g_ui_pointer_start_x = INT_MIN;
static int g_ui_pointer_start_y = INT_MIN;
static Vector2 g_ui_pointer_start_world = {0};
/* Negative means use the shared default; explicit user choices are retained. */
static int g_ui_transition_cues_enabled = -1;
static int g_ui_release_consumed = 0;
static int g_ui_disabled_depth = 0;
static int g_ui_disabled_start = 0;
static int g_ui_disabled_floor;
static int g_scroll_scope_depth = 0;
static unsigned long g_scroll_wheel_frame = 0;
static int *g_scroll_drag_offset = NULL;
static float g_scroll_drag_grab = 0;
static int g_ui_mouse_world_override_enabled = 0;
static Vector2 g_ui_mouse_world_override = {0};
static int ui_default_font_auto_load = 1;

int g_ui_pointer_owner = POINTER_OWNER_NONE;
int g_ui_scroll_gesture_pending = 0;

#define FOCUS_MAX_ITEMS 256
static int g_ui_focus_active_id = 0;
static int g_ui_focus_ids[FOCUS_MAX_ITEMS];
static int g_ui_focus_count = 0;
static int g_ui_focus_tab_dir = 0;
static int g_ui_focus_frame_open = 0;
static int g_ui_focus_text_input_active = 0;
static unsigned long g_ui_overlays_drawn_frame = 0;
static int g_ui_platform_text_input_active = 0;
static int g_ui_text_input_requested = 0;
static int g_ui_text_input_show_requested = 0;
static TextInputPlatformCallback g_ui_text_input_platform_callback = NULL;
static void ui_sync_platform_text_input(void);
static int g_ui_text_area_drag_id = 0;
static int *g_ui_text_area_drag_owner = NULL;
static int g_ui_text_area_last_click_id = 0;
static int *g_ui_text_area_last_click_owner = NULL;
static int g_ui_text_area_last_click_cursor = -1;
static int g_ui_text_area_last_click_x = 0;
static int g_ui_text_area_last_click_y = 0;
static double g_ui_text_area_last_click_time = 0.0;
typedef struct TextSelection {
    int id;
    int *owner;
    int anchor;
    int cursor;
    int dragging;
} TextSelection;

static TextSelection g_ui_text_area_selection = {0};

#define TEXT_AREA_HEIGHT_CACHE_SIZE 16

typedef struct TextAreaHeightCacheEntry {
    const char *text;
    int font;
    int line_gap;
    int wrap_width;
    int len;
    int content_version;
    int height;
} TextAreaHeightCacheEntry;

static TextAreaHeightCacheEntry g_ui_text_area_height_cache[TEXT_AREA_HEIGHT_CACHE_SIZE];
static int g_ui_text_area_height_cache_next = 0;
static TextSelection g_ui_text_field_selection = {0};
static int g_ui_text_field_last_click_id = 0;
static int *g_ui_text_field_last_click_owner = NULL;
static int g_ui_text_field_last_click_x = 0;
static int g_ui_text_field_last_click_y = 0;
static double g_ui_text_field_last_click_time = 0.0;
static int g_ui_text_field_drag_id = 0;
static int *g_ui_text_field_drag_owner = NULL;

#define TEXT_FIELD_SCROLL_CACHE_SIZE 128

typedef struct TextFieldScrollCacheEntry {
    int id;
    int *owner;
    int scroll_x;
} TextFieldScrollCacheEntry;

static TextFieldScrollCacheEntry g_ui_text_field_scroll_cache[TEXT_FIELD_SCROLL_CACHE_SIZE];
static int g_ui_text_field_scroll_cache_next = 0;
static int g_ui_text_field_pan_id = 0;
static int *g_ui_text_field_pan_owner = NULL;
static int g_ui_text_field_pan_start_x = 0;
static int g_ui_text_field_pan_start_y = 0;
static int g_ui_text_field_pan_start_scroll = 0;
static int g_ui_text_field_panning = 0;

enum {
    TEXT_CONTEXT_NONE = 0,
    TEXT_CONTEXT_FIELD,
    TEXT_CONTEXT_AREA
};

enum {
    TEXT_CONTEXT_CUT = 9101,
    TEXT_CONTEXT_COPY,
    TEXT_CONTEXT_PASTE,
    TEXT_CONTEXT_SELECT_ALL
};

static int g_ui_text_context_open = 0;
static int g_ui_text_context_kind = TEXT_CONTEXT_NONE;
static int g_ui_text_context_id = 0;
static int *g_ui_text_context_owner = NULL;
static int g_ui_text_context_x = 0;
static int g_ui_text_context_y = 0;
unsigned long g_ui_frame_serial = 0;
static int g_ui_auto_focus_id = 0x40000000;
static unsigned long g_ui_text_context_target_frame = 0;
static char *g_ui_text_context_text = NULL;
static size_t g_ui_text_context_text_size = 0;
static int *g_ui_text_context_cursor = NULL;
static int g_ui_text_context_max_codepoints = 0;
static TextInputFilter g_ui_text_context_filter = NULL;
static void *g_ui_text_context_filter_user_data = NULL;
static TextSelection *g_ui_text_context_selection = NULL;
static int g_ui_text_context_selection_start = 0;
static int g_ui_text_context_selection_end = 0;
static int g_ui_text_context_allow_newlines = 0;
static int g_ui_text_context_copy_all_when_empty = 0;
static int g_ui_text_context_read_only = 0;
static int g_ui_text_context_changed = 0;
static int g_ui_text_context_changed_kind = TEXT_CONTEXT_NONE;
static int g_ui_text_context_changed_id = 0;
static int *g_ui_text_context_changed_owner = NULL;

static int
ui_text_context_command_kind(int command)
{
    switch(command) {
    case TEXT_CONTEXT_CUT:
        return TextContextCommandCut();
    case TEXT_CONTEXT_COPY:
        return TextContextCommandCopy();
    case TEXT_CONTEXT_PASTE:
        return TextContextCommandPaste();
    case TEXT_CONTEXT_SELECT_ALL:
        return TextContextCommandSelectAll();
    default:
        return 0;
    }
}

/* Identity of the widget that currently owns text input focus, recorded as the
 * address of its `focused` flag. Ownership is PERSISTENT across frames: it does
 * not reset at frame start, it only changes when a text input is clicked (or
 * explicitly claimed). This guarantees that at most one text input (TextFieldProps
 * or TextAreaProps) is ever treated as focused, so at most one blinking caret can
 * ever be drawn - the UI layer owns focus, never trusting stale per-widget
 * flags. Works across widget types without relying on focus_id (which defaults
 * to 0 and is shared when unset). */
static int *g_ui_text_focus_owner = NULL;
/* Frame counter and the frame on which the current owner last confirmed it is
 * still alive. If the owner has not run for a frame, ownership is considered
 * released (the user navigated away from that screen / the widget is gone),
 * which lets a new screens autofocus adopt cleanly. */
static unsigned long g_ui_text_focus_frame = 0;
static unsigned long g_ui_text_focus_owner_frame = 0;
static int *g_ui_text_focus_owner_this_frame = NULL;

#define TEXT_INPUT_QUEUE_MAX 64
static int g_ui_text_input_codepoints[TEXT_INPUT_QUEUE_MAX];
static int g_ui_text_input_codepoint_count = 0;
static int g_ui_text_input_backspace_count = 0;
static int g_ui_text_input_enter_count = 0;
static double g_ui_backspace_next_repeat_at = 0.0;

enum {
    CURSOR_PRIORITY_DEFAULT = 0,
    /* Disabled sits just above default and below every interactive intent:
     * widgets draw back-to-front, so anything clickable, textual or
     * resizable drawn after disabled background content is visually on top
     * of it and must own the cursor. With DISABLED ranked highest, a board
     * that disables its cards behind a modal banned the cursor for the
     * whole frame — the modals own buttons and text never got a say. */
    CURSOR_PRIORITY_DISABLED = 1,
    CURSOR_PRIORITY_TEXT = 2,
    CURSOR_PRIORITY_CLICKABLE = 3,
    CURSOR_PRIORITY_RESIZE = 4
};

#define INPUT_CLIP_STACK_MAX INPUT_CLIP_DEPTH
static Rectangle g_ui_input_clip_stack[INPUT_CLIP_STACK_MAX];
static int g_ui_input_clip_stack_count = 0;

#define INPUT_CAPTURE_STACK_MAX 16
typedef struct InputCapture {
    Rectangle bounds;
    int allow_inside;
} InputCapture;

static InputCapture g_ui_input_capture_stack[INPUT_CAPTURE_STACK_MAX];
static int g_ui_input_capture_stack_count = 0;
static Rectangle g_ui_modal_capture_next_frame_bounds;
static int g_ui_modal_capture_next_frame = 0;

static void
ui_text_selection_clear(TextSelection *selection)
{
    if(selection == NULL)
        return;
    selection->id = 0;
    selection->owner = NULL;
    selection->anchor = 0;
    selection->cursor = 0;
    selection->dragging = 0;
}

static void
ui_clear_text_field_selection(void)
{
    ui_text_selection_clear(&g_ui_text_field_selection);
    g_ui_text_field_drag_id = 0;
    g_ui_text_field_drag_owner = NULL;
    g_ui_text_field_pan_id = 0;
    g_ui_text_field_pan_owner = NULL;
    g_ui_text_field_panning = 0;
}

static void
ui_clear_text_area_selection(void)
{
    ui_text_selection_clear(&g_ui_text_area_selection);
    g_ui_text_area_drag_id = 0;
    g_ui_text_area_drag_owner = NULL;
}

static int
ui_text_selection_matches(TextSelection selection, int id, int *owner)
{
    return TextSelectionOwnerMatches(owner != NULL,
                                     selection.owner == owner,
                                     selection.id, id);
}

static void
ui_text_selection_set(TextSelection *selection, int id, int *owner,
                      int anchor, int cursor, int dragging)
{
    if(selection == NULL)
        return;
    selection->id = id;
    selection->owner = owner;
    selection->anchor = anchor;
    selection->cursor = cursor;
    selection->dragging = dragging;
}

static TextSelectionState
ui_text_selection_collapsed(int cursor)
{
    return TextSelectionCollapsed(cursor);
}

static void
ui_text_selection_set_collapsed(TextSelection *selection, int id, int *owner,
                                int cursor, int dragging)
{
    TextSelectionState collapsed = ui_text_selection_collapsed(cursor);

    ui_text_selection_set(selection, id, owner, collapsed.anchor,
                          collapsed.cursor, dragging);
}

static int
ui_text_field_scroll_entry_matches(TextFieldScrollCacheEntry entry,
                                   int id, int *owner)
{
    if(id > 0)
        return entry.id == id;
    return entry.id == 0 && entry.owner == owner;
}

static int *
ui_text_field_scroll_for(int id, int *owner)
{
    int empty_slot = -1;
    int slot;

    for(int i = 0; i < TEXT_FIELD_SCROLL_CACHE_SIZE; i++) {
        if(ui_text_field_scroll_entry_matches(g_ui_text_field_scroll_cache[i],
                                              id, owner))
            return &g_ui_text_field_scroll_cache[i].scroll_x;
        if(g_ui_text_field_scroll_cache[i].id == 0 &&
           g_ui_text_field_scroll_cache[i].owner == NULL &&
           empty_slot < 0)
            empty_slot = i;
    }

    if(empty_slot >= 0) {
        slot = empty_slot;
    } else {
        slot = g_ui_text_field_scroll_cache_next;
        g_ui_text_field_scroll_cache_next =
            (g_ui_text_field_scroll_cache_next + 1) %
            TEXT_FIELD_SCROLL_CACHE_SIZE;
    }

    g_ui_text_field_scroll_cache[slot].id = id;
    g_ui_text_field_scroll_cache[slot].owner = id > 0 ? NULL : owner;
    g_ui_text_field_scroll_cache[slot].scroll_x = 0;
    return &g_ui_text_field_scroll_cache[slot].scroll_x;
}

Vector2
ui_mouse_world(void)
{
    if(g_ui_mouse_world_override_enabled)
        return g_ui_mouse_world_override;
    return GetScreenToWorld2D(GetMousePosition(), g_ui_camera);
}

static Vector2
screen_to_world_for_input(Vector2 screen)
{
    if(g_ui_mouse_world_override_enabled)
        return g_ui_mouse_world_override;
    return GetScreenToWorld2D(screen, g_ui_camera);
}

void
SetMouseWorldOverride(int enabled, Vector2 position)
{
    g_ui_mouse_world_override_enabled = enabled ? 1 : 0;
    g_ui_mouse_world_override = position;
}

static void
ui_set_cursor_intent(int cursor, int priority)
{
    if(priority < g_ui_cursor_priority)
        return;
    g_ui_cursor_priority = priority;
    g_ui_cursor_had_intent = 1;
    if(g_ui_cursor_current == cursor)
        return;
    g_ui_cursor_current = cursor;
    SetMouseCursor(cursor);
}

void
MarkClickable(void)
{
    if(g_ui_cursor_clickable != NULL)
        *g_ui_cursor_clickable = 1;
    ui_set_cursor_intent(MOUSE_CURSOR_POINTING_HAND, CURSOR_PRIORITY_CLICKABLE);
}

void
MarkCursor(int cursor)
{
    int priority = CURSOR_PRIORITY_CLICKABLE;

    if(cursor == MOUSE_CURSOR_NOT_ALLOWED)
        priority = CURSOR_PRIORITY_DISABLED;
    else if(cursor == MOUSE_CURSOR_IBEAM)
        priority = CURSOR_PRIORITY_TEXT;
    else if(cursor == MOUSE_CURSOR_RESIZE_EW ||
            cursor == MOUSE_CURSOR_RESIZE_NS ||
            cursor == MOUSE_CURSOR_RESIZE_NWSE ||
            cursor == MOUSE_CURSOR_RESIZE_NESW ||
            cursor == MOUSE_CURSOR_RESIZE_ALL)
        priority = CURSOR_PRIORITY_RESIZE;
    ui_set_cursor_intent(cursor, priority);
}

void
MarkDisabled(void)
{
    if(g_ui_cursor_disabled != NULL)
        *g_ui_cursor_disabled = 1;
    ui_set_cursor_intent(MOUSE_CURSOR_NOT_ALLOWED, CURSOR_PRIORITY_DISABLED);
}

int
GetMouseCursorIntent(void)
{
    return g_ui_cursor_current;
}

static void
MarkTextCursor(void)
{
    ui_set_cursor_intent(MOUSE_CURSOR_IBEAM, CURSOR_PRIORITY_TEXT);
}

int
IsKeyboardInputEnabled(void)
{
    return KeyboardInputEnabled() && !ContentDisabled() &&
           !ui_popup_input_keyboard_captures();
}

int
ContentDisabled(void)
{
    return g_ui_disabled_start > 0;
}

void
DisabledScope(int disabled)
{
    g_ui_disabled_depth++;
    if(!disabled || g_ui_disabled_start > 0)
        return;
    g_ui_disabled_start = g_ui_disabled_depth;
    g_theme_content_alpha = 0.45f;
    ApplyCurrentTheme();
}

void
DisabledEndScope(void)
{
    if(g_ui_disabled_depth <= g_ui_disabled_floor)
        return;
    if(g_ui_disabled_depth-- != g_ui_disabled_start)
        return;
    g_ui_disabled_start = 0;
    g_theme_content_alpha = 1.0f;
    ApplyCurrentTheme();
}

static void
ui_reset_disabled_scope(void)
{
    g_ui_disabled_floor = 0;
    while(g_ui_disabled_depth > 0)
        DisabledEndScope();
    g_ui_disabled_start = 0;
}

DisabledScopeState
ui_disabled_suspend(void)
{
    DisabledScopeState scope = {g_ui_disabled_depth,g_ui_disabled_start,g_ui_disabled_floor};
    g_ui_disabled_depth = g_ui_disabled_floor = 1;
    g_ui_disabled_start = scope.start > 0 ? 1 : 0;
    return scope;
}

void
ui_disabled_resume(DisabledScopeState scope)
{
    if(g_ui_disabled_depth != 1 || g_ui_disabled_floor != 1 ||
       g_ui_disabled_start != (scope.start > 0 ? 1 : 0)) abort();
    g_ui_disabled_depth = scope.depth;
    g_ui_disabled_start = scope.start;
    g_ui_disabled_floor = scope.floor;
}

Rectangle
ScrollScope(Rectangle bounds, int content_height, int *scroll_offset)
{
    ScrollMetrics metrics = ui_scope_scroll_metrics();
    int max_scroll = ScrollMax(content_height, (int)bounds.height);
    int offset = 0;
    Rectangle content = bounds;
    Vector2 mouse = ui_mouse_world();
    if(scroll_offset != NULL) {
        *scroll_offset = ScrollClamp(*scroll_offset, max_scroll);
        if(CheckCollisionPointRec(mouse, bounds) && !InputCapturesClick(mouse) &&
           g_scroll_wheel_frame != g_ui_frame_serial && GetMouseWheelMove() != 0) {
            *scroll_offset = ScrollWheelOffsetFor(*scroll_offset,
                                                  GetMouseWheelMove(),
                                                  max_scroll,
                                                  metrics.default_wheel_step);
            g_scroll_wheel_frame = g_ui_frame_serial;
        }
        if(max_scroll > 0 && bounds.width > metrics.scrollbar_width &&
           bounds.height > 0) {
            int scrollbar_x = (int)(bounds.x + bounds.width) -
                              metrics.scrollbar_width;
            ScrollBarPaint paint = ScrollBarPaintFor(
                scrollbar_x, (int)bounds.y, (int)bounds.height,
                content_height, *scroll_offset, max_scroll, metrics);
            if(!ContentDisabled() && !InputCapturesClick(mouse) &&
               CheckCollisionPointRec(mouse, paint.track_bounds) &&
               IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                g_scroll_drag_offset = scroll_offset;
                g_scroll_drag_grab =
                    CheckCollisionPointRec(mouse, paint.thumb_bounds)
                        ? mouse.y - paint.thumb_bounds.y
                        : paint.thumb_bounds.height / 2;
            }
            if(g_scroll_drag_offset == scroll_offset) {
                if(ContentDisabled()) g_scroll_drag_offset = NULL;
                else if(paint.track_span > 0 &&
                        (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
                         IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
                    *scroll_offset = ScrollDragOffsetFor(
                        mouse.y, paint.track_bounds.y, g_scroll_drag_grab,
                        max_scroll, paint);
                if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    ConsumeRelease();
                    g_scroll_drag_offset = NULL;
                }
            }
            paint = ScrollBarPaintFor(scrollbar_x, (int)bounds.y,
                                      (int)bounds.height, content_height,
                                      *scroll_offset, max_scroll, metrics);
            if(IsWindowReady()) {
                Style track_style = ui_unpack_style(
                    ui_control_style_frame_kind(
                        (ButtonProps){.tone = ButtonToneNeutral,
                                      .emphasis = ButtonEmphasisSoft,
                                      .size = ControlSizeSmall},
                        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
                        StyleKindScroll()).value);
                Style thumb_style = ui_unpack_style(
                    ui_control_style_frame_kind(
                        (ButtonProps){.tone = ButtonToneAccent,
                                      .emphasis = ButtonEmphasisFilled,
                                      .size = ControlSizeSmall,
                                      .pill = 1},
                        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
                        StyleKindScrollThumb()).value);
                ui_draw_material(paint.track_bounds, (Rectangle){0},
                                 track_style.background, track_style.border,
                                 track_style.border, track_style.radius,
                                 track_style.border_width, 0.0f, 0.0f, 0,
                                 track_style.focus, 0.0f,
                                 track_style.opacity,
                                 ui_style_fill(track_style),
                                 track_style.material);
                ui_draw_material(paint.thumb_bounds, paint.track_bounds,
                                 thumb_style.background, thumb_style.border,
                                 thumb_style.border, thumb_style.radius,
                                 thumb_style.border_width, 0.0f, 0.0f, 0,
                                 thumb_style.focus, 0.0f,
                                 thumb_style.opacity,
                                 ui_style_fill(thumb_style),
                                 thumb_style.material);
            }
            content = ScrollScopeContentBounds(bounds, 1, metrics);
            bounds = content;
        }
        offset = *scroll_offset;
    }
    PushInputClip(bounds);
    if(IsWindowReady())
        BeginClip((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);
    g_scroll_scope_depth++;
    content.y -= offset;
    content.height = content_height > 0 ? (float)content_height : 0;
    return content;
}

void
ScrollEndScope(void)
{
    if(g_scroll_scope_depth <= 0)
        return;
    g_scroll_scope_depth--;
    if(IsWindowReady())
        EndClip();
    PopInputClip();
}

static int
ui_pointer_dx(void)
{
    Vector2 mouse = GetMousePosition();
    return (int)mouse.x - g_ui_pointer_start_x;
}

static int
ui_backspace_repeat_count(void)
{
    double now;
    int count = 0;

    if(IsKeyPressed(KEY_BACKSPACE)) {
        g_ui_backspace_next_repeat_at = GetTime() + 0.34;
        return 1;
    }
    if(!IsKeyDown(KEY_BACKSPACE)) {
        g_ui_backspace_next_repeat_at = 0.0;
        return 0;
    }
    now = GetTime();
    if(g_ui_backspace_next_repeat_at <= 0.0) {
        g_ui_backspace_next_repeat_at = now + 0.34;
        return 0;
    }
    while(now >= g_ui_backspace_next_repeat_at && count < 8) {
        count++;
        g_ui_backspace_next_repeat_at += 0.045;
    }
    return count;
}

static int
ui_pointer_dy(void)
{
    Vector2 mouse = GetMousePosition();
    return (int)mouse.y - g_ui_pointer_start_y;
}

static int
ui_mod_key_down(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
           IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

int
ui_pointer_drag_is_horizontal(void)
{
    return InputPointerDragIsHorizontal(ui_pointer_dx(), ui_pointer_dy());
}

static void
ui_update_pointer_gesture(void)
{
    Vector2 mouse = GetMousePosition();
    int mx = (int)mouse.x;
    int my = (int)mouse.y;
    int drag_threshold = InputPointerDragThresholdFor(
        (float)Scale(1000) / 1000.0f);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_ui_pointer_down = 1;
        g_ui_pointer_dragging = 0;
        g_ui_pointer_dragged_this_click = 0;
        g_ui_scroll_gesture_pending = 0;
        g_ui_release_consumed = 0;
        g_ui_pointer_owner = POINTER_OWNER_NONE;
        g_ui_pointer_start_x = mx;
        g_ui_pointer_start_y = my;
        g_ui_pointer_start_world = screen_to_world_for_input(mouse);
    } else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && g_ui_pointer_down) {
        int dx = ui_pointer_dx();
        int dy = ui_pointer_dy();
        if(InputPointerDragShouldStart(dx, dy, drag_threshold)) {
            g_ui_pointer_dragging = 1;
            g_ui_pointer_dragged_this_click = 1;
        }
    } else if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        /* A complete injected tap can be pumped before a UI frame. In that
         * case no press origin was observed, so activation falls back to the
         * release point just as it did before origin tracking existed. */
        if(!g_ui_pointer_down) {
            g_ui_pointer_start_x = INT_MIN;
            g_ui_pointer_start_y = INT_MIN;
        }
        g_ui_pointer_down = 0;
        g_ui_pointer_dragging = 0;
        g_ui_scroll_gesture_pending = 0;
        g_ui_pointer_owner = POINTER_OWNER_NONE;
    } else if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        g_ui_pointer_down = 0;
        g_ui_pointer_dragging = 0;
        g_ui_pointer_dragged_this_click = 0;
        g_ui_scroll_gesture_pending = 0;
        g_ui_release_consumed = 0;
        g_ui_pointer_owner = POINTER_OWNER_NONE;
        g_ui_pointer_start_x = INT_MIN;
        g_ui_pointer_start_y = INT_MIN;
    }
}

void
ClearInputCaptures(void)
{
    g_ui_input_capture_stack_count = 0;
}

void
PushInputCapture(Rectangle bounds, int allow_inside)
{
    if(g_ui_input_capture_stack_count >= INPUT_CAPTURE_STACK_MAX)
        return;
    g_ui_input_capture_stack[g_ui_input_capture_stack_count++] =
        (InputCapture){bounds, allow_inside != 0};
}

void
BeginModalLayer(void)
{
    ClearInputCaptures();
    PushInputCapture((Rectangle){0.0f, 0.0f,
                                   (float)ui_view_width,
                                   (float)ui_view_height}, 0);
}

InputClipScopeState
ui_input_clip_suspend(void)
{
    InputClipScopeState scope = {0};
    scope.count = g_ui_input_clip_stack_count;
    scope.scroll_depth = g_scroll_scope_depth;
    memcpy(scope.clips,g_ui_input_clip_stack,(size_t)scope.count*sizeof(Rectangle));
    g_ui_input_clip_stack_count = 0;
    g_scroll_scope_depth = 0;
    return scope;
}

void
ui_input_clip_resume(InputClipScopeState scope)
{
    if(scope.count < 0 || scope.count > INPUT_CLIP_STACK_MAX || scope.scroll_depth < 0 ||
       g_ui_input_clip_stack_count != 0 || g_scroll_scope_depth != 0) abort();
    memcpy(g_ui_input_clip_stack,scope.clips,(size_t)scope.count*sizeof(Rectangle));
    g_ui_input_clip_stack_count = scope.count;
    g_scroll_scope_depth = scope.scroll_depth;
}

int
ui_current_input_clip(Rectangle *bounds)
{
    if(g_ui_input_clip_stack_count <= 0) return 0;
    *bounds = g_ui_input_clip_stack[g_ui_input_clip_stack_count-1];
    return 1;
}

int
ui_base_input_captures_click(Vector2 point, int include_pointer_drag)
{
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && g_ui_release_consumed)
        return 1;

    if(g_ui_input_clip_stack_count > 0 &&
       !CheckCollisionPointRec(point, g_ui_input_clip_stack[g_ui_input_clip_stack_count - 1]))
        return 1;

    if(g_ui_input_capture_stack_count > 0) {
        InputCapture capture =
            g_ui_input_capture_stack[g_ui_input_capture_stack_count - 1];
        if(!capture.allow_inside || !CheckCollisionPointRec(point, capture.bounds))
            return 1;
    }

    return (include_pointer_drag && g_ui_pointer_dragging) ||
           (include_pointer_drag &&
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
            g_ui_pointer_dragged_this_click);
}

int
ui_input_captures_click_internal(Vector2 point, int include_pointer_drag)
{
    return ui_base_input_captures_click(point, include_pointer_drag) ||
           ui_popup_input_current_captures(point) ||
           dropdown_captures(point);
}

int
ui_input_captures_snapshot(Vector2 point, PopupInputToken snapshot)
{
    return ContentDisabled() || InspectInputCapturesClick(point) ||
           ui_base_input_captures_click(point,1) ||
           dropdown_captures(point) ||
           ui_popup_input_snapshot_captures(snapshot,point);
}

int
InputCapturesClick(Vector2 point)
{
    return ui_input_captures_snapshot(point,ui_popup_input_snapshot());
}

int
ReleaseConsumed(void)
{
    return g_ui_release_consumed;
}

void
ConsumeRelease(void)
{
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_release_consumed = 1;
}

int
PointerReleaseConsumed(void)
{
    return ReleaseConsumed();
}

void
ConsumePointerRelease(void)
{
    ConsumeRelease();
}

int
press_started_inside(Rectangle bounds)
{
    if(g_ui_pointer_start_x == INT_MIN && g_ui_pointer_start_y == INT_MIN)
        return 1;
    return CheckCollisionPointRec(g_ui_pointer_start_world, bounds);
}

static int
press_started_inside_circle(Vector2 center, float radius)
{
    if(g_ui_pointer_start_x == INT_MIN && g_ui_pointer_start_y == INT_MIN)
        return 1;
    float dx = g_ui_pointer_start_world.x - center.x;
    float dy = g_ui_pointer_start_world.y - center.y;

    return dx * dx + dy * dy <= radius * radius;
}

int
mouse_release_activates_rect(Rectangle bounds, Vector2 mouse, int active)
{
    return active &&
           IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
           !g_ui_release_consumed &&
           !InputCapturesClick(mouse) &&
           press_started_inside(bounds);
}

int
PointerReleaseAvailable(Vector2 point)
{
    return IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
           !InputCapturesClick(point);
}

int
HandleClick(Rectangle bounds, int disabled, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int mouse_inside = CheckCollisionPointRec(mouse_world, bounds);
    int captured = InputCapturesClick(mouse_world);
    InputPointerInteraction interaction = InputPointerInteractionFor(
        mouse_inside != 0, captured != 0, disabled != 0,
        HoverEffectsEnabled() != 0, IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0,
        g_ui_release_consumed != 0, press_started_inside(bounds) != 0);

    if(hover != NULL)
        *hover = interaction.hovered;
    if(interaction.disabled_marker)
        MarkDisabled();
    if(interaction.active)
        MarkClickable();
    if(interaction.activated) {
        ConsumeRelease();
        return 1;
    }
    return 0;
}

Activation
ReadActivation(Rectangle bounds, int id, bool enabled)
{
    Activation input = {0};
    int hovered = 0;
    input.activated = HandleClick(bounds, !enabled, &hovered);
    input.hovered = hovered;
    input.focused = enabled && id > 0 && RegisterFocus(id, bounds);
    int keyboard = IsFocusActivatePressed(id);
    input.pressed = (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ||
                    (input.focused && keyboard);
    input.activated = input.activated || keyboard;
    return input;
}

int
HandleCircleClick(Vector2 center, float radius, int disabled, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    float dx = mouse_world.x - center.x;
    float dy = mouse_world.y - center.y;
    int mouse_inside = dx * dx + dy * dy <= radius * radius;
    int captured = InputCapturesClick(mouse_world);
    InputPointerInteraction interaction = InputPointerInteractionFor(
        mouse_inside != 0, captured != 0, disabled != 0,
        HoverEffectsEnabled() != 0, IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0,
        g_ui_release_consumed != 0,
        press_started_inside_circle(center, radius) != 0);

    if(hover != NULL)
        *hover = interaction.hovered;
    if(interaction.disabled_marker)
        MarkDisabled();
    if(interaction.active)
        MarkClickable();
    if(interaction.activated) {
        ConsumeRelease();
        return 1;
    }
    return 0;
}

int
PointerReleaseOutside(Rectangle bounds)
{
    Vector2 mouse = ui_mouse_world();

    return PointerReleaseAvailable(mouse) &&
           !CheckCollisionPointRec(mouse, bounds);
}

int
HoverEffectsEnabled(void)
{
#if ANDROID_BUILD
    return 0;
#else
    return 1;
#endif
}

void
SetTransitionCuesEnabled(int enabled)
{
    g_ui_transition_cues_enabled = enabled ? 1 : 0;
}

int
TransitionCuesEnabled(void)
{
#if defined(KRYON_BACKEND_TERMI)
    return 0;
#else
    if(g_ui_transition_cues_enabled < 0)
        return DefaultMotionEnabled();
    return g_ui_transition_cues_enabled;
#endif
}

void
PushInputClip(Rectangle bounds)
{
    if(g_ui_input_clip_stack_count > 0)
        bounds = GetClipIntersection(g_ui_input_clip_stack[g_ui_input_clip_stack_count - 1],
                                         bounds);
    if(g_ui_input_clip_stack_count < INPUT_CLIP_STACK_MAX)
        g_ui_input_clip_stack[g_ui_input_clip_stack_count++] = bounds;
}

void
PopInputClip(void)
{
    if(g_ui_input_clip_stack_count > 0)
        g_ui_input_clip_stack_count--;
}

int
ui_caret_blink_visible(void)
{
    return ((int)(GetTime() * 2.0)) % 2 == 0;
}

void
ui_open_url(const char *url)
{
    if(url == NULL || url[0] == '\0')
        return;
#if defined(PLATFORM_WEB)
    EM_ASM({
        window.location.href = UTF8ToString($0);
    }, url);
#else
    (void)OpenURI(url);
#endif
}

int
ui_text_copy_range(const char *text, int start, int end)
{
    char *copy;
    int len;
    int text_len;
    TextSelectionRange range;

    if(text == NULL)
        return 0;
    text_len = (int)strlen(text);
    range = TextSelectionRangeForLength(start, end, text_len);
    start = range.start;
    end = range.end;
    if(end <= start)
        return 0;
    len = end - start;
    copy = malloc((size_t)len + 1);
    if(copy == NULL)
        return 0;
    memcpy(copy, text + start, (size_t)len);
    copy[len] = '\0';
    SetClipboardTextValue(copy);
    free(copy);
    return 1;
}

int
ui_text_paste_clipboard(TextEdit edit, int allow_newlines)
{
    const char *clip;

    if(edit.text == NULL || edit.text_size == 0 ||
       edit.cursor_position == NULL)
        return 0;
    clip = GetClipboardTextValue();
    if(clip == NULL)
        return 0;
    return ui_text_insert_text(edit.text, edit.text_size,
                               edit.cursor_position, clip, allow_newlines,
                               edit.filter, edit.filter_user_data,
                               edit.max_codepoints);
}

static void
ui_selection_range(TextSelection selection, const char *text,
                   int *start, int *end)
{
    int len = text != NULL ? (int)strlen(text) : 0;
    TextSelectionRange range = TextSelectionRangeForLength(selection.anchor,
                                                           selection.cursor,
                                                           len);

    if(start != NULL)
        *start = range.start;
    if(end != NULL)
        *end = range.end;
}

static int
ui_clipboard_has_text(void)
{
    const char *text = GetClipboardTextValue();

    return text != NULL && text[0] != '\0';
}

static int
ui_text_context_matches(int kind, int id, int *owner)
{
    return g_ui_text_context_open &&
           g_ui_text_context_kind == kind &&
           g_ui_text_context_owner == owner &&
           (id <= 0 || g_ui_text_context_id == id);
}

static void
ui_text_context_close(void)
{
    g_ui_text_context_open = 0;
    g_ui_text_context_kind = TEXT_CONTEXT_NONE;
    g_ui_text_context_id = 0;
    g_ui_text_context_owner = NULL;
    g_ui_text_context_target_frame = 0;
}

static void
ui_text_context_clear_target(void)
{
    g_ui_text_context_text = NULL;
    g_ui_text_context_text_size = 0;
    g_ui_text_context_cursor = NULL;
    g_ui_text_context_max_codepoints = 0;
    g_ui_text_context_filter = NULL;
    g_ui_text_context_filter_user_data = NULL;
    g_ui_text_context_selection = NULL;
    g_ui_text_context_selection_start = 0;
    g_ui_text_context_selection_end = 0;
    g_ui_text_context_allow_newlines = 0;
    g_ui_text_context_copy_all_when_empty = 0;
    g_ui_text_context_read_only = 0;
    g_ui_text_context_target_frame = 0;
}

static int
ui_text_context_open_for(int kind, int id, int *owner,
                         Rectangle bounds, Vector2 mouse, int captured)
{
#if ANDROID_BUILD
    (void)kind;
    (void)id;
    (void)owner;
    (void)bounds;
    (void)mouse;
    (void)captured;
    return 0;
#else
    if(!CheckCollisionPointRec(mouse, bounds) || captured ||
       !IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        return 0;
    g_ui_text_context_open = 1;
    g_ui_text_context_kind = kind;
    g_ui_text_context_id = id;
    g_ui_text_context_owner = owner;
    g_ui_text_context_x = (int)mouse.x;
    g_ui_text_context_y = (int)mouse.y;
    return 1;
#endif
}

static int
ui_text_context_take_changed(int kind, int id, int *owner)
{
    if(!g_ui_text_context_changed ||
       g_ui_text_context_changed_kind != kind ||
       g_ui_text_context_changed_owner != owner ||
       (id > 0 && g_ui_text_context_changed_id != id))
        return 0;
    g_ui_text_context_changed = 0;
    g_ui_text_context_changed_kind = TEXT_CONTEXT_NONE;
    g_ui_text_context_changed_id = 0;
    g_ui_text_context_changed_owner = NULL;
    return 1;
}

static void
ui_text_context_register_target(int kind, int id, int *owner,
                                char *text, size_t text_size,
                                int *cursor_position, int max_codepoints,
                                TextInputFilter filter,
                                void *filter_user_data,
                                TextSelection *selection,
                                int selection_start, int selection_end,
                                int allow_newlines,
                                int copy_all_when_empty,
                                int read_only)
{
    if(!ui_text_context_matches(kind, id, owner))
        return;
    g_ui_text_context_text = text;
    g_ui_text_context_text_size = text_size;
    g_ui_text_context_cursor = cursor_position;
    g_ui_text_context_max_codepoints = max_codepoints;
    g_ui_text_context_filter = filter;
    g_ui_text_context_filter_user_data = filter_user_data;
    g_ui_text_context_selection = selection;
    g_ui_text_context_selection_start = selection_start;
    g_ui_text_context_selection_end = selection_end;
    g_ui_text_context_allow_newlines = allow_newlines;
    g_ui_text_context_copy_all_when_empty = copy_all_when_empty;
    g_ui_text_context_read_only = read_only;
    g_ui_text_context_target_frame = g_ui_frame_serial;
}

static int
ui_text_apply_context_command(int command, TextEdit edit,
                              TextSelection *selection, int selection_id,
                              int *selection_owner, int selection_start,
                              int selection_end, int allow_newlines,
                              int copy_all_when_empty, int read_only)
{
    int changed = 0;
    int len;
    int has_selection;
    int copied_selection = 0;
    TextContextCommandDecision decision;

    if(edit.text == NULL || edit.text_size == 0 ||
       edit.cursor_position == NULL || selection == NULL)
        return 0;
    len = (int)strlen(edit.text);
    {
        TextSelectionRange range = TextSelectionRangeForLength(
            selection_start, selection_end, len);
        selection_start = range.start;
        selection_end = range.end;
    }
    has_selection = selection_end > selection_start;
    decision = TextContextCommandDecisionFor(
        ui_text_context_command_kind(command), has_selection,
        copy_all_when_empty != 0, len > 0, read_only != 0);

    if(decision.copy_selection)
        copied_selection = ui_text_copy_range(edit.text, selection_start,
                                              selection_end);
    if(decision.copy_all)
        SetClipboardTextValue(edit.text);
    if(decision.delete_selection &&
       (!decision.copy_selection || copied_selection) &&
       ui_text_delete_range(edit.text, edit.text_size, edit.cursor_position,
                            selection_start, selection_end))
        changed |= !decision.paste;
    if(decision.clear_all) {
        edit.text[0] = '\0';
        *edit.cursor_position = 0;
        changed = 1;
    }
    if(decision.paste && ui_text_paste_clipboard(edit, allow_newlines))
        changed = 1;
    if(decision.select_all) {
        ui_text_selection_set(selection, selection_id, selection_owner,
                              0, len, 0);
        *edit.cursor_position = len;
    }
    if(decision.collapse_selection) {
        ui_text_selection_set(selection, selection_id, selection_owner,
                              *edit.cursor_position, *edit.cursor_position, 0);
    }
    return changed;
}

static int
ui_text_draw_context_overlay(void)
{
#if ANDROID_BUILD
    return 0;
#else
    MenuItem items[4];
    int has_text = g_ui_text_context_text != NULL &&
                   g_ui_text_context_text[0] != '\0';
    int has_selection = g_ui_text_context_selection_end >
                        g_ui_text_context_selection_start;
    TextContextMenuState menu_state;
    int command;
    MenuProps menu;
    TextEdit edit;
    int changed;

    if(!g_ui_text_context_open)
        return 0;
    if(g_ui_text_context_target_frame != g_ui_frame_serial ||
       g_ui_text_context_text == NULL ||
       g_ui_text_context_cursor == NULL ||
       g_ui_text_context_selection == NULL) {
        ui_text_context_close();
        ui_text_context_clear_target();
        return 0;
    }

    menu_state = TextContextMenuStateFor(
        has_selection,
        g_ui_text_context_copy_all_when_empty != 0,
        has_text != 0,
        g_ui_text_context_read_only != 0,
        ui_clipboard_has_text() != 0);
    items[0] = (MenuItem){MenuCommand, "Cut", "Ctrl+X",
                            TEXT_CONTEXT_CUT,
                            !menu_state.cut_enabled,
                            0, NULL, 0};
    items[1] = (MenuItem){MenuCommand, "Copy", "Ctrl+C",
                            TEXT_CONTEXT_COPY,
                            !menu_state.copy_enabled,
                            0, NULL, 0};
    items[2] = (MenuItem){MenuCommand, "Paste", "Ctrl+V",
                            TEXT_CONTEXT_PASTE,
                            !menu_state.paste_enabled, 0, NULL, 0};
    items[3] = (MenuItem){MenuCommand, "Select All", "Ctrl+A",
                            TEXT_CONTEXT_SELECT_ALL,
                            !menu_state.select_all_enabled, 0, NULL, 0};

    memset(&menu, 0, sizeof(menu));
    menu.id = 8500 + g_ui_text_context_kind;
    menu.mode = MenuModeContext;
    menu.trigger = ui_rect(-10000.0f, -10000.0f, 1.0f, 1.0f);
    menu.items = items;
    menu.item_count = 4;
    menu.open = &g_ui_text_context_open;
    menu.x = &g_ui_text_context_x;
    menu.y = &g_ui_text_context_y;
    command = RenderMenu(menu).activated_id;
    if(command != 0) {
        memset(&edit, 0, sizeof(edit));
        edit.text = g_ui_text_context_text;
        edit.text_size = g_ui_text_context_text_size;
        edit.cursor_position = g_ui_text_context_cursor;
        edit.max_codepoints = g_ui_text_context_max_codepoints;
        edit.filter = g_ui_text_context_filter;
        edit.filter_user_data = g_ui_text_context_filter_user_data;
        changed = ui_text_apply_context_command(
            command,
            edit,
            g_ui_text_context_selection,
            g_ui_text_context_id,
            g_ui_text_context_owner,
            g_ui_text_context_selection_start,
            g_ui_text_context_selection_end,
            g_ui_text_context_allow_newlines,
            g_ui_text_context_copy_all_when_empty,
            g_ui_text_context_read_only);
        if(changed) {
            g_ui_text_context_changed = 1;
            g_ui_text_context_changed_kind = g_ui_text_context_kind;
            g_ui_text_context_changed_id = g_ui_text_context_id;
            g_ui_text_context_changed_owner = g_ui_text_context_owner;
        }
        ui_text_context_close();
        ui_text_context_clear_target();
    }
    return command;
#endif
}

static Rectangle
ui_world_rect_to_screen(Rectangle rect)
{
    return (Rectangle){
        g_ui_camera.offset.x + rect.x * g_ui_camera.zoom,
        g_ui_camera.offset.y + rect.y * g_ui_camera.zoom,
        rect.width * g_ui_camera.zoom,
        rect.height * g_ui_camera.zoom
    };
}

void
ui_begin_world_clip(Rectangle rect)
{
    Rectangle screen = ui_world_rect_to_screen(rect);
    BeginClip((int)screen.x, (int)screen.y,
                     (int)screen.width, (int)screen.height);
}

static void
ui_draw_text_centered_in_rect(const char *text, Rectangle rect, int font_size, Color color)
{
    const char *value = text != NULL ? text : "";
    float scale = (float)Scale(1000) / 1000.0f;
    int text_w = TextWidth(value, font_size);
    int x = (int)(rect.x + (rect.width - (float)text_w) * 0.5f);
    int y = ControlTextBaselineY(value, (int)rect.y, (int)rect.height,
                                 font_size);

    ui_begin_world_clip(TextControlClipBounds(rect, scale));
    RenderText(value, x, y, font_size, color);
    EndClip();
}

const char *
ui_inspect_control_id(char *buf, size_t buf_size, const char *kind,
                     int numeric_id, const char *label)
{
    if(numeric_id > 0) {
        snprintf(buf, buf_size, "%s:%d", kind, numeric_id);
        return buf;
    }
    if(label != NULL && label[0] != '\0') {
        snprintf(buf, buf_size, "tmp:%s:%s", kind, label);
        return buf;
    }
    snprintf(buf, buf_size, "tmp:%s", kind);
    return buf;
}

static int
ui_text_next_smaller_size(int font_size)
{
    int body = Scale(Text16);
    int small = Scale(Text14);
    int caption = Scale(Text12);
    int minimum = Scale(Text8);

    if(font_size > body)
        return body;
    if(font_size > small)
        return small;
    if(font_size > caption)
        return caption;
    return minimum;
}

static int
ui_text_normalize_size(int font_size)
{
    int minimum = Scale(Text8);
    int caption = Scale(Text12);
    int small = Scale(Text14);
    int body = Scale(Text16);

    if(font_size == Text8)
        return minimum;
    if(font_size == Text12)
        return caption;
    if(font_size == Text14)
        return small;
    if(font_size == Text16)
        return body;
    if(font_size == Text24)
        return Scale(Text24);
    if(font_size <= minimum)
        return minimum;
    if(font_size <= caption)
        return caption;
    if(font_size <= small)
        return small;
    if(font_size <= body)
        return body;
    return Scale(Text24);
}

int
FitFontSize(const char *text, int max_width,
            int preferred_size, int min_size)
{
    const char *value = text != NULL ? text : "";
    int font_size = ui_text_normalize_size(preferred_size);
    int min_allowed = ui_text_normalize_size(min_size);

    if(font_size < min_allowed)
        font_size = min_allowed;
    while(font_size > min_allowed && TextWidth(value, font_size) > max_width)
        font_size = ui_text_next_smaller_size(font_size);
    return font_size;
}

void
RenderControlTextInRect(const char *text, Rectangle rect, int font_size,
                        Color color)
{
    const char *value = text != NULL ? text : "";
    float scale = (float)Scale(1000) / 1000.0f;
    int y = ControlTextBaselineY(value, (int)rect.y, (int)rect.height,
                                 font_size);

    ui_begin_world_clip(TextControlClipBounds(rect, scale));
    RenderText(value, (int)rect.x, y, font_size, color);
    EndClip();
}

void
DrawFittedTextInRect(const char *text, Rectangle rect,
                            int preferred_size, int min_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int font_size = FitFontSize(value, (int)rect.width,
                                preferred_size, min_size);
    ui_draw_text_centered_in_rect(value, rect, font_size, color);
}

void
QueueTextInputCodepoint(int codepoint)
{
    if(codepoint <= 0)
        return;
    if(g_ui_text_input_codepoint_count >= TEXT_INPUT_QUEUE_MAX)
        return;
    g_ui_text_input_codepoints[g_ui_text_input_codepoint_count++] = codepoint;
}

void
QueueTextInputBackspace(void)
{
    if(g_ui_text_input_backspace_count < TEXT_INPUT_QUEUE_MAX)
        g_ui_text_input_backspace_count++;
}

void
QueueTextInputEnter(void)
{
    if(g_ui_text_input_enter_count < TEXT_INPUT_QUEUE_MAX)
        g_ui_text_input_enter_count++;
}

void
BeginFocusScope(void)
{
    g_ui_focus_count = 0;
    g_ui_focus_tab_dir = 0;
    g_ui_focus_frame_open = 1;
    /* Advance the frame counter and release ownership if the current owner did
     * not render last frame (the user navigated away or the widget is gone).
     * This lets a new screens autofocus adopt cleanly, while still preventing
     * two live widgets from both showing a caret within a single frame. */
    g_ui_text_focus_frame++;
    if(TextFocusOwnerIsStale(g_ui_text_focus_owner != NULL,
                             (int64_t)g_ui_text_focus_owner_frame,
                             (int64_t)g_ui_text_focus_frame))
        g_ui_text_focus_owner = NULL;
    g_ui_text_focus_owner_this_frame = NULL;
    if(IsKeyPressed(KEY_TAB))
        g_ui_focus_tab_dir = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? -1 : 1;
}

void
ui_consume_focus_tab(void)
{
    g_ui_focus_tab_dir = 0;
}

void
EndFocusScope(void)
{
    int current_index = -1;
    int next_index;

    if(!g_ui_focus_frame_open)
        return;

    g_ui_focus_frame_open = 0;

    /* Missing popup owners must stop blocking traversal before choosing the
     * next focus target, not only when their paint resources are retired. */
    ui_popup_input_retire_missing(ui_popup_input_bound());
    int eligible = 0;
    for(int i = 0; i < g_ui_focus_count; i++) {
        int id = g_ui_focus_ids[i], duplicate = 0;
        if(ui_popup_input_focus_captures(id)) continue;
        for(int j = 0; j < eligible; j++)
            if(g_ui_focus_ids[j] == id) { duplicate = 1; break; }
        if(!duplicate) g_ui_focus_ids[eligible++] = id;
    }
    g_ui_focus_count = eligible;

    if(g_ui_focus_count <= 0) {
        g_ui_focus_active_id = 0;
        g_ui_focus_tab_dir = 0;
        return;
    }

    if(g_ui_focus_tab_dir == 0)
        return;

    for(int i = 0; i < g_ui_focus_count; i++) {
        if(g_ui_focus_ids[i] == g_ui_focus_active_id) {
            current_index = i;
            break;
        }
    }

    if(current_index < 0)
        next_index = g_ui_focus_tab_dir > 0 ? 0 : g_ui_focus_count - 1;
    else
        next_index = (current_index + g_ui_focus_tab_dir + g_ui_focus_count) % g_ui_focus_count;

    g_ui_focus_active_id = g_ui_focus_ids[next_index];
    g_ui_focus_tab_dir = 0;
}

int
FocusFrameOpen(void)
{
    return g_ui_focus_frame_open;
}

int
ui_register_focus_snapshot(int id, Rectangle bounds, PopupInputToken snapshot)
{
    Vector2 mouse_world;
    Vector2 focus_point;

    if(id <= 0)
        return 0;

    focus_point = (Vector2){bounds.x+bounds.width*0.5f,
                            bounds.y+bounds.height*0.5f};
    ui_popup_input_register_focus(id,snapshot,
        !ContentDisabled() && !InspectInputCapturesClick(focus_point) &&
        !ui_base_input_captures_click(focus_point,0) &&
        !dropdown_captures(focus_point) &&
        !ui_popup_input_snapshot_captures(snapshot,focus_point));
    if(g_ui_focus_count < FOCUS_MAX_ITEMS)
        g_ui_focus_ids[g_ui_focus_count++] = id;

    mouse_world = ui_mouse_world();
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
       CheckCollisionPointRec(mouse_world, bounds) &&
       !ui_input_captures_snapshot(mouse_world,snapshot))
        g_ui_focus_active_id = id;

    return g_ui_focus_active_id == id;
}

int
RegisterFocus(int id, Rectangle bounds)
{
    return ui_register_focus_snapshot(id,bounds,ui_popup_input_snapshot());
}

static void
ClaimTextFocus(int *focused)
{
    TextFocusClaimDecision decision = TextFocusClaimDecisionFor(
        focused != NULL, g_ui_text_focus_owner != NULL,
        g_ui_text_focus_owner == focused);

    if(!decision.claim)
        return;
    /* Displace any previous owner first by clearing its flag, so its caret
     * disappears immediately this frame even though it has not re-run yet. */
    if(decision.displace_previous)
        *g_ui_text_focus_owner = 0;
    if(decision.cancel_previous)
        ui_text_composition_cancel(g_ui_text_focus_owner);
    if(decision.close_context)
        ui_text_context_close();
    g_ui_text_focus_owner = focused;
    g_ui_text_focus_owner_this_frame = focused;
    g_ui_text_focus_owner_frame = g_ui_text_focus_frame;
    *focused = 1;
}

static void
ClaimTextFieldFocus(int *focused)
{
    int *previous = g_ui_text_focus_owner;
    TextFocusClaimDecision decision = TextFocusClaimDecisionFor(
        focused != NULL, previous != NULL, previous == focused);

    ClaimTextFocus(focused);
    if(g_ui_text_focus_owner == focused && decision.clear_peer_selection)
        ui_clear_text_area_selection();
}

static void
ClaimTextAreaFocus(int *focused)
{
    int *previous = g_ui_text_focus_owner;
    TextFocusClaimDecision decision = TextFocusClaimDecisionFor(
        focused != NULL, previous != NULL, previous == focused);

    ClaimTextFocus(focused);
    if(g_ui_text_focus_owner == focused && decision.clear_peer_selection)
        ui_clear_text_field_selection();
}

static void
ReleaseTextFocus(int *focused, int focus_id)
{
    TextFocusReleaseDecision decision = TextFocusReleaseDecisionFor(
        focused != NULL, g_ui_text_focus_owner == focused,
        g_ui_text_focus_owner_this_frame == focused,
        focus_id > 0 && g_ui_focus_active_id == focus_id,
        g_ui_text_field_drag_owner == focused,
        g_ui_text_area_drag_owner == focused);

    if(!decision.release)
        return;
    if(decision.cancel_self)
        ui_text_composition_cancel(focused);
    if(decision.clear_owner)
        g_ui_text_focus_owner = NULL;
    if(decision.clear_frame_owner)
        g_ui_text_focus_owner_this_frame = NULL;
    if(decision.clear_active_focus)
        g_ui_focus_active_id = 0;
    *focused = 0;
    if(decision.close_context)
        ui_text_context_close();
    if(decision.clear_field_drag) {
        g_ui_text_field_drag_id = 0;
        g_ui_text_field_drag_owner = NULL;
        g_ui_text_field_selection.dragging = 0;
    }
    if(decision.clear_area_drag) {
        g_ui_text_area_drag_id = 0;
        g_ui_text_area_drag_owner = NULL;
        g_ui_text_area_selection.dragging = 0;
    }
}

void
ClearTextInputFocus(void)
{
    if(g_ui_text_focus_owner != NULL)
        *g_ui_text_focus_owner = 0;
    g_ui_text_focus_owner = NULL;
    g_ui_text_focus_owner_this_frame = NULL;
    g_ui_text_focus_owner_frame = 0;
    g_ui_focus_text_input_active = 0;
    g_ui_text_input_requested = 0;
    g_ui_text_input_show_requested = 0;
    ui_text_composition_cancel(NULL);
    ClearTextComposition();
    ui_text_context_close();
    ui_clear_text_field_selection();
    ui_clear_text_area_selection();
}

static int
IsTextFocusOwner(int *focused)
{
    TextFocusOwnerDecision decision = TextFocusOwnerDecisionFor(
        focused != NULL, g_ui_text_focus_owner == focused,
        g_ui_text_focus_owner_this_frame != NULL,
        focused != NULL && *focused != 0,
        g_ui_text_focus_owner != NULL);

    /* Strict single-owner enforcement. A text input is focused if and only if
     * it is the recorded owner. Ownership survives across frames (so a stale
     * per-widget `focused` flag can never revive a caret that another input
     * owns) but is auto-released if the owner stops rendering (navigation
     * away), letting a new screen autofocus cleanly. Within one frame, only the
     * first widget to claim/adopt wins; every other text input is denied. */
    if(focused == NULL)
        return 0;
    if(decision.mark_frame_owner) {
        if(decision.adopt_owner)
            g_ui_text_focus_owner = focused;
        g_ui_text_focus_owner_this_frame = focused;
        g_ui_text_focus_owner_frame = g_ui_text_focus_frame;
    }
    if(decision.clear_target)
        *focused = 0;
    return decision.focused ? 1 : 0;
}

int
IsFocusActive(int id)
{
    return id > 0 && g_ui_focus_active_id == id;
}

int
IsFocusActivatePressed(int id)
{
    return FocusActivationFor(IsFocusActive(id) != 0,
                              IsKeyboardInputEnabled() != 0,
                              ContentDisabled() != 0,
                              ui_popup_input_focus_captures(id) != 0,
                              IsKeyPressed(KEY_ENTER) != 0,
                              IsKeyPressed(KEY_SPACE) != 0,
                              g_ui_focus_text_input_active != 0);
}

void
SetFocus(int id)
{
    g_ui_focus_active_id = id;
}

int
GetFocus(void)
{
    return g_ui_focus_active_id;
}

void
ClearFocus(void)
{
    g_ui_focus_active_id = 0;
}

void
SetFocusTextInputActive(int active)
{
    active = active != 0;
    if(active) {
        g_ui_focus_text_input_active = 1;
        g_ui_text_input_requested = 1;
    } else {
        g_ui_focus_text_input_active = 0;
        g_ui_text_input_requested = 0;
    }
}

int
TextInputActive(void)
{
    return g_ui_focus_text_input_active != 0;
}

void
RenderFocus(Rectangle bounds)
{
    StyleFrame frame = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneAccent,
                      .emphasis = ButtonEmphasisOutline},
        ButtonStateFocus, 0, 0.0f, 0.0f, 1.0f, StyleKindFocus(),
        FocusBoxRole());
    FocusPaint paint = FocusPaintFor(bounds, (float)GetScale(), frame);
    Style style = ui_unpack_style(frame.value);
    Color focus = style.focus.a != 0 ? style.focus : style.border;

    DrawRectangleLinesEx(paint.bounds, (float)paint.stroke_width,
                         focus.a != 0 ? focus : c_button_hover);
}

int
GetFontSize(void)
{
    return Scale(Text16);
}

int
GetSmallFontSize(void)
{
    return Scale(Text14);
}

int
GetTitleFontSize(const char *title, int max_width)
{
    const char *value = title != NULL ? title : "";
    int large = Scale(Text24);
    int medium = Scale(Text16);
    int small = Scale(Text14);

    if(max_width <= 0 || TextWidth(value, large) <= max_width)
        return large;
    if(TextWidth(value, medium) <= max_width)
        return medium;
    return small;
}

int
ControlTextBaselineY(const char *text, int box_y, int box_h, int font)
{
    (void)text;
    return TextBaselineY(TextControlBaselineSample(), box_y, box_h, font);
}

int
ui_text_input_default_font(int style_kind, int class_name)
{
    Style resolved = ui_unpack_style(ui_style_apply_effects_data(
        ResolveActiveStyle(
            ui_pack_style_states((ControlStyle){.normal = {
                .fields = StyleOpacity,
                .opacity = 1.0f
            }}).normal,
            StyleControlFacts(style_kind, 0, class_name, ButtonToneNeutral,
                              ButtonEmphasisSoft, ControlSizeMedium,
                              ButtonStateNormal),
            ButtonStateNormal)));
    return ResolveFont(0, StyleFontValue(resolved.fields,
                                         resolved.font_size), GetFontSize());
}

TextInputStyle
ui_resolve_text_input_style(TextInputStyle style, int style_kind,
                            int class_name)
{
    uint32_t requested_fields = style.fields;
    Style resolved = ui_unpack_style(ui_style_apply_effects_data(
        ResolveActiveStyle(
            ui_pack_style_states((ControlStyle){.normal = {
                .fields = StyleOpacity,
                .opacity = 1.0f
            }}).normal,
            StyleControlFacts(style_kind, 0, class_name, ButtonToneNeutral,
                              ButtonEmphasisSoft, ControlSizeMedium,
                              ButtonStateNormal),
            ButtonStateNormal)));

    if(style.padding_x > 0)
        requested_fields |= StylePaddingX;
    if(style.padding_y > 0)
        requested_fields |= StylePaddingY;
    if(style.line_gap >= 0)
        requested_fields |= StyleGap;

    if(style.background.a == 0)
        style.background = resolved.background;
    if(style.border.a == 0)
        style.border = resolved.border;
    if(style.focus_border.a == 0)
        style.focus_border = resolved.focus;
    if(style.text.a == 0)
        style.text = resolved.foreground;
    if(style.cursor.a == 0)
        style.cursor = resolved.focus.a != 0 ? resolved.focus
                                             : resolved.foreground;
    if(style.radius <= 0.0f)
        style.radius = resolved.radius;
    style.fields = requested_fields | resolved.fields;
    if((requested_fields & StylePaddingX) != 0) {
        if(style.padding_x < 0)
            style.padding_x = 0;
    } else if((resolved.fields & StylePaddingX) != 0 &&
              resolved.padding_x >= 0.0f) {
        style.padding_x = Scale((int)(resolved.padding_x + 0.5f));
    }
    if((requested_fields & StylePaddingY) != 0) {
        if(style.padding_y < 0)
            style.padding_y = 0;
    } else if((resolved.fields & StylePaddingY) != 0 &&
              resolved.padding_y >= 0.0f) {
        style.padding_y = Scale((int)(resolved.padding_y + 0.5f));
    }
    if((requested_fields & StyleGap) != 0) {
        if(style.line_gap < 0)
            style.line_gap = 0;
    } else if((resolved.fields & StyleGap) != 0) {
        style.line_gap = resolved.gap > 0.0f
            ? Scale((int)(resolved.gap + 0.5f)) : 0;
    } else if(style.line_gap <= 0) {
        style.line_gap = -1;
    }
    return style;
}

TextInputMetrics
ui_text_input_metrics_for_style(TextInputStyle style, int style_kind,
                                int class_name, int default_line_gap)
{
    float scale = (float)Scale(1000) / 1000.0f;

    return TextInputMetricsFor(
        style.fields, 0, style.padding_x, style.padding_y, style.line_gap,
        ui_text_input_default_font(style_kind, class_name),
        TextInputDefaultPaddingX(scale), TextInputDefaultPaddingY(scale),
        default_line_gap);
}

static int
ui_text_input_style_kind(const char *kind)
{
    if(kind != NULL && strcmp(kind, "TextArea") == 0)
        return StyleKindTextArea();
    return StyleKindTextField();
}

static uint64_t
ui_text_input_motion_key(Rectangle bounds, int focus_id, const char *kind)
{
    uint64_t key = UINT64_C(1469598103934665603);

    while(kind != NULL && *kind != '\0') {
        key ^= (unsigned char)*kind++;
        key *= UINT64_C(1099511628211);
    }
    key ^= (uint64_t)(uint32_t)focus_id;
    key *= UINT64_C(1099511628211);
    key ^= (uint64_t)(int)bounds.x;
    key *= UINT64_C(1099511628211);
    key ^= (uint64_t)(int)bounds.y;
    key *= UINT64_C(1099511628211);
    key ^= (uint64_t)(int)bounds.width;
    key *= UINT64_C(1099511628211);
    key ^= (uint64_t)(int)bounds.height;
    key *= UINT64_C(1099511628211);
    return key != 0 ? key : 1;
}

static Rectangle
ui_text_input_surface(Rectangle bounds, TextInputStyle style, int focused,
                      int editable, int focus_id, const char *kind,
                      TextInputStyle requested, int class_name)
{
    int disabled = ContentDisabled();
    int hovered = 0;
    int style_kind = ui_text_input_style_kind(kind);

    if(ui_default_style()) {
        ThemeMetrics metrics = GetThemeMetrics();
        ButtonProps props = {.bounds = bounds, .id = focus_id,
                             .tone = ButtonToneNeutral,
                             .emphasis = ButtonEmphasisSoft,
                             .disabled = disabled,
                             .class_name = class_name};
        Style paint = ui_resolve_button_style_kind(
            props, disabled ? ButtonStateDisabled
                            : (focused ? ButtonStateFocus
                                       : ButtonStateNormal),
            style_kind);
        FillStates fill_states;
        Vector2 mouse = ui_mouse_world();
        Color border = focused ? style.focus_border : style.border;
        Activation sample;
        ButtonInput input;
        InteractionMotion motion;

        if(requested.background.a != 0)
            paint.background = style.background;
        if(requested.border.a != 0 ||
           (focused && requested.focus_border.a != 0))
            paint.border = border;
        if(requested.focus_border.a != 0)
            paint.focus = style.focus_border;
        if(requested.text.a != 0)
            paint.foreground = style.text;
        paint.opacity = 1.0f;

        hovered = editable &&
                  CheckCollisionPointRec(mouse, bounds) &&
                  !InputCapturesClick(mouse) &&
                  HoverEffectsEnabled();
        sample.hovered = hovered;
        sample.pressed = 0;
        sample.focused = focused;
        sample.activated = 0;
        input = ResolveButtonInput((int)props.state, props.disabled,
            props.loading, props.selected, sample);
        motion = AdvanceButtonMotion(
            ui_text_input_motion_key(bounds, focus_id, kind), (int)props.state, input,
            TransitionCuesEnabled(), GetFrameTime() * 1000.0f,
            metrics.transition_normal_ms, metrics.transition_fast_ms);
        if(motion.active)
            InvalidateTree(INVALIDATE_PAINT);
        paint = ui_style_transition(
            paint,
            ui_resolve_button_style_kind(props, ButtonStateNormal, style_kind),
            ui_resolve_button_style_kind(props, ButtonStateHover, style_kind),
            ui_resolve_button_style_kind(props, ButtonStatePressed, style_kind),
            ui_resolve_button_style_kind(props, ButtonStateFocus, style_kind),
            motion.hover.value, motion.press.value, motion.focus.value,
            &fill_states);
        if(requested.background.a != 0)
            paint.background = style.background;
        if(requested.border.a != 0 ||
           (focused && requested.focus_border.a != 0))
            paint.border = border;
        if(requested.focus_border.a != 0)
            paint.focus = style.focus_border;
        if(requested.text.a != 0)
            paint.foreground = style.text;
        paint.opacity = 1.0f;
        fill_states = ui_style_fill(paint);
        return ui_draw_material(
            bounds, (Rectangle){0}, paint.background, paint.border,
            paint.border, paint.radius, paint.border_width,
            motion.hover.value, motion.press.value, disabled, paint.focus,
            motion.focus.value, paint.opacity, fill_states, paint.material);
    }

    ui_draw_box_background(bounds,
                           style.radius >= 0.0f ? style.radius : 0.12f,
                           style.background,
                           focused ? style.focus_border : style.border);
    return bounds;
}

static int ui_text_width_before_cursor(const char *text, int font,
                                       int cursor_position);

static void
RenderTextInputEx(Rectangle bounds, const char *text, int cursor_position,
                  int focused, int text_input_active, int cursor_visible, int font,
                  TextInputStyle style, int selection_start,
                  int selection_end, int composition_start,
                  int composition_end, int scroll_x, int focus_id,
                  int class_name)
{
    TextInputStyle requested_style = style;

    style = ui_resolve_text_input_style(style, StyleKindTextField(),
                                        class_name);
    const char *value = text ? text : "";
    int y = (int)bounds.y;
    int h = (int)bounds.height;
    TextInputMetrics metrics = ui_text_input_metrics_for_style(
        style, StyleKindTextField(), class_name, 0);
    int padding_x = metrics.padding_x;
    float scale = (float)Scale(1000) / 1000.0f;
    int stroke_width = TextInputStrokeWidth(scale);
    int clip_guard = TextFieldClipGuard(scale);
    TextFieldPaint paint = TextFieldPaintFor(bounds, padding_x, scroll_x, font,
                                             TextLineHeight(font),
                                             TextFieldMinCursorHeight(scale),
                                             TextFieldCursorVerticalPadding(scale),
                                             clip_guard);
    int text_x = paint.text_x;
    int text_y = ControlTextBaselineY(value, y, h, font);
    int cursor_h = paint.cursor_height;
    int cursor_y = paint.cursor_y;
    Color text_color = style.text.a != 0 ? style.text : c_text;
    Color cursor_color = style.cursor.a != 0 ? style.cursor : c_circle;

    if(focused && text_input_active)
        SetFocusTextInputActive(1);

    (void)ui_text_input_surface(bounds, style, focused, text_input_active,
                                focus_id, "TextField", requested_style,
                                class_name);

    ui_begin_world_clip(paint.clip_bounds);
    if(selection_end > selection_start) {
        char prefix[1024];
        char selected_text[1024];
        int len = (int)strlen(value);
        TextSelectionPaintSpan span;
        int prefix_len;
        int selected_len;
        int sel_x;
        int sel_w;

        span = TextSelectionPaintSpanForText(selection_start, selection_end,
                                             len);
        if(span.visible) {
            prefix_len = span.start;
            selected_len = span.end - span.start;
            if(prefix_len >= (int)sizeof(prefix))
                prefix_len = (int)sizeof(prefix) - 1;
            if(selected_len >= (int)sizeof(selected_text))
                selected_len = (int)sizeof(selected_text) - 1;
            memcpy(prefix, value, (size_t)prefix_len);
            prefix[prefix_len] = '\0';
            memcpy(selected_text, value + span.start, (size_t)selected_len);
            selected_text[selected_len] = '\0';
            sel_x = text_x + TextWidth(prefix, font);
            sel_w = TextWidth(selected_text, font);
            if(sel_w < stroke_width)
                sel_w = stroke_width;
            DrawRectangle(sel_x, text_y, sel_w,
                          TextLineHeight(font),
                          ui_default_style() ? ui_alpha(c_circle, 82) :
                                                (Color){78, 132, 196, 135});
        }
    }
    RenderText(value, text_x, text_y, font, text_color);

    if(composition_end > composition_start) {
        char composition[1024];
        TextCompositionPaintSpan span;
        int len = (int)strlen(value);
        int composition_len;
        int composition_x;
        int composition_w;
        int composition_y;

        span = TextCompositionPaintSpanForText(composition_start,
                                               composition_end, len);
        if(span.visible) {
            composition_len = span.end - span.start;
            if(composition_len >= (int)sizeof(composition))
                composition_len = (int)sizeof(composition) - 1;
            memcpy(composition, value + span.start,
                   (size_t)composition_len);
            composition[composition_len] = '\0';
            composition_x = text_x +
                ui_text_width_before_cursor(value, font, span.start);
            composition_w = TextWidth(composition, font);
            composition_w = TextCompositionUnderlineEndX(
                composition_x, composition_x + composition_w,
                stroke_width) - composition_x;
            composition_y = TextCompositionUnderlineY(
                text_y, TextLineHeight(font), stroke_width);
            DrawRectangle(composition_x,
                          composition_y,
                          composition_w, stroke_width, cursor_color);
        }
    }

    if(focused && cursor_visible) {
        char before_cursor[1024];
        int len = (int)strlen(value);
        int clamped_cursor = TextCursorForLength(cursor_position, len);
        int copy_len = clamped_cursor;
        if(copy_len >= (int)sizeof(before_cursor))
            copy_len = (int)sizeof(before_cursor) - 1;
        memcpy(before_cursor, value, (size_t)copy_len);
        before_cursor[copy_len] = '\0';

        int cursor_x = text_x + TextWidth(before_cursor, font);
        DrawRectangle(cursor_x, cursor_y, stroke_width, cursor_h,
                      ui_default_style() ? c_circle : cursor_color);
    }
    EndClip();
}

void
DrawTextInput(Rectangle bounds, const char *text, int cursor_position,
                         int focused, int cursor_visible, int font,
                         int focus_id, int class_name)
{
    RenderTextInputEx(bounds, text, cursor_position, focused, 1,
                      cursor_visible, font, (TextInputStyle){0}, 0, 0, 0, 0,
                      0, focus_id, class_name);
}

static int
ui_text_cursor_from_x(const char *text, int font, int text_x, int mouse_x)
{
    char prefix[1024];
    int len;

    if(text == NULL)
        return 0;

    len = (int)strlen(text);
    if(mouse_x <= text_x)
        return 0;

    for(int i = 1; i <= len; i++) {
        int copy_len = i;
        if(copy_len >= (int)sizeof(prefix))
            copy_len = (int)sizeof(prefix) - 1;
        memcpy(prefix, text, (size_t)copy_len);
        prefix[copy_len] = '\0';
        if(text_x + TextWidth(prefix, font) >= mouse_x)
            return i;
    }
    return len;
}

static int
ui_text_width_before_cursor(const char *text, int font, int cursor_position)
{
    char before_cursor[1024];
    int len;
    int copy_len;

    if(text == NULL)
        return 0;

    len = (int)strlen(text);
    copy_len = TextCursorForLength(cursor_position, len);
    if(copy_len >= (int)sizeof(before_cursor))
        copy_len = (int)sizeof(before_cursor) - 1;
    memcpy(before_cursor, text, (size_t)copy_len);
    before_cursor[copy_len] = '\0';
    return TextWidth(before_cursor, font);
}

int
ui_text_cursor_at_x(const char *text, int font, int text_x, int mouse_x)
{
    return ui_text_cursor_from_x(text, font, text_x, mouse_x);
}

void
ui_draw_text_input_selection(Rectangle bounds, const char *text, int cursor,
                             int focused, int font, TextInputStyle style,
                             int selection_start, int selection_end)
{
    RenderTextInputEx(bounds, text, cursor, focused, 1, 1, font, style,
                      selection_start, selection_end, 0, 0, 0, 0, 0);
}

int
EditText(TextEdit edit)
{
    int changed = 0;
    int len;
    int codepoint;
    int anchor;

    if(edit.commit_pressed != NULL)
        *edit.commit_pressed = 0;
    if(edit.text == NULL || edit.text_size == 0 || edit.cursor_position == NULL)
        return 0;

    len = (int)strlen(edit.text);
    *edit.cursor_position = TextCursorForLength(*edit.cursor_position, len);
    if(!IsKeyboardInputEnabled())
        return 0;

    {
        TextNavigationInput navigation = {
            .text = edit.text,
            .key = ui_text_navigation_key(0),
            .modifier = ui_mod_key_down()
        };

        anchor = *edit.cursor_position;
        (void)ui_text_navigate(navigation, &anchor, edit.cursor_position);
    }

    if(ui_mod_key_down() && IsKeyPressed(KEY_C)) {
        TextContextCommandDecision decision =
            TextEditCommandDecisionFor(TextContextCommandCopy(), 0, 1, 0, 1,
                1, 1, 1);
        if(decision.copy_all)
            SetClipboardTextValue(edit.text);
    }
    if(ui_mod_key_down() && IsKeyPressed(KEY_X)) {
        TextContextCommandDecision decision =
            TextEditCommandDecisionFor(TextContextCommandCut(), 0, 1, 1, 1,
                1, 1, 1);
        if(decision.copy_all)
            SetClipboardTextValue(edit.text);
        if(decision.clear_all) {
            edit.text[0] = '\0';
            *edit.cursor_position = 0;
            changed = 1;
        }
    }
    if(ui_mod_key_down() && IsKeyPressed(KEY_V)) {
        TextContextCommandDecision decision =
            TextEditCommandDecisionFor(TextContextCommandPaste(), 0, 0, 0, 1,
                1, 1, 1);
        if(decision.paste)
            changed |= ui_text_paste_clipboard(edit, 0);
    }

    {
        int repeat = g_ui_text_input_backspace_count + ui_backspace_repeat_count();

        if(TextDeleteShortcutShouldRun(0, 0, 0, repeat)) {
            anchor = *edit.cursor_position;
            for(int i = 0; i < repeat; i++)
                changed |= ui_text_delete_key(
                    edit.text, edit.text_size, &anchor, edit.cursor_position,
                    TextDeleteBackspace(), ui_mod_key_down(), 0);
        }
        g_ui_text_input_backspace_count = 0;
    }

    if(TextDeleteShortcutShouldRun(0, 0, IsKeyPressed(KEY_DELETE), 0)) {
        anchor = *edit.cursor_position;
        changed |= ui_text_delete_key(
            edit.text, edit.text_size, &anchor, edit.cursor_position,
            TextDeleteForward(), ui_mod_key_down(), 0);
    }

    if(TextEditCommitShouldRun(IsKeyPressed(KEY_ENTER) ||
       IsKeyPressed(KEY_KP_ENTER), g_ui_text_input_enter_count)) {
        if(edit.commit_pressed != NULL)
            *edit.commit_pressed = 1;
        g_ui_text_input_enter_count = 0;
    }

    codepoint = GetCharPressed();
    while(codepoint > 0) {
        if((edit.filter == NULL || edit.filter(codepoint, edit.filter_user_data)) &&
           ui_text_insert_codepoint(edit.text, edit.text_size, edit.cursor_position,
                                    codepoint, edit.max_codepoints))
            changed = 1;
        codepoint = GetCharPressed();
    }

    for(int i = 0; i < g_ui_text_input_codepoint_count; i++) {
        codepoint = g_ui_text_input_codepoints[i];
        if((edit.filter == NULL || edit.filter(codepoint, edit.filter_user_data)) &&
           ui_text_insert_codepoint(edit.text, edit.text_size, edit.cursor_position,
                                    codepoint, edit.max_codepoints))
            changed = 1;
    }
    g_ui_text_input_codepoint_count = 0;

    len = (int)strlen(edit.text);
    *edit.cursor_position = TextCursorForLength(*edit.cursor_position, len);
    return changed;
}

int
ui_text_input_control_render(TextInputProps input)
{
    char editor_id[96];
    Widget widget;
    int focused = input.focused;

    widget = BeginWidget("TextField",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "TextField",
                                                 input.focus_id, NULL),
                           input.bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
    input.bounds = widget.bounds;

    if(input.focus_id > 0 && RegisterFocus(input.focus_id, input.bounds)) {
        focused = 1;
        SetFocusTextInputActive(1);
    }

    DrawTextInput(input.bounds, input.text, input.cursor_position,
                             focused, input.cursor_visible,
                             ui_text_input_default_font(StyleKindTextField(),
                                                        input.class_name),
                             input.focus_id, input.class_name);
    EndWidget(&widget);
    return focused;
}

int
RenderLink(LinkProps link)
{
    char editor_id[96];
    Widget widget;
    Vector2 mouse_world = ui_mouse_world();
    const char *text = link.text != NULL ? link.text : "";
    Rectangle bounds = link.bounds;
    int font;
    int text_w;
    int mouse_inside;
    int captured;
    LinkInteraction interaction;
    int focused;
    int clicked = 0;
    ButtonProps style_props = {0};
    StyleFrame style_frame;
    LinkAppearance appearance;
    Color color;

    style_props.class_name = link.class_name;
    style_props.emphasis = ButtonEmphasisLink;
    style_props.size = ControlSizeMedium;
    style_props.disabled = link.disabled;
    style_frame = ui_control_style_frame_kind(style_props,
                                              link.disabled ? ButtonStateDisabled : ButtonStateNormal,
                                              0, 0.0f, 0.0f, 0.0f,
                                              StyleKindLink());
    font = ResolveFont(0, StyleFontValue(style_frame.value.fields,
                                         style_frame.value.font_size),
                       GetFontSize());
    text_w = TextWidth(text, font);
    bounds = LinkBoundsFor(bounds, text_w, TextHeight(text, font), font);

    widget = BeginWidget("Link",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "Link", link.focus_id, text),
                           bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
    bounds = widget.bounds;

    mouse_inside = CheckCollisionPointRec(mouse_world, bounds);
    captured = InputCapturesClick(mouse_world);
    interaction = LinkInteractionFor(link.disabled != 0, captured != 0,
                                     mouse_inside != 0,
                                     HoverEffectsEnabled() != 0);
    focused = !link.disabled && link.focus_id > 0 &&
              RegisterFocus(link.focus_id, bounds);

    style_frame = ui_control_style_frame_kind(style_props, interaction.state,
                                              0, 0.0f, 0.0f, 0.0f,
                                              StyleKindLink());
    appearance = ResolveLinkAppearance(style_frame, interaction.hovered,
                                       link.disabled != 0);
    color = Fade(GetColor(appearance.color), style_frame.value.opacity);

    if(interaction.active) {
        MarkClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            clicked = 1;
    } else if(interaction.disabled_marker) {
        MarkDisabled();
    }

    RenderText(text, (int)bounds.x,
               ControlTextBaselineY(text, (int)bounds.y,
                                    (int)bounds.height, font),
               font, color);
    if(appearance.underline && text_w > 0) {
        int underline_y = LinkUnderlineYFor(
            bounds, (float)Scale(1000) / 1000.0f);
        DrawLine((int)bounds.x, underline_y, (int)bounds.x + text_w,
                 underline_y, color);
    }
    if(focused) {
        SetFocusTextInputActive(0);
        RenderFocus(bounds);
    }
    if(clicked)
        ConsumeRelease();
    if(LinkActivated(link.disabled != 0, clicked != 0,
                     IsFocusActivatePressed(link.focus_id) != 0)) {
        ui_open_url(link.link);
        EndWidget(&widget);
        return 1;
    }
    EndWidget(&widget);
    return 0;
}

int
ui_text_line_start(const char *text, int cursor)
{
    int len;
    int i;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    cursor = TextCursorForLength(cursor, len);
    if(cursor <= 0)
        return 0;
    for(i = cursor - 1; i >= 0; i--) {
        if(text[i] == '\n')
            return i + 1;
    }
    return 0;
}

int
ui_text_line_end(const char *text, int cursor)
{
    int len;
    int i;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    cursor = TextCursorForLength(cursor, len);
    for(i = cursor; i < len; i++) {
        if(text[i] == '\n')
            return i;
    }
    return len;
}

static int
ui_text_column_x(const char *text, int start, int cursor, int font)
{
    char prefix[1024];
    int len;

    if(text == NULL || cursor <= start)
        return 0;
    len = cursor - start;
    if(len >= (int)sizeof(prefix))
        len = (int)sizeof(prefix) - 1;
    memcpy(prefix, text + start, (size_t)len);
    prefix[len] = '\0';
    return TextWidth(prefix, font);
}

static int
ui_text_cursor_from_line_x(const char *text, int start, int end, int font, int target_x)
{
    char prefix[1024];
    int i;

    if(target_x <= 0)
        return start;
    for(i = start + 1; i <= end; i++) {
        int len = i - start;
        if(len >= (int)sizeof(prefix))
            len = (int)sizeof(prefix) - 1;
        memcpy(prefix, text + start, (size_t)len);
        prefix[len] = '\0';
        if(TextWidth(prefix, font) >= target_x)
            return i;
    }
    return end;
}

static int
ui_text_cursor_from_line_column(const char *text, int start, int end,
                                int column)
{
    int cursor = start;

    while(column-- > 0 && cursor < end) {
        int next = ui_utf8_next_offset(text, cursor);

        if(next <= cursor)
            break;
        cursor = next;
    }
    return cursor;
}

int
ui_text_move_vertical(const char *text, int cursor, int font, int dir)
{
    int start;
    int end;
    int target_x;
    int other_start;
    int other_end;

    if(text == NULL)
        return 0;
    start = ui_text_line_start(text, cursor);
    end = ui_text_line_end(text, cursor);
    target_x = ui_text_column_x(text, start, cursor, font);
    if(dir < 0) {
        if(start == 0)
            return cursor;
        other_end = start - 1;
        other_start = ui_text_line_start(text, other_end);
    } else {
        int len = (int)strlen(text);
        if(end >= len)
            return cursor;
        other_start = end + 1;
        other_end = ui_text_line_end(text, other_start);
    }
    if(target_x <= 0 && cursor > start) {
        int column = 0;
        int column_cursor = start;

        while(column_cursor < cursor) {
            int next = ui_utf8_next_offset(text, column_cursor);

            if(next <= column_cursor)
                break;
            column_cursor = next;
            column++;
        }
        return ui_text_cursor_from_line_column(
            text, other_start, other_end, column);
    }
    return ui_text_cursor_from_line_x(text, other_start, other_end, font, target_x);
}

int
ui_text_area_move_page(TextAreaProps area, int cursor, int direction)
{
    TextInputStyle style = ui_resolve_text_input_style((TextInputStyle){0},
        StyleKindTextArea(), area.class_name);
    TextInputMetrics metrics = ui_text_input_metrics_for_style(
        style, StyleKindTextArea(), area.class_name, 0);
    int font = metrics.font;
    int line_gap = metrics.line_gap;
    int page_rows = TextAreaPageRows(area.bounds.height, font, line_gap,
                                     metrics.padding_y);

    for(int row = 0; row < page_rows; row++)
        cursor = ui_text_move_vertical(area.text, cursor, font, direction);
    return cursor;
}

int
ui_text_navigation_key(int multiline)
{
    if(IsKeyPressed(KEY_LEFT))
        return TextNavLeft();
    if(IsKeyPressed(KEY_RIGHT))
        return TextNavRight();
    if(IsKeyPressed(KEY_HOME))
        return TextNavHome();
    if(IsKeyPressed(KEY_END))
        return TextNavEnd();
    if(multiline && IsKeyPressed(KEY_UP))
        return TextNavUp();
    if(multiline && IsKeyPressed(KEY_DOWN))
        return TextNavDown();
    if(multiline && IsKeyPressed(KEY_PAGE_UP))
        return TextNavPageUp();
    if(multiline && IsKeyPressed(KEY_PAGE_DOWN))
        return TextNavPageDown();
    return 0;
}

int
ui_text_navigate(TextNavigationInput input, int *anchor, int *cursor)
{
    int start;
    int end;
    int target;
    TextNavigationDecision decision;
    TextSelectionRange range;
    TextSelectionState next;

    if(input.text == NULL || anchor == NULL || cursor == NULL)
        return 0;
    range = TextSelectionRangeFor(*anchor, *cursor);
    start = range.start;
    end = range.end;
    target = *cursor;
    decision = TextNavigationDecisionFor(
        input.key, input.area != NULL, input.shift != 0, input.modifier != 0,
        input.secure != 0, end > start);
    if(!decision.consumed)
        return 0;

    if(decision.collapse_selection_start) {
        target = start;
    } else if(decision.collapse_selection_end) {
        target = end;
    } else if(decision.document_edge < 0) {
        target = 0;
    } else if(decision.document_edge > 0) {
        target = (int)strlen(input.text);
    } else if(decision.line_edge < 0) {
        target = ui_text_line_start(input.text, target);
    } else if(decision.line_edge > 0) {
        target = ui_text_line_end(input.text, target);
    } else if(decision.word_direction < 0) {
        target = ui_text_word_left(input.text, target);
    } else if(decision.word_direction > 0) {
        target = ui_text_word_right(input.text, target);
    } else if(decision.char_direction < 0) {
        target = ui_utf8_prev_offset(input.text, target);
    } else if(decision.char_direction > 0) {
        target = ui_utf8_next_offset(input.text, target);
    } else if(decision.vertical_direction != 0) {
        if(input.area == NULL)
            return 0;
        target = ui_text_move_vertical(input.text, target, input.font,
                                       decision.vertical_direction);
    } else if(decision.page_direction != 0) {
        if(input.area == NULL)
            return 0;
        target = ui_text_area_move_page(*input.area, target,
                                        decision.page_direction);
    } else {
        return 0;
    }
    next = TextSelectionAfterMove(*anchor, *cursor, target,
                                  decision.extend_selection);
    *anchor = next.anchor;
    *cursor = next.cursor;
    return 1;
}
static int
ui_text_area_line_font(const char *text, int start, int end, int base_font)
{
    int hashes = 0;

    if(text == NULL)
        return base_font;
    while(start + hashes < end && hashes < 6 && text[start + hashes] == '#')
        hashes++;
    if(hashes > 0 && start + hashes < end && text[start + hashes] == ' ' && base_font < Text24) {
        if(hashes <= 2)
            return Text24;
    }
    return base_font;
}

static int
ui_text_area_wrap_chunk_end(const char *text, int start, int end, int font,
                            int wrap_width)
{
    char line[1024];
    int chunk_len;
    int remaining = end - start;

    if(wrap_width <= 0 || start >= end)
        return end;
    if(remaining >= (int)sizeof(line))
        remaining = (int)sizeof(line) - 1;
    memcpy(line, text + start, (size_t)remaining);
    line[remaining] = '\0';
    if(TextWidth(line, font) <= wrap_width)
        return end;

    chunk_len = 1;
    while(start + chunk_len < end && chunk_len + 1 < (int)sizeof(line)) {
        snprintf(line, sizeof(line), "%.*s", chunk_len + 1, text + start);
        if(TextWidth(line, font) > wrap_width)
            break;
        chunk_len++;
    }
    return start + chunk_len;
}

static int
ui_text_area_content_height_uncached(const char *text, int font, int line_gap,
                                     int len, int wrap_width)
{
    int line_start = 0;
    int height = 0;

    if(text == NULL)
        text = "";
    for(int i = 0; i <= len; i++) {
        if(text[i] == '\n' || text[i] == '\0') {
            int line_font = ui_text_area_line_font(text, line_start, i, font);
            int line_h = TextLineHeight(line_font) + line_gap;

            if(wrap_width <= 0 || line_start >= i) {
                height += line_h;
            } else {
                int chunk_start = line_start;

                while(chunk_start < i) {
                    int chunk_end = ui_text_area_wrap_chunk_end(
                        text, chunk_start, i, line_font, wrap_width);
                    height += line_h;
                    if(chunk_end <= chunk_start)
                        break;
                    chunk_start = chunk_end;
                }
            }
            line_start = i + 1;
        }
    }
    return height;
}

static int
ui_text_area_content_height(const char *text, int font, int line_gap,
                            int wrap_width, int content_version,
                            int force_recompute)
{
    int len;

    if(text == NULL)
        text = "";
    len = (int)strlen(text);
    if(content_version == 0)
        return ui_text_area_content_height_uncached(text, font, line_gap, len,
                                                   wrap_width);

    if(!force_recompute) {
        for(int i = 0; i < TEXT_AREA_HEIGHT_CACHE_SIZE; i++) {
            TextAreaHeightCacheEntry *entry = &g_ui_text_area_height_cache[i];
            if(entry->text == text && entry->font == font &&
               entry->line_gap == line_gap &&
               entry->wrap_width == wrap_width && entry->len == len &&
               entry->content_version == content_version)
                return entry->height;
        }
    }

    TextAreaHeightCacheEntry *entry =
        &g_ui_text_area_height_cache[g_ui_text_area_height_cache_next];
    entry->text = text;
    entry->font = font;
    entry->line_gap = line_gap;
    entry->wrap_width = wrap_width;
    entry->len = len;
    entry->content_version = content_version;
    entry->height = ui_text_area_content_height_uncached(text, font, line_gap,
                                                        len, wrap_width);
    g_ui_text_area_height_cache_next =
        (g_ui_text_area_height_cache_next + 1) % TEXT_AREA_HEIGHT_CACHE_SIZE;
    return entry->height;
}

static int
ui_text_area_cursor_from_point(const char *text, int font, int line_gap,
                               int wrap_width, int x, int y, int mouse_x,
                               int mouse_y, int scroll_y)
{
    int len;
    int line_start = 0;
    int draw_y = 0;
    int target_y;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    target_y = mouse_y - y + scroll_y;
    if(target_y < 0)
        target_y = 0;
    for(int i = 0; i <= len; i++) {
        if(text[i] == '\n' || text[i] == '\0') {
            int line_font = ui_text_area_line_font(text, line_start, i, font);
            int line_h = TextLineHeight(line_font) + line_gap;
            if(wrap_width <= 0 || line_start >= i) {
                if(target_y < draw_y + line_h || text[i] == '\0')
                    return ui_text_cursor_from_line_x(text, line_start, i,
                                                      line_font, mouse_x - x);
                draw_y += line_h;
            } else {
                int chunk_start = line_start;

                while(chunk_start < i) {
                    int chunk_end = ui_text_area_wrap_chunk_end(
                        text, chunk_start, i, line_font, wrap_width);
                    if(target_y < draw_y + line_h || chunk_end >= i)
                        return ui_text_cursor_from_line_x(
                            text, chunk_start, chunk_end, line_font,
                            mouse_x - x);
                    draw_y += line_h;
                    if(chunk_end <= chunk_start)
                        break;
                    chunk_start = chunk_end;
                }
            }
            line_start = i + 1;
        }
    }
    return len;
}

static int
ui_text_area_cursor_y(const char *text, int cursor, int font, int line_gap,
                      int wrap_width, int *cursor_height)
{
    int len;
    int line_start = 0;
    int draw_y = 0;

    if(text == NULL)
        text = "";
    len = (int)strlen(text);
    cursor = TextCursorForLength(cursor, len);
    for(int i = 0; i <= len; i++) {
        if(text[i] == '\n' || text[i] == '\0') {
            int line_font = ui_text_area_line_font(text, line_start, i, font);
            int line_h = TextLineHeight(line_font) + line_gap;
            int chunk_start = line_start;
            int chunk_end = i;

            do {
                if(wrap_width > 0 && chunk_start < i)
                    chunk_end = ui_text_area_wrap_chunk_end(
                        text, chunk_start, i, line_font, wrap_width);
                else
                    chunk_end = i;
                /* A cursor at a soft-wrap boundary belongs to the next
                 * visual line, except at the logical end of the line. */
                if(cursor >= chunk_start && cursor <= chunk_end &&
                   (cursor < chunk_end || chunk_end == i)) {
                    if(cursor_height != NULL)
                        *cursor_height = TextLineHeight(line_font);
                    return draw_y;
                }
                draw_y += line_h;
                if(chunk_end <= chunk_start)
                    break;
                chunk_start = chunk_end;
            } while(wrap_width > 0 && chunk_start < i);

            line_start = i + 1;
        }
    }
    if(cursor_height != NULL)
        *cursor_height = TextLineHeight(font);
    return draw_y;
}

static int
ui_text_area_select_line(const char *text, int cursor, int *start, int *end)
{
    int len;

    if(text == NULL || start == NULL || end == NULL)
        return 0;
    len = (int)strlen(text);
    cursor = TextCursorForLength(cursor, len);
    if(cursor > 0 && (cursor == len || text[cursor] == '\n'))
        cursor--;
    *start = ui_text_line_start(text, cursor);
    *end = ui_text_line_end(text, cursor);
    return *end >= *start;
}

static int
ui_syntax_is_ident_start(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int
ui_syntax_is_ident(char c)
{
    return ui_syntax_is_ident_start(c) || (c >= '0' && c <= '9') || c == '-';
}

static int
ui_syntax_word_eq(const char *text, int len, const char *word)
{
    return (int)strlen(word) == len && strncmp(text, word, (size_t)len) == 0;
}

static int
ui_syntax_kry_keyword(const char *text, int len)
{
    static const char *keywords[] = {
        "#defined", "#else", "#enum", "#extern", "#export", "#global", "#if",
        "#import", "#module", "#private", "#type", "#ui",
        "app", "args", "background", "button", "c", "c_rect", "call",
        "cimport", "clamp_max", "clamp_min", "do", "draw", "else", "endraw",
        "event", "fn", "font", "for", "fps", "global", "goto", "guard", "icon_button", "if",
        "import", "in", "key", "key_down", "line", "mod", "native", "on", "page",
        "pub", "raw", "rect", "return", "returns", "screen", "set",
        "set_theme", "size", "state", "swatch", "text", "texture", "theme",
        "use", "var", "widget"
    };

    for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if(ui_syntax_word_eq(text, len, keywords[i]))
            return 1;
    }
    return 0;
}

static int
ui_syntax_c_keyword(const char *text, int len)
{
    static const char *keywords[] = {
        "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "float", "for", "goto", "if", "int", "long",
        "return", "short", "sizeof", "static", "struct", "switch",
        "typedef", "void", "while"
    };

    for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if(ui_syntax_word_eq(text, len, keywords[i]))
            return 1;
    }
    return 0;
}

static int
ui_syntax_make_keyword(const char *text, int len)
{
    static const char *keywords[] = {
        "define", "else", "endef", "endif", "export", "ifdef", "ifeq",
        "ifndef", "ifneq", "include", "override", "private", "undefine",
        "unexport", "vpath"
    };

    for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if(ui_syntax_word_eq(text, len, keywords[i]))
            return 1;
    }
    return 0;
}

static Color
ui_syntax_token_color(SyntaxMode syntax, const char *text, int len,
                      int first_token, TextInputStyle style)
{
    int dark = GetEffectiveThemeDarkMode();
    Color keyword = dark ? (Color){142, 160, 240, 255} : (Color){36, 72, 172, 255};
    Color string = dark ? (Color){110, 200, 176, 255} : (Color){34, 126, 82, 255};
    Color number = dark ? (Color){213, 145, 82, 255} : (Color){150, 72, 126, 255};
    Color path = dark ? (Color){205, 170, 110, 255} : (Color){138, 96, 32, 255};
    Color comment = dark ? (Color){138, 146, 160, 255} : (Color){104, 112, 124, 255};

    if(len <= 0)
        return style.text;
    if((syntax == SyntaxKry || syntax == SyntaxMake) &&
       first_token && text[0] == '#' &&
       !(syntax == SyntaxKry && ui_syntax_kry_keyword(text, len)))
        return comment;
    if(text[0] == '"')
        return string;
    if((text[0] >= '0' && text[0] <= '9') ||
       (text[0] == '-' && len > 1 && text[1] >= '0' && text[1] <= '9'))
        return number;
    if(syntax == SyntaxKry &&
       (text[0] == '/' ||
        (text[0] == '.' && len > 1 && (text[1] == '/' || text[1] == '.')) ||
        text[0] == '%'))
        return path;
    if(syntax == SyntaxKry && ui_syntax_kry_keyword(text, len))
        return keyword;
    if(syntax == SyntaxC && ui_syntax_c_keyword(text, len))
        return keyword;
    if(syntax == SyntaxMake && ui_syntax_make_keyword(text, len))
        return keyword;
    if(syntax == SyntaxMake && len > 1 && text[0] == '$')
        return path;
    return style.text;
}

static int
ui_syntax_token_len(const char *line, int len, int index, SyntaxMode syntax,
                    int first_token)
{
    int i = index;

    if(i >= len)
        return 0;
    if(line[i] == ' ' || line[i] == '\t') {
        while(i < len && (line[i] == ' ' || line[i] == '\t'))
            i++;
        return i - index;
    }
    if(syntax == SyntaxKry && first_token && line[i] == '#' &&
       ui_syntax_is_ident_start(line[i + 1])) {
        i++;
        while(i < len && ui_syntax_is_ident(line[i]))
            i++;
        return i - index;
    }
    if((syntax == SyntaxKry || syntax == SyntaxMake) &&
       first_token && line[i] == '#')
        return len - index;
    if(syntax == SyntaxC && line[i] == '/' && i + 1 < len &&
       line[i + 1] == '/')
        return len - index;
    if(line[i] == '"') {
        i++;
        while(i < len) {
            if(line[i] == '\\' && i + 1 < len) {
                i += 2;
                continue;
            }
            if(line[i++] == '"')
                break;
        }
        return i - index;
    }
    if(line[i] == '/' || line[i] == '%' ||
       (syntax == SyntaxMake && line[i] == '$') ||
       (line[i] == '.' && i + 1 < len && (line[i + 1] == '/' || line[i + 1] == '.'))) {
        i++;
        while(i < len && !isspace((unsigned char)line[i]) &&
              strchr("{}[](),", line[i]) == NULL)
            i++;
        return i - index;
    }
    if((line[i] >= '0' && line[i] <= '9') ||
       (line[i] == '-' && i + 1 < len && line[i + 1] >= '0' && line[i + 1] <= '9')) {
        i++;
        while(i < len && ((line[i] >= '0' && line[i] <= '9') ||
                          line[i] == '.' || line[i] == 'x' ||
                          (line[i] >= 'A' && line[i] <= 'F') ||
                          (line[i] >= 'a' && line[i] <= 'f')))
            i++;
        return i - index;
    }
    if(ui_syntax_is_ident_start(line[i])) {
        i++;
        while(i < len && ui_syntax_is_ident(line[i]))
            i++;
        return i - index;
    }
    return 1;
}

static void
ui_draw_text_area_selection(const char *text, int line_start, int line_end,
                            int x, int y, int font, Color color,
                            int selection_start, int selection_end)
{
    TextSelectionPaintSpan span;
    int start;
    int end;
    int start_x;
    int end_x;

    if(text == NULL || selection_end <= selection_start)
        return;
    span = TextSelectionPaintSpanForLine(selection_start, selection_end,
                                         line_start, line_end);
    if(!span.visible)
        return;
    start = span.start;
    end = span.end;
    start_x = x + ui_text_column_x(text, line_start, start, font);
    end_x = x + ui_text_column_x(text, line_start, end, font);
    end_x = TextSelectionHighlightEndX(start_x, end_x,
        span.continues_past_line, (float)Scale(1000) / 1000.0f);
    DrawRectangle(start_x, y, end_x - start_x, TextLineHeight(font),
                  color);
}

static void
ui_draw_text_area_composition(const char *text, int line_start, int line_end,
                              int x, int y, int font, Color color,
                              int composition_start, int composition_end)
{
    int stroke_width = TextInputStrokeWidth((float)Scale(1000) / 1000.0f);
    TextCompositionPaintSpan span;
    int start;
    int end;
    int start_x;
    int end_x;

    if(text == NULL || composition_end <= composition_start)
        return;
    span = TextCompositionPaintSpanForLine(composition_start, composition_end,
                                           line_start, line_end);
    if(!span.visible)
        return;
    start = span.start;
    end = span.end;
    start_x = x + ui_text_column_x(text, line_start, start, font);
    end_x = x + ui_text_column_x(text, line_start, end, font);
    end_x = TextCompositionUnderlineEndX(start_x, end_x, stroke_width);
    DrawRectangle(start_x, TextCompositionUnderlineY(
                      y, TextLineHeight(font), stroke_width),
                  end_x - start_x, stroke_width, color);
}

static void
ui_draw_syntax_line(const char *line, int len, int x, int y, int font,
                    SyntaxMode syntax, TextInputStyle style)
{
    char token[1024];
    int offset = 0;
    int first_token = 1;

    while(offset < len) {
        int token_len = ui_syntax_token_len(line, len, offset, syntax,
                                            first_token);
        Color color;
        int token_is_first;

        if(token_len <= 0)
            break;
        if(token_len >= (int)sizeof(token))
            token_len = (int)sizeof(token) - 1;
        memcpy(token, line + offset, (size_t)token_len);
        token[token_len] = '\0';
        token_is_first = first_token;
        color = ui_syntax_token_color(syntax, token, token_len, token_is_first,
                                      style);
        if(token[0] != ' ' && token[0] != '\t')
            first_token = 0;
        RenderText(token, x, y, font, color);
        x += TextWidth(token, font);
        offset += token_len;
    }
}

static void
ui_draw_text_area_text(const char *text, int cursor, int focused,
                       Rectangle bounds, int font, int line_gap,
                       int scroll_y, int wrap_width, SyntaxMode syntax,
                       TextInputStyle style, int selection_start,
                       int selection_end, int composition_start,
                       int composition_end)
{
    // The editor owns selection; rendered lines must not claim it or copy
    // a second, single-line selection over the editor clipboard contents.
    int selectable = PushTextSelectable(0);
    char line[1024];
    int len;
    int line_start = 0;
    TextInputMetrics metrics = ui_text_input_metrics_for_style(
        style, StyleKindTextArea(), 0, 0);
    int padding_x = metrics.padding_x;
    int padding_y = metrics.padding_y;
    int text_x = (int)bounds.x + padding_x;
    int text_y = (int)bounds.y + padding_y - scroll_y;
    int draw_y = text_y;
    int clip_top = (int)bounds.y + padding_y;
    int clip_bottom = (int)(bounds.y + bounds.height) - padding_y;
    int stroke_width = TextInputStrokeWidth((float)Scale(1000) / 1000.0f);

    if(text == NULL)
        text = "";
    len = (int)strlen(text);
    for(int i = 0; i <= len; i++) {
        if(text[i] == '\n' || text[i] == '\0') {
            int line_font = ui_text_area_line_font(text, line_start, i, font);
            int line_h = TextLineHeight(line_font) + line_gap;
            int chunk_start = line_start;
            int chunk_end = i;

            do {
                int line_len;

                if(wrap_width > 0 && chunk_start < i)
                    chunk_end = ui_text_area_wrap_chunk_end(
                        text, chunk_start, i, line_font, wrap_width);
                else
                    chunk_end = i;

                line_len = chunk_end - chunk_start;
                if(draw_y + line_h >= clip_top && draw_y <= clip_bottom) {
                    if(line_len >= (int)sizeof(line))
                        line_len = (int)sizeof(line) - 1;
                    if(line_len > 0)
                        memcpy(line, text + chunk_start, (size_t)line_len);
                    line[line_len] = '\0';
                    ui_draw_text_area_selection(text, chunk_start, chunk_end,
                                                text_x, draw_y, line_font,
                                                (Color){0, 96, 192, 72},
                                                selection_start, selection_end);
                    if(syntax == SyntaxNone)
                        RenderText(line, text_x, draw_y, line_font, style.text);
                    else
                        ui_draw_syntax_line(line, line_len, text_x, draw_y,
                                            line_font, syntax, style);
                    ui_draw_text_area_composition(
                        text, chunk_start, chunk_end, text_x, draw_y,
                        line_font, style.cursor, composition_start,
                        composition_end);
                    if(focused && cursor >= chunk_start && cursor <= chunk_end &&
                       (cursor < chunk_end || chunk_end == i) &&
                       ui_caret_blink_visible()) {
                        int cursor_x = text_x + ui_text_column_x(
                            text, chunk_start, cursor, line_font);
                        DrawRectangle(cursor_x, draw_y, stroke_width,
                                      TextLineHeight(line_font),
                                      style.cursor);
                    }
                }
                draw_y += line_h;
                if(chunk_end <= chunk_start)
                    break;
                chunk_start = chunk_end;
            } while(wrap_width > 0 && chunk_start < i);

            line_start = i + 1;
            if(draw_y > clip_bottom)
                break;
        }
    }
    PopTextSelectable(selectable);
}

int
ui_text_area_cursor_at_point(TextAreaProps area, int mouse_x, int mouse_y)
{
    TextInputStyle style = ui_resolve_text_input_style((TextInputStyle){0},
        StyleKindTextArea(), area.class_name);
    TextInputMetrics metrics = ui_text_input_metrics_for_style(
        style, StyleKindTextArea(), area.class_name, 0);
    int font = metrics.font;
    int line_gap = metrics.line_gap;
    int padding_x = metrics.padding_x;
    int padding_y = metrics.padding_y;
    int wrap_width = TextAreaWrapWidthFor(area.bounds.width, padding_x,
        area.wrap != 0, TextAreaMinWrapWidth((float)Scale(1000) / 1000.0f));
    int scroll_y = area.scroll_y != NULL ? *area.scroll_y : 0;

    return ui_text_area_cursor_from_point(
        area.text, font, line_gap, wrap_width,
        (int)area.bounds.x + padding_x, (int)area.bounds.y + padding_y,
        mouse_x, mouse_y, scroll_y);
}

void
ui_text_area_reveal_cursor(TextAreaProps area, int cursor)
{
    int font;
    int line_gap;
    int padding_x;
    int padding_y;
    int wrap_width;
    int content_h;
    int viewport_h;
    int max_scroll;
    int cursor_h;
    int cursor_y;
    int scroll_y;
    TextInputStyle style;

    if(area.text == NULL || area.scroll_y == NULL)
        return;
    style = ui_resolve_text_input_style((TextInputStyle){0},
                                        StyleKindTextArea(),
                                        area.class_name);
    {
        TextInputMetrics metrics = ui_text_input_metrics_for_style(
            style, StyleKindTextArea(), area.class_name, 0);
        font = metrics.font;
        line_gap = metrics.line_gap;
        padding_x = metrics.padding_x;
        padding_y = metrics.padding_y;
    }
    wrap_width = TextAreaWrapWidthFor(area.bounds.width, padding_x,
        area.wrap != 0, TextAreaMinWrapWidth((float)Scale(1000) / 1000.0f));
    viewport_h = (int)area.bounds.height - padding_y * 2;
    if(viewport_h <= 0)
        return;
    content_h = ui_text_area_content_height(
        area.text, font, line_gap, wrap_width, area.content_version, 1);
    max_scroll = TextAreaMaxScrollFor(content_h, area.bounds.height,
                                      padding_y);
    scroll_y = TextAreaScrollFor(*area.scroll_y, max_scroll);
    cursor_h = TextLineHeight(font);
    cursor_y = ui_text_area_cursor_y(area.text, cursor, font, line_gap,
                                     wrap_width, &cursor_h);
    scroll_y = TextAreaRevealScroll(scroll_y, cursor_y, cursor_h,
                                    viewport_h);
    *area.scroll_y = TextAreaScrollFor(scroll_y, max_scroll);
}

static void
ui_paint_text_area_internal(TextAreaProps area, int cursor, int focused,
                            int selection_start, int selection_end,
                            int composition_start, int composition_end)
{
    TextInputStyle requested_style = {0};
    TextInputStyle style;
    int font;
    int line_gap;
    int padding_x;
    int padding_y;
    int wrap_width;
    int scroll_y;
    TextAreaPaint paint;

    if(area.text == NULL)
        return;
    style = ui_resolve_text_input_style((TextInputStyle){0},
                                        StyleKindTextArea(), area.class_name);
    {
        TextInputMetrics metrics = ui_text_input_metrics_for_style(
            style, StyleKindTextArea(), area.class_name, 0);
        font = metrics.font;
        line_gap = metrics.line_gap;
        padding_x = metrics.padding_x;
        padding_y = metrics.padding_y;
    }
    scroll_y = area.scroll_y != NULL ? *area.scroll_y : 0;
    {
        int min_wrap_width = TextAreaMinWrapWidth((float)Scale(1000) / 1000.0f);
        wrap_width = TextAreaWrapWidthFor(area.bounds.width, padding_x,
                                          area.wrap != 0, min_wrap_width);
        int content_h = ui_text_area_content_height(
            area.text, font, line_gap, wrap_width,
            area.content_version, 0);
        paint = TextAreaPaintFor(area.bounds, font, line_gap, padding_x,
                                 padding_y, area.wrap != 0, content_h,
                                 scroll_y, TextLineHeight(font),
                                 min_wrap_width);
        scroll_y = paint.scroll_y;
        wrap_width = paint.wrap_width;
        if(area.scroll_y != NULL)
            *area.scroll_y = scroll_y;
    }
    (void)ui_text_input_surface(area.bounds, style, focused,
                                !area.read_only, area.focus_id, "TextArea",
                                requested_style, area.class_name);
    ui_begin_world_clip(paint.clip_bounds);
    if(area.text[0] == '\0' && !focused && area.placeholder != NULL)
        RenderText(area.placeholder, paint.placeholder_x,
                   paint.placeholder_y, font, style.border);
    else
        ui_draw_text_area_text(area.text, cursor,
                               focused && !area.read_only,
                               area.bounds, font, line_gap, scroll_y,
                               wrap_width, area.syntax, style,
                               selection_start, selection_end,
                               composition_start, composition_end);
    EndClip();
}

void
ui_paint_text_area(TextAreaProps area, int cursor, int focused,
                   int selection_start, int selection_end)
{
    ui_paint_text_area_internal(area, cursor, focused, selection_start,
                                selection_end, 0, 0);
}

void
ui_paint_text_area_composition(TextAreaProps area, int cursor, int focused,
                               int selection_start, int selection_end,
                               int composition_start, int composition_end)
{
    ui_paint_text_area_internal(area, cursor, focused, selection_start,
                                selection_end, composition_start,
                                composition_end);
}

int
TextBufferLineAtCursor(const char *text, int cursor)
{
    int line = 1;

    if(text == NULL)
        return 1;
    for(int i = 0; text[i] != '\0' && i < cursor; i++) {
        if(text[i] == '\n')
            line++;
    }
    return line;
}

int
TextBufferColumnAtCursor(const char *text, int cursor)
{
    int col = 1;

    if(text == NULL)
        return 1;
    for(int i = 0; text[i] != '\0' && i < cursor; i++) {
        if(text[i] == '\n')
            col = 1;
        else
            col++;
    }
    return col;
}

int
TextBufferLineCount(const char *text)
{
    int lines = 1;

    if(text == NULL)
        return 1;
    for(int i = 0; text[i] != '\0'; i++) {
        if(text[i] == '\n')
            lines++;
    }
    return lines;
}

static int
text_buffer_line_start(const char *text, int cursor)
{
    int start = TextBufferLineStartCursor(cursor);

    if(text == NULL)
        return 0;
    while(start > 0 && text[start - 1] != '\n')
        start--;
    return start;
}

static int
text_buffer_insert(char *text, int text_size, int at, const char *bytes,
                   int len)
{
    int used;
    TextBufferInsertDecision decision;

    if(text == NULL || bytes == NULL || text_size <= 0 || len <= 0)
        return 0;
    used = (int)strlen(text);
    decision = TextBufferInsertDecisionFor(text_size, used, at, len);
    if(!decision.can_insert)
        return 0;
    memmove(text + at + len, text + at, (size_t)decision.tail_count);
    memcpy(text + at, bytes, (size_t)len);
    return 1;
}

static int
text_buffer_delete(char *text, int at, int len)
{
    int used;
    TextBufferDeleteDecision decision;

    if(text == NULL || len <= 0)
        return 0;
    used = (int)strlen(text);
    decision = TextBufferDeleteDecisionFor(used, at, len);
    if(!decision.can_delete)
        return 0;
    memmove(text + at, text + at + len, (size_t)decision.tail_count);
    return 1;
}

int
TextBufferToggleLineComment(char *text, int text_size, int *cursor)
{
    int start;
    int p;

    if(text == NULL || cursor == NULL)
        return 0;
    start = text_buffer_line_start(text, *cursor);
    p = start;
    while(text[p] == ' ' || text[p] == '\t')
        p++;
    if(text[p] == '/' && text[p + 1] == '/') {
        if(!text_buffer_delete(text, p, 2))
            return 0;
        *cursor = TextBufferCursorAfterDelete(*cursor, p, 2);
        return 1;
    }
    if(!text_buffer_insert(text, text_size, p, "//", 2))
        return 0;
    *cursor = TextBufferCursorAfterInsert(*cursor, p, 2);
    return 1;
}

int
TextBufferIndentLine(char *text, int text_size, int *cursor, int outdent)
{
    int start;
    int remove = 0;

    if(text == NULL || cursor == NULL)
        return 0;
    start = text_buffer_line_start(text, *cursor);
    if(outdent) {
        while(remove < 4 && text[start + remove] == ' ')
            remove++;
        if(remove == 0 && text[start] == '\t')
            remove = 1;
        if(remove == 0)
            return 0;
        if(!text_buffer_delete(text, start, remove))
            return 0;
        *cursor = TextBufferCursorAfterDelete(*cursor, start, remove);
        return 1;
    }
    if(!text_buffer_insert(text, text_size, start, "    ", 4))
        return 0;
    *cursor = TextBufferCursorAfterInsert(*cursor, start, 4);
    return 1;
}

int
TextBufferBracketMatch(const char *text, int cursor)
{
    const char *opens = "([{";
    const char *closes = ")]}";
    TextBufferBracketDecision decision;
    int len;
    int pos;
    int dir;
    char open;
    char close;
    int depth = 0;

    if(text == NULL)
        return -1;
    len = (int)strlen(text);
    if(len <= 0)
        return -1;
    pos = TextBufferBracketCursorFor(cursor, len);
    pos = TextBufferBracketCandidatePos(
        pos, strchr(opens, text[pos]) != NULL ||
                 strchr(closes, text[pos]) != NULL);
    decision = TextBufferBracketDecisionFor(pos, (unsigned char)text[pos]);
    if(!decision.valid)
        return -1;
    open = (char)decision.open;
    close = (char)decision.close;
    dir = decision.direction;
    for(int i = pos; i >= 0 && i < len; i += dir) {
        if(text[i] == open)
            depth += dir > 0 ? 1 : -1;
        else if(text[i] == close)
            depth += dir > 0 ? -1 : 1;
        if(depth == 0 && i != pos)
            return i;
    }
    return -1;
}

Rectangle
TextAreaGutter(TextAreaProps area, int gutter_width)
{
    int font;
    int line_gap;
    int line_h;
    int scroll_y;
    int first;
    int rows;
    int active;
    int total;
    int y;
    float scale;
    Rectangle gutter;
    TextAreaGutterMetrics gutter_metrics;
    TextInputStyle style;

    if(gutter_width <= 0)
        return area.bounds;
    style = ui_resolve_text_input_style((TextInputStyle){0},
                                        StyleKindTextArea(), area.class_name);
    {
        TextInputMetrics metrics = ui_text_input_metrics_for_style(
            style, StyleKindTextArea(), area.class_name, 0);
        font = metrics.font;
        line_gap = metrics.line_gap;
    }
    line_h = TextLineHeight(font) + line_gap;
    if(line_h <= 0)
        return area.bounds;
    scroll_y = area.scroll_y != NULL ? *area.scroll_y : 0;
    first = scroll_y / line_h;
    scale = (float)Scale(1000) / 1000.0f;
    gutter_metrics = TextAreaGutterMetricsFor(scale);
    rows = TextAreaGutterRowsFor(area.bounds.height, line_h, gutter_metrics);
    active = TextBufferLineAtCursor(area.text, area.cursor_position != NULL
        ? *area.cursor_position : 0);
    total = TextBufferLineCount(area.text);
    gutter = (Rectangle){area.bounds.x, area.bounds.y, (float)gutter_width,
                         area.bounds.height};
    DrawRectangleRec(gutter, style.background);
    DrawLine((int)(gutter.x + gutter.width) - 1, (int)gutter.y,
             (int)(gutter.x + gutter.width) - 1,
             (int)(gutter.y + gutter.height), style.border);
    y = TextAreaGutterFirstY(gutter.y, scroll_y, line_h, gutter_metrics);
    for(int row = 0; row < rows; row++) {
        int line_no = first + row + 1;
        char label[24];

        if(line_no > total)
            break;
        if(line_no == active)
            DrawRectangle((int)gutter.x, y - gutter_metrics.active_y_inset,
                          (int)gutter.width,
                          line_h, style.border);
        snprintf(label, sizeof(label), "%d", line_no);
        Color inactive = ui_default_text_color();
        inactive.a = TextAreaGutterInactiveAlpha(inactive.a);
        RenderText(label, (int)gutter.x + gutter_metrics.label_x_inset, y,
                   gutter_metrics.label_font,
                   line_no == active ? style.text : inactive);
        y += line_h;
        if(y > (int)(gutter.y + gutter.height))
            break;
    }
    area.bounds.x += gutter_width;
    area.bounds.width -= gutter_width;
    return area.bounds;
}

int
ui_text_area_render(TextAreaProps area)
{
    char editor_id[96];
    Widget widget;
    int changed = 0;
    int focused;
    int font;
    int line_gap;
    int line_h;
    int padding_x;
    int padding_y;
    int first_line_y;
    int scroll_y;
    int wrap_width;
    int content_h;
    int max_scroll;
    int scrollbar_w;
    int reveal_cursor = 0;
    Vector2 mouse_world;
    int mouse_inside;
    int captured;
    int context_active = 0;
    int selection_start = 0;
    int selection_end = 0;
    int has_selection = 0;
    int selection_key_handled = 0;
    Color border;
    float radius;
    int enter_requested;
    int drag_id;
    int double_clicked = 0;
    int copy_pressed = 0;
    int cut_pressed = 0;
    int paste_pressed = 0;
    TextEdit area_edit;
    TextCompositionView composition = {0};
    const char *display_text;
    int display_cursor;
    int composition_start = 0;
    int composition_end = 0;
    int committed_selection_start;
    int committed_selection_end;

    if(area.text == NULL || area.text_size == 0 || area.cursor_position == NULL || area.focused == NULL)
        return 0;
    TextInputStyle style = ui_resolve_text_input_style((TextInputStyle){0},
        StyleKindTextArea(), area.class_name);
    memset(&area_edit, 0, sizeof(area_edit));
    area_edit.text = area.text;
    area_edit.text_size = area.text_size;
    area_edit.cursor_position = area.cursor_position;
    area_edit.max_codepoints = area.max_codepoints;

    widget = BeginWidget("TextArea",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "TextArea", area.focus_id,
                                                 area.placeholder),
                           area.bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
    area.bounds = widget.bounds;

    {
        TextInputMetrics metrics = ui_text_input_metrics_for_style(
            style, StyleKindTextArea(), area.class_name, 0);
        font = metrics.font;
        line_gap = metrics.line_gap;
        padding_x = metrics.padding_x;
        padding_y = metrics.padding_y;
    }
    line_h = TextLineHeight(font) + line_gap;
    wrap_width = TextAreaWrapWidthFor(area.bounds.width, padding_x,
        area.wrap != 0, TextAreaMinWrapWidth((float)Scale(1000) / 1000.0f));
    first_line_y = ControlTextBaselineY(TextControlBaselineSample(),
                                        (int)area.bounds.y + padding_y,
                                        line_h, font);
    focused = *area.focused != 0;
    focused = IsTextFocusOwner(area.focused) ? focused : 0;
    scroll_y = area.scroll_y != NULL ? *area.scroll_y : 0;

    if(area.focus_id > 0 && RegisterFocus(area.focus_id, area.bounds)) {
        focused = 1;
        ClaimTextAreaFocus(area.focused);
    }

    mouse_world = ui_mouse_world();
    mouse_inside = CheckCollisionPointRec(mouse_world, area.bounds);
    captured = InputCapturesClick(mouse_world);
    if(mouse_inside && !captured)
        MarkTextCursor();
    drag_id = area.focus_id > 0 ? area.focus_id : 1;
    changed |= ui_text_context_take_changed(TEXT_CONTEXT_AREA, drag_id,
                                            area.focused);
    context_active = ui_text_context_matches(TEXT_CONTEXT_AREA, drag_id,
                                             area.focused);
    if(context_active) {
        focused = 1;
        ClaimTextAreaFocus(area.focused);
    }

    if(ui_text_context_open_for(TEXT_CONTEXT_AREA, drag_id, area.focused,
                                area.bounds, mouse_world, captured)) {
        int clicked_cursor;
        int current_start = 0;
        int current_end = 0;

        focused = 1;
        ClaimTextAreaFocus(area.focused);
        clicked_cursor = ui_text_area_cursor_from_point(
            area.text, font, line_gap, wrap_width,
            (int)area.bounds.x + padding_x, (int)area.bounds.y + padding_y,
            (int)mouse_world.x, (int)mouse_world.y, scroll_y);
        if(ui_text_selection_matches(g_ui_text_area_selection, drag_id,
                                     area.focused))
            ui_selection_range(g_ui_text_area_selection, area.text,
                               &current_start, &current_end);
        if(clicked_cursor < current_start || clicked_cursor > current_end ||
           current_end <= current_start) {
            *area.cursor_position = clicked_cursor;
            ui_text_selection_set(&g_ui_text_area_selection, drag_id,
                                  area.focused, clicked_cursor,
                                  clicked_cursor, 0);
        }
    }

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(mouse_inside && !captured) {
            int clicked_cursor;
            double now = GetTime();
            int click_dx = (int)mouse_world.x - g_ui_text_area_last_click_x;
            int click_dy = (int)mouse_world.y - g_ui_text_area_last_click_y;
            float scale = (float)Scale(1000) / 1000.0f;
            int double_click_slop = TextInputDoubleClickSlopFor(scale);
            TextInputDoubleClickDecision click_decision;

            focused = 1;
            ClaimTextAreaFocus(area.focused);
            g_ui_text_input_show_requested = 1;
            clicked_cursor = ui_text_area_cursor_from_point(area.text, font, line_gap,
                wrap_width,
                (int)area.bounds.x + padding_x, (int)area.bounds.y + padding_y,
                (int)mouse_world.x, (int)mouse_world.y, scroll_y);
            *area.cursor_position = clicked_cursor;
            click_decision = TextInputDoubleClickDecisionFor(
                g_ui_text_area_last_click_owner == area.focused,
                g_ui_text_area_last_click_id == drag_id,
                (float)(now - g_ui_text_area_last_click_time),
                click_dx, click_dy, double_click_slop);
            if(click_decision.double_click) {
                int line_start;
                int line_end;

                if(ui_text_area_select_line(area.text, clicked_cursor,
                                            &line_start, &line_end)) {
                    ui_text_selection_set(&g_ui_text_area_selection, drag_id,
                                          area.focused, line_start, line_end,
                                          0);
                    *area.cursor_position = line_end;
                    double_clicked = 1;
                    g_ui_text_area_drag_id = 0;
                    g_ui_text_area_drag_owner = NULL;
                }
            }
            if(!double_clicked) {
                g_ui_text_area_drag_id = drag_id;
                g_ui_text_area_drag_owner = area.focused;
                ui_text_selection_set(&g_ui_text_area_selection, drag_id,
                                      area.focused, *area.cursor_position,
                                      *area.cursor_position, 1);
            }
            g_ui_text_area_last_click_id = drag_id;
            g_ui_text_area_last_click_owner = area.focused;
            g_ui_text_area_last_click_cursor = clicked_cursor;
            g_ui_text_area_last_click_x = (int)mouse_world.x;
            g_ui_text_area_last_click_y = (int)mouse_world.y;
            g_ui_text_area_last_click_time = now;
        } else if(TextOutsideClickShouldBlur(focused, context_active,
                                             g_ui_scroll_gesture_pending)) {
            focused = 0;
            ReleaseTextFocus(area.focused, area.focus_id);
        }
    }
    if(g_ui_text_area_drag_owner == area.focused &&
       IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        scroll_y = TextAreaDragScrollFor(scroll_y, (int)mouse_world.y,
                                         area.bounds.y, area.bounds.height,
                                         padding_y, line_h);
        focused = 1;
        ClaimTextAreaFocus(area.focused);
        *area.cursor_position = ui_text_area_cursor_from_point(area.text, font, line_gap,
            wrap_width,
            (int)area.bounds.x + padding_x, (int)area.bounds.y + padding_y,
            (int)mouse_world.x, (int)mouse_world.y, scroll_y);
        if(ui_text_selection_matches(g_ui_text_area_selection, drag_id,
                                     area.focused)) {
            g_ui_text_area_selection.cursor = *area.cursor_position;
            g_ui_text_area_selection.dragging = 1;
        }
    }
    if(g_ui_text_area_drag_owner == area.focused &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_text_area_drag_id = 0;
        g_ui_text_area_drag_owner = NULL;
        if(ui_text_selection_matches(g_ui_text_area_selection, drag_id,
                                     area.focused))
            g_ui_text_area_selection.dragging = 0;
    }

    *area.focused = focused;
    SetFocusTextInputActive(focused && !area.read_only);
    if(TextEscapeShouldBlur(focused, IsKeyboardInputEnabled(),
       IsKeyPressed(KEY_ESCAPE))) {
        focused = 0;
        ReleaseTextFocus(area.focused, area.focus_id);
        *area.focused = 0;
        SetFocusTextInputActive(0);
    }
    copy_pressed = IsKeyPressed(KEY_C);
    cut_pressed = IsKeyPressed(KEY_X);
    paste_pressed = IsKeyPressed(KEY_V);
    enter_requested = focused && IsKeyboardInputEnabled() &&
                      (IsKeyPressed(KEY_ENTER) ||
                       IsKeyPressed(KEY_KP_ENTER) ||
                       g_ui_text_input_enter_count > 0);
    has_selection = ui_text_selection_matches(g_ui_text_area_selection, drag_id,
                                              area.focused);
    if(TextShortcutShouldClaimSelectionFocus(has_selection, focused,
       IsKeyboardInputEnabled(), ui_mod_key_down(), copy_pressed, cut_pressed,
       paste_pressed)) {
        focused = 1;
        ClaimTextAreaFocus(area.focused);
        *area.focused = 1;
    }
    if(TextSelectionRangeShouldResolve(focused, context_active,
       has_selection)) {
        TextSelectionRange range = TextSelectionRangeForLength(
            g_ui_text_area_selection.anchor, g_ui_text_area_selection.cursor,
            (int)strlen(area.text));
        selection_start = range.start;
        selection_end = range.end;
    }
    {
        int anchor = has_selection
            ? g_ui_text_area_selection.anchor : *area.cursor_position;
        TextCompositionResult composition_result = ui_text_composition_apply(
            area_edit, &anchor, area.focused, focused,
            area.read_only || !IsKeyboardInputEnabled(), 1);

        changed |= composition_result.text_changed;
        if(composition_result.selection_changed) {
            TextSelectionState collapsed = ui_text_selection_collapsed(
                *area.cursor_position);
            ui_text_selection_set(&g_ui_text_area_selection, drag_id,
                                  area.focused, anchor,
                                  *area.cursor_position, 0);
            has_selection = 1;
            selection_start = collapsed.anchor;
            selection_end = collapsed.cursor;
        }
    }
    if(TextKeyboardShouldRun(focused, IsKeyboardInputEnabled())) {
        if(ui_mod_key_down() && IsKeyPressed(KEY_A)) {
            TextContextCommandDecision decision =
                TextEditCommandDecisionFor(TextContextCommandSelectAll(),
                    selection_end > selection_start, 0, 0,
                    area.text[0] != '\0', 1, !area.read_only,
                    !area.read_only);
            if(decision.select_all) {
                TextSelectionState all = TextSelectionAll((int)strlen(area.text));

                ui_text_selection_set(&g_ui_text_area_selection, drag_id,
                                      area.focused, all.anchor, all.cursor, 0);
                *area.cursor_position = all.cursor;
                selection_start = all.anchor;
                selection_end = all.cursor;
            }
            selection_key_handled = 1;
        }
        if(ui_mod_key_down() && copy_pressed) {
            TextContextCommandDecision decision =
                TextEditCommandDecisionFor(TextContextCommandCopy(),
                    selection_end > selection_start, 0, 0,
                    area.text[0] != '\0', 1, !area.read_only,
                    !area.read_only);
            if(decision.copy_selection)
                ui_text_copy_range(area.text, selection_start, selection_end);
            selection_key_handled |= decision.copy_selection;
        }
        if(ui_mod_key_down() && cut_pressed) {
            TextContextCommandDecision decision =
                TextEditCommandDecisionFor(TextContextCommandCut(),
                    selection_end > selection_start, 0, 0,
                    area.text[0] != '\0', !area.read_only, !area.read_only,
                    !area.read_only);
            int copied_selection = 0;
            if(decision.copy_selection)
                copied_selection = ui_text_copy_range(area.text,
                                                      selection_start,
                                                      selection_end);
            if(decision.delete_selection && copied_selection &&
               ui_text_delete_range(area.text, area.text_size,
                                    area.cursor_position, selection_start,
                                    selection_end)) {
                ui_text_selection_set_collapsed(&g_ui_text_area_selection,
                                                drag_id, area.focused,
                                                *area.cursor_position, 0);
                changed = 1;
            }
            selection_key_handled |= decision.copy_selection ||
                                     decision.delete_selection ||
                                     decision.collapse_selection;
        }
        if(ui_mod_key_down() && paste_pressed) {
            TextContextCommandDecision decision =
                TextEditCommandDecisionFor(TextContextCommandPaste(),
                    selection_end > selection_start, 0, 0,
                    area.text[0] != '\0', 1, !area.read_only,
                    !area.read_only);
            if(decision.delete_selection)
                ui_text_delete_range(area.text, area.text_size,
                                     area.cursor_position, selection_start,
                                     selection_end);
            if(decision.paste && ui_text_paste_clipboard(area_edit, 1))
                changed = 1;
            if(decision.collapse_selection) {
                ui_text_selection_set_collapsed(&g_ui_text_area_selection,
                                                drag_id, area.focused,
                                                *area.cursor_position, 0);
            }
            selection_key_handled |= decision.delete_selection ||
                                     decision.paste ||
                                     decision.collapse_selection;
        }
        if(TextDeleteShortcutShouldRun(area.read_only,
           IsKeyPressed(KEY_BACKSPACE), IsKeyPressed(KEY_DELETE),
           g_ui_text_input_backspace_count)) {
            int anchor = has_selection
                ? g_ui_text_area_selection.anchor : *area.cursor_position;
            int backspace_count = g_ui_text_input_backspace_count +
                                  ui_backspace_repeat_count();
            int delete_count = IsKeyPressed(KEY_DELETE) ? 1 : 0;

            while(backspace_count-- > 0)
                changed |= ui_text_delete_key(
                    area.text, area.text_size, &anchor, area.cursor_position,
                    TextDeleteBackspace(), ui_mod_key_down(), 0);
            while(delete_count-- > 0)
                changed |= ui_text_delete_key(
                    area.text, area.text_size, &anchor, area.cursor_position,
                    TextDeleteForward(), ui_mod_key_down(), 0);
            ui_text_selection_set_collapsed(&g_ui_text_area_selection, drag_id,
                                            area.focused,
                                            *area.cursor_position, 0);
            g_ui_text_input_backspace_count = 0;
            selection_key_handled = 1;
        }
        if(ui_mod_key_down() &&
           (copy_pressed || cut_pressed || paste_pressed))
            selection_key_handled = 1;
        if(TextSelectionReplacementShouldRun(area.read_only,
           selection_key_handled, selection_end > selection_start,
           ui_mod_key_down())) {
            int inserted = 0;
            int deleted_selection = 0;
            int codepoint = GetCharPressed();

            while(codepoint > 0) {
                if(!deleted_selection) {
                    ui_text_delete_range(area.text, area.text_size,
                                         area.cursor_position,
                                         selection_start, selection_end);
                    deleted_selection = 1;
                }
                if(ui_text_insert_codepoint(area.text, area.text_size,
                                            area.cursor_position, codepoint,
                                            area.max_codepoints))
                    changed = 1;
                inserted = 1;
                codepoint = GetCharPressed();
            }
            for(int i = 0; i < g_ui_text_input_codepoint_count; i++) {
                codepoint = g_ui_text_input_codepoints[i];
                if(!deleted_selection) {
                    ui_text_delete_range(area.text, area.text_size,
                                         area.cursor_position,
                                         selection_start, selection_end);
                    deleted_selection = 1;
                }
                if(ui_text_insert_codepoint(area.text, area.text_size,
                                            area.cursor_position, codepoint,
                                            area.max_codepoints))
                    changed = 1;
                inserted = 1;
            }
            if(g_ui_text_input_codepoint_count > 0)
                g_ui_text_input_codepoint_count = 0;
            if(enter_requested) {
                if(!deleted_selection) {
                    ui_text_delete_range(area.text, area.text_size,
                                         area.cursor_position,
                                         selection_start, selection_end);
                    deleted_selection = 1;
                }
                if(ui_text_insert_newline(area.text, area.text_size,
                                          area.cursor_position,
                                          area.max_codepoints)) {
                    changed = 1;
                }
                g_ui_text_input_enter_count = 0;
                enter_requested = 0;
                inserted = 1;
            }
            if(inserted) {
                ui_text_selection_set_collapsed(&g_ui_text_area_selection,
                                                drag_id, area.focused,
                                                *area.cursor_position, 0);
                selection_key_handled = 1;
            }
        }
        if(TextNavigationShouldRun(selection_key_handled)) {
            int shift = IsKeyDown(KEY_LEFT_SHIFT) ||
                        IsKeyDown(KEY_RIGHT_SHIFT);
            int cursor = *area.cursor_position;
            int anchor = cursor;
            int navigation_key = ui_text_navigation_key(1);
            TextNavigationInput navigation = {
                .text = area.text,
                .area = &area,
                .font = font,
                .key = navigation_key,
                .shift = shift,
                .modifier = ui_mod_key_down()
            };

            if(ui_text_selection_matches(g_ui_text_area_selection, drag_id,
                                         area.focused))
                anchor = g_ui_text_area_selection.anchor;
            if(ui_text_navigate(navigation, &anchor, &cursor)) {
                TextSelectionRange range = TextSelectionRangeForLength(
                    anchor, cursor, (int)strlen(area.text));
                *area.cursor_position = cursor;
                ui_text_selection_set(&g_ui_text_area_selection, drag_id,
                                      area.focused, anchor, cursor, 0);
                selection_start = range.start;
                selection_end = range.end;
                selection_key_handled = 1;
            }
        }
        if(TextNativeEditShouldRun(area.read_only, selection_key_handled)) {
            changed |= EditText(area_edit);
        }
        if(TextAreaEnterNewlineShouldRun(area.read_only, enter_requested)) {
            if(ui_text_insert_newline(area.text, area.text_size,
                                      area.cursor_position,
                                      area.max_codepoints)) {
                changed = 1;
            }
            g_ui_text_input_enter_count = 0;
        }
        if(TextAreaChangedShouldCollapseSelection(changed,
           selection_key_handled)) {
            if(!g_ui_text_area_selection.dragging) {
                ui_text_selection_set_collapsed(&g_ui_text_area_selection,
                                                drag_id, area.focused,
                                                *area.cursor_position, 0);
            }
        }
    } else {
        int text_len = (int)strlen(area.text);
        *area.cursor_position = TextCursorForLength(*area.cursor_position,
                                                    text_len);
    }

    if(ui_text_selection_matches(g_ui_text_area_selection, drag_id,
                                 area.focused))
        ui_selection_range(g_ui_text_area_selection, area.text,
                           &selection_start, &selection_end);
    else
    {
        TextSelectionState collapsed = ui_text_selection_collapsed(
            *area.cursor_position);
        selection_start = collapsed.anchor;
        selection_end = collapsed.cursor;
    }

    committed_selection_start = selection_start;
    committed_selection_end = selection_end;

    display_text = area.text;
    display_cursor = *area.cursor_position;
    {
        const char *preedit = NULL;
        int preedit_cursor = 0;
        int preedit_selection_length = 0;

        if(ui_text_composition_get(area.focused, &preedit, &preedit_cursor,
                                   &preedit_selection_length) &&
           ui_text_composition_view(
               area.text, selection_start, selection_end, preedit,
               preedit_cursor, preedit_selection_length, &composition)) {
            display_text = composition.text;
            display_cursor = composition.cursor;
            selection_start = composition.selection_start;
            selection_end = composition.selection_end;
            composition_start = composition.composition_start;
            composition_end = composition.composition_end;
        }
    }

    ui_text_context_register_target(TEXT_CONTEXT_AREA, drag_id,
                                    area.focused, area.text, area.text_size,
                                    area.cursor_position,
                                    area.max_codepoints, NULL, NULL,
                                    &g_ui_text_area_selection,
                                    committed_selection_start,
                                    committed_selection_end, 1, 0,
                                    area.read_only);

    content_h = ui_text_area_content_height(display_text, font, line_gap,
                                            wrap_width,
                                            composition.text != NULL
                                                ? 0 : area.content_version,
                                            changed);
    max_scroll = TextAreaMaxScrollFor(content_h, area.bounds.height,
                                      padding_y);
    scrollbar_w = TextAreaScrollbarShouldShow(area.scroll_y != NULL,
        max_scroll)
        ? TextAreaScrollbarWidthFor((float)Scale(1000) / 1000.0f)
        : 0;
    if(TextAreaWheelShouldScroll(mouse_inside, captured, ui_mod_key_down()))
        scroll_y = TextAreaWheelScrollFor(scroll_y, GetMouseWheelMove(),
                                          line_h);
    reveal_cursor = TextAreaRevealCursorShouldRun(focused, changed,
        IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
        IsKeyPressed(KEY_HOME) || IsKeyPressed(KEY_END) ||
        IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) ||
        IsKeyPressed(KEY_PAGE_UP) || IsKeyPressed(KEY_PAGE_DOWN));
    if(reveal_cursor) {
        int cursor_h = TextLineHeight(font);
        int cursor_y = ui_text_area_cursor_y(
            display_text, display_cursor, font, line_gap, wrap_width,
            &cursor_h);
        int viewport_h = (int)area.bounds.height - padding_y * 2;

        scroll_y = TextAreaRevealScroll(scroll_y, cursor_y, cursor_h,
                                        viewport_h);
    }
    scroll_y = TextAreaScrollFor(scroll_y, max_scroll);
    if(area.scroll_y != NULL)
        *area.scroll_y = scroll_y;

    /* Keep editing usable without a renderer, as TextField already does. */
    if(!IsWindowReady()) {
        ui_text_composition_view_free(&composition);
        EndWidget(&widget);
        return changed;
    }
    border = focused ? style.focus_border : style.border;
    radius = style.radius >= 0.0f ? style.radius : 0.12f;
    ui_draw_box_background(area.bounds, radius, style.background, border);

    ui_begin_world_clip((Rectangle){area.bounds.x + padding_x, area.bounds.y + padding_y,
                                    area.bounds.width - padding_x * 2 - scrollbar_w,
                                    area.bounds.height - padding_y * 2});
    if(area.text[0] == '\0' && !focused && area.placeholder != NULL)
        RenderText(area.placeholder, (int)area.bounds.x + padding_x,
                   first_line_y, font, style.border);
    else
        ui_draw_text_area_text(display_text, display_cursor,
                               focused && !area.read_only,
                               area.bounds, font, line_gap, scroll_y,
                               wrap_width,
                               area.syntax, style, selection_start,
                               selection_end, composition_start,
                               composition_end);
    EndClip();
    if(area.scroll_y != NULL && max_scroll > 0) {
        ui_scrollbar((int)(area.bounds.x + area.bounds.width - scrollbar_w),
                     (int)area.bounds.y,
                     (int)area.bounds.height,
                     content_h + padding_y * 2,
                     area.scroll_y,
                     max_scroll,
                     0);
    }
    ui_text_composition_view_free(&composition);
    EndWidget(&widget);
    return changed;
}

int
GetTextAreaSelection(int focus_id, int *start, int *end)
{
    TextSelectionRange selection;

    if(start != NULL)
        *start = 0;
    if(end != NULL)
        *end = 0;
    if(focus_id <= 0 || g_ui_text_area_selection.id != focus_id)
        return 0;
    selection = TextSelectionRangeFor(g_ui_text_area_selection.anchor,
                                      g_ui_text_area_selection.cursor);
    if(!selection.has_selection)
        return 0;
    if(start != NULL)
        *start = selection.start;
    if(end != NULL)
        *end = selection.end;
    return 1;
}

void
SetTextAreaSelection(int focus_id, int anchor, int cursor)
{
    if(focus_id <= 0)
        return;
    ui_text_selection_set(&g_ui_text_area_selection, focus_id, NULL,
                          anchor, cursor, 0);
}

int
ui_text_field_render(TextFieldProps field)
{
    return ui_text_field_render_filtered(field, NULL, NULL);
}

int
ui_text_field_render_filtered(TextFieldProps field,
                              TextInputFilter filter,
                              void *filter_user_data)
{
    enum { TEXT_FIELD_FALLBACK_FOCUS_SLOTS = 128 };
    typedef struct {
        unsigned int key;
        int focused;
    } TextFieldFallbackFocus;
    static TextFieldFallbackFocus fallback_focus[TEXT_FIELD_FALLBACK_FOCUS_SLOTS];
    char editor_id[96];
    Widget widget;
    int changed = 0;
    int focused;
    int font;
    int padding_x;
    int commit_pressed = 0;
    int selection_start = 0;
    int selection_end = 0;
    int selection_handled = 0;
    Vector2 mouse_world;
    int mouse_inside;
    int captured;
    int context_active = 0;
    const char *display_text;
    char *masked_text = NULL;
    TextEdit field_edit;
    int *scroll_x_ptr;
    int clip_w;
    int text_w;
    int max_scroll_x;
    int text_origin_x;
    int panning_field = 0;
    TextCompositionView composition = {0};
    int paint_cursor;
    int composition_start = 0;
    int composition_end = 0;
    int committed_selection_start;
    int committed_selection_end;
    TextInputMetrics metrics;
    TextFieldScroll scroll_policy;
    TextInputStyle layout_style;

    if(field.commit_pressed != NULL)
        *field.commit_pressed = 0;
    if(field.text == NULL || field.text_size == 0 ||
       field.cursor_position == NULL)
        return 0;
    if(field.focused == NULL) {
        unsigned int key = (unsigned int)field.focus_id;
        int slot = -1;
        int empty_slot = -1;

        if(key == 0) {
            key = 2166136261u;
            key = (key ^ (unsigned int)(int)field.bounds.x) * 16777619u;
            key = (key ^ (unsigned int)(int)field.bounds.y) * 16777619u;
            key = (key ^ (unsigned int)(int)field.bounds.width) * 16777619u;
            key = (key ^ (unsigned int)(int)field.bounds.height) * 16777619u;
        }
        if(key == 0)
            key = 1;

        for(int i = 0; i < TEXT_FIELD_FALLBACK_FOCUS_SLOTS; i++) {
            if(fallback_focus[i].key == key) {
                slot = i;
                break;
            }
            if(fallback_focus[i].key == 0 && empty_slot < 0)
                empty_slot = i;
        }
        if(slot < 0) {
            slot = empty_slot >= 0
                       ? empty_slot
                       : (int)(key % TEXT_FIELD_FALLBACK_FOCUS_SLOTS);
            fallback_focus[slot].key = key;
            fallback_focus[slot].focused = 0;
        }
        field.focused = &fallback_focus[slot].focused;
    }
    memset(&field_edit, 0, sizeof(field_edit));
    field_edit.text = field.text;
    field_edit.text_size = field.text_size;
    field_edit.cursor_position = field.cursor_position;
    field_edit.max_codepoints = field.max_codepoints;
    field_edit.filter = filter;
    field_edit.filter_user_data = filter_user_data;
    field_edit.commit_pressed = field.commit_pressed != NULL
                                  ? field.commit_pressed
                                  : &commit_pressed;

    display_text = field.text;
    if(field.secure) {
        size_t len = strlen(field.text);

        display_text = "";
        masked_text = malloc(len + 1);
        if(masked_text != NULL) {
            memset(masked_text, '*', len);
            masked_text[len] = '\0';
            display_text = masked_text;
        }
    }

    widget = BeginWidget("TextField",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "TextField",
                                                 field.focus_id, NULL),
                           field.bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
    field.bounds = widget.bounds;

    layout_style = ui_resolve_text_input_style((TextInputStyle){0},
                                               StyleKindTextField(),
                                               field.class_name);
    metrics = ui_text_input_metrics_for_style(layout_style, StyleKindTextField(),
                                              field.class_name, 0);
    font = metrics.font;
    padding_x = metrics.padding_x;
    focused = *field.focused != 0;
    focused = IsTextFocusOwner(field.focused) ? focused : 0;
    scroll_x_ptr = ui_text_field_scroll_for(field.focus_id, field.focused);
    text_w = TextWidth(display_text, font);
    scroll_policy = TextFieldScrollFor(field.bounds.x, field.bounds.width,
                                       padding_x, text_w, *scroll_x_ptr);
    clip_w = scroll_policy.clip_width;
    max_scroll_x = scroll_policy.max_scroll;
    *scroll_x_ptr = scroll_policy.scroll;
    text_origin_x = scroll_policy.text_origin_x;

    if(field.focus_id > 0 && RegisterFocus(field.focus_id, field.bounds)) {
        focused = 1;
        ClaimTextFieldFocus(field.focused);
    }

    mouse_world = ui_mouse_world();
    mouse_inside = CheckCollisionPointRec(mouse_world, field.bounds);
    captured = InputCapturesClick(mouse_world);
    if(mouse_inside && !captured)
        MarkTextCursor();
    changed |= ui_text_context_take_changed(TEXT_CONTEXT_FIELD,
                                            field.focus_id, field.focused);
    context_active = ui_text_context_matches(TEXT_CONTEXT_FIELD,
                                             field.focus_id, field.focused);
    if(context_active) {
        focused = 1;
        ClaimTextFieldFocus(field.focused);
    }

    if(!field.secure &&
       ui_text_context_open_for(TEXT_CONTEXT_FIELD, field.focus_id,
                                field.focused, field.bounds, mouse_world,
                                captured)) {
        int clicked_cursor;
        int current_start = 0;
        int current_end = 0;

        focused = 1;
        ClaimTextFieldFocus(field.focused);
        clicked_cursor = ui_text_cursor_from_x(
            display_text, font, text_origin_x, (int)mouse_world.x);
        if(ui_text_selection_matches(g_ui_text_field_selection,
                                     field.focus_id, field.focused))
            ui_selection_range(g_ui_text_field_selection, field.text,
                               &current_start, &current_end);
        if(clicked_cursor < current_start || clicked_cursor > current_end ||
           current_end <= current_start) {
            *field.cursor_position = clicked_cursor;
            ui_text_selection_set(&g_ui_text_field_selection, field.focus_id,
                                  field.focused, clicked_cursor,
                                  clicked_cursor, 0);
        }
    }

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(mouse_inside && !captured) {
            double now = GetTime();
            int click_dx = (int)mouse_world.x - g_ui_text_field_last_click_x;
            int click_dy = (int)mouse_world.y - g_ui_text_field_last_click_y;
            float scale = (float)Scale(1000) / 1000.0f;
            int double_click_slop = TextInputDoubleClickSlopFor(scale);
            TextInputDoubleClickDecision click_decision =
                TextInputDoubleClickDecisionFor(
                    g_ui_text_field_last_click_owner == field.focused,
                    g_ui_text_field_last_click_id == field.focus_id,
                    (float)(now - g_ui_text_field_last_click_time),
                    click_dx, click_dy, double_click_slop);
            focused = 1;
            ClaimTextFieldFocus(field.focused);
            g_ui_text_input_show_requested = 1;
            if(max_scroll_x > 0) {
                g_ui_text_field_pan_id = field.focus_id;
                g_ui_text_field_pan_owner = field.focused;
                g_ui_text_field_pan_start_x = (int)mouse_world.x;
                g_ui_text_field_pan_start_y = (int)mouse_world.y;
                g_ui_text_field_pan_start_scroll = *scroll_x_ptr;
                g_ui_text_field_panning = 0;
            }
            if(click_decision.double_click) {
                int len = (int)strlen(field.text);
                *field.cursor_position = len;
                ui_text_selection_set(&g_ui_text_field_selection, field.focus_id,
                                      field.focused, 0, len, 0);
                g_ui_text_field_drag_id = 0;
                g_ui_text_field_drag_owner = NULL;
            } else {
                *field.cursor_position = ui_text_cursor_from_x(
                    display_text, font, text_origin_x, (int)mouse_world.x);
                g_ui_text_field_drag_id = field.focus_id;
                g_ui_text_field_drag_owner = field.focused;
                ui_text_selection_set(&g_ui_text_field_selection, field.focus_id,
                                      field.focused, *field.cursor_position,
                                      *field.cursor_position, 1);
            }
            g_ui_text_field_last_click_id = field.focus_id;
            g_ui_text_field_last_click_owner = field.focused;
            g_ui_text_field_last_click_x = (int)mouse_world.x;
            g_ui_text_field_last_click_y = (int)mouse_world.y;
            g_ui_text_field_last_click_time = now;
        } else if(TextOutsideClickShouldBlur(focused, context_active,
                                             g_ui_scroll_gesture_pending)) {
            focused = 0;
            ReleaseTextFocus(field.focused, field.focus_id);
        }
    }
    if(g_ui_text_field_pan_owner == field.focused &&
       (field.focus_id <= 0 || g_ui_text_field_pan_id == field.focus_id) &&
       IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       (g_ui_pointer_owner == POINTER_OWNER_NONE ||
        g_ui_pointer_owner == POINTER_OWNER_TEXT_FIELD_PAN)) {
        int dx = (int)mouse_world.x - g_ui_text_field_pan_start_x;
        int dy = (int)mouse_world.y - g_ui_text_field_pan_start_y;
        float scale = (float)Scale(1000) / 1000.0f;
        int drag_threshold = TextFieldPanDragThresholdFor(scale);
        TextFieldPanDecision pan_decision = TextFieldPanDecisionFor(
            g_ui_text_field_panning != 0, dx, dy, drag_threshold);

        if(pan_decision.pan) {
            Rectangle capture = {
                0.0f,
                0.0f,
                (float)ui_view_width,
                (float)ui_view_height
            };

            g_ui_pointer_owner = POINTER_OWNER_TEXT_FIELD_PAN;
            g_ui_text_field_panning = 1;
            *scroll_x_ptr = g_ui_text_field_pan_start_scroll - dx;
            scroll_policy = TextFieldScrollFor(field.bounds.x,
                                               field.bounds.width, padding_x,
                                               text_w, *scroll_x_ptr);
            clip_w = scroll_policy.clip_width;
            max_scroll_x = scroll_policy.max_scroll;
            *scroll_x_ptr = scroll_policy.scroll;
            text_origin_x = scroll_policy.text_origin_x;
            g_ui_text_field_drag_id = 0;
            g_ui_text_field_drag_owner = NULL;
            g_ui_text_field_selection.dragging = 0;
            PushInputCapture(capture, 0);
        }
    }
    panning_field = g_ui_text_field_pan_owner == field.focused &&
                    (field.focus_id <= 0 ||
                     g_ui_text_field_pan_id == field.focus_id) &&
                    g_ui_text_field_panning;
    if(g_ui_text_field_drag_owner == field.focused &&
       !panning_field &&
       IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        focused = 1;
        ClaimTextFieldFocus(field.focused);
        *field.cursor_position = ui_text_cursor_from_x(
            display_text, font, text_origin_x, (int)mouse_world.x);
        g_ui_text_field_selection.cursor = *field.cursor_position;
        g_ui_text_field_selection.dragging = 1;
    }
    if(g_ui_text_field_drag_owner == field.focused &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_text_field_drag_id = 0;
        g_ui_text_field_drag_owner = NULL;
        g_ui_text_field_selection.dragging = 0;
    }
    if(g_ui_text_field_pan_owner == field.focused &&
       (field.focus_id <= 0 || g_ui_text_field_pan_id == field.focus_id) &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_text_field_pan_id = 0;
        g_ui_text_field_pan_owner = NULL;
        g_ui_text_field_panning = 0;
    }

    *field.focused = focused;
    SetFocusTextInputActive(focused && !field.read_only);
    if(TextEscapeShouldBlur(focused, IsKeyboardInputEnabled(),
       IsKeyPressed(KEY_ESCAPE))) {
        focused = 0;
        ReleaseTextFocus(field.focused, field.focus_id);
        *field.focused = 0;
        SetFocusTextInputActive(0);
    }
    if(TextSelectionRangeShouldResolve(focused, context_active,
       ui_text_selection_matches(g_ui_text_field_selection,
                                 field.focus_id, field.focused)))
        ui_selection_range(g_ui_text_field_selection, field.text,
                           &selection_start, &selection_end);

    {
        int anchor = ui_text_selection_matches(
            g_ui_text_field_selection, field.focus_id, field.focused)
            ? g_ui_text_field_selection.anchor : *field.cursor_position;
        TextCompositionResult composition_result = ui_text_composition_apply(
            field_edit, &anchor, field.focused, focused,
            field.read_only || !IsKeyboardInputEnabled(), 0);

        changed |= composition_result.text_changed;
        if(composition_result.selection_changed) {
            TextSelectionState collapsed = ui_text_selection_collapsed(
                *field.cursor_position);
            ui_text_selection_set(&g_ui_text_field_selection,
                                  field.focus_id, field.focused, anchor,
                                  *field.cursor_position, 0);
            selection_start = collapsed.anchor;
            selection_end = collapsed.cursor;
        }
    }

    if(TextKeyboardShouldRun(focused, IsKeyboardInputEnabled())) {
        if(ui_mod_key_down() && IsKeyPressed(KEY_A)) {
            TextContextCommandDecision decision =
                TextEditCommandDecisionFor(TextContextCommandSelectAll(),
                    selection_end > selection_start, 0, 0,
                    field.text[0] != '\0', !field.secure,
                    !field.read_only, !field.read_only);
            if(decision.select_all) {
                TextSelectionState all = TextSelectionAll((int)strlen(field.text));

                ui_text_selection_set(&g_ui_text_field_selection, field.focus_id,
                                      field.focused, all.anchor, all.cursor, 0);
                *field.cursor_position = all.cursor;
                selection_start = all.anchor;
                selection_end = all.cursor;
            }
            selection_handled = 1;
        }
        if(ui_mod_key_down() && IsKeyPressed(KEY_C)) {
            TextContextCommandDecision decision =
                TextEditCommandDecisionFor(TextContextCommandCopy(),
                    selection_end > selection_start, 1, 0,
                    1, !field.secure,
                    !field.read_only, !field.read_only);
            if(decision.copy_selection)
                ui_text_copy_range(field.text, selection_start, selection_end);
            if(decision.copy_all)
                SetClipboardTextValue(field.text);
            selection_handled |= decision.copy_selection || decision.copy_all;
        }
        if(ui_mod_key_down() && IsKeyPressed(KEY_X)) {
            TextContextCommandDecision decision =
                TextEditCommandDecisionFor(TextContextCommandCut(),
                    selection_end > selection_start, 1, 1,
                    1, !field.secure,
                    !field.read_only && !field.secure, !field.read_only);
            int copied_selection = 0;
            if(decision.copy_selection)
                copied_selection = ui_text_copy_range(field.text,
                                                      selection_start,
                                                      selection_end);
            if(decision.copy_all)
                SetClipboardTextValue(field.text);
            if(decision.delete_selection && copied_selection &&
               ui_text_delete_range(field.text, field.text_size,
                                    field.cursor_position,
                                    selection_start, selection_end))
                changed = 1;
            if(decision.clear_all) {
                field.text[0] = '\0';
                *field.cursor_position = 0;
                changed = 1;
            }
            if(decision.collapse_selection) {
                TextSelectionState collapsed = ui_text_selection_collapsed(
                    *field.cursor_position);
                ui_text_selection_set_collapsed(&g_ui_text_field_selection,
                                                field.focus_id, field.focused,
                                                *field.cursor_position, 0);
                selection_start = collapsed.anchor;
                selection_end = collapsed.cursor;
            }
            selection_handled |= decision.copy_selection ||
                                 decision.copy_all ||
                                 decision.delete_selection ||
                                 decision.clear_all ||
                                 decision.collapse_selection;
        }
        if(ui_mod_key_down() && IsKeyPressed(KEY_V)) {
            TextContextCommandDecision decision =
                TextEditCommandDecisionFor(TextContextCommandPaste(),
                    selection_end > selection_start, 0, 0,
                    field.text[0] != '\0', 1, !field.read_only,
                    !field.read_only);
            if(decision.delete_selection)
                ui_text_delete_range(field.text, field.text_size,
                                     field.cursor_position, selection_start,
                                     selection_end);
            if(decision.paste && ui_text_paste_clipboard(field_edit, 0))
                changed = 1;
            if(decision.collapse_selection) {
                TextSelectionState collapsed = ui_text_selection_collapsed(
                    *field.cursor_position);
                ui_text_selection_set_collapsed(&g_ui_text_field_selection,
                                                field.focus_id, field.focused,
                                                *field.cursor_position, 0);
                selection_start = collapsed.anchor;
                selection_end = collapsed.cursor;
            }
            selection_handled |= decision.delete_selection ||
                                 decision.paste ||
                                 decision.collapse_selection;
        }
        if(TextDeleteShortcutShouldRun(field.read_only,
           IsKeyPressed(KEY_BACKSPACE), IsKeyPressed(KEY_DELETE),
           g_ui_text_input_backspace_count)) {
            int anchor = ui_text_selection_matches(
                g_ui_text_field_selection, field.focus_id, field.focused)
                ? g_ui_text_field_selection.anchor : *field.cursor_position;
            int backspace_count = g_ui_text_input_backspace_count +
                                  ui_backspace_repeat_count();
            int delete_count = IsKeyPressed(KEY_DELETE) ? 1 : 0;

            while(backspace_count-- > 0)
                changed |= ui_text_delete_key(
                    field.text, field.text_size, &anchor,
                    field.cursor_position, TextDeleteBackspace(),
                    ui_mod_key_down(), field.secure);
            while(delete_count-- > 0)
                changed |= ui_text_delete_key(
                    field.text, field.text_size, &anchor,
                    field.cursor_position, TextDeleteForward(),
                    ui_mod_key_down(), field.secure);
            g_ui_text_input_backspace_count = 0;
            {
                TextSelectionState collapsed = ui_text_selection_collapsed(
                    *field.cursor_position);
                ui_text_selection_set_collapsed(&g_ui_text_field_selection,
                                                field.focus_id, field.focused,
                                                *field.cursor_position, 0);
                selection_start = collapsed.anchor;
                selection_end = collapsed.cursor;
            }
            selection_handled = 1;
        }
        if(TextNavigationShouldRun(selection_handled)) {
            int shift = IsKeyDown(KEY_LEFT_SHIFT) ||
                        IsKeyDown(KEY_RIGHT_SHIFT);
            int cursor = *field.cursor_position;
            int anchor = cursor;
            TextNavigationInput navigation = {
                .text = field.text,
                .key = ui_text_navigation_key(0),
                .shift = shift,
                .modifier = ui_mod_key_down(),
                .secure = field.secure
            };

            if(ui_text_selection_matches(g_ui_text_field_selection,
                                         field.focus_id, field.focused))
                anchor = g_ui_text_field_selection.anchor;
            if(ui_text_navigate(navigation, &anchor, &cursor)) {
                TextSelectionRange range = TextSelectionRangeForLength(
                    anchor, cursor, (int)strlen(field.text));
                *field.cursor_position = cursor;
                ui_text_selection_set(&g_ui_text_field_selection,
                                      field.focus_id, field.focused,
                                      anchor, cursor, 0);
                selection_start = range.start;
                selection_end = range.end;
                selection_handled = 1;
            }
        }
        if(TextSelectionReplacementShouldRun(field.read_only,
           selection_handled, selection_end > selection_start,
           ui_mod_key_down())) {
            int inserted = 0;
            int deleted_selection = 0;
            int codepoint = GetCharPressed();

            while(codepoint > 0) {
                if(!deleted_selection) {
                    ui_text_delete_range(field.text, field.text_size,
                                         field.cursor_position,
                                         selection_start, selection_end);
                    deleted_selection = 1;
                }
                if((filter == NULL ||
                    filter(codepoint, filter_user_data)) &&
                   ui_text_insert_codepoint(field.text, field.text_size,
                                            field.cursor_position, codepoint,
                                            field.max_codepoints))
                    changed = 1;
                inserted = 1;
                codepoint = GetCharPressed();
            }
            for(int i = 0; i < g_ui_text_input_codepoint_count; i++) {
                codepoint = g_ui_text_input_codepoints[i];
                if(!deleted_selection) {
                    ui_text_delete_range(field.text, field.text_size,
                                         field.cursor_position,
                                         selection_start, selection_end);
                    deleted_selection = 1;
                }
                if((filter == NULL ||
                    filter(codepoint, filter_user_data)) &&
                   ui_text_insert_codepoint(field.text, field.text_size,
                                            field.cursor_position, codepoint,
                                            field.max_codepoints))
                    changed = 1;
                inserted = 1;
            }
            if(g_ui_text_input_codepoint_count > 0)
                g_ui_text_input_codepoint_count = 0;
            if(inserted) {
                TextSelectionState collapsed = ui_text_selection_collapsed(
                    *field.cursor_position);
                ui_text_selection_set_collapsed(&g_ui_text_field_selection,
                                                field.focus_id, field.focused,
                                                *field.cursor_position, 0);
                selection_start = collapsed.anchor;
                selection_end = collapsed.cursor;
                selection_handled = 1;
            }
        }
        if(TextNativeEditShouldRun(field.read_only, selection_handled)) {
            changed |= EditText(field_edit);
        }
        if(TextCommitAfterHandledShouldRun(field.read_only, selection_handled,
           IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
           g_ui_text_input_enter_count > 0)) {
            if(field.commit_pressed != NULL)
                *field.commit_pressed = 1;
            g_ui_text_input_enter_count = 0;
        }
        if(TextFieldChangedShouldCollapseSelection(changed,
           IsKeyPressed(KEY_LEFT), IsKeyPressed(KEY_RIGHT),
           IsKeyPressed(KEY_HOME), IsKeyPressed(KEY_END))) {
            if(!g_ui_text_field_selection.dragging) {
                TextSelectionState collapsed = ui_text_selection_collapsed(
                    *field.cursor_position);
                ui_text_selection_set_collapsed(&g_ui_text_field_selection,
                                                field.focus_id, field.focused,
                                                *field.cursor_position, 0);
                selection_start = collapsed.anchor;
                selection_end = collapsed.cursor;
            }
        }
    } else {
        int len = (int)strlen(field.text);
        *field.cursor_position = TextCursorForLength(*field.cursor_position,
                                                     len);
    }

    if(ui_text_selection_matches(g_ui_text_field_selection,
                                 field.focus_id, field.focused))
        ui_selection_range(g_ui_text_field_selection, field.text,
                           &selection_start, &selection_end);
    else
    {
        TextSelectionState collapsed = ui_text_selection_collapsed(
            *field.cursor_position);
        selection_start = collapsed.anchor;
        selection_end = collapsed.cursor;
    }

    committed_selection_start = selection_start;
    committed_selection_end = selection_end;

    display_text = field.text;
    paint_cursor = *field.cursor_position;
    if(TextCompositionDisplayShouldRun(field.secure)) {
        const char *preedit = NULL;
        int preedit_cursor = 0;
        int preedit_selection_length = 0;

        if(ui_text_composition_get(field.focused, &preedit, &preedit_cursor,
                                   &preedit_selection_length) &&
           ui_text_composition_view(
               field.text, selection_start, selection_end, preedit,
               preedit_cursor, preedit_selection_length, &composition)) {
            display_text = composition.text;
            paint_cursor = composition.cursor;
            selection_start = composition.selection_start;
            selection_end = composition.selection_end;
            composition_start = composition.composition_start;
            composition_end = composition.composition_end;
        }
    }

    if(field.secure) {
        size_t len = strlen(field.text);

        free(masked_text);
        masked_text = NULL;
        display_text = "";
        masked_text = malloc(len + 1);
        if(masked_text != NULL) {
            memset(masked_text, '*', len);
            masked_text[len] = '\0';
            display_text = masked_text;
        }
    }

    text_w = TextWidth(display_text, font);
    scroll_policy = TextFieldScrollFor(field.bounds.x, field.bounds.width,
                                       padding_x, text_w, *scroll_x_ptr);
    clip_w = scroll_policy.clip_width;
    max_scroll_x = scroll_policy.max_scroll;
    *scroll_x_ptr = scroll_policy.scroll;
    if(TextFieldRevealCursorShouldRun(focused)) {
        int cursor_text_x = ui_text_width_before_cursor(
            display_text, font, paint_cursor);
        int margin = TextFieldRevealMargin((float)Scale(1000) / 1000.0f);

        *scroll_x_ptr = TextFieldRevealScroll(*scroll_x_ptr, max_scroll_x,
                                              clip_w, cursor_text_x, margin);
    }

    if(TextFieldContextShouldRegister(field.secure))
        ui_text_context_register_target(TEXT_CONTEXT_FIELD, field.focus_id,
                                        field.focused, field.text,
                                        field.text_size, field.cursor_position,
                                        field.max_codepoints, filter,
                                        filter_user_data,
                                        &g_ui_text_field_selection,
                                        committed_selection_start,
                                        committed_selection_end, 0, 1,
                                        field.read_only);

    TextInputPaint paint = {
        .style = (TextInputStyle){0}, .cursor = paint_cursor, .focused = focused,
        .class_name = field.class_name,
        .editable = !field.read_only,
        .caret = focused && !field.read_only && ui_caret_blink_visible(),
        .font = font, .font_token = ui_active_font_token(),
        .selection_start = selection_start, .selection_end = selection_end,
        .composition_start = composition_start,
        .composition_end = composition_end,
        .scroll_x = *scroll_x_ptr
    };
    ui_tree_submit_text_input(field.bounds, display_text, paint, field.focus_id);
    ui_text_composition_view_free(&composition);
    free(masked_text);
    EndWidget(&widget);
    return changed;
}

void
ui_paint_text_input(Rectangle bounds, const char *text, TextInputPaint paint)
{
    if(!IsWindowReady())
        return;
    int previous_font = ui_active_font_token();
    PopTextFont(paint.font_token);
    RenderTextInputEx(bounds, text, paint.cursor, paint.focused, paint.editable,
                      paint.caret, paint.font, paint.style,
                      paint.selection_start, paint.selection_end,
                      paint.composition_start, paint.composition_end,
                      paint.scroll_x, 0, paint.class_name);
    PopTextFont(previous_font);
}

static TextLayout
ParagraphLayout(ParagraphSpec paragraph)
{
    ParagraphMetrics metrics = ParagraphResolveMetrics(paragraph.font,
                                                       GetFontSize(),
                                                       paragraph.line_gap,
                                                       ParagraphDefaultLineGap(
                                                           (float)Scale(1000) /
                                                           1000.0f),
                                                       paragraph.icon_size,
                                                       paragraph.width,
                                                       paragraph.width,
                                                       0, 0);
    TextLayout layout = ParseTextLayout(paragraph.text ? paragraph.text : "",
                                                     paragraph.icon,
                                                     paragraph.icon_type,
                                                     metrics.icon_size);
    ReflowTextLayout(&layout, metrics.width, metrics.font, metrics.line_gap);
    return layout;
}

static Color
paragraph_text_color(ParagraphSpec paragraph)
{
    StyleData base = {
        .fields = StyleOpacity,
        .opacity = 1.0f
    };
    StyleFacts facts = StyleDefaultFacts(StyleKindParagraphText());
    Style style;

    facts.class_name = paragraph.class_name;
    facts.state = ButtonStateNormal;
    style = ui_unpack_style(ResolveActiveStyle(base, facts,
                                               ButtonStateNormal));
    if((style.fields & StyleForeground) == 0)
        style.foreground = ui_default_text_color();
    return Fade(style.foreground, style.opacity);
}

int
ui_paragraph_height(ParagraphSpec paragraph)
{
    if(!ParagraphCanLayout(paragraph.width))
        return 0;
    TextLayout layout = ParagraphLayout(paragraph);
    int height = GetTextLayoutHeight(&layout);
    FreeTextLayout(&layout);
    return height;
}

static void
ui_draw_paragraph_color(ParagraphSpec paragraph, int x, int *y, int align,
                        Color color)
{
    if(y == NULL || !ParagraphCanLayout(paragraph.width))
        return;
    ParagraphMetrics metrics = ParagraphResolveMetrics(paragraph.font,
                                                       GetFontSize(),
                                                       paragraph.line_gap,
                                                       ParagraphDefaultLineGap(
                                                           (float)Scale(1000) /
                                                           1000.0f),
                                                       paragraph.icon_size,
                                                       paragraph.width,
                                                       paragraph.width,
                                                       0, *y);
    int font = metrics.font;
    TextLayout layout = ParagraphLayout(paragraph);
    if(align != TextAlignStart)
        DrawTextLayoutAligned(&layout, x, y, font, color, paragraph.width,
                              align);
    else
        DrawTextLayout(&layout, x, y, font, color);
    FreeTextLayout(&layout);
}

void
ui_draw_paragraph(ParagraphSpec paragraph, int x, int *y)
{
    ui_draw_paragraph_color(paragraph, x, y, paragraph.align,
                            paragraph_text_color(paragraph));
}

void
ui_draw_paragraph_aligned(ParagraphSpec paragraph, int x, int *y, int align)
{
    ui_draw_paragraph_color(paragraph, x, y, align,
                            paragraph_text_color(paragraph));
}

void
ui_draw_paragraph_aligned_color(ParagraphSpec paragraph, int x, int *y,
                                int align, Color color)
{
    ui_draw_paragraph_color(paragraph, x, y, align, color);
}

void
RenderBevel(int x, int y, int w, int h, Color light, Color dark)
{
    BevelLines lines = BevelLinesFor(x, y, w, h);

    DrawLine((int)lines.top.x, (int)lines.top.y,
             (int)(lines.top.x + lines.top.width),
             (int)(lines.top.y + lines.top.height), light);
    DrawLine((int)lines.left.x, (int)lines.left.y,
             (int)(lines.left.x + lines.left.width),
             (int)(lines.left.y + lines.left.height), light);
    DrawLine((int)lines.bottom.x, (int)lines.bottom.y,
             (int)(lines.bottom.x + lines.bottom.width),
             (int)(lines.bottom.y + lines.bottom.height), dark);
    DrawLine((int)lines.right.x, (int)lines.right.y,
             (int)(lines.right.x + lines.right.width),
             (int)(lines.right.y + lines.right.height), dark);
}

void
RenderTextLines(const char **lines, int count, int x, int *y, int font, int line_h, Color color)
{
    for(int i = 0; i < count; i++) {
        RenderText(lines[i], x, *y, font, color);
        *y += line_h;
    }
}

int
GetIconButtonSize(int size)
{
    return IconButtonSizeFor(size, (float)Scale(1000) / 1000.0f);
}

int
GetIconButtonPadding(int size)
{
    return IconButtonPaddingFor(size, (float)Scale(1000) / 1000.0f);
}

void
SetDefaultFontAutoLoad(int enabled)
{
    ui_default_font_auto_load = enabled != 0;
}

void
InitInterface(int width, int height, float dpi)
{
    ui_view_width = width;
    ui_view_height = height;
    SetScale(dpi);
    EnsureBuiltInStylePacks();
    ApplyCurrentTheme();
    if(ui_default_font_auto_load)
        EnsureDefaultFont();
}

int
IsDesktopMode(void)
{
    return IsDesktopWidth(ui_view_width, (float)Scale(1000) / 1000.0f);
}

static void
ui_set_theme_colors(Color text, Color bg, Color surface, Color circle,
                    Color button, Color button_hover, Color icon)
{
    c_text = text;
    c_bg = bg;
    c_surface = surface;
    c_circle = circle;
    c_button = button;
    c_button_hover = button_hover;
    c_icon = icon;
    c_link = GetThemeLink();
}

void
SetLinkColor(Color link)
{
    c_link = link;
}

void
ApplyCurrentTheme(void)
{
    ui_set_theme_colors(GetThemeText(),
                        GetThemeBackground(),
                        GetThemeSurface(),
                        GetThemeCircle(),
                        GetThemeButton(),
                        GetThemeButtonHover(),
                        GetThemeIcon());
    SetLinkColor(GetThemeLink());
}

Camera2D
GetDefaultCamera(void)
{
    Camera2D camera;

    memset(&camera, 0, sizeof(camera));
    camera.zoom = 1.0f;
    return camera;
}

static int
ui_float_isfinite(float value)
{
    return value == value && value > -3.4e38f && value < 3.4e38f;
}

static float
ui_sane_float(float value, float fallback)
{
    return ui_float_isfinite(value) ? value : fallback;
}

static Camera2D
ui_sane_camera(Camera2D camera)
{
    camera.offset.x = ui_sane_float(camera.offset.x, 0.0f);
    camera.offset.y = ui_sane_float(camera.offset.y, 0.0f);
    camera.target.x = ui_sane_float(camera.target.x, 0.0f);
    camera.target.y = ui_sane_float(camera.target.y, 0.0f);
    camera.rotation = ui_sane_float(camera.rotation, 0.0f);
    if(!ui_float_isfinite(camera.zoom) || fabsf(camera.zoom) < 0.0001f)
        camera.zoom = 1.0f;
    return camera;
}

void
ui_camera_ensure_sane(void)
{
    g_ui_camera = ui_sane_camera(g_ui_camera);
}

void
BeginInterfaceFrame(int width, int height, float dpi)
{
    ui_reset_disabled_scope();
    SetViewSize(width, height);
    InitInterface(width, height, dpi);
    SetFrameCamera(GetDefaultCamera());
}

void
SetFrameCamera(Camera2D camera)
{
    PumpWindows();
    EndFocusScope();
    BeginFocusScope();
    g_ui_frame_serial++;
    g_ui_auto_focus_id = 0x40000000;
    ui_text_begin_frame();

    g_ui_text_input_requested = 0;
    g_ui_focus_text_input_active = 0;
    if(!g_ui_cursor_had_intent && g_ui_cursor_current != MOUSE_CURSOR_DEFAULT) {
        g_ui_cursor_current = MOUSE_CURSOR_DEFAULT;
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }
    g_ui_cursor_priority = CURSOR_PRIORITY_DEFAULT;
    g_ui_cursor_had_intent = 0;

    g_ui_camera = ui_sane_camera(camera);
    ui_update_pointer_gesture();
    ClearInputCaptures();
    if(g_ui_modal_capture_next_frame) {
        PushInputCapture(g_ui_modal_capture_next_frame_bounds, 1);
        g_ui_modal_capture_next_frame = 0;
    }
    if(g_ui_text_context_open)
        PushInputCapture((Rectangle){0.0f, 0.0f,
                                       (float)ui_view_width,
                                       (float)ui_view_height}, 0);
    g_ui_input_clip_stack_count = 0;
    g_scroll_scope_depth = 0;
    ResetClip();
    BeginInspectFrame(NULL);
    ui_frame_layers_begin();
}

int
ResolveFocusID(int id)
{
    if(id != 0)
        return id;
    return g_ui_auto_focus_id++;
}

void
RenderFrameOverlays(void)
{
    if(g_ui_overlays_drawn_frame == g_ui_frame_serial)
        return;
    g_ui_overlays_drawn_frame = g_ui_frame_serial;
    ResetClip();
    ui_dropdown_overlays();
    ui_draw_menu_overlays();
    ui_tab_bar_finish_frame();
    ui_text_draw_context_overlay();
}

void
EndInterfaceFrame(void)
{
    RenderFrameOverlays();
    EndFocusScope();
    /* Unhandled text from a popup-captured frame must not be replayed into an
     * underlying editor after dismissal. Retain ordinary non-popup queue
     * behavior; the capture marker also covers a popup closed this frame. */
    if(ui_popup_input_keyboard_captures() || ui_popup_input_keyboard_was_captured()) {
        while(GetCharPressed() != 0) {}
        g_ui_text_input_codepoint_count = 0;
        g_ui_text_input_backspace_count = 0;
        g_ui_text_input_enter_count = 0;
        ClearTextComposition();
    }
    ui_frame_layers_end();
    ui_sync_platform_text_input();
    EndInspectFrame();
}

FrameState
SaveFrameState(void)
{
    FrameState state;

    state.view_width = ui_view_width;
    state.view_height = ui_view_height;
    state.camera = g_ui_camera;
    state.input_clip_count = g_ui_input_clip_stack_count;
    memcpy(state.input_clips, g_ui_input_clip_stack, sizeof(state.input_clips));
    state.input_capture_count = g_ui_input_capture_stack_count;
    for(int i = 0; i < INPUT_CAPTURE_STACK_MAX; i++) {
        state.input_captures[i].bounds = g_ui_input_capture_stack[i].bounds;
        state.input_captures[i].allow_inside =
            g_ui_input_capture_stack[i].allow_inside;
    }
    state.cursor_priority = g_ui_cursor_priority;
    state.cursor_had_intent = g_ui_cursor_had_intent;
    state.pointer_down = g_ui_pointer_down;
    state.pointer_dragging = g_ui_pointer_dragging;
    state.pointer_dragged_this_click = g_ui_pointer_dragged_this_click;
    state.pointer_start_x = g_ui_pointer_start_x;
    state.pointer_start_y = g_ui_pointer_start_y;
    state.pointer_owner = g_ui_pointer_owner;
    state.release_consumed = g_ui_release_consumed;
    state.focus_active_id = g_ui_focus_active_id;
    memcpy(state.focus_ids, g_ui_focus_ids, sizeof(state.focus_ids));
    state.focus_count = g_ui_focus_count;
    state.focus_tab_dir = g_ui_focus_tab_dir;
    state.focus_frame_open = g_ui_focus_frame_open;
    state.focus_text_input_active = g_ui_focus_text_input_active;
    state.text_input_requested = g_ui_text_input_requested;
    state.text_input_show_requested = g_ui_text_input_show_requested;
    state.mouse_world_override_enabled = g_ui_mouse_world_override_enabled;
    state.mouse_world_override = g_ui_mouse_world_override;
    state.frame_serial = g_ui_frame_serial;
    state.auto_focus_id = g_ui_auto_focus_id;
    state.ui_scale = GetScale();
    return state;
}

void
RestoreFrameState(FrameState state)
{
    ui_view_width = state.view_width;
    ui_view_height = state.view_height;
    g_ui_camera = state.camera;
    g_ui_input_clip_stack_count = state.input_clip_count;
    memcpy(g_ui_input_clip_stack, state.input_clips, sizeof(g_ui_input_clip_stack));
    g_ui_input_capture_stack_count = state.input_capture_count;
    for(int i = 0; i < INPUT_CAPTURE_STACK_MAX; i++) {
        g_ui_input_capture_stack[i].bounds = state.input_captures[i].bounds;
        g_ui_input_capture_stack[i].allow_inside =
            state.input_captures[i].allow_inside;
    }
    g_ui_cursor_priority = state.cursor_priority;
    g_ui_cursor_had_intent = state.cursor_had_intent;
    g_ui_pointer_down = state.pointer_down;
    g_ui_pointer_dragging = state.pointer_dragging;
    g_ui_pointer_dragged_this_click = state.pointer_dragged_this_click;
    g_ui_pointer_start_x = state.pointer_start_x;
    g_ui_pointer_start_y = state.pointer_start_y;
    g_ui_pointer_owner = state.pointer_owner;
    g_ui_release_consumed = state.release_consumed;
    g_ui_focus_active_id = state.focus_active_id;
    memcpy(g_ui_focus_ids, state.focus_ids, sizeof(g_ui_focus_ids));
    g_ui_focus_count = state.focus_count;
    g_ui_focus_tab_dir = state.focus_tab_dir;
    g_ui_focus_frame_open = state.focus_frame_open;
    g_ui_focus_text_input_active = state.focus_text_input_active;
    g_ui_text_input_requested = state.text_input_requested;
    g_ui_text_input_show_requested = state.text_input_show_requested;
    g_ui_mouse_world_override_enabled = state.mouse_world_override_enabled;
    g_ui_mouse_world_override = state.mouse_world_override;
    if(g_ui_pointer_start_x != INT_MIN || g_ui_pointer_start_y != INT_MIN)
        g_ui_pointer_start_world = screen_to_world_for_input(
            (Vector2){(float)g_ui_pointer_start_x,
                      (float)g_ui_pointer_start_y});
    g_ui_frame_serial = state.frame_serial;
    g_ui_auto_focus_id = state.auto_focus_id;
    SetScale(state.ui_scale);
    ResetClip();
}

void
SetModalCapture(Rectangle bounds)
{
    ClearInputCaptures();
    PushInputCapture(bounds, 1);
    g_ui_modal_capture_next_frame_bounds = bounds;
    g_ui_modal_capture_next_frame = 1;
}

void
SetTextInputPlatformCallback(TextInputPlatformCallback callback)
{
    g_ui_text_input_platform_callback = callback;
}

void
SetCursorClickable(int *cursor_clickable)
{
    g_ui_cursor_clickable = cursor_clickable;
}

void
SetCursorDisabled(int *cursor_disabled)
{
    g_ui_cursor_disabled = cursor_disabled;
}

void
SetInterfaceIcons(Texture2D gear_icon, Texture2D x_icon)
{
    g_ui_gear_icon = gear_icon;
    g_ui_x_icon = x_icon;
}

void
DrawCustomIcon(int x, int y, int size, Texture2D icon, Color tint)
{
    Rectangle src;
    Rectangle dst;

    if(icon.id == 0 || size <= 0)
        return;

    src = (Rectangle){0, 0, (float)icon.width, (float)icon.height};
    dst = (Rectangle){(float)x, (float)y, (float)size, (float)size};
    DrawTexturePro(icon, src, dst, kryon_zero_vector2, 0, tint);
}

/* ================================================================
 * CONTROLS
 * ================================================================ */

Rectangle
ui_centered_min_hit_rect(int x, int y, int w, int h, int min_w, int min_h)
{
    int hit_w = w < min_w ? min_w : w;
    int hit_h = h < min_h ? min_h : h;

    return (Rectangle){
        (float)(x + w / 2 - hit_w / 2),
        (float)(y + h / 2 - hit_h / 2),
        (float)hit_w,
        (float)hit_h
    };
}
static void
ui_sync_platform_text_input(void)
{
    int text_input_active = TextPlatformInputActiveFor(
        g_ui_text_input_requested != 0,
        g_ui_text_focus_owner != NULL,
        g_ui_text_focus_owner_this_frame == g_ui_text_focus_owner);
    TextPlatformInputSyncDecision decision =
        TextPlatformInputSyncDecisionFor(
            g_ui_text_input_show_requested != 0,
            g_ui_text_input_platform_callback != NULL,
            g_ui_platform_text_input_active != 0,
            text_input_active != 0);

    g_ui_platform_text_input_active = decision.active ? 1 : 0;
    if(decision.call_callback && g_ui_text_input_platform_callback != NULL)
        g_ui_text_input_platform_callback(decision.callback_active ? 1 : 0);
    if(decision.clear_show_request)
        g_ui_text_input_show_requested = 0;
}
