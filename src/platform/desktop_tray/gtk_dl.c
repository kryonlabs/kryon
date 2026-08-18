#include "desktop_tray.h"

#if defined(KRYON_DESKTOP_TRAY_ENABLED) && defined(KRYON_TRAY_GTK_DL)

#include "gtk_dl.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>

/* Storage for the pointers declared in gtk_dl.h; populated once by
 * KryonGtkEnsure(). Keep this list in sync with gtk_dl.h. */
gboolean (*kryon_gtk_dl_gtk_init_check)(int *argc, char ***argv);
GtkWidget *(*kryon_gtk_dl_gtk_menu_new)(void);
GtkWidget *(*kryon_gtk_dl_gtk_menu_item_new_with_label)(const gchar *label);
void (*kryon_gtk_dl_gtk_menu_item_set_submenu)(GtkMenuItem *menu_item,
                                               GtkWidget *submenu);
GtkWidget *(*kryon_gtk_dl_gtk_separator_menu_item_new)(void);
void (*kryon_gtk_dl_gtk_menu_shell_append)(GtkMenuShell *menu_shell,
                                           GtkWidget *child);
void (*kryon_gtk_dl_gtk_widget_set_sensitive)(GtkWidget *widget,
                                              gboolean sensitive);
void (*kryon_gtk_dl_gtk_widget_show_all)(GtkWidget *widget);
void (*kryon_gtk_dl_gtk_menu_popup)(GtkMenu *menu,
                                    GtkWidget *parent_menu_shell,
                                    GtkWidget *parent_menu_item,
                                    GtkMenuPositionFunc func, gpointer data,
                                    guint button, guint32 activate_time);
void (*kryon_gtk_dl_gtk_status_icon_position_menu)(GtkMenu *menu, gint *x,
                                                   gint *y, gboolean *push_in,
                                                   gpointer user_data);
GtkStatusIcon *(*kryon_gtk_dl_gtk_status_icon_new_from_file)(const gchar *filename);
GtkStatusIcon *(*kryon_gtk_dl_gtk_status_icon_new_from_icon_name)(const gchar *icon_name);
void (*kryon_gtk_dl_gtk_status_icon_set_title)(GtkStatusIcon *status_icon,
                                               const gchar *title);
void (*kryon_gtk_dl_gtk_status_icon_set_tooltip_text)(GtkStatusIcon *status_icon,
                                                      const gchar *text);
void (*kryon_gtk_dl_gtk_status_icon_set_visible)(GtkStatusIcon *status_icon,
                                                 gboolean visible);
void (*kryon_gtk_dl_gtk_status_icon_set_from_file)(GtkStatusIcon *status_icon,
                                                   const gchar *filename);
void (*kryon_gtk_dl_gtk_main)(void);
void (*kryon_gtk_dl_gtk_main_quit)(void);
void (*kryon_gtk_dl_gtk_widget_destroy)(GtkWidget *widget);
guint (*kryon_gtk_dl_g_idle_add)(GSourceFunc function, gpointer data);
gulong (*kryon_gtk_dl_g_signal_connect_data)(gpointer instance,
                                             const gchar *detailed_signal,
                                             GCallback c_handler, gpointer data,
                                             GClosureNotify destroy_data,
                                             GConnectFlags connect_flags);
gboolean (*kryon_gtk_dl_g_file_test)(const gchar *filename, GFileTest test);

static pthread_once_t kryon_gtk_dl_once = PTHREAD_ONCE_INIT;
static int kryon_gtk_dl_ready = 0;

static void
kryon_gtk_dl_resolve(void *handle, void **slot, const char *name)
{
    void *sym = dlsym(handle, name);

    if(sym == NULL) {
        fprintf(stderr, "kryon: gtk_dl: missing symbol %s\n", name);
        kryon_gtk_dl_ready = 0;
    }
    *slot = sym;
}

static void
kryon_gtk_dl_load(void)
{
    void *handle = dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_GLOBAL);

    if(handle == NULL) {
        fprintf(stderr, "kryon: gtk_dl: %s\n", dlerror());
        return;
    }

    kryon_gtk_dl_ready = 1;
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_init_check,
                         "gtk_init_check");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_menu_new,
                         "gtk_menu_new");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_menu_item_new_with_label,
                         "gtk_menu_item_new_with_label");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_menu_item_set_submenu,
                         "gtk_menu_item_set_submenu");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_separator_menu_item_new,
                         "gtk_separator_menu_item_new");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_menu_shell_append,
                         "gtk_menu_shell_append");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_widget_set_sensitive,
                         "gtk_widget_set_sensitive");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_widget_show_all,
                         "gtk_widget_show_all");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_menu_popup,
                         "gtk_menu_popup");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_status_icon_position_menu,
                         "gtk_status_icon_position_menu");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_status_icon_new_from_file,
                         "gtk_status_icon_new_from_file");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_status_icon_new_from_icon_name,
                         "gtk_status_icon_new_from_icon_name");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_status_icon_set_title,
                         "gtk_status_icon_set_title");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_status_icon_set_tooltip_text,
                         "gtk_status_icon_set_tooltip_text");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_status_icon_set_visible,
                         "gtk_status_icon_set_visible");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_status_icon_set_from_file,
                         "gtk_status_icon_set_from_file");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_main, "gtk_main");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_main_quit,
                         "gtk_main_quit");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_gtk_widget_destroy,
                         "gtk_widget_destroy");
    /* glib/gobject symbols resolve through GTK's own dependency chain. */
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_g_idle_add, "g_idle_add");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_g_signal_connect_data,
                         "g_signal_connect_data");
    kryon_gtk_dl_resolve(handle, (void **)&kryon_gtk_dl_g_file_test,
                         "g_file_test");
}

int
KryonGtkEnsure(void)
{
    pthread_once(&kryon_gtk_dl_once, kryon_gtk_dl_load);
    return kryon_gtk_dl_ready;
}

#endif /* KRYON_DESKTOP_TRAY_ENABLED && KRYON_TRAY_GTK_DL */
