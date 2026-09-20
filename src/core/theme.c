#include "theme.h"
#include "ui/theme_color.h"
#include "runtime/theme.h"
#include "locale.h"
#include "theme_meta.h"
#include "ui_core.h"
#include <stdio.h>
#include <string.h>

float g_theme_content_alpha = 1.0f;

static bool dark_mode = false;
static int current_theme_id = THEME_MONO;
static ThemeSource theme_source = THEME_SOURCE_SYSTEM;
static ThemeMode theme_mode = THEME_MODE_SYSTEM;

static Theme active_theme;
static int default_theme_enabled = 1;
static int active_theme_set = 0;
static char active_theme_name[THEME_NAME_SIZE];
static ThemeFamily active_theme_family;
static int active_theme_family_set = 0;
static char active_theme_family_name[THEME_NAME_SIZE];
static char active_theme_light_name[THEME_NAME_SIZE];
static char active_theme_dark_name[THEME_NAME_SIZE];

int
ThemeStateCurrentId(void)
{
    return current_theme_id;
}

float
ThemeStateContentAlpha(void)
{
    return g_theme_content_alpha;
}

void ResetTheme(void)
{
    active_theme_set = 0;
    active_theme_family_set = 0;
    ClearThemeMetricsOverride();
}

void
SetThemeSource(ThemeSource source)
{
    default_theme_enabled = 0;
    theme_source = NormalizeThemeSource(source);
}

ThemeSource
GetThemeSource(void)
{
    return theme_source;
}

void
SetThemeMode(ThemeMode mode)
{
    mode = NormalizeThemeMode(mode);
    theme_mode = mode;
    dark_mode = ResolveDark((ThemePolicy)mode, SystemThemePrefersDark());
    if(active_theme_family_set) {
        Theme selected = ThemeFamilySelection(active_theme_family, dark_mode);
        snprintf(active_theme_name, sizeof(active_theme_name), "%s",
                 selected.name != NULL ? selected.name : "Theme");
        active_theme = selected;
        active_theme.name = active_theme_name;
        active_theme_set = 1;
        SetThemeMetrics(active_theme.metrics);
    }
    ApplyCurrentTheme();
}

ThemeMode
GetThemeMode(void)
{
    return theme_mode;
}

void
SetCurrentTheme(int theme_id, int current_dark_mode)
{
    default_theme_enabled = 0;
    active_theme_family_set = 0;
    active_theme_set = 0;
    current_theme_id = NormalizeTheme(theme_id);
    dark_mode = current_dark_mode != 0;
    ApplyCurrentTheme();
}

void
SetTheme(Theme theme)
{
    default_theme_enabled = 0;
    active_theme_family_set = 0;
    snprintf(active_theme_name, sizeof(active_theme_name), "%s",
             theme.name != NULL ? theme.name : "Theme");
    active_theme = theme;
    active_theme.name = active_theme_name;
    active_theme_set = 1;
    theme_mode = ThemeModeFor(theme);
    SetThemeMetrics(theme.metrics);
    ApplyCurrentTheme();
}

void
SetThemeFamily(ThemeFamily family)
{
    default_theme_enabled = 0;
    snprintf(active_theme_family_name, sizeof(active_theme_family_name), "%s",
             family.name != NULL ? family.name : "Theme family");
    snprintf(active_theme_light_name, sizeof(active_theme_light_name), "%s",
             family.light.name != NULL ? family.light.name : "Light");
    snprintf(active_theme_dark_name, sizeof(active_theme_dark_name), "%s",
             family.dark.name != NULL ? family.dark.name : "Dark");
    active_theme_family = family;
    active_theme_family.name = active_theme_family_name;
    active_theme_family.light.name = active_theme_light_name;
    active_theme_family.dark.name = active_theme_dark_name;
    active_theme_family.light.mode = THEME_MODE_LIGHT;
    active_theme_family.dark.mode = THEME_MODE_DARK;
    active_theme_family_set = 1;
    SetThemeMode(GetEffectiveThemeDarkMode() ? THEME_MODE_DARK
                                             : THEME_MODE_LIGHT);
}

ThemeFamily
GetThemeFamily(void)
{
    ThemeFamily family;
    if(active_theme_family_set)
        return active_theme_family;
    if(default_theme_enabled)
        return DefaultThemeFamily();
    memset(&family, 0, sizeof(family));
    return family;
}

Theme GetTheme(void)
{
    if(active_theme_set)
        return active_theme;
    return GetEffectiveThemeDarkMode() ? ThemeDefaultDark() : ThemeDefaultLight();
}
const Theme *GetThemeRef(void)
{
    static Theme defaults;
    if(active_theme_set)
        return &active_theme;
    if(!default_theme_enabled)
        return NULL;
    defaults = GetEffectiveThemeDarkMode() ? ThemeDefaultDark() : ThemeDefaultLight();
    return &defaults;
}
