/**
 * Limbo Module Discovery Helper
 *
 * Scans TaijiOS module directory for available .m interface files
 * and extracts module information for code generation.
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/* ============================================================================
 * Module Discovery
 * ============================================================================ */

/**
 * Extract module name from .m file content
 * Reads the .m file and finds the module declaration (e.g., "String: module {...}")
 *
 * @param m_file_path Path to the .m file
 * @return Allocated string with module name (caller must free), or NULL on error
 */
static char* extract_module_name_from_m(const char* m_file_path) {
    if (!m_file_path) return NULL;

    FILE* f = fopen(m_file_path, "r");
    if (!f) return NULL;

    char line[512];
    char* module_name = NULL;

    while (fgets(line, sizeof(line), f)) {
        // Trim leading whitespace
        char* start = line;
        while (*start == ' ' || *start == '\t') start++;

        // Look for module declaration pattern: "Name: module {"
        // or "Name: module" followed by anything
        char* colon = strchr(start, ':');
        if (!colon) continue;

        // Check if "module" keyword follows the colon
        char* after_colon = colon + 1;
        while (*after_colon == ' ' || *after_colon == '\t') after_colon++;

        if (strncmp(after_colon, "module", 6) == 0) {
            // Extract module name (from start to colon)
            size_t name_len = colon - start;
            module_name = (char*)malloc(name_len + 1);
            if (module_name) {
                memcpy(module_name, start, name_len);
                module_name[name_len] = '\0';

                // Trim trailing whitespace from module name
                while (name_len > 0 && isspace(module_name[name_len - 1])) {
                    module_name[--name_len] = '\0';
                }
            }
            break;
        }
    }

    fclose(f);
    return module_name;
}

/**
 * Generate standard alias for a module name
 * Converts "String" -> "string", "StringBuilder" -> "stringbuilder"
 *
 * @param module_name The capitalized module name (e.g., "String")
 * @return Allocated string with lowercase alias (caller must free), or NULL on error
 */
static char* generate_module_alias(const char* module_name) {
    if (!module_name) return NULL;

    char* alias = strdup(module_name);
    if (!alias) return NULL;

    // Convert to lowercase
    for (char* p = alias; *p; p++) {
        *p = tolower(*p);
    }

    return alias;
}

/**
 * Scan $TAIJI_PATH/module/*.m for available modules
 * Returns allocated array of module names (caller must free with str_free_array)
 *
 * @param count Output parameter for number of modules found
 * @return Array of lowercase module names (e.g., "string", "sh", "bufio")
 */
char** limbo_modules_scan(int* count) {
    if (!count) return NULL;

    *count = 0;

    // Get TaijiOS path from environment or use default
    const char* taiji_path = getenv("TAIJI_PATH");
    if (!taiji_path) {
        taiji_path = "/home/wao/Projects/TaijiOS";
    }

    // Build module directory path
    char module_dir[1024];
    snprintf(module_dir, sizeof(module_dir), "%s/module", taiji_path);

    // Check if directory exists
    struct stat st;
    if (stat(module_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "[limbo] Warning: Module directory not found: %s\n", module_dir);
        return NULL;
    }

    // Count .m files
    int file_count = dir_list_files(module_dir, ".m", NULL, count);
    if (file_count <= 0) {
        return NULL;
    }

    // Get file list
    char** files = NULL;
    int actual_count = 0;
    dir_list_files(module_dir, ".m", &files, &actual_count);

    if (!files || actual_count == 0) {
        return NULL;
    }

    // Allocate result array
    char** modules = (char**)calloc(actual_count, sizeof(char*));
    if (!modules) {
        str_free_array(files, actual_count);
        return NULL;
    }

    int module_count = 0;

    // Process each .m file
    for (int i = 0; i < actual_count; i++) {
        if (!files[i]) continue;

        // Build full path to .m file
        char* m_file_path = path_join(module_dir, files[i]);
        if (!m_file_path) continue;

        // Extract module name from file content
        char* module_name = extract_module_name_from_m(m_file_path);
        free(m_file_path);

        if (!module_name) {
            // Fallback: extract from filename (e.g., "string.m" -> "String")
            const char* basename = files[i];
            char* name_copy = strdup(basename);
            if (name_copy) {
                char* dot = strrchr(name_copy, '.');
                if (dot) *dot = '\0';

                // Capitalize first letter
                if (name_copy[0]) {
                    name_copy[0] = toupper(name_copy[0]);
                }
                module_name = name_copy;
            }
        }

        if (module_name) {
            // Generate lowercase alias
            modules[module_count] = generate_module_alias(module_name);
            if (modules[module_count]) {
                module_count++;
            }
            free(module_name);
        }
    }

    // Cleanup file list
    str_free_array(files, actual_count);

    *count = module_count;
    return modules;
}

/**
 * Get module PATH constant value
 * Reads the .m file and finds the PATH: con "..." declaration
 *
 * @param module_alias Lowercase module name (e.g., "string")
 * @return Allocated string with PATH value (caller must free), or NULL if not found
 */
char* limbo_module_get_path(const char* module_alias) {
    if (!module_alias) return NULL;

    // Get TaijiOS path from environment or use default
    const char* taiji_path = getenv("TAIJI_PATH");
    if (!taiji_path) {
        taiji_path = "/home/wao/Projects/TaijiOS";
    }

    // Build module .m file path (convert lowercase alias to filename)
    char m_filename[256];
    snprintf(m_filename, sizeof(m_filename), "%s.m", module_alias);

    char module_dir[1024];
    snprintf(module_dir, sizeof(module_dir), "%s/module", taiji_path);

    char* m_file_path = path_join(module_dir, m_filename);
    if (!m_file_path) return NULL;

    // Read file and find PATH: con "..." declaration
    FILE* f = fopen(m_file_path, "r");
    free(m_file_path);

    if (!f) return NULL;

    char line[512];
    char* path_value = NULL;

    while (fgets(line, sizeof(line), f)) {
        // Trim leading whitespace
        char* start = line;
        while (*start == ' ' || *start == '\t') start++;

        // Look for PATH: con "..." pattern
        if (strncmp(start, "PATH:", 5) == 0) {
            char* con = strstr(start, "con");
            if (!con) continue;

            char* quote1 = strchr(con, '"');
            if (!quote1) continue;

            char* quote2 = strchr(quote1 + 1, '"');
            if (!quote2) continue;

            // Extract path value
            size_t path_len = quote2 - quote1 - 1;
            path_value = (char*)malloc(path_len + 1);
            if (path_value) {
                memcpy(path_value, quote1 + 1, path_len);
                path_value[path_len] = '\0';
            }
            break;
        }
    }

    fclose(f);
    return path_value;
}
