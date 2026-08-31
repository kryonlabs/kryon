#ifndef KRYON_ANDROID_SURFACE_H
#define KRYON_ANDROID_SURFACE_H

#include "kry_capabilities.h"

typedef struct AndroidWindowInsets {
    int system_left;
    int system_top;
    int system_right;
    int system_bottom;
    int ime_bottom;
    int cutout_left;
    int cutout_top;
    int cutout_right;
    int cutout_bottom;
} AndroidWindowInsets;

typedef struct AndroidSafeArea {
    int left;
    int top;
    int right;
    int bottom;
} AndroidSafeArea;

typedef struct AndroidViewport {
    int x;
    int y;
    int width;
    int height;
    AndroidSafeArea insets;
    int ready;
} AndroidViewport;

typedef enum AndroidViewportInset {
    ANDROID_VIEWPORT_INSET_NONE = 0,
    ANDROID_VIEWPORT_INSET_SYSTEM_BARS = 1 << 0,
    ANDROID_VIEWPORT_INSET_CUTOUT = 1 << 1,
    ANDROID_VIEWPORT_INSET_IME = 1 << 2,
    ANDROID_VIEWPORT_INSET_SAFE_AREA = ANDROID_VIEWPORT_INSET_SYSTEM_BARS |
                                       ANDROID_VIEWPORT_INSET_CUTOUT
} AndroidViewportInset;

typedef struct AndroidViewportPolicy {
    int insets;
    int min_width;
    int min_height;
} AndroidViewportPolicy;

/* Synchronize the active Android native surface with the current device
 * orientation. On Android this refreshes the native buffer geometry, resets
 * Kryon's GL viewport/projection to the actual surface size, stores that size
 * in width/height when provided, and returns 1. Other platforms leave the
 * current Kryon screen size in width/height and return 0. */
int SyncAndroidSurfaceSize(int *width, int *height);
int GetAndroidSurfaceSize(int *width, int *height);

/* Directional Android window insets reported by app Java/JNI glue. Values are
 * Android physical pixels. Non-Android builds keep a zero inset state. */
void SetAndroidWindowInsets(int system_left, int system_top,
                            int system_right, int system_bottom,
                            int ime_bottom,
                            int cutout_left, int cutout_top,
                            int cutout_right, int cutout_bottom);
int GetAndroidWindowInsets(AndroidWindowInsets *out);
AndroidSafeArea GetAndroidSafeAreaInsets(void);
AndroidViewportPolicy AndroidViewportPolicyFull(void);
AndroidViewportPolicy AndroidViewportPolicySafeArea(void);
AndroidViewportPolicy AndroidViewportPolicyResizeForIme(void);
void SetAndroidViewportPolicy(AndroidViewportPolicy policy);
AndroidViewportPolicy GetAndroidViewportPolicy(void);
int ResolveAndroidViewport(int width, int height, AndroidViewportPolicy policy,
                           AndroidViewport *out);
int SyncAndroidViewport(AndroidViewport *out);
KrySafeArea GetAndroidSafeArea(void);

#endif /* KRYON_ANDROID_SURFACE_H */
