/**
 * String Utilities
 * Provides common string manipulation functions for the CLI
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/**
 * Concatenate two strings, returning a newly allocated string
 */
char* str_concat(const char* s1, const char* s2) {
    if (!s1 || !s2) return NULL;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    char* result = (char*)malloc(len1 + len2 + 1);
    if (!result) return NULL;

    strcpy(result, s1);
    strcat(result, s2);

    return result;
}

/**
 * Copy a string, returning a newly allocated copy
 */
char* str_copy(const char* src) {
    if (!src) return NULL;

    size_t len = strlen(src);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;

    strcpy(result, src);
    return result;
}

/**
 * Check if string ends with suffix
 */
bool str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) return false;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/**
 * Check if string starts with prefix
 */
bool str_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;

    size_t prefix_len = strlen(prefix);
    return strncmp(str, prefix, prefix_len) == 0;
}

/**
 * Split string by delimiter, returning array of strings
 * count is set to the number of resulting strings
 */
char** str_split(const char* str, char delim, int* count) {
    if (!str || !count) return NULL;

    // Count delimiters to determine array size
    int num_delims = 0;
    for (const char* p = str; *p; p++) {
        if (*p == delim) num_delims++;
    }

    int num_parts = num_delims + 1;
    char** result = (char**)malloc(num_parts * sizeof(char*));
    if (!result) {
        *count = 0;
        return NULL;
    }

    // Split the string
    int idx = 0;
    const char* start = str;
    const char* end = str;

    while (*end) {
        if (*end == delim) {
            size_t len = end - start;
            result[idx] = (char*)malloc(len + 1);
            if (!result[idx]) {
                // Clean up previously allocated memory on failure
                for (int i = 0; i < idx; i++) {
                    free(result[i]);
                }
                free(result);
                *count = 0;
                return NULL;
            }
            strncpy(result[idx], start, len);
            result[idx][len] = '\0';
            idx++;
            start = end + 1;
        }
        end++;
    }

    // Add final part
    size_t len = end - start;
    result[idx] = (char*)malloc(len + 1);
    if (!result[idx]) {
        // Clean up previously allocated memory on failure
        for (int i = 0; i < idx; i++) {
            free(result[i]);
        }
        free(result);
        *count = 0;
        return NULL;
    }
    strncpy(result[idx], start, len);
    result[idx][len] = '\0';
    idx++;

    *count = idx;
    return result;
}

/**
 * Free array of strings
 */
void str_free_array(char** arr, int count) {
    if (!arr) return;

    for (int i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}
