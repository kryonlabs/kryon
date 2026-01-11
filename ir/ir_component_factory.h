/**
 * @file ir_component_factory.h
 * @brief Component creation and tree manipulation functions
 *
 * This module provides component lifecycle and tree structure functions.
 * Extracted from ir_builder.c to reduce file size and improve organization.
 */

#ifndef IR_COMPONENT_FACTORY_H
#define IR_COMPONENT_FACTORY_H

#include "ir_core.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Component Creation and Destruction
// ============================================================================

/**
 * @brief Create a new component with auto-generated ID
 * @param type Component type
 * @return New component or NULL on failure
 */
IRComponent* ir_create_component(IRComponentType type);

/**
 * @brief Create a new component with specific ID
 * @param type Component type
 * @param id Component ID (0 for auto-generation)
 * @return New component or NULL on failure
 */
IRComponent* ir_create_component_with_id(IRComponentType type, uint32_t id);

/**
 * @brief Destroy a component and all its children
 * @param component Component to destroy
 */
void ir_destroy_component(IRComponent* component);

// ============================================================================
// Tree Structure Management
// ============================================================================

/**
 * @brief Add a child component to a parent
 * @param parent Parent component
 * @param child Child component to add
 */
void ir_add_child(IRComponent* parent, IRComponent* child);

/**
 * @brief Remove a child component from its parent
 * @param parent Parent component
 * @param child Child component to remove
 */
void ir_remove_child(IRComponent* parent, IRComponent* child);

/**
 * @brief Insert a child component at a specific index
 * @param parent Parent component
 * @param child Child component to insert
 * @param index Index at which to insert
 */
void ir_insert_child(IRComponent* parent, IRComponent* child, uint32_t index);

/**
 * @brief Get a child component by index
 * @param component Parent component
 * @param index Child index
 * @return Child component or NULL if not found
 */
IRComponent* ir_get_child(IRComponent* component, uint32_t index);

/**
 * @brief Find a component by ID in the tree
 * @param root Root component to search from
 * @param id Component ID to find
 * @return Component or NULL if not found
 */
IRComponent* ir_find_component_by_id(IRComponent* root, uint32_t id);

// ============================================================================
// Component Property Setters
// ============================================================================

/**
 * @brief Set the text content of a component
 * @param component Component
 * @param text Text content
 */
void ir_set_text_content(IRComponent* component, const char* text);

/**
 * @brief Set custom data on a component
 * @param component Component
 * @param data Custom data string
 */
void ir_set_custom_data(IRComponent* component, const char* data);

/**
 * @brief Set the tag/identifier of a component
 * @param component Component
 * @param tag Tag string
 */
void ir_set_tag(IRComponent* component, const char* tag);

/**
 * @brief Set the ForEach data source
 * @param component Component
 * @param source ForEach data source identifier
 */
void ir_set_each_source(IRComponent* component, const char* source);

// ============================================================================
// Component State Helpers (Checkbox, Dropdown, etc.)
// ============================================================================

// Checkbox state helpers
bool ir_get_checkbox_state(IRComponent* component);
void ir_set_checkbox_state(IRComponent* component, bool checked);
void ir_toggle_checkbox_state(IRComponent* component);

// Dropdown state helpers
int32_t ir_get_dropdown_selected_index(IRComponent* component);
void ir_set_dropdown_selected_index(IRComponent* component, int32_t index);
void ir_set_dropdown_options(IRComponent* component, char** options, uint32_t count);
bool ir_get_dropdown_open_state(IRComponent* component);
void ir_set_dropdown_open_state(IRComponent* component, bool is_open);
void ir_toggle_dropdown_open_state(IRComponent* component);
int32_t ir_get_dropdown_hovered_index(IRComponent* component);
void ir_set_dropdown_hovered_index(IRComponent* component, int32_t index);

#endif // IR_COMPONENT_FACTORY_H
