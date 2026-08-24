#include "kryon.h"
#include "kry_inject.h"
#include "kryon_test.h"
#include "theme.h"
#include "ui_inspect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

static void
test_menu_bar_switches_while_popup_captures_input(void)
{
    static const MenuItem file_items[] = {
        {MenuCommand, "Open", "Ctrl+O", 101, 0, 0, NULL, 0}
    };
    static const MenuItem edit_items[] = {
        {MenuCommand, "Copy", "Ctrl+C", 201, 0, 0, NULL, 0}
    };
    static const Menu menus[] = {
        {{0, 0, 0, 0}, "File", file_items, 1},
        {{0, 0, 0, 0}, "Edit", edit_items, 1}
    };
    Rectangle bounds = {0, 0, 240, 28};
    int open_index = 0;
    int font;
    int edit_x;
    MenuBarResult result;

    KryonInjectReset();
    BeginUIFrame(640, 480, 1.0f);
    font = GetUIFontSize();
    edit_x = ScaleUIPx(4) + TextWidth("File", font) + ScaleUIPx(24) +
             ScaleUIPx(2) + ScaleUIPx(8);
    EndUIFrame();

    KryonInjectTap((float)edit_x, 14.0f);
    KryonInjectPump();
    KryonInjectPump();

    BeginUIFrame(640, 480, 1.0f);
    PushUIInputCapture((Rectangle){0, 28, 180, 64}, 1);
    result = MenuBar(700, bounds, menus, 2, &open_index);
    EndUIFrame();

    check_int("menu bar switches over popup capture", open_index, 1);
    check_int("menu bar result switches over popup capture",
              result.open_index, 1);
}

int
main(void)
{
    Rectangle parent = {10, 20, 200, 120};
    FrameBox frame;
    Grid grid;
    Rectangle r;
    Rectangle hits[3] = {
        {0, 0, 20, 20},
        {10, 10, 20, 20},
        {100, 100, 10, 10}
    };

    SetUIScale(1.0f);

    SetThemeStyle(THEME_STYLE_RETRO);
    check_int("retro style", GetThemeStyle(), THEME_STYLE_RETRO);
    check_int("retro effective style", GetEffectiveThemeStyle(), THEME_STYLE_RETRO);
    check_int("retro bevel", GetUIStyleTokens().bevel_enabled, 1);

    /* Theme-section locale keys must resolve to real strings (the
     * settings picker wires these as fallbacks). */
    {
        static const char *keys[] = {
            "theme_style_label", "theme_style_system", "theme_style_retro",
            "theme_style_material", "theme_label",
            "theme_app", "theme_system", "theme_mode_label",
            "theme_follow_device", "theme_light", "theme_dark",
            "theme_color_label", "theme_picker_title"
        };
        size_t i;

        for(i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const char *text = GetLocaleText(keys[i]);

            if(text == NULL || text[0] == '\0' || strcmp(text, keys[i]) == 0) {
                fprintf(stderr, "locale key unresolved: %s\n", keys[i]);
                return 1;
            }
        }
    }

    SetThemeStyle(THEME_STYLE_MATERIAL);
    check_int("material style", GetThemeStyle(), THEME_STYLE_MATERIAL);
    check_int("material effective style", GetEffectiveThemeStyle(), THEME_STYLE_MATERIAL);
    check_int("material bevel", GetUIStyleTokens().bevel_enabled, 0);
    check_int("material touch target", GetUIStyleTokens().touch_target_min, 48);

    SetThemeStyle((ThemeStyle)3);
    check_int("out-of-range style clamps", GetThemeStyle(), THEME_STYLE_SYSTEM);
    check_int("out-of-range effective style", GetEffectiveThemeStyle(),
              GetDefaultPlatformThemeStyle());
    check_int("theme count", THEME_COUNT, THEME_PLAN9 + 1);
    check_int("out-of-range theme normalizes", NormalizeTheme(THEME_COUNT),
              THEME_MONO);
    /* Theme-section locale keys must resolve to real strings (the
     * settings picker wires these as fallbacks). */
    {
        static const char *keys[] = {
            "theme_style_label", "theme_style_system", "theme_style_retro",
            "theme_style_material", "theme_label",
            "theme_app", "theme_system", "theme_mode_label",
            "theme_follow_device", "theme_light", "theme_dark",
            "theme_color_label", "theme_picker_title"
        };
        size_t i;

        for(i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const char *text = GetLocaleText(keys[i]);

            if(text == NULL || text[0] == '\0' || strcmp(text, keys[i]) == 0) {
                fprintf(stderr, "locale key unresolved: %s\n", keys[i]);
                return 1;
            }
        }
    }

    SetThemeStyle(THEME_STYLE_MATERIAL);

    SetThemeStyle((ThemeStyle)99);
    check_int("invalid style clamps", GetThemeStyle(), THEME_STYLE_SYSTEM);
    SetThemeStyle(THEME_STYLE_SYSTEM);
#if defined(ANDROID_BUILD) && ANDROID_BUILD
    check_int("android default style", GetEffectiveThemeStyle(), THEME_STYLE_MATERIAL);
#elif defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    check_int("android default style", GetEffectiveThemeStyle(), THEME_STYLE_MATERIAL);
#else
    check_int("host default style", GetEffectiveThemeStyle(), THEME_STYLE_SYSTEM);
#endif

    frame = BeginFrameBox(parent, 10, 10, 4);
    r = FramePack(&frame, SideTop, 30);
    check_int("pack x", (int)r.x, 20);
    check_int("pack y", (int)r.y, 30);
    check_int("pack width", (int)r.width, 180);
    check_int("pack height", (int)r.height, 30);

    grid = (Grid){parent, 2, 2, 10, 10, 0, 0};
    r = GridCell(grid, 1, 1, 1, 1);
    check_int("grid x", (int)r.x, 115);
    check_int("grid y", (int)r.y, 85);
    check_int("grid width", (int)r.width, 95);
    check_int("grid height", (int)r.height, 55);

    check_int("topmost hit", CanvasHitTest((Vector2){15, 15}, hits, 3), 1);
    check_int("miss", CanvasHitTest((Vector2){80, 80}, hits, 3), -1);
    test_menu_bar_switches_while_popup_captures_input();

    {
        int sx = 10;
        int sy = 20;
        float zoom = 2.0f;
        Canvas canvas = {{40, 50, 200, 100}, &sx, &sy, &zoom};
        Vector2 p = CanvasToScreen(canvas, (Vector2){50, 70});
        Rectangle rr = CanvasRectToScreen(canvas, (Rectangle){50, 70, 20, 10});
        check_int("canvas screen x", (int)p.x, 40);
        check_int("canvas screen y", (int)p.y, 50);
        check_int("canvas rect w", (int)rr.width, 40);
        check_int("canvas rect h", (int)rr.height, 20);
    }

    {
        Camera2D camera = {0};
        UIWidget widget;
        UIInspectNode node;
        UIInspectSelection selection;
        int token;

        SetUIInspectEnabled(1);
        BeginUIInspectFrame(".");
        SetUIInspectCanvasBounds((Rectangle){40, 50, 200, 120});
        camera.offset = (Vector2){40, 50};
        camera.zoom = 2.0f;
        token = PushUIInspectTransform(camera);
        BeginUIInspectFrame(NULL);
        widget = BeginUIWidget("test", "inspect-transform",
                               (Rectangle){10, 20, 30, 15}, 0);
        EndUIWidget(&widget);
        check_int("inspect transformed count", UIInspectWidgetCount(), 1);
        check_int("inspect node count", UIInspectNodeCount(), 1);
        check_int("inspect find @name",
                  UIInspectFindNode("@inspect-transform", &node), 1);
        check_int("inspect find role", KryTFind("role=test", &node), 1);
        check_int("inspect node line default", node.source_line, 0);
        check_int("inspect transformed hit",
                  UIInspectSelectAt((Vector2){65, 95}), 1);
        selection = UIInspectGetSelection();
        check_int("inspect selected x", (int)selection.bounds.x, 10);
        check_int("inspect transformed miss",
                  UIInspectSelectAt((Vector2){20, 20}), 0);
        PopUIInspectTransform(token);
    }

    return 0;
}
