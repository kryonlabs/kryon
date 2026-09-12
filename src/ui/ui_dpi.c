#include "ui_dpi.h"

#if defined(__FreeBSD__) && !defined(PLATFORM_WEB) && !defined(PLATFORM_ANDROID)
#define GL_FRAMEBUFFER_SRGB 0x8DB9
extern void glDisable(unsigned int cap);
#endif

DPIState dpi_state;
static float g_device_density = 0.0f;

void
InitDPI(void)
{
    FixDPIFramebufferColor();
    dpi_state.physical_width = DPI_BASE_WIDTH;
    dpi_state.physical_height = DPI_BASE_HEIGHT;
    dpi_state.view_width = DPI_BASE_WIDTH;
    dpi_state.view_height = DPI_BASE_HEIGHT;
    dpi_state.layout_width = DPI_BASE_WIDTH;
    dpi_state.layout_height = DPI_BASE_HEIGHT;
    dpi_state.ui_scale = 1.0f;
    dpi_state.ui_scale_clamped = 1.0f;
    dpi_state.render_scale = 1.0f;
    dpi_state.camera_zoom = 1.0f;
    dpi_state.base_width = DPI_BASE_WIDTH;
    dpi_state.base_height = DPI_BASE_HEIGHT;
    dpi_state.needs_update = 0;
}

void
FixDPIFramebufferColor(void)
{
#if defined(__FreeBSD__) && !defined(PLATFORM_WEB) && !defined(PLATFORM_ANDROID)
    if(IsWindowReady()) {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
#endif
}

void
InvalidateDPI(void)
{
    dpi_state.physical_width = -1;
    dpi_state.physical_height = -1;
    dpi_state.view_width = -1;
    dpi_state.view_height = -1;
    dpi_state.layout_width = -1;
    dpi_state.layout_height = -1;
    dpi_state.needs_update = 1;
}

void
SetDeviceDensity(float density)
{
    if(density > 0.0f) {
        g_device_density = density;
        /* Force a recompute on the next UpdateDPI call so the new density
         * actually takes effect, even when the viewport size hasnt changed. */
        dpi_state.physical_width = -1;
        dpi_state.physical_height = -1;
        dpi_state.view_width = -1;
        dpi_state.view_height = -1;
    }
}

void
UpdateDPI(int view_width, int view_height)
{
    int previous_width = dpi_state.physical_width;
    int previous_height = dpi_state.physical_height;
    int base_height = dpi_state.base_height;

    if(base_height <= 0)
        InitDPI();
    base_height = dpi_state.base_height > 0 ? dpi_state.base_height : DPI_BASE_HEIGHT;

    if(previous_width != view_width || previous_height != view_height) {
        int layout_width;
        int layout_height;

        dpi_state.physical_width = view_width;
        dpi_state.physical_height = view_height;

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

        dpi_state.ui_scale = real_dpi;
        if(!(dpi_state.ui_scale > 0.0f) || dpi_state.ui_scale > 8.0f)
            dpi_state.ui_scale = 1.0f;
        dpi_state.ui_scale_clamped = (dpi_state.ui_scale < 1.0f) ? 1.0f : dpi_state.ui_scale;
        dpi_state.render_scale = dpi_state.ui_scale_clamped;
        layout_width = view_width;
        layout_height = view_height;
        if(dpi_state.render_scale > 1.0f) {
            layout_width = (int)((float)view_width / dpi_state.render_scale + 0.5f);
            layout_height = (int)((float)view_height / dpi_state.render_scale + 0.5f);
        }
        if(layout_width < 1)
            layout_width = 1;
        if(layout_height < 1)
            layout_height = 1;
        dpi_state.layout_width = layout_width;
        dpi_state.layout_height = layout_height;
        dpi_state.view_width = layout_width;
        dpi_state.view_height = layout_height;
        dpi_state.camera_zoom = dpi_state.render_scale;
        dpi_state.needs_update = 1;
    } else {
        dpi_state.needs_update = 0;
    }
}

int
IsDPIDirty(void)
{
    return dpi_state.needs_update;
}

int
GetLayoutWidth(void)
{
    return dpi_state.layout_width > 0
        ? dpi_state.layout_width : dpi_state.view_width;
}

int
GetLayoutHeight(void)
{
    return dpi_state.layout_height > 0
        ? dpi_state.layout_height : dpi_state.view_height;
}

float
GetRenderScale(void)
{
    return dpi_state.render_scale > 0.0f
        ? dpi_state.render_scale : dpi_state.ui_scale_clamped;
}
