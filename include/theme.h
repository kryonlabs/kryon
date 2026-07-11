#ifndef THEME_H
#define THEME_H

#include "flint.h"
#include <stdbool.h>

#define THEME_MAX_SCOPES 24
#define THEME_MAX_VALUES 64
#define THEME_NAME_SIZE 64
#define THEME_PATH_SIZE 256
#define THEME_MAX_VARS 64

typedef struct ThemeValue {
    char key[THEME_NAME_SIZE];
    Color value;
} ThemeValue;

typedef struct ThemeScope {
    char name[THEME_NAME_SIZE];
    char path[THEME_PATH_SIZE];
    char dark_path[THEME_PATH_SIZE];
    ThemeValue values[THEME_MAX_VALUES];
    int count;
} ThemeScope;

typedef struct {
    char key[THEME_NAME_SIZE];
    Color value;
    bool scopes[THEME_MAX_SCOPES];
    int scope_count;
} ThemeAggregateVariable;

typedef enum {
    THEME_SOURCE_APP = 0,
    THEME_SOURCE_SYSTEM = 1
} ThemeSource;

typedef enum {
    THEME_MODE_SYSTEM = 0,
    THEME_MODE_LIGHT = 1,
    THEME_MODE_DARK = 2
} ThemeMode;

void ResetTheme(void);
ThemeScope *RegisterThemeScope(const char *name, const char *path);
ThemeScope *RegisterDarkThemeScope(const char *name, const char *path, const char *dark_path);
ThemeScope *GetThemeScope(const char *name);
const ThemeScope *GetThemeScopeAt(int index);
int GetThemeScopeCount(void);
Color GetThemeColor(const char *scope, const char *key);
bool SetThemeColor(const char *scope, const char *key, Color color);
bool SaveThemeScope(const char *scope);
bool SaveAllThemes(void);
const char *GetThemeColorText(Color color, char *buffer, int size);
bool ParseThemeColor(const char *text, Color *color);
void DrawThemeTKBorder(Rectangle rec, int borderWidth, bool raised);

void AggregateAllThemes(void);
ThemeAggregateVariable* GetThemeAggregateVars(void);
int GetThemeAggregateCount(void);
void ApplyThemeAggregate(const char *key, Color color);
bool SyncThemeFromScope(const char *src_scope);
bool SyncThemeToApps(const char *src_scope);
bool ExportTheme(const char *path);
bool ImportTheme(const char *path);

void SetThemeDarkMode(bool dark);
bool GetThemeDarkMode(void);
void ReloadThemes(void);

void SetThemeSource(ThemeSource source);
ThemeSource GetThemeSource(void);
void SetThemeMode(ThemeMode mode);
ThemeMode GetThemeMode(void);
bool IsSystemThemeAvailable(void);
const char *GetSystemThemeName(void);
const char *GetSystemThemeNameCached(void);
bool RefreshSystemTheme(void);
bool SystemThemePrefersDark(void);
void SetSystemThemeDarkMode(bool dark);
bool GetEffectiveThemeDarkMode(void);

void SetCurrentTheme(int theme_id, int dark_mode);
Color GetCurrentThemeColor(const char *key);
Color GetThemeText(void);
Color GetThemeBackground(void);
Color GetThemeSurface(void);
Color GetThemeCircle(void);
Color GetThemeButton(void);
Color GetThemeButtonHover(void);
Color GetThemeIcon(void);
Color GetThemeLink(void);

#endif
