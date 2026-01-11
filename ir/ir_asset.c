/*
 * Kryon Asset Management System Implementation
 *
 * Centralized asset loading, caching, and hot-reloading
 * Now with per-instance registry support
 *
 * Note: Sprite-specific functionality has been moved to the canvas plugin.
 * Audio-specific functionality has been moved to the audio plugin.
 */

#define _POSIX_C_SOURCE 199309L

#include "ir_asset.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

// ============================================================================
// Global State (Legacy - for backward compatibility)
// ============================================================================

// The global asset system (instance 0)
// This is wrapped as an IRAssetRegistry for multi-instance support
static struct {
    bool initialized;

    // Asset registry
    IRAsset assets[IR_MAX_ASSETS];
    uint32_t asset_count;

    // Search paths (using IRSearchPath from header)
    struct {
        char alias[64];
        char real_path[IR_MAX_PATH_LENGTH];
    } search_paths[IR_MAX_SEARCH_PATHS];
    uint32_t search_path_count;

    // Hot reload
    IRAssetHotReloadCallback hot_reload_callback;
    void* hot_reload_user_data;
    uint32_t hot_reload_check_count;

    // Custom asset loaders
    struct {
        IRAssetLoadCallback load;
        IRAssetUnloadCallback unload;
        IRAssetReloadCallback reload;
        bool registered;
    } custom_loaders[IR_ASSET_CUSTOM + 1];  // One slot per asset type

    // Statistics
    uint64_t total_load_time_ms;
    uint32_t total_loads;
} g_asset_system = {0};

// Pointer to global registry as IRAssetRegistry (for API compatibility)
static IRAssetRegistry g_global_registry_wrapper = {
    .instance_id = 0,
    .assets = {0},  // Points to g_asset_system.assets
    .asset_count = 0,
    .search_paths = {0},
    .search_path_count = 0,
    .hot_reload_callback = NULL,
    .hot_reload_user_data = NULL,
    .custom_loaders = {0}
};

// Thread-local current registry (for instance-specific asset operations)
static __thread IRAssetRegistry* t_current_registry = NULL;

// Per-instance registry tracking (for lookup by instance_id)
#define IR_MAX_REGISTRIES 32
static IRAssetRegistry* g_registries[IR_MAX_REGISTRIES] = {0};
static uint32_t g_registry_count = 0;

// ============================================================================
// Helper Functions
// ============================================================================

// Get file modification time
static uint64_t get_file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (uint64_t)st.st_mtime;
    }
    return 0;
}

// Get current time in milliseconds
static uint64_t get_time_ms(void) {
#ifndef _WIN32
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#else
    return (uint64_t)GetTickCount64();
#endif
}

// Find asset by path in a specific registry
static IRAsset* find_asset_in_registry(IRAssetRegistry* registry, const char* path) {
    if (!registry) return NULL;
    for (uint32_t i = 0; i < registry->asset_count; i++) {
        if (strcmp(registry->assets[i].path, path) == 0) {
            return &registry->assets[i];
        }
    }
    return NULL;
}

// Find asset by path (uses current registry or global)
static IRAsset* find_asset(const char* path) {
    IRAssetRegistry* registry = ir_asset_get_current_registry();
    if (!registry) return NULL;
    return find_asset_in_registry(registry, path);
}

// Get the global registry (instance 0)
static IRAssetRegistry* get_global_registry(void) {
    // Sync the wrapper with the actual global state
    g_global_registry_wrapper.instance_id = 0;
    g_global_registry_wrapper.asset_count = g_asset_system.asset_count;
    g_global_registry_wrapper.search_path_count = g_asset_system.search_path_count;
    g_global_registry_wrapper.hot_reload_callback = g_asset_system.hot_reload_callback;
    g_global_registry_wrapper.hot_reload_user_data = g_asset_system.hot_reload_user_data;

    // Copy assets array
    memcpy(g_global_registry_wrapper.assets, g_asset_system.assets, sizeof(g_asset_system.assets));

    // Copy search paths (field-by-field due to struct difference)
    for (uint32_t i = 0; i < g_asset_system.search_path_count && i < IR_MAX_SEARCH_PATHS; i++) {
        strncpy(g_global_registry_wrapper.search_paths[i].alias,
               g_asset_system.search_paths[i].alias, sizeof(g_global_registry_wrapper.search_paths[i].alias) - 1);
        strncpy(g_global_registry_wrapper.search_paths[i].real_path,
               g_asset_system.search_paths[i].real_path, sizeof(g_global_registry_wrapper.search_paths[i].real_path) - 1);
    }

    // Copy custom loaders
    memcpy(g_global_registry_wrapper.custom_loaders, g_asset_system.custom_loaders, sizeof(g_asset_system.custom_loaders));

    return &g_global_registry_wrapper;
}

// ============================================================================
// Per-Instance Registry API
// ============================================================================

IRAssetRegistry* ir_asset_registry_create(uint32_t instance_id) {
    // Check if registry already exists for this instance
    for (uint32_t i = 0; i < g_registry_count; i++) {
        if (g_registries[i] && g_registries[i]->instance_id == instance_id) {
            return g_registries[i];
        }
    }

    // Check capacity
    if (g_registry_count >= IR_MAX_REGISTRIES) {
        fprintf(stderr, "[Asset] Maximum registries reached (%d)\n", IR_MAX_REGISTRIES);
        return NULL;
    }

    // Allocate new registry
    IRAssetRegistry* registry = calloc(1, sizeof(IRAssetRegistry));
    if (!registry) {
        fprintf(stderr, "[Asset] Failed to allocate registry\n");
        return NULL;
    }

    registry->instance_id = instance_id;

    // Register the registry
    g_registries[g_registry_count++] = registry;

    return registry;
}

void ir_asset_registry_destroy(IRAssetRegistry* registry) {
    if (!registry) return;

    // Unload all assets
    for (uint32_t i = 0; i < registry->asset_count; i++) {
        if (registry->assets[i].data && registry->assets[i].ref_count > 0) {
            // Call custom unload if registered
            if (registry->custom_loaders[registry->assets[i].type].unload) {
                registry->custom_loaders[registry->assets[i].type].unload(registry->assets[i].data);
            }
            free(registry->assets[i].data);
        }
    }

    // Remove from registry list
    for (uint32_t i = 0; i < g_registry_count; i++) {
        if (g_registries[i] == registry) {
            // Shift remaining entries down
            for (uint32_t j = i; j < g_registry_count - 1; j++) {
                g_registries[j] = g_registries[j + 1];
            }
            g_registries[g_registry_count - 1] = NULL;
            g_registry_count--;
            break;
        }
    }

    free(registry);
}

IRAssetRegistry* ir_asset_get_current_registry(void) {
    if (t_current_registry) {
        return t_current_registry;
    }
    // Fall back to global registry if not initialized yet
    if (!g_asset_system.initialized) return NULL;
    return get_global_registry();
}

IRAssetRegistry* ir_asset_set_current_registry(IRAssetRegistry* registry) {
    IRAssetRegistry* prev = t_current_registry;
    t_current_registry = registry;
    return prev;
}

IRAssetRegistry* ir_asset_get_registry_by_instance(uint32_t instance_id) {
    if (instance_id == 0) {
        return get_global_registry();
    }

    for (uint32_t i = 0; i < g_registry_count; i++) {
        if (g_registries[i] && g_registries[i]->instance_id == instance_id) {
            return g_registries[i];
        }
    }
    return NULL;
}

// ============================================================================
// Explicit Registry API
// ============================================================================

IRAssetID ir_asset_register_ex(IRAssetRegistry* registry, const char* path,
                                IRAssetType type, void* data, size_t size) {
    if (!registry || !path) return IR_INVALID_ASSET;

    // Check if already exists
    IRAsset* existing = find_asset_in_registry(registry, path);
    if (existing) {
        existing->ref_count++;
        return existing->id;
    }

    // Check capacity
    if (registry->asset_count >= IR_MAX_ASSETS) {
        fprintf(stderr, "[Asset] Maximum assets reached (%d)\n", IR_MAX_ASSETS);
        return IR_INVALID_ASSET;
    }

    // Allocate new asset
    IRAsset* asset = &registry->assets[registry->asset_count++];
    asset->id = registry->asset_count;  // Simple ID assignment
    asset->type = type;
    asset->state = IR_ASSET_LOADED;
    strncpy(asset->path, path, IR_MAX_PATH_LENGTH - 1);
    asset->path[IR_MAX_PATH_LENGTH - 1] = '\0';
    asset->data = data;
    asset->data_size = size;
    asset->ref_count = 1;
    asset->load_time = get_time_ms();
    asset->hot_reload = false;

    return asset->id;
}

void ir_asset_unload_ex(IRAssetRegistry* registry, const char* path) {
    if (!registry || !path) return;

    for (uint32_t i = 0; i < registry->asset_count; i++) {
        if (strcmp(registry->assets[i].path, path) == 0) {
            IRAsset* asset = &registry->assets[i];
            if (asset->ref_count > 0) {
                asset->ref_count--;
                if (asset->ref_count == 0 && asset->data) {
                    // Unload
                    if (registry->custom_loaders[asset->type].unload) {
                        registry->custom_loaders[asset->type].unload(asset->data);
                    }
                    free(asset->data);
                    asset->data = NULL;
                    asset->state = IR_ASSET_UNLOADED;
                }
            }
            return;
        }
    }
}

IRAsset* ir_asset_get_ex(IRAssetRegistry* registry, const char* path) {
    if (!registry || !path) return NULL;
    return find_asset_in_registry(registry, path);
}

bool ir_asset_add_search_path_ex(IRAssetRegistry* registry, const char* real_path, const char* alias) {
    if (!registry || !real_path || !alias) return false;

    if (registry->search_path_count >= IR_MAX_SEARCH_PATHS) {
        fprintf(stderr, "[Asset] Maximum search paths reached (%d)\n", IR_MAX_SEARCH_PATHS);
        return false;
    }

    IRSearchPath* sp = &registry->search_paths[registry->search_path_count++];
    strncpy(sp->alias, alias, sizeof(sp->alias) - 1);
    sp->alias[sizeof(sp->alias) - 1] = '\0';
    strncpy(sp->real_path, real_path, IR_MAX_PATH_LENGTH - 1);
    sp->real_path[IR_MAX_PATH_LENGTH - 1] = '\0';

    return true;
}

bool ir_asset_resolve_path_ex(IRAssetRegistry* registry, const char* virtual_path,
                               char* out_real_path, size_t max_len) {
    if (!registry || !virtual_path || !out_real_path) return false;

    // Check if it's a virtual path (@alias/...)
    if (virtual_path[0] == '@') {
        const char* slash = strchr(virtual_path, '/');
        if (slash) {
            // Extract alias
            size_t alias_len = slash - virtual_path - 1;
            char alias[64];
            if (alias_len < sizeof(alias)) {
                strncpy(alias, virtual_path + 1, alias_len);
                alias[alias_len] = '\0';

                // Find matching search path
                for (uint32_t i = 0; i < registry->search_path_count; i++) {
                    if (strcmp(registry->search_paths[i].alias, alias) == 0) {
                        snprintf(out_real_path, max_len, "%s/%s",
                                 registry->search_paths[i].real_path, slash + 1);
                        return true;
                    }
                }
            }
        }
    }

    // Not a virtual path or alias not found, return as-is
    strncpy(out_real_path, virtual_path, max_len - 1);
    out_real_path[max_len - 1] = '\0';
    return true;
}

// Note: Sprite loading functions have been moved to the canvas plugin.
// Use kcanvas_sprite_* functions instead.

void* ir_asset_load_data_ex(IRAssetRegistry* registry, const char* path, size_t* out_size) {
    IRAssetRegistry* prev = ir_asset_set_current_registry(registry);
    void* result = ir_asset_load_data(path, out_size);
    ir_asset_set_current_registry(prev);
    return result;
}

char* ir_asset_load_text_ex(IRAssetRegistry* registry, const char* path) {
    IRAssetRegistry* prev = ir_asset_set_current_registry(registry);
    char* result = ir_asset_load_text(path);
    ir_asset_set_current_registry(prev);
    return result;
}

// ============================================================================
// Initialization (Global Registry)
// ============================================================================

void ir_asset_init(void) {
    memset(&g_asset_system, 0, sizeof(g_asset_system));
    g_asset_system.initialized = true;

    printf("[Asset] System initialized\n");
}

void ir_asset_shutdown(void) {
    if (!g_asset_system.initialized) return;

    // Unload all assets
    ir_asset_unload_all();

    memset(&g_asset_system, 0, sizeof(g_asset_system));

    printf("[Asset] System shutdown\n");
}

void ir_asset_update(void) {
    if (!g_asset_system.initialized) return;

    // Check for modified assets every 60 frames (~1 second)
    g_asset_system.hot_reload_check_count++;
    if (g_asset_system.hot_reload_check_count >= 60) {
        g_asset_system.hot_reload_check_count = 0;
        ir_asset_check_modified();
    }
}

// ============================================================================
// Search Paths
// ============================================================================

bool ir_asset_add_search_path(const char* real_path, const char* alias) {
    if (!g_asset_system.initialized) {
        ir_asset_init();
    }

    if (g_asset_system.search_path_count >= IR_MAX_SEARCH_PATHS) {
        fprintf(stderr, "[Asset] Max search paths reached\n");
        return false;
    }

    // Use the anonymous struct directly
    strncpy(g_asset_system.search_paths[g_asset_system.search_path_count].alias, alias, 63);
    g_asset_system.search_paths[g_asset_system.search_path_count].alias[63] = '\0';
    strncpy(g_asset_system.search_paths[g_asset_system.search_path_count].real_path, real_path, IR_MAX_PATH_LENGTH - 1);
    g_asset_system.search_paths[g_asset_system.search_path_count].real_path[IR_MAX_PATH_LENGTH - 1] = '\0';

    g_asset_system.search_path_count++;

    printf("[Asset] Added search path: @%s -> %s\n", alias, real_path);
    return true;
}

void ir_asset_remove_search_path(const char* alias) {
    for (uint32_t i = 0; i < g_asset_system.search_path_count; i++) {
        if (strcmp(g_asset_system.search_paths[i].alias, alias) == 0) {
            // Shift remaining paths down
            for (uint32_t j = i; j < g_asset_system.search_path_count - 1; j++) {
                g_asset_system.search_paths[j] = g_asset_system.search_paths[j + 1];
            }
            g_asset_system.search_path_count--;
            return;
        }
    }
}

bool ir_asset_resolve_path(const char* virtual_path, char* out_real_path, size_t max_len) {
    // Check if path starts with @
    if (virtual_path[0] != '@') {
        // Not a virtual path, use as-is
        strncpy(out_real_path, virtual_path, max_len - 1);
        out_real_path[max_len - 1] = '\0';
        return true;
    }

    // Find alias (everything between @ and first /)
    const char* slash = strchr(virtual_path + 1, '/');
    if (!slash) {
        fprintf(stderr, "[Asset] Invalid virtual path: %s\n", virtual_path);
        return false;
    }

    size_t alias_len = slash - (virtual_path + 1);
    char alias[64];
    strncpy(alias, virtual_path + 1, alias_len);
    alias[alias_len] = '\0';

    // Find matching search path
    for (uint32_t i = 0; i < g_asset_system.search_path_count; i++) {
        if (strcmp(g_asset_system.search_paths[i].alias, alias) == 0) {
            // Build real path
            snprintf(out_real_path, max_len, "%s%s",
                    g_asset_system.search_paths[i].real_path, slash);
            return true;
        }
    }

    fprintf(stderr, "[Asset] Unknown alias: @%s\n", alias);
    return false;
}

// ============================================================================
// Asset Loading (High-level API)
// ============================================================================

// Note: Sprite loading functions have been moved to the canvas plugin.
// Use kcanvas_sprite_atlas_create, kcanvas_sprite_load, etc.

void* ir_asset_load_data(const char* path, size_t* out_size) {
    if (!g_asset_system.initialized) {
        ir_asset_init();
    }

    // Resolve path
    char real_path[IR_MAX_PATH_LENGTH];
    if (!ir_asset_resolve_path(path, real_path, sizeof(real_path))) {
        return NULL;
    }

    // Load file
    FILE* file = fopen(real_path, "rb");
    if (!file) {
        fprintf(stderr, "[Asset] Failed to open file: %s\n", real_path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* data = malloc(size);
    if (!data) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(data, 1, size, file);
    fclose(file);

    if (read != size) {
        free(data);
        return NULL;
    }

    if (out_size) *out_size = size;

    // Register in asset system
    ir_asset_register(path, IR_ASSET_DATA, data, size);

    printf("[Asset] Loaded data: %s (%zu bytes)\n", path, size);
    return data;
}

char* ir_asset_load_text(const char* path) {
    size_t size;
    void* data = ir_asset_load_data(path, &size);
    if (!data) return NULL;

    // Ensure null-terminated
    char* text = (char*)realloc(data, size + 1);
    if (!text) {
        free(data);
        return NULL;
    }

    text[size] = '\0';
    return text;
}

// ============================================================================
// Asset Registry (Low-level API)
// ============================================================================

IRAssetID ir_asset_register(const char* path, IRAssetType type, void* data, size_t size) {
    if (!g_asset_system.initialized) {
        ir_asset_init();
    }

    if (g_asset_system.asset_count >= IR_MAX_ASSETS) {
        fprintf(stderr, "[Asset] Max assets reached\n");
        return IR_INVALID_ASSET;
    }

    IRAssetID id = g_asset_system.asset_count;
    IRAsset* asset = &g_asset_system.assets[id];

    memset(asset, 0, sizeof(IRAsset));
    asset->id = id;
    asset->type = type;
    asset->state = IR_ASSET_LOADED;
    strncpy(asset->path, path, sizeof(asset->path) - 1);
    asset->data = data;
    asset->data_size = size;
    asset->ref_count = 1;
    asset->load_time = 0;
    asset->file_mtime = 0;
    asset->hot_reload = false;

    // Resolve real path
    ir_asset_resolve_path(path, asset->real_path, sizeof(asset->real_path));

    g_asset_system.asset_count++;

    return id;
}

void ir_asset_unload(const char* path) {
    IRAsset* asset = find_asset(path);
    if (!asset) return;

    asset->ref_count--;
    if (asset->ref_count == 0) {
        // Free data based on type
        if (asset->data) {
            switch (asset->type) {
                case IR_ASSET_DATA:
                    free(asset->data);
                    break;
                // Other types managed by their respective systems
                default:
                    break;
            }
        }

        asset->state = IR_ASSET_UNLOADED;
        printf("[Asset] Unloaded: %s\n", path);
    }
}

IRAsset* ir_asset_get(const char* path) {
    return find_asset(path);
}

IRAsset* ir_asset_get_by_id(IRAssetID id) {
    if (id >= g_asset_system.asset_count) return NULL;
    return &g_asset_system.assets[id];
}

void ir_asset_retain(const char* path) {
    IRAsset* asset = find_asset(path);
    if (asset) {
        asset->ref_count++;
    }
}

void ir_asset_release(const char* path) {
    ir_asset_unload(path);
}

// ============================================================================
// Hot Reload
// ============================================================================

void ir_asset_enable_hot_reload(const char* path) {
    IRAsset* asset = find_asset(path);
    if (asset) {
        asset->hot_reload = true;
        asset->file_mtime = get_file_mtime(asset->real_path);
    }
}

void ir_asset_disable_hot_reload(const char* path) {
    IRAsset* asset = find_asset(path);
    if (asset) {
        asset->hot_reload = false;
    }
}

void ir_asset_set_hot_reload_callback(IRAssetHotReloadCallback callback, void* user_data) {
    g_asset_system.hot_reload_callback = callback;
    g_asset_system.hot_reload_user_data = user_data;
}

bool ir_asset_reload(const char* path) {
    IRAsset* asset = find_asset(path);
    if (!asset || asset->state != IR_ASSET_LOADED) {
        return false;
    }

    printf("[Asset] Reloading: %s (type=%d)\n", path, asset->type);

    // Check if there's a custom reload callback for this asset type
    if (asset->type >= 0 && asset->type <= IR_ASSET_CUSTOM &&
        g_asset_system.custom_loaders[asset->type].registered &&
        g_asset_system.custom_loaders[asset->type].reload) {

        // Call custom reload callback
        bool reloaded = g_asset_system.custom_loaders[asset->type].reload(asset);
        if (reloaded) {
            printf("[Asset] Successfully reloaded via custom callback\n");
        } else {
            printf("[Asset] Custom reload callback failed\n");
            return false;
        }
    } else {
        // Type-specific reload for built-in asset types
        switch (asset->type) {
            case IR_ASSET_SOUND:
                // Note: Audio has been moved to the audio plugin
                printf("[Asset] SOUND reload requested (use audio plugin)\n");
                break;

            case IR_ASSET_TEXTURE:
            case IR_ASSET_FONT:
            case IR_ASSET_MUSIC:
            case IR_ASSET_DATA:
            case IR_ASSET_SHADER:
            default:
                // For other types, just update modification time
                // Full reload would require backend-specific code
                printf("[Asset] Reload for type %d not fully implemented\n", asset->type);
                break;
        }
    }

    // Update modification time
    asset->file_mtime = get_file_mtime(asset->real_path);

    // Call hot reload callback
    if (g_asset_system.hot_reload_callback) {
        g_asset_system.hot_reload_callback(asset, g_asset_system.hot_reload_user_data);
    }

    return true;
}

void ir_asset_reload_all(void) {
    for (uint32_t i = 0; i < g_asset_system.asset_count; i++) {
        IRAsset* asset = &g_asset_system.assets[i];
        if (asset->state == IR_ASSET_LOADED && asset->hot_reload) {
            ir_asset_reload(asset->path);
        }
    }
}

uint32_t ir_asset_check_modified(void) {
    uint32_t modified_count = 0;

    for (uint32_t i = 0; i < g_asset_system.asset_count; i++) {
        IRAsset* asset = &g_asset_system.assets[i];
        if (asset->state != IR_ASSET_LOADED || !asset->hot_reload) {
            continue;
        }

        uint64_t current_mtime = get_file_mtime(asset->real_path);
        if (current_mtime > asset->file_mtime) {
            ir_asset_reload(asset->path);
            modified_count++;
        }
    }

    return modified_count;
}

// ============================================================================
// Asset Queries
// ============================================================================

uint32_t ir_asset_get_count(void) {
    return g_asset_system.asset_count;
}

size_t ir_asset_get_memory_usage(void) {
    size_t total = 0;
    for (uint32_t i = 0; i < g_asset_system.asset_count; i++) {
        total += g_asset_system.assets[i].data_size;
    }
    return total;
}

uint32_t ir_asset_get_by_type(IRAssetType type, IRAsset** out_assets, uint32_t max_assets) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_asset_system.asset_count && count < max_assets; i++) {
        if (g_asset_system.assets[i].type == type) {
            out_assets[count++] = &g_asset_system.assets[i];
        }
    }
    return count;
}

bool ir_asset_is_loaded(const char* path) {
    IRAsset* asset = find_asset(path);
    return asset && asset->state == IR_ASSET_LOADED;
}

IRAssetState ir_asset_get_state(const char* path) {
    IRAsset* asset = find_asset(path);
    return asset ? asset->state : IR_ASSET_UNLOADED;
}

// ============================================================================
// Memory Management
// ============================================================================

uint32_t ir_asset_gc(void) {
    uint32_t freed = 0;
    for (uint32_t i = 0; i < g_asset_system.asset_count; i++) {
        IRAsset* asset = &g_asset_system.assets[i];
        if (asset->ref_count == 0 && asset->state == IR_ASSET_LOADED) {
            ir_asset_unload(asset->path);
            freed++;
        }
    }
    return freed;
}

void ir_asset_unload_all(void) {
    for (uint32_t i = 0; i < g_asset_system.asset_count; i++) {
        IRAsset* asset = &g_asset_system.assets[i];
        if (asset->state == IR_ASSET_LOADED) {
            asset->ref_count = 1;  // Force unload
            ir_asset_unload(asset->path);
        }
    }
}

void ir_asset_preload(const char** paths, uint32_t count) {
    // Load all assets immediately
    // For true async background loading, a threading system would be needed
    for (uint32_t i = 0; i < count; i++) {
        const char* path = paths[i];
        if (!path) continue;

        // Note: Sprite and audio loading have been moved to plugins
        // For now, just log the preload request
        printf("[Asset] Preload requested: %s (use canvas/audio plugins)\n", path);
    }
}

// Queue an asset for background loading (to be processed later)
// Returns true if successfully queued
bool ir_asset_load_async(const char* path) {
    // For true async loading, this would queue the asset to be loaded
    // in a background thread. For now, we load synchronously.
    // The API is here for future async implementation.

    IRAsset* asset = find_asset(path);
    if (asset && asset->state == IR_ASSET_LOADED) {
        // Already loaded
        return true;
    }

    // Note: Sprite and audio loading have been moved to plugins
    // For now, just mark as queued
    printf("[Asset] Async load requested: %s (use canvas/audio plugins)\n", path);
    return true;
}

// ============================================================================
// Custom Asset Types
// ============================================================================

void ir_asset_register_loader(IRAssetType type,
                              IRAssetLoadCallback load_cb,
                              IRAssetUnloadCallback unload_cb,
                              IRAssetReloadCallback reload_cb) {
    if (type < 0 || type > IR_ASSET_CUSTOM) {
        fprintf(stderr, "[Asset] Invalid asset type for loader registration: %d\n", type);
        return;
    }

    g_asset_system.custom_loaders[type].load = load_cb;
    g_asset_system.custom_loaders[type].unload = unload_cb;
    g_asset_system.custom_loaders[type].reload = reload_cb;
    g_asset_system.custom_loaders[type].registered = true;

    printf("[Asset] Registered custom loader for type %d\n", type);
}

// ============================================================================
// Debug/Profiling
// ============================================================================

void ir_asset_print_registry(void) {
    printf("\n=== Asset Registry ===\n");
    printf("Total Assets: %u\n", g_asset_system.asset_count);
    printf("Memory Usage: %.2f MB\n", ir_asset_get_memory_usage() / (1024.0f * 1024.0f));
    printf("\nAssets:\n");

    for (uint32_t i = 0; i < g_asset_system.asset_count; i++) {
        IRAsset* asset = &g_asset_system.assets[i];
        const char* type_str = "UNKNOWN";
        switch (asset->type) {
            case IR_ASSET_TEXTURE: type_str = "TEXTURE"; break;
            case IR_ASSET_FONT: type_str = "FONT"; break;
            case IR_ASSET_SOUND: type_str = "SOUND"; break;
            case IR_ASSET_MUSIC: type_str = "MUSIC"; break;
            case IR_ASSET_DATA: type_str = "DATA"; break;
            case IR_ASSET_SHADER: type_str = "SHADER"; break;
            case IR_ASSET_CUSTOM: type_str = "CUSTOM"; break;
        }

        printf("  [%u] %s (refs: %u, %s) - %s\n",
               asset->id, type_str, asset->ref_count,
               asset->hot_reload ? "HOT" : "   ",
               asset->path);
    }

    printf("\n");
}

float ir_asset_get_load_time(const char* path) {
    IRAsset* asset = find_asset(path);
    return asset ? (float)asset->load_time : 0.0f;
}

void ir_asset_get_stats(IRAssetStats* stats) {
    if (!stats) return;

    memset(stats, 0, sizeof(IRAssetStats));
    stats->total_assets = g_asset_system.asset_count;
    stats->total_memory = ir_asset_get_memory_usage();

    for (uint32_t i = 0; i < g_asset_system.asset_count; i++) {
        IRAsset* asset = &g_asset_system.assets[i];
        switch (asset->state) {
            case IR_ASSET_LOADED:
                stats->loaded_assets++;
                break;
            case IR_ASSET_UNLOADED:
                stats->unloaded_assets++;
                break;
            case IR_ASSET_ERROR:
                stats->error_assets++;
                break;
            default:
                break;
        }

        if (asset->hot_reload) {
            stats->hot_reload_count++;
        }
    }
}
