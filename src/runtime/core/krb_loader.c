/**
 * @file krb_loader.c
 * @brief KRB File Loader for Runtime
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * Loads and processes KRB binary files into runtime element trees.
 */

#include "internal/runtime.h"
#include "internal/krb_format.h"
#include "internal/events.h"
#include "internal/memory.h"
#include "internal/binary_io.h"
#include <string.h>
#include <stdlib.h>

// =============================================================================
// ELEMENT TYPE MAPPING
// =============================================================================

static const struct {
    uint16_t hex;
    const char *name;
} element_type_map[] = {
    {0x0001, "App"},
    {0x0002, "Container"},
    {0x0003, "Button"},
    {0x0004, "Text"},
    {0x0005, "Input"},
    {0x0006, "Image"},
    {0x0007, "List"},
    {0x0008, "Grid"},
    {0x0009, "Form"},
    {0x000A, "Modal"},
    {0x000B, "Tabs"},
    {0x000C, "Menu"},
    {0x000D, "Dropdown"},
    {KRYON_ELEMENT_EVENT_DIRECTIVE, "EventDirective"},
    {0, NULL}
};

// =============================================================================
// PROPERTY TYPE MAPPING
// =============================================================================

static const struct {
    uint16_t hex;
    const char *name;
    KryonRuntimePropertyType type;
} property_type_map[] = {
    {0x0001, "text", KRYON_RUNTIME_PROP_STRING},
    {0x0002, "backgroundColor", KRYON_RUNTIME_PROP_COLOR},
    {0x0003, "borderColor", KRYON_RUNTIME_PROP_COLOR},
    {0x0004, "borderWidth", KRYON_RUNTIME_PROP_FLOAT},
    {0x0005, "color", KRYON_RUNTIME_PROP_COLOR},
    {0x0006, "fontSize", KRYON_RUNTIME_PROP_FLOAT},
    {0x0007, "fontWeight", KRYON_RUNTIME_PROP_STRING},
    {0x0008, "fontFamily", KRYON_RUNTIME_PROP_STRING},
    {0x0009, "width", KRYON_RUNTIME_PROP_FLOAT},
    {0x000A, "height", KRYON_RUNTIME_PROP_FLOAT},
    {0x000B, "posX", KRYON_RUNTIME_PROP_FLOAT},
    {0x000C, "posY", KRYON_RUNTIME_PROP_FLOAT},
    {0x000D, "padding", KRYON_RUNTIME_PROP_FLOAT},
    {0x000E, "margin", KRYON_RUNTIME_PROP_FLOAT},
    {0x000F, "display", KRYON_RUNTIME_PROP_STRING},
    {0x0010, "flexDirection", KRYON_RUNTIME_PROP_STRING},
    {0x0011, "alignItems", KRYON_RUNTIME_PROP_STRING},
    {0x0012, "justifyContent", KRYON_RUNTIME_PROP_STRING},
    {0x0013, "textAlignment", KRYON_RUNTIME_PROP_STRING},
    {0x0014, "visible", KRYON_RUNTIME_PROP_BOOLEAN},
    {0x0015, "enabled", KRYON_RUNTIME_PROP_BOOLEAN},
    {0x0016, "class", KRYON_RUNTIME_PROP_STRING},
    {0x0017, "id", KRYON_RUNTIME_PROP_STRING},
    {0x0018, "style", KRYON_RUNTIME_PROP_STRING},
    {0x0019, "onClick", KRYON_RUNTIME_PROP_FUNCTION},
    {0x001A, "onHover", KRYON_RUNTIME_PROP_FUNCTION},
    {0x001B, "onFocus", KRYON_RUNTIME_PROP_FUNCTION},
    {0x001C, "onChange", KRYON_RUNTIME_PROP_FUNCTION},
    {0x001D, "contentAlignment", KRYON_RUNTIME_PROP_STRING},
    {0x001E, "title", KRYON_RUNTIME_PROP_STRING},
    {0x001F, "version", KRYON_RUNTIME_PROP_STRING},
    {0x0020, "selectedIndex", KRYON_RUNTIME_PROP_INTEGER},
    {0x0021, "multiSelect", KRYON_RUNTIME_PROP_BOOLEAN},
    {0x0022, "maxHeight", KRYON_RUNTIME_PROP_FLOAT},
    {0x0023, "borderRadius", KRYON_RUNTIME_PROP_FLOAT},
    {0, NULL, 0}
};

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonElement *load_element_from_binary(KryonRuntime *runtime, 
                                             const uint8_t *data, 
                                             size_t *offset, 
                                             size_t size,
                                             KryonElement *parent);
static bool load_element_properties(KryonElement *element,
                                   const uint8_t *data,
                                   size_t *offset,
                                   size_t size,
                                   uint32_t property_count);
static bool load_property_value(KryonProperty *property,
                               const uint8_t *data,
                               size_t *offset,
                               size_t size);
static void register_event_directive_handlers(KryonRuntime *runtime, KryonElement *element);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static const char *get_element_type_name(uint16_t hex) {
    // Check built-in element types
    for (int i = 0; element_type_map[i].name; i++) {
        if (element_type_map[i].hex == hex) {
            return element_type_map[i].name;
        }
    }
    
    // Handle custom elements (hex >= 0x1000)
    if (hex >= 0x1000) {
        // For now, return a generic custom element name
        // TODO: In the future, custom element names should be stored in the KRB file
        static char custom_name[32];
        snprintf(custom_name, sizeof(custom_name), "CustomElement_0x%04X", hex);
        return custom_name;
    }
    
    return "Unknown";
}

static const char *get_property_name(uint16_t hex) {
    for (int i = 0; property_type_map[i].name; i++) {
        if (property_type_map[i].hex == hex) {
            return property_type_map[i].name;
        }
    }
    return NULL;
}

static KryonRuntimePropertyType get_property_type(uint16_t hex) {
    for (int i = 0; property_type_map[i].name; i++) {
        if (property_type_map[i].hex == hex) {
            return property_type_map[i].type;
        }
    }
    return KRYON_RUNTIME_PROP_STRING; // Default to string
}

static bool read_uint16_safe(const uint8_t *data, size_t *offset, size_t size, uint16_t *value) {
    return kryon_read_uint16(data, offset, size, value);
}

static bool read_uint32_safe(const uint8_t *data, size_t *offset, size_t size, uint32_t *value) {
    return kryon_read_uint32(data, offset, size, value);
}

static bool read_float_safe(const uint8_t *data, size_t *offset, size_t size, float *value) {
    if (*offset + sizeof(float) > size) {
        return false;
    }
    
    // Read as uint32 with endian conversion, then reinterpret as float
    uint32_t float_bits;
    if (!kryon_read_uint32(data, offset, size, &float_bits)) {
        return false;
    }
    
    memcpy(value, &float_bits, sizeof(float));
    return true;
}

static bool read_uint8_safe(const uint8_t *data, size_t *offset, size_t size, uint8_t *value) {
    return kryon_read_uint8(data, offset, size, value);
}

// =============================================================================
// KRB LOADING IMPLEMENTATION
// =============================================================================

bool kryon_runtime_load_krb_data(KryonRuntime *runtime, const uint8_t *data, size_t size) {
    if (!runtime || !data || size < 12) {
        return false;
    }
    
    size_t offset = 0;
    
    // Read and validate header
    uint32_t magic;
    if (!read_uint32_safe(data, &offset, size, &magic)) {
        printf("DEBUG: Failed to read magic number\n");
        return false;
    }
    if (magic != 0x4B52594E) { // "KRYN"
        printf("DEBUG: Invalid magic number: 0x%08X (expected 0x4B52594E)\n", magic);
        return false;
    }
    
    uint32_t version, flags;
    if (!read_uint32_safe(data, &offset, size, &version) ||
        !read_uint32_safe(data, &offset, size, &flags)) {
        printf("DEBUG: Failed to read version/flags\n");
        return false;
    }
    
    // Simple format: header is followed directly by element count
    if (offset + 4 > size) {
        return false;
    }
    
    // Read element count
    uint32_t element_count;
    if (!read_uint32_safe(data, &offset, size, &element_count)) {
        printf("DEBUG: Failed to read element count\n");
        return false;
    }
    printf("DEBUG: Found %u elements in KRB file\n", element_count);
    
    // Load root element (should be the first one)
    if (element_count > 0) {
        runtime->root = load_element_from_binary(runtime, data, &offset, size, NULL);
        if (!runtime->root) {
            printf("DEBUG: Failed to load root element\n");
            return false;
        }
        printf("DEBUG: Successfully loaded root element: %s\n", runtime->root->type_name ? runtime->root->type_name : "unknown");
    } else {
        printf("DEBUG: No elements found in KRB file\n");
    }
    
    return true;
}

static KryonElement *load_element_from_binary(KryonRuntime *runtime, 
                                             const uint8_t *data, 
                                             size_t *offset, 
                                             size_t size,
                                             KryonElement *parent) {
    if (*offset + 10 > size) {
        return NULL;
    }
    
    // Read element header
    uint16_t element_type;
    uint32_t element_id, property_count;
    if (!read_uint16_safe(data, offset, size, &element_type) ||
        !read_uint32_safe(data, offset, size, &element_id) ||
        !read_uint32_safe(data, offset, size, &property_count)) {
        printf("DEBUG: Failed to read element header\n");
        return NULL;
    }
    
    // Create element
    KryonElement *element = kryon_element_create(runtime, element_type, parent);
    if (!element) {
        return NULL;
    }
    
    // Set element type name
    element->type_name = kryon_alloc(strlen(get_element_type_name(element_type)) + 1);
    if (element->type_name) {
        strcpy(element->type_name, get_element_type_name(element_type));
    }
    
    // Load properties
    if (property_count > 0) {
        if (!load_element_properties(element, data, offset, size, property_count)) {
            kryon_element_destroy(runtime, element);
            return NULL;
        }
    }
    
    // Special handling for event directive elements
    if (element_type == KRYON_ELEMENT_EVENT_DIRECTIVE) {
        register_event_directive_handlers(runtime, element);
    }
    
    // Read child count
    if (*offset + 4 > size) {
        kryon_element_destroy(runtime, element);
        return NULL;
    }
    
    uint32_t child_count;
    if (!read_uint32_safe(data, offset, size, &child_count)) {
        printf("DEBUG: Failed to read child count\n");
        kryon_element_destroy(runtime, element);
        return NULL;
    }
    
    if (child_count > 0) {
        printf("DEBUG: Element %s has %u children\n", element->type_name, child_count);
    }
    
    // Load children
    for (uint32_t i = 0; i < child_count; i++) {
        KryonElement *child = load_element_from_binary(runtime, data, offset, size, element);
        if (!child) {
            kryon_element_destroy(runtime, element);
            return NULL;
        }
        printf("DEBUG: Loaded child %u: %s\n", i, child->type_name);
    }
    
    return element;
}

static bool load_element_properties(KryonElement *element,
                                   const uint8_t *data,
                                   size_t *offset,
                                   size_t size,
                                   uint32_t property_count) {
    // Allocate property array
    element->property_capacity = property_count;
    element->properties = kryon_alloc(property_count * sizeof(KryonProperty*));
    if (!element->properties) {
        return false;
    }
    
    for (uint32_t i = 0; i < property_count; i++) {
        if (*offset + 2 > size) {
            return false;
        }
        
        // Read property header
        uint16_t property_id;
        if (!read_uint16_safe(data, offset, size, &property_id)) {
            printf("DEBUG: Failed to read property ID\n");
            return false;
        }
        
        // Create property
        KryonProperty *property = kryon_alloc(sizeof(KryonProperty));
        if (!property) {
            return false;
        }
        
        property->id = property_id;
        property->name = kryon_alloc(strlen(get_property_name(property_id)) + 1);
        if (property->name) {
            strcpy(property->name, get_property_name(property_id));
        }
        property->type = get_property_type(property_id);
        
        printf("DEBUG: Loading property %s (id=0x%04X, type=%d)\n", 
               property->name, property_id, property->type);
        
        // Handle special properties
        if (property_id == 0x0017) { // "id" property
            // Read the property value
            if (!load_property_value(property, data, offset, size)) {
                kryon_free(property->name);
                kryon_free(property);
                return false;
            }
            
            // Set element ID
            if (property->type == KRYON_RUNTIME_PROP_STRING && property->value.string_value) {
                element->element_id = kryon_alloc(strlen(property->value.string_value) + 1);
                if (element->element_id) {
                    strcpy(element->element_id, property->value.string_value);
                }
            }
        } else if (property_id == 0x0016) { // "class" property
            // Read the property value
            if (!load_property_value(property, data, offset, size)) {
                kryon_free(property->name);
                kryon_free(property);
                return false;
            }
            
            // Parse class names
            if (property->type == KRYON_RUNTIME_PROP_STRING && property->value.string_value) {
                // For simplicity, support single class name for now
                element->class_count = 1;
                element->class_names = kryon_alloc(sizeof(char*));
                if (element->class_names) {
                    element->class_names[0] = kryon_alloc(strlen(property->value.string_value) + 1);
                    if (element->class_names[0]) {
                        strcpy(element->class_names[0], property->value.string_value);
                    }
                }
            }
        } else {
            // Read property value
            if (!load_property_value(property, data, offset, size)) {
                kryon_free(property->name);
                kryon_free(property);
                return false;
            }
        }
        
        element->properties[element->property_count++] = property;
    }
    
    return true;
}

static bool load_property_value(KryonProperty *property,
                               const uint8_t *data,
                               size_t *offset,
                               size_t size) {
    if (*offset + 1 > size) {
        return false;
    }
    
    // Read value type
    uint8_t value_type;
    if (!read_uint8_safe(data, offset, size, &value_type)) {
        printf("DEBUG: Failed to read value type\n");
        return false;
    }
    
    switch (value_type) {
        case 0: // String (inline format: length + data)
            {
                uint16_t string_length;
                if (!read_uint16_safe(data, offset, size, &string_length)) {
                    printf("DEBUG: Failed to read string length\n");
                    return false;
                }
                
                if (*offset + string_length > size) {
                    printf("DEBUG: String data exceeds buffer size\n");
                    return false;
                }
                
                // Allocate and copy string data
                property->value.string_value = kryon_alloc(string_length + 1);
                if (!property->value.string_value) {
                    printf("DEBUG: Failed to allocate string memory\n");
                    return false;
                }
                
                if (string_length > 0) {
                    memcpy(property->value.string_value, data + *offset, string_length);
                }
                property->value.string_value[string_length] = '\0';
                *offset += string_length;
                
                printf("DEBUG: Read inline string: '%s'\n", property->value.string_value);
            }
            break;
            
        case 1: // Integer (stored as uint32)
            {
                uint32_t int_value;
                if (!read_uint32_safe(data, offset, size, &int_value)) {
                    printf("DEBUG: Failed to read integer value\n");
                    return false;
                }
                property->value.int_value = (int64_t)int_value;
            }
            break;
            
        case 2: // Float (stored as 32-bit float)
            {
                float float_value;
                if (!read_float_safe(data, offset, size, &float_value)) {
                    printf("DEBUG: Failed to read float value\n"); 
                    return false;
                }
                property->value.float_value = (double)float_value;
                printf("DEBUG: Read float property '%s' = %f\n", property->name, float_value);
            }
            break;
            
        case 3: // Boolean (stored as uint8)
            {
                uint8_t bool_value;
                if (!read_uint8_safe(data, offset, size, &bool_value)) {
                    printf("DEBUG: Failed to read boolean value\n");
                    return false;
                }
                property->value.bool_value = bool_value != 0;
            }
            break;
            
        case 4: // Color (stored as uint32 RGBA)
            {
                uint32_t color_value;
                if (!read_uint32_safe(data, offset, size, &color_value)) {
                    printf("DEBUG: Failed to read color value\n");
                    return false;
                }
                property->value.color_value = color_value;
            }
            break;
            
        default:
            return false;
    }
    
    return true;
}

// =============================================================================
// PROPERTY ACCESS
// =============================================================================

bool kryon_element_set_property(KryonElement *element, uint16_t property_id, const void *value) {
    if (!element || !value) {
        return false;
    }
    
    // Find existing property
    for (size_t i = 0; i < element->property_count; i++) {
        if (element->properties[i]->id == property_id) {
            KryonProperty *prop = element->properties[i];
            
            // Update value based on type
            switch (prop->type) {
                case KRYON_RUNTIME_PROP_STRING:
                    kryon_free(prop->value.string_value);
                    prop->value.string_value = kryon_alloc(strlen((const char*)value) + 1);
                    if (prop->value.string_value) {
                        strcpy(prop->value.string_value, (const char*)value);
                    }
                    break;
                    
                case KRYON_RUNTIME_PROP_INTEGER:
                    prop->value.int_value = *(int64_t*)value;
                    break;
                    
                case KRYON_RUNTIME_PROP_FLOAT:
                    prop->value.float_value = *(double*)value;
                    break;
                    
                case KRYON_RUNTIME_PROP_BOOLEAN:
                    prop->value.bool_value = *(bool*)value;
                    break;
                    
                case KRYON_RUNTIME_PROP_COLOR:
                    prop->value.color_value = *(uint32_t*)value;
                    break;
                    
                default:
                    return false;
            }
            
            // Mark element for update
            kryon_element_invalidate_render(element);
            return true;
        }
    }
    
    // Property not found, add new one
    if (element->property_count >= element->property_capacity) {
        size_t new_capacity = element->property_capacity ? element->property_capacity * 2 : 4;
        KryonProperty **new_properties = kryon_realloc(element->properties,
                                                      new_capacity * sizeof(KryonProperty*));
        if (!new_properties) {
            return false;
        }
        element->properties = new_properties;
        element->property_capacity = new_capacity;
    }
    
    // Create new property
    KryonProperty *prop = kryon_alloc(sizeof(KryonProperty));
    if (!prop) {
        return false;
    }
    
    prop->id = property_id;
    prop->name = kryon_alloc(strlen(get_property_name(property_id)) + 1);
    if (prop->name) {
        strcpy(prop->name, get_property_name(property_id));
    }
    prop->type = get_property_type(property_id);
    
    // Set value based on type
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_STRING:
            prop->value.string_value = kryon_alloc(strlen((const char*)value) + 1);
            if (prop->value.string_value) {
                strcpy(prop->value.string_value, (const char*)value);
            }
            break;
            
        case KRYON_RUNTIME_PROP_INTEGER:
            prop->value.int_value = *(int64_t*)value;
            break;
            
        case KRYON_RUNTIME_PROP_FLOAT:
            prop->value.float_value = *(double*)value;
            break;
            
        case KRYON_RUNTIME_PROP_BOOLEAN:
            prop->value.bool_value = *(bool*)value;
            break;
            
        case KRYON_RUNTIME_PROP_COLOR:
            prop->value.color_value = *(uint32_t*)value;
            break;
            
        default:
            kryon_free(prop->name);
            kryon_free(prop);
            return false;
    }
    
    element->properties[element->property_count++] = prop;
    kryon_element_invalidate_render(element);
    
    return true;
}

const void *kryon_element_get_property(const KryonElement *element, uint16_t property_id) {
    if (!element) {
        return NULL;
    }
    
    for (size_t i = 0; i < element->property_count; i++) {
        if (element->properties[i]->id == property_id) {
            KryonProperty *prop = element->properties[i];
            
            switch (prop->type) {
                case KRYON_RUNTIME_PROP_STRING:
                    return prop->value.string_value;
                case KRYON_RUNTIME_PROP_INTEGER:
                    return &prop->value.int_value;
                case KRYON_RUNTIME_PROP_FLOAT:
                    return &prop->value.float_value;
                case KRYON_RUNTIME_PROP_BOOLEAN:
                    return &prop->value.bool_value;
                case KRYON_RUNTIME_PROP_COLOR:
                    return &prop->value.color_value;
                default:
                    return NULL;
            }
        }
    }
    
    return NULL;
}

// =============================================================================
// EVENT DIRECTIVE HANDLING
// =============================================================================

/**
 * Register event handlers for event directive elements
 */
static void register_event_directive_handlers(KryonRuntime *runtime, KryonElement *element) {
    if (!runtime || !element || !runtime->event_system) {
        return;
    }
    
    printf("DEBUG: Registering event handlers for event directive\n");
    
    // Process each property as an event handler
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty *prop = &element->properties[i];
        if (!prop->name || !prop->value.string_value) {
            continue;
        }
        
        printf("DEBUG: Found event handler property: %s -> %s\n", 
               prop->name, prop->value.string_value);
        
        // Determine event type from property name
        if (strncmp(prop->name, "keyboard:", 9) == 0) {
            // Keyboard event: "keyboard:Ctrl+I" -> "Ctrl+I"
            const char *shortcut = prop->name + 9;
            const char *handler_name = prop->value.string_value;
            
            // TODO: Look up actual handler function from script system
            // For now, just register a placeholder handler
            printf("DEBUG: Would register keyboard handler for '%s' -> '%s'\n", 
                   shortcut, handler_name);
            
            // kryon_event_register_keyboard_handler(runtime->event_system, 
            //                                       shortcut, 
            //                                       script_handler, 
            //                                       handler_name);
            
        } else if (strncmp(prop->name, "mouse:", 6) == 0) {
            // Mouse event: "mouse:LeftClick" -> "LeftClick"
            const char *mouse_event = prop->name + 6;
            const char *handler_name = prop->value.string_value;
            
            printf("DEBUG: Would register mouse handler for '%s' -> '%s'\n", 
                   mouse_event, handler_name);
            
            // kryon_event_register_mouse_handler(runtime->event_system, 
            //                                    mouse_event, 
            //                                    script_handler, 
            //                                    handler_name);
        }
    }
}