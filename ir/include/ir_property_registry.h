#ifndef IR_PROPERTY_REGISTRY_H
#define IR_PROPERTY_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Property parser function type
 *
 * @param component_id Component ID being parsed
 * @param property_name Property name (e.g., "animation")
 * @param property_value Property value string (e.g., "pulse(2.0, -1)")
 * @param plugin_data Pointer to component's plugin_data field
 * @return true on success, false on parse error
 */
typedef bool (*kryon_property_parser_fn)(
    uint32_t component_id,
    const char* property_name,
    const char* property_value,
    void** plugin_data
);

/**
 * Register a property parser
 *
 * @param name Property name (e.g., "animation", "transition")
 * @param parser Parser function
 * @return true on success, false if already registered or registry full
 */
bool ir_register_property_parser(const char* name, kryon_property_parser_fn parser);

/**
 * Get a property parser by name
 *
 * @param name Property name
 * @return Parser function, or NULL if not found
 */
kryon_property_parser_fn ir_get_property_parser(const char* name);

#endif /* IR_PROPERTY_REGISTRY_H */
