#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "ui.h"
#include "ui_clip.h"
#include "ui_dpi.h"
#include "flint.h"
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

#endif
