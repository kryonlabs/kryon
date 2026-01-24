/**
 * Kryon Reactive UI Binding - Implementation
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "kryon_reactive_ui.h"
#include "kryon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Include IR core for direct access (needed for text updates)
#include "../../ir/include/ir_core.h"
#include "../../ir/include/ir_builder.h"

// ============================================================================
// Internal Structures
// ============================================================================

// Include desktop internals for g_desktop_renderer and renderer functions
#include "../../runtime/desktop/desktop_internal.h"

struct KryonTextBinding {
    IRComponent* component;
    KryonSignal* signal;
    char* format;
    char* prefix;
    char* suffix;
    uint32_t subscription_id;
    bool is_string_signal;
    bool is_two_way;
    bool owns_component;
};

struct KryonVisibilityBinding {
    IRComponent* component;
    KryonSignal* signal;
    uint32_t subscription_id;
    bool is_bool;
    bool invert;  // If true, visibility is inverted
};

struct KryonBindingGroup {
    KryonTextBinding** text_bindings;
    KryonVisibilityBinding** visibility_bindings;
    uint32_t text_count;
    uint32_t text_capacity;
    uint32_t visibility_count;
    uint32_t visibility_capacity;
};

// ============================================================================
// Static Callback Functions
// ============================================================================

/* Background color change callback */
static void background_color_callback(float value, void* user_data) {
    KryonVisibilityBinding* b = (KryonVisibilityBinding*)user_data;
    kryon_set_background(b->component, (uint32_t)value);

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
    }
}

/* Text color change callback */
static void text_color_callback(float value, void* user_data) {
    KryonVisibilityBinding* b = (KryonVisibilityBinding*)user_data;
    kryon_set_color(b->component, (uint32_t)value);

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
    }
}

/* Opacity callback for enabled binding */
static void opacity_from_float_callback(float value, void* user_data) {
    KryonVisibilityBinding* b = (KryonVisibilityBinding*)user_data;
    kryon_set_opacity(b->component, value != 0.0f ? 1.0f : 0.5f);

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
    }
}

/* Opacity callback from bool */
static void opacity_from_bool_callback(bool value, void* user_data) {
    KryonVisibilityBinding* b = (KryonVisibilityBinding*)user_data;
    kryon_set_opacity(b->component, value ? 1.0f : 0.5f);

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
    }
}

/* Generic property setter callback */
static void property_setter_callback(float value, void* user_data) {
    KryonTextBinding* b = (KryonTextBinding*)user_data;
    KryonPropertySetter setter = (KryonPropertySetter)(uintptr_t)b->format;
    setter(b->component, value);

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
    }
}

// ============================================================================
// Helpers
// ============================================================================

// Forward declare IR builder function
extern void ir_set_text_content(IRComponent* component, const char* text);

static void update_text_component(IRComponent* component, const char* text) {
    if (!component || !text) return;

    // Use the IR builder API to set text content
    ir_set_text_content(component, text);
}

static const char* format_float_value(float value, const char* format) {
    static __thread char buffer[256];
    snprintf(buffer, sizeof(buffer), format ? format : "%g", value);
    return buffer;
}

// ============================================================================
// Text Binding Implementation
// ============================================================================

static void text_binding_callback_float(float value, void* user_data) {
    KryonTextBinding* binding = (KryonTextBinding*)user_data;

    // Debug logging
    printf("[BINDING] text_binding_callback_float called: value=%f, component=%p\n",
           value, (void*)binding->component);

    // Format the value
    const char* formatted = format_float_value(value, binding->format);

    // Apply prefix/suffix if present
    if (binding->prefix || binding->suffix) {
        static __thread char buffer[512];
        snprintf(buffer, sizeof(buffer),
                 "%s%s%s",
                 binding->prefix ? binding->prefix : "",
                 formatted,
                 binding->suffix ? binding->suffix : "");
        printf("[BINDING] Updating text component to: %s\n", buffer);
        update_text_component(binding->component, buffer);
    } else {
        printf("[BINDING] Updating text component to: %s\n", formatted);
        update_text_component(binding->component, formatted);
    }

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
        printf("[BINDING] Marked renderer as reactive dirty\n");
    } else {
        printf("[BINDING] WARNING: g_desktop_renderer is NULL!\n");
    }
}

static void text_binding_callback_string(const char* value, void* user_data) {
    KryonTextBinding* binding = (KryonTextBinding*)user_data;

    if (binding->prefix || binding->suffix) {
        static __thread char buffer[512];
        snprintf(buffer, sizeof(buffer),
                 "%s%s%s",
                 binding->prefix ? binding->prefix : "",
                 value ? value : "",
                 binding->suffix ? binding->suffix : "");
        update_text_component(binding->component, buffer);
    } else {
        update_text_component(binding->component, value ? value : "");
    }

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
    }
}

static KryonTextBinding* text_binding_create(IRComponent* component,
                                              KryonSignal* signal,
                                              const char* format,
                                              const char* prefix,
                                              const char* suffix) {
    if (!component || !signal) return NULL;

    KryonTextBinding* binding = calloc(1, sizeof(KryonTextBinding));
    if (!binding) return NULL;

    binding->component = component;
    binding->signal = signal;
    binding->is_string_signal = kryon_signal_is_string(signal);

    if (format) {
        binding->format = strdup(format);
    }
    if (prefix) {
        binding->prefix = strdup(prefix);
    }
    if (suffix) {
        binding->suffix = strdup(suffix);
    }

    // Subscribe to signal changes
    if (binding->is_string_signal) {
        binding->subscription_id = kryon_signal_subscribe_string(
            signal, text_binding_callback_string, binding);
    } else {
        binding->subscription_id = kryon_signal_subscribe(
            signal, text_binding_callback_float, binding);
    }

    // Trigger initial update
    if (binding->is_string_signal) {
        text_binding_callback_string(kryon_signal_get_string(signal), binding);
    } else {
        text_binding_callback_float(kryon_signal_get(signal), binding);
    }

    return binding;
}

KryonTextBinding* kryon_bind_text(IRComponent* component,
                                   KryonSignal* signal,
                                   const char* format) {
    return text_binding_create(component, signal, format, NULL, NULL);
}

KryonTextBinding* kryon_bind_text_template(IRComponent* component,
                                            KryonSignal* signal,
                                            const char* prefix,
                                            const char* suffix) {
    return text_binding_create(component, signal, NULL, prefix, suffix);
}

void kryon_unbind_text(KryonTextBinding* binding) {
    if (!binding) return;

    if (binding->subscription_id > 0) {
        kryon_signal_unsubscribe(binding->signal, binding->subscription_id);
    }

    free(binding->format);
    free(binding->prefix);
    free(binding->suffix);

    // Note: We don't destroy the component as it may be used elsewhere
    free(binding);
}

// ============================================================================
// Visibility Binding Implementation
// ============================================================================

static void visibility_callback_bool(bool value, void* user_data) {
    KryonVisibilityBinding* binding = (KryonVisibilityBinding*)user_data;

    bool visible = binding->invert ? !value : value;
    kryon_set_visible(binding->component, visible);

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
    }
}

static void visibility_callback_float(float value, void* user_data) {
    KryonVisibilityBinding* binding = (KryonVisibilityBinding*)user_data;

    // Treat non-zero as visible
    bool visible = binding->invert ? (value == 0.0f) : (value != 0.0f);
    kryon_set_visible(binding->component, visible);

    // Mark renderer as reactive dirty to trigger re-render
    if (g_desktop_renderer) {
        desktop_ir_renderer_mark_reactive_dirty(g_desktop_renderer);
    }
}

KryonVisibilityBinding* kryon_bind_visible(IRComponent* component,
                                            KryonSignal* signal) {
    if (!component || !signal) return NULL;

    KryonVisibilityBinding* binding = calloc(1, sizeof(KryonVisibilityBinding));
    if (!binding) return NULL;

    binding->component = component;
    binding->signal = signal;
    binding->is_bool = kryon_signal_is_bool(signal);

    // Subscribe to signal changes
    if (binding->is_bool) {
        binding->subscription_id = kryon_signal_subscribe_bool(
            signal, visibility_callback_bool, binding);
    } else {
        binding->subscription_id = kryon_signal_subscribe(
            signal, visibility_callback_float, binding);
    }

    // Trigger initial update
    if (binding->is_bool) {
        visibility_callback_bool(kryon_signal_get_bool(signal), binding);
    } else {
        visibility_callback_float(kryon_signal_get(signal), binding);
    }

    return binding;
}

void kryon_unbind_visible(KryonVisibilityBinding* binding) {
    if (!binding) return;

    if (binding->subscription_id > 0) {
        kryon_signal_unsubscribe(binding->signal, binding->subscription_id);
    }

    free(binding);
}

// ============================================================================
// Property Binding Implementation
// ============================================================================

KryonTextBinding* kryon_bind_property(IRComponent* component,
                                       KryonSignal* signal,
                                       KryonPropertySetter setter) {
    if (!component || !signal || !setter) return NULL;

    // Create a binding that uses the custom setter
    KryonTextBinding* binding = calloc(1, sizeof(KryonTextBinding));
    if (!binding) return NULL;

    binding->component = component;
    binding->signal = signal;

    // Store setter function pointer in format field (hacky but works)
    binding->format = (char*)(uintptr_t)setter;

    // Subscribe with callback
    uint32_t sub_id = kryon_signal_subscribe(signal,
        property_setter_callback, binding);

    binding->subscription_id = sub_id;

    // Trigger initial update
    setter(component, kryon_signal_get(signal));

    return binding;
}

// ============================================================================
// Color Bindings
// ============================================================================

KryonVisibilityBinding* kryon_bind_background(IRComponent* component,
                                               KryonSignal* signal) {
    if (!component || !signal) return NULL;

    KryonVisibilityBinding* binding = calloc(1, sizeof(KryonVisibilityBinding));
    if (!binding) return NULL;

    binding->component = component;
    binding->signal = signal;

    binding->subscription_id = kryon_signal_subscribe(signal,
        background_color_callback, binding);

    // Trigger initial update
    kryon_set_background(component, (uint32_t)kryon_signal_get(signal));

    return binding;
}

KryonVisibilityBinding* kryon_bind_color(IRComponent* component,
                                          KryonSignal* signal) {
    if (!component || !signal) return NULL;

    KryonVisibilityBinding* binding = calloc(1, sizeof(KryonVisibilityBinding));
    if (!binding) return NULL;

    binding->component = component;
    binding->signal = signal;

    binding->subscription_id = kryon_signal_subscribe(signal,
        text_color_callback, binding);

    // Trigger initial update
    kryon_set_color(component, (uint32_t)kryon_signal_get(signal));

    return binding;
}

// ============================================================================
// Enabled Binding
// ============================================================================

KryonVisibilityBinding* kryon_bind_enabled(IRComponent* component,
                                            KryonSignal* signal) {
    if (!component || !signal) return NULL;

    KryonVisibilityBinding* binding = calloc(1, sizeof(KryonVisibilityBinding));
    if (!binding) return NULL;

    binding->component = component;
    binding->signal = signal;
    binding->is_bool = kryon_signal_is_bool(signal);

    if (binding->is_bool) {
        binding->subscription_id = kryon_signal_subscribe_bool(signal,
            opacity_from_bool_callback, binding);
        // Trigger initial
        kryon_set_opacity(component, kryon_signal_get_bool(signal) ? 1.0f : 0.5f);
    } else {
        binding->subscription_id = kryon_signal_subscribe(signal,
            opacity_from_float_callback, binding);
        // Trigger initial
        kryon_set_opacity(component, kryon_signal_get(signal) != 0.0f ? 1.0f : 0.5f);
    }

    return binding;
}

// ============================================================================
// Two-Way Input Binding
// ============================================================================

// This is a simplified version - full implementation would need to
// hook into the input component's change event
KryonTextBinding* kryon_bind_input_two_way(IRComponent* component,
                                             KryonSignal* signal) {
    // Create one-way binding from signal to input
    KryonTextBinding* binding = kryon_bind_text(component, signal, NULL);
    if (binding) {
        binding->is_two_way = true;

        // TODO: Set up event handler on input to update signal
        // This requires access to the input component's change event
        // which would be done during component creation or via
        // a separate kryon_input_bind_two_way function that
        // registers the change handler
    }

    return binding;
}

// ============================================================================
// List Binding
// ============================================================================

typedef struct {
    IRComponent* container;
    KryonListItemRenderer renderer;
    void* user_data;
    KryonSignal* count_signal;
    uint32_t subscription_id;
    IRComponent** items;
    size_t item_count;
} ListBindingData;

static void list_binding_callback(float count, void* user_data) {
    ListBindingData* data = (ListBindingData*)user_data;
    size_t new_count = (size_t)count;

    // Remove old items
    for (size_t i = 0; i < data->item_count; i++) {
        kryon_remove_child(data->container, data->items[i]);
    }

    // Free old items array
    free(data->items);

    // Create new items array
    data->items = calloc(new_count, sizeof(IRComponent*));
    data->item_count = new_count;

    // Render new items
    for (size_t i = 0; i < new_count; i++) {
        IRComponent* item = kryon_container();  // Wrapper for each item
        kryon_add_child(data->container, item);
        data->items[i] = item;

        if (data->renderer) {
            data->renderer(item, i, data->user_data);
        }
    }
}

KryonTextBinding* kryon_bind_list(IRComponent* container,
                                   KryonSignal* count_signal,
                                   KryonListItemRenderer renderer,
                                   void* user_data) {
    if (!container || !count_signal) return NULL;

    ListBindingData* data = calloc(1, sizeof(ListBindingData));
    if (!data) return NULL;

    data->container = container;
    data->renderer = renderer;
    data->user_data = user_data;
    data->count_signal = count_signal;

    // Subscribe to count changes
    data->subscription_id = kryon_signal_subscribe(count_signal,
        list_binding_callback, data);

    // Trigger initial render
    list_binding_callback(kryon_signal_get(count_signal), data);

    // Return a wrapper binding
    KryonTextBinding* binding = calloc(1, sizeof(KryonTextBinding));
    binding->component = container;
    binding->signal = count_signal;
    binding->subscription_id = 1;  // Dummy ID
    binding->format = (char*)data;  // Store data pointer

    return binding;
}

// ============================================================================
// Binding Groups
// ============================================================================

KryonBindingGroup* kryon_binding_group_create(void) {
    KryonBindingGroup* group = calloc(1, sizeof(KryonBindingGroup));
    if (group) {
        group->text_capacity = 16;
        group->text_bindings = calloc(group->text_capacity, sizeof(KryonTextBinding*));
        group->visibility_capacity = 16;
        group->visibility_bindings = calloc(group->visibility_capacity, sizeof(KryonVisibilityBinding*));
    }
    return group;
}

void kryon_binding_group_add_text(KryonBindingGroup* group,
                                    KryonTextBinding* binding) {
    if (!group || !binding) return;

    if (group->text_count >= group->text_capacity) {
        group->text_capacity *= 2;
        group->text_bindings = realloc(group->text_bindings,
                                       group->text_capacity * sizeof(KryonTextBinding*));
    }

    group->text_bindings[group->text_count++] = binding;
}

void kryon_binding_group_add_visible(KryonBindingGroup* group,
                                      KryonVisibilityBinding* binding) {
    if (!group || !binding) return;

    if (group->visibility_count >= group->visibility_capacity) {
        group->visibility_capacity *= 2;
        group->visibility_bindings = realloc(group->visibility_bindings,
                                             group->visibility_capacity * sizeof(KryonVisibilityBinding*));
    }

    group->visibility_bindings[group->visibility_count++] = binding;
}

void kryon_binding_group_destroy(KryonBindingGroup* group) {
    if (!group) return;

    // Destroy all text bindings
    for (uint32_t i = 0; i < group->text_count; i++) {
        // Check if this is a list binding (has data in format pointer)
        if (group->text_bindings[i]->format &&
            group->text_bindings[i]->subscription_id == 1) {
            // List binding data
            ListBindingData* data = (ListBindingData*)group->text_bindings[i]->format;
            if (data) {
                kryon_signal_unsubscribe(data->count_signal, data->subscription_id);
                free(data->items);
                free(data);
            }
        }
        kryon_unbind_text(group->text_bindings[i]);
    }

    // Destroy all visibility bindings
    for (uint32_t i = 0; i < group->visibility_count; i++) {
        kryon_unbind_visible(group->visibility_bindings[i]);
    }

    free(group->text_bindings);
    free(group->visibility_bindings);
    free(group);
}
