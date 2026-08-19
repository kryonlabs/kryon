#include "kry_activity_monitor.h"

#if defined(__linux__) || defined(__FreeBSD__)

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(KRYON_NOTIFICATION_GDBUS)
#include <gio/gio.h>
#endif

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;

enum { GrabModeSync = 0, GrabModeAsync = 1 };

typedef struct {
    Window window;
    int state;
    int kind;
    unsigned long til_or_since;
    unsigned long idle;
    unsigned long eventMask;
} KryXScreenSaverInfo;

typedef Display *(*KryXOpenDisplay)(const char *display_name);
typedef int (*KryXCloseDisplay)(Display *display);
typedef Window (*KryXDefaultRootWindow)(Display *display);
typedef int (*KryXGrabKeyboard)(Display *display, Window grab_window,
                                int owner_events, int pointer_mode,
                                int keyboard_mode, unsigned long time);
typedef int (*KryXUngrabKeyboard)(Display *display, unsigned long time);
typedef int (*KryXSync)(Display *display, int discard);
typedef KryXScreenSaverInfo *(*KryXScreenSaverAllocInfo)(void);
typedef int (*KryXScreenSaverQueryInfo)(Display *display, Drawable drawable,
                                        KryXScreenSaverInfo *info);

static Display *g_display;
static void *g_x11;
static void *g_xss;
static KryXScreenSaverAllocInfo g_ss_alloc;
static KryXScreenSaverQueryInfo g_ss_query;
static KryXDefaultRootWindow g_root_window;
static KryXGrabKeyboard g_grab_keyboard;
static KryXUngrabKeyboard g_ungrab_keyboard;
static KryXSync g_sync;
static int g_inited;
static int g_available;
static int g_blocked;

#if defined(KRYON_NOTIFICATION_GDBUS)
enum {
    KryWaylandIdleNone = 0,
    KryWaylandIdleMutter,
    KryWaylandIdleScreenSaver
};
static GDBusConnection *g_wayland_bus;
static int g_wayland_idle_backend;

static long
wayland_idle_call(const char *bus_name, const char *object_path,
                  const char *interface_name, const char *method)
{
    GError *error = 0;
    GVariant *reply;
    GVariant *value;
    guint64 idle = 0;

    if(g_wayland_bus == 0)
        return -1;
    reply = g_dbus_connection_call_sync(g_wayland_bus, bus_name, object_path,
                                        interface_name, method, 0, 0,
                                        G_DBUS_CALL_FLAGS_NONE, 500, 0,
                                        &error);
    if(reply == 0) {
        if(error != 0)
            g_error_free(error);
        return -1;
    }
    if(g_variant_n_children(reply) != 1) {
        g_variant_unref(reply);
        return -1;
    }
    value = g_variant_get_child_value(reply, 0);
    if(g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64))
        idle = g_variant_get_uint64(value);
    else if(g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
        idle = g_variant_get_uint32(value);
    else {
        g_variant_unref(value);
        g_variant_unref(reply);
        return -1;
    }
    g_variant_unref(value);
    g_variant_unref(reply);
    return idle > LONG_MAX ? LONG_MAX : (long)idle;
}

static void
wayland_idle_init(void)
{
    GError *error = 0;

    g_wayland_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, 0, &error);
    if(g_wayland_bus == 0) {
        if(error != 0)
            g_error_free(error);
        return;
    }
    if(wayland_idle_call("org.gnome.Mutter.IdleMonitor",
                         "/org/gnome/Mutter/IdleMonitor/Core",
                         "org.gnome.Mutter.IdleMonitor", "GetIdletime") >= 0) {
        g_wayland_idle_backend = KryWaylandIdleMutter;
        return;
    }
    if(wayland_idle_call("org.freedesktop.ScreenSaver", "/ScreenSaver",
                         "org.freedesktop.ScreenSaver",
                         "GetSessionIdleTime") >= 0) {
        g_wayland_idle_backend = KryWaylandIdleScreenSaver;
    }
}

static long
wayland_idle_get_ms(void)
{
    if(g_wayland_idle_backend == KryWaylandIdleMutter)
        return wayland_idle_call("org.gnome.Mutter.IdleMonitor",
                                 "/org/gnome/Mutter/IdleMonitor/Core",
                                 "org.gnome.Mutter.IdleMonitor", "GetIdletime");
    if(g_wayland_idle_backend == KryWaylandIdleScreenSaver)
        return wayland_idle_call("org.freedesktop.ScreenSaver", "/ScreenSaver",
                                 "org.freedesktop.ScreenSaver",
                                 "GetSessionIdleTime");
    return -1;
}
#endif

static void *
resolve(void *handle, const char *name)
{
    return handle != 0 ? dlsym(handle, name) : 0;
}

static void *
load_library(const char *const *names)
{
    for(int i = 0; names[i] != 0; i++) {
        void *handle = dlopen(names[i], RTLD_LAZY | RTLD_LOCAL);

        if(handle != 0)
            return handle;
    }
    return 0;
}

static int
session_is_wayland(void)
{
    const char *type = getenv("XDG_SESSION_TYPE");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");

    return (type != 0 && strcmp(type, "wayland") == 0) ||
           (wayland_display != 0 && wayland_display[0] != '\0');
}

void
KryActivityMonitorInit(void)
{
    static const char *const x11_names[] = {"libX11.so.6", "libX11.so", 0};
    static const char *const xss_names[] = {"libXss.so.1", "libXss.so", 0};
    KryXOpenDisplay open_display;

    if(g_inited)
        return;
    g_inited = 1;
    if(session_is_wayland()) {
#if defined(KRYON_NOTIFICATION_GDBUS)
        wayland_idle_init();
        g_available = g_wayland_idle_backend != KryWaylandIdleNone;
#endif
        return;
    }
    g_x11 = load_library(x11_names);
    g_xss = load_library(xss_names);
    if(g_x11 == 0 || g_xss == 0)
        return;
    open_display = (KryXOpenDisplay)resolve(g_x11, "XOpenDisplay");
    g_root_window = (KryXDefaultRootWindow)resolve(g_x11, "XDefaultRootWindow");
    g_grab_keyboard = (KryXGrabKeyboard)resolve(g_x11, "XGrabKeyboard");
    g_ungrab_keyboard = (KryXUngrabKeyboard)resolve(g_x11, "XUngrabKeyboard");
    g_sync = (KryXSync)resolve(g_x11, "XSync");
    g_ss_alloc = (KryXScreenSaverAllocInfo)resolve(g_xss,
                                                  "XScreenSaverAllocInfo");
    g_ss_query = (KryXScreenSaverQueryInfo)resolve(g_xss,
                                                  "XScreenSaverQueryInfo");
    if(open_display == 0 || g_ss_alloc == 0 || g_ss_query == 0 ||
       g_root_window == 0) {
        g_grab_keyboard = 0;
        g_ungrab_keyboard = 0;
        g_sync = 0;
        return;
    }
    g_display = open_display(0);
    if(g_display != 0)
        g_available = 1;
}

int
KryActivityIsWayland(void)
{
    return session_is_wayland();
}

int
KryActivityAvailable(void)
{
    if(!g_inited)
        KryActivityMonitorInit();
    return g_available;
}

long
KryActivityGetIdleMilliseconds(void)
{
    KryXScreenSaverInfo *info;
    long idle;

    if(!KryActivityAvailable())
        return -1;
#if defined(KRYON_NOTIFICATION_GDBUS)
    if(session_is_wayland())
        return wayland_idle_get_ms();
#endif
    if(g_display == 0 || g_ss_alloc == 0 || g_ss_query == 0 ||
       g_root_window == 0)
        return -1;
    info = g_ss_alloc();
    if(info == 0)
        return -1;
    if(!g_ss_query(g_display, g_root_window(g_display), info)) {
        free(info);
        return -1;
    }
    idle = info->idle > LONG_MAX ? LONG_MAX : (long)info->idle;
    free(info);
    return idle;
}

int
KryActivitySetInputBlocked(int on)
{
    int ok;

    if(!KryActivityAvailable() || session_is_wayland() || g_display == 0 ||
       g_root_window == 0 || g_grab_keyboard == 0 || g_ungrab_keyboard == 0)
        return 0;
    if(on) {
        if(g_blocked)
            return 1;
        ok = g_grab_keyboard(g_display, g_root_window(g_display), 1,
                             GrabModeAsync, GrabModeAsync, 0) == 0;
        if(ok) {
            g_blocked = 1;
            if(g_sync != 0)
                g_sync(g_display, 0);
        }
        return ok ? 1 : 0;
    }
    if(!g_blocked)
        return 1;
    ok = g_ungrab_keyboard(g_display, 0) == 0;
    if(ok) {
        g_blocked = 0;
        if(g_sync != 0)
            g_sync(g_display, 0);
    }
    return ok ? 1 : 0;
}

int
KryActivityInputBlocked(void)
{
    return g_blocked;
}

#else

void KryActivityMonitorInit(void) {}
int KryActivityIsWayland(void) { return 0; }
int KryActivityAvailable(void) { return 0; }
long KryActivityGetIdleMilliseconds(void) { return -1; }
int KryActivitySetInputBlocked(int on) { (void)on; return 0; }
int KryActivityInputBlocked(void) { return 0; }

#endif
