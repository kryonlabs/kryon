#ifndef KRYON_DEVICE_PREFERENCES_H
#define KRYON_DEVICE_PREFERENCES_H

#include "theme.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*KrySystemDarkModeFn)(void *user);
typedef void (*KrySetOrientationModeFn)(void *user, int mode);
typedef void (*KryRequestWindowSizeFn)(void *user, int width, int height);

typedef struct KryThemePreference {
    int theme_id;
    ThemeSource source;
    ThemeMode mode;
    ThemeStyle style;
    KrySystemDarkModeFn system_dark_mode;
    void *user;
} KryThemePreference;

typedef struct KryOrientationPreference {
    int mode;
    int portrait_mode;
    int landscape_mode;
    KryRequestWindowSizeFn request_size;
    KrySetOrientationModeFn set_platform_orientation;
    void *user;
} KryOrientationPreference;

int KryApplyThemePreference(KryThemePreference pref);
int KryEffectiveThemeDarkMode(KryThemePreference pref);
int KryApplyOrientationPreference(KryOrientationPreference pref);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_DEVICE_PREFERENCES_H */
