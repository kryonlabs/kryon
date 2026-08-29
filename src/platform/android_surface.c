#include "android_surface.h"

#include "kryon_compat.generated.h"

#include <stddef.h>

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
