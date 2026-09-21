#include "locale.h"
#include "platform.h"
#include "embedded_assets.h"
#include "ui/locale_defaults.h"
#include "ui/locale_parser.h"
#include "ui/locale_policy.h"

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
dup_cstr(const char *src)
{
    size_t len = strlen(src);
    char *out = malloc(len + 1);
    if(out == NULL)
        return NULL;
    memcpy(out, src, len + 1);
    return out;
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

void
LocaleStoreEntry(LocaleEntry **entries, size_t *count, size_t *cap, const char *key, const char *value)
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

void
LocaleStoreLanguage(const char *code, const char *label)
{
    if(code == NULL || label == NULL)
        return;
    ensure_language_capacity();
    if(g_language_count >= g_language_cap)
        return;

    g_languages[g_language_count].code = dup_cstr(code);
    g_languages[g_language_count].label = dup_cstr(label);
    if(g_languages[g_language_count].code == NULL ||
       g_languages[g_language_count].label == NULL) {
        free(g_languages[g_language_count].code);
        free(g_languages[g_language_count].label);
        g_languages[g_language_count].code = NULL;
        g_languages[g_language_count].label = NULL;
        return;
    }
    g_language_count++;
}

static int
load_file_text_from_paths(const char *relative_path, char **out_text)
{
    char *text;

    if(out_text == NULL)
        return 0;
    *out_text = NULL;

#if defined(KRYON_EMBEDDED_ONLY) && KRYON_EMBEDDED_ONLY
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

#if defined(KRYON_EMBEDDED_ONLY) && KRYON_EMBEDDED_ONLY
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
set_current_code(const char *code)
{
    snprintf(g_current_code, sizeof(g_current_code), "%s", code);
}

static void
load_base_locale(void)
{
    char *text = NULL;
    for(int i = 0; i < LocaleDefaultCount(); i++)
        LocaleStoreEntry(&g_base_entries, &g_base_count, &g_base_cap,
                         LocaleDefaultKeyAt(i), LocaleDefaultValueAt(i));

    if(load_file_text_from_paths("locales/en.txt", &text)) {
        LocaleParseEntries(text, &g_base_entries, &g_base_count, &g_base_cap);
        UnloadFileText(text);
    }
}

static void
load_registry(void)
{
    char *text = NULL;
    if(load_file_text_from_paths("locales/index.txt", &text)) {
        LocaleParseLanguages(text);
        UnloadFileText(text);
    }
    LocaleEnsureEnglish(g_languages, g_language_count);
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

    if(!g_loaded)
        InitLocale();

    code = LocaleRequestedCode(code);

    if(strcmp(code, "en") == 0) {
        clear_active_locale();
        set_current_code("en");
        return 1;
    }

    if(!LocaleContainsCode(g_languages, g_language_count, code))
        return 0;

    clear_active_locale();
    if(load_locale_file_for_code(code, &text)) {
        LocaleParseEntries(text, &g_active_entries, &g_active_count, &g_active_cap);
        UnloadFileText(text);
        set_current_code(code);
        return 1;
    }

    clear_active_locale();
    set_current_code("en");
    return 0;
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
    if(!g_loaded)
        InitLocale();

    get_platform_locale_preferences(preferences, sizeof(preferences));
    return LocalePreferredCode(g_languages, g_language_count, preferences);
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
    return LocaleResolveValue(g_active_entries, g_active_count,
                              g_base_entries, g_base_count, key);
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
