#include "kryon.h"
#include "kry_inject.h"
#include "kryon_test.h"
#include "ui_overlay.h"

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

static const char *mode_opts[3] = {"System", "Light", "Dark"};
static const char *palette_opts[12] = {"Cobalt", "Cherry", "Dawn", "Forest",
                                       "Lavender", "Mint", "Mono", "Ocean",
                                       "Sage", "Sepia", "Sky", "Sunset"};
static const char *style_opts[4] = {"System style", "Retro", "Material",
                                    "Aero"};

static int source_sel = 0;
static int mode_sel = 0;
static int palette_sel = 10;
static int style_sel = 3;
static UIThemeSettingsState menu_state = {0};

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
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

    KryonInjectPump();
    BeginUIFrame(VIEW_W, VIEW_H, 1.0f);
    DrawUIThemeSettings(theme_props(), &menu_state);
    EndUIFrame();
    result = DrawUIThemeSettingsMenus(theme_props(), &menu_state);
    (void)result;
}

static void
tap(int x, int y)
{
    KryonInjectTap((float)x, (float)y);
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
    KryonInjectKeyTap(KEY_ESCAPE);
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
    SetThemeSource(THEME_SOURCE_APP);
    SetThemeStyle(THEME_STYLE_AERO);
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
    KryonInjectMousePosition(450, mode_row + 94);
    KryonInjectMouseButton(0, 1);
    step();
    KryonInjectMousePosition(462, mode_row + 99);
    step();
    KryonInjectMousePosition(450, mode_row + 94);
    step();
    KryonInjectMousePosition(450, mode_row + 94);
    KryonInjectMouseButton(0, 0);
    step();
    step();
    check_int("Dark selectable with wobbled click", mode_sel, THEME_MODE_DARK);

    /* Same press through the style dropdown at the bottom, whose popup
     * may flip upward across the other fields. */
    style_sel = 0;
    tap(450, style_row);
    /* Aero is the fourth option: field bottom + gap + padding + 3.5 rows */
    tap(450, style_row + 15 + 4 + 4 + 105);
    check_int("Aero selectable from style popup", style_sel, THEME_STYLE_AERO);
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
