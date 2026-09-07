#include "ui_dpi.h"

#if defined(__FreeBSD__) && !defined(PLATFORM_WEB) && !defined(PLATFORM_ANDROID)
#define GL_FRAMEBUFFER_SRGB 0x8DB9
extern void glDisable(unsigned int cap);
#endif

UIDPIState ui_dpi_state;
static float g_device_density = 0.0f;

void
InitUIDPI(void)
{
    FixUIDPIFramebufferColor();
    ui_dpi_state.view_width = UI_DPI_BASE_WIDTH;
    ui_dpi_state.view_height = UI_DPI_BASE_HEIGHT;
    ui_dpi_state.ui_scale = 1.0f;
    ui_dpi_state.ui_scale_clamped = 1.0f;
    ui_dpi_state.camera_zoom = 1.0f;
    ui_dpi_state.base_width = UI_DPI_BASE_WIDTH;
    ui_dpi_state.base_height = UI_DPI_BASE_HEIGHT;
    ui_dpi_state.needs_update = 0;
}

void
FixUIDPIFramebufferColor(void)
{
#if defined(__FreeBSD__) && !defined(PLATFORM_WEB) && !defined(PLATFORM_ANDROID)
    if(IsWindowReady()) {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
#endif
}

void
InvalidateUIDPI(void)
{
    ui_dpi_state.view_width = -1;
    ui_dpi_state.view_height = -1;
    ui_dpi_state.needs_update = 1;
}

void
SetUIDeviceDensity(float density)
{
    if(density > 0.0f) {
        g_device_density = density;
        /* Force a recompute on the next UpdateUIDPI call so the new density
         * actually takes effect, even when the viewport size hasnt changed. */
        ui_dpi_state.view_width = -1;
        ui_dpi_state.view_height = -1;
    }
}

void
UpdateUIDPI(int view_width, int view_height)
{
    int previous_width = ui_dpi_state.view_width;
    int previous_height = ui_dpi_state.view_height;
    int base_height = ui_dpi_state.base_height;

    if(base_height <= 0)
        InitUIDPI();
    base_height = ui_dpi_state.base_height > 0 ? ui_dpi_state.base_height : UI_DPI_BASE_HEIGHT;

    if(previous_width != view_width || previous_height != view_height) {
        ui_dpi_state.view_width = view_width;
        ui_dpi_state.view_height = view_height;

        float viewport_scale = view_height > 0
                                   ? (float)view_height / (float)base_height
                                   : 1.0f;
        float real_dpi = 1.0f;
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(PLATFORM_WEB)
        (void)viewport_scale;
#endif

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
        /* Android view dimensions are physical pixels. DisplayMetrics density
         * is the pixel-to-dp conversion; scaling again from a tall viewport
         * makes controls grow with aspect ratio and can push anchored chrome
         * off-screen. Keep the viewport fallback for early startup, before
         * the activity has delivered its density. */
        real_dpi = g_device_density > 0.0f ? g_device_density : viewport_scale;
#elif defined(PLATFORM_WEB)
        /* Web view dimensions are CSS pixels, so viewport scale remains a
         * useful density-independent fallback there. */
        real_dpi = viewport_scale;
#else
        if(g_device_density > real_dpi)
            real_dpi = g_device_density;
#endif

#if !defined(PLATFORM_WEB) && !defined(PLATFORM_ANDROID) && !defined(__ANDROID__)
        /* Native desktop window size is layout space, not density. A taller
         * window must show more content instead of making every token larger.
         * Use the monitor DPI only when the OS reports an actual scale. */
        Vector2 dpi_scale = GetWindowScaleDPI();
        float window_dpi = (dpi_scale.x > 1.0f) ? dpi_scale.x : dpi_scale.y;
        if(window_dpi > real_dpi)
            real_dpi = window_dpi;
#endif

        ui_dpi_state.ui_scale = real_dpi;
        if(!(ui_dpi_state.ui_scale > 0.0f) || ui_dpi_state.ui_scale > 8.0f)
            ui_dpi_state.ui_scale = 1.0f;
        ui_dpi_state.ui_scale_clamped = (ui_dpi_state.ui_scale < 1.0f) ? 1.0f : ui_dpi_state.ui_scale;
        ui_dpi_state.needs_update = 1;
    } else {
        ui_dpi_state.needs_update = 0;
    }
}

int
IsUIDPIDirty(void)
{
    return ui_dpi_state.needs_update;
}
