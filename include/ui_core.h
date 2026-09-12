#ifndef KRYON_CORE_H
#define KRYON_CORE_H

#include "kryon_compat.generated.h"
#include "ui_input_props.generated.h"

typedef void (*TextInputPlatformCallback)(int active);

typedef struct FrameState {
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
    int text_input_show_requested;
    int mouse_world_override_enabled;
    Vector2 mouse_world_override;
    unsigned long frame_serial;
    int auto_focus_id;
    float ui_scale;
} FrameState;

void InitInterface(int width, int height, float dpi);
void SetDefaultFontAutoLoad(int enabled);
void SetLinkColor(Color link);
void ApplyCurrentTheme(void);
int IsDesktopMode(void);
Camera2D GetDefaultCamera(void);
void BeginInterfaceFrame(int width, int height, float dpi);
void EndInterfaceFrame(void);
/* Zero means "automatic": stable for a widget's call order within a frame. */
int ResolveFocusID(int id);
void SetFrameCamera(Camera2D camera);
FrameState SaveFrameState(void);
void RestoreFrameState(FrameState state);
void SetMouseWorldOverride(int enabled, Vector2 position);
int SetKeyboardInputEnabled(int enabled);
int IsKeyboardInputEnabled(void);

void ClearInputCaptures(void);
void PushInputCapture(Rectangle bounds, int allow_inside);
void BeginModalLayer(void);
void PushInputClip(Rectangle bounds);
void PopInputClip(void);
void SetModalCapture(Rectangle bounds);

void SetTextInputPlatformCallback(TextInputPlatformCallback callback);
void SetCursorClickable(int *cursor_clickable);
void SetCursorDisabled(int *cursor_disabled);
void MarkCursor(int cursor);
void MarkClickable(void);
void MarkDisabled(void);
/* The cursor the intents marked so far this frame (test/diagnostic aid). */
int GetMouseCursorIntent(void);
void SetInterfaceIcons(Texture2D gear_icon, Texture2D x_icon);

int HandleClick(Rectangle bounds, int disabled, int *hover);
Activation ReadActivation(Rectangle bounds, int id, bool enabled);
int HandleCircleClick(Vector2 center, float radius, int disabled, int *hover);
int InputCapturesClick(Vector2 point);
int ReleaseConsumed(void);
void ConsumeRelease(void);
int PointerReleaseConsumed(void);
void ConsumePointerRelease(void);
int PointerReleaseAvailable(Vector2 point);
int PointerReleaseOutside(Rectangle bounds);
int HoverEffectsEnabled(void);
void SetTransitionCuesEnabled(int enabled);
int TransitionCuesEnabled(void);

void BeginFocusScope(void);
void EndFocusScope(void);
int FocusFrameOpen(void);
int RegisterFocus(int id, Rectangle bounds);
int IsFocusActive(int id);
int IsFocusActivatePressed(int id);
void SetFocus(int id);
int GetFocus(void);
void ClearFocus(void);
void SetFocusTextInputActive(int active);
int TextInputActive(void);

extern int ui_view_height;
extern int ui_view_width;

#endif
