/* aero_shot - render one frame of kryon's widget showcase and export it
 * as a PNG, headless from the caller's perspective (a window opens for
 * the GL context and closes after the capture).
 *
 * usage: aero_shot OUT.png [theme_id] [dark] [style_id]
 *   theme_id  palette id (default THEME_SKY; see theme_meta.h)
 *   dark      0 light, 1 dark (default 0)
 *   style_id  widget style (default THEME_STYLE_AERO; SYSTEM/RETRO/MATERIAL
 *             work too, for side-by-side comparisons)
 *
 * The capture rides kryon's armed EndDrawing: the back buffer is read
 * before the swap, then written with kryon's own PNG writer (raylib's
 * ExportImage does not honor the passed image on this stack).
 */

#include "kryon.h"
#include "ui_tk.h"
#include "ui_modal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Widget entry points declared only in kryon's internal UI header. */
void DrawUITitleBar(const char *title, int height);
void DrawUIProgressBar(ProgressBarProps progress);
int DrawUIRadioButton(RadioButtonProps radio);
int DrawUISpinbox(SpinboxProps spinbox);
void DrawUILabelFrame(LabelFrameProps frame);
/* From src/backend/kry_screenshot.c. */
int kry_write_png_file(const char *path, const unsigned char *rgba,
                       int w, int h);

static char g_text[64] = "Aero glass text field";
static int g_cursor = 21;
static int g_focused = 0;
static int g_slider = 64;
static int g_toggle = 1;
static int g_check = 1;
static int g_combo = 1;
static int g_spin = 4;
static int g_tab_scroll = 0;

static void
draw_scene(int w, int h)
{
    const char *options[3] = {"Glass", "Frost", "Clear"};
    UITab tabs[3] = {
        {"General", {0}, 0, 0, {0}, 0, 0},
        {"Appearance", {0}, 0, 0, {0}, 0, 0},
        {"Advanced", {0}, 0, 0, {0}, 0, 0}
    };
    int x = ScaleUIPx(36);
    int y;

    DrawUITitleBar("Kryon Widget Showcase", ScaleUIPx(44));

    y = ScaleUIPx(80);
    DrawUIText("A single frame of every themed control.", x, y,
               UI_TEXT_16, GetThemeIcon());
    y += ScaleUIPx(44);
    DrawUITabBar((TabBarProps){{(float)x, (float)y,
                                (float)(w - x * 2), (float)ScaleUIPx(40)},
                                tabs, 3, 0, 0, 0, 0, &g_tab_scroll, 0, NULL});
    y += ScaleUIPx(64);
    DrawUIButton((UIButtonSpec){{(float)x, (float)y,
                                 (float)ScaleUIPx(170), (float)ScaleUIPx(44)},
                                 "Primary", UI_TEXT_16, 401, 0,
                                 BLANK, BLANK, BLANK, BLANK, 0});
    DrawUIButton((UIButtonSpec){{(float)x + ScaleUIPx(190), (float)y,
                                 (float)ScaleUIPx(170), (float)ScaleUIPx(44)},
                                 "Danger", UI_TEXT_16, 402, 0,
                                 (Color){180, 70, 70, 255},
                                 (Color){200, 90, 90, 255},
                                 BLANK, BLANK, 0});
    DrawUIButton((UIButtonSpec){{(float)x + ScaleUIPx(380), (float)y,
                                 (float)ScaleUIPx(170), (float)ScaleUIPx(44)},
                                 "Secondary", UI_TEXT_16, 403, 0,
                                 DarkenUIColor(GetThemeBackground(), 14),
                                 GetThemeButton(), BLANK, BLANK, 0});
    y += ScaleUIPx(68);
    DrawUITextField((TextFieldProps){{(float)x, (float)y,
                                      (float)ScaleUIPx(360), (float)ScaleUIPx(44)},
                                      g_text, sizeof(g_text), &g_cursor,
                                      &g_focused, 80, UI_TEXT_16, 404,
                                      (UITextInputStyle){0}, NULL, NULL, NULL, 0});
    DrawUIDropdown(405, x + ScaleUIPx(390), y, ScaleUIPx(360), ScaleUIPx(44),
                   options, 3, &g_combo);
    y += ScaleUIPx(72);
    DrawUISlider(406, x, y, ScaleUIPx(360), "Slider", 0, 100, &g_slider,
                 "%", NULL);
    DrawUIToggleSwitch(x + ScaleUIPx(390), y, ScaleUIPx(170), ScaleUIPx(44),
                       &g_toggle, "Off", "On");
    DrawUICheckboxToggle(x + ScaleUIPx(590), y + ScaleUIPx(12), "Frost",
                         &g_check);
    y += ScaleUIPx(72);
    DrawUIProgressBar((ProgressBarProps){{(float)x, (float)y,
                                          (float)ScaleUIPx(360),
                                          (float)ScaleUIPx(22)},
                                          0, 100, 62, "Syncing"});
    DrawUIRadioButton((RadioButtonProps){{(float)x + ScaleUIPx(390),
                                          (float)y - ScaleUIPx(8),
                                          (float)ScaleUIPx(170),
                                          (float)ScaleUIPx(36)},
                                          "Frosted", 409, 1, 0});
    DrawUIRadioButton((RadioButtonProps){{(float)x + ScaleUIPx(390),
                                          (float)y + ScaleUIPx(32),
                                          (float)ScaleUIPx(170),
                                          (float)ScaleUIPx(36)},
                                          "Clear", 410, 0, 0});
    y += ScaleUIPx(64);
    DrawUISpinbox((SpinboxProps){{(float)x, (float)y,
                                  (float)ScaleUIPx(220), (float)ScaleUIPx(44)},
                                  411, 0, 20, 1, &g_spin, 0, NULL, 0});
    y += ScaleUIPx(80);
    DrawUILabelFrame((LabelFrameProps){{(float)x, (float)y,
                                        (float)ScaleUIPx(420),
                                        (float)ScaleUIPx(140)},
                                        "Glass panel"});
    DrawUIText("Translucent panel with gloss and soft shadow.",
               x + ScaleUIPx(16), y + ScaleUIPx(40), UI_TEXT_16,
               GetThemeText());
    (void)h;
}

int
main(int argc, char **argv)
{
    const char *out;
    int theme_id;
    int dark;
    ThemeStyle style;
    int w = 900;
    int h = 680;
    Image img;

    if(argc < 2) {
        fprintf(stderr,
                "usage: aero_shot OUT.png [theme_id] [dark] [style_id]\n"
                "  theme_id palette (default %d, count %d)\n"
                "  dark      0/1 (default 0)\n"
                "  style_id  %d System, %d Retro, %d Material, %d Aero\n",
                THEME_SKY, THEME_COUNT, THEME_STYLE_SYSTEM, THEME_STYLE_RETRO,
                THEME_STYLE_MATERIAL, THEME_STYLE_AERO);
        return 1;
    }
    out = argv[1];
    theme_id = argc > 2 ? atoi(argv[2]) : THEME_SKY;
    dark = argc > 3 ? atoi(argv[3]) != 0 : 0;
    style = argc > 4 ? (ThemeStyle)atoi(argv[4]) : THEME_STYLE_AERO;

    InitWindow(w, h, "kryon-aero-shot");
    SetTargetFPS(5);
    EnsureUIDefaultFont();
    InitUI(w, h, GetUIScale());
    SetThemeSource(THEME_SOURCE_APP);
    SetThemeMode(dark ? THEME_MODE_DARK : THEME_MODE_LIGHT);
    SetThemeStyle(style);
    SetCurrentTheme(theme_id, dark);

    /* Kryon's EndDrawing captures the completed back buffer before the
     * swap while armed; LoadImageFromScreen hands that frame back. */
    setenv("KRYON_SHOT_ARM", "1", 1);

    BeginDrawing();
    ClearBackground(GetThemeBackground());
    BeginUIFrame(w, h, 1.0f);
    draw_scene(w, h);
    EndUIFrame();
    EndDrawing();

    img = LoadImageFromScreen();
    if(img.data == NULL) {
        fprintf(stderr, "aero_shot: framebuffer readback failed\n");
        return 1;
    }
    if(kry_write_png_file(out, img.data, img.width, img.height) != 0) {
        fprintf(stderr, "aero_shot: could not write %s\n", out);
        return 1;
    }
    free(img.data);

    CloseWindow();
    printf("wrote %s (%dx%d)\n", out, img.width, img.height);
    return 0;
}
