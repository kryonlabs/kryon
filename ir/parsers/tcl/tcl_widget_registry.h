/**
 * Tcl Widget Registry
 * Maps between Tcl/Tk widget types and KIR widget types
 */

#ifndef TCL_WIDGET_REGISTRY_H
#define TCL_WIDGET_REGISTRY_H

#include <stdbool.h>

/* ============================================================================
 * Mapping Structures
 * ============================================================================ */

/**
 * Widget type mapping
 */
typedef struct {
    const char* tcl_widget;    // Tcl widget type (e.g., "button", "label")
    const char* kir_type;      // KIR widget type (e.g., "Button", "Text")
} TclWidgetMapping;

#define NULL_TERMINATOR {NULL, NULL}

/**
 * Property mapping
 */
typedef struct {
    const char* tcl_option;     // Tcl widget option (e.g., "-text", "-background")
    const char* kir_property;   // KIR property name (e.g., "text", "background")
} TclPropertyMapping;

/* ============================================================================
 * Widget Type Mapping API
 * ============================================================================ */

/**
 * Convert Tcl widget type to KIR widget type
 *
 * @param tcl_widget_type Tcl widget type (e.g., "button", "ttk::button")
 * @return const char* KIR widget type, or "Container" if unknown
 */
const char* tcl_widget_to_kir_type(const char* tcl_widget_type);

/**
 * Convert KIR widget type to Tcl widget type
 *
 * @param kir_widget_type KIR widget type (e.g., "Button", "Text")
 * @return const char* Tcl widget type, or "frame" if unknown
 */
const char* kir_widget_to_tcl_type(const char* kir_widget_type);

/**
 * Check if a Tcl widget type is valid/known
 *
 * @param tcl_widget_type Tcl widget type
 * @return true if known, false otherwise
 */
bool tcl_is_valid_widget_type(const char* tcl_widget_type);

/* ============================================================================
 * Property Mapping API
 * ============================================================================ */

/**
 * Convert Tcl widget option to KIR property
 *
 * @param tcl_option Tcl widget option (e.g., "-text", "-background")
 * @return const char* KIR property name, or NULL if unknown
 */
const char* tcl_option_to_kir_property(const char* tcl_option);

/**
 * Convert KIR property to Tcl widget option
 *
 * @param kir_property KIR property name
 * @return const char* Tcl option, or NULL if unknown
 */
const char* kir_property_to_tcl_option(const char* kir_property);

/**
 * Check if a Tcl option is valid/known
 *
 * @param tcl_option Tcl widget option
 * @return true if known, false otherwise
 */
bool tcl_is_valid_option(const char* tcl_option);

/* ============================================================================
 * Layout Manager API
 * ============================================================================ */

/**
 * Check if a command is a layout manager
 *
 * @param command Command name
 * @return true if pack/grid/place, false otherwise
 */
bool tcl_is_layout_command(const char* command);

/**
 * Convert Tcl layout manager to KIR layout type
 *
 * @param tcl_layout Layout manager name (pack/grid/place)
 * @return const char* KIR layout type, or NULL if unknown
 */
const char* tcl_layout_to_kir_layout(const char* tcl_layout);

/* ============================================================================
 * Widget Capabilities
 * ============================================================================ */

/**
 * Check if a widget type supports a specific option
 *
 * @param widget_type Tcl widget type
 * @param option Tcl option name
 * @return true if supported, false otherwise
 */
bool tcl_widget_supports_option(const char* widget_type, const char* option);

/* ============================================================================
 * Debug/Utilities
 * ============================================================================ */

/**
 * Dump all mappings to stdout (for debugging)
 */
void tcl_widget_registry_dump_mappings(void);

#endif /* TCL_WIDGET_REGISTRY_H */
