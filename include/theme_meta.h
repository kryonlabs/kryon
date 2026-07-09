#ifndef THEME_META_H
#define THEME_META_H

#include "raylib.h"

#include <stdbool.h>

#define THEME_COUNT 12

typedef enum {
    THEME_SKY = 0,
    THEME_OCEAN = 1,
    THEME_FOREST = 2,
    THEME_SUNSET = 3,
    THEME_LAVENDER = 4,
    THEME_CHERRY = 5,
    THEME_DAWN = 6,
    THEME_SAGE = 7,
    THEME_INK = 8,
    THEME_MONO = 9,
    THEME_MINT = 10,
    THEME_COBALT = 11
} ThemeId;

typedef struct {
    const char *name;
    const char *light_scope;
    const char *dark_scope;
} ThemeMeta;

extern const ThemeMeta themes[THEME_COUNT];

const ThemeMeta *GetThemeMeta(ThemeId theme);
ThemeId NormalizeTheme(int theme);
const char *GetThemeLabel(ThemeId theme);
const char *GetThemeScopeName(ThemeId theme, bool dark_mode);
bool GetThemeCatalogColor(ThemeId theme, bool dark_mode, const char *key, Color *color);
bool GetThemeCatalogScopeColor(const char *scope, const char *key, Color *color);

#endif
