#include "locale.h"
#include "platform.h"
#include "embedded_assets.h"

#include "kryon.h"

#if ANDROID_BUILD
#include <android_native_app_glue.h>
#include <jni.h>
extern struct android_app *GetAndroidApp(void);
#endif

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LocaleEntry *g_base_entries = NULL;
static size_t g_base_count = 0;
static size_t g_base_cap = 0;

static LocaleEntry *g_active_entries = NULL;
static size_t g_active_count = 0;
static size_t g_active_cap = 0;

static LocaleLanguage *g_languages = NULL;
static size_t g_language_count = 0;
static size_t g_language_cap = 0;

static int g_loaded = 0;
static char g_current_code[32] = "en";

static char *
dup_range(const char *src, size_t len)
{
    char *out = malloc(len + 1);
    if(out == NULL)
        return NULL;
    memcpy(out, src, len);
    out[len] = '\0';
    return out;
}

static char *
dup_cstr(const char *src)
{
    return dup_range(src, strlen(src));
}

static void
free_locale_entries(LocaleEntry **entries, size_t *count, size_t *cap)
{
    if(entries == NULL || *entries == NULL)
        return;

    for(size_t i = 0; i < *count; i++) {
        free((*entries)[i].key);
        free((*entries)[i].value);
    }
    free(*entries);
    *entries = NULL;
    *count = 0;
    *cap = 0;
}

static void
free_languages(void)
{
    if(g_languages == NULL)
        return;

    for(size_t i = 0; i < g_language_count; i++) {
        free(g_languages[i].code);
        free(g_languages[i].label);
    }
    free(g_languages);
    g_languages = NULL;
    g_language_count = 0;
    g_language_cap = 0;
}

static void
ensure_locale_capacity(LocaleEntry **entries, size_t *cap, size_t count)
{
    LocaleEntry *next;
    size_t next_cap;

    if(count < *cap)
        return;

    next_cap = (*cap == 0) ? 32 : *cap * 2;
    next = realloc(*entries, next_cap * sizeof(*next));
    if(next == NULL)
        return;

    *entries = next;
    *cap = next_cap;
}

static void
ensure_language_capacity(void)
{
    LocaleLanguage *next;
    size_t next_cap;

    if(g_language_count < g_language_cap)
        return;

    next_cap = (g_language_cap == 0) ? 8 : g_language_cap * 2;
    next = realloc(g_languages, next_cap * sizeof(*next));
    if(next == NULL)
        return;

    g_languages = next;
    g_language_cap = next_cap;
}

static void
set_locale_entry(LocaleEntry **entries, size_t *count, size_t *cap, const char *key, const char *value)
{
    size_t i;

    if(entries == NULL || count == NULL || cap == NULL || key == NULL || value == NULL)
        return;

    for(i = 0; i < *count; i++) {
        if(strcmp((*entries)[i].key, key) == 0) {
            char *new_value = dup_cstr(value);
            if(new_value == NULL)
                return;
            free((*entries)[i].value);
            (*entries)[i].value = new_value;
            return;
        }
    }

    ensure_locale_capacity(entries, cap, *count);
    if(*count >= *cap)
        return;

    (*entries)[*count].key = dup_cstr(key);
    (*entries)[*count].value = dup_cstr(value);
    if((*entries)[*count].key == NULL || (*entries)[*count].value == NULL) {
        free((*entries)[*count].key);
        free((*entries)[*count].value);
        (*entries)[*count].key = NULL;
        (*entries)[*count].value = NULL;
        return;
    }
    (*count)++;
}

static const char *
find_locale_entry(LocaleEntry *entries, size_t count, const char *key)
{
    if(key == NULL)
        return "";

    for(size_t i = 0; i < count; i++) {
        if(strcmp(entries[i].key, key) == 0)
            return entries[i].value;
    }
    return NULL;
}

static void
append_text(char **buf, size_t *len, size_t *cap, const char *text)
{
    size_t text_len;
    char *next;
    size_t needed;

    if(text == NULL)
        return;

    text_len = strlen(text);
    needed = *len + text_len + 1;
    if(*cap < needed) {
        size_t next_cap = (*cap == 0) ? 128 : *cap;
        while(next_cap < needed)
            next_cap *= 2;
        next = realloc(*buf, next_cap);
        if(next == NULL)
            return;
        *buf = next;
        *cap = next_cap;
    }

    memcpy(*buf + *len, text, text_len);
    *len += text_len;
    (*buf)[*len] = '\0';
}

static void
append_char(char **buf, size_t *len, size_t *cap, char c)
{
    char tmp[2] = { c, '\0' };
    append_text(buf, len, cap, tmp);
}

static void
load_locale_text_into(LocaleEntry **entries, size_t *count, size_t *cap, const char *text)
{
    char *mutable_text;
    char *cursor;
    char *current_key = NULL;
    char *body = NULL;
    size_t body_len = 0;
    size_t body_cap = 0;

    if(text == NULL || entries == NULL || count == NULL || cap == NULL)
        return;

    mutable_text = dup_cstr(text);
    if(mutable_text == NULL)
        return;

    cursor = mutable_text;
    while(cursor != NULL && *cursor != '\0') {
        char *line = cursor;
        char *eol = strpbrk(cursor, "\r\n");
        if(eol != NULL) {
            char line_end = *eol;
            *eol = '\0';
            cursor = eol + 1;
            if((line_end == '\r' && *cursor == '\n') || (line_end == '\n' && *cursor == '\r'))
                cursor++;
        } else {
            cursor = NULL;
        }

        if(line[0] == '[') {
            size_t line_len = strlen(line);
            if(line_len > 2 && line[line_len - 1] == ']') {
                char *key = dup_range(line + 1, line_len - 2);
                if(key != NULL) {
                    free(current_key);
                    current_key = key;
                    free(body);
                    body = NULL;
                    body_len = 0;
                    body_cap = 0;
                }
                continue;
            }
        }

        if(strcmp(line, "---") == 0) {
            if(current_key != NULL) {
                set_locale_entry(entries, count, cap, current_key, body != NULL ? body : "");
                free(current_key);
                current_key = NULL;
            }
            free(body);
            body = NULL;
            body_len = 0;
            body_cap = 0;
            continue;
        }

        if(current_key == NULL)
            continue;

        if(body_len > 0)
            append_char(&body, &body_len, &body_cap, '\n');
        append_text(&body, &body_len, &body_cap, line);
    }

    if(current_key != NULL)
        set_locale_entry(entries, count, cap, current_key, body != NULL ? body : "");

    free(current_key);
    free(body);
    free(mutable_text);
}

static void
load_language_list_from_text(const char *text)
{
    char *mutable_text;
    char *cursor;

    if(text == NULL)
        return;

    mutable_text = dup_cstr(text);
    if(mutable_text == NULL)
        return;

    cursor = mutable_text;
    while(cursor != NULL && *cursor != '\0') {
        char *line = cursor;
        char *eol = strpbrk(cursor, "\r\n");
        if(eol != NULL) {
            char line_end = *eol;
            *eol = '\0';
            cursor = eol + 1;
            if((line_end == '\r' && *cursor == '\n') || (line_end == '\n' && *cursor == '\r'))
                cursor++;
        } else {
            cursor = NULL;
        }

        if(line[0] == '\0' || line[0] == '#')
            continue;

        char *sep = strchr(line, '|');
        if(sep == NULL)
            continue;

        *sep = '\0';
        const char *code = line;
        const char *label = sep + 1;
        if(code[0] == '\0' || label[0] == '\0')
            continue;

        ensure_language_capacity();
        if(g_language_count >= g_language_cap)
            continue;

        g_languages[g_language_count].code = dup_cstr(code);
        g_languages[g_language_count].label = dup_cstr(label);
        if(g_languages[g_language_count].code == NULL || g_languages[g_language_count].label == NULL) {
            free(g_languages[g_language_count].code);
            free(g_languages[g_language_count].label);
            g_languages[g_language_count].code = NULL;
            g_languages[g_language_count].label = NULL;
            continue;
        }
        g_language_count++;
    }

    free(mutable_text);
}

static int
load_file_text_from_paths(const char *relative_path, char **out_text)
{
    char *text;

    if(out_text == NULL)
        return 0;
    *out_text = NULL;

#if defined(UI_EMBEDDED_ONLY) && UI_EMBEDDED_ONLY
    text = LoadEmbeddedAssetText(relative_path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }
    return 0;
#elif ANDROID_BUILD
    text = LoadFileText(relative_path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }
    text = LoadEmbeddedAssetText(relative_path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }
    return 0;
#else
    static const char *prefixes[] = {
        "",
        "../",
        "../../",
        "../../../",
        "../../../../",
        NULL
    };

    char path[256];

    for(int i = 0; prefixes[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], relative_path);
        if(FileExists(path)) {
            text = LoadFileText(path);
            if(text != NULL) {
                *out_text = text;
                return 1;
            }
        }
    }

    text = LoadEmbeddedAssetText(relative_path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }
    return 0;
#endif
}

static int
load_locale_file_for_code(const char *code, char **out_text)
{
    char path[64];
    char *text;

    if(out_text == NULL)
        return 0;
    *out_text = NULL;

    if(code == NULL || code[0] == 0)
        code = "en";

#if defined(UI_EMBEDDED_ONLY) && UI_EMBEDDED_ONLY
    snprintf(path, sizeof(path), "locales/%s.txt", code);
    text = LoadEmbeddedAssetText(path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }
    return 0;
#elif ANDROID_BUILD
    snprintf(path, sizeof(path), "locales/%s.txt", code);
    text = LoadFileText(path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }
    text = LoadEmbeddedAssetText(path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }
    return 0;
#else
    static const char *prefixes[] = {
        "",
        "../",
        "../../",
        "../../../",
        "../../../../",
        NULL
    };

    for(int i = 0; prefixes[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%slocales/%s.txt", prefixes[i], code);
        if(FileExists(path)) {
            text = LoadFileText(path);
            if(text != NULL) {
                *out_text = text;
                return 1;
            }
        }
    }

    snprintf(path, sizeof(path), "locales/%s.txt", code);
    text = LoadEmbeddedAssetText(path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }
    return 0;
#endif
}

static void
load_defaults(void)
{
    int has_english = 0;

    for(size_t i = 0; i < g_language_count; i++) {
        if(g_languages[i].code != NULL && strcmp(g_languages[i].code, "en") == 0) {
            has_english = 1;
            break;
        }
    }

    if(has_english)
        return;

    ensure_language_capacity();
    if(g_language_count < g_language_cap) {
        g_languages[g_language_count].code = dup_cstr("en");
        g_languages[g_language_count].label = dup_cstr("English");
        if(g_languages[g_language_count].code != NULL && g_languages[g_language_count].label != NULL)
            g_language_count++;
        else {
            free(g_languages[g_language_count].code);
            free(g_languages[g_language_count].label);
            g_languages[g_language_count].code = NULL;
            g_languages[g_language_count].label = NULL;
        }
    }
}

static void
set_current_code(const char *code)
{
    if(code == NULL || code[0] == '\0')
        code = "en";
    snprintf(g_current_code, sizeof(g_current_code), "%s", code);
}

static void
load_base_locale(void)
{
    char *text = NULL;
    static const struct {
        const char *key;
        const char *value;
    } defaults[] = {
        {"theme_style_label", "Style"},
        {"theme_style_system", "System"},
        {"theme_style_retro", "Retro"},
        {"theme_style_material", "Material"},
        {"theme_style_fluent", "Fluent"},
        {"theme_style_adwaita", "Adwaita"},
        {"theme_style_liquid_glass", "Liquid Glass"},
        {"theme_label", "Theme source"},
        {"theme_app", "Kryon"},
        {"theme_system", "System"},
        {"theme_mode_label", "Mode"},
        {"theme_follow_device", "Follow device"},
        {"theme_light", "Light"},
        {"theme_dark", "Dark"},
        {"theme_color_label", "Color theme"},
        {"theme_picker_title", "Color theme"},
        {"theme_sky", "Sky"},
        {"theme_ocean", "Ocean"},
        {"theme_forest", "Forest"},
        {"theme_sunset", "Sunset"},
        {"theme_lavender", "Lavender"},
        {"theme_cherry", "Cherry"},
        {"theme_dawn", "Dawn"},
        {"theme_sage", "Sage"},
        {"theme_sepia", "Sepia"},
        {"theme_mono", "Mono"},
        {"theme_mint", "Mint"},
        {"theme_cobalt", "Cobalt"},
        {"theme_plan9", "Plan9"},
    };

    for(size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++)
        set_locale_entry(&g_base_entries, &g_base_count, &g_base_cap,
                         defaults[i].key, defaults[i].value);

    if(load_file_text_from_paths("locales/en.txt", &text)) {
        load_locale_text_into(&g_base_entries, &g_base_count, &g_base_cap, text);
        UnloadFileText(text);
    }
}

static void
load_registry(void)
{
    char *text = NULL;
    if(load_file_text_from_paths("locales/index.txt", &text)) {
        load_language_list_from_text(text);
        UnloadFileText(text);
    }
    load_defaults();
}

static void
clear_active_locale(void)
{
    free_locale_entries(&g_active_entries, &g_active_count, &g_active_cap);
}

void
InitLocale(void)
{
    if(g_loaded)
        return;

    free_locale_entries(&g_base_entries, &g_base_count, &g_base_cap);
    free_locale_entries(&g_active_entries, &g_active_count, &g_active_cap);
    free_languages();
    load_registry();
    load_base_locale();
    clear_active_locale();
    set_current_code("en");
    g_loaded = 1;
}

int
SetLocale(const char *code)
{
    char *text = NULL;
    int found = 0;

    if(!g_loaded)
        InitLocale();

    if(code == NULL || code[0] == '\0')
        code = "en";

    if(strcmp(code, "en") == 0) {
        clear_active_locale();
        set_current_code("en");
        return 1;
    }

    for(size_t i = 0; i < g_language_count; i++) {
        if(strcmp(g_languages[i].code, code) == 0) {
            found = 1;
            break;
        }
    }
    if(!found)
        return 0;

    clear_active_locale();
    if(load_locale_file_for_code(code, &text)) {
        load_locale_text_into(&g_active_entries, &g_active_count, &g_active_cap, text);
        UnloadFileText(text);
        set_current_code(code);
        return 1;
    }

    clear_active_locale();
    set_current_code("en");
    return 0;
}

static const char *
lookup_locale_value(const char *key)
{
    const char *value;

    value = find_locale_entry(g_active_entries, g_active_count, key);
    if(value != NULL)
        return value;

    value = find_locale_entry(g_base_entries, g_base_count, key);
    if(value != NULL)
        return value;

    return key != NULL ? key : "";
}

static int
locale_is_pref_separator(char c)
{
    return c == ':' || c == ';' || c == ',' || c == ' ' || c == '\t' ||
           c == '\r' || c == '\n';
}

static char
locale_ascii_lower(char c)
{
    if(c >= 'A' && c <= 'Z')
        return (char)(c + ('a' - 'A'));
    return c;
}

static int
normalize_locale_candidate(const char *src, size_t src_len,
                           char *dst, size_t dst_size)
{
    size_t out = 0;
    int saw_alnum = 0;
    int last_sep = 0;

    if(src == NULL || dst == NULL || dst_size == 0)
        return 0;

    for(size_t i = 0; i < src_len && src[i] != '\0'; i++) {
        char c = src[i];

        if(c == '.' || c == '@' || c == '%')
            break;
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9')) {
            if(out + 1 >= dst_size)
                break;
            dst[out++] = locale_ascii_lower(c);
            saw_alnum = 1;
            last_sep = 0;
            continue;
        }
        if(c == '-' || c == '_') {
            if(out == 0 || last_sep)
                continue;
            if(out + 1 >= dst_size)
                break;
            dst[out++] = '-';
            last_sep = 1;
        }
    }

    while(out > 0 && dst[out - 1] == '-')
        out--;
    dst[out] = '\0';

    if(!saw_alnum || strcmp(dst, "c") == 0 || strcmp(dst, "posix") == 0)
        return 0;
    return out > 0;
}

static int
locale_code_equal_normalized(const char *code, const char *normalized)
{
    char norm_code[32];

    if(code == NULL || normalized == NULL)
        return 0;
    if(!normalize_locale_candidate(code, strlen(code), norm_code, sizeof(norm_code)))
        return 0;
    return strcmp(norm_code, normalized) == 0;
}

static const char *
find_supported_locale_code(const char *candidate)
{
    char normalized[32];
    char base[32];
    char *dash;

    if(candidate == NULL)
        return NULL;
    if(!normalize_locale_candidate(candidate, strlen(candidate),
                                   normalized, sizeof(normalized)))
        return NULL;

    for(size_t i = 0; i < g_language_count; i++) {
        if(locale_code_equal_normalized(g_languages[i].code, normalized))
            return g_languages[i].code;
    }

    snprintf(base, sizeof(base), "%s", normalized);
    dash = strchr(base, '-');
    if(dash != NULL)
        *dash = '\0';
    if(base[0] == '\0')
        return NULL;

    for(size_t i = 0; i < g_language_count; i++) {
        if(locale_code_equal_normalized(g_languages[i].code, base))
            return g_languages[i].code;
    }

    return NULL;
}

static const char *
find_supported_locale_from_preferences(const char *preferences)
{
    const char *cursor;

    if(preferences == NULL || preferences[0] == '\0')
        return NULL;

    cursor = preferences;
    while(*cursor != '\0') {
        const char *start;
        size_t len;
        char candidate[64];
        const char *code;

        while(locale_is_pref_separator(*cursor))
            cursor++;
        start = cursor;
        while(*cursor != '\0' && !locale_is_pref_separator(*cursor))
            cursor++;
        len = (size_t)(cursor - start);
        if(len == 0)
            continue;
        if(len >= sizeof(candidate))
            len = sizeof(candidate) - 1;
        memcpy(candidate, start, len);
        candidate[len] = '\0';

        code = find_supported_locale_code(candidate);
        if(code != NULL)
            return code;
    }

    return NULL;
}

static const char *
append_locale_preference(char *dst, size_t dst_size, const char *value)
{
    size_t len;
    size_t value_len;

    if(dst == NULL || dst_size == 0 || value == NULL || value[0] == '\0')
        return dst;

    len = strlen(dst);
    if(len + 1 >= dst_size)
        return dst;
    if(len > 0)
        dst[len++] = ',';

    value_len = strlen(value);
    if(value_len >= dst_size - len)
        value_len = dst_size - len - 1;
    memcpy(dst + len, value, value_len);
    dst[len + value_len] = '\0';
    return dst;
}

static void
get_platform_locale_preferences(char *dst, size_t dst_size)
{
    const char *env;

    if(dst == NULL || dst_size == 0)
        return;
    dst[0] = '\0';

    env = getenv("KRYON_TEST_LOCALE");
    if(env != NULL && env[0] != '\0') {
        append_locale_preference(dst, dst_size, env);
        return;
    }

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    EM_ASM({
        var values = [];
        if(typeof navigator !== "undefined") {
            if(navigator.languages && navigator.languages.length)
                values = Array.prototype.slice.call(navigator.languages);
            else if(navigator.language)
                values = [navigator.language];
        }
        stringToUTF8(values.join(","), $0, $1);
    }, dst, (int)dst_size);
    if(dst[0] != '\0')
        return;
#endif

#if ANDROID_BUILD
    {
        struct android_app *app = GetAndroidApp();
        JNIEnv *jni = NULL;
        int attached = 0;
        jobject activity = NULL;
        jobject resources = NULL;
        jobject config = NULL;
        jobject locale = NULL;
        jstring tag = NULL;
        const char *chars = NULL;

        if(app != NULL && app->activity != NULL && app->activity->vm != NULL &&
           app->activity->clazz != NULL) {
            JavaVM *vm = app->activity->vm;
            if((*vm)->GetEnv(vm, (void **)&jni, JNI_VERSION_1_6) != JNI_OK) {
                if((*vm)->AttachCurrentThread(vm, &jni, NULL) == JNI_OK)
                    attached = 1;
            }
            if(jni != NULL) {
                activity = app->activity->clazz;
                jclass activity_cls = (*jni)->GetObjectClass(jni, activity);
                jmethodID get_resources = activity_cls != NULL
                    ? (*jni)->GetMethodID(jni, activity_cls, "getResources",
                                          "()Landroid/content/res/Resources;")
                    : NULL;
                if(get_resources != NULL)
                    resources = (*jni)->CallObjectMethod(jni, activity, get_resources);
                if(resources != NULL) {
                    jclass resources_cls = (*jni)->GetObjectClass(jni, resources);
                    jmethodID get_config = resources_cls != NULL
                        ? (*jni)->GetMethodID(jni, resources_cls,
                                              "getConfiguration",
                                              "()Landroid/content/res/Configuration;")
                        : NULL;
                    if(get_config != NULL)
                        config = (*jni)->CallObjectMethod(jni, resources, get_config);
                    if(config != NULL) {
                        jclass config_cls = (*jni)->GetObjectClass(jni, config);
                        jmethodID get_locales = config_cls != NULL
                            ? (*jni)->GetMethodID(jni, config_cls, "getLocales",
                                                  "()Landroid/os/LocaleList;")
                            : NULL;
                        if(get_locales != NULL) {
                            jobject locales = (*jni)->CallObjectMethod(jni, config,
                                                                       get_locales);
                            if(locales != NULL) {
                                jclass locales_cls = (*jni)->GetObjectClass(jni, locales);
                                jmethodID get = locales_cls != NULL
                                    ? (*jni)->GetMethodID(jni, locales_cls, "get",
                                                          "(I)Ljava/util/Locale;")
                                    : NULL;
                                if(get != NULL)
                                    locale = (*jni)->CallObjectMethod(jni, locales, get, 0);
                                if(locales_cls != NULL)
                                    (*jni)->DeleteLocalRef(jni, locales_cls);
                                (*jni)->DeleteLocalRef(jni, locales);
                            }
                            if((*jni)->ExceptionCheck(jni))
                                (*jni)->ExceptionClear(jni);
                        }
                        if(locale == NULL && config_cls != NULL) {
                            jfieldID locale_field = (*jni)->GetFieldID(
                                jni, config_cls, "locale", "Ljava/util/Locale;");
                            if(locale_field != NULL)
                                locale = (*jni)->GetObjectField(jni, config, locale_field);
                            if((*jni)->ExceptionCheck(jni))
                                (*jni)->ExceptionClear(jni);
                        }
                        if(config_cls != NULL)
                            (*jni)->DeleteLocalRef(jni, config_cls);
                    }
                    if(resources_cls != NULL)
                        (*jni)->DeleteLocalRef(jni, resources_cls);
                }
                if(activity_cls != NULL)
                    (*jni)->DeleteLocalRef(jni, activity_cls);

                if(locale == NULL) {
                    jclass locale_cls = (*jni)->FindClass(jni, "java/util/Locale");
                    jmethodID get_default = locale_cls != NULL
                        ? (*jni)->GetStaticMethodID(jni, locale_cls, "getDefault",
                                                    "()Ljava/util/Locale;")
                        : NULL;
                    if(get_default != NULL)
                        locale = (*jni)->CallStaticObjectMethod(jni, locale_cls,
                                                                get_default);
                    if(locale_cls != NULL)
                        (*jni)->DeleteLocalRef(jni, locale_cls);
                }

                if(locale != NULL) {
                    jclass locale_cls = (*jni)->GetObjectClass(jni, locale);
                    jmethodID to_language_tag = locale_cls != NULL
                        ? (*jni)->GetMethodID(jni, locale_cls, "toLanguageTag",
                                              "()Ljava/lang/String;")
                        : NULL;
                    if(to_language_tag != NULL)
                        tag = (jstring)(*jni)->CallObjectMethod(jni, locale,
                                                                to_language_tag);
                    if(tag != NULL) {
                        chars = (*jni)->GetStringUTFChars(jni, tag, NULL);
                        if(chars != NULL) {
                            append_locale_preference(dst, dst_size, chars);
                            (*jni)->ReleaseStringUTFChars(jni, tag, chars);
                        }
                        (*jni)->DeleteLocalRef(jni, tag);
                    }
                    if(locale_cls != NULL)
                        (*jni)->DeleteLocalRef(jni, locale_cls);
                    (*jni)->DeleteLocalRef(jni, locale);
                }

                if(config != NULL)
                    (*jni)->DeleteLocalRef(jni, config);
                if(resources != NULL)
                    (*jni)->DeleteLocalRef(jni, resources);
                if(attached)
                    (*vm)->DetachCurrentThread(vm);
            }
        }
        if(dst[0] != '\0')
            return;
    }
#endif

#if defined(_WIN32)
    {
        WCHAR name[LOCALE_NAME_MAX_LENGTH];
        if(GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0) {
            char out[LOCALE_NAME_MAX_LENGTH];
            size_t i;
            for(i = 0; i + 1 < sizeof(out) && name[i] != 0; i++)
                out[i] = name[i] < 128 ? (char)name[i] : '\0';
            out[i] = '\0';
            append_locale_preference(dst, dst_size, out);
            if(dst[0] != '\0')
                return;
        }
    }
#endif

    env = getenv("LANGUAGE");
    if(env != NULL && env[0] != '\0')
        append_locale_preference(dst, dst_size, env);
    env = getenv("LC_ALL");
    if(env != NULL && env[0] != '\0')
        append_locale_preference(dst, dst_size, env);
    env = getenv("LC_MESSAGES");
    if(env != NULL && env[0] != '\0')
        append_locale_preference(dst, dst_size, env);
    env = getenv("LANG");
    if(env != NULL && env[0] != '\0')
        append_locale_preference(dst, dst_size, env);
}

/* Best supported locale for this system or browser. Regional tags and
 * codesets ("pt-BR", "pt_BR.UTF-8") map onto the base catalog ("pt") when
 * one exists; unsupported preferences fall through to English. */
const char *
GetSystemLocaleCode(void)
{
    char preferences[512];
    const char *code;

    if(!g_loaded)
        InitLocale();

    get_platform_locale_preferences(preferences, sizeof(preferences));
    code = find_supported_locale_from_preferences(preferences);
    if(code != NULL)
        return code;
    return "en";
}

const char *
GetDefaultLocaleCode(void)
{
    return GetSystemLocaleCode();
}

const char *
GetLocaleText(const char *key)
{
    if(!g_loaded)
        InitLocale();
    return lookup_locale_value(key);
}

void
FormatLocaleText(char *dst, size_t dst_size, const char *key, ...)
{
    const char *fmt;
    va_list args;

    if(dst == NULL || dst_size == 0)
        return;

    fmt = GetLocaleText(key);
    va_start(args, key);
    vsnprintf(dst, dst_size, fmt, args);
    va_end(args);
}

int
GetLocaleCount(void)
{
    if(!g_loaded)
        InitLocale();
    return (int)g_language_count;
}

const char *
GetLocaleCode(int index)
{
    if(!g_loaded)
        InitLocale();
    if(index < 0 || (size_t)index >= g_language_count)
        return "";
    return g_languages[index].code != NULL ? g_languages[index].code : "";
}

const char *
GetLocaleLabel(int index)
{
    if(!g_loaded)
        InitLocale();
    if(index < 0 || (size_t)index >= g_language_count)
        return "";
    return g_languages[index].label != NULL ? g_languages[index].label : "";
}

int
GetLocaleIndex(const char *code)
{
    if(!g_loaded)
        InitLocale();
    if(code == NULL)
        return -1;
    for(size_t i = 0; i < g_language_count; i++) {
        if(g_languages[i].code != NULL && strcmp(g_languages[i].code, code) == 0)
            return (int)i;
    }
    return -1;
}

const char *
GetCurrentLocaleCode(void)
{
    if(!g_loaded)
        InitLocale();
    return g_current_code;
}

int
GetCurrentLocaleIndex(void)
{
    return GetLocaleIndex(GetCurrentLocaleCode());
}
