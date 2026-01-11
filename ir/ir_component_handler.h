#ifndef IR_COMPONENT_HANDLER_H
#define IR_COMPONENT_HANDLER_H

#include "ir_core.h"
#include "ir_component_types.h"
#include "ir_layout_types.h"

// Forward declarations
typedef struct cJSON cJSON;

// ============================================================================
// Component Handler - Comprehensive Handler Interface
// ============================================================================

/**
 * IRComponentHandler
 *
 * A comprehensive handler interface for component types that provides:
 * - Layout computation (via IRLayoutTrait compatibility)
 * - Serialization/deserialization
 * - Validation
 * - Style defaults
 * - String representation
 *
 * This replaces 289+ duplicate switch statements across the codebase with
 * a centralized handler registry.
 */
typedef struct IRComponentHandler {
    // === Layout Operations (compatible with IRLayoutTrait) ===
    void (*layout_component)(IRComponent* c, IRLayoutConstraints constraints,
                             float parent_x, float parent_y);

    // === Serialization Operations ===
    /**
     * Serialize component-specific properties to JSON
     * @param comp The component to serialize
     * @param json The JSON object to add properties to
     * @return true on success, false on error
     */
    bool (*serialize)(const IRComponent* comp, cJSON* json);

    /**
     * Deserialize component-specific properties from JSON
     * @param json The JSON object to read properties from
     * @param comp The component to update
     * @return true on success, false on error
     */
    bool (*deserialize)(const cJSON* json, IRComponent* comp);

    // === Measurement Operations ===
    /**
     * Measure intrinsic dimensions of this component
     * @param comp The component to measure
     * @param out_width Output for intrinsic width
     * @param out_height Output for intrinsic height
     */
    void (*measure)(const IRComponent* comp, float* out_width, float* out_height);

    // === Style Operations ===
    /**
     * Get default style for this component type
     * @return Default IRStyle or NULL to use global defaults
     */
    IRStyle* (*get_default_style)(void);

    /**
     * Apply component-specific style transformations
     * @param comp The component to style
     * @return true if style was applied, false otherwise
     */
    bool (*apply_style)(IRComponent* comp);

    // === Validation Operations ===
    /**
     * Validate component structure and properties
     * @param comp The component to validate
     * @param error_msg Output buffer for error message
     * @param error_size Size of error message buffer
     * @return true if valid, false otherwise
     */
    bool (*validate)(const IRComponent* comp, char* error_msg, size_t error_size);

    // === String Representation ===
    /**
     * Get string representation of component for debugging
     * @param comp The component
     * @param buffer Output buffer
     * @param size Size of output buffer
     */
    void (*to_string)(const IRComponent* comp, char* buffer, size_t size);

    // === Metadata ===
    const char* name;              // Component type name (e.g., "Button")
    IRComponentType type;          // Component type enum value

} IRComponentHandler;

// ============================================================================
// Component Handler Registry
// ============================================================================

#define IR_COMPONENT_HANDLER_REGISTRY_SIZE (IR_COMPONENT_MAX + 1)

/**
 * Initialize the component handler registry
 * Must be called before any component operations
 */
void ir_component_handler_registry_init(void);

/**
 * Register a component handler
 * @param handler The handler to register
 * @return true on success, false if type already registered or out of bounds
 */
bool ir_component_handler_register(const IRComponentHandler* handler);

/**
 * Get handler for a component type
 * @param type The component type
 * @return Handler pointer or NULL if not registered
 */
const IRComponentHandler* ir_component_handler_get(IRComponentType type);

/**
 * Check if a handler is registered for a component type
 * @param type The component type
 * @return true if registered, false otherwise
 */
bool ir_component_handler_has(IRComponentType type);

// ============================================================================
// Convenience Dispatch Functions
// ============================================================================

/**
 * Dispatch serialization to component handler
 * @return true on success, false if no handler or handler failed
 */
bool ir_component_handler_serialize(const IRComponent* comp, cJSON* json);

/**
 * Dispatch deserialization to component handler
 * @return true on success, false if no handler or handler failed
 */
bool ir_component_handler_deserialize(const cJSON* json, IRComponent* comp);

/**
 * Dispatch measurement to component handler
 * @return true on success, false if no handler or handler failed
 */
bool ir_component_handler_measure(const IRComponent* comp,
                                   float* out_width, float* out_height);

/**
 * Dispatch style application to component handler
 * @return true on success, false if no handler or handler failed
 */
bool ir_component_handler_apply_style(IRComponent* comp);

/**
 * Get default style from component handler
 * @return IRStyle pointer or NULL if no handler/default
 */
IRStyle* ir_component_handler_get_default_style(IRComponentType type);

/**
 * Dispatch validation to component handler
 * @return true if valid, false if invalid or no handler
 */
bool ir_component_handler_validate(const IRComponent* comp,
                                    char* error_msg, size_t error_size);

/**
 * Dispatch to_string to component handler
 * @return true on success, false if no handler
 */
bool ir_component_handler_to_string(const IRComponent* comp,
                                     char* buffer, size_t size);

// ============================================================================
// Built-in Component Handler Registration
// ============================================================================

/**
 * Register all built-in component handlers
 * Called by ir_component_handler_registry_init()
 */
void ir_component_handlers_register_builtin(void);

// Individual component registration functions
void ir_register_text_handler(void);
void ir_register_button_handler(void);
void ir_register_container_handler(void);
void ir_register_row_handler(void);
void ir_register_column_handler(void);
void ir_register_input_handler(void);
void ir_register_image_handler(void);
void ir_register_canvas_handler(void);
void ir_register_checkbox_handler(void);
void ir_register_dropdown_handler(void);
void ir_register_table_handler(void);
void ir_register_modal_handler(void);
void ir_register_misc_handler(void);
void ir_register_tabs_handler(void);

#endif // IR_COMPONENT_HANDLER_H
