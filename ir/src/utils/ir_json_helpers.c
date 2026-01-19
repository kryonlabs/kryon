/**
 * @file ir_json_helpers.c
 * @brief Safe cJSON wrapper implementation
 */

#define _GNU_SOURCE
#include "../utils/ir_json_helpers.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void cJSON_AddStringOrNull(cJSON* obj, const char* key, const char* value) {
    if (!obj || !key) return;

    if (value) {
        cJSON_AddStringToObject(obj, key, value);
    } else {
        cJSON_AddNullToObject(obj, key);
    }
}

void cJSON_AddStringFormatted(cJSON* obj, const char* key, const char* fmt, ...) {
    if (!obj || !key || !fmt) return;

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    cJSON_AddStringToObject(obj, key, buffer);
}

cJSON* cJSON_CreateStringOrNull(const char* value) {
    return value ? cJSON_CreateString(value) : cJSON_CreateNull();
}

const char* cJSON_GetStringSafe(cJSON* obj, const char* key) {
    if (!obj || !key) return NULL;

    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsString(item)) return NULL;

    return item->valuestring;
}

double cJSON_GetNumberSafe(cJSON* obj, const char* key, double default_value) {
    if (!obj || !key) return default_value;

    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return default_value;

    return item->valuedouble;
}

bool cJSON_GetBoolSafe(cJSON* obj, const char* key, bool default_value) {
    if (!obj || !key) return default_value;

    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return default_value;

    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }

    return default_value;
}
