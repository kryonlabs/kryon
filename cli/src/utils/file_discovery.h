#ifndef FILE_DISCOVERY_H
#define FILE_DISCOVERY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char* source_path;      // Relative to project root
    char* output_path;      // Relative to build dir
    char* route;            // URL route
    bool is_template;       // Ignore in build
} DiscoveredFile;

typedef struct {
    DiscoveredFile* files;
    uint32_t count;
    uint32_t capacity;
    char* docs_template;    // NULL if none found
} DiscoveryResult;

// Main discovery function
DiscoveryResult* discover_project_files(const char* project_root);

// Free discovery result
void free_discovery_result(DiscoveryResult* result);

// Helper: Convert source path to route
// "index.html" → "/"
// "docs/getting-started.md" → "/docs/getting-started"
char* path_to_route(const char* source_path);

// Helper: Convert route to output path
// "/" → "build/index.html"
// "/docs/getting-started" → "build/docs/getting-started/index.html"
char* route_to_output_path(const char* route, const char* build_dir);

// Helper: Check if file is special (template, partial, etc.)
bool is_special_file(const char* filename);

#endif // FILE_DISCOVERY_H
