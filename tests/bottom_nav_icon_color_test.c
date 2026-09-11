#include "kryon.h"
#include "runtime/navigation_bar.h"

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

static void
check_true(const char *name, int value)
{
    if(value)
        return;
    fprintf(stderr, "%s: false\n", name);
    exit(1);
}

void
__wrap_DrawRectangle(int pos_x, int pos_y, int width, int height, Color color)
{
    (void)pos_x;
    (void)pos_y;
    (void)width;
    (void)height;
    (void)color;
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
__wrap_DrawRectangleGradientV(int pos_x, int pos_y, int width, int height,
                              Color top, Color bottom)
{
    (void)pos_x;
    (void)pos_y;
    (void)width;
    (void)height;
    (void)top;
    (void)bottom;
}

void
__wrap_DrawRectangleLinesEx(Rectangle rec, float line_thick, Color color)
{
    (void)rec;
    (void)line_thick;
    (void)color;
}

void
__wrap_DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments,
                                 Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)color;
}

void
__wrap_DrawRectangleRoundedLinesEx(Rectangle rec, float roundness,
                                   int segments, float line_thick,
                                   Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)line_thick;
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
    NavigationBarItem items[3];
    NavigationBarResult result;

    color_icon.id = 42;
    color_icon.width = 16;
    color_icon.height = 16;
    items[0] = (NavigationBarItem){1, "", color_icon, UI_ICON_TYPE_NONE, 0, 0};
    items[1] = (NavigationBarItem){2, "", color_icon, UI_ICON_TYPE_NONE, 1, 0};
    items[2] = (NavigationBarItem){3, "", color_icon, UI_ICON_TYPE_NONE, 0, 1};

    SetUIScale(1.0f);
    SetUIDefaultFontAutoLoad(0);
    SetThemeSource(THEME_SOURCE_APP);
    SetThemeStyle(THEME_STYLE_DEFAULT);
    SetCurrentTheme(THEME_SKY, 0);

    check_int("compact navigation bar default height",
              NavigationBarDefaultHeight(1.0f), 86);
    {
        Palette palette = DefaultPalette(false);
        Metrics metrics = DefaultMetrics();
        NavigationBarPaint paint = NavigationBarPaintFor((NavigationBarSpec){
            .view_width = 900,
            .view_height = 720,
            .count = 4,
            .scale = 1.0f,
            .palette = palette,
            .metrics = metrics,
        });
        NavigationBarItemPaint item_paint =
            NavigationBarItemPaintFor((NavigationBarItemSpec){
                .bar = paint,
                .index = 1,
                .active = true,
                .label_height = TextLineHeight(GetSmallFontSize()),
                .palette = palette,
                .metrics = metrics,
            });

        check_int("compact navigation bar hit target width",
                  (int)item_paint.bounds.width, 225);
        check_int("compact navigation bar active badge width",
                  (int)item_paint.state_bounds.width, 58);
        check_int("compact navigation bar active badge height",
                  (int)item_paint.state_bounds.height, 58);
        check_true("compact navigation bar badge stays within tab",
                   item_paint.state_bounds.x > item_paint.bounds.x &&
                   item_paint.state_bounds.x + item_paint.state_bounds.width <
                       item_paint.bounds.x + item_paint.bounds.width);
        check_true("compact navigation bar active badge stays above label",
                   item_paint.state_bounds.y + item_paint.state_bounds.height <=
                       item_paint.label_bounds.y);
        check_true("compact navigation bar icon is centered in badge",
                   (int)item_paint.icon_bounds.y ==
                       (int)(item_paint.state_bounds.y +
                             (item_paint.state_bounds.height -
                              item_paint.icon_bounds.height) / 2.0f));
        check_true("compact navigation bar active badge is pill-shaped",
                   item_paint.face.value.radius >= metrics.radius_pill);
    }

    BeginUIFrame(900, 720, 1.0f);
    result = NavigationBar((NavigationBarProps){
        .view_width = 900,
        .view_height = 720,
        .count = 3,
        .items = items,
        .height = 66,
        .bottom_margin = 48,
    });
    EndUIFrame();

    check_int("navigation bar drew all icons", icon_calls, 3);
    check_int("navigation bar y honors bottom margin", result.y, 606);
    check_int("navigation bar lower edge anchors to usable bottom",
              result.y + result.height + 48, 720);
    {
        check_int("inactive navigation bar icon alpha", icon_tints[0].a, 255);
        check_int("active navigation bar icon alpha", icon_tints[1].a, 255);
        check_true("active navigation bar icon has distinct theme color",
                   icon_tints[0].r != icon_tints[1].r ||
                   icon_tints[0].g != icon_tints[1].g ||
                   icon_tints[0].b != icon_tints[1].b);
        check_true("disabled navigation bar icon is not active accent",
                   icon_tints[2].r != icon_tints[1].r ||
                   icon_tints[2].g != icon_tints[1].g ||
                   icon_tints[2].b != icon_tints[1].b);
        check_int("disabled navigation bar icon alpha", icon_tints[2].a, 150);
    }
    for(int dark = 0; dark <= 1; dark++) {
        SetCurrentTheme(THEME_SKY, dark);
        Color tint = GetThemeText();
        icon_calls = 0;
        BeginUIFrame(900, 720, 1.0f);
        NavigationBar((NavigationBarProps){
            .view_width = 900, .view_height = 720,
            .count = 3, .items = items, .height = 66,
            .icon_color = tint,
        });
        EndUIFrame();
        check_int("tinted icon count", icon_calls, 3);
        for(int i = 0; i < 3; i++) {
            check_int("theme tint red", icon_tints[i].r, tint.r);
            check_int("theme tint green", icon_tints[i].g, tint.g);
            check_int("theme tint blue", icon_tints[i].b, tint.b);
        }
        check_int("tinted disabled alpha", icon_tints[2].a, 150);
    }
    return 0;
}
