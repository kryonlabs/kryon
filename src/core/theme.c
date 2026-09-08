#include "theme.h"
#include "runtime/theme.h"
#include "embedded_assets.h"
#include "locale.h"
#include "theme_meta.h"
#include "ui_core.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

float g_theme_content_alpha = 1.0f;

static ThemeScope scopes[THEME_MAX_SCOPES];
static int scope_count = 0;
static bool dark_mode = false;
#if defined(KRYON_BACKEND_LIBDRAW)
#if defined(KRYON_PLATFORM_PLAN9)
static int current_theme_id = THEME_PLAN9;
#else
static int current_theme_id = THEME_MONO;
#endif
#if defined(KRYON_PLATFORM_PLAN9)
static ThemeSource theme_source = THEME_SOURCE_SYSTEM;
static ThemeMode theme_mode = THEME_MODE_SYSTEM;
static ThemeStyle theme_style = THEME_STYLE_SYSTEM;
#else
static ThemeSource theme_source = THEME_SOURCE_SYSTEM;
static ThemeMode theme_mode = THEME_MODE_SYSTEM;
static ThemeStyle theme_style = THEME_STYLE_SYSTEM;
#endif

#else
static int current_theme_id = THEME_MONO;
static ThemeSource theme_source = THEME_SOURCE_SYSTEM;
static ThemeMode theme_mode = THEME_MODE_SYSTEM;
static ThemeStyle theme_style = THEME_STYLE_SYSTEM;
#endif

static Theme active_theme;
static int active_theme_set = 0;
static char active_theme_name[THEME_NAME_SIZE];
static ThemeFamily active_theme_family;
static int active_theme_family_set = 0;
static char active_theme_family_name[THEME_NAME_SIZE];
static char active_theme_light_name[THEME_NAME_SIZE];
static char active_theme_dark_name[THEME_NAME_SIZE];

static ThemeAggregateVariable aggregate_vars[THEME_MAX_VARS];
static int aggregate_count = 0;

bool SystemThemeColor(const char *key, Color *color);
ThemeStyle GetSystemThemeStyle(void);

static void copy_text(char *dst, int size, const char *src)
{
    snprintf(dst, (size_t)size, "%s", src ? src : "");
}

static int hex_value(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static unsigned char hex_byte(const char *text)
{
    int hi = hex_value(text[0]);
    int lo = hex_value(text[1]);
    if(hi < 0 || lo < 0)
        return 0;
    return (unsigned char)(hi * 16 + lo);
}

bool ParseThemeColor(const char *text, Color *color)
{
    int len;

    if(text == NULL || color == NULL)
        return false;
    if(text[0] == '#')
        text++;

    len = (int)strlen(text);
    if(len != 6 && len != 8)
        return false;

    for(int i = 0; i < len; i++) {
        if(hex_value(text[i]) < 0)
            return false;
    }

    color->r = hex_byte(text);
    color->g = hex_byte(text + 2);
    color->b = hex_byte(text + 4);
    color->a = (len == 8) ? hex_byte(text + 6) : 255;
    return true;
}

const char *GetThemeColorText(Color color, char *buffer, int size)
{
    if(buffer == NULL || size <= 0)
        return "";

    snprintf(buffer, (size_t)size, "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return buffer;
}

void ResetTheme(void)
{
    memset(scopes, 0, sizeof(scopes));
    scope_count = 0;
    active_theme_set = 0;
    active_theme_family_set = 0;
    ClearThemeMetricsOverride();
}

ThemeScope *GetThemeScope(const char *name)
{
    if(name == NULL)
        return NULL;

    for(int i = 0; i < scope_count; i++) {
        if(strcmp(scopes[i].name, name) == 0)
            return &scopes[i];
    }
    return NULL;
}

const ThemeScope *GetThemeScopeAt(int index)
{
    if(index < 0 || index >= scope_count)
        return NULL;
    return &scopes[index];
}

int GetThemeScopeCount(void)
{
    return scope_count;
}

static ThemeValue *scope_value(ThemeScope *scope, const char *key)
{
    if(scope == NULL || key == NULL)
        return NULL;

    for(int i = 0; i < scope->count; i++) {
        if(strcmp(scope->values[i].key, key) == 0)
            return &scope->values[i];
    }
    return NULL;
}

static ThemeValue *scope_add_value(ThemeScope *scope, const char *key, Color color)
{
    ThemeValue *value;

    if(scope == NULL || key == NULL || key[0] == '\0')
        return NULL;

    value = scope_value(scope, key);
    if(value != NULL) {
        value->value = color;
        return value;
    }

    if(scope->count >= THEME_MAX_VALUES)
        return NULL;

    value = &scope->values[scope->count++];
    memset(value, 0, sizeof(*value));
    copy_text(value->key, THEME_NAME_SIZE, key);
    value->value = color;
    return value;
}

static void load_scope_file(ThemeScope *scope)
{
    FILE *file;
    char line[256];
    char *embedded_text = NULL;
    char *embedded_cursor = NULL;

    if(scope == NULL || scope->path[0] == '\0')
        return;

    embedded_text = LoadEmbeddedAssetText(scope->path);
    if(embedded_text != NULL)
        embedded_cursor = embedded_text;

#if !defined(UI_EMBEDDED_ONLY)
    file = fopen(scope->path, "r");
    if(file == NULL && embedded_text == NULL) {
        fprintf(stderr, "warning: could not load theme file '%s' for scope '%s'\n",
                scope->path, scope->name);
        return;
    }
#else
    file = NULL;
    if(embedded_text == NULL)
        return;
#endif

    while(embedded_text != NULL || fgets(line, sizeof(line), file) != NULL) {
        char key[THEME_NAME_SIZE];
        char value[32];
        char *cursor = line;
        int key_len = 0;
        Color color;

        if(embedded_text != NULL) {
            char *end = strpbrk(embedded_cursor, "\r\n");
            size_t len;

            if(*embedded_cursor == '\0')
                break;

            len = end != NULL ? (size_t)(end - embedded_cursor) : strlen(embedded_cursor);
            if(len >= sizeof(line))
                len = sizeof(line) - 1;
            memcpy(line, embedded_cursor, len);
            line[len] = '\0';

            if(end != NULL) {
                embedded_cursor = end + 1;
                if((*end == '\r' && *embedded_cursor == '\n') ||
                   (*end == '\n' && *embedded_cursor == '\r'))
                    embedded_cursor++;
            } else {
                embedded_cursor += strlen(embedded_cursor);
            }
        }

        while(isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == '#' || *cursor == '\0')
            continue;

        while(*cursor != '\0' && !isspace((unsigned char)*cursor) &&
              key_len < THEME_NAME_SIZE - 1) {
            key[key_len++] = *cursor++;
        }
        key[key_len] = '\0';

        while(isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == '"')
            cursor++;

        int value_len = 0;
        while(*cursor != '\0' && *cursor != '"' && *cursor != '\n' &&
              !isspace((unsigned char)*cursor) && value_len < (int)sizeof(value) - 1) {
            value[value_len++] = *cursor++;
        }
        value[value_len] = '\0';

        if(ParseThemeColor(value, &color))
            scope_add_value(scope, key, color);
    }

    if(file != NULL)
        fclose(file);
    free(embedded_text);
}

ThemeScope *RegisterThemeScope(const char *name, const char *path)
{
    ThemeScope *scope = GetThemeScope(name);
    if(scope == NULL) {
        if(scope_count >= THEME_MAX_SCOPES)
            return NULL;
        scope = &scopes[scope_count++];
    }

    memset(scope, 0, sizeof(*scope));
    copy_text(scope->name, THEME_NAME_SIZE, name);
    copy_text(scope->path, THEME_PATH_SIZE, path);

    // Auto-generate dark path by inserting "_dark" before extension
    if(path != NULL && path[0] != '\0') {
        char *dot = strrchr((char *)path, '.');
        if(dot != NULL && dot > path) {
            int base_len = (int)(dot - path);
            if(base_len < THEME_PATH_SIZE - 10) {
                snprintf(scope->dark_path, THEME_PATH_SIZE, "%.*s_dark%s", base_len, path, dot);
            }
        }
    }

    load_scope_file(scope);
    return scope;
}

ThemeScope *RegisterDarkThemeScope(const char *name, const char *path, const char *dark_path)
{
    ThemeScope *scope = GetThemeScope(name);
    if(scope == NULL) {
        if(scope_count >= THEME_MAX_SCOPES)
            return NULL;
        scope = &scopes[scope_count++];
    }

    memset(scope, 0, sizeof(*scope));
    copy_text(scope->name, THEME_NAME_SIZE, name);
    copy_text(scope->path, THEME_PATH_SIZE, path);
    copy_text(scope->dark_path, THEME_PATH_SIZE, dark_path);
    load_scope_file(scope);
    return scope;
}

Color GetThemeColor(const char *scope_name, const char *key)
{
    Color catalog_color;
    if(GetThemeCatalogScopeColor(scope_name, key, &catalog_color))
        return catalog_color;

    ThemeValue *value = scope_value(GetThemeScope(scope_name), key);
    if(value != NULL)
        return value->value;

    value = scope_value(GetThemeScope("kryon"), key);
    if(value != NULL)
        return value->value;

    value = scope_value(GetThemeScope("default"), key);
    if(value != NULL)
        return value->value;

    for(int i = 0; i < scope_count; i++) {
        value = scope_value(&scopes[i], key);
        if(value != NULL)
            return value->value;
    }

    fprintf(stderr, "warning: missing theme color: %s.%s, using fallback\n",
            scope_name != NULL ? scope_name : "(null)",
            key != NULL ? key : "(null)");

    if(key != NULL) {
        if(strstr(key, "background") != NULL || strstr(key, "paper") != NULL)
            return (Color){0xE2, 0xEE, 0xFC, 0xFF};
        if(strstr(key, "surface") != NULL || strstr(key, "modal") != NULL ||
           strstr(key, "panel") != NULL)
            return (Color){0xD4, 0xE4, 0xF5, 0xFF};
        if(strstr(key, "text") != NULL || strstr(key, "foreground") != NULL ||
           strstr(key, "ink") != NULL)
            return (Color){0x24, 0x48, 0x7C, 0xFF};
        if(strstr(key, "circle") != NULL || strstr(key, "selection") != NULL)
            return (Color){0x7E, 0xB7, 0xE6, 0xFF};
        if(strstr(key, "button_hover") != NULL || strstr(key, "face_hover") != NULL)
            return (Color){0x68, 0x9E, 0xD7, 0xFF};
        if(strstr(key, "button") != NULL || strstr(key, "face") != NULL)
            return (Color){0xA6, 0xCF, 0xF2, 0xFF};
        if(strstr(key, "icon") != NULL)
            return (Color){0xE2, 0xEE, 0xFC, 0xFF};
        if(strstr(key, "link") != NULL)
            return (Color){0x4A, 0x90, 0xE2, 0xFF};
    }

    return (Color){0xE2, 0xEE, 0xFC, 0xFF};
}

bool SetThemeColor(const char *scope_name, const char *key, Color color)
{
    ThemeValue *value = scope_value(GetThemeScope(scope_name), key);
    if(value == NULL)
        return false;
    value->value = color;
    return true;
}

bool SaveThemeScope(const char *scope_name)
{
    ThemeScope *scope = GetThemeScope(scope_name);
    FILE *file;
    char text[16];
    const char *dir;

    if(scope == NULL || scope->path[0] == '\0')
        return false;

    dir = GetDirectoryPath(scope->path);
    if(dir != NULL && dir[0] != '\0' && !DirectoryExists(dir))
        MakeDirectory(dir);

    file = fopen(scope->path, "w");
    if(file == NULL)
        return false;

    fprintf(file, "# Kryon theme: %s\n", scope->name);
    for(int i = 0; i < scope->count; i++)
        fprintf(file, "%s \"%s\"\n", scope->values[i].key,
                GetThemeColorText(scope->values[i].value, text, sizeof(text)));

    fclose(file);
    return true;
}

bool SaveAllThemes(void)
{
    bool ok = true;
    for(int i = 0; i < scope_count; i++) {
        if(scopes[i].path[0] != '\0' && !SaveThemeScope(scopes[i].name))
            ok = false;
    }
    return ok;
}

void DrawThemeTKBorder(Rectangle rec, int borderWidth, bool raised)
{
    Color highlight = GetThemeColor(NULL, "border_light");
    Color shadow = GetThemeColor(NULL, "border_shadow");
    Color topLeft = raised ? highlight : shadow;
    Color bottomRight = raised ? shadow : highlight;

    int x = (int)rec.x;
    int y = (int)rec.y;
    int w = (int)rec.width;
    int h = (int)rec.height;

    DrawRectangle(x, y, w, borderWidth, topLeft);
    DrawRectangle(x, y, borderWidth, h, topLeft);
    DrawRectangle(x, y + h - borderWidth, w, borderWidth, bottomRight);
    DrawRectangle(x + w - borderWidth, y, borderWidth, h, bottomRight);
}

static int find_aggregate_var(const char *key)
{
    for(int i = 0; i < aggregate_count; i++) {
        if(strcmp(aggregate_vars[i].key, key) == 0)
            return i;
    }
    return -1;
}

static ThemeAggregateVariable *add_aggregate_var(const char *key, Color value, int scope_index)
{
    ThemeAggregateVariable *var;

    if(aggregate_count >= THEME_MAX_VARS)
        return NULL;

    var = &aggregate_vars[aggregate_count++];
    memset(var, 0, sizeof(*var));
    copy_text(var->key, THEME_NAME_SIZE, key);
    var->value = value;
    var->scopes[scope_index] = true;
    var->scope_count = 1;
    return var;
}

void AggregateAllThemes(void)
{
    aggregate_count = 0;
    memset(aggregate_vars, 0, sizeof(aggregate_vars));

    for(int s = 0; s < scope_count; s++) {
        ThemeScope *scope = &scopes[s];
        for(int v = 0; v < scope->count; v++) {
            const char *key = scope->values[v].key;
            Color value = scope->values[v].value;
            int idx = find_aggregate_var(key);

            if(idx >= 0) {
                aggregate_vars[idx].scopes[s] = true;
                aggregate_vars[idx].scope_count++;
            } else {
                add_aggregate_var(key, value, s);
            }
        }
    }
}

ThemeAggregateVariable* GetThemeAggregateVars(void)
{
    return aggregate_vars;
}

int GetThemeAggregateCount(void)
{
    return aggregate_count;
}

void ApplyThemeAggregate(const char *key, Color color)
{
    int idx = find_aggregate_var(key);
    if(idx < 0)
        return;

    aggregate_vars[idx].value = color;

    for(int s = 0; s < scope_count; s++) {
        if(aggregate_vars[idx].scopes[s]) {
            SetThemeColor(scopes[s].name, key, color);
        }
    }
}

bool SyncThemeFromScope(const char *src_scope)
{
    ThemeScope *source = GetThemeScope(src_scope);
    if(source == NULL)
        return false;

    for(int s = 0; s < scope_count; s++) {
        if(&scopes[s] == source)
            continue;

        for(int v = 0; v < source->count; v++) {
            const char *key = source->values[v].key;
            Color value = source->values[v].value;
            SetThemeColor(scopes[s].name, key, value);
        }
    }
    return true;
}

bool SyncThemeToApps(const char *src_scope)
{
    return SyncThemeFromScope(src_scope);
}

bool ExportTheme(const char *path)
{
    FILE *file;
    char text[16];

    if(path == NULL || path[0] == '\0')
        return false;

    file = fopen(path, "w");
    if(file == NULL)
        return false;

    fprintf(file, "# Kryon theme export\n");
    fprintf(file, "# Generated by ExportTheme()\n\n");

    for(int s = 0; s < scope_count; s++) {
        fprintf(file, "# Scope: %s\n", scopes[s].name);
        fprintf(file, "[%s]\n", scopes[s].name);

        for(int v = 0; v < scopes[s].count; v++) {
            fprintf(file, "%s \"%s\"\n", scopes[s].values[v].key,
                    GetThemeColorText(scopes[s].values[v].value, text, sizeof(text)));
        }
        fprintf(file, "\n");
    }

    fclose(file);
    return true;
}

bool ImportTheme(const char *path)
{
    FILE *file;
    char line[512];
    char current_scope[THEME_NAME_SIZE] = {0};

    if(path == NULL || path[0] == '\0')
        return false;

    file = fopen(path, "r");
    if(file == NULL)
        return false;

    while(fgets(line, sizeof(line), file) != NULL) {
        char *cursor = line;

        while(isspace((unsigned char)*cursor))
            cursor++;

        if(*cursor == '#' || *cursor == '\0')
            continue;

        if(*cursor == '[') {
            char *end = strchr(cursor, ']');
            if(end != NULL) {
                *end = '\0';
                copy_text(current_scope, THEME_NAME_SIZE, cursor + 1);
            }
            continue;
        }

        char key[THEME_NAME_SIZE];
        char value[32];
        int key_len = 0;
        Color color;

        while(*cursor != '\0' && !isspace((unsigned char)*cursor) &&
              key_len < THEME_NAME_SIZE - 1) {
            key[key_len++] = *cursor++;
        }
        key[key_len] = '\0';

        while(isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == '"')
            cursor++;

        int value_len = 0;
        while(*cursor != '\0' && *cursor != '"' && *cursor != '\n' &&
              !isspace((unsigned char)*cursor) && value_len < (int)sizeof(value) - 1) {
            value[value_len++] = *cursor++;
        }
        value[value_len] = '\0';

        if(ParseThemeColor(value, &color)) {
            if(current_scope[0] != '\0') {
                SetThemeColor(current_scope, key, color);
            }
        }
    }

    fclose(file);
    return true;
}

void SetThemeDarkMode(bool dark)
{
    SetThemeMode(dark ? THEME_MODE_DARK : THEME_MODE_LIGHT);
}

bool GetThemeDarkMode(void)
{
    return GetEffectiveThemeDarkMode();
}

void ReloadThemes(void)
{
    for(int i = 0; i < scope_count; i++) {
        ThemeScope *scope = &scopes[i];
        const char *load_path = dark_mode && scope->dark_path[0] != '\0' ? scope->dark_path : scope->path;

        // Clear current values
        scope->count = 0;

        // Load from appropriate file
        if(load_path != NULL && load_path[0] != '\0') {
            FILE *file = fopen(load_path, "r");
            if(file != NULL) {
                char line[256];
                while(fgets(line, sizeof(line), file) != NULL) {
                    char key[THEME_NAME_SIZE];
                    char value[32];
                    char *cursor = line;
                    int key_len = 0;
                    Color color;

                    while(isspace((unsigned char)*cursor))
                        cursor++;
                    if(*cursor == '#' || *cursor == '\0')
                        continue;

                    while(*cursor != '\0' && !isspace((unsigned char)*cursor) &&
                          key_len < THEME_NAME_SIZE - 1) {
                        key[key_len++] = *cursor++;
                    }
                    key[key_len] = '\0';

                    while(isspace((unsigned char)*cursor))
                        cursor++;
                    if(*cursor == '"')
                        cursor++;

                    int value_len = 0;
                    while(*cursor != '\0' && *cursor != '"' && *cursor != '\n' &&
                          !isspace((unsigned char)*cursor) && value_len < (int)sizeof(value) - 1) {
                        value[value_len++] = *cursor++;
                    }
                    value[value_len] = '\0';

                    if(ParseThemeColor(value, &color))
                        scope_add_value(scope, key, color);
                }
                fclose(file);
            }
        }
    }

    AggregateAllThemes();
}

void
SetThemeSource(ThemeSource source)
{
    if(source != THEME_SOURCE_SYSTEM)
        source = THEME_SOURCE_APP;
    theme_source = source;
}

ThemeSource
GetThemeSource(void)
{
    return theme_source;
}

void
SetThemeMode(ThemeMode mode)
{
    if(mode < THEME_MODE_SYSTEM || mode > THEME_MODE_DARK)
        mode = THEME_MODE_SYSTEM;
    theme_mode = mode;
    dark_mode = ResolveDark((int32_t)mode, SystemThemePrefersDark());
    if(active_theme_family_set) {
        Theme selected = dark_mode ? active_theme_family.dark
                                   : active_theme_family.light;
        snprintf(active_theme_name, sizeof(active_theme_name), "%s",
                 selected.name != NULL ? selected.name : "Theme");
        active_theme = selected;
        active_theme.name = active_theme_name;
        active_theme.mode = dark_mode ? THEME_MODE_DARK : THEME_MODE_LIGHT;
        active_theme_set = 1;
        SetThemeMetrics(active_theme.metrics);
        ApplyCurrentUITheme();
    }
}

ThemeMode
GetThemeMode(void)
{
    return theme_mode;
}

void
SetThemeStyle(ThemeStyle style)
{
    if(style < THEME_STYLE_SYSTEM || style > THEME_STYLE_DEFAULT)
        style = THEME_STYLE_SYSTEM;
    theme_style = style;
    ApplyCurrentUITheme();
}

ThemeStyle
GetThemeStyle(void)
{
    return theme_style;
}

ThemeStyle
GetEffectiveThemeStyle(void)
{
    if(theme_style == THEME_STYLE_SYSTEM)
        return GetDefaultPlatformThemeStyle();
    return theme_style;
}

ThemeStyle
GetDefaultPlatformThemeStyle(void)
{
#if defined(KRYON_PLATFORM_PLAN9)
    return GetSystemThemeStyle();
#elif defined(ANDROID_BUILD) && ANDROID_BUILD
    return THEME_STYLE_DEFAULT;
#elif defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    return THEME_STYLE_DEFAULT;
#else
    return THEME_STYLE_SYSTEM;
#endif
}

ThemeSource
GetDefaultPlatformThemeSource(void)
{
    /* System theme is the default everywhere. Platforms without a readable
       system palette (e.g. Android before the app feeds device dark mode)
       gracefully fall back to the app palette in GetCurrentThemeColor. */
    return THEME_SOURCE_SYSTEM;
}

ThemeMode
GetDefaultPlatformThemeMode(void)
{
    if(GetDefaultPlatformThemeSource() == THEME_SOURCE_SYSTEM)
        return THEME_MODE_SYSTEM;
    return THEME_MODE_LIGHT;
}

int
GetDefaultThemeForThemeStyle(ThemeStyle style)
{
#if defined(KRYON_PLATFORM_PLAN9)
    if(style == THEME_STYLE_SYSTEM)
        return THEME_PLAN9;
#endif
    if(style == THEME_STYLE_SYSTEM)
        style = GetDefaultPlatformThemeStyle();

    switch(style) {
    case THEME_STYLE_CLASSIC:
        return THEME_MONO;
    case THEME_STYLE_DEFAULT:
        return THEME_SWEET;
    case THEME_STYLE_SYSTEM:
    default:
        return THEME_MONO;
    }
}

const char *
GetThemeStyleLabel(ThemeStyle style)
{
    const char *key = NULL;
    const char *fallback = "System";
    const char *text;

    switch(style) {
    case THEME_STYLE_SYSTEM:
        key = "theme_style_system";
        fallback = "System";
        break;
    case THEME_STYLE_CLASSIC:
        key = "theme_style_classic";
        fallback = "Classic";
        break;
    case THEME_STYLE_DEFAULT:
        key = "theme_style_default";
        fallback = "Default";
        break;
    default:
        return "System";
    }
    text = GetLocaleText(key);
    if(text == NULL || text[0] == '\0' || strcmp(text, key) == 0)
        return fallback;
    return text;
}

bool
GetEffectiveThemeDarkMode(void)
{
    if(theme_mode == THEME_MODE_LIGHT)
        return false;
    if(theme_mode == THEME_MODE_DARK)
        return true;
    return SystemThemePrefersDark();
}

void
SetCurrentTheme(int theme_id, int current_dark_mode)
{
    active_theme_family_set = 0;
    active_theme_set = 0;
    current_theme_id = NormalizeTheme(theme_id);
    dark_mode = current_dark_mode != 0;
    ApplyCurrentUITheme();
}

Color
GetCurrentThemeColor(const char *key)
{
    Color color = BLANK;
    if(active_theme_set) {
        ThemeColors *colors = &active_theme.colors;
        if(strcmp(key, "background") == 0) color = colors->background;
        else if(strcmp(key, "surface") == 0) color = colors->surface;
        else if(strcmp(key, "text") == 0) color = colors->text;
        else if(strcmp(key, "circle") == 0) color = colors->accent;
        else if(strcmp(key, "button") == 0) color = colors->accent;
        else if(strcmp(key, "button_hover") == 0) color = colors->accent_hover;
        else if(strcmp(key, "icon") == 0) color = colors->icon;
        else if(strcmp(key, "link") == 0) color = colors->link;
        else if(strcmp(key, "border") == 0) color = colors->border;
        goto apply_alpha;
    }
    if(theme_source == THEME_SOURCE_SYSTEM) {
        SetSystemThemeDarkMode(GetEffectiveThemeDarkMode());
        if(SystemThemeColor(key, &color))
            goto apply_alpha;
    }
    GetThemeCatalogColor(NormalizeTheme(current_theme_id),
                              GetEffectiveThemeDarkMode(), key, &color);
apply_alpha:
    color.a = (unsigned char)((float)color.a * g_theme_content_alpha + 0.5f);
    return color;
}

static Theme
theme_default(int dark)
{
    Theme theme;
    memset(&theme, 0, sizeof(theme));
    theme.name = dark ? "Default dark" : "Default light";
    theme.mode = dark ? THEME_MODE_DARK : THEME_MODE_LIGHT;
    theme.metrics = GetThemeMetricsForThemeStyle(THEME_STYLE_DEFAULT);
    if(dark) {
        theme.colors.background=(Color){0x07,0x14,0x26,0xFF};
        theme.colors.surface=(Color){0x0D,0x21,0x38,0xFF};
        theme.colors.surface_raised=(Color){0x12,0x2B,0x48,0xFF};
        theme.colors.surface_sunken=(Color){0x08,0x1A,0x2E,0xFF};
        theme.colors.text=(Color){0xF5,0xF8,0xFF,0xFF};
        theme.colors.text_muted=(Color){0xA9,0xBB,0xD1,0xFF};
        theme.colors.text_disabled=(Color){0x72,0x83,0x9A,0xFF};
        theme.colors.icon=(Color){0xD8,0xE5,0xF5,0xFF};
        theme.colors.border=(Color){0x31,0x50,0x6F,0xFF};
        theme.colors.focus=(Color){0x4D,0xA3,0xFF,0xFF};
        theme.colors.selection=(Color){0x1E,0x6D,0xE0,0xFF};
        theme.colors.accent=(Color){0x14,0x78,0xFF,0xFF};
        theme.colors.on_accent=RAYWHITE;
        theme.colors.accent_hover=(Color){0x2D,0x8C,0xFF,0xFF};
        theme.colors.accent_pressed=(Color){0x08,0x62,0xD9,0xFF};
        theme.colors.success=(Color){0x07,0x96,0x69,0xFF};
        theme.colors.warning=(Color){0xC8,0x87,0x00,0xFF};
        theme.colors.danger=(Color){0xDC,0x2F,0x4F,0xFF};
        theme.colors.info=(Color){0x16,0x8B,0xD2,0xFF};
        theme.colors.link=(Color){0x59,0xA8,0xFF,0xFF};
        theme.colors.link_hover=(Color){0x8A,0xC2,0xFF,0xFF};
    } else {
        theme.colors.background=(Color){0xF5,0xF8,0xFC,0xFF};
        theme.colors.surface=RAYWHITE;
        theme.colors.surface_raised=RAYWHITE;
        theme.colors.surface_sunken=(Color){0xEA,0xF1,0xF8,0xFF};
        theme.colors.text=(Color){0x10,0x23,0x3A,0xFF};
        theme.colors.text_muted=(Color){0x53,0x6A,0x83,0xFF};
        theme.colors.text_disabled=(Color){0x8C,0x9A,0xA9,0xFF};
        theme.colors.icon=(Color){0x18,0x3C,0x63,0xFF};
        theme.colors.border=(Color){0xC5,0xD4,0xE3,0xFF};
        theme.colors.focus=(Color){0x06,0x6C,0xFF,0xFF};
        theme.colors.selection=(Color){0xD8,0xE9,0xFF,0xFF};
        theme.colors.accent=(Color){0x17,0x69,0xE8,0xFF};
        theme.colors.on_accent=RAYWHITE;
        theme.colors.accent_hover=(Color){0x0F,0x5E,0xD8,0xFF};
        theme.colors.accent_pressed=(Color){0x0A,0x4D,0xB8,0xFF};
        theme.colors.success=(Color){0x07,0x80,0x5A,0xFF};
        theme.colors.warning=(Color){0xB5,0x6D,0x00,0xFF};
        theme.colors.danger=(Color){0xD6,0x24,0x45,0xFF};
        theme.colors.info=(Color){0x08,0x7C,0xBF,0xFF};
        theme.colors.link=(Color){0x07,0x5F,0xD1,0xFF};
        theme.colors.link_hover=(Color){0x03,0x4B,0xA9,0xFF};
    }
    theme.colors.on_success=RAYWHITE;
    theme.colors.on_warning=GetThemeReadableText(theme.colors.warning);
    theme.colors.on_danger=RAYWHITE;
    theme.colors.on_info=RAYWHITE;
    theme.colors.icon_muted=theme.colors.text_muted;
    theme.colors.border_strong=theme.colors.border;
    theme.colors.divider=theme.colors.border;
    theme.colors.overlay=(Color){0,0,0,dark ? 0xD9 : 0x66};
    theme.colors.shadow=(Color){0,0,0,dark ? 0x80 : 0x24};
    return theme;
}

Theme ThemeDefaultLight(void) { return theme_default(0); }
Theme ThemeDefaultDark(void) { return theme_default(1); }

void
SetTheme(Theme theme)
{
    active_theme_family_set = 0;
    snprintf(active_theme_name, sizeof(active_theme_name), "%s",
             theme.name != NULL ? theme.name : "Theme");
    active_theme = theme;
    active_theme.name = active_theme_name;
    active_theme_set = 1;
    theme_mode = theme.mode == THEME_MODE_DARK
        ? THEME_MODE_DARK : THEME_MODE_LIGHT;
    SetThemeMetrics(theme.metrics);
    ApplyCurrentUITheme();
}

void
SetThemeFamily(ThemeFamily family)
{
    snprintf(active_theme_family_name, sizeof(active_theme_family_name), "%s",
             family.name != NULL ? family.name : "Theme family");
    snprintf(active_theme_light_name, sizeof(active_theme_light_name), "%s",
             family.light.name != NULL ? family.light.name : "Light");
    snprintf(active_theme_dark_name, sizeof(active_theme_dark_name), "%s",
             family.dark.name != NULL ? family.dark.name : "Dark");
    active_theme_family = family;
    active_theme_family.name = active_theme_family_name;
    active_theme_family.light.name = active_theme_light_name;
    active_theme_family.dark.name = active_theme_dark_name;
    active_theme_family.light.mode = THEME_MODE_LIGHT;
    active_theme_family.dark.mode = THEME_MODE_DARK;
    active_theme_family_set = 1;
    SetThemeMode(GetEffectiveThemeDarkMode() ? THEME_MODE_DARK
                                             : THEME_MODE_LIGHT);
}

ThemeFamily
GetThemeFamily(void)
{
    ThemeFamily family;
    if(active_theme_family_set)
        return active_theme_family;
    memset(&family, 0, sizeof(family));
    return family;
}

Theme GetTheme(void) { return active_theme_set ? active_theme : theme_default(GetEffectiveThemeDarkMode()); }
const Theme *GetThemeRef(void) { return active_theme_set ? &active_theme : NULL; }

static int
theme_luminance(Color color)
{
    return ((int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114) / 1000;
}

static unsigned char
theme_mix_channel(unsigned char from, unsigned char to, float amount)
{
    float value;

    if(amount < 0.0f)
        amount = 0.0f;
    if(amount > 1.0f)
        amount = 1.0f;
    value = (float)from + ((float)to - (float)from) * amount;
    if(value < 0.0f)
        value = 0.0f;
    if(value > 255.0f)
        value = 255.0f;
    return (unsigned char)(value + 0.5f);
}

Color
MixThemeColor(Color from, Color to, float amount)
{
    return (Color){
        theme_mix_channel(from.r, to.r, amount),
        theme_mix_channel(from.g, to.g, amount),
        theme_mix_channel(from.b, to.b, amount),
        theme_mix_channel(from.a, to.a, amount)
    };
}

int
GetThemeColorLuminance(Color color)
{
    return theme_luminance(color);
}

bool
IsThemeColorDark(Color color)
{
    return theme_luminance(color) < 128;
}

Color
GetThemeReadableText(Color background)
{
    return IsThemeColorDark(background) ? RAYWHITE : BLACK;
}

static Color
theme_readable_semantic_color(const char *key, const char *surface_key)
{
    Color color = GetCurrentThemeColor(key);
    Color surface = GetCurrentThemeColor(surface_key);
    int delta = theme_luminance(color) - theme_luminance(surface);

    if(delta < 0)
        delta = -delta;
    if(delta < 72) {
        Color text = GetCurrentThemeColor("text");
        text.a = color.a != 0 ? color.a : 255;
        return text;
    }
    return color;
}

Color GetThemeText(void) { return GetCurrentThemeColor("text"); }
Color GetThemeBackground(void) { return GetCurrentThemeColor("background"); }
Color GetThemeSurface(void) { return GetCurrentThemeColor("surface"); }
Color GetThemeCircle(void) { return GetCurrentThemeColor("circle"); }
Color GetThemeButton(void) { return GetCurrentThemeColor("button"); }
Color GetThemeButtonHover(void) { return GetCurrentThemeColor("button_hover"); }
Color GetThemeIcon(void) { return theme_readable_semantic_color("icon", "surface"); }
Color GetThemeLink(void) { return GetCurrentThemeColor("link"); }

Color
GetThemeSurfaceAlt(void)
{
    return MixThemeColor(GetThemeSurface(), GetThemeBackground(),
                         IsThemeColorDark(GetThemeBackground()) ? 0.20f : 0.42f);
}

Color
GetThemeBorder(void)
{
    return MixThemeColor(GetThemeText(), GetThemeSurface(),
                         IsThemeColorDark(GetThemeBackground()) ? 0.70f : 0.80f);
}

Color
GetThemeMutedText(void)
{
    return MixThemeColor(GetThemeText(), GetThemeSurface(),
                         IsThemeColorDark(GetThemeBackground()) ? 0.36f : 0.48f);
}

Color
GetThemeSelection(void)
{
    return MixThemeColor(GetThemeSurface(), GetThemeButton(),
                         IsThemeColorDark(GetThemeBackground()) ? 0.26f : 0.16f);
}

Color
GetThemeButtonText(void)
{
    return GetThemeReadableText(GetThemeButton());
}
