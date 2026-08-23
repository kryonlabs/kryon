/*
 * plan9_ui_globals.c - definitions for the globals the theme core pulls in
 * from UI translation units that the native Plan 9 library build currently
 * excludes (the widget layer) plus the generated embedded-asset tables.
 *
 * The theme engine (theme.c, theme_meta.c, ui_style.c, ui_color.c) keeps
 * its state in these shared globals; until the full widget layer is ported
 * to the native Plan 9 compiler, the definitions live here so the theme
 * core links standalone. Hosted builds compile the real ui.c instead, so
 * this whole file is inert unless building for native Plan 9.
 */
#if defined(KRYON_PLATFORM_PLAN9) || defined(KRYON_NATIVE_PLAN9)

#include "../../ui/ui_internal.h"
#include "embedded_assets.h"

/* ui.c */
Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover,
    c_icon, c_link;
unsigned long g_ui_frame_serial = 0;

/* ui_dpi.c */
UIDPIState ui_dpi_state;

/* build-generated embedded_asset_data.c (UI_EMBEDDED_ONLY=0 build) */
const EmbeddedAsset embedded_assets[] = {{0, 0, 0, 0}};
const unsigned int embedded_asset_count = 0;

/* ui.c: ApplyCurrentUITheme, inlined here for the theme-core-only build.
 * It mirrors ui.c: ui_set_theme_colors(...) plus the link color. */
void ApplyCurrentUITheme(void);
void
ApplyCurrentUITheme(void)
{
    c_text = GetThemeText();
    c_bg = GetThemeBackground();
    c_surface = GetThemeSurface();
    c_circle = GetThemeCircle();
    c_button = GetThemeButton();
    c_button_hover = GetThemeButtonHover();
    c_icon = GetThemeIcon();
    c_link = GetThemeLink();
}

/* ui.c: mouse position helper; without the widget layer there is no live
 * pointer state, and the material ripple (the only consumer) never runs
 * from the theme core. */
Vector2
ui_mouse_world(void)
{
    Vector2 zero = {0, 0};

    return zero;
}

#endif /* KRYON_PLATFORM_PLAN9 */
