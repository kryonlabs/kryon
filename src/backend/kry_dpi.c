#include "kry_dpi_internal.h"

#if defined(__FreeBSD__) && !defined(PLATFORM_WEB) && !defined(PLATFORM_ANDROID)
#define GL_FRAMEBUFFER_SRGB 0x8DB9
extern void glDisable(unsigned int cap);
#endif

int
kry_dpi_platform_kind(void)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    return 1;
#elif defined(PLATFORM_WEB)
    return 2;
#else
    return 0;
#endif
}

Vector2
kry_dpi_window_scale(void)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(PLATFORM_WEB)
    return (Vector2){1.0f, 1.0f};
#else
    return GetWindowScaleDPI();
#endif
}

void
FixDPIFramebufferColor(void)
{
#if defined(__FreeBSD__) && !defined(PLATFORM_WEB) && !defined(PLATFORM_ANDROID)
    if(IsWindowReady())
        glDisable(GL_FRAMEBUFFER_SRGB);
#endif
}
