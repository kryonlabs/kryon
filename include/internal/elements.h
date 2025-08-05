/**
 * @file elements.h
 * @brief Kryon Element System - Complete Element Interface
 */

#ifndef KRYON_INTERNAL_ELEMENTS_H
#define KRYON_INTERNAL_ELEMENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "internal/types.h"
#include "internal/events.h"

// =============================================================================
// ELEMENT BASE TYPES
// =============================================================================

/**
 * @brief Element structure
 */
struct KryonElement {
    // Identification
    uint32_t id;                     // Unique element ID
    uint16_t type;                   // Element type (hex code)
    char *type_name;                 // Element type name
    char *element_id;                // User-defined ID (optional)
    
    // Hierarchy
    struct KryonElement *parent;            // Parent element
    struct KryonElement **children;         // Child elements
    size_t child_count;              // Number of children
    size_t child_capacity;           // Child array capacity
    
    // Properties
    KryonProperty **properties;      // Element properties
    size_t property_count;           // Number of properties
    size_t property_capacity;        // Property array capacity
    
    // Styling
    KryonStyle *computed_style;      // Computed style after cascade
    char **class_names;              // Applied CSS classes
    size_t class_count;              // Number of classes
    
    // Layout
    float x, y;                      // Position
    float width, height;             // Size
    float padding[4];                // Top, right, bottom, left
    float margin[4];                 // Top, right, bottom, left
    bool needs_layout;               // Layout dirty flag
    
    // State
    KryonElementState state;         // Lifecycle state
    bool visible;                    // Visibility flag
    bool enabled;                    // Enabled flag
    void *user_data;                 // User-defined data
    
    // Events
    ElementEventHandler **handlers;  // Event handlers
    size_t handler_count;            // Number of handlers
    size_t handler_capacity;         // Handler array capacity
    
    // Rendering
    bool needs_render;               // Render dirty flag
    uint32_t render_order;           // Z-order for rendering
    void *render_data;               // Renderer-specific data
    
    // Component instance (if this element is a component)
    KryonComponentInstance *component_instance; // Component instance data

    // Scripting
    void *script_state;
};

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_ELEMENTS_H