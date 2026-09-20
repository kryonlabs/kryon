#include "kryon_compat.generated.h"
#include "kry_uri.h"
#include <stddef.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

int
ui_text_context_menu_supported(void)
{
#if ANDROID_BUILD
    return 0;
#else
    return 1;
#endif
}

int
HoverEffectsEnabled(void)
{
#if ANDROID_BUILD
    return 0;
#else
    return 1;
#endif
}

int
ui_motion_backend_available(void)
{
#if defined(KRYON_BACKEND_TERMI)
    return 0;
#else
    return 1;
#endif
}

void
ui_open_url(const char *url)
{
    if(url == NULL || url[0] == '\0')
        return;
#if defined(PLATFORM_WEB)
    EM_ASM({
        window.location.href = UTF8ToString($0);
    }, url);
#else
    (void)OpenURI(url);
#endif
}

void
DrawCustomIcon(int x, int y, int size, Texture2D icon, Color tint)
{
    if(icon.id == 0 || size <= 0)
        return;
    Rectangle src = {0, 0, (float)icon.width, (float)icon.height};
    Rectangle dst = {(float)x, (float)y, (float)size, (float)size};
    Vector2 origin = {0};
    DrawTexturePro(icon, src, dst, origin, 0, tint);
}
