#include "flint_dpi.h"

#if defined(__FreeBSD__) && !defined(PLATFORM_WEB)
#define GL_FRAMEBUFFER_SRGB 0x8DB9
extern void glDisable(unsigned int cap);
#endif

FlintDpiState flint_dpi_state;

void
flint_dpi_init(void)
{
    flint_dpi_fix_framebuffer_color();
    flint_dpi_state.view_width = FLINT_DPI_BASE_WIDTH;
    flint_dpi_state.view_height = FLINT_DPI_BASE_HEIGHT;
    flint_dpi_state.ui_scale = 1.0f;
    flint_dpi_state.ui_scale_clamped = 1.0f;
    flint_dpi_state.camera_zoom = 1.0f;
    flint_dpi_state.base_width = FLINT_DPI_BASE_WIDTH;
    flint_dpi_state.base_height = FLINT_DPI_BASE_HEIGHT;
    flint_dpi_state.needs_update = 0;
}

void
flint_dpi_fix_framebuffer_color(void)
{
#if defined(__FreeBSD__) && !defined(PLATFORM_WEB)
    if(IsWindowReady()) {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
#endif
}

void
flint_dpi_invalidate(void)
{
    flint_dpi_state.view_width = -1;
    flint_dpi_state.view_height = -1;
    flint_dpi_state.needs_update = 1;
}

void
flint_dpi_update(int view_width, int view_height)
{
    int previous_width = flint_dpi_state.view_width;
    int previous_height = flint_dpi_state.view_height;

    if(previous_width != view_width || previous_height != view_height) {
        flint_dpi_state.view_width = view_width;
        flint_dpi_state.view_height = view_height;
        flint_dpi_state.ui_scale = (float)view_height / (float)flint_dpi_state.base_height;
        flint_dpi_state.ui_scale_clamped = (flint_dpi_state.ui_scale < 1.0f) ? 1.0f : flint_dpi_state.ui_scale;
        flint_dpi_state.needs_update = 1;
    } else {
        flint_dpi_state.needs_update = 0;
    }
}

int
flint_dpi_is_dirty(void)
{
    return flint_dpi_state.needs_update;
}
