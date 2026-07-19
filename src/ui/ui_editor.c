#include "ui_editor.h"

#if defined(__GNUC__) || defined(__clang__)
#define FLINT_EDITOR_WEAK __attribute__((weak))
#else
#define FLINT_EDITOR_WEAK
#endif

FLINT_EDITOR_WEAK
void
BeginUIEditorFrame(const char *project_root)
{
    (void)project_root;
}

FLINT_EDITOR_WEAK
void
EndUIEditorFrame(void)
{
}

FLINT_EDITOR_WEAK
void
SetUIEditorEnabled(int enabled)
{
    (void)enabled;
}

FLINT_EDITOR_WEAK
int
UIEditorEnabled(void)
{
    return 0;
}

FLINT_EDITOR_WEAK
int
UIEditorWidgetCount(void)
{
    return 0;
}

FLINT_EDITOR_WEAK
UIEditorSelection
UIEditorGetSelection(void)
{
    UIEditorSelection selection = {0};
    return selection;
}

FLINT_EDITOR_WEAK
void
SetUIEditorCanvasBounds(Rectangle bounds)
{
    (void)bounds;
}

FLINT_EDITOR_WEAK
int
PushUIEditorChrome(int enabled)
{
    (void)enabled;
    return 0;
}

FLINT_EDITOR_WEAK
void
PopUIEditorChrome(int token)
{
    (void)token;
}

FLINT_EDITOR_WEAK
int
UIEditorInputCapturesClick(Vector2 point)
{
    (void)point;
    return 0;
}

FLINT_EDITOR_WEAK
void
UIEditorApplyBounds(const char *id, Rectangle *bounds)
{
    (void)id;
    (void)bounds;
}

FLINT_EDITOR_WEAK
void
UIEditorRegisterWidget(const char *id, const char *kind,
                       Rectangle *bounds, int flags)
{
    (void)id;
    (void)kind;
    (void)bounds;
    (void)flags;
}

FLINT_EDITOR_WEAK
void
DrawUIEditorOverlay(void)
{
}
