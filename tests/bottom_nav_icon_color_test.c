#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>

static int icon_calls;
static Color icon_tints[3];

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

void
__wrap_DrawRectangleRec(Rectangle rec, Color color)
{
    (void)rec;
    (void)color;
}

void
__wrap_DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                            Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)color;
}

void
__wrap_SetMouseCursor(int cursor)
{
    (void)cursor;
}

Vector2
__wrap_GetMousePosition(void)
{
    return (Vector2){-1000.0f, -1000.0f};
}

bool
__wrap_IsMouseButtonReleased(int button)
{
    (void)button;
    return false;
}

bool
__wrap_IsMouseButtonDown(int button)
{
    (void)button;
    return false;
}

void
__wrap_DrawTexturePro(Texture2D texture, Rectangle srcrec, Rectangle dstrec,
                      Vector2 origin, float rotation, Color tint)
{
    (void)srcrec;
    (void)dstrec;
    (void)origin;
    (void)rotation;

    if(texture.id != 42)
        return;
    if(icon_calls < 3)
        icon_tints[icon_calls] = tint;
    icon_calls++;
}

int
main(void)
{
    Texture2D color_icon = {0};
    BottomNavItem items[3];

    color_icon.id = 42;
    color_icon.width = 16;
    color_icon.height = 16;
    items[0] = (BottomNavItem){1, "", color_icon, 0, 0};
    items[1] = (BottomNavItem){2, "", color_icon, 1, 0};
    items[2] = (BottomNavItem){3, "", color_icon, 0, 1};

    SetUIScale(1.0f);
    SetUIDefaultFontAutoLoad(0);
    SetThemeSource(THEME_SOURCE_APP);
    SetThemeStyle(THEME_STYLE_MATERIAL);
    SetCurrentTheme(THEME_SKY, 0);

    BeginUIFrame(900, 720, 1.0f);
    BottomNav((BottomNavProps){
        .view_width = 900,
        .view_height = 720,
        .count = 3,
        .items = items,
    });
    EndUIFrame();

    check_int("bottom nav drew all icons", icon_calls, 3);
    {
        check_int("inactive bottom nav icon red", icon_tints[0].r, 255);
        check_int("inactive bottom nav icon green", icon_tints[0].g, 255);
        check_int("inactive bottom nav icon blue", icon_tints[0].b, 255);
        check_int("active bottom nav icon red", icon_tints[1].r, 255);
        check_int("active bottom nav icon green", icon_tints[1].g, 255);
        check_int("active bottom nav icon blue", icon_tints[1].b, 255);
        check_int("disabled bottom nav icon red", icon_tints[2].r, 255);
        check_int("disabled bottom nav icon green", icon_tints[2].g, 255);
        check_int("disabled bottom nav icon blue", icon_tints[2].b, 255);
        check_int("disabled bottom nav icon alpha", icon_tints[2].a, 150);
    }
    return 0;
}
