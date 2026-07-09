#include "flint_theme.h"
#include "flint_color.h"

#include <stdio.h>
#include <string.h>

#if defined(FLINT_SYSTEM_THEME_GTK)
#include <gtk/gtk.h>
#endif

typedef struct FlintSystemThemePalette {
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
    char name[FLINT_THEME_NAME_SIZE];
} FlintSystemThemePalette;

static FlintSystemThemePalette system_palette = {
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
    .name = "System"
};

static const FlintSystemThemePalette material_light_palette = {
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

static const FlintSystemThemePalette material_dark_palette = {
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
}

#if defined(FLINT_SYSTEM_THEME_GTK)
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
gtk_system_theme_refresh(void)
{
    static int initialized = 0;
    static int init_ok = 0;
    GtkSettings *settings;
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *button;
    char *theme_name = NULL;
    gboolean prefer_dark = FALSE;
    Color bg;
    Color surface;
    Color text;
    Color button_bg;
    Color button_hover;
    Color accent;

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

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    label = gtk_label_new("flint");
    button = gtk_button_new_with_label("flint");
    gtk_container_add(GTK_CONTAINER(window), box);
    gtk_container_add(GTK_CONTAINER(box), label);
    gtk_container_add(GTK_CONTAINER(box), button);
    gtk_widget_realize(window);
    gtk_widget_realize(box);
    gtk_widget_realize(label);
    gtk_widget_realize(button);

    bg = style_background_for(gtk_widget_get_style_context(window), GTK_STATE_FLAG_NORMAL,
                              system_palette.background);
    surface = style_background_for(gtk_widget_get_style_context(box), GTK_STATE_FLAG_NORMAL,
                                   bg);
    text = style_color_for(gtk_widget_get_style_context(label), GTK_STATE_FLAG_NORMAL,
                           system_palette.text);
    button_bg = style_background_for(gtk_widget_get_style_context(button), GTK_STATE_FLAG_NORMAL,
                                     surface);
    button_hover = style_background_for(gtk_widget_get_style_context(button), GTK_STATE_FLAG_PRELIGHT,
                                        flint_lighten(button_bg, 18));
    accent = button_hover;
    if(accent.r == button_bg.r && accent.g == button_bg.g && accent.b == button_bg.b)
        accent = flint_lighten(button_bg, 24);

    system_palette.background = bg;
    system_palette.surface = surface;
    system_palette.text = text;
    system_palette.button = button_bg;
    system_palette.button_hover = button_hover;
    system_palette.circle = accent;
    system_palette.icon = text;
    system_palette.link = accent;
    system_palette.prefers_dark = prefer_dark ? 1 : 0;
    system_palette.available = 1;
    snprintf(system_palette.name, sizeof(system_palette.name), "%s",
             theme_name != NULL && theme_name[0] != '\0' ? theme_name : "GTK");

    gtk_widget_destroy(window);
    g_free(theme_name);

    while(gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    return 1;
}
#endif

bool
flint_theme_refresh_system(void)
{
#if defined(FLINT_SYSTEM_THEME_GTK)
    if(gtk_system_theme_refresh())
        return true;
#endif
    return system_palette.available != 0;
}

bool
flint_theme_system_available(void)
{
    if(!system_palette.available)
        flint_theme_refresh_system();
    return system_palette.available != 0;
}

const char *
flint_theme_system_name(void)
{
    if(!system_palette.available)
        flint_theme_refresh_system();
    return system_palette.name;
}

const char *
flint_theme_system_name_cached(void)
{
    return system_palette.name;
}

bool
flint_theme_system_prefers_dark(void)
{
    if(!system_palette.available)
        flint_theme_refresh_system();
    return system_palette.prefers_dark != 0;
}

void
flint_theme_set_system_dark_mode(bool dark)
{
    if(!system_palette.available ||
       strcmp(system_palette.name, "System") == 0 ||
       strcmp(system_palette.name, "Material") == 0) {
        apply_material_palette(dark);
        return;
    }
    system_palette.prefers_dark = dark ? 1 : 0;
}

bool
flint_theme_system_color(const char *key, Color *color)
{
    if(key == NULL || color == NULL)
        return false;
    if(!flint_theme_system_available())
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
