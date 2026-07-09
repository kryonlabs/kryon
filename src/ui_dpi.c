#include "ui_dpi.h"

#if defined(__FreeBSD__) && !defined(PLATFORM_WEB)
#define GL_FRAMEBUFFER_SRGB 0x8DB9
extern void glDisable(unsigned int cap);
#endif

UIDPIState ui_dpi_state;

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
UpdateUIDPI(int view_width, int view_height)
{
    int previous_width = ui_dpi_state.view_width;
    int previous_height = ui_dpi_state.view_height;

    if(previous_width != view_width || previous_height != view_height) {
        ui_dpi_state.view_width = view_width;
        ui_dpi_state.view_height = view_height;
        ui_dpi_state.ui_scale = (float)view_height / (float)ui_dpi_state.base_height;
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
