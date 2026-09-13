#include "theme.h"
#include "runtime/theme.h"
#include "locale.h"
#include "theme_meta.h"
#include "ui_core.h"
#include <stdio.h>
#include <string.h>

float g_theme_content_alpha = 1.0f;

static bool dark_mode = false;
#if defined(KRYON_BACKEND_LIBDRAW)
#if defined(KRYON_PLATFORM_PLAN9)
static int current_theme_id = THEME_MONO;
#else
static int current_theme_id = THEME_MONO;
#endif
#if defined(KRYON_PLATFORM_PLAN9)
static ThemeSource theme_source = THEME_SOURCE_SYSTEM;
static ThemeMode theme_mode = THEME_MODE_SYSTEM;
#else
static ThemeSource theme_source = THEME_SOURCE_SYSTEM;
static ThemeMode theme_mode = THEME_MODE_SYSTEM;
#endif

#else
static int current_theme_id = THEME_MONO;
static ThemeSource theme_source = THEME_SOURCE_SYSTEM;
static ThemeMode theme_mode = THEME_MODE_SYSTEM;
#endif

static Theme active_theme;
static int default_theme_enabled = 1;
static int active_theme_set = 0;
static char active_theme_name[THEME_NAME_SIZE];
static ThemeFamily active_theme_family;
static int active_theme_family_set = 0;
static char active_theme_family_name[THEME_NAME_SIZE];
static char active_theme_light_name[THEME_NAME_SIZE];
static char active_theme_dark_name[THEME_NAME_SIZE];

bool SystemThemeColor(const char *key, Color *color);

void ResetTheme(void)
{
    active_theme_set = 0;
    active_theme_family_set = 0;
    ClearThemeMetricsOverride();
}

Color GetThemeColor(const char *scope_name, const char *key)
{
    Color catalog_color;
    if(GetThemeCatalogScopeColor(scope_name, key, &catalog_color))
        return catalog_color;

    fprintf(stderr, "warning: missing theme color: %s.%s, using fallback\n",
            scope_name != NULL ? scope_name : "(null)",
            key != NULL ? key : "(null)");

    if(key != NULL) {
        if(strstr(key, "background") != NULL || strstr(key, "paper") != NULL)
            return (Color){0xE2, 0xEE, 0xFC, 0xFF};
        if(strstr(key, "surface") != NULL || strstr(key, "modal") != NULL ||
           strstr(key, "panel") != NULL)
            return (Color){0xD4, 0xE4, 0xF5, 0xFF};
        if(strstr(key, "text") != NULL || strstr(key, "foreground") != NULL ||
           strstr(key, "ink") != NULL)
            return (Color){0x24, 0x48, 0x7C, 0xFF};
        if(strstr(key, "circle") != NULL || strstr(key, "selection") != NULL)
            return (Color){0x7E, 0xB7, 0xE6, 0xFF};
        if(strstr(key, "button_hover") != NULL || strstr(key, "face_hover") != NULL)
            return (Color){0x68, 0x9E, 0xD7, 0xFF};
        if(strstr(key, "button") != NULL || strstr(key, "face") != NULL)
            return (Color){0xA6, 0xCF, 0xF2, 0xFF};
        if(strstr(key, "icon") != NULL)
            return (Color){0xE2, 0xEE, 0xFC, 0xFF};
        if(strstr(key, "link") != NULL)
            return (Color){0x4A, 0x90, 0xE2, 0xFF};
    }

    return (Color){0xE2, 0xEE, 0xFC, 0xFF};
}

void DrawThemeTKBorder(Rectangle rec, int borderWidth, bool raised)
{
    Color highlight = GetThemeColor(NULL, "border_light");
    Color shadow = GetThemeColor(NULL, "border_shadow");
    Color topLeft = raised ? highlight : shadow;
    Color bottomRight = raised ? shadow : highlight;

    int x = (int)rec.x;
    int y = (int)rec.y;
    int w = (int)rec.width;
    int h = (int)rec.height;

    DrawRectangle(x, y, w, borderWidth, topLeft);
    DrawRectangle(x, y, borderWidth, h, topLeft);
    DrawRectangle(x, y + h - borderWidth, w, borderWidth, bottomRight);
    DrawRectangle(x + w - borderWidth, y, borderWidth, h, bottomRight);
}

void SetThemeDarkMode(bool dark)
{
    SetThemeMode(dark ? THEME_MODE_DARK : THEME_MODE_LIGHT);
}

bool GetThemeDarkMode(void)
{
    return GetEffectiveThemeDarkMode();
}

void
SetThemeSource(ThemeSource source)
{
    default_theme_enabled = 0;
    if(source != THEME_SOURCE_SYSTEM)
        source = THEME_SOURCE_APP;
    theme_source = source;
}

ThemeSource
GetThemeSource(void)
{
    return theme_source;
}

void
SetThemeMode(ThemeMode mode)
{
    if(mode < THEME_MODE_SYSTEM || mode > THEME_MODE_DARK)
        mode = THEME_MODE_SYSTEM;
    theme_mode = mode;
    dark_mode = ResolveDark((int32_t)mode, SystemThemePrefersDark());
    if(active_theme_family_set) {
        Theme selected = dark_mode ? active_theme_family.dark
                                   : active_theme_family.light;
        snprintf(active_theme_name, sizeof(active_theme_name), "%s",
                 selected.name != NULL ? selected.name : "Theme");
        active_theme = selected;
        active_theme.name = active_theme_name;
        active_theme.mode = dark_mode ? THEME_MODE_DARK : THEME_MODE_LIGHT;
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

ThemeSource
GetDefaultPlatformThemeSource(void)
{
    /* System theme is the default everywhere. Platforms without a readable
       system palette (e.g. Android before the app feeds device dark mode)
       gracefully fall back to the app palette in GetCurrentThemeColor. */
    return THEME_SOURCE_SYSTEM;
}

ThemeMode
GetDefaultPlatformThemeMode(void)
{
    if(GetDefaultPlatformThemeSource() == THEME_SOURCE_SYSTEM)
        return THEME_MODE_SYSTEM;
    return THEME_MODE_LIGHT;
}

bool
GetEffectiveThemeDarkMode(void)
{
    if(theme_mode == THEME_MODE_LIGHT)
        return false;
    if(theme_mode == THEME_MODE_DARK)
        return true;
    return SystemThemePrefersDark();
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

Color
GetCurrentThemeColor(const char *key)
{
    Color color = BLANK;
    const Theme *resolved = GetThemeRef();
    if(resolved != NULL) {
        const ThemeColors *colors = &resolved->colors;
        if(strcmp(key, "background") == 0) color = colors->background;
        else if(strcmp(key, "surface") == 0) color = colors->surface;
        else if(strcmp(key, "text") == 0) color = colors->text;
        else if(strcmp(key, "circle") == 0) color = colors->accent;
        else if(strcmp(key, "button") == 0) color = colors->accent;
        else if(strcmp(key, "button_hover") == 0) color = colors->accent_hover;
        else if(strcmp(key, "icon") == 0) color = colors->icon;
        else if(strcmp(key, "link") == 0) color = colors->link;
        else if(strcmp(key, "border") == 0) color = colors->border;
        else if(strcmp(key, "text_muted") == 0) color = colors->text_muted;
        else if(strcmp(key, "selection") == 0) color = colors->selection;
        else if(strcmp(key, "on_accent") == 0) color = colors->on_accent;
        goto apply_alpha;
    }
    if(theme_source == THEME_SOURCE_SYSTEM) {
        SetSystemThemeDarkMode(GetEffectiveThemeDarkMode());
        if(SystemThemeColor(key, &color))
            goto apply_alpha;
    }
    GetThemeCatalogColor(NormalizeTheme(current_theme_id),
                              GetEffectiveThemeDarkMode(), key, &color);
apply_alpha:
    color.a = (unsigned char)((float)color.a * g_theme_content_alpha + 0.5f);
    return color;
}

static Theme
theme_default(int dark)
{
    Theme theme;
    Palette palette = DefaultPalette(dark != 0);
    memset(&theme, 0, sizeof(theme));
    theme.name = dark ? "Default dark" : "Default light";
    theme.mode = dark ? THEME_MODE_DARK : THEME_MODE_LIGHT;
    theme.metrics = GetDefaultThemeMetrics();
    theme.colors.background = GetColor(palette.background);
    theme.colors.surface = GetColor(palette.surface);
    theme.colors.surface_raised = GetColor(palette.surface_raised);
    theme.colors.surface_sunken = GetColor(palette.surface_sunken);
    theme.colors.overlay = GetColor(palette.overlay);
    theme.colors.text = GetColor(palette.text);
    theme.colors.text_muted = GetColor(palette.text_muted);
    theme.colors.text_disabled = GetColor(palette.text_disabled);
    theme.colors.icon = GetColor(palette.icon);
    theme.colors.icon_muted = GetColor(palette.icon_muted);
    theme.colors.border = GetColor(palette.border);
    theme.colors.border_strong = GetColor(palette.border_strong);
    theme.colors.divider = GetColor(palette.divider);
    theme.colors.focus = GetColor(palette.focus);
    theme.colors.selection = GetColor(palette.selection);
    theme.colors.accent = GetColor(palette.accent);
    theme.colors.on_accent = GetColor(palette.on_accent);
    theme.colors.accent_hover = GetColor(palette.accent_hover);
    theme.colors.accent_pressed = GetColor(palette.accent_pressed);
    theme.colors.success = GetColor(palette.success);
    theme.colors.on_success = GetColor(palette.on_success);
    theme.colors.warning = GetColor(palette.warning);
    theme.colors.on_warning = GetColor(palette.on_warning);
    theme.colors.danger = GetColor(palette.danger);
    theme.colors.on_danger = GetColor(palette.on_danger);
    theme.colors.info = GetColor(palette.info);
    theme.colors.on_info = GetColor(palette.on_info);
    theme.colors.link = GetColor(palette.link);
    theme.colors.link_hover = GetColor(palette.link_hover);
    theme.colors.shadow = GetColor(palette.shadow);
    return theme;
}

Theme ThemeDefaultLight(void) { return theme_default(0); }
Theme ThemeDefaultDark(void) { return theme_default(1); }

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
    theme_mode = theme.mode == THEME_MODE_DARK
        ? THEME_MODE_DARK : THEME_MODE_LIGHT;
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
        return (ThemeFamily){.name = "Default", .light = ThemeDefaultLight(), .dark = ThemeDefaultDark()};
    memset(&family, 0, sizeof(family));
    return family;
}

Theme GetTheme(void) { return active_theme_set ? active_theme : theme_default(GetEffectiveThemeDarkMode()); }
const Theme *GetThemeRef(void)
{
    static Theme defaults;
    if(active_theme_set)
        return &active_theme;
    if(!default_theme_enabled)
        return NULL;
    defaults = theme_default(GetEffectiveThemeDarkMode());
    return &defaults;
}

static int
theme_luminance(Color color)
{
    return ((int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114) / 1000;
}

static unsigned char
theme_mix_channel(unsigned char from, unsigned char to, float amount)
{
    float value;

    if(amount < 0.0f)
        amount = 0.0f;
    if(amount > 1.0f)
        amount = 1.0f;
    value = (float)from + ((float)to - (float)from) * amount;
    if(value < 0.0f)
        value = 0.0f;
    if(value > 255.0f)
        value = 255.0f;
    return (unsigned char)(value + 0.5f);
}

Color
MixThemeColor(Color from, Color to, float amount)
{
    return (Color){
        theme_mix_channel(from.r, to.r, amount),
        theme_mix_channel(from.g, to.g, amount),
        theme_mix_channel(from.b, to.b, amount),
        theme_mix_channel(from.a, to.a, amount)
    };
}

int
GetThemeColorLuminance(Color color)
{
    return theme_luminance(color);
}

bool
IsThemeColorDark(Color color)
{
    return theme_luminance(color) < 128;
}

Color
GetThemeReadableText(Color background)
{
    return IsThemeColorDark(background) ? RAYWHITE : BLACK;
}

static Color
theme_readable_semantic_color(const char *key, const char *surface_key)
{
    Color color = GetCurrentThemeColor(key);
    Color surface = GetCurrentThemeColor(surface_key);
    int delta = theme_luminance(color) - theme_luminance(surface);

    if(delta < 0)
        delta = -delta;
    if(delta < 72) {
        Color text = GetCurrentThemeColor("text");
        text.a = color.a != 0 ? color.a : 255;
        return text;
    }
    return color;
}

Color GetThemeText(void) { return GetCurrentThemeColor("text"); }
Color GetThemeBackground(void) { return GetCurrentThemeColor("background"); }
Color GetThemeSurface(void) { return GetCurrentThemeColor("surface"); }
Color GetThemeCircle(void) { return GetCurrentThemeColor("circle"); }
Color GetThemeButton(void) { return GetCurrentThemeColor("button"); }
Color GetThemeButtonHover(void) { return GetCurrentThemeColor("button_hover"); }
Color
GetThemeIcon(void)
{
    if(GetThemeRef() != NULL)
        return GetCurrentThemeColor("icon");
    return theme_readable_semantic_color("icon", "surface");
}
Color GetThemeLink(void) { return GetCurrentThemeColor("link"); }

Color
GetThemeSurfaceAlt(void)
{
    return MixThemeColor(GetThemeSurface(), GetThemeBackground(),
                         IsThemeColorDark(GetThemeBackground()) ? 0.20f : 0.42f);
}

Color
GetThemeBorder(void)
{
    if(GetThemeRef() != NULL)
        return GetCurrentThemeColor("border");
    return MixThemeColor(GetThemeText(), GetThemeSurface(),
                         IsThemeColorDark(GetThemeBackground()) ? 0.70f : 0.80f);
}

Color
GetThemeMutedText(void)
{
    if(GetThemeRef() != NULL)
        return GetCurrentThemeColor("text_muted");
    return MixThemeColor(GetThemeText(), GetThemeSurface(),
                         IsThemeColorDark(GetThemeBackground()) ? 0.36f : 0.48f);
}

Color
GetThemeSelection(void)
{
    if(GetThemeRef() != NULL)
        return GetCurrentThemeColor("selection");
    return MixThemeColor(GetThemeSurface(), GetThemeButton(),
                         IsThemeColorDark(GetThemeBackground()) ? 0.26f : 0.16f);
}

Color
GetThemeButtonText(void)
{
    if(GetThemeRef() != NULL)
        return GetCurrentThemeColor("on_accent");
    return GetThemeReadableText(GetThemeButton());
}
