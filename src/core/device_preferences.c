#include "device_preferences.h"
#include "kryon.h"

#if defined(PLATFORM_WEB)
#include "web.h"
#endif

int
KryEffectiveThemeDarkMode(KryThemePreference pref)
{
    if(pref.mode == THEME_MODE_LIGHT)
        return 0;
    if(pref.mode == THEME_MODE_DARK)
        return 1;
    if(pref.system_dark_mode != 0)
        return pref.system_dark_mode(pref.user) ? 1 : 0;
    return GetEffectiveThemeDarkMode() ? 1 : 0;
}

int
KryApplyThemePreference(KryThemePreference pref)
{
    int dark;

    SetThemeSource(pref.source);
    SetThemeMode(pref.mode);
    SetThemeStyle(pref.style);
    if(pref.system_dark_mode != 0)
        SetSystemThemeDarkMode(pref.system_dark_mode(pref.user) != 0);
    dark = KryEffectiveThemeDarkMode(pref);
    if(pref.system_dark_mode != 0 && pref.source == THEME_SOURCE_SYSTEM)
        SetSystemThemeDarkMode(dark != 0);
    SetCurrentTheme(pref.theme_id, dark);
    return dark;
}

int
KryApplyOrientationPreference(KryOrientationPreference pref)
{
    int width = GetScreenWidth();
    int height = GetScreenHeight();

    if(pref.request_size != 0 && width > 0 && height > 0) {
        if(pref.mode == pref.portrait_mode && width > height) {
            pref.request_size(pref.user, height, width);
            return 1;
        }
        if(pref.mode == pref.landscape_mode && height > width) {
            pref.request_size(pref.user, height, width);
            return 1;
        }
    }
    if(pref.set_platform_orientation != 0) {
        pref.set_platform_orientation(pref.user, pref.mode);
        return 1;
    }
#if defined(PLATFORM_WEB)
    SetWebOrientationMode(pref.mode);
    SyncWebWindowSize();
    return 1;
#else
    if(pref.mode == pref.portrait_mode && width > height) {
        SetWindowSize(height, width);
        return 1;
    }
    if(pref.mode == pref.landscape_mode && height > width) {
        SetWindowSize(height, width);
        return 1;
    }
    return 0;
#endif
}
