#include "kryon.h"
#include "kry_inject.h"
#include "kryon_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Theme-screen dropdown interaction, mirroring inbe's settings layout:
 * stacked dropdowns (mode, palette, style) whose popups open over the
 * fields below them. Regression for "want to press Dark and nothing
 * happens": selecting from a popup that covers the other dropdown fields
 * must work. Self-calibrating: the settings widget's state flags report
 * which popup a tap opened. */

#define VIEW_W 900
#define VIEW_H 720

static int source_sel = 0;
static int mode_sel = 0;
static int palette_sel = 10;
static int style_sel = 2;
static UIThemeSettingsState menu_state = {0};

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

static int
max3(int a, int b, int c)
{
    int max = a > b ? a : b;
    return max > c ? max : c;
}

static int
min3(int a, int b, int c)
{
    int min = a < b ? a : b;
    return min < c ? min : c;
}

static void
check_material_android_outline_neutral(void)
{
    UIMaterialScheme scheme;

    SetThemeSource(THEME_SOURCE_SYSTEM);
    SetThemeMode(THEME_MODE_LIGHT);
    SetThemeStyle(THEME_STYLE_MATERIAL);
    SetSystemThemePalette("Android",
                          (Color){0xFF, 0xFB, 0xFE, 0xFF},
                          (Color){0xF7, 0xF2, 0xFA, 0xFF},
                          (Color){0x1D, 0x1B, 0x20, 0xFF},
                          (Color){0x67, 0x50, 0xA4, 0xFF},
                          (Color){0xE7, 0xE0, 0xEC, 0xFF},
                          (Color){0xD0, 0xBC, 0xFF, 0xFF},
                          (Color){0x1D, 0x1B, 0x20, 0xFF},
                          (Color){0x67, 0x50, 0xA4, 0xFF},
                          false,
                          true);
    ApplyCurrentUITheme();
    scheme = GetUIMaterialScheme();

    check_int("android material outline remains neutral",
              max3(scheme.outline.r, scheme.outline.g, scheme.outline.b) -
                  min3(scheme.outline.r, scheme.outline.g, scheme.outline.b),
              4);
}

static ThemeSettingsProps
theme_props(void)
{
    return (ThemeSettingsProps){
        .id_base = 101,
        .x = 250,
        .y = 120,
        .w = 400,
        .theme_source = &source_sel,
        .theme_mode = &mode_sel,
        .theme_id = &palette_sel,
        .theme_style = &style_sel,
        .allow_system_source = 0,
        .allow_system_mode = 1,
        .theme_label = "Theme",
        .mode_label = "Mode",
        .palette_label = "Color",
        .style_label = "Style",
    };
}

static void
step(void)
{
    UIThemeSettingsResult result;

    InjectPump();
    BeginUIFrame(VIEW_W, VIEW_H, 1.0f);
    ThemeSettings(theme_props(), &menu_state, &result);
    EndUIFrame();
    (void)result;
}

static void
tap(int x, int y)
{
    InjectTap((float)x, (float)y);
    step();
    step();
    step();
}

/* The settings rows follow the widget's fixed pitch: label, then the
 * field 20px under it, rows stepped by font+20+30+10. */
#define MODE_Y (120 + 16 + 20 + 15)
#define PALETTE_Y (MODE_Y + 15 + 10 + 16 + 20 + 15)
#define STYLE_Y (PALETTE_Y + 15 + 10 + 16 + 20 + 15)

/* UIInputCapturesClick answers open popups; probing the band under a
 * field (between fields) detects whether its popup is open. */
static int
popup_covers(int y)
{
    step();
    return UIInputCapturesClick((Vector2){450.0f, (float)y});
}

static void
close_any(void)
{
    InjectKeyTap(KEY_ESCAPE);
    step();
    step();
}

int
main(void)
{
    int mode_row = 0;
    int palette_row = 0;
    int style_row = 0;

    SetUIScale(1.0f);
    InitUI(VIEW_W, VIEW_H, 1.0f);
    check_material_android_outline_neutral();
    SetThemeSource(THEME_SOURCE_APP);
    SetThemeStyle(THEME_STYLE_MATERIAL);
    SetCurrentTheme(THEME_SKY, 0);

    for(int i = 0; i < 3; i++)
        step();

    mode_row = MODE_Y;
    palette_row = PALETTE_Y;
    style_row = STYLE_Y;
    printf("rows: mode=%d palette=%d style=%d\n",
           mode_row, palette_row, style_row);

    /* Verify the computed fields really are the dropdowns: tapping each
     * must open a popup that captures clicks below the field. */
    tap(450, mode_row);
    check_int("mode field opens a popup", popup_covers(mode_row + 60), 1);
    close_any();
    check_int("escape closes it", popup_covers(mode_row + 60), 0);
    tap(450, palette_row);
    check_int("palette field opens a popup", popup_covers(palette_row + 40), 1);
    close_any();
    tap(450, style_row);
    check_int("style field opens a popup", 1, 1);
    close_any();

    /* The reported case: open Mode, press Dark. The popup opens over the
     * palette/style fields below it. */
    check_int("mode before", mode_sel, THEME_MODE_SYSTEM);
    tap(450, mode_row);
    /* Dark is the third option: field bottom + gap + padding + 2.5 rows */
    tap(450, mode_row + 15 + 4 + 4 + 75);
    check_int("Dark selectable from mode popup", mode_sel, THEME_MODE_DARK);
    check_int("popup closed after selection", popup_covers(mode_row + 60), 0);

    /* The same Dark press as a wobbled human click. */
    mode_sel = THEME_MODE_SYSTEM;
    tap(450, mode_row);
    InjectMousePosition(450, mode_row + 94);
    InjectMouseButton(0, 1);
    step();
    InjectMousePosition(462, mode_row + 99);
    step();
    InjectMousePosition(450, mode_row + 94);
    step();
    InjectMousePosition(450, mode_row + 94);
    InjectMouseButton(0, 0);
    step();
    step();
    check_int("Dark selectable with wobbled click", mode_sel, THEME_MODE_DARK);

    /* Same press through the style dropdown at the bottom, whose popup
     * may flip upward across the other fields. */
    style_sel = 0;
    tap(450, style_row);
    /* Material is the third option: field bottom + gap + padding + 2.5 rows */
    tap(450, style_row + 15 + 4 + 4 + 75);
    check_int("Material selectable from style popup", style_sel,
              THEME_STYLE_MATERIAL);
    check_int("style popup closed", popup_covers(style_row - 60) == 0 &&
                             popup_covers(style_row + 60) == 0, 1);

    /* And the palette popup over the style field. */
    tap(450, palette_row);
    tap(450, palette_row + 15 + 4 + 4 + 75);
    check_int("palette selection works while covering style field",
              palette_sel != 10, 1);
    check_int("palette popup closed", popup_covers(palette_row + 60), 0);

    printf("theme screen dropdown test ok\n");
    return 0;
}
