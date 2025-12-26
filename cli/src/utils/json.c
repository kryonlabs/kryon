/**
 * JSON Utilities
 * Wrapper around cJSON for parsing and generating JSON
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <cJSON.h>

/**
 * Parse JSON string into cJSON object
 * Returns NULL on parse error
 */
void* json_parse(const char* str) {
    if (!str) return NULL;

    return (void*)cJSON_Parse(str);
}

/**
 * Free JSON object
 */
void json_free(void* json) {
    if (!json) return;

    cJSON_Delete((cJSON*)json);
}

/**
 * Convert JSON object to string
 * Returns newly allocated string
 */
char* json_to_string(void* json) {
    if (!json) return NULL;

    return cJSON_Print((cJSON*)json);
}
