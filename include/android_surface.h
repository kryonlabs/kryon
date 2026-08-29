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
KrySafeArea GetAndroidSafeArea(void);

#endif /* KRYON_ANDROID_SURFACE_H */
