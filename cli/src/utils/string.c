/**
 * String Utilities
 * Provides common string manipulation functions for the CLI
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * Format a string into a dynamically allocated buffer
 * Caller must free the result
 *
 * This replaces fixed-size buffers with dynamic allocation
 * to avoid magic numbers and buffer overflows
 */
char* str_format(const char* fmt, ...) {
    if (!fmt) return NULL;

    va_list args;
    va_start(args, fmt);

    // First pass: get size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return NULL;
    }

    // Allocate and format
    char* result = malloc(size + 1);
    if (!result) {
        va_end(args);
        return NULL;
    }

    vsprintf(result, fmt, args);
    va_end(args);

    return result;
}

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
 * Duplicate a string (alias for str_copy)
 * This provides a more semantic name for string duplication operations
 */
char* str_duplicate(const char* s) {
    return str_copy(s);
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

/**
 * Replace all occurrences of a substring in a string
 * Returns a newly allocated string (caller must free)
 */
char* string_replace(const char* str, const char* old, const char* new_str) {
    if (!str || !old || !new_str) return NULL;

    // Count occurrences to calculate result size
    int count = 0;
    const char* p = str;
    while ((p = strstr(p, old)) != NULL) {
        count++;
        p += strlen(old);
    }

    if (count == 0) {
        return str_copy(str);
    }

    // Calculate result size
    size_t str_len = strlen(str);
    size_t old_len = strlen(old);
    size_t new_len = strlen(new_str);
    size_t result_len = str_len + count * (new_len - old_len);

    // Allocate result buffer
    char* result = (char*)malloc(result_len + 1);
    if (!result) return NULL;

    // Build result string
    const char* src = str;
    char* dst = result;
    while (*src) {
        if (strncmp(src, old, old_len) == 0) {
            strncpy(dst, new_str, new_len);
            dst += new_len;
            src += old_len;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    return result;
}
