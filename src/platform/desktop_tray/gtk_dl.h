#ifndef KRYON_TRAY_GTK_DL_H
#define KRYON_TRAY_GTK_DL_H

/*
 * Lazy GTK3 loader for the desktop tray (GTK_STATUS_ICON backend).
 *
 * Included instead of <gtk/gtk.h> when KRYON_TRAY_GTK_DL is defined. Types,
 * constants, and cast macros still come from the real GTK headers at compile
 * time, but every gtk- and g-prefixed function call below resolves through a
 * dlsym'd pointer after KryonGtkEnsure() succeeds. Applications then link no
 * GTK libraries, so libgtk-3 and its gdk/pango/cairo dependency chain stay
 * out of the process until the tray actually starts -- a tray-less (or
 * tray-disabled) process never pays for them.
 *
 * Every symbol shadowed here must also be resolved in gtk_dl.c. Keep the two
 * lists in sync.
 */

#include <gtk/gtk.h>

/* dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_GLOBAL) and resolve the symbol
 * table. Returns 1 on success, 0 when GTK is unavailable. Thread-safe; safe
 * to call repeatedly. */
int KryonGtkEnsure(void);

#define gtk_init_check                        kryon_gtk_dl_gtk_init_check
#define gtk_menu_new                          kryon_gtk_dl_gtk_menu_new
#define gtk_menu_item_new_with_label          kryon_gtk_dl_gtk_menu_item_new_with_label
#define gtk_menu_item_set_submenu             kryon_gtk_dl_gtk_menu_item_set_submenu
#define gtk_separator_menu_item_new           kryon_gtk_dl_gtk_separator_menu_item_new
#define gtk_menu_shell_append                 kryon_gtk_dl_gtk_menu_shell_append
#define gtk_widget_set_sensitive              kryon_gtk_dl_gtk_widget_set_sensitive
#define gtk_widget_show_all                   kryon_gtk_dl_gtk_widget_show_all
#define gtk_menu_popup                        kryon_gtk_dl_gtk_menu_popup
#define gtk_status_icon_position_menu         kryon_gtk_dl_gtk_status_icon_position_menu
#define gtk_status_icon_new_from_file         kryon_gtk_dl_gtk_status_icon_new_from_file
#define gtk_status_icon_new_from_icon_name    kryon_gtk_dl_gtk_status_icon_new_from_icon_name
#define gtk_status_icon_set_title             kryon_gtk_dl_gtk_status_icon_set_title
#define gtk_status_icon_set_tooltip_text      kryon_gtk_dl_gtk_status_icon_set_tooltip_text
#define gtk_status_icon_set_visible           kryon_gtk_dl_gtk_status_icon_set_visible
#define gtk_status_icon_is_embedded           kryon_gtk_dl_gtk_status_icon_is_embedded
#define gtk_status_icon_set_from_file         kryon_gtk_dl_gtk_status_icon_set_from_file
#define gtk_main                              kryon_gtk_dl_gtk_main
#define gtk_main_quit                         kryon_gtk_dl_gtk_main_quit
#define gtk_widget_destroy                    kryon_gtk_dl_gtk_widget_destroy
#define g_idle_add                            kryon_gtk_dl_g_idle_add
#define g_signal_connect_data                 kryon_gtk_dl_g_signal_connect_data
#define g_file_test                           kryon_gtk_dl_g_file_test

extern gboolean (*kryon_gtk_dl_gtk_init_check)(int *argc, char ***argv);
extern GtkWidget *(*kryon_gtk_dl_gtk_menu_new)(void);
extern GtkWidget *(*kryon_gtk_dl_gtk_menu_item_new_with_label)(const gchar *label);
extern void (*kryon_gtk_dl_gtk_menu_item_set_submenu)(GtkMenuItem *menu_item,
                                                      GtkWidget *submenu);
extern GtkWidget *(*kryon_gtk_dl_gtk_separator_menu_item_new)(void);
extern void (*kryon_gtk_dl_gtk_menu_shell_append)(GtkMenuShell *menu_shell,
                                                  GtkWidget *child);
extern void (*kryon_gtk_dl_gtk_widget_set_sensitive)(GtkWidget *widget,
                                                     gboolean sensitive);
extern void (*kryon_gtk_dl_gtk_widget_show_all)(GtkWidget *widget);
extern void (*kryon_gtk_dl_gtk_menu_popup)(GtkMenu *menu,
                                           GtkWidget *parent_menu_shell,
                                           GtkWidget *parent_menu_item,
                                           GtkMenuPositionFunc func, gpointer data,
                                           guint button, guint32 activate_time);
extern void (*kryon_gtk_dl_gtk_status_icon_position_menu)(GtkMenu *menu, gint *x,
                                                          gint *y, gboolean *push_in,
                                                          gpointer user_data);
extern GtkStatusIcon *(*kryon_gtk_dl_gtk_status_icon_new_from_file)(const gchar *filename);
extern GtkStatusIcon *(*kryon_gtk_dl_gtk_status_icon_new_from_icon_name)(const gchar *icon_name);
extern void (*kryon_gtk_dl_gtk_status_icon_set_title)(GtkStatusIcon *status_icon,
                                                      const gchar *title);
extern void (*kryon_gtk_dl_gtk_status_icon_set_tooltip_text)(GtkStatusIcon *status_icon,
                                                             const gchar *text);
extern void (*kryon_gtk_dl_gtk_status_icon_set_visible)(GtkStatusIcon *status_icon,
                                                        gboolean visible);
extern gboolean (*kryon_gtk_dl_gtk_status_icon_is_embedded)(GtkStatusIcon *status_icon);
extern void (*kryon_gtk_dl_gtk_status_icon_set_from_file)(GtkStatusIcon *status_icon,
                                                          const gchar *filename);
extern void (*kryon_gtk_dl_gtk_main)(void);
extern void (*kryon_gtk_dl_gtk_main_quit)(void);
extern void (*kryon_gtk_dl_gtk_widget_destroy)(GtkWidget *widget);
extern guint (*kryon_gtk_dl_g_idle_add)(GSourceFunc function, gpointer data);
extern gulong (*kryon_gtk_dl_g_signal_connect_data)(gpointer instance,
                                                    const gchar *detailed_signal,
                                                    GCallback c_handler, gpointer data,
                                                    GClosureNotify destroy_data,
                                                    GConnectFlags connect_flags);
extern gboolean (*kryon_gtk_dl_g_file_test)(const gchar *filename, GFileTest test);

#endif /* KRYON_TRAY_GTK_DL_H */
