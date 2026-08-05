#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "ui.h"
#include "ui_clip.h"
#include "ui_dpi.h"
#include "kryon.h"
#include "ui_text_layout.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

extern Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon, c_link;
extern Camera2D g_ui_camera;
extern Texture2D g_ui_gear_icon;
extern Texture2D g_ui_x_icon;
extern int g_ui_slider_active_id;
extern int g_ui_pointer_dragging;
extern int g_ui_pointer_owner;

enum {
    UI_POINTER_OWNER_NONE = 0,
    UI_POINTER_OWNER_SCROLL,
    UI_POINTER_OWNER_HORIZONTAL_SLIDER,
    UI_POINTER_OWNER_VERTICAL_SLIDER,
    UI_POINTER_OWNER_REORDER
};

Vector2 ui_mouse_world(void);
void MarkUICursor(int cursor);
void MarkUIClickable(void);
void MarkUIDisabled(void);
int ui_pointer_drag_is_horizontal(void);
int UIHoverEffectsEnabled(void);
int UIReleaseConsumed(void);
void UIConsumeRelease(void);
int UIPointerReleaseConsumed(void);
void UIConsumePointerRelease(void);
int UIPointerReleaseAvailable(Vector2 point);
int UIPointerReleaseOutside(Rectangle bounds);
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
int ui_input_captures_click_internal(Vector2 point, int include_pointer_drag);
void PushUIInputClip(Rectangle bounds);
void PopUIInputClip(void);
int ui_clampi(int value, int min_value, int max_value);
int ui_retro_style(void);
int ui_modern_style(void);
float ui_control_radius(float classic_radius);
int ui_control_bevel_enabled(void);
int ui_touch_target_min(void);
Color ui_alpha(Color color, unsigned char alpha);
void ui_draw_control_background(Rectangle bounds, Color background,
                                Color border, float classic_radius);

/* Draws a filled box with an outline: rounded when radius > 0, otherwise a
 * plain rectangle with a 1px line border. Used by text input, text area, and
 * read-only box backgrounds. */
void ui_draw_box_background(Rectangle bounds, float radius, Color background,
                            Color border);
/* Blink phase of the text caret: on roughly every other half-second. */
int ui_caret_blink_visible(void);
/* Navigate to a URL: in-browser redirect on web, platform opener otherwise.
 * A no-op for a NULL/empty url. */
void ui_open_url(const char *url);

/* UTF-8 codec and text-buffer helpers (implemented in ui_text_edit.c). */
int ui_utf8_next_offset(const char *text, int offset);
int ui_utf8_prev_offset(const char *text, int offset);
int ui_utf8_codepoint_count(const char *text);
int ui_utf8_encode(int codepoint, char out[5]);
int ui_text_delete_range(char *text, size_t text_size, int *cursor,
                         int start, int end);
int ui_text_insert_ascii(char *text, size_t text_size, int *cursor, char ch,
                         int max_codepoints);
int ui_text_insert_codepoint(char *text, size_t text_size, int *cursor,
                             int codepoint, int max_codepoints);
int ui_text_insert_text(char *text, size_t text_size, int *cursor,
                        const char *input, int allow_newlines,
                        UITextInputFilter filter, void *filter_user_data,
                        int max_codepoints);

#endif
