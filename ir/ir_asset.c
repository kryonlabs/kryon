/*
 * Kryon Asset Management System Implementation
 *
 * Centralized asset loading, caching, and hot-reloading
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
// Global State
// ============================================================================

typedef struct {
    char alias[64];
    char real_path[IR_MAX_PATH_LENGTH];
} IRSearchPath;

static struct {
    bool initialized;

    // Asset registry
    IRAsset assets[IR_MAX_ASSETS];
    uint32_t asset_count;

    // Search paths
    IRSearchPath search_paths[IR_MAX_SEARCH_PATHS];
    uint32_t search_path_count;

    // Hot reload
    IRAssetHotReloadCallback hot_reload_callback;
    void* hot_reload_user_data;
    uint32_t hot_reload_check_count;

    // Default sprite atlas for individual sprites
    IRSpriteAtlasID default_atlas;

    // Statistics
    uint64_t total_load_time_ms;
    uint32_t total_loads;
} g_asset_system = {0};

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

// Find asset by path (linear search - O(n) but acceptable for < 1024 assets)
static IRAsset* find_asset(const char* path) {
    for (uint32_t i = 0; i < g_asset_system.asset_count; i++) {
        if (strcmp(g_asset_system.assets[i].path, path) == 0) {
            return &g_asset_system.assets[i];
        }
    }
    return NULL;
}

// ============================================================================
// Initialization
// ============================================================================

void ir_asset_init(void) {
    memset(&g_asset_system, 0, sizeof(g_asset_system));

    // Initialize sprite system if not already initialized
    ir_sprite_init();

    // Create default sprite atlas (2048x2048)
    g_asset_system.default_atlas = ir_sprite_atlas_create(2048, 2048);

    g_asset_system.initialized = true;

    printf("[Asset] System initialized\n");
}

void ir_asset_shutdown(void) {
    if (!g_asset_system.initialized) return;

    // Unload all assets
    ir_asset_unload_all();

    // Destroy sprite atlas
    if (g_asset_system.default_atlas != IR_INVALID_SPRITE_ATLAS) {
        ir_sprite_atlas_destroy(g_asset_system.default_atlas);
    }

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

    IRSearchPath* sp = &g_asset_system.search_paths[g_asset_system.search_path_count];
    strncpy(sp->alias, alias, sizeof(sp->alias) - 1);
    strncpy(sp->real_path, real_path, sizeof(sp->real_path) - 1);

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

IRSpriteAtlasID ir_asset_load_sprite_atlas(const char* path, uint16_t width, uint16_t height) {
    if (!g_asset_system.initialized) {
        ir_asset_init();
    }

    // Resolve path
    char real_path[IR_MAX_PATH_LENGTH];
    if (!ir_asset_resolve_path(path, real_path, sizeof(real_path))) {
        return IR_INVALID_SPRITE_ATLAS;
    }

    // Create atlas
    IRSpriteAtlasID atlas = ir_sprite_atlas_create(width, height);
    if (atlas == IR_INVALID_SPRITE_ATLAS) {
        return IR_INVALID_SPRITE_ATLAS;
    }

    // Register in asset system
    ir_asset_register(path, IR_ASSET_SPRITE_ATLAS, (void*)(uintptr_t)atlas, 0);

    printf("[Asset] Loaded sprite atlas: %s\n", path);
    return atlas;
}

IRSpriteID ir_asset_load_sprite(const char* path) {
    if (!g_asset_system.initialized) {
        ir_asset_init();
    }

    // Check if already loaded
    IRAsset* asset = find_asset(path);
    if (asset && asset->state == IR_ASSET_LOADED) {
        asset->ref_count++;
        return (IRSpriteID)(uintptr_t)asset->data;
    }

    // Resolve path
    char real_path[IR_MAX_PATH_LENGTH];
    if (!ir_asset_resolve_path(path, real_path, sizeof(real_path))) {
        return IR_INVALID_SPRITE;
    }

    uint64_t start_time = get_time_ms();

    // Load sprite into default atlas
    IRSpriteID sprite = ir_sprite_atlas_add_image(g_asset_system.default_atlas, real_path);
    if (sprite == IR_INVALID_SPRITE) {
        fprintf(stderr, "[Asset] Failed to load sprite: %s\n", path);
        return IR_INVALID_SPRITE;
    }

    // Pack atlas if needed
    IRSpriteAtlas* atlas = ir_sprite_atlas_get(g_asset_system.default_atlas);
    if (atlas && !atlas->is_packed) {
        ir_sprite_atlas_pack(g_asset_system.default_atlas);
    }

    uint64_t load_time = get_time_ms() - start_time;

    // Register in asset system
    IRAssetID asset_id = ir_asset_register(path, IR_ASSET_SPRITE, (void*)(uintptr_t)sprite, 0);
    asset = ir_asset_get_by_id(asset_id);
    if (asset) {
        asset->load_time = load_time;
        asset->file_mtime = get_file_mtime(real_path);
    }

    g_asset_system.total_load_time_ms += load_time;
    g_asset_system.total_loads++;

    printf("[Asset] Loaded sprite: %s (%.2f ms)\n", path, (float)load_time);
    return sprite;
}

IRSpriteID* ir_asset_load_sprite_sheet(const char* path,
                                        uint16_t frame_width,
                                        uint16_t frame_height,
                                        uint16_t* out_frame_count) {
    if (!g_asset_system.initialized) {
        ir_asset_init();
    }

    // Resolve path
    char real_path[IR_MAX_PATH_LENGTH];
    if (!ir_asset_resolve_path(path, real_path, sizeof(real_path))) {
        if (out_frame_count) *out_frame_count = 0;
        return NULL;
    }

    uint64_t start_time = get_time_ms();

    // Load sprite sheet into default atlas
    IRSpriteID* sprite_ids = ir_sprite_load_sheet(g_asset_system.default_atlas,
                                                   real_path,
                                                   frame_width,
                                                   frame_height,
                                                   out_frame_count);

    if (!sprite_ids) {
        fprintf(stderr, "[Asset] Failed to load sprite sheet: %s\n", path);
        if (out_frame_count) *out_frame_count = 0;
        return NULL;
    }

    // Pack atlas to upload textures
    IRSpriteAtlas* atlas = ir_sprite_atlas_get(g_asset_system.default_atlas);
    if (atlas && !atlas->is_packed) {
        ir_sprite_atlas_pack(g_asset_system.default_atlas);
    }

    uint64_t load_time = get_time_ms() - start_time;

    // Register sprite sheet in asset system (store the first sprite ID as reference)
    uint16_t frame_count = out_frame_count ? *out_frame_count : 0;
    ir_asset_register(path, IR_ASSET_SPRITE, (void*)(uintptr_t)sprite_ids[0], 0);

    g_asset_system.total_load_time_ms += load_time;
    g_asset_system.total_loads++;

    printf("[Asset] Loaded sprite sheet: %s (%u frames, %.2f ms)\n",
           path, frame_count, (float)load_time);

    return sprite_ids;
}

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

    printf("[Asset] Reloading: %s\n", path);

    // TODO: Implement type-specific reload
    // For now, just update modification time
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
    // TODO: Implement background loading
    for (uint32_t i = 0; i < count; i++) {
        ir_asset_load_sprite(paths[i]);
    }
}

// ============================================================================
// Custom Asset Types
// ============================================================================

void ir_asset_register_loader(IRAssetType type,
                              IRAssetLoadCallback load_cb,
                              IRAssetUnloadCallback unload_cb,
                              IRAssetReloadCallback reload_cb) {
    // TODO: Implement custom loaders
    (void)type; (void)load_cb; (void)unload_cb; (void)reload_cb;
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
            case IR_ASSET_SPRITE: type_str = "SPRITE"; break;
            case IR_ASSET_SPRITE_ATLAS: type_str = "ATLAS"; break;
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
