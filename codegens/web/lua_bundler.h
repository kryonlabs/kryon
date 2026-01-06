#ifndef LUA_BUNDLER_H
#define LUA_BUNDLER_H

#include <stdbool.h>

// Forward declaration for IR component (avoid including ir_core.h in header)
struct IRComponent;

// Maximum number of Lua modules that can be bundled
#define LUA_BUNDLER_MAX_MODULES 256

// Represents a single Lua module
typedef struct {
    char* module_name;    // e.g., "kryon.reactive"
    char* file_path;      // e.g., "/path/to/kryon/reactive.lua"
    char* source;         // Lua source code
    bool is_bundled;      // Already processed
} LuaBundleModule;

// Lua bundler context
typedef struct {
    LuaBundleModule modules[LUA_BUNDLER_MAX_MODULES];
    int module_count;
    char* search_paths[16];      // Lua package.path equivalent
    int search_path_count;
    char* kryon_bindings_path;   // Path to kryon/bindings/lua
    bool use_web_modules;        // Use _web variants when available
    char* project_name;          // App namespace (e.g., "habits") for auto-exporting handlers
    char local_functions[64][256]; // Local function names from main.lua
    int local_function_count;    // Number of local functions
    // Note: Uses component ID dispatch with handler map from KIR
    // The bundler injects __KRYON_HANDLERS__[component_id] = handler_index
} LuaBundler;

// Create a new Lua bundler
LuaBundler* lua_bundler_create(void);

// Destroy the bundler and free memory
void lua_bundler_destroy(LuaBundler* bundler);

// Add a search path for Lua modules
void lua_bundler_add_search_path(LuaBundler* bundler, const char* path);

// Set the Kryon bindings path (for kryon.* modules)
void lua_bundler_set_kryon_path(LuaBundler* bundler, const char* path);

// Enable web module substitution (use runtime_web.lua instead of runtime.lua, etc.)
void lua_bundler_use_web_modules(LuaBundler* bundler, bool enable);

// Set the project name for automatic handler namespace export
// Creates _G.<project_name> table and auto-promotes local functions to it
void lua_bundler_set_project_name(LuaBundler* bundler, const char* name);

// Note: lua_bundler_set_event_ids removed - direct index mapping is used instead

// Bundle a Lua file and all its dependencies
// Returns the bundled Lua source as a single string, or NULL on error
// Caller is responsible for freeing the returned string
char* lua_bundler_bundle(LuaBundler* bundler, const char* main_file);

// Generate package.preload entries for embedded HTML
// Returns HTML-safe script content with handler map injected from KIR
// kir_root: IR component tree (used to build component_id -> handler_index mapping)
char* lua_bundler_generate_script(LuaBundler* bundler, const char* main_file, struct IRComponent* kir_root);

#endif // LUA_BUNDLER_H
