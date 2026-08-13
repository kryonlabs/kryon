#ifndef UI_CORE_H
#define UI_CORE_H

#include "kryon_compat.generated.h"

typedef void (*UITextInputPlatformCallback)(int active);

typedef struct UIFrameState {
    int view_width;
    int view_height;
    Camera2D camera;
    int input_clip_count;
    Rectangle input_clips[16];
    int input_capture_count;
    struct {
        Rectangle bounds;
        int allow_inside;
    } input_captures[16];
    int cursor_priority;
    int cursor_had_intent;
    int pointer_down;
    int pointer_dragging;
    int pointer_dragged_this_click;
    int pointer_start_x;
    int pointer_start_y;
    int pointer_owner;
    int release_consumed;
    int focus_active_id;
    int focus_ids[256];
    int focus_count;
    int focus_tab_dir;
    int focus_frame_open;
    int focus_text_input_active;
    int text_input_requested;
    int mouse_world_override_enabled;
    Vector2 mouse_world_override;
    unsigned long frame_serial;
    float ui_scale;
} UIFrameState;

void InitUI(int width, int height, float dpi);
void SetUIDefaultFontAutoLoad(int enabled);
void SetUILinkColor(Color link);
void ApplyCurrentUITheme(void);
int IsUIDesktopMode(void);
Camera2D GetUIDefaultCamera(void);
void BeginUIFrame(int width, int height, float dpi);
void SetUIFrame(Camera2D camera);
UIFrameState SaveUIFrameState(void);
void RestoreUIFrameState(UIFrameState state);
void SetUIMouseWorldOverride(int enabled, Vector2 position);
int SetUIKeyboardInputEnabled(int enabled);
int UIKeyboardInputEnabled(void);

void ClearUIInputCaptures(void);
void PushUIInputCapture(Rectangle bounds, int allow_inside);
void BeginUIModalLayer(void);
void PushUIInputClip(Rectangle bounds);
void PopUIInputClip(void);
void SetUIModalCapture(Rectangle bounds);

void SetUITextInputPlatformCallback(UITextInputPlatformCallback callback);
void SetUICursorClickable(int *cursor_clickable);
void SetUICursorDisabled(int *cursor_disabled);
void MarkUICursor(int cursor);
void MarkUIClickable(void);
void MarkUIDisabled(void);
void SetUIIcons(Texture2D gear_icon, Texture2D x_icon);

int UIHandleClick(Rectangle bounds, int disabled, int *hover);
int UIInputCapturesClick(Vector2 point);
int UIReleaseConsumed(void);
void UIConsumeRelease(void);
int UIPointerReleaseConsumed(void);
void UIConsumePointerRelease(void);
int UIPointerReleaseAvailable(Vector2 point);
int UIPointerReleaseOutside(Rectangle bounds);
int UIHoverEffectsEnabled(void);
void SetUITransitionCuesEnabled(int enabled);
int UITransitionCuesEnabled(void);

void BeginUIFocus(void);
void EndUIFocus(void);
int RegisterUIFocus(int id, Rectangle bounds);
int IsUIFocusActive(int id);
int IsUIFocusActivatePressed(int id);
void SetUIFocus(int id);
void ClearUIFocus(void);
void SetUIFocusTextInputActive(int active);

extern int ui_view_height;
extern int ui_view_width;

#endif
