#include "flint_theme_meta.h"
#include "flint_theme.h"

#include <stdio.h>
#include <string.h>

const FlintThemeMeta flint_themes[FLINT_THEME_COUNT] = {
    [FLINT_THEME_SKY] = {"Sky", "sky_light", "sky_dark", "themes/sky.ini", "themes/sky_dark.ini"},
    [FLINT_THEME_OCEAN] = {"Ocean", "ocean_light", "ocean_dark", "themes/ocean.ini", "themes/ocean_dark.ini"},
    [FLINT_THEME_FOREST] = {"Forest", "forest_light", "forest_dark", "themes/forest.ini", "themes/forest_dark.ini"},
    [FLINT_THEME_SUNSET] = {"Sunset", "sunset_light", "sunset_dark", "themes/sunset.ini", "themes/sunset_dark.ini"},
    [FLINT_THEME_LAVENDER] = {"Lavender", "lavender_light", "lavender_dark", "themes/lavender.ini", "themes/lavender_dark.ini"},
    [FLINT_THEME_CHERRY] = {"Cherry", "cherry_light", "cherry_dark", "themes/cherry.ini", "themes/cherry_dark.ini"}
};

FlintThemeId
flint_theme_normalize(int theme)
{
    if(theme < 0 || theme >= FLINT_THEME_COUNT)
        return FLINT_THEME_SKY;
    return (FlintThemeId)theme;
}

const FlintThemeMeta *
flint_theme_meta(FlintThemeId theme)
{
    return &flint_themes[flint_theme_normalize(theme)];
}

const char *
flint_theme_label(FlintThemeId theme)
{
    return flint_theme_meta(theme)->name;
}

const char *
flint_theme_scope_for(FlintThemeId theme, bool dark_mode)
{
    const FlintThemeMeta *meta = flint_theme_meta(theme);
    return dark_mode ? meta->dark_scope : meta->light_scope;
}

void
flint_theme_register_defaults(const char *theme_dir)
{
    char light_path[FLINT_THEME_PATH_SIZE];
    char dark_path[FLINT_THEME_PATH_SIZE];

    for(int i = 0; i < FLINT_THEME_COUNT; i++) {
        const FlintThemeMeta *meta = &flint_themes[i];

        if(theme_dir != NULL && theme_dir[0] != '\0') {
            const char *light_name = strrchr(meta->light_path, '/');
            const char *dark_name = strrchr(meta->dark_path, '/');
            light_name = light_name != NULL ? light_name + 1 : meta->light_path;
            dark_name = dark_name != NULL ? dark_name + 1 : meta->dark_path;
            snprintf(light_path, sizeof(light_path), "%s/%s", theme_dir, light_name);
            snprintf(dark_path, sizeof(dark_path), "%s/%s", theme_dir, dark_name);
        } else {
            snprintf(light_path, sizeof(light_path), "%s", meta->light_path);
            snprintf(dark_path, sizeof(dark_path), "%s", meta->dark_path);
        }

        flint_theme_register_scope_dark(meta->light_scope, light_path, dark_path);
        flint_theme_register_scope_dark(meta->dark_scope, dark_path, dark_path);
    }
}
