#include "kry_backend.h"
#include "kryon.h"

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
    return MeasureUIText(s, size);
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
};

static void
select_draw(void)
{
    KryBackendSelect(&KryBackendDraw);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
#endif
static void
kry_backend_draw_init(void)
{
    select_draw();
}
