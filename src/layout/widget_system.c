/**
 * @file widget_system.c
 * @brief Implementation of Flutter-inspired widget system
 */

#include "kryon/widget_system.h"
#include "internal/memory.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// CONSTANTS
// =============================================================================

#define KRYON_WIDGET_INITIAL_CHILDREN_CAPACITY 4
#define KRYON_WIDGET_REGISTRY_INITIAL_MAP_SIZE 64
#define KRYON_WIDGET_AUTO_ID_PREFIX "widget_"

// =============================================================================
// PRIVATE FUNCTIONS
// =============================================================================

/// Simple hash function for widget IDs
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/// Generate auto ID for widgets without explicit ID
static char* generate_auto_id(KryonWidgetRegistry* registry) {
    (void)registry; // Suppress unused parameter warning
    static uint32_t counter = 0;
    char* id = kryon_malloc(64);
    snprintf(id, 64, "%s%u", KRYON_WIDGET_AUTO_ID_PREFIX, ++counter);
    return id;
}

/// Resize widget registry capacity
static bool resize_registry(KryonWidgetRegistry* registry, size_t new_capacity) {
    KryonWidget** new_widgets = kryon_realloc(registry->widgets, 
                                             new_capacity * sizeof(KryonWidget*));
    if (!new_widgets) {
        return false;
    }
    
    registry->widgets = new_widgets;
    registry->capacity = new_capacity;
    return true;
}

/// Resize ID map capacity
static bool resize_id_map(KryonWidgetRegistry* registry, size_t new_capacity) {
    KryonWidgetMapEntry* new_map = kryon_calloc(new_capacity, sizeof(KryonWidgetMapEntry));
    if (!new_map) {
        return false;
    }
    
    // Rehash existing entries
    KryonWidgetMapEntry* old_map = registry->id_map;
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
            registry->id_map[index].widget = old_map[i].widget;
            registry->map_size++;
        }
    }
    
    kryon_free(old_map);
    return true;
}

/// Add widget to ID map
static bool add_to_id_map(KryonWidgetRegistry* registry, const char* id, KryonWidget* widget) {
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
            registry->id_map[index].widget = widget;
            return true;
        }
        index = (index + 1) % registry->map_capacity;
    }
    
    // Insert new entry
    registry->id_map[index].key = kryon_strdup(id);
    registry->id_map[index].widget = widget;
    registry->map_size++;
    
    return true;
}

/// Remove widget from ID map
static void __attribute__((unused)) remove_from_id_map(KryonWidgetRegistry* registry, const char* id) {
    uint32_t hash = hash_string(id);
    size_t index = hash % registry->map_capacity;
    
    // Find entry
    while (registry->id_map[index].key) {
        if (strcmp(registry->id_map[index].key, id) == 0) {
            kryon_free(registry->id_map[index].key);
            registry->id_map[index].key = NULL;
            registry->id_map[index].widget = NULL;
            registry->map_size--;
            break;
        }
        index = (index + 1) % registry->map_capacity;
    }
}

/// Initialize widget properties based on type
static void init_widget_properties(KryonWidget* widget) {
    memset(&widget->props, 0, sizeof(widget->props));
    
    switch (widget->type) {
        case KRYON_WIDGET_TEXT:
            widget->props.text_props.style.font_size = 16.0f;
            widget->props.text_props.style.color = (KryonColor){0, 0, 0, 255};
            break;
            
        case KRYON_WIDGET_BUTTON:
            widget->props.button_props.background_color = (KryonColor){0, 123, 255, 255};
            widget->props.button_props.text_color = (KryonColor){255, 255, 255, 255};
            break;
            
        case KRYON_WIDGET_CONTAINER:
            widget->props.container_props.background_color = (KryonColor){0, 0, 0, 0};
            break;
            
        case KRYON_WIDGET_INPUT:
            widget->props.input_props.border.width = 1.0f;
            widget->props.input_props.border.color = (KryonColor){206, 212, 218, 255};
            break;
            
        default:
            break;
    }
}

/// Destroy widget properties based on type
static void destroy_widget_properties(KryonWidget* widget) {
    switch (widget->type) {
        case KRYON_WIDGET_TEXT:
            kryon_free(widget->props.text_props.text);
            break;
            
        case KRYON_WIDGET_BUTTON:
            kryon_free(widget->props.button_props.label);
            kryon_free(widget->props.button_props.on_click_script);
            break;
            
        case KRYON_WIDGET_INPUT:
            kryon_free(widget->props.input_props.value);
            kryon_free(widget->props.input_props.placeholder);
            break;
            
        case KRYON_WIDGET_IMAGE:
            kryon_free(widget->props.image_props.src);
            break;
            
        default:
            break;
    }
}

// =============================================================================
// WIDGET REGISTRY API
// =============================================================================

KryonWidgetRegistry* kryon_widget_registry_create(size_t initial_capacity) {
    KryonWidgetRegistry* registry = kryon_malloc(sizeof(KryonWidgetRegistry));
    if (!registry) {
        return NULL;
    }
    
    registry->widgets = kryon_malloc(initial_capacity * sizeof(KryonWidget*));
    if (!registry->widgets) {
        kryon_free(registry);
        return NULL;
    }
    
    registry->id_map = kryon_calloc(KRYON_WIDGET_REGISTRY_INITIAL_MAP_SIZE, 
                                   sizeof(KryonWidgetMapEntry));
    if (!registry->id_map) {
        kryon_free(registry->widgets);
        kryon_free(registry);
        return NULL;
    }
    
    registry->count = 0;
    registry->capacity = initial_capacity;
    registry->map_size = 0;
    registry->map_capacity = KRYON_WIDGET_REGISTRY_INITIAL_MAP_SIZE;
    
    return registry;
}

void kryon_widget_registry_destroy(KryonWidgetRegistry* registry) {
    if (!registry) return;
    
    // Release all widgets that are still in the registry
    for (size_t i = 0; i < registry->count; i++) {
        if (registry->widgets[i]) {
            kryon_widget_release(registry->widgets[i]);
        }
    }
    
    // Free ID map
    for (size_t i = 0; i < registry->map_capacity; i++) {
        if (registry->id_map[i].key) {
            kryon_free(registry->id_map[i].key);
        }
    }
    
    kryon_free(registry->widgets);
    kryon_free(registry->id_map);
    kryon_free(registry);
}

// =============================================================================
// WIDGET LIFECYCLE API
// =============================================================================

KryonWidget* kryon_widget_create(KryonWidgetRegistry* registry, 
                                KryonWidgetType type, 
                                const char* id) {
    if (!registry) return NULL;
    
    // Resize registry if needed
    if (registry->count >= registry->capacity) {
        if (!resize_registry(registry, registry->capacity * 2)) {
            return NULL;
        }
    }
    
    // Allocate widget
    KryonWidget* widget = kryon_malloc(sizeof(KryonWidget));
    if (!widget) {
        return NULL;
    }
    
    // Initialize basic properties
    memset(widget, 0, sizeof(KryonWidget));
    widget->type = type;
    widget->transform_state = KRYON_TRANSFORM_NONE;
    widget->ref_count = 1;
    
    // Set ID
    if (id) {
        widget->id = kryon_strdup(id);
    } else {
        widget->id = generate_auto_id(registry);
    }
    widget->hash = hash_string(widget->id);
    
    // Initialize layout properties with sensible defaults
    widget->main_axis = KRYON_MAIN_START;
    widget->cross_axis = KRYON_CROSS_START;
    widget->spacing = 0.0f;
    widget->flex = 0;
    widget->visible = true;
    
    // Initialize content alignment properties
    widget->content_alignment = KRYON_CONTENT_TOP_LEFT;
    widget->content_alignment_x = KRYON_ALIGN_LEFT;
    widget->content_alignment_y = KRYON_ALIGN_TOP;
    widget->content_distribution = KRYON_DISTRIBUTE_NONE;
    
    // Initialize size constraints with expressions
    widget->width_expr = kryon_expression_literal(-1.0f);   // Auto size
    widget->height_expr = kryon_expression_literal(-1.0f);  // Auto size
    widget->posX_expr = kryon_expression_literal(0.0f);     // Default position
    widget->posY_expr = kryon_expression_literal(0.0f);     // Default position
    
    // Initialize cached computed values
    widget->width = -1.0f;  // Auto size
    widget->height = -1.0f; // Auto size
    widget->min_width = 0.0f;
    widget->min_height = 0.0f;
    widget->max_width = HUGE_VAL;
    widget->max_height = HUGE_VAL;
    
    // Initialize children array for layout widgets
    if (kryon_widget_is_layout_type(type)) {
        widget->children = kryon_malloc(KRYON_WIDGET_INITIAL_CHILDREN_CAPACITY * sizeof(KryonWidget*));
        if (!widget->children) {
            kryon_free(widget->id);
            kryon_free(widget);
            return NULL;
        }
        widget->child_capacity = KRYON_WIDGET_INITIAL_CHILDREN_CAPACITY;
    }
    
    // Initialize type-specific properties
    init_widget_properties(widget);
    
    // Add to registry
    registry->widgets[registry->count++] = widget;
    
    // Add to ID map
    if (!add_to_id_map(registry, widget->id, widget)) {
        // Non-fatal error - widget still created but not in map
    }
    
    return widget;
}

KryonWidget* kryon_widget_find_by_id(KryonWidgetRegistry* registry, const char* id) {
    if (!registry || !id) return NULL;
    
    uint32_t hash = hash_string(id);
    size_t index = hash % registry->map_capacity;
    
    // Linear probing search
    while (registry->id_map[index].key) {
        if (strcmp(registry->id_map[index].key, id) == 0) {
            return registry->id_map[index].widget;
        }
        index = (index + 1) % registry->map_capacity;
    }
    
    return NULL;
}

KryonWidget* kryon_widget_retain(KryonWidget* widget) {
    if (widget) {
        widget->ref_count++;
    }
    return widget;
}

void kryon_widget_release(KryonWidget* widget) {
    if (!widget) return;
    
    widget->ref_count--;
    if (widget->ref_count <= 0) {
        // Recursively release children
        for (size_t i = 0; i < widget->child_count; i++) {
            kryon_widget_release(widget->children[i]);
        }
        
        // Destroy properties
        destroy_widget_properties(widget);
        
        // Free memory
        kryon_free(widget->id);
        kryon_free(widget->children);
        kryon_free(widget);
    }
}

// =============================================================================
// WIDGET HIERARCHY API
// =============================================================================

bool kryon_widget_add_child(KryonWidget* parent, KryonWidget* child) {
    if (!parent || !child) return false;
    
    // Only layout widgets can have children
    if (!kryon_widget_is_layout_type(parent->type)) {
        return false;
    }
    
    // Resize children array if needed
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity * 2;
        KryonWidget** new_children = kryon_realloc(parent->children, 
                                                  new_capacity * sizeof(KryonWidget*));
        if (!new_children) {
            return false;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    // Add child
    parent->children[parent->child_count++] = kryon_widget_retain(child);
    child->parent = parent;
    
    // Mark layout as dirty
    kryon_widget_mark_dirty(parent);
    
    return true;
}

bool kryon_widget_remove_child(KryonWidget* parent, KryonWidget* child) {
    if (!parent || !child) return false;
    
    // Find child index
    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            // Remove child and shift remaining children
            kryon_widget_release(child);
            child->parent = NULL;
            
            for (size_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            
            // Mark layout as dirty
            kryon_widget_mark_dirty(parent);
            
            return true;
        }
    }
    
    return false;
}

void kryon_widget_clear_children(KryonWidget* parent) {
    if (!parent) return;
    
    for (size_t i = 0; i < parent->child_count; i++) {
        parent->children[i]->parent = NULL;
        kryon_widget_release(parent->children[i]);
    }
    
    parent->child_count = 0;
    kryon_widget_mark_dirty(parent);
}

KryonWidget* kryon_widget_get_child(KryonWidget* parent, size_t index) {
    if (!parent || index >= parent->child_count) {
        return NULL;
    }
    return parent->children[index];
}

// =============================================================================
// LAYOUT MANAGEMENT
// =============================================================================

void kryon_widget_mark_dirty(KryonWidget* widget) {
    if (!widget) return;
    
    widget->layout_dirty = true;
    
    // Propagate dirty flag up to root
    KryonWidget* current = widget->parent;
    while (current) {
        if (current->layout_dirty) {
            break; // Already marked, no need to continue
        }
        current->layout_dirty = true;
        current = current->parent;
    }
}

void kryon_layout_invalidate_from(KryonWidget* widget) {
    kryon_widget_mark_dirty(widget);
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

bool kryon_widget_is_layout_type(KryonWidgetType type) {
    switch (type) {
        case KRYON_WIDGET_COLUMN:
        case KRYON_WIDGET_ROW:
        case KRYON_WIDGET_STACK:
        case KRYON_WIDGET_CONTAINER:
        case KRYON_WIDGET_EXPANDED:
        case KRYON_WIDGET_FLEXIBLE:
            return true;
        default:
            return false;
    }
}

const char* kryon_widget_type_to_string(KryonWidgetType type) {
    switch (type) {
        case KRYON_WIDGET_COLUMN: return "column";
        case KRYON_WIDGET_ROW: return "row";
        case KRYON_WIDGET_STACK: return "stack";
        case KRYON_WIDGET_CONTAINER: return "container";
        case KRYON_WIDGET_EXPANDED: return "expanded";
        case KRYON_WIDGET_FLEXIBLE: return "flexible";
        case KRYON_WIDGET_TEXT: return "text";
        case KRYON_WIDGET_BUTTON: return "button";
        case KRYON_WIDGET_INPUT: return "input";
        case KRYON_WIDGET_IMAGE: return "image";
        case KRYON_WIDGET_TEXTAREA: return "textarea";
        case KRYON_WIDGET_CUSTOM: return "custom";
        default: return "unknown";
    }
}

KryonWidgetType kryon_widget_type_from_string(const char* type_str) {
    if (!type_str) return KRYON_WIDGET_CUSTOM;
    
    if (strcmp(type_str, "column") == 0) return KRYON_WIDGET_COLUMN;
    if (strcmp(type_str, "row") == 0) return KRYON_WIDGET_ROW;
    if (strcmp(type_str, "stack") == 0) return KRYON_WIDGET_STACK;
    if (strcmp(type_str, "container") == 0) return KRYON_WIDGET_CONTAINER;
    if (strcmp(type_str, "expanded") == 0) return KRYON_WIDGET_EXPANDED;
    if (strcmp(type_str, "flexible") == 0) return KRYON_WIDGET_FLEXIBLE;
    if (strcmp(type_str, "text") == 0) return KRYON_WIDGET_TEXT;
    if (strcmp(type_str, "button") == 0) return KRYON_WIDGET_BUTTON;
    if (strcmp(type_str, "input") == 0) return KRYON_WIDGET_INPUT;
    if (strcmp(type_str, "image") == 0) return KRYON_WIDGET_IMAGE;
    if (strcmp(type_str, "textarea") == 0) return KRYON_WIDGET_TEXTAREA;
    
    return KRYON_WIDGET_CUSTOM;
}

KryonEdgeInsets kryon_edge_insets_all(float value) {
    return (KryonEdgeInsets){value, value, value, value};
}

KryonEdgeInsets kryon_edge_insets_trbl(float top, float right, float bottom, float left) {
    return (KryonEdgeInsets){top, right, bottom, left};
}