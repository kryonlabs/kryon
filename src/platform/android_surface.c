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

static int
android_clamp_content_size(int size, int leading_inset, int trailing_inset,
                           int min_size)
{
    int content_size = size - leading_inset - trailing_inset;

    if(content_size <= 0)
        return size;
    if(min_size > 0 && content_size < min_size)
        return size;

    return content_size;
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

static AndroidSafeArea
android_safe_area_for_policy(AndroidWindowInsets insets,
                             AndroidViewportPolicy policy)
{
    AndroidSafeArea area = {0};

    if((policy.insets & ANDROID_VIEWPORT_INSET_SYSTEM_BARS) != 0) {
        area.left = android_max_nonnegative(area.left, insets.system_left);
        area.top = android_max_nonnegative(area.top, insets.system_top);
        area.right = android_max_nonnegative(area.right, insets.system_right);
        area.bottom = android_max_nonnegative(area.bottom, insets.system_bottom);
    }
    if((policy.insets & ANDROID_VIEWPORT_INSET_CUTOUT) != 0) {
        area.left = android_max_nonnegative(area.left, insets.cutout_left);
        area.top = android_max_nonnegative(area.top, insets.cutout_top);
        area.right = android_max_nonnegative(area.right, insets.cutout_right);
        area.bottom = android_max_nonnegative(area.bottom, insets.cutout_bottom);
    }
    if((policy.insets & ANDROID_VIEWPORT_INSET_IME) != 0)
        area.bottom = android_max_nonnegative(area.bottom, insets.ime_bottom);
    return area;
}

AndroidSafeArea
GetAndroidSafeAreaInsets(void)
{
    AndroidWindowInsets insets;

    GetAndroidWindowInsets(&insets);
    return android_safe_area_for_policy(insets, AndroidViewportPolicySafeArea());
}

AndroidViewportPolicy
AndroidViewportPolicyFull(void)
{
    AndroidViewportPolicy policy = {0};

    policy.insets = ANDROID_VIEWPORT_INSET_NONE;
    return policy;
}

AndroidViewportPolicy
AndroidViewportPolicySafeArea(void)
{
    AndroidViewportPolicy policy = {0};

    policy.insets = ANDROID_VIEWPORT_INSET_SAFE_AREA;
    return policy;
}

AndroidViewportPolicy
AndroidViewportPolicyResizeForIme(void)
{
    AndroidViewportPolicy policy = {0};

    policy.insets = ANDROID_VIEWPORT_INSET_SAFE_AREA |
                    ANDROID_VIEWPORT_INSET_IME;
    return policy;
}

int
ResolveAndroidViewport(int width, int height, AndroidViewportPolicy policy,
                       AndroidViewport *out)
{
    AndroidWindowInsets insets = {0};
    AndroidSafeArea area;
    AndroidViewport viewport;
    int ready;

    ready = GetAndroidWindowInsets(&insets);
    area = android_safe_area_for_policy(insets, policy);
    viewport.x = area.left;
    viewport.y = area.top;
    viewport.width = android_clamp_content_size(width, area.left, area.right,
                                                policy.min_width);
    viewport.height = android_clamp_content_size(height, area.top, area.bottom,
                                                 policy.min_height);
    if(viewport.width == width && area.left + area.right > 0)
        viewport.x = 0;
    if(viewport.height == height && area.top + area.bottom > 0)
        viewport.y = 0;
    viewport.insets = area;
    viewport.ready = ready;

    if(viewport.width <= 0) {
        viewport.x = 0;
        viewport.width = width;
    }
    if(viewport.height <= 0) {
        viewport.y = 0;
        viewport.height = height;
    }

    if(out != NULL)
        *out = viewport;
    return ready;
}

KrySafeArea
GetAndroidSafeArea(void)
{
    AndroidSafeArea safe = GetAndroidSafeAreaInsets();
    KrySafeArea area;

    area.left = safe.left;
    area.top = safe.top;
    area.right = safe.right;
    area.bottom = safe.bottom;
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
