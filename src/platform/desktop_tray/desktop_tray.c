#include "desktop_tray.h"

#if defined(KRYON_BACKEND_TERMI)

int InitDesktopTray(const DesktopTraySpec *spec) { (void)spec; return 0; }
void ShutdownDesktopTray(void) {}
int PollDesktopTrayAction(void) { return 0; }
void SetDesktopTrayStatus(const char *text) { (void)text; }
void SetDesktopTrayIcon(const char *icon_path) { (void)icon_path; }
void SetDesktopTrayMenu(const DesktopTrayMenuItem *items, int count)
{
    (void)items;
    (void)count;
}
void SetDesktopTrayActivateAction(int action) { (void)action; }

#elif defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRAY_MESSAGE (WM_APP + 76)
#define TRAY_ICON_ID 0x4b52
#define TRAY_COMMAND_BASE 0x5100
#define TRAY_COMMAND_MAX 256

static HWND TrayWindow;
static NOTIFYICONDATAW TrayIcon;
static DesktopTrayMenuItem *TrayItems;
static int TrayItemCount;
static int TrayActivateAction;
static int PendingAction;
static int CommandActions[TRAY_COMMAND_MAX];
static int CommandCount;
static char TrayTitle[128] = "App";

static void Utf8Wide(const char *src, WCHAR *dst, int count)
{
    if(dst == NULL || count <= 0) return;
    dst[0] = 0;
    if(src == NULL) return;
    if(!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, dst, count))
        MultiByteToWideChar(CP_ACP, 0, src, -1, dst, count);
    dst[count - 1] = 0;
}

static char *CopyString(const char *text)
{
    size_t length;
    char *copy;
    if(text == NULL) text = "";
    length = strlen(text) + 1;
    copy = (char *)malloc(length);
    if(copy != NULL) memcpy(copy, text, length);
    return copy;
}

static void FreeItems(DesktopTrayMenuItem *items, int count)
{
    int i;
    for(i = 0; i < count; i++) {
        free((char *)items[i].label);
        FreeItems((DesktopTrayMenuItem *)items[i].children, items[i].child_count);
    }
    free(items);
}

static DesktopTrayMenuItem *CopyItems(const DesktopTrayMenuItem *items, int count)
{
    DesktopTrayMenuItem *copy;
    int i;
    if(items == NULL || count <= 0) return NULL;
    copy = (DesktopTrayMenuItem *)calloc((size_t)count, sizeof(*copy));
    if(copy == NULL) return NULL;
    for(i = 0; i < count; i++) {
        copy[i] = items[i];
        copy[i].label = CopyString(items[i].label);
        copy[i].children = CopyItems(items[i].children, items[i].child_count);
    }
    return copy;
}

static HMENU BuildMenu(const DesktopTrayMenuItem *items, int count)
{
    HMENU menu = CreatePopupMenu();
    int i;
    for(i = 0; i < count; i++) {
        WCHAR label[160];
        UINT flags = MF_STRING;
        if(items[i].kind == DESKTOP_TRAY_MENU_ITEM_SEPARATOR) {
            AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            continue;
        }
        Utf8Wide(items[i].label, label, (int)(sizeof(label) / sizeof(label[0])));
        if(!items[i].enabled) flags |= MF_GRAYED;
        if(items[i].kind == DESKTOP_TRAY_MENU_ITEM_SUBMENU) {
            HMENU child = BuildMenu(items[i].children, items[i].child_count);
            AppendMenuW(menu, flags | MF_POPUP, (UINT_PTR)child, label);
        } else if(CommandCount < TRAY_COMMAND_MAX) {
            UINT command = TRAY_COMMAND_BASE + (UINT)CommandCount;
            CommandActions[CommandCount++] = items[i].action;
            AppendMenuW(menu, flags, command, label);
        }
    }
    return menu;
}

static void ShowMenu(void)
{
    POINT point;
    HMENU menu;
    CommandCount = 0;
    menu = BuildMenu(TrayItems, TrayItemCount);
    GetCursorPos(&point);
    SetForegroundWindow(TrayWindow);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   point.x, point.y, 0, TrayWindow, NULL);
    PostMessageW(TrayWindow, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static LRESULT CALLBACK TrayWindowProc(HWND window, UINT message,
                                       WPARAM wparam, LPARAM lparam)
{
    if(message == TRAY_MESSAGE) {
        UINT event = (UINT)lparam;
        if(event == WM_LBUTTONUP || event == NIN_SELECT ||
           event == NIN_KEYSELECT)
            PendingAction = TrayActivateAction;
        else if(event == WM_RBUTTONUP || event == WM_CONTEXTMENU)
            ShowMenu();
        return 0;
    }
    if(message == WM_COMMAND) {
        UINT command = LOWORD(wparam);
        if(command >= TRAY_COMMAND_BASE &&
           command < TRAY_COMMAND_BASE + (UINT)CommandCount)
            PendingAction = CommandActions[command - TRAY_COMMAND_BASE];
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int InitDesktopTray(const DesktopTraySpec *spec)
{
    static const WCHAR class_name[] = L"KryonDesktopTrayWindow";
    WNDCLASSEXW window_class;
    HINSTANCE instance;
    if(spec == NULL) return 0;
    instance = GetModuleHandleW(NULL);
    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = TrayWindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    RegisterClassExW(&window_class);
    TrayWindow = CreateWindowExW(WS_EX_TOOLWINDOW, class_name, L"", WS_POPUP,
        0, 0, 0, 0, NULL, NULL, instance, NULL);
    if(TrayWindow == NULL) return 0;
    snprintf(TrayTitle, sizeof(TrayTitle), "%s", spec->title ? spec->title : "App");
    TrayActivateAction = spec->activate_action;
    SetDesktopTrayMenu(spec->menu_items, spec->menu_item_count);
    memset(&TrayIcon, 0, sizeof(TrayIcon));
    TrayIcon.cbSize = sizeof(TrayIcon);
    TrayIcon.hWnd = TrayWindow;
    TrayIcon.uID = TRAY_ICON_ID;
    TrayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    TrayIcon.uCallbackMessage = TRAY_MESSAGE;
    TrayIcon.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(32512));
    if(TrayIcon.hIcon == NULL)
        TrayIcon.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
    Utf8Wide(TrayTitle, TrayIcon.szTip, (int)(sizeof(TrayIcon.szTip) / sizeof(WCHAR)));
    if(!Shell_NotifyIconW(NIM_ADD, &TrayIcon)) return 0;
    TrayIcon.uVersion = NOTIFYICON_VERSION;
    Shell_NotifyIconW(NIM_SETVERSION, &TrayIcon);
    return 1;
}

void ShutdownDesktopTray(void)
{
    if(TrayWindow != NULL) Shell_NotifyIconW(NIM_DELETE, &TrayIcon);
    FreeItems(TrayItems, TrayItemCount);
    TrayItems = NULL; TrayItemCount = 0;
    if(TrayWindow != NULL) DestroyWindow(TrayWindow);
    TrayWindow = NULL;
}

int PollDesktopTrayAction(void)
{
    MSG message;
    int action;
    while(TrayWindow != NULL && PeekMessageW(&message, TrayWindow, 0, 0, PM_REMOVE))
        { TranslateMessage(&message); DispatchMessageW(&message); }
    action = PendingAction; PendingAction = 0; return action;
}

void SetDesktopTrayStatus(const char *text)
{
    if(TrayWindow == NULL) return;
    Utf8Wide(text ? text : TrayTitle, TrayIcon.szTip,
             (int)(sizeof(TrayIcon.szTip) / sizeof(WCHAR)));
    TrayIcon.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &TrayIcon);
}

void SetDesktopTrayMenu(const DesktopTrayMenuItem *items, int count)
{
    DesktopTrayMenuItem *copy = CopyItems(items, count);
    FreeItems(TrayItems, TrayItemCount);
    TrayItems = copy; TrayItemCount = copy != NULL ? count : 0;
}

void SetDesktopTrayActivateAction(int action) { TrayActivateAction = action; }
void SetDesktopTrayIcon(const char *icon_path) { (void)icon_path; }

#elif defined(KRYON_DESKTOP_TRAY_ENABLED)

#include <SDL.h>
#if defined(KRYON_TRAY_GTK_DL)
#include "gtk_dl.h" /* lazy GTK: no link-time GTK dependency for the tray */
#else
#include <gtk/gtk.h>
#endif
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(KRYON_DESKTOP_TRAY_AYATANA)
#include <libayatana-appindicator/app-indicator.h>
#elif defined(KRYON_DESKTOP_TRAY_APPINDICATOR)
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
static pthread_mutex_t TrayIconLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t TrayStateCond = PTHREAD_COND_INITIALIZER;
static int PendingAction;
static int TrayStarted;
static int TrayState;
static int TrayCloseAction;
static int TrayActivateAction;
static int TrayMenuUpdatePending;
static int TrayStatusUpdatePending;
static int TrayIconUpdatePending;
static char TrayIconOverride[512]; /* non-empty: emblem/alternate icon */
static char TrayTitle[128] = "App";
static char TrayIconName[128] = "application-x-executable";
static char TrayStatusText[128] = "App";
static const char *const *TrayIconPaths;
static DesktopTrayMenuState TrayMenuData;
static GtkWidget *TrayMenu;
#if defined(KRYON_DESKTOP_TRAY_GTK_STATUS_ICON)
static GtkStatusIcon *TrayStatusIcon;
#endif
#if defined(KRYON_DESKTOP_TRAY_AYATANA) || defined(KRYON_DESKTOP_TRAY_APPINDICATOR)
static AppIndicator *TrayIndicator;
#endif

static gboolean ApplyDesktopTrayMenu(gpointer user_data);
static gboolean ApplyDesktopTrayStatus(gpointer user_data);
static gboolean ApplyDesktopTrayIcon(gpointer user_data);
static const char *GetDesktopTrayIconPath(void);

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
            gtk_menu_shell_append((GtkMenuShell *)(void *)menu,
                                 gtk_separator_menu_item_new());
            continue;
        }

        item = gtk_menu_item_new_with_label(items[i].label != NULL ? items[i].label : "");
        gtk_widget_set_sensitive(item, items[i].enabled != 0);
        if(items[i].kind == DESKTOP_TRAY_MENU_ITEM_SUBMENU) {
            GtkWidget *submenu = CreateDesktopTrayGtkMenu(items[i].children,
                                                          items[i].child_count);
            gtk_menu_item_set_submenu((GtkMenuItem *)(void *)item, submenu);
        } else {
            g_signal_connect(item, "activate", G_CALLBACK(DesktopTrayMenuAction),
                             (gpointer)(intptr_t)items[i].action);
        }
        gtk_menu_shell_append((GtkMenuShell *)(void *)menu, item);
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

#if defined(KRYON_DESKTOP_TRAY_AYATANA) || defined(KRYON_DESKTOP_TRAY_APPINDICATOR)
    if(TrayIndicator != NULL)
        app_indicator_set_menu(TrayIndicator, GTK_MENU(TrayMenu));
#endif

#if defined(KRYON_DESKTOP_TRAY_GTK_STATUS_ICON)
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

#if defined(KRYON_DESKTOP_TRAY_GTK_STATUS_ICON)
    if(TrayStatusIcon != NULL) {
        gtk_status_icon_set_title(TrayStatusIcon, text);
        gtk_status_icon_set_tooltip_text(TrayStatusIcon, text);
    }
#endif
#if defined(KRYON_DESKTOP_TRAY_AYATANA) || defined(KRYON_DESKTOP_TRAY_APPINDICATOR)
    if(TrayIndicator != NULL)
        app_indicator_set_title(TrayIndicator, text);
#endif

    return G_SOURCE_REMOVE;
}

/* Swap the tray icon at runtime — the emblem/attention state (e.g. a badge
 * variant while unseen notifications wait). icon_path NULL or "" restores
 * the icon InitDesktopTray resolved. Applied on the GTK thread. */
void
SetDesktopTrayIcon(const char *icon_path)
{
    pthread_mutex_lock(&TrayIconLock);
    snprintf(TrayIconOverride, sizeof(TrayIconOverride), "%s",
             icon_path != NULL ? icon_path : "");
    if(TrayState == DESKTOP_TRAY_STATE_READY && !TrayIconUpdatePending) {
        TrayIconUpdatePending = 1;
        g_idle_add(ApplyDesktopTrayIcon, NULL);
    }
    pthread_mutex_unlock(&TrayIconLock);
}

static gboolean
ApplyDesktopTrayIcon(gpointer user_data)
{
    char path[sizeof(TrayIconOverride)];

    (void)user_data;
    pthread_mutex_lock(&TrayIconLock);
    snprintf(path, sizeof(path), "%s", TrayIconOverride);
    TrayIconUpdatePending = 0;
    pthread_mutex_unlock(&TrayIconLock);

#if defined(KRYON_DESKTOP_TRAY_GTK_STATUS_ICON)
    if(TrayStatusIcon != NULL) {
        if(path[0] != '\0') {
            gtk_status_icon_set_from_file(TrayStatusIcon, path);
        } else {
            /* Empty override restores the icon InitDesktopTray resolved. */
            const char *icon_path = GetDesktopTrayIconPath();

            if(icon_path != NULL)
                gtk_status_icon_set_from_file(TrayStatusIcon, icon_path);
            else {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
                gtk_status_icon_set_from_icon_name(TrayStatusIcon, TrayIconName);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
            }
        }
    }
#endif
#if defined(KRYON_DESKTOP_TRAY_AYATANA) || defined(KRYON_DESKTOP_TRAY_APPINDICATOR)
    if(TrayIndicator != NULL)
        app_indicator_set_icon_full(TrayIndicator,
                                    path[0] != '\0' ? path : TrayIconName,
                                    TrayTitle);
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

static int
IconFileExistsAt(char *buf, size_t bufsize, const char *dir, const char *subdir)
{
    int n;

    n = snprintf(buf, bufsize, "%s", dir);
    if(n < 0 || (size_t)n >= bufsize)
        return 0;
    if(subdir != NULL) {
        n = snprintf(buf + strlen(buf), bufsize - strlen(buf), "/%s", subdir);
        if(n < 0 || (size_t)n >= bufsize - strlen(buf))
            return 0;
    }
    n = snprintf(buf + strlen(buf), bufsize - strlen(buf), "/%s.png", TrayIconName);
    if(n < 0 || (size_t)n >= bufsize - strlen(buf))
        return 0;
    return g_file_test(buf, G_FILE_TEST_IS_REGULAR) ? 1 : 0;
}

static const char *
GetDesktopTrayIconPath(void)
{
    static char path[512];
    static char *candidate_path;
    static char exe_dir[384];
    static const char *const exe_links[] = {
#if defined(__FreeBSD__)
        "/proc/curproc/file",
#endif
#if defined(__linux__)
        "/proc/self/exe",
#endif
        NULL
    };
    /* Standard hicolor sizes, largest first (sharpest rendering). */
    static const char *const hicolor_sizes[] = {
        "512x512", "256x256", "192x192", "128x128", "96x96", "64x64", "48x48",
        "32x32", "24x24", "16x16", "scalable", NULL
    };
    exe_dir[0] = '\0';

    for(int i = 0; exe_links[i] != NULL; i++) {
        ssize_t len = readlink(exe_links[i], exe_dir, sizeof(exe_dir) - 1);
        if(len > 0 && (size_t)len < sizeof(exe_dir) - 1) {
            char *slash;
            exe_dir[len] = '\0';
            slash = strrchr(exe_dir, '/');
            if(slash != NULL) {
                *slash = '\0';
                break;
            }
            exe_dir[0] = '\0';
        }
    }

    /* 1. Icon next to the executable (e.g. portable/AppImage layout). */
    if(exe_dir[0] != '\0') {
        if(IconFileExistsAt(path, sizeof(path), exe_dir, NULL))
            return path;
    }

    /* 2. Caller-supplied candidate paths (often CWD-relative). */
    if(TrayIconPaths != NULL) {
        for(int i = 0; TrayIconPaths[i] != NULL; i++) {
            if(g_file_test(TrayIconPaths[i], G_FILE_TEST_IS_REGULAR)) {
                /* The indicator host is a different process.  It cannot
                 * resolve a path relative to this application's CWD. */
                char *absolute_path = g_canonicalize_filename(TrayIconPaths[i], NULL);
                if(absolute_path != NULL) {
                    g_free(candidate_path);
                    candidate_path = absolute_path;
                    return candidate_path;
                }
                return TrayIconPaths[i];
            }
        }
    }

    /* 3. Installed hicolor theme icons relative to the executable
     *    (<exedir>/../share/icons/hicolor/<size>/apps/<icon>.png). This is where
     *    the icon actually ships when installed via a package/AppImage. */
    if(exe_dir[0] != '\0') {
        for(int i = 0; hicolor_sizes[i] != NULL; i++) {
            char apps_dir[448];
            int n = snprintf(apps_dir, sizeof(apps_dir),
                             "%s/../share/icons/hicolor/%s/apps",
                             exe_dir, hicolor_sizes[i]);
            if(n > 0 && (size_t)n < sizeof(apps_dir) &&
               IconFileExistsAt(path, sizeof(path), apps_dir, NULL))
                return path;
        }
    }

    /* 4. System data dirs ($XDG_DATA_DIRS, default /usr/local/share:/usr/share). */
    {
        const char *data_dirs = getenv("XDG_DATA_DIRS");
        char buf[512];
        if(data_dirs == NULL || data_dirs[0] == '\0')
            data_dirs = "/usr/local/share:/usr/share";
        snprintf(buf, sizeof(buf), "%s", data_dirs);
        char *tok = strtok(buf, ":");
        while(tok != NULL) {
            for(int i = 0; hicolor_sizes[i] != NULL; i++) {
                char apps_dir[512];
                int n = snprintf(apps_dir, sizeof(apps_dir),
                                 "%s/icons/hicolor/%s/apps", tok, hicolor_sizes[i]);
                if(n > 0 && (size_t)n < sizeof(apps_dir) &&
                   IconFileExistsAt(path, sizeof(path), apps_dir, NULL))
                    return path;
            }
            tok = strtok(NULL, ":");
        }
    }

    return NULL;
}

#if defined(KRYON_DESKTOP_TRAY_GTK_STATUS_ICON)
static Uint32 TrayReadyTicks; /* SDL_GetTicks() when the icon became ready */

static void
DesktopTrayStatusIconActivate(GtkStatusIcon *status_icon, gpointer user_data)
{
    (void)status_icon;
    (void)user_data;
    /* Some systray hosts emit an activation during the XEMBED handshake.
     * Honoring it would toggle (and hide!) the app window right after
     * launch with nobody touching the icon. Ignore activates from an
     * unembedded icon or within the embed window. */
    if(TrayStatusIcon != NULL && !gtk_status_icon_is_embedded(TrayStatusIcon))
        return;
    if(TrayReadyTicks != 0 && SDL_GetTicks() - TrayReadyTicks < 2000)
        return;
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
    gtk_menu_popup((GtkMenu *)(void *)menu, NULL, NULL, gtk_status_icon_position_menu,
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

#if defined(KRYON_TRAY_GTK_DL)
    /* Pull GTK in on demand; with the tray disabled (or GTK absent) the
     * process never maps libgtk-3 or its dependency chain. */
    if(!KryonGtkEnsure()) {
        SetDesktopTrayState(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }
#endif
    if(!gtk_init_check(NULL, NULL)) {
        SetDesktopTrayState(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }

#if defined(KRYON_DESKTOP_TRAY_AYATANA) || defined(KRYON_DESKTOP_TRAY_APPINDICATOR)
    TrayIndicator = app_indicator_new(TrayIconName, TrayIconName,
                                      APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    if(TrayIndicator == NULL) {
        SetDesktopTrayState(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }

    app_indicator_set_status(TrayIndicator, APP_INDICATOR_STATUS_ACTIVE);
    {
        const char *icon_path = GetDesktopTrayIconPath();
        app_indicator_set_icon_full(TrayIndicator,
                                    icon_path != NULL ? icon_path : TrayIconName,
                                    TrayTitle);
    }

    menu = CreateDesktopTrayMenu();
    TrayMenu = menu;
    app_indicator_set_menu(TrayIndicator, GTK_MENU(menu));
#elif defined(KRYON_DESKTOP_TRAY_GTK_STATUS_ICON)
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
#if defined(KRYON_DESKTOP_TRAY_GTK_STATUS_ICON)
    TrayReadyTicks = SDL_GetTicks();
#endif
    gtk_main();
    SetDesktopTrayState(DESKTOP_TRAY_STATE_STOPPED);
    TrayMenu = NULL;
#if defined(KRYON_DESKTOP_TRAY_AYATANA) || defined(KRYON_DESKTOP_TRAY_APPINDICATOR)
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
void SetDesktopTrayIcon(const char *icon_path) { (void)icon_path; }
void SetDesktopTrayMenu(const DesktopTrayMenuItem *items, int count)
{
    (void)items;
    (void)count;
}
void SetDesktopTrayActivateAction(int action) { (void)action; }

#endif
