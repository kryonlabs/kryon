#include "ui_editor.h"

void
BeginUIEditorFrame(const char *project_root)
{
    (void)project_root;
}

void
EndUIEditorFrame(void)
{
}

void
SetUIEditorEnabled(int enabled)
{
    (void)enabled;
}

int
UIEditorEnabled(void)
{
    return 0;
}

int
UIEditorWidgetCount(void)
{
    return 0;
}

UIEditorSelection
UIEditorGetSelection(void)
{
    UIEditorSelection selection = {0};
    return selection;
}

void
SetUIEditorCanvasBounds(Rectangle bounds)
{
    (void)bounds;
}

int
PushUIEditorChrome(int enabled)
{
    (void)enabled;
    return 0;
}

void
PopUIEditorChrome(int token)
{
    (void)token;
}

int
UIEditorInputCapturesClick(Vector2 point)
{
    (void)point;
    return 0;
}

void
UIEditorApplyBounds(const char *id, Rectangle *bounds)
{
    (void)id;
    (void)bounds;
}

void
UIEditorRegisterWidget(const char *id, const char *kind,
                       Rectangle *bounds, int flags)
{
    (void)id;
    (void)kind;
    (void)bounds;
    (void)flags;
}

void
DrawUIEditorOverlay(void)
{
}
