#include "theme.h"
#include "ui_color.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    if(!initialized) {
        init_ok = gtk_init_check(NULL, NULL) ? 1 : 0;
        initialized = 1;
    }
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
find_xml_value(const char *tag, char *out, int out_size)
{
    const char *value;
    const char *end;

    if(tag == NULL)
        return 0;
    value = strstr(tag, "value=\"");
    if(value == NULL)
        return 0;
    value += 7;
    end = strchr(value, '"');
    if(end == NULL || end == value)
        return 0;
    copy_path(out, out_size, value, (int)(end - value));
    return out != NULL && out[0] != '\0';
}

bool
GetSystemDesktopBackground(char *out, int out_size)
{
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
}

bool
RefreshSystemTheme(void)
{
#if defined(SYSTEM_THEME_GTK)
    if(gtk_system_theme_refresh())
        return true;
#endif
    return system_palette.available != 0;
}

bool
IsSystemThemeAvailable(void)
{
    if(!system_palette.available)
        RefreshSystemTheme();
    return system_palette.available != 0;
}

const char *
GetSystemThemeName(void)
{
    if(!system_palette.available)
        RefreshSystemTheme();
    return system_palette.name;
}

const char *
GetSystemThemeNameCached(void)
{
    return system_palette.name;
}

bool
SystemThemePrefersDark(void)
{
    if(!system_palette.available)
        RefreshSystemTheme();
    return system_prefers_dark != 0;
}

bool
SystemThemeSupportsMode(void)
{
    if(!system_palette.available)
        RefreshSystemTheme();
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
