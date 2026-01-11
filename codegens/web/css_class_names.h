/**
 * @file css_class_names.h
 * @brief Component class name mapping for CSS generation
 *
 * Provides mappings from IR component types to CSS class names.
 */

#ifndef CSS_CLASS_NAMES_H
#define CSS_CLASS_NAMES_H

#include <stdbool.h>
#include <stddef.h>
#include "../../ir/ir_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Component Class Name Mapping
// ============================================================================

/**
 * Get the CSS class name for a component type
 * Returns a natural, semantic class name for the given component type.
 * For example, IR_COMPONENT_BUTTON -> "button", IR_COMPONENT_ROW -> "row"
 *
 * @param type The IR component type
 * @return Static string containing the class name (never NULL)
 */
const char* css_get_class_name_for_type(IRComponentType type);

/**
 * Get the HTML element tag name for a component type
 * Returns the recommended HTML tag for the given component type.
 * For example, IR_COMPONENT_TEXT -> "span", IR_COMPONENT_BUTTON -> "button"
 *
 * @param type The IR component type
 * @return Static string containing the tag name (never NULL)
 */
const char* css_get_tag_name_for_type(IRComponentType type);

/**
 * Check if a component type is a container (should use display: flex/grid)
 *
 * @param type The IR component type
 * @return true if the component type is a container
 */
bool css_is_container_type(IRComponentType type);

/**
 * Check if a component type is a text element
 *
 * @param type The IR component type
 * @return true if the component type is a text element
 */
bool css_is_text_type(IRComponentType type);

/**
 * Check if a component type is a form element
 *
 * @param type The IR component type
 * @return true if the component type is a form element
 */
bool css_is_form_type(IRComponentType type);

/**
 * Check if a component type is self-closing in HTML
 *
 * @param type The IR component type
 * @return true if the component type is self-closing
 */
bool css_is_self_closing_type(IRComponentType type);

/**
 * Generate a compound CSS selector from space-separated class names
 * Converts "btn btn-primary" to ".btn.btn-primary"
 *
 * @param class_names Space-separated class names
 * @return Allocated selector string (caller must free), or NULL on error
 */
char* css_classes_to_selector(const char* class_names);

/**
 * Generate a complete CSS selector for a component
 * Includes element selector, class selector, and any pseudo-classes
 *
 * @param component The IR component
 * @return Allocated selector string (caller must free), or NULL on error
 */
char* css_generate_selector(IRComponent* component);

/**
 * Generate a pseudo-class selector for a component
 * e.g., ".button:hover" or "button:focus"
 *
 * @param component The IR component
 * @param pseudo_class The pseudo-class name (without colon)
 * @return Allocated selector string (caller must free), or NULL on error
 */
char* css_generate_pseudo_selector(IRComponent* component, const char* pseudo_class);

/**
 * Check if a selector is an element selector (not class or ID)
 *
 * @param selector The CSS selector to check
 * @return true if the selector is an element selector
 */
bool css_is_element_selector(const char* selector);

/**
 * Check if a selector is a class selector
 *
 * @param selector The CSS selector to check
 * @return true if the selector is a class selector
 */
bool css_is_class_selector(const char* selector);

/**
 * Check if a selector is an ID selector
 *
 * @param selector The CSS selector to check
 * @return true if the selector is an ID selector
 */
bool css_is_id_selector(const char* selector);

/**
 * Extract the base class name from a compound selector
 * e.g., ".btn.btn-primary" -> "btn"
 *
 * @param selector The CSS selector
 * @return Static string containing the base class name, or NULL
 */
const char* css_extract_base_class(const char* selector);

/**
 * Get the default display value for a component type
 *
 * @param type The IR component type
 * @return CSS display value (e.g., "flex", "block", "inline")
 */
const char* css_get_default_display(IRComponentType type);

/**
 * Check if a component type should output inline styles for width/height
 * Some components (like Image) use inline width/height, others don't.
 *
 * @param type The IR component type
 * @return true if inline dimensions should be output
 */
bool css_uses_inline_dimensions(IRComponentType type);

#ifdef __cplusplus
}
#endif

#endif // CSS_CLASS_NAMES_H
