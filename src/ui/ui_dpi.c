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

        float real_dpi = 1.0f;
        if(g_device_density > 0.0f) {
            real_dpi = g_device_density;
        } else {
            Vector2 dpi_scale = GetWindowScaleDPI();
            real_dpi = (dpi_scale.x > 1.0f) ? dpi_scale.x : dpi_scale.y;
            if(real_dpi <= 1.0f) {
                real_dpi = view_height > 0 ? (float)view_height / (float)base_height : 1.0f;
            }
        }

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
