#include "android_surface.h"

#include "kryon_compat.generated.h"

#include <stddef.h>

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
#include <android/native_window.h>

void rlLoadIdentity(void);
void rlMatrixMode(int mode);
void rlOrtho(double left, double right, double bottom, double top,
             double znear, double zfar);
void rlViewport(int x, int y, int width, int height);

#define KRY_RL_MODELVIEW 0x1700
#define KRY_RL_PROJECTION 0x1701
#endif

int
SyncAndroidSurfaceSize(int *width, int *height)
{
    int w = GetScreenWidth();
    int h = GetScreenHeight();

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    ANativeWindow *window = (ANativeWindow *)GetWindowHandle();
    if(window != NULL) {
        int format = ANativeWindow_getFormat(window);

        ANativeWindow_setBuffersGeometry(window, 0, 0, format);
        w = ANativeWindow_getWidth(window);
        h = ANativeWindow_getHeight(window);
        if(w > 0 && h > 0) {
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
