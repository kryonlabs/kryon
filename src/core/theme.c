#include "theme.h"
#include "embedded_assets.h"
#include "theme_meta.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static ThemeScope scopes[THEME_MAX_SCOPES];
static int scope_count = 0;
static bool dark_mode = false;
static int current_theme_id = THEME_SKY;
static ThemeSource theme_source = THEME_SOURCE_SYSTEM;
static ThemeMode theme_mode = THEME_MODE_SYSTEM;

static ThemeAggregateVariable aggregate_vars[THEME_MAX_VARS];
static int aggregate_count = 0;

bool SystemThemeColor(const char *key, Color *color);

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

    value = scope_value(GetThemeScope("flint"), key);
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

    fprintf(file, "# Flint theme: %s\n", scope->name);
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

    fprintf(file, "# Flint theme export\n");
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
    dark_mode = dark;
    theme_mode = dark ? THEME_MODE_DARK : THEME_MODE_LIGHT;
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
    if(mode == THEME_MODE_LIGHT)
        dark_mode = false;
    else if(mode == THEME_MODE_DARK)
        dark_mode = true;
}

ThemeMode
GetThemeMode(void)
{
    return theme_mode;
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
    current_theme_id = NormalizeTheme(theme_id);
    dark_mode = current_dark_mode != 0;
}

Color
GetCurrentThemeColor(const char *key)
{
    Color color = BLANK;
    if(theme_source == THEME_SOURCE_SYSTEM &&
       SystemThemeColor(key, &color))
        return color;
    GetThemeCatalogColor(NormalizeTheme(current_theme_id),
                              GetEffectiveThemeDarkMode(), key, &color);
    return color;
}

Color GetThemeText(void) { return GetCurrentThemeColor("text"); }
Color GetThemeBackground(void) { return GetCurrentThemeColor("background"); }
Color GetThemeSurface(void) { return GetCurrentThemeColor("surface"); }
Color GetThemeCircle(void) { return GetCurrentThemeColor("circle"); }
Color GetThemeButton(void) { return GetCurrentThemeColor("button"); }
Color GetThemeButtonHover(void) { return GetCurrentThemeColor("button_hover"); }
Color GetThemeIcon(void) { return GetCurrentThemeColor("icon"); }
Color GetThemeLink(void) { return GetCurrentThemeColor("link"); }
