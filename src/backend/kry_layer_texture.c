#include "kryon.h"

#if defined(KRYON_BACKEND_RAYLIB)
int kryon_rl_framebuffer_save(void);
void kryon_rl_framebuffer_restore(int framebuffer);
#endif

/* Texture allocation and readback may bind a framebuffer internally. */
RenderTexture2D
kry_layer_texture_replace(RenderTexture2D old, int width, int height)
{
#if defined(KRYON_BACKEND_RAYLIB)
    int framebuffer = kryon_rl_framebuffer_save();
#endif
    if(old.id)
        UnloadRenderTexture(old);
    RenderTexture2D texture = width > 0
        ? LoadRenderTexture(width, height) : (RenderTexture2D){0};
#if defined(KRYON_BACKEND_RAYLIB)
    kryon_rl_framebuffer_restore(framebuffer);
#endif
    return texture;
}

Image
ui_paint_readback(Texture2D texture)
{
#if defined(KRYON_BACKEND_RAYLIB)
    int framebuffer = kryon_rl_framebuffer_save();
#endif
    Image image = LoadImageFromTexture(texture);
#if defined(KRYON_BACKEND_RAYLIB)
    kryon_rl_framebuffer_restore(framebuffer);
#endif
    return image;
}
