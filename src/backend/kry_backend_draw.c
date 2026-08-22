#include "kry_backend.h"
#include "krb.h"
#include "kryon.h"
#include "ui_picture.h"
#include "../ui/ui_internal.h"
#include "../ui/ui_picture_internal.h"

static unsigned
pack_color(Color c)
{
    return ((unsigned)c.r << 24) | ((unsigned)c.g << 16) |
           ((unsigned)c.b << 8) | (unsigned)c.a;
}

static Color
unpack_color(unsigned rgba)
{
    Color c;

    c.r = (unsigned char)((rgba >> 24) & 0xff);
    c.g = (unsigned char)((rgba >> 16) & 0xff);
    c.b = (unsigned char)((rgba >> 8) & 0xff);
    c.a = (unsigned char)(rgba & 0xff);
    return c;
}

static void
draw_clear(unsigned color)
{
    ClearBackground(unpack_color(color));
}

static void
draw_rect(int x, int y, int w, int h, unsigned color)
{
    DrawRectangle(x, y, w, h, unpack_color(color));
}

static void
draw_text(const char *s, int x, int y, int size, unsigned color)
{
    if(s == NULL)
        s = "";
    DrawUIText(s, x, y, size, unpack_color(color));
}

static int
draw_measure_text(const char *s, int size)
{
    if(s == NULL)
        s = "";
    return TextWidth(s, size);
}

static void
draw_clip_push(int x, int y, int w, int h)
{
    BeginUIClip(x, y, w, h);
}

static void
draw_clip_pop(void)
{
    EndUIClip();
}

static void
draw_mouse(int *x, int *y)
{
    Vector2 p = GetMousePosition();

    if(x != NULL)
        *x = (int)p.x;
    if(y != NULL)
        *y = (int)p.y;
}

static int
draw_mouse_down(int button)
{
    int ray = MOUSE_BUTTON_LEFT;

    if(button == KRY_MOUSE_RIGHT)
        ray = MOUSE_BUTTON_RIGHT;
    else if(button == KRY_MOUSE_MIDDLE)
        ray = MOUSE_BUTTON_MIDDLE;
    return IsMouseButtonDown(ray);
}

static int
draw_mouse_pressed(int button)
{
    int ray = MOUSE_BUTTON_LEFT;

    if(button == KRY_MOUSE_RIGHT)
        ray = MOUSE_BUTTON_RIGHT;
    else if(button == KRY_MOUSE_MIDDLE)
        ray = MOUSE_BUTTON_MIDDLE;
    return IsMouseButtonPressed(ray);
}

static int
draw_width(void)
{
    return GetScreenWidth();
}

static int
draw_height(void)
{
    return GetScreenHeight();
}

static float
draw_time(void)
{
    return (float)GetTime();
}

static int
draw_scale_px(int px)
{
    return ScaleUIPx(px);
}

static unsigned
draw_theme_color(int slot)
{
    Color c;

    switch(slot) {
    case KRY_THEME_TEXT:
        c = GetThemeText();
        break;
    case KRY_THEME_ICON:
        c = GetThemeIcon();
        break;
    case KRY_THEME_SURFACE:
        c = GetThemeSurface();
        break;
    case KRY_THEME_BUTTON:
        c = GetThemeButton();
        break;
    case KRY_THEME_BACKGROUND:
    default:
        c = GetThemeBackground();
        break;
    }
    return pack_color(c);
}

static void
draw_texture(const char *asset_path, int x, int y, int w, int h,
             unsigned tint, int fit)
{
    Texture2D tex;
    PictureProps pic;

    if(asset_path == NULL || asset_path[0] == '\0' || w <= 0 || h <= 0)
        return;
    tex = LoadPictureTexture(asset_path);
    if(tex.id == 0)
        return;
    pic = (PictureProps){
        .asset_path = asset_path,
        .bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h},
        .tint = unpack_color(tint),
        .fit = (PictureFit)fit,
    };
    DrawTexturePro(tex,
                   (Rectangle){0, 0, (float)tex.width, (float)tex.height},
                   PictureFitRect(pic, tex), (Vector2){0, 0}, 0.0f, pic.tint);
}

static unsigned
draw_text_key(void)
{
    int c = GetCharPressed();

    return c > 0 && c < 0x110000u ? (unsigned)c : 0;
}

static int
draw_wheel(void)
{
    return (int)(GetMouseWheelMove() * 50.0f);
}

static void
draw_circle(int cx, int cy, int r, unsigned color)
{
    DrawCircle(cx, cy, (float)r, unpack_color(color));
}

static void
draw_ring(int cx, int cy, int inner, int outer, unsigned color)
{
    DrawRing((Vector2){(float)cx, (float)cy}, (float)inner, (float)outer,
             0.0f, 360.0f, 0, unpack_color(color));
}

const KryBackend KryBackendDraw = {
    draw_clear,
    draw_rect,
    draw_text,
    draw_measure_text,
    draw_clip_push,
    draw_clip_pop,
    draw_mouse,
    draw_mouse_down,
    draw_mouse_pressed,
    draw_width,
    draw_height,
    draw_time,
    draw_scale_px,
    draw_theme_color,
    draw_texture,
    draw_circle,
    draw_ring,
    NULL, /* texture_rgba: host textures load through draw_texture */
    draw_wheel,
    draw_text_key,
};

static void
select_draw(void)
{
    KryBackendSelect(&KryBackendDraw);
}

/* Plan-08 audio capability: the raylib surface owns a real audio
 * device, so cartridges get working cap.audio.play/stop here. Skipped
 * whole when the app builds raylib without the audio module. */
#if !defined(SUPPORT_MODULE_RAUDIO) || SUPPORT_MODULE_RAUDIO
static int
cap_audio_raylib(const char *asset_path, int stop)
{
    static Sound current = {0};

    if(stop) {
        if(current.frameCount > 0)
            StopSound(current);
        return 0;
    }
    if(asset_path == NULL || asset_path[0] == '\0')
        return -1;
    if(current.frameCount > 0)
        StopSound(current);
    current = LoadSound(asset_path);
    if(current.frameCount == 0)
        return -1;
    PlaySound(current);
    return 0;
}
#define KRY_KRB_CAP_AUDIO 1
#else
#define KRY_KRB_CAP_AUDIO 0
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
#endif
static void
krb_caps_raylib_init(void)
{
#if KRY_KRB_CAP_AUDIO
    KrbCapAudioHook = cap_audio_raylib;
#endif
    select_draw();
}
