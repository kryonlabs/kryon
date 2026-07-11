#include "../include/desktop_tray.h"

#if defined(FLINT_DESKTOP_TRAY_ENABLED)

#include <SDL.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(FLINT_DESKTOP_TRAY_AYATANA)
#include <libayatana-appindicator/app-indicator.h>
#elif defined(FLINT_DESKTOP_TRAY_APPINDICATOR)
#include <libappindicator/app-indicator.h>
#endif

typedef struct DesktopTrayMenuState {
    DesktopTrayMenuItem *items;
    int count;
} DesktopTrayMenuState;

enum {
    DESKTOP_TRAY_STATE_STARTING = 0,
    DESKTOP_TRAY_STATE_READY,
    DESKTOP_TRAY_STATE_FAILED,
    DESKTOP_TRAY_STATE_STOPPED
};

static pthread_t TrayThread;
static pthread_mutex_t TrayActionLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t TrayStateLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t TrayStatusLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t TrayMenuLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t TrayStateCond = PTHREAD_COND_INITIALIZER;
static int PendingAction;
static int TrayStarted;
static int TrayState;
static int TrayCloseAction;
static int TrayActivateAction;
static int TrayMenuUpdatePending;
static int TrayStatusUpdatePending;
static char TrayTitle[128] = "App";
static char TrayIconName[128] = "application-x-executable";
static char TrayStatusText[128] = "App";
static const char *const *TrayIconPaths;
static DesktopTrayMenuState TrayMenuData;
static GtkWidget *TrayMenu;
#if defined(FLINT_DESKTOP_TRAY_GTK_STATUS_ICON)
static GtkStatusIcon *TrayStatusIcon;
#endif
#if defined(FLINT_DESKTOP_TRAY_AYATANA) || defined(FLINT_DESKTOP_TRAY_APPINDICATOR)
static AppIndicator *TrayIndicator;
#endif

static gboolean ApplyDesktopTrayMenu(gpointer user_data);
static gboolean ApplyDesktopTrayStatus(gpointer user_data);

static char *
CopyDesktopTrayString(const char *text)
{
    size_t len;
    char *copy;

    if(text == NULL)
        text = "";
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if(copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static void
FreeDesktopTrayMenuItems(DesktopTrayMenuItem *items, int count)
{
    if(items == NULL)
        return;
    for(int i = 0; i < count; i++) {
        free((char *)items[i].label);
        FreeDesktopTrayMenuItems((DesktopTrayMenuItem *)items[i].children,
                                 items[i].child_count);
    }
    free(items);
}

static DesktopTrayMenuItem *
CopyDesktopTrayMenuItems(const DesktopTrayMenuItem *items, int count)
{
    DesktopTrayMenuItem *copy;

    if(items == NULL || count <= 0)
        return NULL;
    copy = (DesktopTrayMenuItem *)calloc((size_t)count, sizeof(*copy));
    if(copy == NULL)
        return NULL;

    for(int i = 0; i < count; i++) {
        copy[i] = items[i];
        copy[i].label = CopyDesktopTrayString(items[i].label);
        copy[i].children = CopyDesktopTrayMenuItems(items[i].children,
                                                    items[i].child_count);
        if(items[i].child_count > 0 && copy[i].children == NULL) {
            FreeDesktopTrayMenuItems(copy, count);
            return NULL;
        }
    }

    return copy;
}

static void
SetDesktopTrayAction(int action)
{
    pthread_mutex_lock(&TrayActionLock);
    PendingAction = action;
    pthread_mutex_unlock(&TrayActionLock);
}

int
PollDesktopTrayAction(void)
{
    int action;

    pthread_mutex_lock(&TrayActionLock);
    action = PendingAction;
    PendingAction = 0;
    pthread_mutex_unlock(&TrayActionLock);

    return action;
}

static int
DesktopTraySdlEventFilter(void *userdata, SDL_Event *event)
{
    (void)userdata;

    if(event != NULL && event->type == SDL_QUIT && TrayCloseAction != 0) {
        SetDesktopTrayAction(TrayCloseAction);
        return 0;
    }

    return 1;
}

static void
DesktopTrayMenuAction(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    SetDesktopTrayAction((int)(intptr_t)user_data);
}

static GtkWidget *
CreateDesktopTrayGtkMenu(const DesktopTrayMenuItem *items, int count)
{
    GtkWidget *menu = gtk_menu_new();

    for(int i = 0; i < count; i++) {
        GtkWidget *item;

        if(items[i].kind == DESKTOP_TRAY_MENU_ITEM_SEPARATOR) {
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            continue;
        }

        item = gtk_menu_item_new_with_label(items[i].label != NULL ? items[i].label : "");
        gtk_widget_set_sensitive(item, items[i].enabled != 0);
        if(items[i].kind == DESKTOP_TRAY_MENU_ITEM_SUBMENU) {
            GtkWidget *submenu = CreateDesktopTrayGtkMenu(items[i].children,
                                                          items[i].child_count);
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
        } else {
            g_signal_connect(item, "activate", G_CALLBACK(DesktopTrayMenuAction),
                             (gpointer)(intptr_t)items[i].action);
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    gtk_widget_show_all(menu);
    return menu;
}

static GtkWidget *
CreateDesktopTrayMenu(void)
{
    GtkWidget *menu;
    DesktopTrayMenuState snapshot;

    pthread_mutex_lock(&TrayMenuLock);
    snapshot.count = TrayMenuData.count;
    snapshot.items = CopyDesktopTrayMenuItems(TrayMenuData.items, TrayMenuData.count);
    pthread_mutex_unlock(&TrayMenuLock);

    if(snapshot.count > 0 && snapshot.items == NULL)
        return gtk_menu_new();

    menu = CreateDesktopTrayGtkMenu(snapshot.items, snapshot.count);
    FreeDesktopTrayMenuItems(snapshot.items, snapshot.count);
    return menu;
}

static gboolean
ApplyDesktopTrayMenu(gpointer user_data)
{
    GtkWidget *old_menu;
    GtkWidget *next_menu;

    (void)user_data;

    pthread_mutex_lock(&TrayMenuLock);
    TrayMenuUpdatePending = 0;
    pthread_mutex_unlock(&TrayMenuLock);

    next_menu = CreateDesktopTrayMenu();
    old_menu = TrayMenu;
    TrayMenu = next_menu;

#if defined(FLINT_DESKTOP_TRAY_AYATANA) || defined(FLINT_DESKTOP_TRAY_APPINDICATOR)
    if(TrayIndicator != NULL)
        app_indicator_set_menu(TrayIndicator, GTK_MENU(TrayMenu));
#endif

#if defined(FLINT_DESKTOP_TRAY_GTK_STATUS_ICON)
    if(old_menu != NULL)
        gtk_widget_destroy(old_menu);
#else
    (void)old_menu;
#endif

    return G_SOURCE_REMOVE;
}

void
SetDesktopTrayMenu(const DesktopTrayMenuItem *items, int count)
{
    DesktopTrayMenuItem *copy = CopyDesktopTrayMenuItems(items, count);
    DesktopTrayMenuItem *old_items;
    int old_count;

    if(count > 0 && copy == NULL)
        return;

    pthread_mutex_lock(&TrayMenuLock);
    old_items = TrayMenuData.items;
    old_count = TrayMenuData.count;
    TrayMenuData.items = copy;
    TrayMenuData.count = count > 0 ? count : 0;
    if(TrayState == DESKTOP_TRAY_STATE_READY && !TrayMenuUpdatePending) {
        TrayMenuUpdatePending = 1;
        g_idle_add(ApplyDesktopTrayMenu, NULL);
    }
    pthread_mutex_unlock(&TrayMenuLock);

    FreeDesktopTrayMenuItems(old_items, old_count);
}

void
SetDesktopTrayActivateAction(int action)
{
    TrayActivateAction = action;
}

void
SetDesktopTrayStatus(const char *text)
{
    pthread_mutex_lock(&TrayStatusLock);
    snprintf(TrayStatusText, sizeof(TrayStatusText), "%s",
             text != NULL ? text : TrayTitle);
    if(TrayState == DESKTOP_TRAY_STATE_READY && !TrayStatusUpdatePending) {
        TrayStatusUpdatePending = 1;
        g_idle_add((GSourceFunc)ApplyDesktopTrayStatus, NULL);
    }
    pthread_mutex_unlock(&TrayStatusLock);
}

static gboolean
ApplyDesktopTrayStatus(gpointer user_data)
{
    char text[sizeof(TrayStatusText)];

    (void)user_data;

    pthread_mutex_lock(&TrayStatusLock);
    snprintf(text, sizeof(text), "%s", TrayStatusText);
    TrayStatusUpdatePending = 0;
    pthread_mutex_unlock(&TrayStatusLock);

#if defined(FLINT_DESKTOP_TRAY_GTK_STATUS_ICON)
    if(TrayStatusIcon != NULL) {
        gtk_status_icon_set_title(TrayStatusIcon, text);
        gtk_status_icon_set_tooltip_text(TrayStatusIcon, text);
    }
#endif

    return G_SOURCE_REMOVE;
}

static gboolean
QuitDesktopTrayMain(gpointer user_data)
{
    (void)user_data;
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

static const char *
GetDesktopTrayIconPath(void)
{
    static char path[512];
    static const char *const exe_links[] = {
#if defined(__FreeBSD__)
        "/proc/curproc/file",
#endif
#if defined(__linux__)
        "/proc/self/exe",
#endif
        NULL
    };

    for(int i = 0; exe_links[i] != NULL; i++) {
        ssize_t len = readlink(exe_links[i], path, sizeof(path) - 128);
        if(len > 0 && (size_t)len < sizeof(path) - 128) {
            char *slash;
            path[len] = '\0';
            slash = strrchr(path, '/');
            if(slash != NULL) {
                slash[1] = '\0';
                strncat(path, TrayIconName, sizeof(path) - strlen(path) - 1);
                strncat(path, ".png", sizeof(path) - strlen(path) - 1);
                if(g_file_test(path, G_FILE_TEST_IS_REGULAR))
                    return path;
            }
        }
    }

    if(TrayIconPaths != NULL) {
        for(int i = 0; TrayIconPaths[i] != NULL; i++) {
            if(g_file_test(TrayIconPaths[i], G_FILE_TEST_IS_REGULAR))
                return TrayIconPaths[i];
        }
    }

    return NULL;
}

#if defined(FLINT_DESKTOP_TRAY_GTK_STATUS_ICON)
static void
DesktopTrayStatusIconActivate(GtkStatusIcon *status_icon, gpointer user_data)
{
    (void)status_icon;
    (void)user_data;
    if(TrayActivateAction != 0)
        SetDesktopTrayAction(TrayActivateAction);
}

static void
DesktopTrayStatusIconPopup(GtkStatusIcon *status_icon, guint button,
                           guint activate_time, gpointer user_data)
{
    GtkWidget *menu = TrayMenu;

    (void)user_data;
    if(menu == NULL)
        return;
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, gtk_status_icon_position_menu,
                   status_icon, button, activate_time);
}
#endif

static void
SetDesktopTrayState(int state)
{
    pthread_mutex_lock(&TrayStateLock);
    TrayState = state;
    pthread_cond_signal(&TrayStateCond);
    pthread_mutex_unlock(&TrayStateLock);
}

static void *
DesktopTrayThreadMain(void *arg)
{
    GtkWidget *menu;
    (void)arg;

    if(!gtk_init_check(NULL, NULL)) {
        SetDesktopTrayState(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }

#if defined(FLINT_DESKTOP_TRAY_AYATANA) || defined(FLINT_DESKTOP_TRAY_APPINDICATOR)
    TrayIndicator = app_indicator_new(TrayIconName, TrayIconName,
                                      APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    if(TrayIndicator == NULL) {
        SetDesktopTrayState(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }

    app_indicator_set_status(TrayIndicator, APP_INDICATOR_STATUS_ACTIVE);
    {
        const char *icon_path = GetDesktopTrayIconPath();
        if(icon_path != NULL)
            app_indicator_set_icon_full(TrayIndicator, icon_path, TrayTitle);
        else
            app_indicator_set_icon_full(TrayIndicator, TrayIconName, TrayTitle);
    }

    menu = CreateDesktopTrayMenu();
    TrayMenu = menu;
    app_indicator_set_menu(TrayIndicator, GTK_MENU(menu));
#elif defined(FLINT_DESKTOP_TRAY_GTK_STATUS_ICON)
    menu = CreateDesktopTrayMenu();
    TrayMenu = menu;
    {
        const char *icon_path = GetDesktopTrayIconPath();
        TrayStatusIcon = icon_path != NULL
                         ? gtk_status_icon_new_from_file(icon_path)
                         : gtk_status_icon_new_from_icon_name(TrayIconName);
    }
    if(TrayStatusIcon == NULL) {
        SetDesktopTrayState(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }
    gtk_status_icon_set_title(TrayStatusIcon, TrayTitle);
    gtk_status_icon_set_tooltip_text(TrayStatusIcon, TrayTitle);
    gtk_status_icon_set_visible(TrayStatusIcon, TRUE);
    g_signal_connect(TrayStatusIcon, "activate",
                     G_CALLBACK(DesktopTrayStatusIconActivate), NULL);
    g_signal_connect(TrayStatusIcon, "popup-menu",
                     G_CALLBACK(DesktopTrayStatusIconPopup), NULL);
#else
    SetDesktopTrayState(DESKTOP_TRAY_STATE_FAILED);
    return NULL;
#endif

    SetDesktopTrayState(DESKTOP_TRAY_STATE_READY);
    gtk_main();
    SetDesktopTrayState(DESKTOP_TRAY_STATE_STOPPED);
    TrayMenu = NULL;
#if defined(FLINT_DESKTOP_TRAY_AYATANA) || defined(FLINT_DESKTOP_TRAY_APPINDICATOR)
    TrayIndicator = NULL;
#endif

    return NULL;
}

int
InitDesktopTray(const DesktopTraySpec *spec)
{
    int ready;

    if(spec == NULL)
        return 0;

    snprintf(TrayTitle, sizeof(TrayTitle), "%s",
             spec->title != NULL ? spec->title : "App");
    snprintf(TrayIconName, sizeof(TrayIconName), "%s",
             spec->icon_name != NULL ? spec->icon_name : "application-x-executable");
    snprintf(TrayStatusText, sizeof(TrayStatusText), "%s", TrayTitle);
    TrayIconPaths = spec->icon_paths;
    TrayCloseAction = spec->close_action;
    TrayActivateAction = spec->activate_action;
    SetDesktopTrayMenu(spec->menu_items, spec->menu_item_count);

    pthread_mutex_lock(&TrayStateLock);
    TrayState = DESKTOP_TRAY_STATE_STARTING;
    pthread_mutex_unlock(&TrayStateLock);
    if(pthread_create(&TrayThread, NULL, DesktopTrayThreadMain, NULL) != 0)
        return 0;

    TrayStarted = 1;
    pthread_mutex_lock(&TrayStateLock);
    while(TrayState == DESKTOP_TRAY_STATE_STARTING)
        pthread_cond_wait(&TrayStateCond, &TrayStateLock);
    ready = TrayState == DESKTOP_TRAY_STATE_READY;
    pthread_mutex_unlock(&TrayStateLock);

    if(!ready) {
        pthread_join(TrayThread, NULL);
        TrayStarted = 0;
        return 0;
    }

    SDL_SetEventFilter(DesktopTraySdlEventFilter, NULL);
    return ready;
}

void
ShutdownDesktopTray(void)
{
    int ready;

    pthread_mutex_lock(&TrayStateLock);
    ready = TrayState == DESKTOP_TRAY_STATE_READY;
    pthread_mutex_unlock(&TrayStateLock);

    if(TrayStarted && ready)
        g_idle_add(QuitDesktopTrayMain, NULL);

    if(TrayStarted)
        pthread_join(TrayThread, NULL);

    TrayStarted = 0;
}

#else

int InitDesktopTray(const DesktopTraySpec *spec) { (void)spec; return 0; }
void ShutdownDesktopTray(void) {}
int PollDesktopTrayAction(void) { return 0; }
void SetDesktopTrayStatus(const char *text) { (void)text; }
void SetDesktopTrayMenu(const DesktopTrayMenuItem *items, int count)
{
    (void)items;
    (void)count;
}
void SetDesktopTrayActivateAction(int action) { (void)action; }

#endif
