/**
 * @file ir_json_helpers.h
 * @brief Safe wrapper functions for cJSON operations
 *
 * These wrappers handle NULL values safely and prevent common mistakes
 * when working with cJSON for JSON serialization.
 */

#ifndef IR_JSON_HELPERS_H
#define IR_JSON_HELPERS_H

#include <stdbool.h>
#include "third_party/cJSON/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add a string to an object, skipping if value is NULL
 * @param obj cJSON object (must not be NULL)
 * @param key Key string (must not be NULL)
 * @param value Value string (can be NULL)
 */
void cJSON_AddStringOrNull(cJSON* obj, const char* key, const char* value);

/**
 * Add a formatted string to an object
 * @param obj cJSON object (must not be NULL)
 * @param key Key string (must not be NULL)
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
void cJSON_AddStringFormatted(cJSON* obj, const char* key, const char* fmt, ...);

/**
 * Create a cJSON string or null based on value
 * @param value String value (can be NULL)
 * @return cJSON string node or null node
 */
cJSON* cJSON_CreateStringOrNull(const char* value);

/**
 * Safely get a string item from an object
 * Returns NULL if the item doesn't exist or is not a string
 * @param obj cJSON object
 * @param key Key to look up
 * @return String value or NULL
 */
const char* cJSON_GetStringSafe(cJSON* obj, const char* key);

/**
 * Safely get a number item from an object
 * Returns default_value if the item doesn't exist or is not a number
 * @param obj cJSON object
 * @param key Key to look up
 * @param default_value Default value to return
 * @return Number value or default
 */
double cJSON_GetNumberSafe(cJSON* obj, const char* key, double default_value);

/**
 * Safely get a boolean item from an object
 * Returns default_value if the item doesn't exist or is not a boolean
 * @param obj cJSON object
 * @param key Key to look up
 * @param default_value Default value to return
 * @return Boolean value or default
 */
bool cJSON_GetBoolSafe(cJSON* obj, const char* key, bool default_value);

#ifdef __cplusplus
}
#endif

#endif // IR_JSON_HELPERS_H
