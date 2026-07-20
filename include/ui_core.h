#ifndef UI_CORE_H
#define UI_CORE_H

#include "kryon_compat.generated.h"

typedef void (*UITextInputPlatformCallback)(int active);

void InitUI(int width, int height, float dpi);
void SetUILinkColor(Color link);
void ApplyCurrentUITheme(void);
int IsUIDesktopMode(void);
Camera2D GetUIDefaultCamera(void);
void BeginUIFrame(int width, int height, float dpi);
void SetUIFrame(Camera2D camera);

void ClearUIInputCaptures(void);
void PushUIInputCapture(Rectangle bounds, int allow_inside);
void BeginUIModalLayer(void);
void PushUIInputClip(Rectangle bounds);
void PopUIInputClip(void);
void SetUIModalCapture(Rectangle bounds);

void SetUITextInputPlatformCallback(UITextInputPlatformCallback callback);
void SetUICursorClickable(int *cursor_clickable);
void SetUICursorDisabled(int *cursor_disabled);
void MarkUIClickable(void);
void MarkUIDisabled(void);
void SetUIIcons(Texture2D gear_icon, Texture2D x_icon);

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
void DrawUIFocus(Rectangle bounds);

extern int ui_view_height;
extern int ui_view_width;

#endif
