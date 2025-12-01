/**
 * Kryon Hot Reload System
 *
 * File watcher and hot reload infrastructure for development workflows.
 * Monitors source files for changes and triggers rebuild/reload cycles.
 */

#ifndef IR_HOT_RELOAD_H
#define IR_HOT_RELOAD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// File Watcher Types
// ============================================================================

typedef struct IRFileWatcher IRFileWatcher;

typedef enum {
    IR_FILE_EVENT_CREATED,
    IR_FILE_EVENT_MODIFIED,
    IR_FILE_EVENT_DELETED,
    IR_FILE_EVENT_MOVED
} IRFileEventType;

typedef struct {
    IRFileEventType type;
    char* path;
    char* old_path;  // For MOVED events
    uint64_t timestamp;
} IRFileEvent;

// Callback for file change notifications
typedef void (*IRFileWatchCallback)(const IRFileEvent* event, void* user_data);

// ============================================================================
// File Watcher API
// ============================================================================

/**
 * Create a file watcher instance
 */
IRFileWatcher* ir_file_watcher_create(void);

/**
 * Add a path to watch (file or directory)
 *
 * @param watcher The file watcher instance
 * @param path Path to watch (supports directories with recursive watching)
 * @param recursive If true, watch subdirectories recursively
 * @return true on success, false on failure
 */
bool ir_file_watcher_add_path(IRFileWatcher* watcher, const char* path, bool recursive);

/**
 * Remove a watched path
 */
bool ir_file_watcher_remove_path(IRFileWatcher* watcher, const char* path);

/**
 * Set callback for file events
 */
void ir_file_watcher_set_callback(IRFileWatcher* watcher,
                                   IRFileWatchCallback callback,
                                   void* user_data);

/**
 * Poll for file events (non-blocking)
 * Call this regularly from your main loop
 *
 * @param watcher The file watcher instance
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking, -1 = blocking)
 * @return Number of events processed
 */
int ir_file_watcher_poll(IRFileWatcher* watcher, int timeout_ms);

/**
 * Check if watcher is active
 */
bool ir_file_watcher_is_active(IRFileWatcher* watcher);

/**
 * Destroy file watcher and free resources
 */
void ir_file_watcher_destroy(IRFileWatcher* watcher);

// ============================================================================
// Hot Reload State Preservation
// ============================================================================

typedef struct {
    uint64_t component_id;
    char* state_data;       // Serialized state (JSON or custom format)
    uint32_t state_size;
} IRComponentState;

typedef struct {
    IRComponentState* states;
    uint32_t state_count;
    uint32_t capacity;
} IRStateSnapshot;

/**
 * Capture current state of the IR tree
 * This includes:
 * - Input field values
 * - Checkbox states
 * - Tab selections
 * - Scroll positions
 * - Reactive state values
 */
IRStateSnapshot* ir_capture_state(struct IRComponent* root);

/**
 * Restore state to an IR tree after rebuild
 * Matches components by ID and restores their saved state
 */
void ir_restore_state(struct IRComponent* root, const IRStateSnapshot* snapshot);

/**
 * Destroy state snapshot and free memory
 */
void ir_state_snapshot_destroy(IRStateSnapshot* snapshot);

// ============================================================================
// Hot Reload Protocol
// ============================================================================

typedef enum {
    IR_RELOAD_SUCCESS,
    IR_RELOAD_BUILD_FAILED,
    IR_RELOAD_NO_CHANGES,
    IR_RELOAD_ERROR
} IRReloadResult;

typedef struct IRHotReloadContext IRHotReloadContext;

// Callback to rebuild the UI (called on file change)
typedef struct IRComponent* (*IRRebuildCallback)(void* user_data);

/**
 * Create hot reload context
 */
IRHotReloadContext* ir_hot_reload_create(void);

/**
 * Set rebuild callback (called when files change)
 */
void ir_hot_reload_set_rebuild_callback(IRHotReloadContext* ctx,
                                         IRRebuildCallback callback,
                                         void* user_data);

/**
 * Add source file to watch
 */
bool ir_hot_reload_watch_file(IRHotReloadContext* ctx, const char* path);

/**
 * Add directory to watch recursively
 */
bool ir_hot_reload_watch_directory(IRHotReloadContext* ctx, const char* path);

/**
 * Process reload cycle (call from main loop)
 * This checks for file changes, triggers rebuild if needed, and swaps IR trees
 *
 * @param ctx Hot reload context
 * @param current_root Pointer to current root component (will be updated on reload)
 * @return Reload result
 */
IRReloadResult ir_hot_reload_poll(IRHotReloadContext* ctx,
                                   struct IRComponent** current_root);

/**
 * Enable/disable hot reload
 */
void ir_hot_reload_set_enabled(IRHotReloadContext* ctx, bool enabled);

/**
 * Check if hot reload is enabled
 */
bool ir_hot_reload_is_enabled(IRHotReloadContext* ctx);

/**
 * Get the file watcher from hot reload context
 */
IRFileWatcher* ir_hot_reload_get_watcher(IRHotReloadContext* ctx);

/**
 * Destroy hot reload context
 */
void ir_hot_reload_destroy(IRHotReloadContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // IR_HOT_RELOAD_H
