/**
 * @file element_system.c
 * @brief Implementation of Flutter-inspired element system
 */

#include "elements.h"
#include "memory.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// CONSTANTS
// =============================================================================

#define KRYON_ELEMENT_INITIAL_CHILDREN_CAPACITY 4
#define KRYON_ELEMENT_REGISTRY_INITIAL_MAP_SIZE 64
#define KRYON_ELEMENT_AUTO_ID_PREFIX "element_"

// =============================================================================
// PRIVATE FUNCTIONS
// =============================================================================

/// Simple hash function for element IDs
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/// Generate auto ID for elements without explicit ID
static char* generate_auto_id(KryonElementRegistry* registry) {
    (void)registry; // Suppress unused parameter warning
    static uint32_t counter = 0;
    char* id = kryon_malloc(64);
    snprintf(id, 64, "%s%u", KRYON_ELEMENT_AUTO_ID_PREFIX, ++counter);
    return id;
}

/// Resize element registry capacity
static bool resize_registry(KryonElementRegistry* registry, size_t new_capacity) {
    KryonElement** new_elements = kryon_realloc(registry->elements, 
                                             new_capacity * sizeof(KryonElement*));
    if (!new_elements) {
        return false;
    }
    
    registry->elements = new_elements;
    registry->capacity = new_capacity;
    return true;
}

/// Resize ID map capacity
static bool resize_id_map(KryonElementRegistry* registry, size_t new_capacity) {
    KryonElementMapEntry* new_map = kryon_calloc(new_capacity, sizeof(KryonElementMapEntry));
    if (!new_map) {
        return false;
    }
    
    // Rehash existing entries
    KryonElementMapEntry* old_map = registry->id_map;
    size_t old_capacity = registry->map_capacity;
    
    registry->id_map = new_map;
    registry->map_capacity = new_capacity;
    registry->map_size = 0;
    
    // Re-insert all entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_map[i].key) {
            uint32_t hash = hash_string(old_map[i].key);
            size_t index = hash % new_capacity;
            
            // Linear probing for collision resolution
            while (registry->id_map[index].key) {
                index = (index + 1) % new_capacity;
            }
            
            registry->id_map[index].key = old_map[i].key;
            registry->id_map[index].element = old_map[i].element;
            registry->map_size++;
        }
    }
    
    kryon_free(old_map);
    return true;
}

/// Add element to ID map
static bool add_to_id_map(KryonElementRegistry* registry, const char* id, KryonElement* element) {
    // Resize if load factor > 0.7
    if (registry->map_size >= registry->map_capacity * 0.7) {
        if (!resize_id_map(registry, registry->map_capacity * 2)) {
            return false;
        }
    }
    
    uint32_t hash = hash_string(id);
    size_t index = hash % registry->map_capacity;
    
    // Linear probing for collision resolution
    while (registry->id_map[index].key) {
        if (strcmp(registry->id_map[index].key, id) == 0) {
            // ID already exists - update entry
            registry->id_map[index].element = element;
            return true;
        }
        index = (index + 1) % registry->map_capacity;
    }
    
    // Insert new entry
    registry->id_map[index].key = kryon_strdup(id);
    registry->id_map[index].element = element;
    registry->map_size++;
    
    return true;
}

/// Remove element from ID map
static void __attribute__((unused)) remove_from_id_map(KryonElementRegistry* registry, const char* id) {
    uint32_t hash = hash_string(id);
    size_t index = hash % registry->map_capacity;
    
    // Find entry
    while (registry->id_map[index].key) {
        if (strcmp(registry->id_map[index].key, id) == 0) {
            kryon_free(registry->id_map[index].key);
            registry->id_map[index].key = NULL;
            registry->id_map[index].element = NULL;
            registry->map_size--;
            break;
        }
        index = (index + 1) % registry->map_capacity;
    }
}

/// Initialize element properties based on type
static void init_element_properties(KryonElement* element) {
    memset(&element->props, 0, sizeof(element->props));
    
    switch (element->type) {
        case KRYON_ELEMENT_TEXT:
            element->props.text_props.style.font_size = 16.0f;
            element->props.text_props.style.color = (KryonColor){0, 0, 0, 255};
            break;
            
        case KRYON_ELEMENT_BUTTON:
            element->props.button_props.background_color = (KryonColor){0, 123, 255, 255};
            element->props.button_props.text_color = (KryonColor){255, 255, 255, 255};
            break;
            
        case KRYON_ELEMENT_CONTAINER:
            element->props.container_props.background_color = (KryonColor){0, 0, 0, 0};
            break;
            
        case KRYON_ELEMENT_INPUT:
            element->props.input_props.border.width = 1.0f;
            element->props.input_props.border.color = (KryonColor){206, 212, 218, 255};
            break;
            
        default:
            break;
    }
}

/// Destroy element properties based on type
static void destroy_element_properties(KryonElement* element) {
    switch (element->type) {
        case KRYON_ELEMENT_TEXT:
            kryon_free(element->props.text_props.text);
            break;
            
        case KRYON_ELEMENT_BUTTON:
            kryon_free(element->props.button_props.label);
            kryon_free(element->props.button_props.on_click_script);
            break;
            
        case KRYON_ELEMENT_INPUT:
            kryon_free(element->props.input_props.value);
            kryon_free(element->props.input_props.placeholder);
            break;
            
        case KRYON_ELEMENT_IMAGE:
            kryon_free(element->props.image_props.src);
            break;
            
        default:
            break;
    }

    if (element->script_state) {
        kryon_free(element->script_state);
        element->script_state = NULL;
    }
}

// =============================================================================
// ELEMENT REGISTRY API
// =============================================================================

KryonElementRegistry* kryon_element_registry_create(size_t initial_capacity) {
    KryonElementRegistry* registry = kryon_malloc(sizeof(KryonElementRegistry));
    if (!registry) {
        return NULL;
    }
    
    registry->elements = kryon_malloc(initial_capacity * sizeof(KryonElement*));
    if (!registry->elements) {
        kryon_free(registry);
        return NULL;
    }
    
    registry->id_map = kryon_calloc(KRYON_ELEMENT_REGISTRY_INITIAL_MAP_SIZE, 
                                   sizeof(KryonElementMapEntry));
    if (!registry->id_map) {
        kryon_free(registry->elements);
        kryon_free(registry);
        return NULL;
    }
    
    registry->count = 0;
    registry->capacity = initial_capacity;
    registry->map_size = 0;
    registry->map_capacity = KRYON_ELEMENT_REGISTRY_INITIAL_MAP_SIZE;
    
    return registry;
}

void kryon_element_registry_destroy(KryonElementRegistry* registry) {
    if (!registry) return;
    
    // Release all elements that are still in the registry
    for (size_t i = 0; i < registry->count; i++) {
        if (registry->elements[i]) {
            kryon_element_release(registry->elements[i]);
        }
    }
    
    // Free ID map
    for (size_t i = 0; i < registry->map_capacity; i++) {
        if (registry->id_map[i].key) {
            kryon_free(registry->id_map[i].key);
        }
    }
    
    kryon_free(registry->elements);
    kryon_free(registry->id_map);
    kryon_free(registry);
}

// =============================================================================
// ELEMENT LIFECYCLE API
// =============================================================================

KryonElement* kryon_element_create(KryonElementRegistry* registry, 
                                KryonElementType type, 
                                const char* id) {
    if (!registry) return NULL;
    
    // Resize registry if needed
    if (registry->count >= registry->capacity) {
        if (!resize_registry(registry, registry->capacity * 2)) {
            return NULL;
        }
    }
    
    // Allocate element
    KryonElement* element = kryon_malloc(sizeof(KryonElement));
    if (!element) {
        return NULL;
    }
    
    // Initialize basic properties
    memset(element, 0, sizeof(KryonElement));
    element->type = type;
    element->transform_state = KRYON_TRANSFORM_NONE;
    element->ref_count = 1;
    
    // Set ID
    if (id) {
        element->id = kryon_strdup(id);
    } else {
        element->id = generate_auto_id(registry);
    }
    element->hash = hash_string(element->id);
    
    // Initialize layout properties with sensible defaults
    element->main_axis = KRYON_MAIN_START;
    element->cross_axis = KRYON_CROSS_START;
    element->spacing = 0.0f;
    element->flex = 0;
    element->visible = true;
    
    // Initialize content alignment properties
    element->content_alignment = KRYON_CONTENT_TOP_LEFT;
    element->content_alignment_x = KRYON_ALIGN_LEFT;
    element->content_alignment_y = KRYON_ALIGN_TOP;
    element->content_distribution = KRYON_DISTRIBUTE_NONE;
    
    // Initialize size constraints with expressions
    element->width_expr = kryon_expression_literal(-1.0f);   // Auto size
    element->height_expr = kryon_expression_literal(-1.0f);  // Auto size
    element->posX_expr = kryon_expression_literal(0.0f);     // Default position
    element->posY_expr = kryon_expression_literal(0.0f);     // Default position
    
    // Initialize cached computed values
    element->width = -1.0f;  // Auto size
    element->height = -1.0f; // Auto size
    element->min_width = 0.0f;
    element->min_height = 0.0f;
    element->max_width = HUGE_VAL;
    element->max_height = HUGE_VAL;
    
    // Initialize children array for layout elements
    if (kryon_element_is_layout_type(type)) {
        element->children = kryon_malloc(KRYON_ELEMENT_INITIAL_CHILDREN_CAPACITY * sizeof(KryonElement*));
        if (!element->children) {
            kryon_free(element->id);
            kryon_free(element);
            return NULL;
        }
        element->child_capacity = KRYON_ELEMENT_INITIAL_CHILDREN_CAPACITY;
    }
    
    // Initialize type-specific properties
    init_element_properties(element);
    
    // Add to registry
    registry->elements[registry->count++] = element;
    
    // Add to ID map
    if (!add_to_id_map(registry, element->id, element)) {
        // Non-fatal error - element still created but not in map
    }
    
    return element;
}

KryonElement* kryon_element_find_by_id(KryonElementRegistry* registry, const char* id) {
    if (!registry || !id) return NULL;
    
    uint32_t hash = hash_string(id);
    size_t index = hash % registry->map_capacity;
    
    // Linear probing search
    while (registry->id_map[index].key) {
        if (strcmp(registry->id_map[index].key, id) == 0) {
            return registry->id_map[index].element;
        }
        index = (index + 1) % registry->map_capacity;
    }
    
    return NULL;
}

KryonElement* kryon_element_retain(KryonElement* element) {
    if (element) {
        element->ref_count++;
    }
    return element;
}

void kryon_element_release(KryonElement* element) {
    if (!element) return;
    
    element->ref_count--;
    if (element->ref_count <= 0) {
        // Recursively release children
        for (size_t i = 0; i < element->child_count; i++) {
            kryon_element_release(element->children[i]);
        }
        
        // Destroy properties
        destroy_element_properties(element);
        
        // Free memory
        kryon_free(element->id);
        kryon_free(element->children);
        kryon_free(element);
    }
}

// =============================================================================
// ELEMENT HIERARCHY API
// =============================================================================

bool kryon_element_add_child(KryonElement* parent, KryonElement* child) {
    if (!parent || !child) return false;
    
    // Only layout elements can have children
    if (!kryon_element_is_layout_type(parent->type)) {
        return false;
    }
    
    // Resize children array if needed
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity * 2;
        KryonElement** new_children = kryon_realloc(parent->children, 
                                                  new_capacity * sizeof(KryonElement*));
        if (!new_children) {
            return false;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    // Add child
    parent->children[parent->child_count++] = kryon_element_retain(child);
    child->parent = parent;
    
    // Mark layout as dirty
    kryon_element_mark_dirty(parent);
    
    return true;
}

bool kryon_element_remove_child(KryonElement* parent, KryonElement* child) {
    if (!parent || !child) return false;
    
    // Find child index
    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            // Remove child and shift remaining children
            kryon_element_release(child);
            child->parent = NULL;
            
            for (size_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            
            // Mark layout as dirty
            kryon_element_mark_dirty(parent);
            
            return true;
        }
    }
    
    return false;
}

void kryon_element_clear_children(KryonElement* parent) {
    if (!parent) return;
    
    for (size_t i = 0; i < parent->child_count; i++) {
        parent->children[i]->parent = NULL;
        kryon_element_release(parent->children[i]);
    }
    
    parent->child_count = 0;
    kryon_element_mark_dirty(parent);
}

KryonElement* kryon_element_get_child(KryonElement* parent, size_t index) {
    if (!parent || index >= parent->child_count) {
        return NULL;
    }
    return parent->children[index];
}

// =============================================================================
// LAYOUT MANAGEMENT
// =============================================================================

void kryon_element_mark_dirty(KryonElement* element) {
    if (!element) return;
    
    element->layout_dirty = true;
    
    // Propagate dirty flag up to root
    KryonElement* current = element->parent;
    while (current) {
        if (current->layout_dirty) {
            break; // Already marked, no need to continue
        }
        current->layout_dirty = true;
        current = current->parent;
    }
}

void kryon_layout_invalidate_from(KryonElement* element) {
    kryon_element_mark_dirty(element);
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

bool kryon_element_is_layout_type(KryonElementType type) {
    switch (type) {
        case KRYON_ELEMENT_COLUMN:
        case KRYON_ELEMENT_ROW:
        case KRYON_ELEMENT_STACK:
        case KRYON_ELEMENT_CONTAINER:
        case KRYON_ELEMENT_EXPANDED:
        case KRYON_ELEMENT_FLEXIBLE:
            return true;
        default:
            return false;
    }
}

const char* kryon_element_type_to_string(KryonElementType type) {
    switch (type) {
        case KRYON_ELEMENT_COLUMN: return "column";
        case KRYON_ELEMENT_ROW: return "row";
        case KRYON_ELEMENT_STACK: return "stack";
        case KRYON_ELEMENT_CONTAINER: return "container";
        case KRYON_ELEMENT_EXPANDED: return "expanded";
        case KRYON_ELEMENT_FLEXIBLE: return "flexible";
        case KRYON_ELEMENT_TEXT: return "text";
        case KRYON_ELEMENT_BUTTON: return "button";
        case KRYON_ELEMENT_INPUT: return "input";
        case KRYON_ELEMENT_IMAGE: return "image";
        case KRYON_ELEMENT_TEXTAREA: return "textarea";
        case KRYON_ELEMENT_CUSTOM: return "custom";
        default: return "unknown";
    }
}

KryonElementType kryon_element_type_from_string(const char* type_str) {
    if (!type_str) return KRYON_ELEMENT_CUSTOM;
    
    if (strcmp(type_str, "column") == 0) return KRYON_ELEMENT_COLUMN;
    if (strcmp(type_str, "row") == 0) return KRYON_ELEMENT_ROW;
    if (strcmp(type_str, "stack") == 0) return KRYON_ELEMENT_STACK;
    if (strcmp(type_str, "container") == 0) return KRYON_ELEMENT_CONTAINER;
    if (strcmp(type_str, "expanded") == 0) return KRYON_ELEMENT_EXPANDED;
    if (strcmp(type_str, "flexible") == 0) return KRYON_ELEMENT_FLEXIBLE;
    if (strcmp(type_str, "text") == 0) return KRYON_ELEMENT_TEXT;
    if (strcmp(type_str, "button") == 0) return KRYON_ELEMENT_BUTTON;
    if (strcmp(type_str, "input") == 0) return KRYON_ELEMENT_INPUT;
    if (strcmp(type_str, "image") == 0) return KRYON_ELEMENT_IMAGE;
    if (strcmp(type_str, "textarea") == 0) return KRYON_ELEMENT_TEXTAREA;
    
    return KRYON_ELEMENT_CUSTOM;
}

KryonEdgeInsets kryon_edge_insets_all(float value) {
    return (KryonEdgeInsets){value, value, value, value};
}

KryonEdgeInsets kryon_edge_insets_trbl(float top, float right, float bottom, float left) {
    return (KryonEdgeInsets){top, right, bottom, left};
}