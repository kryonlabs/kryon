#ifndef KRYON_ANDROID_SURFACE_H
#define KRYON_ANDROID_SURFACE_H

/* Synchronize the active Android native surface with the current device
 * orientation. On Android this refreshes the native buffer geometry, resets
 * Kryon's GL viewport/projection to the actual surface size, stores that size
 * in width/height when provided, and returns 1. Other platforms leave the
 * current Kryon screen size in width/height and return 0. */
int SyncAndroidSurfaceSize(int *width, int *height);

#endif /* KRYON_ANDROID_SURFACE_H */
