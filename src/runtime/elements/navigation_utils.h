/**
 * @file navigation_utils.h
 * @brief Shared navigation utilities for elements that support navigation
 * 
 * This file provides reusable navigation functions that can be used by
 * any element that needs to support internal/external navigation.
 * 
 * 0BSD License
 */

#ifndef KRYON_NAVIGATION_UTILS_H
#define KRYON_NAVIGATION_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Forward declarations to avoid circular dependencies
struct KryonRuntime;
struct KryonElement;

// =============================================================================
// NAVIGATION UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Ensures navigation manager is created when first navigation element is encountered.
 * 
 * @param runtime Runtime context
 * @param element_name Name of element type for debug messages
 */
void navigation_ensure_manager(struct KryonRuntime* runtime, const char* element_name);

/**
 * @brief Checks if element has navigation properties and initializes navigation manager if needed.
 * This should be called during element rendering for elements that support navigation.
 * 
 * @param runtime Runtime context
 * @param element Element to check for navigation properties
 * @param element_name Name of element type for debug messages
 */
void navigation_check_and_init(struct KryonRuntime* runtime, 
                              struct KryonElement* element, 
                              const char* element_name);

/**
 * @brief Handles navigation for an element with 'to' and 'external' properties.
 * This function consolidates all navigation logic including URL detection,
 * navigation manager usage, and external URL handling.
 * 
 * @param runtime Runtime context
 * @param element Element that triggered navigation
 * @param element_name Name of element type for debug messages
 * @return true if navigation was handled, false if no navigation properties
 */
bool navigation_handle_click(struct KryonRuntime* runtime, 
                            struct KryonElement* element,
                            const char* element_name);

/**
 * @brief Checks if a path is an external URL.
 * 
 * @param path Path to check
 * @return true if path is an external URL, false otherwise
 */
bool navigation_is_external_url(const char* path);

#ifdef __cplusplus
}
#endif

#endif // KRYON_NAVIGATION_UTILS_H