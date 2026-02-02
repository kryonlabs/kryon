/**
 * Kryon Toolkit Registry
 *
 * Registry system for UI toolkit profiles.
 * Each toolkit (Tk, Draw, DOM, Android Views, stdio) has its own profile
 * that defines widget types, properties, and layout systems.
 */

#ifndef TOOLKIT_REGISTRY_H
#define TOOLKIT_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Toolkit Type Enumeration
// ============================================================================

/**
 * Supported UI toolkits
 */
typedef enum {
    TOOLKIT_TK,              // Tcl/Tk
    TOOLKIT_DRAW,            // Inferno/Limbo Draw
    TOOLKIT_DOM,             // HTML/DOM
    TOOLKIT_ANDROID_VIEWS,   // Android Views
    TOOLKIT_TERMINAL,        // Terminal/console
    TOOLKIT_SDL3,            // SDL3
    TOOLKIT_RAYLIB,          // Raylib
    TOOLKIT_NONE             // No toolkit (logic only)
} ToolkitType;

/**
 * Convert toolkit type string to enum
 * @param type_str Toolkit type string (e.g., "tk", "draw")
 * @return ToolkitType enum, or TOOLKIT_NONE if invalid
 */
ToolkitType toolkit_type_from_string(const char* type_str);

/**
 * Convert toolkit enum to string
 * @param type ToolkitType enum
 * @return Static string representation, or "unknown" if invalid
 */
const char* toolkit_type_to_string(ToolkitType type);

/**
 * Check if toolkit type is valid
 * @param type ToolkitType to check
 * @return true if valid, false otherwise
 */
bool toolkit_type_is_valid(ToolkitType type);

// ============================================================================
// Toolkit Profile
// ============================================================================

/**
 * Widget mapping entry
 * Maps KIR widget types to toolkit-specific widget types
 */
typedef struct {
    const char* kir_type;      // KIR widget type (e.g., "Button", "Input")
    const char* toolkit_type;  // Toolkit widget type (e.g., "button", "entry" for Tk)
    const char* class_name;    // Optional class name for object-oriented toolkits
} ToolkitWidgetMapping;

/**
 * Property mapping entry
 * Maps KIR property names to toolkit-specific property names
 */
typedef struct {
    const char* kir_property;     // KIR property name
    const char* toolkit_property; // Toolkit property name
    const char* transform;        // Optional transformation function
} ToolkitPropertyMapping;

/**
 * Layout system characteristics
 */
typedef enum {
    LAYOUT_PACK,        // Pack layout (Tk-style)
    LAYOUT_GRID,        // Grid layout
    LAYOUT_ABSOLUTE,    // Absolute positioning (Draw-style)
    LAYOUT_FLEX,        // Flex layout (DOM-style)
    LAYOUT_CONSTRAINT,  // Constraint layout (Android-style)
    LAYOUT_NONE         // No layout system
} LayoutSystemType;

/**
 * Toolkit profile
 * Defines all characteristics of a UI toolkit
 */
typedef struct {
    const char* name;              // Toolkit name (e.g., "tk", "draw")
    ToolkitType type;              // Toolkit type enum

    // Widget mapping
    const ToolkitWidgetMapping* widgets;  // Array of widget mappings
    size_t widget_count;                   // Number of widget mappings

    // Property mapping
    const ToolkitPropertyMapping* properties;  // Array of property mappings
    size_t property_count;                     // Number of property mappings

    // Layout system
    LayoutSystemType layout_system;      // Primary layout system
    bool supports_percentages;           // Supports percentage sizing
    bool supports_absolute_positioning;  // Supports absolute positioning

    // Event handling
    bool supports_events;                // Supports event binding
    const char** supported_events;       // Array of supported event types
    size_t event_count;                  // Number of supported events

    // Code generation
    const char* file_extension;          // Generated file extension
    const char* comment_prefix;          // Comment prefix (e.g., "#", "//")
    const char* string_quote;            // String quote character

} ToolkitProfile;

// ============================================================================
// Toolkit Registry API
// ============================================================================

/**
 * Register a toolkit profile
 * @param profile Toolkit profile to register (static storage, not copied)
 * @return true on success, false on failure
 */
bool toolkit_register(const ToolkitProfile* profile);

/**
 * Get toolkit profile by name
 * @param name Toolkit name (e.g., "tk", "draw")
 * @return Toolkit profile, or NULL if not found
 */
const ToolkitProfile* toolkit_get_profile(const char* name);

/**
 * Get toolkit profile by type
 * @param type ToolkitType enum
 * @return Toolkit profile, or NULL if not found
 */
const ToolkitProfile* toolkit_get_profile_by_type(ToolkitType type);

/**
 * Check if toolkit is registered
 * @param name Toolkit name
 * @return true if registered, false otherwise
 */
bool toolkit_is_registered(const char* name);

/**
 * Get all registered toolkits
 * @param out_types Output array for toolkit types
 * @param max_count Maximum number of toolkits to return
 * @return Number of toolkits returned
 */
size_t toolkit_get_all_registered(const ToolkitProfile** out_profiles, size_t max_count);

/**
 * Map KIR widget type to toolkit widget type
 * @param profile Toolkit profile
 * @param kir_type KIR widget type
 * @return Toolkit widget type, or NULL if not found
 */
const char* toolkit_map_widget_type(const ToolkitProfile* profile, const char* kir_type);

/**
 * Map KIR property to toolkit property
 * @param profile Toolkit profile
 * @param kir_property KIR property name
 * @return Toolkit property name, or NULL if not found
 */
const char* toolkit_map_property(const ToolkitProfile* profile, const char* kir_property);

/**
 * Initialize toolkit registry with all built-in toolkits
 * Called automatically during system initialization
 */
void toolkit_registry_init(void);

/**
 * Cleanup toolkit registry
 */
void toolkit_registry_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // TOOLKIT_REGISTRY_H
