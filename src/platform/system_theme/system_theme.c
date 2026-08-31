#include "theme.h"
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#include <string.h>
#endif
#include "ui_color.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#undef ShowCursor
#undef CloseWindow
#undef Rectangle
#endif

#if defined(SYSTEM_THEME_GTK)
#include <gtk/gtk.h>
#endif

typedef struct SystemThemePalette {
    Color background;
    Color surface;
    Color text;
    Color circle;
    Color button;
    Color button_hover;
    Color icon;
    Color link;
    int available;
    int prefers_dark;
    int supports_mode;
    char name[THEME_NAME_SIZE];
} SystemThemePalette;

static SystemThemePalette system_palette = {
    .background = {0xF0, 0xF0, 0xF0, 0xFF},
    .surface = {0xE8, 0xE8, 0xE8, 0xFF},
    .text = {0x10, 0x10, 0x10, 0xFF},
    .circle = {0x4A, 0x90, 0xE2, 0xFF},
    .button = {0xDD, 0xDD, 0xDD, 0xFF},
    .button_hover = {0xC8, 0xD8, 0xEA, 0xFF},
    .icon = {0x10, 0x10, 0x10, 0xFF},
    .link = {0x20, 0x70, 0xC0, 0xFF},
    .available = 0,
    .prefers_dark = 0,
    .supports_mode = 0,
    .name = "System"
};

static SystemThemePalette system_light_palette;
static SystemThemePalette system_dark_palette;
static int system_prefers_dark = 0;
#if defined(KRYON_PLATFORM_PLAN9)
static ThemeStyle system_theme_style = THEME_STYLE_RETRO;
#else
static ThemeStyle system_theme_style = THEME_STYLE_SYSTEM;
#endif
static char system_ui_font_name[128];
static char system_ui_font_file[512];
static int system_ui_font_attempted = 0;

/* Automatic refresh discipline.
 *
 * IsSystemThemeAvailable/SystemThemePrefersDark/... answer questions asked
 * for every themed widget on every frame. When desktop detection fails (no
 * readable GTK theme CSS), an uncached failure re-ran the whole file-reading
 * probe on every single call — thousands of config-file reads per frame,
 * freezing the UI. The automatic path now attempts detection at most once
 * per retry window; the explicit RefreshSystemTheme() still forces a full
 * re-detection. */
#define SYSTEM_THEME_RETRY_S 30.0
static double system_theme_last_attempt_s = -1.0;
static int system_theme_clock_ok = 1;
static long system_theme_refresh_count = 0;

static double
system_theme_now_s(void)
{
#if defined(_WIN32)
    return (double)GetTickCount64() / 1000.0;
#elif defined(KRYON_PLATFORM_PLAN9)
    return (double)nsec() / 1.0e9;
#elif defined(PLATFORM_WEB)
    return 0.0;
#else
    struct timespec ts;

    if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        system_theme_clock_ok = 0;
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1.0e9;
#endif
}

static void
system_theme_auto_refresh(void)
{
    double now;

#if !defined(_WIN32)
    if(system_palette.available)
        return;
#endif
    if(!system_theme_clock_ok) {
        /* No monotonic clock: attempt exactly once, never per-frame. */
        if(system_theme_last_attempt_s >= 0.0)
            return;
        system_theme_last_attempt_s = 0.0;
        RefreshSystemTheme();
        return;
    }
    now = system_theme_now_s();
    if(system_theme_last_attempt_s >= 0.0 &&
       now - system_theme_last_attempt_s < SYSTEM_THEME_RETRY_S)
        return;
    system_theme_last_attempt_s = now;
    RefreshSystemTheme();
}

static const SystemThemePalette material_light_palette = {
    .background = {0xFF, 0xFB, 0xFE, 0xFF},
    .surface = {0xF7, 0xF2, 0xFA, 0xFF},
    .text = {0x1D, 0x1B, 0x20, 0xFF},
    .circle = {0x67, 0x50, 0xA4, 0xFF},
    .button = {0xE7, 0xE0, 0xEC, 0xFF},
    .button_hover = {0xD0, 0xBC, 0xFF, 0xFF},
    .icon = {0x1D, 0x1B, 0x20, 0xFF},
    .link = {0x67, 0x50, 0xA4, 0xFF},
    .available = 1,
    .prefers_dark = 0,
    .name = "Material"
};

static const SystemThemePalette material_dark_palette = {
    .background = {0x14, 0x12, 0x18, 0xFF},
    .surface = {0x21, 0x1F, 0x26, 0xFF},
    .text = {0xE6, 0xE0, 0xE9, 0xFF},
    .circle = {0xD0, 0xBC, 0xFF, 0xFF},
    .button = {0x4A, 0x44, 0x58, 0xFF},
    .button_hover = {0x67, 0x50, 0xA4, 0xFF},
    .icon = {0xE6, 0xE0, 0xE9, 0xFF},
    .link = {0xD0, 0xBC, 0xFF, 0xFF},
    .available = 1,
    .prefers_dark = 1,
    .name = "Material"
};

static void
apply_material_palette(bool dark)
{
    system_palette = dark ? material_dark_palette : material_light_palette;
    system_palette.supports_mode = 1;
    system_prefers_dark = dark ? 1 : 0;
    system_light_palette = material_light_palette;
    system_dark_palette = material_dark_palette;
    system_light_palette.supports_mode = 1;
    system_dark_palette.supports_mode = 1;
}

#if defined(_WIN32)
static int
windows_system_theme_refresh(void)
{
    static const WCHAR personalize_key[] =
        L"Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Themes\\\\Personalize";
    DWORD use_light = 1;
    DWORD size = sizeof(use_light);
    LSTATUS status;

    status = RegGetValueW(HKEY_CURRENT_USER, personalize_key,
                          L"AppsUseLightTheme", RRF_RT_REG_DWORD,
                          NULL, &use_light, &size);
    if(status != ERROR_SUCCESS) {
        size = sizeof(use_light);
        status = RegGetValueW(HKEY_CURRENT_USER, personalize_key,
                              L"SystemUsesLightTheme", RRF_RT_REG_DWORD,
                              NULL, &use_light, &size);
    }
    if(status != ERROR_SUCCESS)
        return 0;
    apply_material_palette(use_light == 0);
    snprintf(system_palette.name, sizeof(system_palette.name), "Windows");
    snprintf(system_light_palette.name, sizeof(system_light_palette.name), "Windows");
    snprintf(system_dark_palette.name, sizeof(system_dark_palette.name), "Windows");
    return 1;
}
#endif

#if defined(SYSTEM_THEME_GTK)
static Color
color_from_unit(double r, double g, double b, double a)
{
    Color color;
    color.r = (unsigned char)(r <= 0.0 ? 0 : r >= 1.0 ? 255 : r * 255.0 + 0.5);
    color.g = (unsigned char)(g <= 0.0 ? 0 : g >= 1.0 ? 255 : g * 255.0 + 0.5);
    color.b = (unsigned char)(b <= 0.0 ? 0 : b >= 1.0 ? 255 : b * 255.0 + 0.5);
    color.a = (unsigned char)(a <= 0.0 ? 0 : a >= 1.0 ? 255 : a * 255.0 + 0.5);
    return color;
}

static Color
color_from_gdk(GdkRGBA color)
{
    return color_from_unit(color.red, color.green, color.blue, color.alpha);
}

static Color
style_color_for(GtkStyleContext *context, GtkStateFlags state, Color fallback)
{
    GdkRGBA rgba;

    if(context == NULL)
        return fallback;
    gtk_style_context_get_color(context, state, &rgba);
    return color_from_gdk(rgba);
}

static Color
style_background_for(GtkStyleContext *context, GtkStateFlags state, Color fallback)
{
    GdkRGBA rgba;

    if(context == NULL)
        return fallback;
    gtk_style_context_get_background_color(context, state, &rgba);
    if(rgba.alpha <= 0.0)
        return fallback;
    return color_from_gdk(rgba);
}

static int
color_channel_delta(unsigned char a, unsigned char b)
{
    return a > b ? a - b : b - a;
}

static int
color_delta(Color a, Color b)
{
    return color_channel_delta(a.r, b.r) +
           color_channel_delta(a.g, b.g) +
           color_channel_delta(a.b, b.b);
}

static int
palette_colors_differ(SystemThemePalette a, SystemThemePalette b)
{
    int background_delta = color_delta(a.background, b.background);
    int surface_delta = color_delta(a.surface, b.surface);
    int text_delta = color_delta(a.text, b.text);
    int button_delta = color_delta(a.button, b.button);

    return background_delta + surface_delta + text_delta + button_delta >= 96;
}

static int
gtk_sample_palette(GtkSettings *settings, gboolean prefer_dark,
                   const char *theme_name, SystemThemePalette *out)
{
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *entry;
    Color bg;
    Color surface;
    Color view_bg;
    Color text;
    Color button_bg;
    Color button_hover;
    Color accent;

    if(settings == NULL || out == NULL)
        return 0;

    g_object_set(settings,
                 "gtk-application-prefer-dark-theme", prefer_dark,
                 NULL);
    while(gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    label = gtk_label_new("kryon");
    button = gtk_button_new_with_label("kryon");
    entry = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(window), box);
    gtk_container_add(GTK_CONTAINER(box), label);
    gtk_container_add(GTK_CONTAINER(box), button);
    gtk_container_add(GTK_CONTAINER(box), entry);
    gtk_widget_realize(window);
    gtk_widget_realize(box);
    gtk_widget_realize(label);
    gtk_widget_realize(button);
    gtk_widget_realize(entry);

    bg = style_background_for(gtk_widget_get_style_context(window), GTK_STATE_FLAG_NORMAL,
                              system_palette.background);
    surface = style_background_for(gtk_widget_get_style_context(box), GTK_STATE_FLAG_NORMAL,
                                   bg);
    view_bg = style_background_for(gtk_widget_get_style_context(entry), GTK_STATE_FLAG_NORMAL,
                                   bg);
    text = style_color_for(gtk_widget_get_style_context(label), GTK_STATE_FLAG_NORMAL,
                           system_palette.text);
    button_bg = style_background_for(gtk_widget_get_style_context(button), GTK_STATE_FLAG_NORMAL,
                                     surface);
    button_hover = style_background_for(gtk_widget_get_style_context(button), GTK_STATE_FLAG_PRELIGHT,
                                        LightenUIColor(button_bg, 18));
    accent = button_hover;
    if(accent.r == button_bg.r && accent.g == button_bg.g && accent.b == button_bg.b)
        accent = LightenUIColor(button_bg, 24);

    *out = system_palette;
    out->background = view_bg;
    out->surface = surface;
    out->text = text;
    out->button = button_bg;
    out->button_hover = button_hover;
    out->circle = accent;
    out->icon = text;
    out->link = accent;
    out->prefers_dark = prefer_dark ? 1 : 0;
    out->available = 1;
    snprintf(out->name, sizeof(out->name), "%s",
             theme_name != NULL && theme_name[0] != '\0' ? theme_name : "GTK");

    gtk_widget_destroy(window);

    while(gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    return 1;
}

static int
gtk_system_theme_refresh(void)
{
    static int initialized = 0;
    static int init_ok = 0;
    GtkSettings *settings;
    char *theme_name = NULL;
    gboolean prefer_dark = FALSE;

    if(initialized)
        return init_ok && system_palette.available != 0;
    initialized = 1;
    init_ok = gtk_init_check(NULL, NULL) ? 1 : 0;
    if(!init_ok)
        return 0;

    settings = gtk_settings_get_default();
    if(settings == NULL)
        return 0;

    g_object_get(settings,
                 "gtk-theme-name", &theme_name,
                 "gtk-application-prefer-dark-theme", &prefer_dark,
                 NULL);

    if(!gtk_sample_palette(settings, FALSE, theme_name, &system_light_palette) ||
       !gtk_sample_palette(settings, TRUE, theme_name, &system_dark_palette)) {
        g_object_set(settings,
                     "gtk-application-prefer-dark-theme", prefer_dark,
                     NULL);
        g_free(theme_name);
        return 0;
    }

    system_light_palette.supports_mode =
        palette_colors_differ(system_light_palette, system_dark_palette);
    system_dark_palette.supports_mode = system_light_palette.supports_mode;
    system_palette = prefer_dark ? system_dark_palette : system_light_palette;
    system_palette.prefers_dark = prefer_dark ? 1 : 0;
    system_prefers_dark = prefer_dark ? 1 : 0;

    g_object_set(settings,
                 "gtk-application-prefer-dark-theme", prefer_dark,
                 NULL);
    g_free(theme_name);

    while(gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    return 1;
}
#endif

static void
copy_path(char *out, int out_size, const char *path, int len)
{
    if(out == NULL || out_size <= 0)
        return;
    if(path == NULL || len <= 0) {
        out[0] = '\0';
        return;
    }
    if(len >= out_size)
        len = out_size - 1;
    snprintf(out, (size_t)out_size, "%.*s", len, path);
}

static int
path_exists(const char *path)
{
    FILE *file;

    if(path == NULL || path[0] == '\0')
        return 0;
    file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    fclose(file);
    return 1;
}

static int
join_path_literal(char *out, int out_size, const char *base, const char *suffix)
{
    size_t base_len;
    size_t suffix_len;

    if(out == NULL || out_size <= 0 || base == NULL || suffix == NULL)
        return 0;
    base_len = strlen(base);
    suffix_len = strlen(suffix);
    if(base_len + suffix_len >= (size_t)out_size) {
        out[0] = '\0';
        return 0;
    }
    memcpy(out, base, base_len);
    memcpy(out + base_len, suffix, suffix_len + 1);
    return 1;
}

static int
join_gtk_theme_css_path(char *out, int out_size, const char *root,
                        const char *theme)
{
    const char *middle = "/gtk-3.0/gtk.css";
    size_t root_len;
    size_t theme_len;
    size_t middle_len;

    if(out == NULL || out_size <= 0 || root == NULL || theme == NULL)
        return 0;
    root_len = strlen(root);
    theme_len = strlen(theme);
    middle_len = strlen(middle);
    if(root_len + 1 + theme_len + middle_len >= (size_t)out_size) {
        out[0] = '\0';
        return 0;
    }
    memcpy(out, root, root_len);
    out[root_len] = '/';
    memcpy(out + root_len + 1, theme, theme_len);
    memcpy(out + root_len + 1 + theme_len, middle, middle_len + 1);
    return 1;
}

static int
read_text_file(const char *path, char *out, int out_size)
{
    FILE *file;
    size_t n;

    if(path == NULL || out == NULL || out_size <= 1)
        return 0;
    file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    n = fread(out, 1, (size_t)out_size - 1, file);
    out[n] = '\0';
    fclose(file);
    return n > 0;
}

static int
xfce_desktop_config_path(char *out, int out_size)
{
    const char *config = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if(out == NULL || out_size <= 0)
        return 0;
    if(config != NULL && config[0] != '\0')
        snprintf(out, (size_t)out_size,
                 "%s/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml",
                 config);
    else if(home != NULL && home[0] != '\0')
        snprintf(out, (size_t)out_size,
                 "%s/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml",
                 home);
    else
        return 0;
    return out[0] != '\0';
}

static int
xsettings_config_path(char *out, int out_size)
{
    const char *config = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if(out == NULL || out_size <= 0)
        return 0;
    if(config != NULL && config[0] != '\0')
        snprintf(out, (size_t)out_size,
                 "%s/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml",
                 config);
    else if(home != NULL && home[0] != '\0')
        snprintf(out, (size_t)out_size,
                 "%s/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml",
                 home);
    else
        return 0;
    return out[0] != '\0';
}

static int
gtk_settings_config_path(char *out, int out_size)
{
    const char *config = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if(out == NULL || out_size <= 0)
        return 0;
    if(config != NULL && config[0] != '\0')
        snprintf(out, (size_t)out_size, "%s/gtk-3.0/settings.ini", config);
    else if(home != NULL && home[0] != '\0')
        snprintf(out, (size_t)out_size, "%s/.config/gtk-3.0/settings.ini", home);
    else
        return 0;
    return out[0] != '\0';
}

static int
find_xml_value(const char *tag, char *out, int out_size)
{
    const char *value;
    const char *end;
    const char *tag_end;

    if(tag == NULL)
        return 0;
    tag_end = strchr(tag, '>');
    if(tag_end == NULL)
        return 0;
    value = strstr(tag, "value=\"");
    if(value == NULL || value > tag_end)
        return 0;
    value += 7;
    end = strchr(value, '"');
    if(end == NULL || end > tag_end || end == value)
        return 0;
    copy_path(out, out_size, value, (int)(end - value));
    return out != NULL && out[0] != '\0';
}

static int
trim_setting_value(const char *src, char *out, int out_size)
{
    const char *start;
    const char *end;

    if(src == NULL || out == NULL || out_size <= 0)
        return 0;
    start = src;
    while(*start == ' ' || *start == '\t' || *start == '"')
        start++;
    end = start;
    while(*end != '\0' && *end != '\n' && *end != '\r')
        end++;
    while(end > start &&
          (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '"'))
        end--;
    copy_path(out, out_size, start, (int)(end - start));
    return out[0] != '\0';
}

static int
settings_ini_value(const char *text, const char *key, char *out, int out_size)
{
    const char *cursor;

    if(text == NULL || key == NULL || out == NULL || out_size <= 0)
        return 0;
    cursor = strstr(text, key);
    if(cursor == NULL)
        return 0;
    cursor = strchr(cursor, '=');
    if(cursor == NULL)
        return 0;
    return trim_setting_value(cursor + 1, out, out_size);
}

static int shell_quote_arg(char *out, int out_size, const char *value);

static int
xfconf_query_value(const char *property, char *out, int out_size)
{
#if !defined(_WIN32) && !defined(PLATFORM_WEB) && !defined(KRYON_PLATFORM_PLAN9)
    char quoted[160];
    char command[256];
    FILE *pipe;
    char line[256];

    if(property == NULL || property[0] == '\0' || out == NULL || out_size <= 0)
        return 0;
    if(!shell_quote_arg(quoted, sizeof(quoted), property))
        return 0;
    snprintf(command, sizeof(command),
             "xfconf-query -c xsettings -p %s 2>/dev/null", quoted);
    pipe = popen(command, "r");
    if(pipe == NULL)
        return 0;
    if(fgets(line, sizeof(line), pipe) == NULL) {
        pclose(pipe);
        return 0;
    }
    pclose(pipe);
    return trim_setting_value(line, out, out_size);
#else
    (void)property;
    (void)out;
    (void)out_size;
    return 0;
#endif
}

static int
read_system_ui_font_name(char *out, int out_size)
{
    char path[512];
    char text[65536];
    const char *cursor;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';

    if(xsettings_config_path(path, sizeof(path)) &&
       read_text_file(path, text, sizeof(text))) {
        cursor = strstr(text, "name=\"FontName\"");
        if(cursor != NULL && find_xml_value(cursor, out, out_size))
            return 1;
    }

    if(xfconf_query_value("/Gtk/FontName", out, out_size))
        return 1;

    if(gtk_settings_config_path(path, sizeof(path)) &&
       read_text_file(path, text, sizeof(text)) &&
       settings_ini_value(text, "gtk-font-name", out, out_size))
        return 1;

    snprintf(out, (size_t)out_size, "%s", "Sans 10");
    return 1;
}

static int
shell_quote_arg(char *out, int out_size, const char *value)
{
    int n = 0;

    if(out == NULL || out_size <= 0 || value == NULL)
        return 0;
    if(n < out_size)
        out[n] = '\'';
    n++;
    for(const char *p = value; *p != '\0'; p++) {
        if(*p == '\'') {
            const char *esc = "'\\''";
            for(int i = 0; esc[i] != '\0'; i++) {
                if(n < out_size)
                    out[n] = esc[i];
                n++;
            }
        } else {
            if(n < out_size)
                out[n] = *p;
            n++;
        }
    }
    if(n < out_size)
        out[n] = '\'';
    n++;
    if(n >= out_size) {
        out[out_size - 1] = '\0';
        return 0;
    }
    out[n] = '\0';
    return 1;
}

static int
fontconfig_match_file(const char *font_name, char *out, int out_size)
{
#if !defined(_WIN32) && !defined(PLATFORM_WEB) && !defined(KRYON_PLATFORM_PLAN9)
    char quoted[256];
    char command[384];
    FILE *pipe;
    char line[512];

    if(font_name == NULL || font_name[0] == '\0' || out == NULL || out_size <= 0)
        return 0;
    if(!shell_quote_arg(quoted, sizeof(quoted), font_name))
        return 0;
    snprintf(command, sizeof(command), "fc-match -f '%%{file}\\n' %s", quoted);
    pipe = popen(command, "r");
    if(pipe == NULL)
        return 0;
    if(fgets(line, sizeof(line), pipe) == NULL) {
        pclose(pipe);
        return 0;
    }
    pclose(pipe);
    if(!trim_setting_value(line, out, out_size))
        return 0;
    return path_exists(out);
#else
    (void)font_name;
    (void)out;
    (void)out_size;
    return 0;
#endif
}

static int
refresh_system_ui_font(void)
{
    char name[sizeof(system_ui_font_name)];
    char file[sizeof(system_ui_font_file)];

    if(system_ui_font_attempted)
        return system_ui_font_file[0] != '\0';
    system_ui_font_attempted = 1;
    system_ui_font_name[0] = '\0';
    system_ui_font_file[0] = '\0';

    if(!read_system_ui_font_name(name, sizeof(name)))
        return 0;
    snprintf(system_ui_font_name, sizeof(system_ui_font_name), "%s", name);
    if(!fontconfig_match_file(name, file, sizeof(file)))
        return 0;
    snprintf(system_ui_font_file, sizeof(system_ui_font_file), "%s", file);
    return 1;
}

/*
 * --- GTK theme palette straight from the themes CSS ---
 *
 * The in-process GTK sampler only exists where GTK is linked in
 * (SYSTEM_THEME_GTK). Apps that keep GTK out of the binary (inbes
 * no-in-process-GTK policy) still deserve the desktops real colors, so the
 * theme is read the same way the wallpaper is: the XFCE xsettings channel
 * (or GTK_THEME, or gtk-3.0 settings.ini) names the GTK theme, and the
 * themes gtk-3.0/gtk.css @define-color lines carry the palette. Plain
 * hex / numeric / named colors are used; expressions (shade(), mix(),
 * @references) are skipped and fall back to the next candidate.
 */
static int
css_hex_value(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int
css_parse_hex(const char *s, const char *end, Color *out)
{
    int nib[8];
    int n = 0;

    s++; /* skip # */
    while(s < end && n < 8) {
        int v = css_hex_value(*s);

        if(v < 0)
            break;
        nib[n++] = v;
        s++;
    }
    if(n == 3) {
        out->r = (unsigned char)(nib[0] * 17);
        out->g = (unsigned char)(nib[1] * 17);
        out->b = (unsigned char)(nib[2] * 17);
    } else if(n == 6) {
        out->r = (unsigned char)(nib[0] * 16 + nib[1]);
        out->g = (unsigned char)(nib[2] * 16 + nib[3]);
        out->b = (unsigned char)(nib[4] * 16 + nib[5]);
    } else {
        return 0;
    }
    out->a = 255;
    return 1;
}

static int
css_parse_named(const char *s, const char *end, Color *out)
{
    static const struct {
        const char *name;
        Color color;
    } named[] = {
        {"black", {0x00, 0x00, 0x00, 0xFF}},
        {"white", {0xFF, 0xFF, 0xFF, 0xFF}},
        {"gray", {0x80, 0x80, 0x80, 0xFF}},
        {"grey", {0x80, 0x80, 0x80, 0xFF}},
        {"silver", {0xC0, 0xC0, 0xC0, 0xFF}},
        {"navy", {0x00, 0x00, 0x80, 0xFF}},
        {"red", {0xFF, 0x00, 0x00, 0xFF}},
        {"green", {0x00, 0x80, 0x00, 0xFF}},
        {"blue", {0x00, 0x00, 0xFF, 0xFF}},
        {"yellow", {0xFF, 0xFF, 0x00, 0xFF}},
        {"orange", {0xFF, 0xA5, 0x00, 0xFF}},
        {"purple", {0x80, 0x00, 0x80, 0xFF}},
        {"magenta", {0xFF, 0x00, 0xFF, 0xFF}},
        {"cyan", {0x00, 0xFF, 0xFF, 0xFF}},
    };
    size_t len = (size_t)(end - s);
    size_t i;

    for(i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
        if(strlen(named[i].name) == len &&
           strncmp(s, named[i].name, len) == 0) {
            *out = named[i].color;
            return 1;
        }
    }
    return 0;
}

static int
css_parse_value(const char *s, const char *end, Color *out)
{
    double d[4];
    int nd = 0;

    if(s == NULL || end == NULL || out == NULL || end <= s)
        return 0;
    while(s < end && (*s == ' ' || *s == '\t'))
        s++;
    if(s >= end)
        return 0;
    if(*s == '#')
        return css_parse_hex(s, end, out);

    /* Numeric form: "r g b [a]" with 0..1 doubles (GTK CSS style). */
    while(s < end && nd < 4) {
        char *num_end;
        double v = strtod(s, &num_end);

        if(num_end == s)
            break;
        d[nd++] = v;
        s = num_end;
        while(s < end && (*s == ' ' || *s == '\t'))
            s++;
    }
    if(nd >= 3) {
        double rgb[3];
        int i;

        for(i = 0; i < 3; i++)
            rgb[i] = d[i] <= 0.0 ? 0.0 : d[i] >= 1.0 ? 1.0 : d[i];
        out->r = (unsigned char)(rgb[0] * 255.0 + 0.5);
        out->g = (unsigned char)(rgb[1] * 255.0 + 0.5);
        out->b = (unsigned char)(rgb[2] * 255.0 + 0.5);
        out->a = 255;
        return 1;
    }
    return css_parse_named(s, end, out);
}

static int
css_define_color(const char *text, const char *name, Color *out)
{
    char pattern[96];
    const char *p;
    size_t plen;

    if(text == NULL || name == NULL)
        return 0;
    snprintf(pattern, sizeof(pattern), "@define-color %s", name);
    plen = strlen(pattern);
    p = text;
    while((p = strstr(p, pattern)) != NULL) {
        const char *v = p + plen;
        const char *semi;
        char c = *v;

        /* Reject longer names sharing this prefix (bg_color vs
         * bg_color_light). */
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-') {
            p += plen;
            continue;
        }
        while(*v == ' ' || *v == '\t')
            v++;
        semi = strchr(v, ';');
        if(semi == NULL)
            return 0;
        return css_parse_value(v, semi, out);
    }
    return 0;
}

static int
gtk_css_theme_name(char *out, int out_size)
{
    const char *env = getenv("GTK_THEME");
    char path[512];
    char text[65536];
    const char *cursor;
    const char *config = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';

    /* GTK_THEME="Theme[:variant]" overrides every desktop setting. */
    if(env != NULL && env[0] != '\0') {
        const char *colon = strchr(env, ':');

        copy_path(out, out_size, env,
                  colon != NULL ? (int)(colon - env) : (int)strlen(env));
        if(out[0] != '\0')
            return 1;
    }

    /* XFCE names the GTK theme in the xsettings channel. */
    if(config != NULL && config[0] != '\0')
        snprintf(path, sizeof(path),
                 "%s/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml", config);
    else if(home != NULL && home[0] != '\0')
        snprintf(path, sizeof(path),
                 "%s/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml",
                 home);
    else
        path[0] = '\0';
    if(path[0] != '\0' && read_text_file(path, text, sizeof(text))) {
        cursor = strstr(text, "name=\"ThemeName\"");
        if(cursor != NULL && find_xml_value(cursor, out, out_size))
            return 1;
    }

    /* Generic GTK fallback: gtk-3.0/settings.ini gtk-theme-name=... */
    if(config != NULL && config[0] != '\0')
        snprintf(path, sizeof(path), "%s/gtk-3.0/settings.ini", config);
    else if(home != NULL && home[0] != '\0')
        snprintf(path, sizeof(path), "%s/.config/gtk-3.0/settings.ini", home);
    else
        path[0] = '\0';
    if(path[0] != '\0' && read_text_file(path, text, sizeof(text))) {
        cursor = strstr(text, "gtk-theme-name");
        if(cursor != NULL) {
            cursor = strchr(cursor, '=');
            if(cursor != NULL) {
                const char *end;

                cursor++;
                while(*cursor == ' ' || *cursor == '\t')
                    cursor++;
                end = cursor;
                while(*end != '\0' && *end != '\n' && *end != '"' &&
                      *end != ' ' && *end != '\t' && *end != '\r')
                    end++;
                copy_path(out, out_size, cursor, (int)(end - cursor));
                if(out[0] != '\0')
                    return 1;
            }
        }
    }
    return 0;
}

static int
gtk_css_theme_file(const char *theme, char *out, int out_size)
{
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_DATA_HOME");
    char prefix[512];
    int i;
    static const char *const roots[] = {
        "home-themes",
        "xdg-themes",
        "/usr/local/share/themes",
        "/usr/share/themes",
        NULL
    };

    if(theme == NULL || theme[0] == '\0' || out == NULL || out_size <= 0)
        return 0;
    for(i = 0; roots[i] != NULL; i++) {
        if(strcmp(roots[i], "home-themes") == 0) {
            if(home == NULL || home[0] == '\0')
                continue;
            if(!join_path_literal(prefix, sizeof(prefix), home, "/.themes"))
                continue;
        } else if(strcmp(roots[i], "xdg-themes") == 0) {
            if(xdg == NULL || xdg[0] == '\0')
                continue;
            if(!join_path_literal(prefix, sizeof(prefix), xdg, "/themes"))
                continue;
        } else {
            copy_path(prefix, sizeof(prefix), roots[i], (int)strlen(roots[i]));
        }
        if(join_gtk_theme_css_path(out, out_size, prefix, theme) && path_exists(out))
            return 1;
    }
    return 0;
}

static int
palette_luminance(Color color)
{
    return ((int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114) / 1000;
}

static int
gtk_css_palette_refresh(void)
{
    char theme[96];
    char path[512];
    char text[65536];
    SystemThemePalette palette;
    Color bg, base, fg, selected;
    int have_bg, have_base, have_fg, have_selected;

    if(!gtk_css_theme_name(theme, sizeof(theme)))
        return 0;
    if(!gtk_css_theme_file(theme, path, sizeof(path)))
        return 0;
    if(!read_text_file(path, text, sizeof(text)))
        return 0;

    have_base = css_define_color(text, "theme_base_color", &base) ||
                css_define_color(text, "base_color", &base);
    have_bg = css_define_color(text, "theme_bg_color", &bg) ||
              css_define_color(text, "bg_color", &bg);
    have_fg = css_define_color(text, "theme_fg_color", &fg) ||
              css_define_color(text, "fg_color", &fg);
    have_selected = css_define_color(text, "theme_selected_bg_color", &selected) ||
                    css_define_color(text, "selected_bg_color", &selected);
    if(!have_bg && !have_base)
        return 0;

    palette = system_palette;
    palette.background = have_base ? base : bg;
    palette.surface = have_bg ? bg : palette.background;
    palette.text = have_fg ? fg : (Color){0x10, 0x10, 0x10, 0xFF};
    palette.button = palette.surface;
    palette.button_hover = LightenUIColor(palette.button, 18);
    palette.circle = have_selected ? selected : palette.button_hover;
    palette.link = palette.circle;
    palette.icon = palette.text;
    palette.available = 1;
    palette.supports_mode = 0;
    palette.prefers_dark =
        palette_luminance(palette.text) > palette_luminance(palette.background);
    copy_path(palette.name, sizeof(palette.name), theme, (int)strlen(theme));

    system_palette = palette;
    system_light_palette = palette;
    system_dark_palette = palette;
    system_prefers_dark = palette.prefers_dark;
    return 1;
}

bool
GetSystemDesktopBackground(char *out, int out_size)
{
#if defined(KRYON_PLATFORM_PLAN9)
    char text[512];
    char path[512];
    char *home;
    char *end;

    if(out == NULL || out_size <= 0)
        return false;
    out[0] = '\0';

    home = getenv("home");
    if(home != NULL && home[0] != '\0') {
        snprintf(path, sizeof(path), "%s/lib/wallpaper", home);
        if(read_text_file(path, text, sizeof(text))) {
            end = text;
            while(*end != '\0' && *end != '\n' && *end != '\r')
                end++;
            *end = '\0';
            if(text[0] == '/' && path_exists(text)) {
                copy_path(out, out_size, text, (int)strlen(text));
                return true;
            }
        }
        snprintf(path, sizeof(path), "%s/lib/wallpaper.png", home);
        if(path_exists(path)) {
            copy_path(out, out_size, path, (int)strlen(path));
            return true;
        }
    }
    if(path_exists("/lib/wallpaper.png")) {
        copy_path(out, out_size, "/lib/wallpaper.png",
                  (int)strlen("/lib/wallpaper.png"));
        return true;
    }
    return false;
#else
    char config_path[512];
    char text[65536];
    const char *cursor;

    if(out == NULL || out_size <= 0)
        return false;
    out[0] = '\0';

    if(!xfce_desktop_config_path(config_path, sizeof(config_path)))
        return false;
    if(!read_text_file(config_path, text, sizeof(text)))
        return false;

    cursor = text;
    while((cursor = strstr(cursor, "name=\"last-image\"")) != NULL) {
        char path[512];

        path[0] = '\0';
        if(find_xml_value(cursor, path, sizeof(path)) && path_exists(path)) {
            copy_path(out, out_size, path, (int)strlen(path));
            return true;
        }
        cursor += 17;
    }

    return false;
#endif
}

#if defined(KRYON_PLATFORM_PLAN9)
/* Native Plan 9: the window manager owns the system theme.
 *
 * A rio(1) variant that links Kryon writes a small state file whenever the
 * user switches the desktop style, palette, or mode:
 *
 *     /lib/kryon/system-theme     system-wide default written by the WM
 *     $home/lib/kryon/theme       per-user WM state
 *
 * with one `key=value` pair per line (name, mode, style). Reading it here
 * turns the WM choice into Kryons system theme, so apps that follow
 * THEME_SOURCE_SYSTEM re-skin together with the desktop. */
static int
plan9_theme_file_value(const char *text, const char *key, char *out, int out_size)
{
    const char *line = text;
    size_t key_len = strlen(key);

    while(line != NULL && *line != '\0') {
        const char *eol = strchr(line, '\n');
        size_t len = eol != NULL ? (size_t)(eol - line) : strlen(line);

        if(len > key_len && strncmp(line, key, key_len) == 0 &&
           line[key_len] == '=') {
            size_t value_len = len - key_len - 1;

            if(value_len >= (size_t)out_size)
                value_len = (size_t)out_size - 1;
            memcpy(out, line + key_len + 1, value_len);
            out[value_len] = '\0';
            return 1;
        }
        line = eol != NULL ? eol + 1 : NULL;
    }
    return 0;
}

static int
plan9_read_theme_file(char *text, int text_size)
{
    char path[512];
    char *home;
    FILE *f;

    f = fopen("/lib/kryon/system-theme", "r");
    if(f == NULL) {
        home = getenv("home");
        if(home == NULL)
            return 0;
        snprintf(path, sizeof(path), "%s/lib/kryon/theme", home);
        f = fopen(path, "r");
    }
    if(f == NULL)
        return 0;
    {
        size_t got = fread(text, 1, (size_t)text_size - 1, f);

        fclose(f);
        text[got] = '\0';
    }
    return 1;
}

static int
plan9_system_theme_refresh(void)
{
    char text[1024];
    char name[THEME_NAME_SIZE];
    char mode[16];
    char style[16];
    int theme_id = -1;
    int dark = 0;
    int i;

    if(!plan9_read_theme_file(text, sizeof(text)))
        return 0;

    if(!plan9_theme_file_value(text, "name", name, sizeof(name)))
        return 0;
    plan9_theme_file_value(text, "mode", mode, sizeof(mode));
    if(strcmp(mode, "dark") == 0)
        dark = 1;
    if(plan9_theme_file_value(text, "style", style, sizeof(style))) {
        if(strcmp(style, "material") == 0)
            system_theme_style = THEME_STYLE_MATERIAL;
        else if(strcmp(style, "retro") == 0)
            system_theme_style = THEME_STYLE_RETRO;
        else
            system_theme_style = THEME_STYLE_RETRO;
    } else {
        system_theme_style = THEME_STYLE_RETRO;
    }

    for(i = 0; i < THEME_COUNT; i++) {
        if(strcmp(themes[i].name, name) == 0) {
            theme_id = i;
            break;
        }
    }
    if(theme_id < 0)
        return 0;

    {
        SystemThemePalette palette;
        const char *keys[8] = {
            "background", "surface", "text", "circle",
            "button", "button_hover", "icon", "link"
        };
        Color *slots[8];
        int k;

        memset(&palette, 0, sizeof(palette));
        slots[0] = &palette.background;
        slots[1] = &palette.surface;
        slots[2] = &palette.text;
        slots[3] = &palette.circle;
        slots[4] = &palette.button;
        slots[5] = &palette.button_hover;
        slots[6] = &palette.icon;
        slots[7] = &palette.link;
        for(k = 0; k < 8; k++) {
            if(!GetThemeCatalogColor((ThemeId)theme_id, dark != 0, keys[k], slots[k]))
                return 0;
        }
        palette.available = 1;
        palette.prefers_dark = dark;
        palette.supports_mode = 1;
        snprintf(palette.name, sizeof(palette.name), "%s", name);

        system_palette = palette;
        system_light_palette = palette;
        system_dark_palette = palette;
        system_light_palette.prefers_dark = 0;
        system_dark_palette.prefers_dark = 1;
        system_prefers_dark = dark;
    }
    return 1;
}
#endif

bool
RefreshSystemTheme(void)
{
#if defined(PLATFORM_WEB)
    /* Browsers expose no desktop palette, but they do expose the users
       light/dark preference; map it onto the material palettes. */
    {
        int prefers_dark = EM_ASM_INT_V(
            return (typeof matchMedia === 'function' &&
                    matchMedia('(prefers-color-scheme: dark)').matches) ? 1 : 0;
        );
        apply_material_palette(prefers_dark != 0);
        snprintf(system_palette.name, sizeof(system_palette.name), "System");
        return true;
    }
#endif
    system_theme_refresh_count++;
#if defined(KRYON_PLATFORM_PLAN9)
    return plan9_system_theme_refresh() != 0;
#endif
#if defined(_WIN32)
    return windows_system_theme_refresh() != 0;
#endif
    /* Prefer reading the theme CSS files directly: initializing GTK
       in-process while the apps SDL window and GL context are already live
       can corrupt the render context (GTK performs late global X
       initialization, realizes real X windows, and pumps the GLib main
       loop from the render thread). The CSS reader needs no X connection,
       so it is safe to call at any point after startup. Only fall back to
       the in-process sampler while no app window exists yet. */
    if(gtk_css_palette_refresh())
        return true;
#if defined(SYSTEM_THEME_GTK)
    if(!IsWindowReady() && gtk_system_theme_refresh())
        return true;
#endif
    return system_palette.available != 0;
}

long
SystemThemeRefreshCount(void)
{
    return system_theme_refresh_count;
}

bool
IsSystemThemeAvailable(void)
{
    system_theme_auto_refresh();
    return system_palette.available != 0;
}

const char *
GetSystemThemeName(void)
{
    system_theme_auto_refresh();
    return system_palette.name;
}

const char *
GetSystemThemeNameCached(void)
{
    return system_palette.name;
}

ThemeStyle
GetSystemThemeStyle(void)
{
    system_theme_auto_refresh();
    return system_theme_style;
}

ThemeStyle
GetSystemThemeStyleCached(void)
{
    return system_theme_style;
}

bool
GetSystemUIFontName(char *out, int out_size)
{
    if(out == NULL || out_size <= 0)
        return false;
    out[0] = '\0';
    if(system_ui_font_name[0] == '\0')
        refresh_system_ui_font();
    if(system_ui_font_name[0] == '\0')
        return false;
    snprintf(out, (size_t)out_size, "%s", system_ui_font_name);
    return true;
}

bool
GetSystemUIFontFile(char *out, int out_size)
{
    if(out == NULL || out_size <= 0)
        return false;
    out[0] = '\0';
    if(system_ui_font_file[0] == '\0')
        refresh_system_ui_font();
    if(system_ui_font_file[0] == '\0')
        return false;
    snprintf(out, (size_t)out_size, "%s", system_ui_font_file);
    return true;
}

bool
SystemThemePrefersDark(void)
{
    system_theme_auto_refresh();
    return system_prefers_dark != 0;
}

bool
SystemThemeSupportsMode(void)
{
    system_theme_auto_refresh();
    return system_palette.supports_mode != 0;
}

void
SetSystemThemeDarkMode(bool dark)
{
    if(!system_palette.available ||
       strcmp(system_palette.name, "System") == 0 ||
       strcmp(system_palette.name, "Material") == 0) {
        apply_material_palette(dark);
        return;
    }
    if(system_palette.supports_mode) {
        system_palette = dark ? system_dark_palette : system_light_palette;
        system_palette.prefers_dark = dark ? 1 : 0;
        return;
    }
}

void
SetSystemThemePalette(const char *name,
                      Color background,
                      Color surface,
                      Color text,
                      Color circle,
                      Color button,
                      Color button_hover,
                      Color icon,
                      Color link,
                      bool prefers_dark,
                      bool supports_mode)
{
    system_palette.background = background;
    system_palette.surface = surface;
    system_palette.text = text;
    system_palette.circle = circle;
    system_palette.button = button;
    system_palette.button_hover = button_hover;
    system_palette.icon = icon;
    system_palette.link = link;
    system_palette.available = 1;
    system_palette.prefers_dark = prefers_dark ? 1 : 0;
    system_palette.supports_mode = supports_mode ? 1 : 0;
    system_prefers_dark = prefers_dark ? 1 : 0;
    snprintf(system_palette.name, sizeof(system_palette.name), "%s",
             name != NULL && name[0] != '\0' ? name : "System");
    if(prefers_dark)
        system_dark_palette = system_palette;
    else
        system_light_palette = system_palette;
}

bool
SystemThemeColor(const char *key, Color *color)
{
    if(key == NULL || color == NULL)
        return false;
    if(!IsSystemThemeAvailable())
        return false;

    if(strcmp(key, "background") == 0)
        *color = system_palette.background;
    else if(strcmp(key, "surface") == 0)
        *color = system_palette.surface;
    else if(strcmp(key, "text") == 0)
        *color = system_palette.text;
    else if(strcmp(key, "circle") == 0)
        *color = system_palette.circle;
    else if(strcmp(key, "button") == 0)
        *color = system_palette.button;
    else if(strcmp(key, "button_hover") == 0)
        *color = system_palette.button_hover;
    else if(strcmp(key, "icon") == 0)
        *color = system_palette.icon;
    else if(strcmp(key, "link") == 0)
        *color = system_palette.link;
    else
        return false;

    return true;
}
