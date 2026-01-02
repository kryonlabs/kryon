#include "file_discovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

// File extensions we can build
static const char* buildable_extensions[] = {
    ".html", ".md", ".tsx", ".jsx", ".kry", ".c", ".nim", ".lua", NULL
};

// Directories to skip during discovery
static const char* skip_dirs[] = {
    ".", "..", ".kryon_cache", "build", "dist", "node_modules", ".git", NULL
};

// Check if filename has a buildable extension
static bool is_buildable_file(const char* filename) {
    for (int i = 0; buildable_extensions[i] != NULL; i++) {
        const char* ext = buildable_extensions[i];
        size_t ext_len = strlen(ext);
        size_t name_len = strlen(filename);

        if (name_len >= ext_len &&
            strcmp(filename + name_len - ext_len, ext) == 0) {
            return true;
        }
    }
    return false;
}

// Check if directory should be skipped
static bool should_skip_dir(const char* dirname) {
    for (int i = 0; skip_dirs[i] != NULL; i++) {
        if (strcmp(dirname, skip_dirs[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool is_special_file(const char* filename) {
    // Files starting with underscore are templates/partials
    const char* basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;
    return basename[0] == '_';
}

char* path_to_route(const char* source_path) {
    char* route = malloc(PATH_MAX);
    if (!route) return NULL;

    // Remove extension
    const char* dot = strrchr(source_path, '.');
    size_t base_len = dot ? (size_t)(dot - source_path) : strlen(source_path);

    // Special case: index.html → "/"
    if (strncmp(source_path, "index.", 6) == 0) {
        strcpy(route, "/");
        return route;
    }

    // General case: docs/getting-started.md → "/docs/getting-started"
    route[0] = '/';
    strncpy(route + 1, source_path, base_len);
    route[base_len + 1] = '\0';

    return route;
}

char* route_to_output_path(const char* route, const char* build_dir) {
    char* output = malloc(PATH_MAX);
    if (!output) return NULL;

    // "/" → "build/index.html"
    if (strcmp(route, "/") == 0) {
        snprintf(output, PATH_MAX, "%s/index.html", build_dir);
        return output;
    }

    // "/docs/getting-started" → "build/docs/getting-started/index.html"
    snprintf(output, PATH_MAX, "%s%s/index.html", build_dir, route);
    return output;
}

// Add file to discovery result
static void add_discovered_file(DiscoveryResult* result, const char* source_path,
                                 bool is_template) {
    // Expand capacity if needed
    if (result->count >= result->capacity) {
        result->capacity = result->capacity == 0 ? 16 : result->capacity * 2;
        result->files = realloc(result->files,
                                result->capacity * sizeof(DiscoveredFile));
        if (!result->files) {
            fprintf(stderr, "Failed to allocate memory for discovered files\n");
            return;
        }
    }

    DiscoveredFile* file = &result->files[result->count];
    file->source_path = strdup(source_path);
    file->is_template = is_template;

    if (!is_template) {
        file->route = path_to_route(source_path);
        file->output_path = route_to_output_path(file->route, "build");
    } else {
        file->route = NULL;
        file->output_path = NULL;
    }

    result->count++;
}

// Recursively scan directory
static void scan_directory(const char* base_path, const char* rel_path,
                            DiscoveryResult* result) {
    char current_path[PATH_MAX];

    if (rel_path && rel_path[0] != '\0') {
        snprintf(current_path, PATH_MAX, "%s/%s", base_path, rel_path);
    } else {
        snprintf(current_path, PATH_MAX, "%s", base_path);
    }

    DIR* dir = opendir(current_path);
    if (!dir) {
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip directories in skip list
        if (should_skip_dir(entry->d_name)) {
            continue;
        }

        // Build full path
        char entry_path[PATH_MAX];
        snprintf(entry_path, PATH_MAX, "%s/%s", current_path, entry->d_name);

        // Build relative path
        char entry_rel_path[PATH_MAX];
        if (rel_path && rel_path[0] != '\0') {
            snprintf(entry_rel_path, PATH_MAX, "%s/%s", rel_path, entry->d_name);
        } else {
            snprintf(entry_rel_path, PATH_MAX, "%s", entry->d_name);
        }

        struct stat st;
        if (stat(entry_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively scan subdirectory
            scan_directory(base_path, entry_rel_path, result);
        } else if (S_ISREG(st.st_mode)) {
            // Check if it's a buildable file
            if (!is_buildable_file(entry->d_name)) {
                continue;
            }

            bool is_template = is_special_file(entry_rel_path);

            // Special case: docs/_layout.html
            if (strcmp(entry_rel_path, "docs/_layout.html") == 0) {
                result->docs_template = strdup(entry_rel_path);
                continue;  // Don't add to files list
            }

            // Add file (templates are added but marked as such)
            if (!is_template) {
                add_discovered_file(result, entry_rel_path, false);
            }
        }
    }

    closedir(dir);
}

DiscoveryResult* discover_project_files(const char* project_root) {
    DiscoveryResult* result = calloc(1, sizeof(DiscoveryResult));
    if (!result) {
        fprintf(stderr, "Failed to allocate memory for discovery result\n");
        return NULL;
    }

    result->files = NULL;
    result->count = 0;
    result->capacity = 0;
    result->docs_template = NULL;

    // Scan project directory
    scan_directory(project_root, "", result);

    return result;
}

void free_discovery_result(DiscoveryResult* result) {
    if (!result) return;

    for (uint32_t i = 0; i < result->count; i++) {
        free(result->files[i].source_path);
        free(result->files[i].route);
        free(result->files[i].output_path);
    }

    free(result->files);
    free(result->docs_template);
    free(result);
}
