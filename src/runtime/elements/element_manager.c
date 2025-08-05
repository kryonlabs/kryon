/**
 * @file element_manager.c
 * @brief Element Management and Tree Operations
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * Provides element lifecycle management, tree traversal, and property binding.
 */

#include "internal/runtime.h"
#include "internal/memory.h"
#include "internal/types.h"
#include "internal/events.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// ELEMENT TRAVERSAL
// =============================================================================

typedef struct {
    bool (*visitor)(KryonElement *element, void *user_data);
    void *user_data;
    bool stop_traversal;
} ElementTraversalContext;

static void traverse_element_tree_internal(KryonElement *element, ElementTraversalContext *ctx) {
    if (!element || ctx->stop_traversal) {
        return;
    }
    
    // Visit current element
    if (!ctx->visitor(element, ctx->user_data)) {
        ctx->stop_traversal = true;
        return;
    }
    
    // Visit children
    for (size_t i = 0; i < element->child_count; i++) {
        traverse_element_tree_internal(element->children[i], ctx);
        if (ctx->stop_traversal) {
            break;
        }
    }
}

void kryon_element_traverse(KryonElement *root, 
                           bool (*visitor)(KryonElement *element, void *user_data),
                           void *user_data) {
    if (!root || !visitor) {
        return;
    }
    
    ElementTraversalContext ctx = {
        .visitor = visitor,
        .user_data = user_data,
        .stop_traversal = false
    };
    
    traverse_element_tree_internal(root, &ctx);
}

// =============================================================================
// ELEMENT QUERIES
// =============================================================================

typedef struct {
    const char *class_name;
    KryonElement **results;
    size_t max_results;
    size_t found_count;
} ClassSearchContext;

static bool class_search_visitor(KryonElement *element, void *user_data) {
    ClassSearchContext *ctx = (ClassSearchContext*)user_data;
    
    // Check if element has the class
    for (size_t i = 0; i < element->class_count; i++) {
        if (element->class_names[i] && 
            strcmp(element->class_names[i], ctx->class_name) == 0) {
            if (ctx->found_count < ctx->max_results) {
                ctx->results[ctx->found_count++] = element;
            }
            break;
        }
    }
    
    return ctx->found_count < ctx->max_results; // Continue if we have room
}

size_t kryon_element_find_by_class(KryonElement *root, const char *class_name,
                                  KryonElement **results, size_t max_results) {
    if (!root || !class_name || !results || max_results == 0) {
        return 0;
    }
    
    ClassSearchContext ctx = {
        .class_name = class_name,
        .results = results,
        .max_results = max_results,
        .found_count = 0
    };
    
    kryon_element_traverse(root, class_search_visitor, &ctx);
    return ctx.found_count;
}

typedef struct {
    uint16_t element_type;
    KryonElement **results;
    size_t max_results;
    size_t found_count;
} TypeSearchContext;

static bool type_search_visitor(KryonElement *element, void *user_data) {
    TypeSearchContext *ctx = (TypeSearchContext*)user_data;
    
    if (element->type == ctx->element_type) {
        if (ctx->found_count < ctx->max_results) {
            ctx->results[ctx->found_count++] = element;
        }
    }
    
    return ctx->found_count < ctx->max_results;
}

size_t kryon_element_find_by_type(KryonElement *root, uint16_t element_type,
                                 KryonElement **results, size_t max_results) {
    if (!root || !results || max_results == 0) {
        return 0;
    }
    
    TypeSearchContext ctx = {
        .element_type = element_type,
        .results = results,
        .max_results = max_results,
        .found_count = 0
    };
    
    kryon_element_traverse(root, type_search_visitor, &ctx);
    return ctx.found_count;
}

// =============================================================================
// ELEMENT PROPERTIES
// =============================================================================

KryonProperty *kryon_element_get_property_by_name(KryonElement *element, const char *name) {
    if (!element || !name) {
        return NULL;
    }
    
    for (size_t i = 0; i < element->property_count; i++) {
        if (element->properties[i]->name && 
            strcmp(element->properties[i]->name, name) == 0) {
            return element->properties[i];
        }
    }
    
    return NULL;
}

bool kryon_element_set_property_by_name(KryonElement *element, const char *name, const void *value) {
    if (!element || !name || !value) {
        return false;
    }
    
    KryonProperty *prop = kryon_element_get_property_by_name(element, name);
    if (prop) {
        // Update existing property
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
        
        kryon_element_invalidate_render(element);
        return true;
    }
    
    // Property doesn't exist - we would need to create it
    // For now, return false
    return false;
}

// =============================================================================
// ELEMENT LIFECYCLE
// =============================================================================

void kryon_element_mount(KryonElement *element) {
    if (!element || element->state != KRYON_ELEMENT_STATE_CREATED) {
        return;
    }
    
    element->state = KRYON_ELEMENT_STATE_MOUNTING;
    
    // TODO: Call lifecycle hooks
    
    element->state = KRYON_ELEMENT_STATE_MOUNTED;
    
    // Mount children
    for (size_t i = 0; i < element->child_count; i++) {
        kryon_element_mount(element->children[i]);
    }
}

void kryon_element_unmount(KryonElement *element) {
    if (!element || element->state != KRYON_ELEMENT_STATE_MOUNTED) {
        return;
    }
    
    element->state = KRYON_ELEMENT_STATE_UNMOUNTING;
    
    // Unmount children first
    for (size_t i = 0; i < element->child_count; i++) {
        kryon_element_unmount(element->children[i]);
    }
    
    // TODO: Call lifecycle hooks
    
    element->state = KRYON_ELEMENT_STATE_UNMOUNTED;
}

// =============================================================================
// ELEMENT BOUNDS AND LAYOUT
// =============================================================================

void kryon_element_set_bounds(KryonElement *element, float x, float y, float width, float height) {
    if (!element) {
        return;
    }
    
    bool changed = false;
    
    if (element->x != x || element->y != y) {
        element->x = x;
        element->y = y;
        changed = true;
    }
    
    if (element->width != width || element->height != height) {
        element->width = width;
        element->height = height;
        changed = true;
        element->needs_layout = true;
    }
    
    if (changed) {
        element->needs_render = true;
    }
}

void kryon_element_get_bounds(KryonElement *element, float *x, float *y, float *width, float *height) {
    if (!element) {
        return;
    }
    
    if (x) *x = element->x;
    if (y) *y = element->y;
    if (width) *width = element->width;
    if (height) *height = element->height;
}

void kryon_element_set_padding(KryonElement *element, float top, float right, float bottom, float left) {
    if (!element) {
        return;
    }
    
    element->padding[0] = top;
    element->padding[1] = right;
    element->padding[2] = bottom;
    element->padding[3] = left;
    
    element->needs_layout = true;
    element->needs_render = true;
}

void kryon_element_set_margin(KryonElement *element, float top, float right, float bottom, float left) {
    if (!element) {
        return;
    }
    
    element->margin[0] = top;
    element->margin[1] = right;
    element->margin[2] = bottom;
    element->margin[3] = left;
    
    element->needs_layout = true;
    
    // Margin changes affect parent layout
    if (element->parent) {
        element->parent->needs_layout = true;
    }
}

// =============================================================================
// ELEMENT VISIBILITY AND STATE
// =============================================================================

void kryon_element_set_visible(KryonElement *element, bool visible) {
    if (!element || element->visible == visible) {
        return;
    }
    
    element->visible = visible;
    element->needs_render = true;
    
    // Visibility changes can affect parent layout
    if (element->parent) {
        element->parent->needs_layout = true;
    }
}

void kryon_element_set_enabled(KryonElement *element, bool enabled) {
    if (!element || element->enabled == enabled) {
        return;
    }
    
    element->enabled = enabled;
    element->needs_render = true;
}

// =============================================================================
// ELEMENT HIERARCHY
// =============================================================================

bool kryon_element_add_child(KryonElement *parent, KryonElement *child) {
    if (!parent || !child || child->parent) {
        return false;
    }
    
    // Expand children array if needed
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity ? parent->child_capacity * 2 : 4;
        KryonElement **new_children = kryon_realloc(parent->children,
                                                   new_capacity * sizeof(KryonElement*));
        if (!new_children) {
            return false;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    
    // Adding a child requires layout update
    parent->needs_layout = true;
    parent->needs_render = true;
    
    // Mount child if parent is mounted
    if (parent->state == KRYON_ELEMENT_STATE_MOUNTED) {
        kryon_element_mount(child);
    }
    
    return true;
}

bool kryon_element_remove_child(KryonElement *parent, KryonElement *child) {
    if (!parent || !child || child->parent != parent) {
        return false;
    }
    
    // Find child in parent's array
    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            // Unmount child if mounted
            if (child->state == KRYON_ELEMENT_STATE_MOUNTED) {
                kryon_element_unmount(child);
            }
            
            // Remove from array
            for (size_t j = i + 1; j < parent->child_count; j++) {
                parent->children[j - 1] = parent->children[j];
            }
            parent->child_count--;
            
            child->parent = NULL;
            
            // Removing a child requires layout update
            parent->needs_layout = true;
            parent->needs_render = true;
            
            return true;
        }
    }
    
    return false;
}

bool kryon_element_insert_child(KryonElement *parent, KryonElement *child, size_t index) {
    if (!parent || !child || child->parent) {
        return false;
    }
    
    if (index > parent->child_count) {
        index = parent->child_count;
    }
    
    // Expand children array if needed
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity ? parent->child_capacity * 2 : 4;
        KryonElement **new_children = kryon_realloc(parent->children,
                                                   new_capacity * sizeof(KryonElement*));
        if (!new_children) {
            return false;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    // Shift elements to make room
    for (size_t i = parent->child_count; i > index; i--) {
        parent->children[i] = parent->children[i - 1];
    }
    
    parent->children[index] = child;
    parent->child_count++;
    child->parent = parent;
    
    // Inserting a child requires layout update
    parent->needs_layout = true;
    parent->needs_render = true;
    
    // Mount child if parent is mounted
    if (parent->state == KRYON_ELEMENT_STATE_MOUNTED) {
        kryon_element_mount(child);
    }
    
    return true;
}

// =============================================================================
// EVENT HANDLING
// =============================================================================

bool kryon_element_add_handler(KryonElement *element, KryonEventType type,
                              KryonEventHandler handler, void *user_data, bool capture) {
    if (!element || !handler) {
        return false;
    }
    
    // Check if handler already exists
    for (size_t i = 0; i < element->handler_count; i++) {
        ElementEventHandler *h = element->handlers[i];
        if (h->type == type && h->handler == handler && h->capture == capture) {
            // Update user data
            h->user_data = user_data;
            return true;
        }
    }
    
    // Expand handlers array if needed
    if (element->handler_count >= element->handler_capacity) {
        size_t new_capacity = element->handler_capacity ? element->handler_capacity * 2 : 4;
        ElementEventHandler **new_handlers = kryon_realloc(element->handlers,
                                                        new_capacity * sizeof(ElementEventHandler*));
        if (!new_handlers) {
            return false;
        }
        element->handlers = new_handlers;
        element->handler_capacity = new_capacity;
    }
    
    // Create new handler
    ElementEventHandler *new_handler = kryon_alloc(sizeof(ElementEventHandler));
    if (!new_handler) {
        return false;
    }
    
    new_handler->type = type;
    new_handler->handler = handler;
    new_handler->user_data = user_data;
    new_handler->capture = capture;
    
    element->handlers[element->handler_count++] = new_handler;
    
    return true;
}

bool kryon_element_remove_handler(KryonElement *element, KryonEventType type,
                                 KryonEventHandler handler) {
    if (!element || !handler) {
        return false;
    }
    
    bool removed = false;
    
    // Remove all matching handlers
    for (size_t i = 0; i < element->handler_count; ) {
        ElementEventHandler *h = element->handlers[i];
        if (h->type == type && h->handler == handler) {
            // Free handler
            kryon_free(h);
            
            // Shift remaining handlers
            for (size_t j = i + 1; j < element->handler_count; j++) {
                element->handlers[j - 1] = element->handlers[j];
            }
            element->handler_count--;
            removed = true;
        } else {
            i++;
        }
    }
    
    return removed;
}

void kryon_element_dispatch_event(KryonElement *element, const KryonEvent *event) {
    if (!element || !event) {
        return;
    }
    
    // Dispatch to handlers
    for (size_t i = 0; i < element->handler_count; i++) {
        ElementEventHandler *handler = element->handlers[i];
        if (handler->type == event->type) {
            handler->handler(event, handler->user_data);
            
            // Note: events.h KryonEvent may not have propagation_stopped field
            // Skip propagation check for now
        }
    }
}

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

void kryon_element_print_tree(KryonElement *element, FILE *file, int indent) {
    if (!element) {
        return;
    }
    
    if (!file) {
        file = stdout;
    }
    
    // Print indent
    for (int i = 0; i < indent; i++) {
        fprintf(file, "  ");
    }
    
    // Print element info
    fprintf(file, "<%s", element->type_name ? element->type_name : "Unknown");
    
    if (element->element_id) {
        fprintf(file, " id=\"%s\"", element->element_id);
    }
    
    if (element->class_count > 0) {
        fprintf(file, " class=\"");
        for (size_t i = 0; i < element->class_count; i++) {
            if (i > 0) fprintf(file, " ");
            fprintf(file, "%s", element->class_names[i]);
        }
        fprintf(file, "\"");
    }
    
    fprintf(file, " state=%d visible=%d enabled=%d",
           element->state, element->visible, element->enabled);
    
    if (element->child_count == 0) {
        fprintf(file, " />\n");
    } else {
        fprintf(file, ">\n");
        
        // Print children
        for (size_t i = 0; i < element->child_count; i++) {
            kryon_element_print_tree(element->children[i], file, indent + 1);
        }
        
        // Print closing tag
        for (int i = 0; i < indent; i++) {
            fprintf(file, "  ");
        }
        fprintf(file, "</%s>\n", element->type_name ? element->type_name : "Unknown");
    }
}