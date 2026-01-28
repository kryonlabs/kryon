/**
 * @file kir_utils.c
 * @brief KIR Utility Functions
 */

#include "kir_format.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <libgen.h>

bool kryon_is_kir_file(const char *file_path) {
    if (!file_path) return false;

    size_t len = strlen(file_path);
    if (len < 4) return false;

    return strcmp(file_path + len - 4, ".kir") == 0;
}

char *kryon_kir_get_output_path(const char *source_path) {
    if (!source_path) return NULL;

    size_t len = strlen(source_path);
    char *kir_path = malloc(len + 5); // +5 for ".kir\0"
    if (!kir_path) return NULL;

    strcpy(kir_path, source_path);

    // Replace .kry extension with .kir
    char *ext = strrchr(kir_path, '.');
    if (ext && strcmp(ext, ".kry") == 0) {
        strcpy(ext, ".kir");
    } else {
        strcat(kir_path, ".kir");
    }

    return kir_path;
}

char *kryon_cache_get_output_path(const char *source_path,
                                   const char *extension,
                                   bool create_dir) {
    if (!source_path || !extension) {
        return NULL;
    }

    // Create a copy of source_path for dirname (dirname may modify the string)
    char *path_copy = strdup(source_path);
    if (!path_copy) {
        return NULL;
    }

    // Extract directory from source_path
    char *dir = dirname(path_copy);

    // Extract filename without extension
    const char *filename_start = strrchr(source_path, '/');
    if (!filename_start) {
        filename_start = source_path;
    } else {
        filename_start++; // Skip the '/'
    }

    // Find the extension in the filename
    char *name_copy = strdup(filename_start);
    if (!name_copy) {
        free(path_copy);
        return NULL;
    }

    char *ext = strrchr(name_copy, '.');
    if (ext) {
        *ext = '\0'; // Remove extension
    }

    // Construct cache directory path: {source_dir}/.kryon_cache
    size_t cache_dir_len = strlen(dir) + strlen("/.kryon_cache") + 1;
    char *cache_dir = malloc(cache_dir_len);
    if (!cache_dir) {
        free(path_copy);
        free(name_copy);
        return NULL;
    }
    snprintf(cache_dir, cache_dir_len, "%s/.kryon_cache", dir);

    // Create directory if requested
    if (create_dir) {
        struct stat st = {0};
        if (stat(cache_dir, &st) == -1) {
            if (mkdir(cache_dir, 0755) != 0) {
                fprintf(stderr, "Warning: Failed to create .kryon_cache directory: %s\n", cache_dir);
                free(path_copy);
                free(name_copy);
                free(cache_dir);
                return NULL;
            }
        }
    }

    // Construct final path: {source_dir}/.kryon_cache/{filename}{extension}
    size_t output_len = strlen(cache_dir) + strlen("/") + strlen(name_copy) + strlen(extension) + 1;
    char *output_path = malloc(output_len);
    if (!output_path) {
        free(path_copy);
        free(name_copy);
        free(cache_dir);
        return NULL;
    }
    snprintf(output_path, output_len, "%s/%s%s", cache_dir, name_copy, extension);

    free(path_copy);
    free(name_copy);
    free(cache_dir);

    return output_path;
}

int kryon_kir_compare_files(const char *file1_path, const char *file2_path,
                            bool ignore_metadata, bool ignore_locations) {
    // TODO: Implement KIR comparison
    (void)file1_path;
    (void)file2_path;
    (void)ignore_metadata;
    (void)ignore_locations;
    return -1; // Not implemented
}

size_t kryon_kir_validate_file(const char *input_path, char **errors, size_t max_errors) {
    // TODO: Implement KIR validation
    (void)input_path;
    (void)errors;
    (void)max_errors;
    return 0;
}

size_t kryon_kir_validate_string(const char *json_string, char **errors, size_t max_errors) {
    // TODO: Implement KIR validation
    (void)json_string;
    (void)errors;
    (void)max_errors;
    return 0;
}

bool kryon_kir_check_version_compat(const char *input_path, int min_major, int min_minor) {
    // TODO: Implement version checking
    (void)input_path;
    (void)min_major;
    (void)min_minor;
    return true; // Assume compatible for now
}
