/**
 * @file kir_utils.c
 * @brief KIR Utility Functions
 */

#include "kir_format.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

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
