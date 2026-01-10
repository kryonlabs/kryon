/**
 * Kryon Instance Management
 *
 * This module provides multi-instance support for Kryon applications.
 * Each instance has its own IRContext, executor, assets, audio, and rendering state.
 *
 * Key Design:
 * - IRInstanceContext is a lightweight wrapper bundling per-instance contexts
 * - Existing IR structures already have instance tracking (owner_instance_id, scope)
 * - Main work is converting global contexts to per-instance contexts
 */

#ifndef IR_INSTANCE_H
#define IR_INSTANCE_H

#include "ir_core.h"
#include "ir_executor.h"
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct IRAssetRegistry IRAssetRegistry;
typedef struct IRAudioState IRAudioState;
typedef struct IRCommandBuffer IRCommandBuffer;
typedef struct IRFileWatcher IRFileWatcher;
typedef struct IRDesktopState IRDesktopState;

// Forward declare IRInstanceContext for use in callbacks
typedef struct IRInstanceContext IRInstanceContext;

// Forward declare IRDesktopState (from backends/desktop)
typedef struct IRDesktopState IRDesktopState;

// ============================================================================
// Render Target Types
// ============================================================================

#define IR_MAX_INSTANCES 32
#define IR_INSTANCE_NAME_MAX 64

// ============================================================================
// Render Target Types
// ============================================================================

typedef enum {
    IR_RENDER_TARGET_WINDOW,     // OS window
    IR_RENDER_TARGET_TEXTURE,    // Render to texture
    IR_RENDER_TARGET_FRAMEBUFFER // Custom framebuffer
} IRRenderTargetType;

// Render target (binds instance to platform-specific output)
typedef struct {
    IRRenderTargetType type;
    uint32_t instance_id;        // Owning instance
    void* platform_handle;       // SDL_Window*, HWND, etc.
    int width, height;
    struct IRCommandBuffer* commands;
} IRRenderTarget;

// ============================================================================
// Instance Callbacks
// ============================================================================

typedef struct IRInstanceCallbacks {
    // Lifecycle callbacks
    void (*on_create)(IRInstanceContext* inst);
    void (*on_destroy)(IRInstanceContext* inst);
    void (*on_suspend)(IRInstanceContext* inst);
    void (*on_resume)(IRInstanceContext* inst);

    // Hot reload callbacks
    bool (*can_reload)(IRInstanceContext* inst);
    void (*on_before_reload)(IRInstanceContext* inst, IRComponent* old_root);
    void (*on_after_reload)(IRInstanceContext* inst, IRComponent* new_root);

    // Error handling
    void (*on_error)(IRInstanceContext* inst, const char* error);

    void* user_data;
} IRInstanceCallbacks;

// ============================================================================
// Instance Context
// ============================================================================

typedef struct IRInstanceContext {
    // Identification
    uint32_t instance_id;              // Unique instance ID
    char name[IR_INSTANCE_NAME_MAX];   // Instance name for debugging

    // IR Context (per-instance, moved from global)
    struct IRContext* ir_context;      // Component tree, logic, reactive state

    // Executor Context (per-instance, moved from global)
    IRExecutorContext* executor;       // Event handlers, variable storage

    // Per-Instance Resources
    struct {
        IRAssetRegistry* assets;       // Instance's asset cache
        IRAudioState* audio;           // Instance's audio mixer
        IRCommandBuffer* commands;     // Instance's render commands
    } resources;

    // Rendering
    struct {
        IRRenderTarget* target;        // Platform-specific (SDL_Window*, etc.)
        int viewport[4];               // x, y, width, height
        IRDesktopState* desktop;       // Desktop-specific state (SDL3, etc.)
    } render;

    // Hot Reload
    struct {
        bool enabled;
        char watch_path[512];
        IRFileWatcher* watcher;
        uint64_t last_reload_time;
        uint32_t reload_count;
    } hot_reload;

    // Lifecycle
    bool is_running;
    bool is_suspended;
    uint32_t version;                  // Increments on hot reload

    // Metadata
    void* user_data;
    IRInstanceCallbacks* callbacks;
} IRInstanceContext;

// ============================================================================
// Instance Registry
// ============================================================================

typedef struct {
    IRInstanceContext* instances[IR_MAX_INSTANCES];
    uint32_t count;
    uint32_t next_id;
} IRInstanceRegistry;

// Global instance registry
extern IRInstanceRegistry g_instance_registry;

// ============================================================================
// Lifecycle Functions
// ============================================================================

/**
 * Initialize the global instance registry
 * Call once at startup before creating any instances
 */
void ir_instance_registry_init(void);

/**
 * Shutdown the global instance registry
 * Destroys all active instances
 */
void ir_instance_registry_shutdown(void);

/**
 * Create a new instance
 * @param name Instance name for debugging (can be NULL)
 * @param callbacks Optional lifecycle callbacks (can be NULL)
 * @return New instance context, or NULL on failure
 */
IRInstanceContext* ir_instance_create(const char* name, IRInstanceCallbacks* callbacks);

/**
 * Destroy an instance
 * @param inst Instance to destroy (can be NULL)
 */
void ir_instance_destroy(IRInstanceContext* inst);

/**
 * Suspend an instance (pause execution and rendering)
 * @param inst Instance to suspend
 */
void ir_instance_suspend(IRInstanceContext* inst);

/**
 * Resume a suspended instance
 * @param inst Instance to resume
 */
void ir_instance_resume(IRInstanceContext* inst);

// ============================================================================
// Registry Lookup Functions
// ============================================================================

/**
 * Get instance by ID
 * @param instance_id Instance ID
 * @return Instance context, or NULL if not found
 */
IRInstanceContext* ir_instance_get_by_id(uint32_t instance_id);

/**
 * Get instance by name
 * @param name Instance name
 * @return Instance context, or NULL if not found
 */
IRInstanceContext* ir_instance_get_by_name(const char* name);

/**
 * Get number of active instances
 * @return Active instance count
 */
uint32_t ir_instance_get_count(void);

/**
 * Check if an instance is running
 * @param inst Instance to check
 * @return true if running, false otherwise
 */
bool ir_instance_is_running(IRInstanceContext* inst);

// ============================================================================
// Thread-Local Context Access
// ============================================================================

/**
 * Get the current instance for this thread
 * @return Current instance, or NULL if not set
 */
IRInstanceContext* ir_instance_get_current(void);

/**
 * Set the current instance for this thread
 * @param inst Instance to set as current
 */
void ir_instance_set_current(IRInstanceContext* inst);

/**
 * Push instance context (for nested execution)
 * @param inst Instance to push
 * @return Previous instance (for restoring)
 */
IRInstanceContext* ir_instance_push_context(IRInstanceContext* inst);

/**
 * Pop instance context
 * @param restore Previous instance to restore (from push_context)
 */
void ir_instance_pop_context(IRInstanceContext* restore);

// ============================================================================
// Component ID Namespacing
// ============================================================================

/**
 * Pack instance_id and local_id into a global component ID
 * Component ID format: (instance_id << 16) | local_id
 */
#define IR_COMPONENT_ID_PACK(instance_id, local_id) \
    (((uint32_t)(instance_id) << 16) | ((local_id) & 0xFFFF))

/**
 * Extract instance ID from a packed component ID
 */
#define IR_COMPONENT_ID_INSTANCE(packed_id) ((packed_id) >> 16)

/**
 * Extract local ID from a packed component ID
 */
#define IR_COMPONENT_ID_LOCAL(packed_id) ((packed_id) & 0xFFFF)

// ============================================================================
// Instance Query Helpers
// ============================================================================

/**
 * Get the instance ID that owns a component
 * @param comp Component to query
 * @return Instance ID, or 0 if global/unknown
 */
uint32_t ir_instance_get_owner(IRComponent* comp);

/**
 * Set the instance owner for a component
 * @param comp Component to update
 * @param instance_id Instance that owns this component
 */
void ir_instance_set_owner(IRComponent* comp, uint32_t instance_id);

/**
 * Get the scope string for a component
 * @param comp Component to query
 * @return Scope string (e.g., "Counter#0"), or NULL
 */
const char* ir_instance_get_scope(IRComponent* comp);

/**
 * Set the scope string for a component
 * @param comp Component to update
 * @param scope Scope string (e.g., "Counter#0", "global")
 * Note: Takes ownership of the string (will be freed with component)
 */
void ir_instance_set_scope(IRComponent* comp, char* scope);

// ============================================================================
// KIR Loading Functions
// ============================================================================

/**
 * Load a KIR JSON file into an instance
 * @param inst Instance to load into
 * @param filename Path to KIR file
 * @return Root component, or NULL on failure
 */
IRComponent* ir_instance_load_kir(IRInstanceContext* inst, const char* filename);

/**
 * Load a KIR JSON string into an instance
 * @param inst Instance to load into
 * @param json_string KIR JSON string
 * @return Root component, or NULL on failure
 */
IRComponent* ir_instance_load_kir_string(IRInstanceContext* inst, const char* json_string);

/**
 * Set the root component for an instance
 * @param inst Instance to update
 * @param root Root component to set (takes ownership)
 */
void ir_instance_set_root(IRInstanceContext* inst, IRComponent* root);

/**
 * Get the root component for an instance
 * @param inst Instance to query
 * @return Root component, or NULL if not set
 */
IRComponent* ir_instance_get_root(IRInstanceContext* inst);

/**
 * Set owner_instance_id recursively on a component tree
 * @param comp Root of component tree
 * @param instance_id Instance ID to set
 */
void ir_instance_set_owner_recursive(IRComponent* comp, uint32_t instance_id);

// ============================================================================
// IR Context Access (for loading components into specific contexts)
// ============================================================================

/**
 * Get the current IRContext for this thread
 * @return Current IRContext, or NULL if using global
 */
struct IRContext* ir_context_get_current(void);

/**
 * Set the current IRContext for this thread
 * @param ctx IRContext to set as current (NULL to use global)
 * @return Previous context (for restoring)
 */
struct IRContext* ir_context_set_current(struct IRContext* ctx);

// ============================================================================
// Debug/Logging
// ============================================================================

/**
 * Print instance registry state (for debugging)
 */
void ir_instance_registry_print(void);

/**
 * Print instance info (for debugging)
 * @param inst Instance to print
 */
void ir_instance_print(IRInstanceContext* inst);

#endif // IR_INSTANCE_H
