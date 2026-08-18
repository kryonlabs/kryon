#include "kryon.h"
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

int
main(void)
{
    Rectangle parent = {10, 20, 200, 120};
    UIFrame frame;
    UIGrid grid;
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
            "theme_style_material", "theme_style_aero", "theme_label",
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

    SetThemeStyle(THEME_STYLE_AERO);
    check_int("aero style", GetThemeStyle(), THEME_STYLE_AERO);
    check_int("aero effective style", GetEffectiveThemeStyle(), THEME_STYLE_AERO);
    check_int("aero bevel", GetUIStyleTokens().bevel_enabled, 0);
    check_int("aero shine", GetUIStyleTokens().shine_alpha > 0, 1);
    check_int("aero translucent panels", GetUIStyleTokens().panel_alpha < 255, 1);
    check_int("aero default theme", GetDefaultThemeForThemeStyle(THEME_STYLE_AERO),
              THEME_SKY);
    check_int("aero style label", strcmp(GetThemeStyleLabel(THEME_STYLE_AERO),
                                         "Aero") == 0, 1);
    check_int("theme count", THEME_COUNT, 12);
    check_int("out-of-range theme normalizes", NormalizeTheme(THEME_COUNT),
              THEME_MONO);
    {
        UIAeroScheme scheme = GetUIAeroScheme();

        check_int("aero scheme glass alpha", scheme.glass.a > 0, 1);
        check_int("aero scheme text alpha", scheme.text.a > 0, 1);
        check_int("aero scheme fill top lighter",
                  scheme.fill_top.r + scheme.fill_top.g + scheme.fill_top.b >=
                  scheme.fill_bottom.r + scheme.fill_bottom.g + scheme.fill_bottom.b,
                  1);
    }
    /* Theme-section locale keys must resolve to real strings (the
     * settings picker wires these as fallbacks). */
    {
        static const char *keys[] = {
            "theme_style_label", "theme_style_system", "theme_style_retro",
            "theme_style_material", "theme_style_aero", "theme_label",
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
    check_int("host default style", GetEffectiveThemeStyle(), THEME_STYLE_MATERIAL);
#endif

    frame = BeginUIFrameBox(parent, 10, 10, 4);
    r = UIFramePack(&frame, UI_SIDE_TOP, 30);
    check_int("pack x", (int)r.x, 20);
    check_int("pack y", (int)r.y, 30);
    check_int("pack width", (int)r.width, 180);
    check_int("pack height", (int)r.height, 30);

    grid = (UIGrid){parent, 2, 2, 10, 10, 0, 0};
    r = UIGridCell(grid, 1, 1, 1, 1);
    check_int("grid x", (int)r.x, 115);
    check_int("grid y", (int)r.y, 85);
    check_int("grid width", (int)r.width, 95);
    check_int("grid height", (int)r.height, 55);

    check_int("topmost hit", UICanvasHitTest((Vector2){15, 15}, hits, 3), 1);
    check_int("miss", UICanvasHitTest((Vector2){80, 80}, hits, 3), -1);

    {
        int sx = 10;
        int sy = 20;
        float zoom = 2.0f;
        UICanvas canvas = {{40, 50, 200, 100}, &sx, &sy, &zoom};
        Vector2 p = UICanvasToScreen(canvas, (Vector2){50, 70});
        Rectangle rr = UICanvasRectToScreen(canvas, (Rectangle){50, 70, 20, 10});
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
