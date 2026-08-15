#include "ui_dpi.h"

#if defined(__FreeBSD__) && !defined(PLATFORM_WEB)
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
#if defined(__FreeBSD__) && !defined(PLATFORM_WEB)
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
         * actually takes effect, even when the viewport size hasn't changed. */
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
        float real_dpi = viewport_scale;
        if(g_device_density > 0.0f && g_device_density > real_dpi)
            real_dpi = g_device_density;

#if !defined(PLATFORM_WEB)
        /* Native windows are sized in physical pixels, so the monitor scale
         * factor is a legitimate additional scaling input. The web is
         * different: the viewport IS CSS pixels (density-independent by
         * definition) and the canvas backing store already absorbs
         * devicePixelRatio — raylib's web GetWindowScaleDPI() returns exactly
         * that ratio, and taking it here scaled phone UIs by dpr (~3x) on
         * top of an already-correct viewport ratio. */
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
