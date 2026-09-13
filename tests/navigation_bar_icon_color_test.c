#include "kryon.h"
#include "runtime/navigation_bar.h"
#include "../src/ui/ui_internal.h"

#include <stdio.h>
#include <stdlib.h>

static int icon_calls;
static Color icon_tints[3];

static StyleFrame
test_style_frame(uint32_t background, uint32_t foreground, uint32_t border)
{
    StyleFrame frame = {0};
    frame.value.fields = StyleBackground | StyleForeground | StyleBorder |
                         StyleOpacity;
    frame.value.background = background;
    frame.value.foreground = foreground;
    frame.value.border = border;
    frame.value.opacity = 1.0f;
    return frame;
}

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
    items[0] = (NavigationBarItem){1, "", color_icon, ICON_NONE, 0, 0};
    items[1] = (NavigationBarItem){2, "", color_icon, ICON_NONE, 1, 0};
    items[2] = (NavigationBarItem){3, "", color_icon, ICON_NONE, 0, 1};

    SetScale(1.0f);
    SetDefaultFontAutoLoad(0);
    SetThemeSource(THEME_SOURCE_APP);
    SetCurrentTheme(THEME_SKY, 0);
    ClearStylePacks();
    if(!RegisterBuiltInStylePacks()) {
        fprintf(stderr, "built-in styles did not register\n");
        return 1;
    }
    if(!RegisterStylePackSource("@pack test.navigation.margin;\n"
                                "NavigationBar { padding-y: 48; }\n"
                                "NavigationBarItem { foreground: #8d919a; }\n"
                                "NavigationBarItem:selected { foreground: #171022; }\n"
                                "NavigationBarItem:disabled { foreground: #8d919a; }\n",
                                "Test Navigation Margin", "") ||
       !SetActiveStylePack("test.navigation.margin")) {
        fprintf(stderr, "test navigation margin style did not activate\n");
        return 1;
    }

    check_int("compact navigation bar default height",
              NavigationBarDefaultHeight(1.0f), 86);
    {
        StyleFrame bar = test_style_frame(0x111111ffu, 0x222222ffu,
                                          0x333333ffu);
        StyleFrame item = test_style_frame(0x444444ffu, 0x555555ffu,
                                           0x666666ffu);
        item.value.radius = 29.0f;
        NavigationBarPaint paint = NavigationBarPaintFor((NavigationBarSpec){
            .view_width = 900,
            .view_height = 720,
            .count = 4,
            .icon_size = -1,
            .scale = 1.0f,
            .bar = bar,
        });
        NavigationBarItemPaint item_paint =
            NavigationBarItemPaintFor((NavigationBarItemSpec){
                .bar = paint,
                .index = 1,
                .active = true,
                .label_height = TextLineHeight(GetSmallFontSize()),
                .base = item,
                .face = item,
            });

        check_int("compact navigation bar hit target width",
                  (int)item_paint.bounds.width, 225);
        check_int("compact navigation bar default icon size",
                  paint.icon_size, 32);
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
        check_int("compact navigation bar active badge uses KSS radius",
                  (int)item_paint.face.value.radius, 29);

        item.value.fields |= StylePaddingX | StylePaddingY | StyleGap |
                             StyleContentOffset;
        item.value.padding_x = 10.0f;
        item.value.padding_y = 6.0f;
        item.value.gap = 8.0f;
        item.value.offset_x = 20.0f;
        item_paint = NavigationBarItemPaintFor((NavigationBarItemSpec){
            .bar = paint,
            .index = 1,
            .active = true,
            .label_height = TextLineHeight(GetSmallFontSize()),
            .base = item,
            .face = item,
        });
        check_int("compact navigation bar badge width uses KSS",
                  (int)item_paint.state_bounds.width, 52);
        check_int("compact navigation bar badge height uses KSS",
                  (int)item_paint.state_bounds.height, 52);
        check_int("compact navigation bar label inset uses KSS",
                  (int)item_paint.label_bounds.x, 235);
        check_int("compact navigation bar label width uses KSS",
                  (int)item_paint.label_bounds.width, 205);

        item.value.padding_x = 0.0f;
        item.value.padding_y = 0.0f;
        item.value.gap = 0.0f;
        item.value.offset_x = 0.0f;
        item_paint = NavigationBarItemPaintFor((NavigationBarItemSpec){
            .bar = paint,
            .index = 1,
            .active = true,
            .label_height = TextLineHeight(GetSmallFontSize()),
            .base = item,
            .face = item,
        });
        check_int("compact navigation bar zero badge width uses KSS",
                  (int)item_paint.state_bounds.width, 32);
        check_int("compact navigation bar zero badge height uses KSS",
                  (int)item_paint.state_bounds.height, 32);
        check_int("compact navigation bar zero label inset uses KSS",
                  (int)item_paint.label_bounds.x, 225);
        check_int("compact navigation bar zero label width uses KSS",
                  (int)item_paint.label_bounds.width, 225);

        paint = NavigationBarPaintFor((NavigationBarSpec){
            .view_width = 900,
            .view_height = 720,
            .count = 4,
            .icon_size = 0,
            .scale = 1.0f,
            .bar = bar,
        });
        item_paint = NavigationBarItemPaintFor((NavigationBarItemSpec){
            .bar = paint,
            .index = 1,
            .active = true,
            .label_height = TextLineHeight(GetSmallFontSize()),
            .base = item,
            .face = item,
        });
        check_int("compact navigation bar explicit zero icon size",
                  paint.icon_size, 0);
        check_int("compact navigation bar explicit zero icon width",
                  (int)item_paint.icon_bounds.width, 0);

        bar.value.fields |= StyleIconSize | StyleContentOffset |
                            StyleBorderWidth;
        bar.value.icon_size = 72.0f;
        bar.value.offset_x = 180.0f;
        bar.value.offset_y = 70.0f;
        bar.value.border_width = 4.0f;
        paint = NavigationBarPaintFor((NavigationBarSpec){
            .view_width = 120,
            .view_height = 200,
            .count = 2,
            .icon_size = -1,
            .scale = 1.0f,
            .bar = bar,
        });
        check_int("compact navigation bar height uses KSS",
                  paint.height, 72);
        check_int("compact navigation bar min width uses KSS",
                  (int)paint.bar_bounds.width, 172);
        check_int("compact navigation bar inset uses KSS",
                  (int)paint.bar_bounds.x, 4);
        bar.value.border_width = 0.0f;
        paint = NavigationBarPaintFor((NavigationBarSpec){
            .view_width = 120,
            .view_height = 200,
            .count = 2,
            .icon_size = -1,
            .scale = 1.0f,
            .bar = bar,
        });
        check_int("compact navigation bar zero inset uses KSS",
                  (int)paint.bar_bounds.x, 0);
        check_int("compact navigation bar zero inset width uses KSS",
                  (int)paint.bar_bounds.width, 180);
    }
    {
        StyleFrame panel = {.value = {.fields = StyleContentOffset |
                                                StyleIconSize |
                                                StylePaddingY,
                                       .offset_x = 340.0f,
                                       .icon_size = 128.0f,
                                       .padding_y = 58.0f}};
        StyleFrame row_frame = {.value = {.fields = StyleIconSize |
                                                    StyleContentOffset |
                                                    StylePaddingX |
                                                    StylePaddingY,
                                           .icon_size = 58.0f,
                                           .offset_y = 22.0f,
                                           .padding_x = 36.0f,
                                           .padding_y = 36.0f}};
        StyleFrame action = {.value = {.fields = StyleIconSize |
                                                 StyleContentOffset |
                                                 StylePaddingX |
                                                 StylePaddingY |
                                                 StyleGap,
                                        .icon_size = 34.0f,
                                        .offset_x = 180.0f,
                                        .offset_y = 16.0f,
                                        .padding_x = 92.0f,
                                        .padding_y = 36.0f,
                                        .gap = 8.0f}};
        StyleFrame route = {.value = {.fields = StyleIconSize |
                                                StyleContentOffset |
                                                StylePaddingY |
                                                StyleGap,
                                       .icon_size = 48.0f,
                                       .offset_y = 8.0f,
                                       .padding_y = 12.0f,
                                       .gap = 8.0f}};
        NavigationBarConfigMetrics metrics =
            NavigationBarConfigMetricsFor(1.0f, panel, row_frame, action,
                                          route);
        NavigationBarConfigLayout layout =
            NavigationBarConfigLayoutFor((Rectangle){100, 80, 340, 360},
                                         (Rectangle){118, 138, 304, 286},
                                         3, metrics);
        NavigationBarConfigRowLayout row =
            NavigationBarConfigRowLayoutFor((Rectangle){118, 138, 304, 286},
                                            196, 18, metrics);

        check_int("navigation config frame width", metrics.frame_width, 340);
        check_int("navigation config frame height",
                  NavigationBarConfigFrameHeight(3, metrics), 394);
        check_int("navigation config route view height",
                  (int)layout.route_bounds.height, 196);
        check_int("navigation config route content height",
                  layout.route_content_height, 174);
        check_int("navigation config add x", (int)layout.add_bounds.x, 180);
        check_int("navigation config add y", (int)layout.add_bounds.y, 346);
        check_int("navigation config reset x", (int)layout.reset_bounds.x, 124);
        check_int("navigation config save x", (int)layout.save_bounds.x, 324);
        check_int("navigation config row dropdown width",
                  (int)row.dropdown_bounds.width, 260);
        check_int("navigation config row remove x",
                  (int)row.remove_bounds.x, 386);

        panel.value.offset_x = 360.0f;
        row_frame.value.icon_size = 62.0f;
        row_frame.value.padding_x = 40.0f;
        action.value.gap = 10.0f;
        route.value.offset_y = 12.0f;
        metrics = NavigationBarConfigMetricsFor(1.0f, panel, row_frame,
                                                action, route);
        check_int("navigation config frame width uses KSS",
                  metrics.frame_width, 360);
        check_int("navigation config row height uses KSS",
                  metrics.row_height, 62);
        check_int("navigation config remove width uses KSS",
                  metrics.remove_width, 40);
        check_int("navigation config button gap uses KSS",
                  metrics.button_gap, 10);
        check_int("navigation config scrollbar width uses KSS",
                  metrics.scrollbar_width, 12);

        panel.value.padding_y = 0.0f;
        row_frame.value.offset_y = 0.0f;
        row_frame.value.padding_x = 0.0f;
        action.value.offset_y = 0.0f;
        action.value.gap = 0.0f;
        route.value.padding_y = 0.0f;
        route.value.gap = 0.0f;
        route.value.offset_y = 0.0f;
        metrics = NavigationBarConfigMetricsFor(1.0f, panel, row_frame,
                                                action, route);
        check_int("navigation config zero bottom extra uses KSS",
                  metrics.frame_bottom_extra, 0);
        check_int("navigation config zero row control y uses KSS",
                  metrics.row_control_y, 0);
        check_int("navigation config zero remove width uses KSS",
                  metrics.remove_width, 0);
        check_int("navigation config zero button gap uses KSS",
                  metrics.button_gap, 0);
        check_int("navigation config zero scrollbar width uses KSS",
                  metrics.scrollbar_width, 0);
    }

    BeginInterfaceFrame(900, 720, 1.0f);
    result = NavigationBar((NavigationBarProps){
        .view_width = 900,
        .view_height = 720,
        .count = 3,
        .items = items,
        .height = 66,
    });
    EndInterfaceFrame();

    check_int("navigation bar drew all icons", icon_calls, 3);
    check_int("navigation bar y honors KSS padding-y", result.y, 606);
    check_int("navigation bar lower edge anchors to KSS padding-y",
              result.y + result.height + 48, 720);
    {
        check_int("inactive navigation bar icon alpha", icon_tints[0].a, 255);
        check_int("active navigation bar icon alpha", icon_tints[1].a, 255);
        check_int("inactive navigation bar icon KSS red", icon_tints[0].r, 0x8d);
        check_int("inactive navigation bar icon KSS green", icon_tints[0].g, 0x91);
        check_int("inactive navigation bar icon KSS blue", icon_tints[0].b, 0x9a);
        check_int("active navigation bar icon KSS red", icon_tints[1].r, 0x17);
        check_int("active navigation bar icon KSS green", icon_tints[1].g, 0x10);
        check_int("active navigation bar icon KSS blue", icon_tints[1].b, 0x22);
        check_int("disabled navigation bar icon alpha", icon_tints[2].a, 150);
    }
    return 0;
}
