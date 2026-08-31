#include "kry_automation.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

static void
copy_option(char *dst, int dst_size, const char *src)
{
    size_t len;

    if(dst == NULL || dst_size <= 0)
        return;
    if(src == NULL)
        src = "";
    len = strlen(src);
    if(len >= (size_t)dst_size)
        len = (size_t)dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void
make_env_key(char *dst, size_t dst_size, const char *key)
{
    const char prefix[] = "KRYON_AUTOMATION_";
    size_t i = 0;

    if(dst == NULL || dst_size == 0)
        return;
    while(prefix[i] != '\0' && i + 1 < dst_size) {
        dst[i] = prefix[i];
        i++;
    }
    if(key == NULL)
        key = "";
    while(*key != '\0' && i + 1 < dst_size) {
        unsigned char ch = (unsigned char)*key++;

        if(isalnum(ch))
            dst[i++] = (char)toupper(ch);
        else
            dst[i++] = '_';
    }
    dst[i] = '\0';
}

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
EM_JS(int, js_kry_automation_query_option,
      (const char *key_ptr, char *out, int out_size), {
    if (!key_ptr || !out || out_size <= 0) return 0;
    var key = UTF8ToString(key_ptr);
    var candidates = [key, 'kryon_' + key, 'kryon-' + key];
    var sources = [];

    if (typeof location !== 'undefined') {
        if (location.search) sources.push(location.search.substring(1));
        if (location.hash) {
            var hash = location.hash.substring(1);
            var query = hash.indexOf('?');
            sources.push(query >= 0 ? hash.substring(query + 1) : hash);
        }
    }

    for (var s = 0; s < sources.length; s++) {
        var params = new URLSearchParams(sources[s]);
        for (var i = 0; i < candidates.length; i++) {
            if (!params.has(candidates[i])) continue;
            var value = params.get(candidates[i]);
            stringToUTF8(value === null ? '1' : value, out, out_size);
            return 1;
        }
    }
    return 0;
});
#endif

int
KryAutomationGetOption(const char *key, const char *fallback,
                       char *out, int out_size)
{
    char env_key[128];
    const char *value;

    copy_option(out, out_size, fallback);
    if(key == NULL || key[0] == '\0' || out == NULL || out_size <= 0)
        return 0;

    make_env_key(env_key, sizeof(env_key), key);
    value = getenv(env_key);
    if(value != NULL && value[0] != '\0') {
        copy_option(out, out_size, value);
        return 1;
    }

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    if(js_kry_automation_query_option(key, out, out_size))
        return 1;
#endif

    return 0;
}

int
KryAutomationGetInt(const char *key, int fallback, int *out)
{
    char text[64];
    char *end;
    long value;

    if(out == NULL)
        return 0;
    *out = fallback;
    if(!KryAutomationGetOption(key, "", text, sizeof(text)) || text[0] == '\0')
        return 0;
    value = strtol(text, &end, 10);
    if(end == text || *end != '\0')
        return 0;
    *out = (int)value;
    return 1;
}

unsigned int
KryAutomationGetSeed(unsigned int fallback)
{
    int value;

    if(!KryAutomationGetInt("seed", (int)fallback, &value))
        return fallback;
    return value < 0 ? fallback : (unsigned int)value;
}
