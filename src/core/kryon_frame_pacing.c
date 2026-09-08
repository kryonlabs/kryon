#include "kryon_frame.h"
#include "kryon_compat.generated.h"

static int frame_pacing_enabled;
static int frame_pacing_idle_fps;
static int frame_pacing_active_fps;
static int frame_pacing_active;
static int frame_pacing_target_fps;

static int
frame_pacing_sanitize_fps(int fps)
{
    return fps > 0 ? fps : 0;
}

void
ConfigureFramePacing(int idle_fps, int active_fps)
{
    frame_pacing_idle_fps = frame_pacing_sanitize_fps(idle_fps);
    frame_pacing_active_fps = frame_pacing_sanitize_fps(active_fps);
    frame_pacing_enabled = frame_pacing_idle_fps > 0 ||
                           frame_pacing_active_fps > 0;
    frame_pacing_target_fps = 0;
    UpdateFramePacing();
}

void
DisableFramePacing(void)
{
    frame_pacing_enabled = 0;
    frame_pacing_active = 0;
    frame_pacing_idle_fps = 0;
    frame_pacing_active_fps = 0;
    if(frame_pacing_target_fps != 0) {
        SetTargetFPS(0);
        frame_pacing_target_fps = 0;
    }
}

void
SetFramePacingActive(int active)
{
    frame_pacing_active = active != 0;
}

void
UpdateFramePacing(void)
{
    int target_fps;

    if(!frame_pacing_enabled)
        return;

    target_fps = frame_pacing_active ? frame_pacing_active_fps
                                     : frame_pacing_idle_fps;
    if(target_fps <= 0)
        target_fps = frame_pacing_active ? frame_pacing_idle_fps
                                         : frame_pacing_active_fps;

    if(frame_pacing_target_fps != target_fps) {
        SetTargetFPS(target_fps);
        frame_pacing_target_fps = target_fps;
    }
}

int
GetFramePacingTargetFPS(void)
{
    return frame_pacing_target_fps;
}
