#ifndef UI_EDITOR_H
#define UI_EDITOR_H

#include "flint_compat.generated.h"

enum {
    UI_EDITOR_WIDGET_MOVABLE = 1 << 0,
    UI_EDITOR_WIDGET_RESIZABLE = 1 << 1,
    UI_EDITOR_WIDGET_READONLY = 1 << 2,
    UI_EDITOR_WIDGET_TEMPORARY_ID = 1 << 3
};

typedef struct UIEditorSelection {
    char id[96];
    char kind[32];
    Rectangle bounds;
    int flags;
    int valid;
} UIEditorSelection;

void BeginUIEditorFrame(const char *project_root);
void EndUIEditorFrame(void);
void SetUIEditorEnabled(int enabled);
int UIEditorEnabled(void);
int UIEditorWidgetCount(void);
UIEditorSelection UIEditorGetSelection(void);
void SetUIEditorCanvasBounds(Rectangle bounds);
int PushUIEditorChrome(int enabled);
void PopUIEditorChrome(int token);
int UIEditorInputCapturesClick(Vector2 point);
void UIEditorApplyBounds(const char *id, Rectangle *bounds);
void UIEditorRegisterWidget(const char *id, const char *kind,
                            Rectangle *bounds, int flags);
void DrawUIEditorOverlay(void);

#endif
