#ifndef IR_ASSET_H
#define IR_ASSET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ir_sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Kryon Asset Management System
 *
 * Provides centralized asset loading, caching, and hot-reloading for games.
 * Features:
 * - Type-safe asset loading (textures, sprites, fonts, sounds, data)
 * - Asset registry with reference counting
 * - Path aliases (e.g., "@sprites/player.png")
 * - Hot reload support (watches files for changes)
 * - Memory management (unload unused assets)
 * - Per-instance asset registries (for multi-instance support)
 * - Asset bundles/packs (future)
 *
 * Usage (Single Instance / Global):
 *   ir_asset_init();
 *   ir_asset_add_search_path("assets/sprites", "sprites");
 *
 *   IRSpriteID player = ir_asset_load_sprite("@sprites/player.png");
 *   // Use sprite...
 *   ir_asset_unload("@sprites/player.png");
 *
 *   ir_asset_shutdown();
 *
 * Usage (Multi-Instance):
 *   IRAssetRegistry* registry = ir_asset_registry_create(instance_id);
 *   IRSpriteID player = ir_asset_load_sprite_ex(registry, "@sprites/player.png");
 *   ir_asset_registry_destroy(registry);
 */

// ============================================================================
// Type Definitions
// ============================================================================

typedef uint32_t IRAssetID;
typedef uint32_t IRAssetHandle;

#define IR_INVALID_ASSET 0
#define IR_MAX_ASSETS 1024
#define IR_MAX_SEARCH_PATHS 16
#define IR_MAX_PATH_LENGTH 512

// Asset types
typedef enum {
    IR_ASSET_TEXTURE,     // Raw texture (atlas texture)
    IR_ASSET_SPRITE,      // Sprite with metadata
    IR_ASSET_SPRITE_ATLAS,// Sprite atlas
    IR_ASSET_FONT,        // TrueType font
    IR_ASSET_SOUND,       // Sound effect (WAV, OGG)
    IR_ASSET_MUSIC,       // Music track (streaming)
    IR_ASSET_DATA,        // Generic data (JSON, XML, etc.)
    IR_ASSET_SHADER,      // Shader program
    IR_ASSET_CUSTOM       // User-defined type
} IRAssetType;

// Asset state
typedef enum {
    IR_ASSET_UNLOADED,    // Not yet loaded
    IR_ASSET_LOADING,     // Currently loading
    IR_ASSET_LOADED,      // Successfully loaded
    IR_ASSET_ERROR        // Load failed
} IRAssetState;

// Asset metadata
typedef struct {
    IRAssetID id;
    IRAssetType type;
    IRAssetState state;

    char path[IR_MAX_PATH_LENGTH];    // Virtual path (e.g., "@sprites/player.png")
    char real_path[IR_MAX_PATH_LENGTH]; // Actual filesystem path

    void* data;           // Type-specific data
    size_t data_size;     // Size in bytes

    uint32_t ref_count;   // Reference count
    uint64_t load_time;   // When loaded (for hot reload)
    uint64_t file_mtime;  // File modification time

    bool hot_reload;      // Enable hot reload for this asset
} IRAsset;

// Asset load callbacks (for custom asset types)
typedef void* (*IRAssetLoadCallback)(const char* path, size_t* out_size);
typedef void (*IRAssetUnloadCallback)(void* data);
typedef bool (*IRAssetReloadCallback)(IRAsset* asset);

// Hot reload callback (called when asset is reloaded)
typedef void (*IRAssetHotReloadCallback)(IRAsset* asset, void* user_data);

// ============================================================================
// Per-Instance Asset Registry
// ============================================================================

/**
 * Search path entry for asset resolution
 */
typedef struct {
    char alias[64];              // Short name (e.g., "sprites")
    char real_path[IR_MAX_PATH_LENGTH];  // Actual filesystem path
} IRSearchPath;

/**
 * Custom loader entry for specific asset types
 */
typedef struct {
    IRAssetLoadCallback load;
    IRAssetUnloadCallback unload;
    IRAssetReloadCallback reload;
    bool registered;
} IRAssetCustomLoader;

/**
 * Per-instance asset registry
 *
 * Each instance (or the global registry) has its own:
 * - Asset cache (loaded assets with ref counting)
 * - Search paths (for virtual path resolution)
 * - Custom loaders (type-specific loading logic)
 * - Hot reload state
 */
typedef struct IRAssetRegistry {
    uint32_t instance_id;        // Owner instance ID (0 = global)

    // Asset storage
    IRAsset assets[IR_MAX_ASSETS];
    uint32_t asset_count;

    // Search paths
    IRSearchPath search_paths[IR_MAX_SEARCH_PATHS];
    uint32_t search_path_count;

    // Hot reload
    IRAssetHotReloadCallback hot_reload_callback;
    void* hot_reload_user_data;

    // Custom loaders (index by IRAssetType)
    IRAssetCustomLoader custom_loaders[IR_ASSET_CUSTOM + 1];

    // Default sprite atlas
    IRSpriteAtlasID default_atlas;
} IRAssetRegistry;

// ============================================================================
// Per-Instance Registry API
// ============================================================================

/**
 * Create a new asset registry for an instance
 * @param instance_id Instance ID (0 for global registry)
 * @return New registry, or NULL on failure
 */
IRAssetRegistry* ir_asset_registry_create(uint32_t instance_id);

/**
 * Destroy an asset registry
 * Unloads all assets in the registry
 * @param registry Registry to destroy (can be NULL)
 */
void ir_asset_registry_destroy(IRAssetRegistry* registry);

/**
 * Get the current thread's asset registry
 * Returns the instance's registry if set, otherwise global registry
 * @return Current registry, or NULL if not initialized
 */
IRAssetRegistry* ir_asset_get_current_registry(void);

/**
 * Set the current thread's asset registry
 * @param registry Registry to set as current (NULL for global)
 * @return Previous registry (for restoring)
 */
IRAssetRegistry* ir_asset_set_current_registry(IRAssetRegistry* registry);

/**
 * Get asset registry by instance ID
 * @param instance_id Instance ID
 * @return Registry, or NULL if not found
 */
IRAssetRegistry* ir_asset_get_registry_by_instance(uint32_t instance_id);

// ============================================================================
// Initialization (Global Registry)
// ============================================================================

/**
 * Initialize asset management system
 */
void ir_asset_init(void);

/**
 * Shutdown asset management system
 * Unloads all assets
 */
void ir_asset_shutdown(void);

/**
 * Update asset system (check for hot reload)
 * Call this every frame or periodically
 */
void ir_asset_update(void);

// ============================================================================
// Search Paths
// ============================================================================

/**
 * Add search path with alias
 * Example: ir_asset_add_search_path("assets/sprites", "sprites")
 * Then use: "@sprites/player.png"
 */
bool ir_asset_add_search_path(const char* real_path, const char* alias);

/**
 * Remove search path by alias
 */
void ir_asset_remove_search_path(const char* alias);

/**
 * Resolve virtual path to real filesystem path
 * Example: "@sprites/player.png" -> "assets/sprites/player.png"
 */
bool ir_asset_resolve_path(const char* virtual_path, char* out_real_path, size_t max_len);

// ============================================================================
// Asset Loading (High-level API)
// ============================================================================

/**
 * Load sprite atlas from image file
 * Returns atlas ID or IR_INVALID_SPRITE_ATLAS on failure
 */
IRSpriteAtlasID ir_asset_load_sprite_atlas(const char* path, uint16_t width, uint16_t height);

/**
 * Load individual sprite from image file
 * Automatically creates an atlas if needed
 */
IRSpriteID ir_asset_load_sprite(const char* path);

/**
 * Load sprite sheet (grid of sprites)
 * Returns array of sprite IDs (must be freed by caller)
 */
IRSpriteID* ir_asset_load_sprite_sheet(const char* path,
                                        uint16_t frame_width,
                                        uint16_t frame_height,
                                        uint16_t* out_frame_count);

/**
 * Load data file (JSON, text, etc.)
 * Returns pointer to data (must be freed by caller)
 */
void* ir_asset_load_data(const char* path, size_t* out_size);

/**
 * Load text file as string
 * Returns null-terminated string (must be freed by caller)
 */
char* ir_asset_load_text(const char* path);

// ============================================================================
// Explicit Registry API (for multi-instance support)
// ============================================================================

/**
 * Load sprite atlas from image file (explicit registry)
 */
IRSpriteAtlasID ir_asset_load_sprite_atlas_ex(IRAssetRegistry* registry, const char* path,
                                              uint16_t width, uint16_t height);

/**
 * Load individual sprite from image file (explicit registry)
 */
IRSpriteID ir_asset_load_sprite_ex(IRAssetRegistry* registry, const char* path);

/**
 * Load sprite sheet (grid of sprites) (explicit registry)
 */
IRSpriteID* ir_asset_load_sprite_sheet_ex(IRAssetRegistry* registry, const char* path,
                                          uint16_t frame_width, uint16_t frame_height,
                                          uint16_t* out_frame_count);

/**
 * Load data file (explicit registry)
 */
void* ir_asset_load_data_ex(IRAssetRegistry* registry, const char* path, size_t* out_size);

/**
 * Load text file as string (explicit registry)
 */
char* ir_asset_load_text_ex(IRAssetRegistry* registry, const char* path);

/**
 * Register asset (explicit registry)
 */
IRAssetID ir_asset_register_ex(IRAssetRegistry* registry, const char* path,
                                IRAssetType type, void* data, size_t size);

/**
 * Unload asset (explicit registry)
 */
void ir_asset_unload_ex(IRAssetRegistry* registry, const char* path);

/**
 * Get asset by path (explicit registry)
 */
IRAsset* ir_asset_get_ex(IRAssetRegistry* registry, const char* path);

/**
 * Add search path (explicit registry)
 */
bool ir_asset_add_search_path_ex(IRAssetRegistry* registry, const char* real_path, const char* alias);

/**
 * Resolve path (explicit registry)
 */
bool ir_asset_resolve_path_ex(IRAssetRegistry* registry, const char* virtual_path,
                               char* out_real_path, size_t max_len);

// ============================================================================
// Asset Registry (Low-level API)
// ============================================================================

/**
 * Register asset in the system
 * Returns asset ID
 */
IRAssetID ir_asset_register(const char* path, IRAssetType type, void* data, size_t size);

/**
 * Unregister asset
 * Decrements ref count, unloads if ref count reaches 0
 */
void ir_asset_unload(const char* path);

/**
 * Get asset by path
 * Returns NULL if not found
 */
IRAsset* ir_asset_get(const char* path);

/**
 * Get asset by ID
 */
IRAsset* ir_asset_get_by_id(IRAssetID id);

/**
 * Increment asset reference count
 */
void ir_asset_retain(const char* path);

/**
 * Decrement asset reference count
 * Unloads if ref count reaches 0
 */
void ir_asset_release(const char* path);

// ============================================================================
// Hot Reload
// ============================================================================

/**
 * Enable hot reload for an asset
 */
void ir_asset_enable_hot_reload(const char* path);

/**
 * Disable hot reload for an asset
 */
void ir_asset_disable_hot_reload(const char* path);

/**
 * Set hot reload callback (called when any asset is reloaded)
 */
void ir_asset_set_hot_reload_callback(IRAssetHotReloadCallback callback, void* user_data);

/**
 * Manually reload an asset
 * Returns true on success
 */
bool ir_asset_reload(const char* path);

/**
 * Reload all assets
 */
void ir_asset_reload_all(void);

/**
 * Check if any assets have been modified
 * Returns number of assets that need reloading
 */
uint32_t ir_asset_check_modified(void);

// ============================================================================
// Asset Queries
// ============================================================================

/**
 * Get total number of loaded assets
 */
uint32_t ir_asset_get_count(void);

/**
 * Get total memory used by assets (bytes)
 */
size_t ir_asset_get_memory_usage(void);

/**
 * Get assets by type
 * Returns number of assets found
 */
uint32_t ir_asset_get_by_type(IRAssetType type, IRAsset** out_assets, uint32_t max_assets);

/**
 * Check if asset is loaded
 */
bool ir_asset_is_loaded(const char* path);

/**
 * Get asset load state
 */
IRAssetState ir_asset_get_state(const char* path);

// ============================================================================
// Memory Management
// ============================================================================

/**
 * Unload all assets with ref count == 0
 * Returns number of assets unloaded
 */
uint32_t ir_asset_gc(void);

/**
 * Unload all assets (ignores ref count)
 */
void ir_asset_unload_all(void);

/**
 * Preload assets in background
 * Useful for loading screens
 */
void ir_asset_preload(const char** paths, uint32_t count);

/**
 * Load an asset asynchronously
 * Returns true if queued/successful, false on failure
 *
 * Note: Current implementation loads synchronously but provides
 * the API for future async/background threading support
 */
bool ir_asset_load_async(const char* path);

// ============================================================================
// Custom Asset Types
// ============================================================================

/**
 * Register custom asset loader
 */
void ir_asset_register_loader(IRAssetType type,
                              IRAssetLoadCallback load_cb,
                              IRAssetUnloadCallback unload_cb,
                              IRAssetReloadCallback reload_cb);

// ============================================================================
// Debug/Profiling
// ============================================================================

/**
 * Print asset registry for debugging
 */
void ir_asset_print_registry(void);

/**
 * Get asset load time (milliseconds)
 */
float ir_asset_get_load_time(const char* path);

/**
 * Get asset statistics
 */
typedef struct {
    uint32_t total_assets;
    uint32_t loaded_assets;
    uint32_t unloaded_assets;
    uint32_t error_assets;
    size_t total_memory;
    uint32_t hot_reload_count;
} IRAssetStats;

void ir_asset_get_stats(IRAssetStats* stats);

#ifdef __cplusplus
}
#endif

#endif // IR_ASSET_H
