/**
 * @file navigation.h
 * @brief Navigation system interface for Kryon Link elements.
 *
 * This header defines the navigation manager API for handling internal
 * file navigation (.kry/.krb) and external URL handling.
 *
 * 0BSD License
 */

#ifndef KRYON_NAVIGATION_H
#define KRYON_NAVIGATION_H

#include "runtime.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
//  Types and Structures
// =============================================================================

/**
 * @brief Navigation result codes.
 */
typedef enum {
    KRYON_NAV_SUCCESS = 0,
    KRYON_NAV_ERROR_FILE_NOT_FOUND,
    KRYON_NAV_ERROR_COMPILATION_FAILED,
    KRYON_NAV_ERROR_INVALID_PATH,
    KRYON_NAV_ERROR_EXTERNAL_FAILED,
    KRYON_NAV_ERROR_MEMORY,
    KRYON_NAV_ERROR_NO_HISTORY
} KryonNavigationResult;

/**
 * @brief Navigation history entry.
 */
typedef struct NavigationHistoryItem {
    char* path;                                   // File path or URL
    char* title;                                  // Optional display title
    time_t timestamp;                             // When navigation occurred
    struct NavigationHistoryItem* next;          // Next item in history
    struct NavigationHistoryItem* prev;          // Previous item in history
} NavigationHistoryItem;

/**
 * @brief Overlay injection information for navigation.
 */
typedef struct {
    char* component_name;                        // Name of component to inject (owned copy)
    KryonComponentDefinition* component_def;     // Deep copy of component definition
} KryonNavigationOverlay;

/**
 * @brief Navigation manager state.
 */
typedef struct {
    NavigationHistoryItem* current;              // Current page in history
    NavigationHistoryItem* head;                 // Start of history list
    NavigationHistoryItem* tail;                 // End of history list
    size_t history_count;                        // Number of items in history
    size_t max_history;                          // Maximum history items to keep
    
    // Compilation cache
    struct CompilationCacheEntry* cache_head;    // Compilation cache list
    size_t cache_count;                          // Number of cached compilations
    size_t max_cache;                            // Maximum cache entries
    
    // Runtime reference
    KryonRuntime* runtime;                       // Associated runtime
    
    // Current overlay to inject (if any)
    KryonNavigationOverlay* pending_overlay;     // Overlay to inject on next navigation
} KryonNavigationManager;

/**
 * @brief Compilation cache entry.
 */
typedef struct CompilationCacheEntry {
    char* kry_path;                              // Source .kry file path
    char* krb_path;                              // Compiled .krb file path
    time_t kry_mtime;                            // Source file modification time
    time_t krb_mtime;                            // Compiled file modification time
    struct CompilationCacheEntry* next;          // Next cache entry
} CompilationCacheEntry;

// =============================================================================
//  Navigation Manager API
// =============================================================================

/**
 * @brief Creates a new navigation manager.
 * @param runtime The associated Kryon runtime
 * @return Pointer to navigation manager or NULL on failure
 */
KryonNavigationManager* kryon_navigation_create(KryonRuntime* runtime);

/**
 * @brief Destroys a navigation manager and frees all resources.
 * @param nav_manager Navigation manager to destroy
 */
void kryon_navigation_destroy(KryonNavigationManager* nav_manager);

/**
 * @brief Navigates to a target path or URL.
 * @param nav_manager Navigation manager
 * @param target Target path (.kry/.krb file) or URL
 * @param external Whether to treat as external URL
 * @return Navigation result code
 */
KryonNavigationResult kryon_navigate_to(KryonNavigationManager* nav_manager, const char* target, bool external);

/**
 * @brief Sets an overlay component to inject during the next navigation.
 * @param nav_manager Navigation manager
 * @param component_name Name of the component to inject
 * @param source_runtime Runtime containing the component definition
 */
void kryon_navigation_set_overlay(KryonNavigationManager* nav_manager, const char* component_name, KryonRuntime* source_runtime);

/**
 * @brief Navigates back in history.
 * @param nav_manager Navigation manager
 * @return Navigation result code
 */
KryonNavigationResult kryon_navigate_back(KryonNavigationManager* nav_manager);

/**
 * @brief Navigates forward in history.
 * @param nav_manager Navigation manager
 * @return Navigation result code
 */
KryonNavigationResult kryon_navigate_forward(KryonNavigationManager* nav_manager);

/**
 * @brief Gets the current navigation item.
 * @param nav_manager Navigation manager
 * @return Current navigation item or NULL
 */
NavigationHistoryItem* kryon_navigation_get_current(KryonNavigationManager* nav_manager);

/**
 * @brief Sets the current navigation path without adding to history.
 * @param nav_manager Navigation manager
 * @param path Current file path
 */
void kryon_navigation_set_current_path(KryonNavigationManager* nav_manager, const char* path);

/**
 * @brief Checks if navigation back is possible.
 * @param nav_manager Navigation manager
 * @return true if can navigate back
 */
bool kryon_navigation_can_go_back(KryonNavigationManager* nav_manager);

/**
 * @brief Checks if navigation forward is possible.
 * @param nav_manager Navigation manager
 * @return true if can navigate forward
 */
bool kryon_navigation_can_go_forward(KryonNavigationManager* nav_manager);

// =============================================================================
//  File Navigation API
// =============================================================================

/**
 * @brief Loads a KRB file directly.
 * @param nav_manager Navigation manager
 * @param krb_path Path to .krb file
 * @return Navigation result code
 */
KryonNavigationResult kryon_navigation_load_krb(KryonNavigationManager* nav_manager, const char* krb_path);

/**
 * @brief Compiles a KRY file and loads the result.
 * @param nav_manager Navigation manager
 * @param kry_path Path to .kry file
 * @return Navigation result code
 */
KryonNavigationResult kryon_navigation_compile_and_load(KryonNavigationManager* nav_manager, const char* kry_path);

/**
 * @brief Opens an external URL in the system browser.
 * @param url URL to open
 * @return Navigation result code
 */
KryonNavigationResult kryon_navigation_open_external(const char* url);

// =============================================================================
//  Compilation Cache API
// =============================================================================

/**
 * @brief Compiles a .kry file to a temporary .krb file.
 * @param nav_manager Navigation manager
 * @param kry_path Source .kry file path
 * @param output_krb_path Output parameter for compiled .krb path
 * @return Navigation result code
 */
KryonNavigationResult kryon_compile_kry_to_temp(KryonNavigationManager* nav_manager, const char* kry_path, char** output_krb_path);

/**
 * @brief Clears the compilation cache.
 * @param nav_manager Navigation manager
 */
void kryon_navigation_clear_cache(KryonNavigationManager* nav_manager);

// =============================================================================
//  Utility Functions
// =============================================================================

/**
 * @brief Checks if a path is an external URL.
 * @param path Path to check
 * @return true if external URL
 */
bool kryon_navigation_is_external_url(const char* path);

/**
 * @brief Checks if a file exists.
 * @param path File path to check
 * @return true if file exists
 */
bool kryon_navigation_file_exists(const char* path);

/**
 * @brief Gets file modification time.
 * @param path File path
 * @return Modification time or 0 on error
 */
time_t kryon_navigation_file_mtime(const char* path);

/**
 * @brief Converts navigation result to string.
 * @param result Navigation result code
 * @return Human-readable string
 */
const char* kryon_navigation_result_string(KryonNavigationResult result);

#ifdef __cplusplus
}
#endif

#endif // KRYON_NAVIGATION_H