#ifndef UI_OVERLAY_H
#define UI_OVERLAY_H

#include "kryon_compat.generated.h"

typedef struct {
    Rectangle anchor;
    const char *text;
} UIGuideStep;

typedef struct {
    const UIGuideStep *steps;
    int count;
    int *step;
    int view_width;
    int view_height;
    int reserved_top;
    int reserved_bottom;
    int max_width;
    int line_gap;
    int paragraph_font;
    Texture2D close_icon;
    Texture2D back_icon;
    Texture2D next_icon;
    Texture2D done_icon;
} UIGuideOverlay;

typedef struct {
    int closed;
    int finished;
    int changed;
    int step;
} UIGuideResult;

typedef struct {
    int draw_source_menu;
    int draw_mode_menu;
    int draw_palette_menu;
    int draw_style_menu;
    int palette_index;
} UIThemeSettingsState;

typedef struct {
    int id_base;
    int x;
    int y;
    int w;
    int *theme_source;
    int *theme_mode;
    int *theme_id;
    int *theme_style;
    int allow_system_source;
    int allow_system_mode;
    const char *theme_label;
    const char *source_app_label;
    const char *source_system_label;
    const char *mode_label;
    const char *mode_system_label;
    const char *mode_light_label;
    const char *mode_dark_label;
    const char *palette_label;
    const char *style_label;
    const char *style_system_label;
    const char *style_retro_label;
    const char *style_material_label;
    const char *style_fluent_label;
    const char *style_adwaita_label;
    const char *style_liquid_glass_label;
    const char *style_aero_label;
    const char *system_theme_label;
} UIThemeSettings;

typedef struct {
    int changed;
    int source_changed;
    int mode_changed;
    int palette_changed;
    int style_changed;
} UIThemeSettingsResult;

int DrawUIThemeSettings(UIThemeSettings settings, UIThemeSettingsState *state);
UIThemeSettingsResult DrawUIThemeSettingsMenus(UIThemeSettings settings,
                                                 UIThemeSettingsState *state);

#endif
