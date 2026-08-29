#include "kryon.h"

static int frame_width;
static int frame_height;
static float frame_scale = 1.0f;

void SyncFrame(void)
{
    int width = GetScreenWidth();
    int height = GetScreenHeight();

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    SyncAndroidSurfaceSize(&width, &height);
#endif
#if defined(PLATFORM_WEB)
    SyncWebWindowSize();
    width = GetScreenWidth();
    height = GetScreenHeight();
#endif

    if(width <= 0)
        width = GetScreenWidth();
    if(height <= 0)
        height = GetScreenHeight();

    UpdateUIDPI(width, height);
    frame_width = width;
    frame_height = height;
    frame_scale = ui_dpi_state.ui_scale_clamped;
    if(!(frame_scale > 0.0f))
        frame_scale = GetUIScale();
    if(!(frame_scale > 0.0f))
        frame_scale = 1.0f;
}

void BeginFrame(void)
{
    SyncFrame();
    BeginDrawing();
    SyncFrame();
}

void EndFrame(void)
{
    EndDrawing();
}

int GetFrameWidth(void)
{
    return frame_width > 0 ? frame_width : GetScreenWidth();
}

int GetFrameHeight(void)
{
    return frame_height > 0 ? frame_height : GetScreenHeight();
}

float GetFrameScale(void)
{
    return frame_scale > 0.0f ? frame_scale : GetUIScale();
}
