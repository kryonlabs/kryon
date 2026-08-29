#include "android_surface.h"

#include "kryon_compat.generated.h"
#include "platform.h"

#include <stddef.h>
#include <string.h>

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
#include <android/native_window.h>
#include <stdbool.h>

void rlLoadIdentity(void);
void rlMatrixMode(int mode);
void rlOrtho(double left, double right, double bottom, double top,
             double znear, double zfar);
void rlViewport(int x, int y, int width, int height);

#define KRY_RL_MODELVIEW 0x1700
#define KRY_RL_PROJECTION 0x1701

typedef struct { int x; int y; } KryRaylibCorePoint;
typedef struct { unsigned int width; unsigned int height; } KryRaylibCoreSize;

typedef struct KryRaylibCoreData {
    struct {
        const char *title;
        unsigned int flags;
        bool ready;
        bool shouldClose;
        bool resizedLastFrame;
        bool eventWaiting;
        bool usingFbo;
        KryRaylibCoreSize display;
        KryRaylibCoreSize screen;
        KryRaylibCorePoint position;
        KryRaylibCoreSize previousScreen;
        KryRaylibCorePoint previousPosition;
        KryRaylibCoreSize render;
        KryRaylibCorePoint renderOffset;
        KryRaylibCoreSize currentFbo;
        KryRaylibCoreSize screenMin;
        KryRaylibCoreSize screenMax;
        Matrix screenScale;
    } Window;
} KryRaylibCoreData;

extern KryRaylibCoreData CORE;
#endif

static int g_android_surface_w;
static int g_android_surface_h;
static KryMutex g_android_window_insets_mutex = KRY_MUTEX_INIT;
static AndroidWindowInsets g_android_window_insets;
static int g_android_window_insets_ready;

static int
android_nonnegative(int value)
{
    return value > 0 ? value : 0;
}

static int
android_max_nonnegative(int first, int second)
{
    first = android_nonnegative(first);
    second = android_nonnegative(second);
    return first > second ? first : second;
}

int
GetAndroidSurfaceSize(int *width, int *height)
{
    int w = g_android_surface_w > 0 ? g_android_surface_w : GetScreenWidth();
    int h = g_android_surface_h > 0 ? g_android_surface_h : GetScreenHeight();

    if(width != NULL)
        *width = w;
    if(height != NULL)
        *height = h;
    return g_android_surface_w > 0 && g_android_surface_h > 0;
}

void
SetAndroidWindowInsets(int system_left, int system_top,
                       int system_right, int system_bottom,
                       int ime_bottom,
                       int cutout_left, int cutout_top,
                       int cutout_right, int cutout_bottom)
{
    KryMutexLock(&g_android_window_insets_mutex);
    g_android_window_insets.system_left = android_nonnegative(system_left);
    g_android_window_insets.system_top = android_nonnegative(system_top);
    g_android_window_insets.system_right = android_nonnegative(system_right);
    g_android_window_insets.system_bottom = android_nonnegative(system_bottom);
    g_android_window_insets.ime_bottom = android_nonnegative(ime_bottom);
    g_android_window_insets.cutout_left = android_nonnegative(cutout_left);
    g_android_window_insets.cutout_top = android_nonnegative(cutout_top);
    g_android_window_insets.cutout_right = android_nonnegative(cutout_right);
    g_android_window_insets.cutout_bottom = android_nonnegative(cutout_bottom);
    g_android_window_insets_ready = 1;
    KryMutexUnlock(&g_android_window_insets_mutex);
}

int
GetAndroidWindowInsets(AndroidWindowInsets *out)
{
    int ready;

    KryMutexLock(&g_android_window_insets_mutex);
    if(out != NULL)
        memcpy(out, &g_android_window_insets, sizeof(*out));
    ready = g_android_window_insets_ready;
    KryMutexUnlock(&g_android_window_insets_mutex);
    return ready;
}

KrySafeArea
GetAndroidSafeArea(void)
{
    AndroidWindowInsets insets;
    KrySafeArea area;

    GetAndroidWindowInsets(&insets);
    area.left = android_max_nonnegative(insets.system_left, insets.cutout_left);
    area.top = android_max_nonnegative(insets.system_top, insets.cutout_top);
    area.right = android_max_nonnegative(insets.system_right, insets.cutout_right);
    area.bottom = android_max_nonnegative(insets.system_bottom,
                                          insets.cutout_bottom);
    return area;
}

int
SyncAndroidSurfaceSize(int *width, int *height)
{
    int w = GetScreenWidth();
    int h = GetScreenHeight();

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    ANativeWindow *window = (ANativeWindow *)GetWindowHandle();
    if(window != NULL) {
        int format = ANativeWindow_getFormat(window);
        int changed;

        ANativeWindow_setBuffersGeometry(window, 0, 0, format);
        w = ANativeWindow_getWidth(window);
        h = ANativeWindow_getHeight(window);
        if(w > 0 && h > 0) {
            changed = g_android_surface_w != w || g_android_surface_h != h ||
                      (int)CORE.Window.screen.width != w ||
                      (int)CORE.Window.screen.height != h ||
                      (int)CORE.Window.render.width != w ||
                      (int)CORE.Window.render.height != h;
            g_android_surface_w = w;
            g_android_surface_h = h;
            CORE.Window.display.width = (unsigned int)w;
            CORE.Window.display.height = (unsigned int)h;
            CORE.Window.screen.width = (unsigned int)w;
            CORE.Window.screen.height = (unsigned int)h;
            CORE.Window.render.width = (unsigned int)w;
            CORE.Window.render.height = (unsigned int)h;
            if(!CORE.Window.usingFbo) {
                CORE.Window.currentFbo.width = (unsigned int)w;
                CORE.Window.currentFbo.height = (unsigned int)h;
            }
            CORE.Window.resizedLastFrame = changed;
            rlViewport(0, 0, w, h);
            rlMatrixMode(KRY_RL_PROJECTION);
            rlLoadIdentity();
            rlOrtho(0, w, h, 0, 0.0, 1.0);
            rlMatrixMode(KRY_RL_MODELVIEW);
            rlLoadIdentity();
        }
    }
#endif

    if(width != NULL)
        *width = w;
    if(height != NULL)
        *height = h;

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    return w > 0 && h > 0;
#else
    return 0;
#endif
}
