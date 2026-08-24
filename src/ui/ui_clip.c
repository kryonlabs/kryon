#include "ui_clip.h"
#include "android_surface.h"

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
#define KRY_GL_SCISSOR_TEST 0x0C11
void glDisable(unsigned int cap);
void glEnable(unsigned int cap);
void glScissor(int x, int y, int width, int height);
#endif

#define UI_CLIP_STACK_MAX 16

static Rectangle g_ui_clip_stack[UI_CLIP_STACK_MAX];
static int g_ui_clip_stack_count = 0;

static void
ui_apply_clip(Rectangle bounds)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    int surface_h = 0;

    GetAndroidSurfaceSize(NULL, &surface_h);
    if(surface_h > 0) {
        int x = (int)bounds.x;
        int y = surface_h - (int)bounds.y - (int)bounds.height;
        int w = (int)bounds.width;
        int h = (int)bounds.height;

        if(y < 0) {
            h += y;
            y = 0;
        }
        if(w < 0)
            w = 0;
        if(h < 0)
            h = 0;
        glEnable(KRY_GL_SCISSOR_TEST);
        glScissor(x, y, w, h);
        return;
    }
#endif
    BeginScissorMode((int)bounds.x, (int)bounds.y,
                     (int)bounds.width, (int)bounds.height);
}

static void
ui_clear_clip(void)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    int surface_h = 0;

    if(GetAndroidSurfaceSize(NULL, &surface_h) && surface_h > 0) {
        glDisable(KRY_GL_SCISSOR_TEST);
        return;
    }
#endif
    EndScissorMode();
}

Rectangle
GetUIClipIntersection(Rectangle a, Rectangle b)
{
    float x1 = a.x > b.x ? a.x : b.x;
    float y1 = a.y > b.y ? a.y : b.y;
    float x2 = a.x + a.width < b.x + b.width ? a.x + a.width : b.x + b.width;
    float y2 = a.y + a.height < b.y + b.height ? a.y + a.height : b.y + b.height;

    if(x2 < x1)
        x2 = x1;
    if(y2 < y1)
        y2 = y1;

    return (Rectangle){x1, y1, x2 - x1, y2 - y1};
}

Rectangle
GetUIClipEffective(Rectangle bounds)
{
    if(g_ui_clip_stack_count > 0)
        bounds = GetUIClipIntersection(g_ui_clip_stack[g_ui_clip_stack_count - 1],
                                         bounds);
    return bounds;
}

void
BeginUIClip(int x, int y, int w, int h)
{
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};

    if(w < 0)
        bounds.width = 0;
    if(h < 0)
        bounds.height = 0;

    bounds = GetUIClipEffective(bounds);
    if(g_ui_clip_stack_count < UI_CLIP_STACK_MAX)
        g_ui_clip_stack[g_ui_clip_stack_count++] = bounds;

    ui_apply_clip(bounds);
}

void
EndUIClip(void)
{
    ui_clear_clip();
    if(g_ui_clip_stack_count > 0)
        g_ui_clip_stack_count--;
    if(g_ui_clip_stack_count > 0) {
        Rectangle bounds = g_ui_clip_stack[g_ui_clip_stack_count - 1];
        ui_apply_clip(bounds);
    }
}

void
ResetUIClip(void)
{
    g_ui_clip_stack_count = 0;
}
