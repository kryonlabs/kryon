#include "kryon.h"

static int frame_width;
static int frame_height;
static float frame_scale = 1.0f;

typedef struct KryonPostFrameEntry {
    KryonPostFrameCallback callback;
    void *userdata;
} KryonPostFrameEntry;

enum {
    KRYON_POST_FRAME_MAX = 64
};

static KryonPostFrameEntry post_frame_entries[KRYON_POST_FRAME_MAX];
static int post_frame_count;

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

int SchedulePostFrameCallback(KryonPostFrameCallback callback, void *userdata)
{
    if(callback == 0 || post_frame_count >= KRYON_POST_FRAME_MAX)
        return 0;
    post_frame_entries[post_frame_count].callback = callback;
    post_frame_entries[post_frame_count].userdata = userdata;
    post_frame_count++;
    return 1;
}

void kryon_run_post_frame_callbacks(void)
{
    KryonPostFrameEntry entries[KRYON_POST_FRAME_MAX];
    int count = post_frame_count;
    int i;

    if(count <= 0)
        return;
    if(count > KRYON_POST_FRAME_MAX)
        count = KRYON_POST_FRAME_MAX;
    for(i = 0; i < count; i++)
        entries[i] = post_frame_entries[i];
    post_frame_count = 0;
    for(i = 0; i < count; i++) {
        if(entries[i].callback != 0)
            entries[i].callback(entries[i].userdata);
    }
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
