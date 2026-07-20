#include "ui_internal.h"
#include "platform.h"
#include "theme.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
Texture2D g_ui_gear_icon = {0};
Texture2D g_ui_x_icon = {0};
int g_ui_slider_active_id = 0;
static int g_ui_pointer_down = 0;
int g_ui_pointer_dragging = 0;
static int g_ui_pointer_dragged_this_click = 0;
static int g_ui_pointer_start_x = 0;
static int g_ui_pointer_start_y = 0;
static int g_ui_transition_cues_enabled = 0;
static int g_ui_release_consumed = 0;

int g_ui_pointer_owner = UI_POINTER_OWNER_NONE;

#define UI_FOCUS_MAX_ITEMS 256
static int g_ui_focus_active_id = 0;
static int g_ui_focus_ids[UI_FOCUS_MAX_ITEMS];
static int g_ui_focus_count = 0;
static int g_ui_focus_tab_dir = 0;
static int g_ui_focus_frame_open = 0;
static int g_ui_focus_text_input_active = 0;
static int g_ui_platform_text_input_active = 0;
static int g_ui_text_input_requested = 0;
static UITextInputPlatformCallback g_ui_text_input_platform_callback = NULL;
static int g_ui_text_area_drag_id = 0;

#define UI_TEXT_INPUT_QUEUE_MAX 64
static int g_ui_text_input_codepoints[UI_TEXT_INPUT_QUEUE_MAX];
static int g_ui_text_input_codepoint_count = 0;
static int g_ui_text_input_backspace_count = 0;
static int g_ui_text_input_enter_count = 0;
static double g_ui_backspace_next_repeat_at = 0.0;

enum {
    UI_CURSOR_PRIORITY_DEFAULT = 0,
    UI_CURSOR_PRIORITY_CLICKABLE = 1,
    UI_CURSOR_PRIORITY_TEXT = 1,
    UI_CURSOR_PRIORITY_DISABLED = 2
};

#define UI_INPUT_CLIP_STACK_MAX 16
static Rectangle g_ui_input_clip_stack[UI_INPUT_CLIP_STACK_MAX];
static int g_ui_input_clip_stack_count = 0;

#define UI_INPUT_CAPTURE_STACK_MAX 16
typedef struct UIInputCapture {
    Rectangle bounds;
    int allow_inside;
} UIInputCapture;

static UIInputCapture g_ui_input_capture_stack[UI_INPUT_CAPTURE_STACK_MAX];
static int g_ui_input_capture_stack_count = 0;

Vector2
ui_mouse_world(void)
{
    return GetScreenToWorld2D(GetMousePosition(), g_ui_camera);
}

static void
ui_set_cursor_intent(int cursor, int priority)
{
    if(priority < g_ui_cursor_priority)
        return;
    g_ui_cursor_priority = priority;
    SetMouseCursor(cursor);
}

void
MarkUIClickable(void)
{
    if(g_ui_cursor_clickable != NULL)
        *g_ui_cursor_clickable = 1;
    ui_set_cursor_intent(MOUSE_CURSOR_POINTING_HAND, UI_CURSOR_PRIORITY_CLICKABLE);
}

void
MarkUIDisabled(void)
{
    if(g_ui_cursor_disabled != NULL)
        *g_ui_cursor_disabled = 1;
    ui_set_cursor_intent(MOUSE_CURSOR_NOT_ALLOWED, UI_CURSOR_PRIORITY_DISABLED);
}

static void
MarkUITextCursor(void)
{
    ui_set_cursor_intent(MOUSE_CURSOR_IBEAM, UI_CURSOR_PRIORITY_TEXT);
}

static int
ui_iabs(int value)
{
    return value < 0 ? -value : value;
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
    return ui_iabs(ui_pointer_dx()) >= ui_iabs(ui_pointer_dy());
}

static void
ui_update_pointer_gesture(void)
{
    Vector2 mouse = GetMousePosition();
    int mx = (int)mouse.x;
    int my = (int)mouse.y;
    int drag_threshold = ScaleUIPx(5);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_ui_pointer_down = 1;
        g_ui_pointer_dragging = 0;
        g_ui_pointer_dragged_this_click = 0;
        g_ui_release_consumed = 0;
        g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
        g_ui_pointer_start_x = mx;
        g_ui_pointer_start_y = my;
    } else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && g_ui_pointer_down) {
        int dx = ui_pointer_dx();
        int dy = ui_pointer_dy();
        if(dx > drag_threshold || dx < -drag_threshold ||
           dy > drag_threshold || dy < -drag_threshold) {
            g_ui_pointer_dragging = 1;
            g_ui_pointer_dragged_this_click = 1;
        }
    } else if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_pointer_down = 0;
        g_ui_pointer_dragging = 0;
        g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
    } else if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        g_ui_pointer_down = 0;
        g_ui_pointer_dragging = 0;
        g_ui_pointer_dragged_this_click = 0;
        g_ui_release_consumed = 0;
        g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
    }
}

void
ClearUIInputCaptures(void)
{
    g_ui_input_capture_stack_count = 0;
}

void
PushUIInputCapture(Rectangle bounds, int allow_inside)
{
    if(g_ui_input_capture_stack_count >= UI_INPUT_CAPTURE_STACK_MAX)
        return;
    g_ui_input_capture_stack[g_ui_input_capture_stack_count++] =
        (UIInputCapture){bounds, allow_inside != 0};
}

void
BeginUIModalLayer(void)
{
    ClearUIInputCaptures();
    PushUIInputCapture((Rectangle){0.0f, 0.0f,
                                   (float)ui_view_width,
                                   (float)ui_view_height}, 0);
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
        UIInputCapture capture =
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
           UIDropdownCapturesClick(point);
}

int
UIInputCapturesClick(Vector2 point)
{
    return UIEditorInputCapturesClick(point) ||
           ui_input_captures_click_internal(point, 1);
}

int
UIReleaseConsumed(void)
{
    return g_ui_release_consumed;
}

void
UIConsumeRelease(void)
{
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_release_consumed = 1;
}

int
UIPointerReleaseConsumed(void)
{
    return UIReleaseConsumed();
}

void
UIConsumePointerRelease(void)
{
    UIConsumeRelease();
}

int
UIPointerReleaseAvailable(Vector2 point)
{
    return IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
           !UIInputCapturesClick(point);
}

int
UIPointerReleaseOutside(Rectangle bounds)
{
    Vector2 mouse = ui_mouse_world();

    return UIPointerReleaseAvailable(mouse) &&
           !CheckCollisionPointRec(mouse, bounds);
}

int
UIHoverEffectsEnabled(void)
{
#if ANDROID_BUILD
    return 0;
#else
    return 1;
#endif
}

void
SetUITransitionCuesEnabled(int enabled)
{
    g_ui_transition_cues_enabled = enabled ? 1 : 0;
}

int
UITransitionCuesEnabled(void)
{
    return g_ui_transition_cues_enabled;
}

void
PushUIInputClip(Rectangle bounds)
{
    if(g_ui_input_clip_stack_count > 0)
        bounds = GetUIClipIntersection(g_ui_input_clip_stack[g_ui_input_clip_stack_count - 1],
                                         bounds);
    if(g_ui_input_clip_stack_count < UI_INPUT_CLIP_STACK_MAX)
        g_ui_input_clip_stack[g_ui_input_clip_stack_count++] = bounds;
}

void
PopUIInputClip(void)
{
    if(g_ui_input_clip_stack_count > 0)
        g_ui_input_clip_stack_count--;
}

int
ui_clampi(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

static int
ui_utf8_next_offset(const char *text, int offset)
{
    int codepoint_size = 0;
    int len;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    if(offset < 0)
        offset = 0;
    if(offset >= len)
        return len;

    GetCodepointNext(text + offset, &codepoint_size);
    if(codepoint_size <= 0)
        codepoint_size = 1;
    if(offset + codepoint_size > len)
        return len;
    return offset + codepoint_size;
}

static int
ui_utf8_prev_offset(const char *text, int offset)
{
    int len;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    if(offset > len)
        offset = len;
    if(offset <= 0)
        return 0;

    offset--;
    while(offset > 0 && (((unsigned char)text[offset] & 0xC0) == 0x80))
        offset--;
    return offset;
}

static int
ui_utf8_codepoint_count(const char *text)
{
    int count = 0;

    if(text == NULL)
        return 0;
    for(int i = 0; text[i] != '\0';) {
        int next = ui_utf8_next_offset(text, i);
        if(next <= i)
            break;
        count++;
        i = next;
    }
    return count;
}

static int
ui_utf8_encode(int codepoint, char out[5])
{
    if(out == NULL)
        return 0;
    if(codepoint < 0x80) {
        out[0] = (char)codepoint;
        out[1] = '\0';
        return 1;
    }
    if(codepoint < 0x800) {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        out[2] = '\0';
        return 2;
    }
    if(codepoint < 0x10000) {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        out[3] = '\0';
        return 3;
    }
    if(codepoint <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (codepoint >> 18));
        out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = (char)(0x80 | (codepoint & 0x3F));
        out[4] = '\0';
        return 4;
    }
    return 0;
}

static int
ui_text_delete_range(char *text, size_t text_size, int *cursor, int start, int end)
{
    int len;

    if(text == NULL || text_size == 0 || cursor == NULL)
        return 0;
    len = (int)strlen(text);
    start = ui_clampi(start, 0, len);
    end = ui_clampi(end, 0, len);
    if(end <= start)
        return 0;
    memmove(text + start, text + end, (size_t)(len - end + 1));
    *cursor = start;
    return 1;
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

static void
ui_begin_world_clip(Rectangle rect)
{
    Rectangle screen = ui_world_rect_to_screen(rect);
    BeginUIClip((int)screen.x, (int)screen.y,
                     (int)screen.width, (int)screen.height);
}

static void
ui_draw_text_centered_in_rect(const char *text, Rectangle rect, int font_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int text_w = MeasureUIText(value, font_size);
    int x = (int)(rect.x + (rect.width - (float)text_w) * 0.5f);
    int y = GetUIControlTextY(value, (int)rect.y, (int)rect.height, font_size);
    int guard = 1;

    ui_begin_world_clip((Rectangle){rect.x, rect.y - guard,
                                    rect.width, rect.height + guard * 2});
    DrawUIText(value, x, y, font_size, color);
    EndUIClip();
}

static int
ui_control_height_for_font(int font)
{
    int line_h = GetUITextLineHeight(font);
    int pad_y = ScaleUIPx(5);

    if(line_h < font)
        line_h = font;
    return line_h + pad_y * 2;
}

static const char *
ui_editor_control_id(char *buf, size_t buf_size, const char *kind,
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
ui_control_cursor_height(int font, int box_h)
{
    int h = GetUITextLineHeight(font);
    int max_h = box_h - ScaleUIPx(8);

    if(h < font)
        h = font;
    if(max_h < ScaleUIPx(8))
        max_h = box_h;
    if(h > max_h)
        h = max_h;
    if(h < ScaleUIPx(8))
        h = ScaleUIPx(8);
    return h;
}

static int
ui_text_next_smaller_size(int font_size)
{
    if(font_size > UI_TEXT_16)
        return UI_TEXT_16;
    if(font_size > UI_TEXT_12)
        return UI_TEXT_12;
    return UI_TEXT_8;
}

static int
ui_text_normalize_size(int font_size)
{
    if(font_size <= UI_TEXT_8)
        return UI_TEXT_8;
    if(font_size <= UI_TEXT_12)
        return UI_TEXT_12;
    if(font_size <= UI_TEXT_16)
        return UI_TEXT_16;
    return UI_TEXT_24;
}

void
DrawLeftUIControlTextInRect(const char *text, Rectangle rect, int font_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int y = GetUIControlTextY(value, (int)rect.y, (int)rect.height, font_size);
    int guard = 1;

    ui_begin_world_clip((Rectangle){rect.x, rect.y - guard,
                                    rect.width, rect.height + guard * 2});
    DrawUIText(value, (int)rect.x, y, font_size, color);
    EndUIClip();
}

void
DrawFittedUITextInRect(const char *text, Rectangle rect,
                            int preferred_size, int min_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int font_size = ui_text_normalize_size(preferred_size);
    int min_allowed = ui_text_normalize_size(min_size);

    if(font_size < min_allowed)
        font_size = min_allowed;
    while(font_size > min_allowed && MeasureUIText(value, font_size) > (int)rect.width)
        font_size = ui_text_next_smaller_size(font_size);
    ui_draw_text_centered_in_rect(value, rect, font_size, color);
}

static int
ui_text_insert_codepoint(char *text, size_t text_size, int *cursor, int codepoint,
                         int max_codepoints)
{
    char encoded[5];
    int encoded_len;
    int len;

    if(text == NULL || text_size == 0 || cursor == NULL || codepoint < 32)
        return 0;
    if(max_codepoints > 0 && ui_utf8_codepoint_count(text) >= max_codepoints)
        return 0;

    encoded_len = ui_utf8_encode(codepoint, encoded);
    if(encoded_len <= 0)
        return 0;
    len = (int)strlen(text);
    *cursor = ui_clampi(*cursor, 0, len);
    if((size_t)(len + encoded_len + 1) > text_size)
        return 0;

    memmove(text + *cursor + encoded_len, text + *cursor, (size_t)(len - *cursor + 1));
    memcpy(text + *cursor, encoded, (size_t)encoded_len);
    *cursor += encoded_len;
    return 1;
}

void
QueueUITextInputCodepoint(int codepoint)
{
    if(codepoint <= 0)
        return;
    if(g_ui_text_input_codepoint_count >= UI_TEXT_INPUT_QUEUE_MAX)
        return;
    g_ui_text_input_codepoints[g_ui_text_input_codepoint_count++] = codepoint;
}

void
QueueUITextInputBackspace(void)
{
    if(g_ui_text_input_backspace_count < UI_TEXT_INPUT_QUEUE_MAX)
        g_ui_text_input_backspace_count++;
}

void
QueueUITextInputEnter(void)
{
    if(g_ui_text_input_enter_count < UI_TEXT_INPUT_QUEUE_MAX)
        g_ui_text_input_enter_count++;
}

void
BeginUIFocus(void)
{
    g_ui_focus_count = 0;
    g_ui_focus_tab_dir = 0;
    g_ui_focus_frame_open = 1;
    if(IsKeyPressed(KEY_TAB))
        g_ui_focus_tab_dir = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? -1 : 1;
}

void
EndUIFocus(void)
{
    int current_index = -1;
    int next_index;

    if(!g_ui_focus_frame_open)
        return;

    g_ui_focus_frame_open = 0;

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
RegisterUIFocus(int id, Rectangle bounds)
{
    Vector2 mouse_world;

    if(id <= 0)
        return 0;

    if(g_ui_focus_count < UI_FOCUS_MAX_ITEMS)
        g_ui_focus_ids[g_ui_focus_count++] = id;

    mouse_world = ui_mouse_world();
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
       CheckCollisionPointRec(mouse_world, bounds) &&
       !UIInputCapturesClick(mouse_world))
        g_ui_focus_active_id = id;

    return g_ui_focus_active_id == id;
}

int
IsUIFocusActive(int id)
{
    return id > 0 && g_ui_focus_active_id == id;
}

int
IsUIFocusActivatePressed(int id)
{
    return IsUIFocusActive(id) &&
           (IsKeyPressed(KEY_ENTER) || (!g_ui_focus_text_input_active && IsKeyPressed(KEY_SPACE)));
}

void
SetUIFocus(int id)
{
    g_ui_focus_active_id = id;
}

void
ClearUIFocus(void)
{
    g_ui_focus_active_id = 0;
}

void
SetUIFocusTextInputActive(int active)
{
    active = active != 0;
    if(active) {
        g_ui_focus_text_input_active = 1;
        g_ui_text_input_requested = 1;
    } else {
        g_ui_focus_text_input_active = 0;
    }
}

void
DrawUIFocus(Rectangle bounds)
{
    DrawRectangleLinesEx((Rectangle){bounds.x - ScaleUIPx(3), bounds.y - ScaleUIPx(3),
                                     bounds.width + ScaleUIPx(6), bounds.height + ScaleUIPx(6)},
                         ScaleUIPx(2), c_button_hover);
}

int
GetUIFontSize(void)
{
    return UI_TEXT_16;
}

int
GetUISmallFontSize(void)
{
    return UI_TEXT_12;
}

int
GetUITitleFontSize(const char *title, int max_width)
{
    const char *value = title != NULL ? title : "";

    if(max_width <= 0 || MeasureUIText(value, UI_TEXT_24) <= max_width)
        return UI_TEXT_24;
    if(MeasureUIText(value, UI_TEXT_16) <= max_width)
        return UI_TEXT_16;
    return UI_TEXT_12;
}

int
GetUIControlTextY(const char *text, int box_y, int box_h, int font)
{
    (void)text;
    return GetUITextY("Hg", box_y, box_h, font);
}

void
DrawCenteredUIControlText(const char *text, int center_x, int center_y, int font, Color color)
{
    int text_w = MeasureUIText(text, font);
    int h = ui_control_height_for_font(font);
    int y = GetUIControlTextY(text, center_y - h / 2, h, font);

    DrawUIText(text, center_x - text_w / 2, y, font, color);
}

void
DrawUITextInput(Rectangle bounds, const char *text, int cursor_position,
                         int focused, int cursor_visible, int font,
                         UITextInputStyle style)
{
    const char *value = text ? text : "";
    int x = (int)bounds.x;
    int y = (int)bounds.y;
    int w = (int)bounds.width;
    int h = (int)bounds.height;
    int padding_x = style.padding_x > 0 ? style.padding_x : ScaleUIPx(10);
    int clip_guard = 1;
    int text_x = x + padding_x;
    int text_y = GetUIControlTextY(value, y, h, font);
    int cursor_h = ui_control_cursor_height(font, h);
    int cursor_y = y + (h - cursor_h) / 2;
    Color border = focused ? style.focus_border : style.border;
    float radius = style.radius >= 0.0f ? style.radius : 0.12f;

    if(focused)
        SetUIFocusTextInputActive(1);

    if(radius <= 0.0f) {
        DrawRectangleRec(bounds, style.background);
        DrawRectangleLinesEx(bounds, 1, border);
    } else {
        DrawRectangleRounded(bounds, radius, 8, style.background);
        DrawRectangleRoundedLines(bounds, radius, 8, border);
    }

    ui_begin_world_clip((Rectangle){(float)(x + padding_x), (float)(y - clip_guard),
                                    (float)(w - padding_x * 2),
                                    (float)(h + clip_guard * 2)});
    DrawUIText(value, text_x, text_y, font, style.text);

    if(focused && cursor_visible) {
        char before_cursor[1024];
        int len = (int)strlen(value);
        int clamped_cursor = ui_clampi(cursor_position, 0, len);
        int copy_len = clamped_cursor;
        if(copy_len >= (int)sizeof(before_cursor))
            copy_len = (int)sizeof(before_cursor) - 1;
        memcpy(before_cursor, value, (size_t)copy_len);
        before_cursor[copy_len] = '\0';

        int cursor_x = text_x + MeasureUIText(before_cursor, font);
        DrawRectangle(cursor_x, cursor_y, ScaleUIPx(2), cursor_h, style.cursor);
    }
    EndUIClip();
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
        if(text_x + MeasureUIText(prefix, font) >= mouse_x)
            return i;
    }
    return len;
}

int
EditUIText(UITextEdit edit)
{
    int changed = 0;
    int len;
    int codepoint;

    if(edit.commit_pressed != NULL)
        *edit.commit_pressed = 0;
    if(edit.text == NULL || edit.text_size == 0 || edit.cursor_position == NULL)
        return 0;

    len = (int)strlen(edit.text);
    *edit.cursor_position = ui_clampi(*edit.cursor_position, 0, len);

    if(IsKeyPressed(KEY_LEFT)) {
        *edit.cursor_position = ui_utf8_prev_offset(edit.text, *edit.cursor_position);
        changed = 1;
    }
    if(IsKeyPressed(KEY_RIGHT)) {
        *edit.cursor_position = ui_utf8_next_offset(edit.text, *edit.cursor_position);
        changed = 1;
    }
    if(IsKeyPressed(KEY_HOME)) {
        *edit.cursor_position = 0;
        changed = 1;
    }
    if(IsKeyPressed(KEY_END)) {
        *edit.cursor_position = (int)strlen(edit.text);
        changed = 1;
    }

    if(ui_mod_key_down() && IsKeyPressed(KEY_C)) {
        SetClipboardText(edit.text);
    }
    if(ui_mod_key_down() && IsKeyPressed(KEY_X)) {
        SetClipboardText(edit.text);
        edit.text[0] = '\0';
        *edit.cursor_position = 0;
        changed = 1;
    }
    if(ui_mod_key_down() && IsKeyPressed(KEY_V)) {
        const char *clip = GetClipboardText();
        if(clip != NULL) {
            for(int i = 0; clip[i] != '\0';) {
                int bytes = 0;
                int cp = GetCodepointNext(&clip[i], &bytes);
                if(bytes <= 0)
                    break;
                if((edit.filter == NULL || edit.filter(cp, edit.filter_user_data)) &&
                   ui_text_insert_codepoint(edit.text, edit.text_size,
                                            edit.cursor_position, cp,
                                            edit.max_codepoints))
                    changed = 1;
                i += bytes;
            }
        }
    }

    {
        int repeat = g_ui_text_input_backspace_count + ui_backspace_repeat_count();
        for(int i = 0; i < repeat; i++) {
            int start = ui_utf8_prev_offset(edit.text, *edit.cursor_position);
            changed |= ui_text_delete_range(edit.text, edit.text_size,
                                            edit.cursor_position, start,
                                            *edit.cursor_position);
        }
        g_ui_text_input_backspace_count = 0;
    }

    if(IsKeyPressed(KEY_DELETE)) {
        int end = ui_utf8_next_offset(edit.text, *edit.cursor_position);
        changed |= ui_text_delete_range(edit.text, edit.text_size,
                                        edit.cursor_position,
                                        *edit.cursor_position, end);
    }

    if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
       g_ui_text_input_enter_count > 0) {
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
    *edit.cursor_position = ui_clampi(*edit.cursor_position, 0, len);
    return changed;
}

int
DrawUIButton(UIButton button)
{
    char editor_id[96];
    Vector2 mouse_world = ui_mouse_world();
    int mouse_inside;
    int captured;
    int active;
    int hovered;
    int focused;
    int clicked = 0;
    int font = button.font > 0 ? button.font : GetUIFontSize();
    Color background = button.background.a != 0 ? button.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color text = button.text.a != 0 ? button.text : c_text;
    Color border = button.border.a != 0 ? button.border : LightenUIColor(background, 32);
    float radius = button.radius > 0.0f ? button.radius : 0.06f;
    int cues = UITransitionCuesEnabled();
    Color draw_background;
    Color draw_border;

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "button", button.focus_id, button.label),
                        &button.bounds);
    UIEditorRegisterWidget(editor_id, "button", &button.bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);
    UIEditorSetWidgetAction(editor_id, button.label);

    mouse_inside = CheckCollisionPointRec(mouse_world, button.bounds);
    captured = UIInputCapturesClick(mouse_world);
    active = !button.disabled && !captured && mouse_inside;
    hovered = active && UIHoverEffectsEnabled();
    focused = !button.disabled && button.focus_id > 0 &&
              RegisterUIFocus(button.focus_id, button.bounds);

    if(button.disabled) {
        background.a = background.a > 120 ? 120 : background.a;
        text.a = text.a > 150 ? 150 : text.a;
    }

    draw_background = hovered ? hover_background : background;
    draw_border = hovered ? LightenUIColor(hover_background, cues ? 54 : 40) : border;
    if(cues && hovered)
        draw_background = LightenUIColor(draw_background, 6);

    DrawRectangleRounded(button.bounds, radius, 8, draw_background);
    DrawRectangleRoundedLines(button.bounds, radius, 8, draw_border);
    if(cues && hovered && button.bounds.width > 4 && button.bounds.height > 4) {
        Color cue = LightenUIColor(draw_background, 42);
        cue.a = cue.a > 170 ? 170 : cue.a;
        DrawRectangle((int)button.bounds.x + 2, (int)button.bounds.y + 1,
                      (int)button.bounds.width - 4, ScaleUIPx(1), cue);
    }

    if(button.disabled && !captured && mouse_inside)
        MarkUIDisabled();

    if(active) {
        MarkUIClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            clicked = 1;
    }

    if(focused) {
        SetUIFocusTextInputActive(0);
        DrawUIFocus(button.bounds);
    }

    DrawFittedUITextInRect(button.label ? button.label : "", button.bounds,
                                font, UI_TEXT_8, text);
    if(clicked)
        UIConsumeRelease();
    return clicked || IsUIFocusActivatePressed(button.focus_id);
}

int
DrawUIIconButton(UIIconButton button)
{
    char editor_id[96];
    Vector2 mouse_world = ui_mouse_world();
    int mouse_inside;
    int captured;
    int active;
    int hovered;
    int focused;
    int clicked = 0;
    int icon_padding = button.icon_padding > 0 ? button.icon_padding : ScaleUIPx(3);
    int draw_size = button.icon_size;
    Color background = button.background.a != 0 ? button.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color icon_color = button.icon_color.a != 0 ? button.icon_color : c_text;
    Color border = button.border.a != 0 ? button.border : DarkenUIColor(background, 35);
    float radius = button.radius > 0.0f ? button.radius : 0.06f;
    int cues = UITransitionCuesEnabled();
    Color draw_background;
    Color draw_border;

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "icon_button", button.focus_id, NULL),
                        &button.bounds);
    UIEditorRegisterWidget(editor_id, "icon_button", &button.bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    mouse_inside = CheckCollisionPointRec(mouse_world, button.bounds);
    captured = UIInputCapturesClick(mouse_world);
    active = !button.disabled && !captured && mouse_inside;
    hovered = active && UIHoverEffectsEnabled();
    focused = !button.disabled && button.focus_id > 0 &&
              RegisterUIFocus(button.focus_id, button.bounds);

    if(draw_size <= 0) {
        int available_w = (int)button.bounds.width - icon_padding * 2;
        int available_h = (int)button.bounds.height - icon_padding * 2;
        draw_size = available_w < available_h ? available_w : available_h;
    }
    if(draw_size < 1)
        draw_size = 1;

    if(button.disabled) {
        background.a = background.a > 120 ? 120 : background.a;
        icon_color.a = icon_color.a > 150 ? 150 : icon_color.a;
    }

    draw_background = hovered ? hover_background : background;
    draw_border = hovered ? LightenUIColor(hover_background, cues ? 54 : 40) : border;
    if(cues && hovered) {
        draw_background = LightenUIColor(draw_background, 6);
        icon_color = LightenUIColor(icon_color, 8);
    }

    DrawRectangleRounded(button.bounds, radius, 8, draw_background);
    DrawRectangleRoundedLines(button.bounds, radius, 8, draw_border);
    if(cues && hovered && button.bounds.width > 4 && button.bounds.height > 4) {
        Color cue = LightenUIColor(draw_background, 42);
        cue.a = cue.a > 170 ? 170 : cue.a;
        DrawRectangle((int)button.bounds.x + 2, (int)button.bounds.y + 1,
                      (int)button.bounds.width - 4, ScaleUIPx(1), cue);
    }

    if(button.disabled && !captured && mouse_inside)
        MarkUIDisabled();

    if(active) {
        MarkUIClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            clicked = 1;
    }

    if(focused) {
        SetUIFocusTextInputActive(0);
        DrawUIFocus(button.bounds);
    }

    int icon_x = (int)(button.bounds.x + (button.bounds.width - (float)draw_size) * 0.5f);
    int icon_y = (int)(button.bounds.y + (button.bounds.height - (float)draw_size) * 0.5f);
    if(button.icon.id != 0) {
        Rectangle src = {0, 0, (float)button.icon.width, (float)button.icon.height};
        Rectangle dst = {(float)icon_x, (float)icon_y, (float)draw_size, (float)draw_size};
        DrawTexturePro(button.icon, src, dst, (Vector2){0}, 0, icon_color);
    }
    if(clicked)
        UIConsumeRelease();
    return clicked || IsUIFocusActivatePressed(button.focus_id);
}

int
DrawUITextInputControl(UITextInput input)
{
    char editor_id[96];
    int focused = input.focused;

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "text_input", input.focus_id, NULL),
                        &input.bounds);
    UIEditorRegisterWidget(editor_id, "text_input", &input.bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    if(input.focus_id > 0 && RegisterUIFocus(input.focus_id, input.bounds)) {
        focused = 1;
        SetUIFocusTextInputActive(1);
        DrawUIFocus(input.bounds);
    }

    DrawUITextInput(input.bounds, input.text, input.cursor_position,
                             focused, input.cursor_visible,
                             input.font > 0 ? input.font : GetUIFontSize(),
                             input.style);
    return focused;
}

int
DrawUIHref(UIHref link)
{
    char editor_id[96];
    Vector2 mouse_world = ui_mouse_world();
    int font = link.font > 0 ? link.font : GetUIFontSize();
    const char *text = link.text != NULL ? link.text : "";
    int text_w = MeasureUIText(text, font);
    Rectangle bounds = link.bounds;
    int mouse_inside;
    int captured;
    int active;
    int hovered;
    int focused;
    int clicked = 0;
    Color color = link.color.a != 0 ? link.color : c_link;

    if(bounds.width <= 0)
        bounds.width = (float)text_w;
    if(bounds.height <= 0)
        bounds.height = (float)GetUITextHeight(text, font);
    if(bounds.height <= 0)
        bounds.height = (float)font;

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "href", link.focus_id, text), &bounds);
    UIEditorRegisterWidget(editor_id, "href", &bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    mouse_inside = CheckCollisionPointRec(mouse_world, bounds);
    captured = UIInputCapturesClick(mouse_world);
    active = !link.disabled && !captured && mouse_inside;
    hovered = active && UIHoverEffectsEnabled();
    focused = !link.disabled && link.focus_id > 0 &&
              RegisterUIFocus(link.focus_id, bounds);

    if(link.disabled)
        color = DarkenUIColor(color, 38);
    else if(hovered)
        color = link.hover_color.a != 0 ? link.hover_color : LightenUIColor(color, 18);

    if(active) {
        MarkUIClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            clicked = 1;
    } else if(link.disabled && !captured && mouse_inside) {
        MarkUIDisabled();
    }

    DrawUIText(text, (int)bounds.x,
               GetUIControlTextY(text, (int)bounds.y, (int)bounds.height, font),
               font, color);
    if(hovered && text_w > 0) {
        int underline_y = (int)(bounds.y + bounds.height) - ScaleUIPx(2);
        DrawLine((int)bounds.x, underline_y, (int)bounds.x + text_w,
                 underline_y, color);
    }
    if(focused) {
        SetUIFocusTextInputActive(0);
        DrawUIFocus(bounds);
    }
    if(clicked)
        UIConsumeRelease();
    if(!link.disabled && (clicked || IsUIFocusActivatePressed(link.focus_id))) {
        if(link.href != NULL && link.href[0] != '\0') {
#if defined(PLATFORM_WEB)
            EM_ASM({
                window.location.href = UTF8ToString($0);
            }, link.href);
#else
            OpenURL(link.href);
#endif
        }
        return 1;
    }
    return 0;
}

static int
ui_text_line_start(const char *text, int cursor)
{
    int i;

    if(text == NULL || cursor <= 0)
        return 0;
    for(i = cursor - 1; i >= 0; i--) {
        if(text[i] == '\n')
            return i + 1;
    }
    return 0;
}

static int
ui_text_line_end(const char *text, int cursor)
{
    int len;
    int i;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    cursor = ui_clampi(cursor, 0, len);
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
    return MeasureUIText(prefix, font);
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
        if(MeasureUIText(prefix, font) >= target_x)
            return i;
    }
    return end;
}

static int
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
    return ui_text_cursor_from_line_x(text, other_start, other_end, font, target_x);
}

static int
ui_text_area_line_font(const char *text, int start, int end, int base_font)
{
    int hashes = 0;

    if(text == NULL)
        return base_font;
    while(start + hashes < end && hashes < 6 && text[start + hashes] == '#')
        hashes++;
    if(hashes > 0 && start + hashes < end && text[start + hashes] == ' ' && base_font < UI_TEXT_24) {
        if(hashes <= 2)
            return UI_TEXT_24;
    }
    return base_font;
}

static int
ui_text_area_line_height(const char *text, int start, int end, int base_font, int line_gap)
{
    int line_font = ui_text_area_line_font(text, start, end, base_font);
    return GetUITextLineHeight(line_font) + line_gap;
}

static int
ui_text_area_content_height(const char *text, int font, int line_gap)
{
    int len;
    int line_start = 0;
    int height = 0;

    if(text == NULL)
        text = "";
    len = (int)strlen(text);
    for(int i = 0; i <= len; i++) {
        if(text[i] == '\n' || text[i] == '\0') {
            height += ui_text_area_line_height(text, line_start, i, font, line_gap);
            line_start = i + 1;
        }
    }
    return height;
}

static int
ui_text_area_cursor_from_point(const char *text, int font, int line_gap, int x, int y, int mouse_x, int mouse_y, int scroll_y)
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
            int line_h = GetUITextLineHeight(line_font) + line_gap;
            if(target_y < draw_y + line_h || text[i] == '\0')
                return ui_text_cursor_from_line_x(text, line_start, i, line_font, mouse_x - x);
            draw_y += line_h;
            line_start = i + 1;
        }
    }
    return len;
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
        "action", "args", "bind", "button", "checkbox", "circle",
        "const", "def", "down", "dropdown", "elif", "else", "enter",
        "exit", "fn", "guard", "icon_button", "if", "include", "label",
        "let", "listen", "logic", "native", "on", "page", "radiobutton",
        "screen", "set", "slider", "spacer", "switch", "text", "tick",
        "toggle", "up", "var", "window", "write"
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
        "double", "else", "enum", "float", "for", "if", "int", "long",
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
ui_syntax_token_color(UISyntaxMode syntax, const char *text, int len,
                      int first_token, UITextInputStyle style)
{
    Color keyword = (Color){0, 0, 170, 255};
    Color string = (Color){0, 116, 32, 255};
    Color number = (Color){128, 0, 128, 255};
    Color path = (Color){128, 64, 0, 255};
    Color comment = (Color){96, 96, 96, 255};

    if(len <= 0)
        return style.text;
    if((syntax == UI_SYNTAX_KRY || syntax == UI_SYNTAX_MAKE) &&
       first_token && text[0] == '#')
        return comment;
    if(text[0] == '"')
        return string;
    if((text[0] >= '0' && text[0] <= '9') ||
       (text[0] == '-' && len > 1 && text[1] >= '0' && text[1] <= '9'))
        return number;
    if(syntax == UI_SYNTAX_KRY &&
       (text[0] == '/' ||
        (text[0] == '.' && len > 1 && (text[1] == '/' || text[1] == '.')) ||
        text[0] == '%'))
        return path;
    if(syntax == UI_SYNTAX_KRY && ui_syntax_kry_keyword(text, len))
        return keyword;
    if(syntax == UI_SYNTAX_C && ui_syntax_c_keyword(text, len))
        return keyword;
    if(syntax == UI_SYNTAX_MAKE && ui_syntax_make_keyword(text, len))
        return keyword;
    if(syntax == UI_SYNTAX_MAKE && len > 1 && text[0] == '$')
        return path;
    return style.text;
}

static int
ui_syntax_token_len(const char *line, int len, int index, UISyntaxMode syntax,
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
    if((syntax == UI_SYNTAX_KRY || syntax == UI_SYNTAX_MAKE) &&
       first_token && line[i] == '#')
        return len - index;
    if(syntax == UI_SYNTAX_C && line[i] == '/' && i + 1 < len &&
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
       (syntax == UI_SYNTAX_MAKE && line[i] == '$') ||
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
ui_draw_syntax_line(const char *line, int len, int x, int y, int font,
                    UISyntaxMode syntax, UITextInputStyle style)
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
        DrawUIText(token, x, y, font, color);
        x += MeasureUIText(token, font);
        offset += token_len;
    }
}

static void
ui_draw_text_area_text(const char *text, int cursor, int focused,
                       Rectangle bounds, int font, int line_gap,
                       int scroll_y, UISyntaxMode syntax,
                       UITextInputStyle style)
{
    char line[1024];
    int len;
    int line_start = 0;
    int padding_x = style.padding_x > 0 ? style.padding_x : ScaleUIPx(10);
    int padding_y = style.padding_y > 0 ? style.padding_y : ScaleUIPx(8);
    int text_x = (int)bounds.x + padding_x;
    int text_y = (int)bounds.y + padding_y - scroll_y;
    int draw_y = text_y;

    if(text == NULL)
        text = "";
    len = (int)strlen(text);
    for(int i = 0; i <= len; i++) {
        if(text[i] == '\n' || text[i] == '\0') {
            int line_len = i - line_start;
            int line_font = ui_text_area_line_font(text, line_start, i, font);
            int line_h = GetUITextLineHeight(line_font) + line_gap;
            if(line_len >= (int)sizeof(line))
                line_len = (int)sizeof(line) - 1;
            memcpy(line, text + line_start, (size_t)line_len);
            line[line_len] = '\0';
            if(syntax == UI_SYNTAX_NONE)
                DrawUIText(line, text_x, draw_y, line_font, style.text);
            else
                ui_draw_syntax_line(line, line_len, text_x, draw_y,
                                    line_font, syntax, style);
            if(focused && cursor >= line_start && cursor <= i && (((int)(GetTime() * 2.0)) % 2 == 0)) {
                int cursor_x = text_x + ui_text_column_x(text, line_start, cursor, line_font);
                DrawRectangle(cursor_x, draw_y, ScaleUIPx(2), GetUITextLineHeight(line_font), style.cursor);
            }
            draw_y += line_h;
            line_start = i + 1;
        }
    }
}

int
DrawUITextArea(UITextArea area)
{
    char editor_id[96];
    int changed = 0;
    int focused;
    int font;
    int line_gap;
    int line_h;
    int padding_x;
    int padding_y;
    int first_line_y;
    int scroll_y;
    int content_h;
    int max_scroll;
    Vector2 mouse_world;
    int mouse_inside;
    int captured;
    Color border;
    float radius;
    int enter_requested;
    int drag_id;

    if(area.text == NULL || area.text_size == 0 || area.cursor_position == NULL || area.focused == NULL)
        return 0;

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "text_area", area.focus_id, area.placeholder),
                        &area.bounds);
    UIEditorRegisterWidget(editor_id, "text_area", &area.bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    font = area.font > 0 ? area.font : GetUIFontSize();
    line_gap = area.line_gap >= 0 ? area.line_gap : ScaleUIPx(6);
    line_h = GetUITextLineHeight(font) + line_gap;
    padding_x = area.style.padding_x > 0 ? area.style.padding_x : ScaleUIPx(10);
    padding_y = area.style.padding_y > 0 ? area.style.padding_y : ScaleUIPx(8);
    first_line_y = GetUIControlTextY("Hg", (int)area.bounds.y + padding_y,
                                     line_h, font);
    focused = *area.focused != 0;
    scroll_y = area.scroll_y != NULL ? *area.scroll_y : 0;

    if(area.focus_id > 0 && RegisterUIFocus(area.focus_id, area.bounds)) {
        focused = 1;
        DrawUIFocus(area.bounds);
    }

    mouse_world = ui_mouse_world();
    mouse_inside = CheckCollisionPointRec(mouse_world, area.bounds);
    captured = UIInputCapturesClick(mouse_world);
    if(mouse_inside && !captured)
        MarkUITextCursor();
    drag_id = area.focus_id > 0 ? area.focus_id : 1;

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(mouse_inside && !captured) {
            focused = 1;
            g_ui_text_area_drag_id = drag_id;
            *area.cursor_position = ui_text_area_cursor_from_point(area.text, font, line_gap,
                (int)area.bounds.x + padding_x, (int)area.bounds.y + padding_y,
                (int)mouse_world.x, (int)mouse_world.y, scroll_y);
        } else if(focused) {
            focused = 0;
        }
    }
    if(g_ui_text_area_drag_id == drag_id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if((int)mouse_world.y < (int)area.bounds.y + padding_y)
            scroll_y -= line_h;
        if((int)mouse_world.y > (int)(area.bounds.y + area.bounds.height) - padding_y)
            scroll_y += line_h;
        if(scroll_y < 0)
            scroll_y = 0;
        focused = 1;
        *area.cursor_position = ui_text_area_cursor_from_point(area.text, font, line_gap,
            (int)area.bounds.x + padding_x, (int)area.bounds.y + padding_y,
            (int)mouse_world.x, (int)mouse_world.y, scroll_y);
    }
    if(g_ui_text_area_drag_id == drag_id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_text_area_drag_id = 0;

    *area.focused = focused;
    SetUIFocusTextInputActive(focused);
    enter_requested = focused && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || g_ui_text_input_enter_count > 0);
    if(focused) {
        changed = EditUIText((UITextEdit){
            .text = area.text,
            .text_size = area.text_size,
            .cursor_position = area.cursor_position,
            .max_codepoints = area.max_codepoints,
            .filter = area.filter,
            .filter_user_data = area.filter_user_data
        });
        if(enter_requested) {
            int len = (int)strlen(area.text);
            *area.cursor_position = ui_clampi(*area.cursor_position, 0, len);
            if((size_t)(len + 2) <= area.text_size) {
                memmove(area.text + *area.cursor_position + 1,
                        area.text + *area.cursor_position,
                        (size_t)(len - *area.cursor_position + 1));
                area.text[*area.cursor_position] = '\n';
                (*area.cursor_position)++;
                changed = 1;
            }
            g_ui_text_input_enter_count = 0;
        }
        if(IsKeyPressed(KEY_UP))
            *area.cursor_position = ui_text_move_vertical(area.text, *area.cursor_position, font, -1);
        if(IsKeyPressed(KEY_DOWN))
            *area.cursor_position = ui_text_move_vertical(area.text, *area.cursor_position, font, 1);
    } else {
        int text_len = (int)strlen(area.text);
        *area.cursor_position = ui_clampi(*area.cursor_position, 0, text_len);
    }

    content_h = ui_text_area_content_height(area.text, font, line_gap);
    max_scroll = content_h - ((int)area.bounds.height - padding_y * 2);
    if(max_scroll < 0)
        max_scroll = 0;
    if(mouse_inside && !captured)
        scroll_y -= (int)(GetMouseWheelMove() * (float)line_h * 3.0f);
    scroll_y = ui_clampi(scroll_y, 0, max_scroll);
    if(area.scroll_y != NULL)
        *area.scroll_y = scroll_y;

    border = focused ? area.style.focus_border : area.style.border;
    radius = area.style.radius >= 0.0f ? area.style.radius : 0.12f;
    if(radius <= 0.0f) {
        DrawRectangleRec(area.bounds, area.style.background);
        DrawRectangleLinesEx(area.bounds, 1, border);
    } else {
        DrawRectangleRounded(area.bounds, radius, 8, area.style.background);
        DrawRectangleRoundedLines(area.bounds, radius, 8, border);
    }

    ui_begin_world_clip((Rectangle){area.bounds.x + padding_x, area.bounds.y + padding_y,
                                    area.bounds.width - padding_x * 2, area.bounds.height - padding_y * 2});
    if(area.text[0] == '\0' && !focused && area.placeholder != NULL)
        DrawUIText(area.placeholder, (int)area.bounds.x + padding_x,
                   first_line_y, font, area.style.border);
    else
        ui_draw_text_area_text(area.text, *area.cursor_position, focused,
                               area.bounds, font, line_gap, scroll_y,
                               area.syntax, area.style);
    EndUIClip();
    return changed;
}

int
DrawUITextField(UITextField field)
{
    char editor_id[96];
    int changed = 0;
    int focused;
    int font;
    int padding_x;
    int commit_pressed = 0;
    Vector2 mouse_world;
    int mouse_inside;
    int captured;

    if(field.commit_pressed != NULL)
        *field.commit_pressed = 0;
    if(field.text == NULL || field.text_size == 0 ||
       field.cursor_position == NULL || field.focused == NULL)
        return 0;

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "text_field", field.focus_id, NULL), &field.bounds);
    UIEditorRegisterWidget(editor_id, "text_field", &field.bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    font = field.font > 0 ? field.font : GetUIFontSize();
    padding_x = field.style.padding_x > 0 ? field.style.padding_x : ScaleUIPx(10);
    focused = *field.focused != 0;

    if(field.focus_id > 0 && RegisterUIFocus(field.focus_id, field.bounds)) {
        focused = 1;
        DrawUIFocus(field.bounds);
    }

    mouse_world = ui_mouse_world();
    mouse_inside = CheckCollisionPointRec(mouse_world, field.bounds);
    captured = UIInputCapturesClick(mouse_world);
    if(mouse_inside && !captured)
        MarkUITextCursor();

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(mouse_inside && !captured) {
            focused = 1;
            *field.cursor_position = ui_text_cursor_from_x(
                field.text, font, (int)field.bounds.x + padding_x,
                (int)mouse_world.x);
        } else if(focused) {
            focused = 0;
        }
    }

    *field.focused = focused;
    SetUIFocusTextInputActive(focused);

    if(focused) {
        changed = EditUIText((UITextEdit){
            .text = field.text,
            .text_size = field.text_size,
            .cursor_position = field.cursor_position,
            .max_codepoints = field.max_codepoints,
            .filter = field.filter,
            .filter_user_data = field.filter_user_data,
            .commit_pressed = field.commit_pressed != NULL
                                  ? field.commit_pressed
                                  : &commit_pressed
        });
    } else {
        int len = (int)strlen(field.text);
        *field.cursor_position = ui_clampi(*field.cursor_position, 0, len);
    }

    DrawUITextInput(field.bounds, field.text, *field.cursor_position,
                             focused, focused && (((int)(GetTime() * 2.0)) % 2 == 0),
                             font, field.style);
    return changed;
}

int
GetUIReadonlyTextBoxHeight(const char *text, int font, int width,
                                  UITextInputStyle style, int line_gap)
{
    char line[1024];
    int len;
    int offset = 0;
    int line_count = 0;
    int padding_x = style.padding_x > 0 ? style.padding_x : ScaleUIPx(10);
    int padding_y = style.padding_y > 0 ? style.padding_y : ScaleUIPx(8);
    int content_w = width - padding_x * 2;

    if(font <= 0)
        font = GetUIFontSize();
    if(line_gap < 0)
        line_gap = 0;
    if(content_w < ScaleUIPx(24))
        content_w = ScaleUIPx(24);
    if(text == NULL)
        text = "";

    len = (int)strlen(text);
    while(offset < len) {
        int chunk_len;

        if(MeasureUIText(text + offset, font) <= content_w) {
            line_count++;
            break;
        }
        chunk_len = 1;
        while(offset + chunk_len < len && chunk_len + 1 < (int)sizeof(line)) {
            snprintf(line, sizeof(line), "%.*s", chunk_len + 1, text + offset);
            if(MeasureUIText(line, font) > content_w)
                break;
            chunk_len++;
        }
        offset += chunk_len;
        line_count++;
    }
    if(line_count < 1)
        line_count = 1;
    return padding_y * 2 + line_count * GetUITextLineHeight(font) +
           (line_count - 1) * line_gap;
}

int
DrawUIReadonlyTextBox(UIReadonlyTextBox box)
{
    char editor_id[96];
    char line[1024];
    const char *text = box.text != NULL ? box.text : "";
    int font = box.font > 0 ? box.font : GetUIFontSize();
    int padding_x = box.style.padding_x > 0 ? box.style.padding_x : ScaleUIPx(10);
    int padding_y = box.style.padding_y > 0 ? box.style.padding_y : ScaleUIPx(8);
    int line_gap = box.line_gap > 0 ? box.line_gap : 0;
    int content_w = (int)box.bounds.width - padding_x * 2;
    int line_h = GetUITextLineHeight(font);
    int offset = 0;
    int len = (int)strlen(text);
    int draw_y = (int)box.bounds.y + padding_y;
    float radius = box.style.radius >= 0.0f ? box.style.radius : 0.12f;
    Vector2 mouse_world = ui_mouse_world();
    int active;

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "readonly_text_box", 0, text), &box.bounds);
    UIEditorRegisterWidget(editor_id, "readonly_text_box", &box.bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    active = CheckCollisionPointRec(mouse_world, box.bounds) &&
             !UIInputCapturesClick(mouse_world);

    if(content_w < ScaleUIPx(24))
        content_w = ScaleUIPx(24);

    if(radius <= 0.0f) {
        DrawRectangleRec(box.bounds, box.style.background);
        DrawRectangleLinesEx(box.bounds, 1, box.style.border);
    } else {
        DrawRectangleRounded(box.bounds, radius, 8, box.style.background);
        DrawRectangleRoundedLines(box.bounds, radius, 8, box.style.border);
    }

    ui_begin_world_clip((Rectangle){box.bounds.x + (float)padding_x, box.bounds.y,
                                    (float)content_w, box.bounds.height});
    while(offset < len) {
        int chunk_len;

        if(MeasureUIText(text + offset, font) <= content_w) {
            snprintf(line, sizeof(line), "%s", text + offset);
            DrawUIText(line, (int)box.bounds.x + padding_x, draw_y,
                            font, box.style.text);
            break;
        }

        chunk_len = 1;
        while(offset + chunk_len < len && chunk_len + 1 < (int)sizeof(line)) {
            snprintf(line, sizeof(line), "%.*s", chunk_len + 1, text + offset);
            if(MeasureUIText(line, font) > content_w)
                break;
            chunk_len++;
        }
        snprintf(line, sizeof(line), "%.*s", chunk_len, text + offset);
        DrawUIText(line, (int)box.bounds.x + padding_x, draw_y,
                        font, box.style.text);
        draw_y += line_h + line_gap;
        offset += chunk_len;
    }
    if(len == 0)
        DrawUIText("", (int)box.bounds.x + padding_x, draw_y, font, box.style.text);
    EndUIClip();

    if(active) {
        MarkUIClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            UIConsumeRelease();
            return 1;
        }
    }
    return 0;
}

static UITextLayout
UIParagraphLayout(UIParagraph paragraph)
{
    int font = paragraph.font > 0 ? paragraph.font : GetUIFontSize();
    int line_gap = paragraph.line_gap > 0 ? paragraph.line_gap : ScaleUIPx(4);
    int icon_size = paragraph.icon_size > 0 ? paragraph.icon_size : font;
    UITextLayout layout = ParseUITextLayout(paragraph.text ? paragraph.text : "",
                                                     paragraph.icon,
                                                     paragraph.icon_type,
                                                     icon_size);
    ReflowUITextLayout(&layout, paragraph.width, font, line_gap);
    return layout;
}

int
GetUIParagraphHeight(UIParagraph paragraph)
{
    if(paragraph.width <= 0)
        return 0;
    UITextLayout layout = UIParagraphLayout(paragraph);
    int height = GetUITextLayoutHeight(&layout);
    FreeUITextLayout(&layout);
    return height;
}

void
DrawUIParagraph(UIParagraph paragraph, int x, int *y)
{
    if(y == NULL || paragraph.width <= 0)
        return;
    int font = paragraph.font > 0 ? paragraph.font : GetUIFontSize();
    Color color = paragraph.color.a != 0 ? paragraph.color : c_text;
    UITextLayout layout = UIParagraphLayout(paragraph);
    DrawUITextLayout(&layout, x, y, font, color);
    FreeUITextLayout(&layout);
}

void
DrawUIBevel(int x, int y, int w, int h, Color light, Color dark)
{
    DrawLine(x, y, x + w - 1, y, light);
    DrawLine(x, y, x, y + h - 1, light);
    DrawLine(x, y + h - 1, x + w - 1, y + h - 1, dark);
    DrawLine(x + w - 1, y, x + w - 1, y + h - 1, dark);
}

void
DrawUITextLines(const char **lines, int count, int x, int *y, int font, int line_h, Color color)
{
    for(int i = 0; i < count; i++) {
        DrawUIText(lines[i], x, *y, font, color);
        *y += line_h;
    }
}

int
GetUIIconBtnSize(int size)
{
    switch(size) {
    case UI_ICON_SIZE_TINY: return ClampUIPx(12, 10, 18);
    case UI_ICON_SIZE_SMALL: return ClampUIPx(14, 12, 20);
    case UI_ICON_SIZE_MEDIUM: return ClampUIPx(16, 14, 24);
    case UI_ICON_SIZE_LARGE: return ClampUIPx(20, 18, 28);
    default: return ClampUIPx(14, 12, 20);
    }
}

int
GetUIIconBtnPadding(int size)
{
    switch(size) {
    case UI_ICON_SIZE_TINY: return ScaleUIPx(4);
    case UI_ICON_SIZE_SMALL: return ScaleUIPx(5);
    case UI_ICON_SIZE_MEDIUM: return ScaleUIPx(6);
    case UI_ICON_SIZE_LARGE: return ScaleUIPx(7);
    default: return ScaleUIPx(5);
    }
}

void
InitUI(int width, int height, float dpi)
{
    ui_view_width = width;
    ui_view_height = height;
    SetUIScale(dpi);
}

int
IsUIDesktopMode(void)
{
    return ui_view_width >= ScaleUIPx(500);
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
SetUILinkColor(Color link)
{
    c_link = link;
}

void
ApplyCurrentUITheme(void)
{
    ui_set_theme_colors(GetThemeText(),
                        GetThemeBackground(),
                        GetThemeSurface(),
                        GetThemeCircle(),
                        GetThemeButton(),
                        GetThemeButtonHover(),
                        GetThemeIcon());
    SetUILinkColor(GetThemeLink());
}

Camera2D
GetUIDefaultCamera(void)
{
    return (Camera2D){
        .offset = {0.0f, 0.0f},
        .target = {0.0f, 0.0f},
        .rotation = 0.0f,
        .zoom = 1.0f
    };
}

static float
ui_sane_float(float value, float fallback)
{
    return isfinite(value) ? value : fallback;
}

static Camera2D
ui_sane_camera(Camera2D camera)
{
    camera.offset.x = ui_sane_float(camera.offset.x, 0.0f);
    camera.offset.y = ui_sane_float(camera.offset.y, 0.0f);
    camera.target.x = ui_sane_float(camera.target.x, 0.0f);
    camera.target.y = ui_sane_float(camera.target.y, 0.0f);
    camera.rotation = ui_sane_float(camera.rotation, 0.0f);
    if(!isfinite(camera.zoom) || fabsf(camera.zoom) < 0.0001f)
        camera.zoom = 1.0f;
    return camera;
}

void
BeginUIFrame(int width, int height, float dpi)
{
    SetUIViewSize(width, height);
    InitUI(width, height, dpi);
    SetUIFrame(GetUIDefaultCamera());
}

void
SetUIFrame(Camera2D camera)
{
    int text_input_active = g_ui_text_input_requested != 0;

    EndUIFocus();
    BeginUIFocus();

    if(g_ui_platform_text_input_active != text_input_active) {
        g_ui_platform_text_input_active = text_input_active;
        if(g_ui_text_input_platform_callback != NULL)
            g_ui_text_input_platform_callback(text_input_active);
    }
    g_ui_text_input_requested = 0;
    g_ui_focus_text_input_active = 0;
    g_ui_cursor_priority = UI_CURSOR_PRIORITY_DEFAULT;
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);

    g_ui_camera = ui_sane_camera(camera);
    ui_update_pointer_gesture();
    ClearUIInputCaptures();
    g_ui_input_clip_stack_count = 0;
    ResetUIClip();
    BeginUIEditorFrame(NULL);
}

void
SetUIModalCapture(Rectangle bounds)
{
    ClearUIInputCaptures();
    PushUIInputCapture(bounds, 1);
}

void
SetUITextInputPlatformCallback(UITextInputPlatformCallback callback)
{
    g_ui_text_input_platform_callback = callback;
}

void
SetUICursorClickable(int *cursor_clickable)
{
    g_ui_cursor_clickable = cursor_clickable;
}

void
SetUICursorDisabled(int *cursor_disabled)
{
    g_ui_cursor_disabled = cursor_disabled;
}

void
SetUIIcons(Texture2D gear_icon, Texture2D x_icon)
{
    g_ui_gear_icon = gear_icon;
    g_ui_x_icon = x_icon;
}

/* ================================================================
 * ICON BUTTONS
 * ================================================================ */

int
GetUIIconButtonSize(UIIconSize size)
{
    return GetUIIconBtnSize((int)size);
}

int
GetUIIconButtonPadding(UIIconSize size)
{
    return GetUIIconBtnPadding((int)size);
}

void
DrawUIIconTexture(int x, int y, int size, Texture2D icon, Color tint)
{
    Rectangle src;
    Rectangle dst;

    if(icon.id == 0 || size <= 0)
        return;

    src = (Rectangle){0, 0, (float)icon.width, (float)icon.height};
    dst = (Rectangle){(float)x, (float)y, (float)size, (float)size};
    DrawTexturePro(icon, src, dst, (Vector2){0}, 0, tint);
}

int
DrawUIIconBtn(int x, int y, UIIconSize size, Texture2D icon, int *hover)
{
    int btn_size = GetUIIconButtonSize(size);
    int padding = GetUIIconButtonPadding(size);
    int w = btn_size + padding * 2;
    int h = btn_size + padding * 2;
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    Vector2 mouse_world = ui_mouse_world();
    int hovered = CheckCollisionPointRec(mouse_world, bounds) &&
                  !UIInputCapturesClick(mouse_world) &&
                  UIHoverEffectsEnabled();

    if(hover != NULL)
        *hover = hovered;
    return DrawUIIconButton((UIIconButton){
        .bounds = bounds,
        .icon = icon,
        .icon_size = btn_size,
        .icon_padding = padding,
        .background = c_button,
        .hover_background = c_button_hover,
        .icon_color = c_icon,
        .border = DarkenUIColor(c_button, 35),
        .radius = 0.12f
    });
}

int
DrawUIPaddedIconBtn(int x, int y, int size, int padding, Texture2D icon, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int w = size + padding * 2;
    int h = size + padding * 2;
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int hovered = CheckCollisionPointRec(mouse_world, bounds) &&
                  !UIInputCapturesClick(mouse_world) &&
                  UIHoverEffectsEnabled();

    if(hover != NULL)
        *hover = hovered;
    return DrawUIIconButton((UIIconButton){
        .bounds = bounds,
        .icon = icon,
        .icon_size = size,
        .icon_padding = padding,
        .background = c_button,
        .hover_background = c_button_hover,
        .icon_color = c_icon,
        .border = DarkenUIColor(c_button, 35),
        .radius = 0.12f
    });
}

int
DrawUITextButton(int x, int y, const char *label, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int font = GetUISmallFontSize();
    const char *text = label != NULL ? label : "";
    int w = (int)MeasureUIText(text, font) + ScaleUIPx(16);
    int h = GetUITextLineHeight(font) + ScaleUIPx(8);
    Rectangle bounds;
    int hovered;

    x = x - w / 2;
    bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    hovered = CheckCollisionPointRec(mouse_world, bounds) &&
              !UIInputCapturesClick(mouse_world) &&
              UIHoverEffectsEnabled();
    if(hover != NULL)
        *hover = hovered;
    return DrawUIButton((UIButton){
        .bounds = bounds,
        .label = text,
        .font = font,
        .background = c_button,
        .hover_background = c_button_hover,
        .text = c_text,
        .border = LightenUIColor(c_button, 32),
        .radius = 0.06f
    });
}

static void
ui_button_style_colors(UIButtonStyle style, Color *bg, Color *hover_bg,
                       Color *text_color)
{
    switch(style) {
    case UI_BUTTON_STYLE_SECONDARY:
        *bg = DarkenUIColor(c_bg, 14);
        *hover_bg = c_button;
        *text_color = c_text;
        return;
    case UI_BUTTON_STYLE_DANGER:
        *bg = (Color){180, 70, 70, 255};
        *hover_bg = (Color){200, 90, 90, 255};
        *text_color = c_text;
        return;
    case UI_BUTTON_STYLE_TAB:
        *bg = DarkenUIColor(c_bg, 10);
        *hover_bg = c_button;
        *text_color = c_text;
        return;
    case UI_BUTTON_STYLE_TAB_SELECTED:
        *bg = c_button;
        *hover_bg = c_button;
        *text_color = c_text;
        return;
    case UI_BUTTON_STYLE_PRIMARY:
    default:
        *bg = c_button;
        *hover_bg = c_button_hover;
        *text_color = c_text;
        return;
    }
}

int
DrawUIGenericButton(int x, int y, int w, int h, const char *label,
                       UIButtonStyle style, int disabled, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int font = GetUISmallFontSize();
    Rectangle bounds = {x, y, w, h};
    int mouse_inside = CheckCollisionPointRec(mouse_world, bounds);
    int captured = UIInputCapturesClick(mouse_world);
    int hovered = mouse_inside && !disabled && !captured && UIHoverEffectsEnabled();
    int clicked;
    Color bg;
    Color hover_bg;
    Color text_color;

    ui_button_style_colors(style, &bg, &hover_bg, &text_color);
    if(disabled) {
        bg = DarkenUIColor(bg, 22);
        text_color = DarkenUIColor(text_color, 70);
    }

    if(hover != NULL)
        *hover = hovered;

    clicked = DrawUIButton((UIButton){
        .bounds = bounds,
        .label = label,
        .font = font,
        .disabled = disabled,
        .background = bg,
        .hover_background = hover_bg,
        .text = text_color,
        .border = LightenUIColor(bg, 32),
        .radius = 0.08f
    });

    if(UITransitionCuesEnabled() && style == UI_BUTTON_STYLE_TAB_SELECTED &&
       !disabled && w > ScaleUIPx(18)) {
        int cue_h = ScaleUIPx(2);
        if(cue_h < 1)
            cue_h = 1;
        DrawRectangle(x + ScaleUIPx(9), y + h - cue_h, w - ScaleUIPx(18),
                      cue_h, LightenUIColor(c_button_hover, 18));
    }

    return clicked;
}

int
DrawUISubtabBar(UISubtabBar bar)
{
    Vector2 mouse_world = ui_mouse_world();
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int clicked_tab = -1;
    int font = bar.font > 0 ? bar.font : GetUIFontSize();
    int tab_w;
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int bar_w = (int)bar.bounds.width;
    int bar_h = (int)bar.bounds.height;
    int cues = UITransitionCuesEnabled();

    if(bar.tabs == NULL || bar.count <= 0 || bar.bounds.width <= 0 || bar.bounds.height <= 0)
        return -1;

    tab_w = bar_w / bar.count;
    if(tab_w <= 0)
        return -1;

    DrawRectangle(bar_x, bar_y, bar_w, bar_h, DarkenUIColor(c_bg, 8));
    DrawLine(bar_x, bar_y, bar_x + bar_w, bar_y, DarkenUIColor(c_bg, 34));
    DrawLine(bar_x, bar_y + bar_h - 1, bar_x + bar_w, bar_y + bar_h - 1, DarkenUIColor(c_bg, 38));

    for(int i = 0; i < bar.count; i++) {
        int tab_x = bar_x + i * tab_w;
        int tab_h = bar_h;
        int is_last = i == bar.count - 1;
        int draw_w = is_last ? bar_x + bar_w - tab_x : tab_w;
        int label_pad = ScaleUIPx(4);
        Rectangle tab_rect = {(float)tab_x, bar.bounds.y, (float)draw_w, bar.bounds.height};
        Rectangle label_rect = {(float)(tab_x + label_pad), bar.bounds.y,
                                (float)(draw_w - label_pad * 2), bar.bounds.height};
        int input_captured = UIInputCapturesClick(mouse_world);
        int is_active = CheckCollisionPointRec(mouse_world, tab_rect) && !input_captured;
        int is_hovered = is_active && UIHoverEffectsEnabled();
        int is_selected = i == bar.selected_index;
        int is_disabled = bar.tabs[i].disabled;
        Color accent = bar.tabs[i].accent.a != 0 ? bar.tabs[i].accent : c_button_hover;
        Color text_color = c_text;
        const char *label = bar.tabs[i].label ? bar.tabs[i].label : "";
        Texture2D icon = bar.tabs[i].icon;
        int icon_size = bar.tabs[i].icon_size > 0 ? bar.tabs[i].icon_size : ScaleUIPx(20);

        if(is_disabled) {
            text_color = DarkenUIColor(c_text, 70);
            text_color.a = text_color.a > 150 ? 150 : text_color.a;
        }

        if(is_hovered && !is_disabled && !is_selected)
            DrawRectangle(tab_x, bar_y + ScaleUIPx(2), draw_w, tab_h - ScaleUIPx(4),
                          cues ? LightenUIColor(DarkenUIColor(c_button_hover, 10), 6)
                               : DarkenUIColor(c_button_hover, 10));

        if(is_selected) {
            int underline_h = ScaleUIPx(cues ? 4 : 3);
            if(cues) {
                Color glow = accent;
                glow.a = glow.a > 90 ? 90 : glow.a;
                DrawRectangle(tab_x + ScaleUIPx(10), bar_y + tab_h - underline_h - ScaleUIPx(2),
                              draw_w - ScaleUIPx(20), ScaleUIPx(2), glow);
            }
            DrawRectangle(tab_x + ScaleUIPx(10), bar_y + tab_h - underline_h,
                          draw_w - ScaleUIPx(20), underline_h, accent);
        }

        if(i > 0)
            DrawLine(tab_x, bar_y + ScaleUIPx(8), tab_x, bar_y + tab_h - ScaleUIPx(8),
                     DarkenUIColor(c_bg, 24));

        if(is_active) {
            if(is_disabled)
                MarkUIDisabled();
            else if(!is_selected)
                MarkUIClickable();

            if(released)
                clicked_tab = i;
        }

        if(label_rect.width < 1)
            label_rect.width = 1;
        if(icon.id != 0) {
            Rectangle src = {0, 0, (float)icon.width, (float)icon.height};
            Rectangle dst = {
                tab_x + (draw_w - icon_size) / 2.0f,
                bar_y + (tab_h - icon_size) / 2.0f,
                (float)icon_size,
                (float)icon_size
            };
            Color icon_color = is_disabled ? DarkenUIColor(c_icon, 40) : c_icon;
            DrawTexturePro(icon, src, dst, (Vector2){0}, 0, icon_color);
        } else {
            DrawFittedUITextInRect(label, label_rect, font, UI_TEXT_8, text_color);
        }
    }

    if(clicked_tab >= 0)
        UIConsumeRelease();
    return clicked_tab;
}

void
DrawUIIconLink(int x, int y, int icon_size, Texture2D icon, const char *url)
{
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;
    int padding = ScaleUIPx(4);
    int btn_w = icon_size + padding * 2;
    int btn_h = icon_size + padding * 2;
    int btn_x = x - padding;
    int btn_y = y - padding;

    if(mx > btn_x && mx < btn_x + btn_w && my > btn_y && my < btn_y + btn_h &&
       !UIInputCapturesClick(mouse_world)) {
        hover = UIHoverEffectsEnabled();
        MarkUIClickable();
    }

    if(hover) {
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, c_button_hover);
        DrawUIBevel(btn_x, btn_y, btn_w, btn_h, DarkenUIColor(c_button_hover, 40), LightenUIColor(c_button_hover, 40));
    } else {
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, c_button);
        DrawUIBevel(btn_x, btn_y, btn_w, btn_h, LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));
    }

    DrawUIIconTexture(x, y, icon_size, icon, c_icon);

    if(mx > btn_x && mx < btn_x + btn_w && my > btn_y && my < btn_y + btn_h &&
       !UIInputCapturesClick(mouse_world) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        UIConsumeRelease();
#if defined(PLATFORM_WEB)
        EM_ASM({
            window.location.href = UTF8ToString($0);
        }, url);
#else
        OpenURL(url);
#endif
    }
}

/* ================================================================
 * CONTROLS
 * ================================================================ */

static Rectangle
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

int
DrawUISlider(int id, int x, int y, int w, const char *label,
               int min, int max, int *value, const char *suffix)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)x, (float)y, (float)w, (float)ScaleUIPx(56)};
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int label_font = GetUIFontSize();
    int value_font = GetUIFontSize();
    int track_y = y + ScaleUIPx(28);
    int track_h = ScaleUIPx(8);
    int knob_w = ScaleUIPx(12);
    int knob_h = ScaleUIPx(22);
    int knob_y = track_y - (knob_h - track_h) / 2;
    int min_touch_h = ScaleUIPx(36);
    int changed = 0;
    char value_text[32];
    Rectangle hit = ui_centered_min_hit_rect(x, knob_y, w, knob_h, w, min_touch_h);

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "slider", id, label), &editor_bounds);
    x = (int)editor_bounds.x;
    y = (int)editor_bounds.y;
    w = (int)editor_bounds.width;
    if(w < ScaleUIPx(32))
        w = ScaleUIPx(32);
    track_y = y + ScaleUIPx(28);
    knob_y = track_y - (knob_h - track_h) / 2;
    hit = ui_centered_min_hit_rect(x, knob_y, w, knob_h, w, min_touch_h);
    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)ScaleUIPx(56)};
    UIEditorRegisterWidget(editor_id, "slider", &editor_bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    snprintf(value_text, sizeof(value_text), "%d%s", *value, suffix != NULL ? suffix : "");
    DrawUIText(label, x, y, label_font, c_text);
    DrawUIText(value_text, x + w - MeasureUIText(value_text, value_font), y, value_font, c_text);

    DrawRectangle(x, track_y, w, track_h, DarkenUIColor(c_bg, 28));
    DrawUIBevel(x, track_y, w, track_h, DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));

    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
        MarkUIClickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = id;
    }

    if(g_ui_slider_active_id == id && g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
       g_ui_pointer_dragging) {
        if(ui_pointer_drag_is_horizontal())
            g_ui_pointer_owner = UI_POINTER_OWNER_HORIZONTAL_SLIDER;
        else
            g_ui_slider_active_id = 0;
    }

    if(g_ui_slider_active_id == id &&
       ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
         g_ui_pointer_owner == UI_POINTER_OWNER_HORIZONTAL_SLIDER) ||
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
        int old_value = *value;
        float t = (float)(mx - x) / (float)w;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
    }

    float t = (float)(*value - min) / (float)(max - min);
    int knob_x = x + (int)(t * (float)w) - knob_w / 2;
    DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
    DrawUIBevel(knob_x, knob_y, knob_w, knob_h, LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));

    return changed;
}

int
DrawUIVerticalSlider(int id, int x, int y, int h,
                        int min, int max, int *value)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)(x - ScaleUIPx(18)), (float)y,
                               (float)ScaleUIPx(36), (float)h};
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    int track_w = ScaleUIPx(8);
    int knob_w = ScaleUIPx(20);
    int knob_h = ScaleUIPx(12);
    int track_x = x - track_w / 2;
    int min_touch_w = ScaleUIPx(36);
    int changed = 0;
    Rectangle hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h, min_touch_w, h);

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "vertical_slider", id, NULL), &editor_bounds);
    x = (int)(editor_bounds.x + editor_bounds.width * 0.5f);
    y = (int)editor_bounds.y;
    h = (int)editor_bounds.height;
    if(h < ScaleUIPx(32))
        h = ScaleUIPx(32);
    track_x = x - track_w / 2;
    hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h, min_touch_w, h);
    editor_bounds = (Rectangle){(float)(x - ScaleUIPx(18)), (float)y,
                                (float)ScaleUIPx(36), (float)h};
    UIEditorRegisterWidget(editor_id, "vertical_slider", &editor_bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    /* Draw track */
    DrawRectangle(track_x, y, track_w, h, DarkenUIColor(c_bg, 28));
    DrawUIBevel(track_x, y, track_w, h, DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));

    /* Check for interaction */
    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
        MarkUIClickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_ui_slider_active_id = id;
            g_ui_pointer_owner = UI_POINTER_OWNER_VERTICAL_SLIDER;
        }
    }

    /* Handle drag */
    if(g_ui_slider_active_id == id &&
       (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
        int old_value = *value;
        /* Invert Y so 0% is at bottom, 100% at top */
        float t = 1.0f - (float)(my - y) / (float)h;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
    }

    float t = (float)(*value - min) / (float)(max - min);
    int position_y = y + h - (int)(t * (float)h);  /* Position on track */
    int knob_y;
    int knob_x = track_x - (knob_w - track_w) / 2;

    if(position_y < y)
        position_y = y;
    if(position_y > y + h)
        position_y = y + h;
    knob_y = position_y - knob_h / 2;
    if(knob_y < y)
        knob_y = y;
    if(knob_y + knob_h > y + h)
        knob_y = y + h - knob_h;

    DrawRectangle(track_x, position_y, track_w, y + h - position_y, c_button_hover);
    DrawUIBevel(track_x, position_y, track_w, y + h - position_y,
                  LightenUIColor(c_button_hover, 35), DarkenUIColor(c_button_hover, 35));

    /* Draw knob */
    DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
    DrawUIBevel(knob_x, knob_y, knob_w, knob_h, LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));

    return changed;
}

int
DrawUIVerticalSliderWithMarks(int id, int x, int y, int h,
                                   int min, int max, int *value, UIVerticalSliderMarkCallback callback,
                                   void *callback_user_data)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)(x - ScaleUIPx(18)), (float)y,
                               (float)ScaleUIPx(36), (float)h};
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    int track_w = ScaleUIPx(8);
    int knob_w = ScaleUIPx(20);
    int knob_h = ScaleUIPx(12);
    int track_x = x - track_w / 2;
    int min_touch_w = ScaleUIPx(36);
    int changed = 0;
    Rectangle hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h, min_touch_w, h);

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "vertical_slider_marks", id, NULL), &editor_bounds);
    x = (int)(editor_bounds.x + editor_bounds.width * 0.5f);
    y = (int)editor_bounds.y;
    h = (int)editor_bounds.height;
    if(h < ScaleUIPx(32))
        h = ScaleUIPx(32);
    track_x = x - track_w / 2;
    hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h, min_touch_w, h);
    editor_bounds = (Rectangle){(float)(x - ScaleUIPx(18)), (float)y,
                                (float)ScaleUIPx(36), (float)h};
    UIEditorRegisterWidget(editor_id, "vertical_slider_marks", &editor_bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    /* Draw track */
    DrawRectangle(track_x, y, track_w, h, DarkenUIColor(c_bg, 28));
    DrawUIBevel(track_x, y, track_w, h, DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));

    /* Draw custom marks via callback (between track and knob) */
    if(callback != NULL)
        callback(callback_user_data, x, y, h, min, max, *value);

    /* Check for interaction */
    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
        MarkUIClickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_ui_slider_active_id = id;
            g_ui_pointer_owner = UI_POINTER_OWNER_VERTICAL_SLIDER;
        }
    }

    /* Handle drag */
    if(g_ui_slider_active_id == id &&
       (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
        int old_value = *value;
        /* Invert Y so 0% is at bottom, 100% at top */
        float t = 1.0f - (float)(my - y) / (float)h;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
    }

    float t = (float)(*value - min) / (float)(max - min);
    int position_y = y + h - (int)(t * (float)h);  /* Position on track */
    int knob_y;
    int knob_x = track_x - (knob_w - track_w) / 2;

    if(position_y < y)
        position_y = y;
    if(position_y > y + h)
        position_y = y + h;
    knob_y = position_y - knob_h / 2;
    if(knob_y < y)
        knob_y = y;
    if(knob_y + knob_h > y + h)
        knob_y = y + h - knob_h;

    DrawRectangle(track_x, position_y, track_w, y + h - position_y, c_button_hover);
    DrawUIBevel(track_x, position_y, track_w, y + h - position_y,
                  LightenUIColor(c_button_hover, 35), DarkenUIColor(c_button_hover, 35));

    /* Draw knob */
    DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
    DrawUIBevel(knob_x, knob_y, knob_w, knob_h, LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));

    return changed;
}

int
DrawUIToggleSwitch(int x, int y, int w, int h, int *value,
                     const char *off_label, const char *on_label)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)x, (float)y, (float)w, (float)h};
    Vector2 mouse_world = ui_mouse_world();
    int min_touch = ScaleUIPx(36);
    int font = GetUIFontSize();
    int off_w = MeasureUIText(off_label, font);
    int on_w = MeasureUIText(on_label, font);
    int min_half_w = (off_w > on_w ? off_w : on_w) + ScaleUIPx(16);
    int min_w = min_half_w * 2 + ScaleUIPx(6);
    if(w < min_w)
        w = min_w;
    if(h < ScaleUIPx(34))
        h = ScaleUIPx(34);

    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "toggle", 0, off_label), &editor_bounds);
    x = (int)editor_bounds.x;
    y = (int)editor_bounds.y;
    w = (int)editor_bounds.width;
    h = (int)editor_bounds.height;
    if(w < min_w)
        w = min_w;
    if(h < ScaleUIPx(34))
        h = ScaleUIPx(34);
    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    UIEditorRegisterWidget(editor_id, "toggle", &editor_bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    Rectangle bounds = ui_centered_min_hit_rect(x, y, w, h, min_touch, min_touch);

    if(CheckCollisionPointRec(mouse_world, bounds) && !UIInputCapturesClick(mouse_world)) {
        MarkUIClickable();
    }

    int pressed = CheckCollisionPointRec(mouse_world, bounds) &&
                  !UIInputCapturesClick(mouse_world) &&
                  IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    if(pressed) {
        *value = !(*value);
        UIConsumeRelease();
    }

    Color bg = DarkenUIColor(c_bg, 8);
    DrawRectangle(x, y, w, h, bg);

    int track_h = h - 6;
    int track_y = y + 3;
    DrawRectangleRounded((Rectangle){x + 3, track_y, w - 6, track_h}, 0.5f, 8, DarkenUIColor(c_bg, 20));

    int active_w = (w - 6) / 2;
    int active_x = *value ? x + w - active_w - 3 : x + 3;
    DrawRectangleRounded((Rectangle){active_x, track_y, active_w, track_h}, 0.5f, 8, c_button);

    Color label_color = c_text;
    /* Center text in each half of the toggle */
    int off_x = x + w / 4 - off_w / 2;
    int on_x = x + w * 3 / 4 - on_w / 2;
    DrawUIText(off_label, off_x, GetUIControlTextY(off_label, y, h, font), font, label_color);
    DrawUIText(on_label, on_x, GetUIControlTextY(on_label, y, h, font), font, label_color);

    return pressed;
}

int
DrawDisabledUICheckboxToggle(int x, int y, const char *label,
                                int *value, int disabled)
{
    char editor_id[96];
    int font = GetUIFontSize();
    int box_size = ScaleUIPx(22);
    int label_gap = ScaleUIPx(10);
    int label_w = MeasureUIText(label, font);
    int label_h = GetUITextLineHeight(font);
    int row_h = box_size > label_h ? box_size : label_h;
    Rectangle bounds = {x, y, box_size + label_gap + label_w, row_h};
    Vector2 mouse_world = ui_mouse_world();
    Color box_color = disabled ? DarkenUIColor(c_button, 18) : c_button;
    Color mark_color = disabled ? DarkenUIColor(c_text, 35) : c_text;
    Color label_color = disabled ? DarkenUIColor(c_text, 35) : c_text;

    UIEditorApplyBounds(ui_editor_control_id(editor_id, sizeof(editor_id),
                        "checkbox", 0, label), &bounds);
    x = (int)bounds.x;
    y = (int)bounds.y;
    UIEditorRegisterWidget(editor_id, "checkbox", &bounds,
                           UI_EDITOR_WIDGET_MOVABLE |
                           UI_EDITOR_WIDGET_RESIZABLE);

    if(CheckCollisionPointRec(mouse_world, bounds) && !UIInputCapturesClick(mouse_world)) {
        if(disabled)
            MarkUIDisabled();
        else {
            MarkUIClickable();
        }
    }

    int pressed = CheckCollisionPointRec(mouse_world, bounds) && !disabled &&
                  !UIInputCapturesClick(mouse_world) &&
                  IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if(pressed) {
        *value = !(*value);
        UIConsumeRelease();
    }

    DrawRectangle(x, y + (row_h - box_size) / 2, box_size, box_size, box_color);
    DrawUIBevel(x, y + (row_h - box_size) / 2, box_size, box_size,
                  DarkenUIColor(c_bg, 30), LightenUIColor(c_bg, 20));

    if(*value) {
        int padding = ScaleUIPx(4);
        int box_y = y + (row_h - box_size) / 2;
        DrawLine(x + padding, box_y + padding, x + box_size / 2,
                 box_y + box_size - padding, mark_color);
        DrawLine(x + box_size / 2, box_y + box_size - padding,
                 x + box_size - padding, box_y + padding, mark_color);
    }

    DrawUIText(label, x + box_size + label_gap,
                    GetUIControlTextY(label, y, row_h, font),
                    font, label_color);

    return pressed;
}

int
DrawUICheckboxToggle(int x, int y, const char *label, int *value)
{
    return DrawDisabledUICheckboxToggle(x, y, label, value, 0);
}

int
DrawUIInfoButton(int center_x, int center_y, int diameter)
{
    Vector2 mouse_world = ui_mouse_world();
    int min_touch = ScaleUIPx(32);
    int radius;
    int active = 0;
    int hover = 0;
    Rectangle hit;
    Color fill;
    Color stroke;
    Color text;
    int font;

    if(diameter <= 0)
        diameter = ScaleUIPx(18);
    radius = diameter / 2;
    hit = ui_centered_min_hit_rect(center_x - radius, center_y - radius,
                                  diameter, diameter, min_touch, min_touch);

    active = CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world);
    if(active) {
        hover = UIHoverEffectsEnabled();
        MarkUIClickable();
    }

    fill = hover ? c_button_hover : DarkenUIColor(c_bg, 8);
    stroke = c_text;
    text = c_text;
    DrawCircle(center_x, center_y, radius, fill);
    DrawCircleLines(center_x, center_y, radius, stroke);
    font = GetUISmallFontSize();
    DrawCenteredUIControlText("i", center_x, center_y, font, text);

    if(active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        UIConsumeRelease();
        return 1;
    }
    return 0;
}
