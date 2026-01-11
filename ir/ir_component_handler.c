#include "ir_component_handler.h"
#include "components/ir_component_registry.h"
#include "components/ir_component_text.h"
#include "components/ir_component_flexbox.h"
#include "components/ir_component_button.h"
#include "components/ir_component_input.h"
#include "components/ir_component_checkbox.h"
#include "components/ir_component_dropdown.h"
#include "components/ir_component_table.h"
#include "components/ir_component_image.h"
#include "components/ir_component_modal.h"
#include "components/ir_component_canvas.h"
#include "components/ir_component_misc.h"
#include "components/ir_component_tabs.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Handler Registry - Global State
// ============================================================================

// Global handler registry - maps component types to their handlers
// NOTE: Must NOT be static - needs to be shared across all shared library users
const IRComponentHandler* g_component_handlers[IR_COMPONENT_HANDLER_REGISTRY_SIZE] = {NULL};

// Registry initialization flag
static bool g_handler_registry_initialized = false;

// ============================================================================
// Registry API
// ============================================================================

void ir_component_handler_registry_init(void) {
    if (g_handler_registry_initialized) {
        return;  // Already initialized
    }

    // Initialize all handlers to NULL
    for (size_t i = 0; i < IR_COMPONENT_HANDLER_REGISTRY_SIZE; i++) {
        g_component_handlers[i] = NULL;
    }

    // Register all built-in component handlers
    ir_component_handlers_register_builtin();

    // Also initialize the layout traits registry for compatibility
    ir_layout_init_traits();

    g_handler_registry_initialized = true;

    if (getenv("KRYON_DEBUG_HANDLER_REGISTRY")) {
        fprintf(stderr, "[HandlerRegistry] Component handler registry initialized\n");
    }
}

bool ir_component_handler_register(const IRComponentHandler* handler) {
    if (!handler) {
        fprintf(stderr, "[HandlerRegistry] Cannot register NULL handler\n");
        return false;
    }

    if (handler->type < 0 || handler->type >= IR_COMPONENT_HANDLER_REGISTRY_SIZE) {
        fprintf(stderr, "[HandlerRegistry] Invalid component type: %d\n", handler->type);
        return false;
    }

    g_component_handlers[handler->type] = handler;

    if (getenv("KRYON_DEBUG_HANDLER_REGISTRY")) {
        fprintf(stderr, "[HandlerRegistry] Registered handler: %s (type=%d)\n",
                handler->name, handler->type);
    }

    return true;
}

const IRComponentHandler* ir_component_handler_get(IRComponentType type) {
    if (type < 0 || type >= IR_COMPONENT_HANDLER_REGISTRY_SIZE) {
        return NULL;
    }
    return g_component_handlers[type];
}

bool ir_component_handler_has(IRComponentType type) {
    return ir_component_handler_get(type) != NULL;
}

// ============================================================================
// Convenience Dispatch Functions
// ============================================================================

bool ir_component_handler_serialize(const IRComponent* comp, cJSON* json) {
    if (!comp || !json) return false;

    const IRComponentHandler* handler = ir_component_handler_get(comp->type);
    if (!handler || !handler->serialize) {
        return false;  // No handler or no serialize function
    }

    return handler->serialize(comp, json);
}

bool ir_component_handler_deserialize(const cJSON* json, IRComponent* comp) {
    if (!json || !comp) return false;

    const IRComponentHandler* handler = ir_component_handler_get(comp->type);
    if (!handler || !handler->deserialize) {
        return false;  // No handler or no deserialize function
    }

    return handler->deserialize(json, comp);
}

bool ir_component_handler_measure(const IRComponent* comp,
                                   float* out_width, float* out_height) {
    if (!comp || !out_width || !out_height) return false;

    const IRComponentHandler* handler = ir_component_handler_get(comp->type);
    if (!handler || !handler->measure) {
        return false;  // No handler or no measure function
    }

    handler->measure(comp, out_width, out_height);
    return true;
}

bool ir_component_handler_apply_style(IRComponent* comp) {
    if (!comp) return false;

    const IRComponentHandler* handler = ir_component_handler_get(comp->type);
    if (!handler || !handler->apply_style) {
        return false;  // No handler or no apply_style function
    }

    return handler->apply_style(comp);
}

IRStyle* ir_component_handler_get_default_style(IRComponentType type) {
    const IRComponentHandler* handler = ir_component_handler_get(type);
    if (!handler || !handler->get_default_style) {
        return NULL;  // No handler or no default style
    }

    return handler->get_default_style();
}

bool ir_component_handler_validate(const IRComponent* comp,
                                    char* error_msg, size_t error_size) {
    if (!comp) {
        if (error_msg && error_size > 0) {
            snprintf(error_msg, error_size, "Component is NULL");
        }
        return false;
    }

    const IRComponentHandler* handler = ir_component_handler_get(comp->type);
    if (!handler || !handler->validate) {
        return true;  // No handler means no validation rules - pass by default
    }

    return handler->validate(comp, error_msg, error_size);
}

bool ir_component_handler_to_string(const IRComponent* comp,
                                     char* buffer, size_t size) {
    if (!comp || !buffer || size == 0) return false;

    const IRComponentHandler* handler = ir_component_handler_get(comp->type);
    if (!handler) {
        snprintf(buffer, size, "Component(type=%d)", comp->type);
        return true;
    }

    if (handler->to_string) {
        handler->to_string(comp, buffer, size);
    } else {
        snprintf(buffer, size, "%s(id=%u)", handler->name ? handler->name : "Unknown",
                 comp->id);
    }

    return true;
}

// ============================================================================
// Built-in Handler Registration
// ============================================================================

void ir_component_handlers_register_builtin(void) {
    // Register all built-in component handlers
    ir_register_text_handler();
    ir_register_button_handler();
    ir_register_container_handler();
    ir_register_row_handler();
    ir_register_column_handler();
    ir_register_input_handler();
    ir_register_image_handler();
    ir_register_canvas_handler();
    ir_register_checkbox_handler();
    ir_register_dropdown_handler();
    ir_register_table_handler();
    ir_register_modal_handler();
    ir_register_misc_handler();
    ir_register_tabs_handler();

    if (getenv("KRYON_DEBUG_HANDLER_REGISTRY")) {
        fprintf(stderr, "[HandlerRegistry] All built-in handlers registered\n");
    }
}

// ============================================================================
// Generic Handler Implementations
// ============================================================================

// Generic serialize function - handles standard properties
static bool generic_serialize(const IRComponent* comp, cJSON* json) {
    if (!comp || !json) return false;

    // Add component type
    cJSON_AddStringToObject(json, "type", ir_component_type_to_string(comp->type));

    // Add id
    cJSON_AddNumberToObject(json, "id", (double)comp->id);

    // Add text content if present
    if (comp->text_content) {
        cJSON_AddStringToObject(json, "text", comp->text_content);
    }

    // Add tag if present
    if (comp->tag) {
        cJSON_AddStringToObject(json, "tag", comp->tag);
    }

    // Add css_class if present
    if (comp->css_class) {
        cJSON_AddStringToObject(json, "class", comp->css_class);
    }

    return true;
}

// Generic to_string function
static void generic_to_string(const IRComponent* comp, char* buffer, size_t size) {
    if (!comp || !buffer || size == 0) return;

    const char* type_name = ir_component_type_to_string(comp->type);
    if (comp->text_content) {
        snprintf(buffer, size, "%s(id=%u, text=\"%s\")",
                 type_name, comp->id, comp->text_content);
    } else {
        snprintf(buffer, size, "%s(id=%u, children=%u)",
                 type_name, comp->id, comp->child_count);
    }
}

// Generic validate function
static bool generic_validate(const IRComponent* comp, char* error_msg, size_t error_size) {
    (void)comp;  // Unused
    (void)error_msg;
    (void)error_size;
    return true;  // Generic components are always valid
}

// ============================================================================
// Component Handler Definitions
// ============================================================================
// Each handler wraps the existing layout trait and provides generic handlers
// for operations not yet implemented per-component
// ============================================================================

// Text handler
static const IRComponentHandler g_text_handler = {
    .layout_component = NULL,  // Uses IR_TEXT_LAYOUT_TRAIT via registry
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Text",
    .type = IR_COMPONENT_TEXT
};

void ir_register_text_handler(void) {
    ir_component_handler_register(&g_text_handler);
}

// Button handler
static const IRComponentHandler g_button_handler = {
    .layout_component = NULL,  // Uses IR_BUTTON_LAYOUT_TRAIT via registry
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Button",
    .type = IR_COMPONENT_BUTTON
};

void ir_register_button_handler(void) {
    ir_component_handler_register(&g_button_handler);
}

// Container handler
static const IRComponentHandler g_container_handler = {
    .layout_component = NULL,  // Uses IR_CONTAINER_LAYOUT_TRAIT via registry
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Container",
    .type = IR_COMPONENT_CONTAINER
};

void ir_register_container_handler(void) {
    ir_component_handler_register(&g_container_handler);
}

// Row handler
static const IRComponentHandler g_row_handler = {
    .layout_component = NULL,  // Uses IR_ROW_LAYOUT_TRAIT via registry
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Row",
    .type = IR_COMPONENT_ROW
};

void ir_register_row_handler(void) {
    ir_component_handler_register(&g_row_handler);
}

// Column handler
static const IRComponentHandler g_column_handler = {
    .layout_component = NULL,  // Uses IR_COLUMN_LAYOUT_TRAIT via registry
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Column",
    .type = IR_COMPONENT_COLUMN
};

void ir_register_column_handler(void) {
    ir_component_handler_register(&g_column_handler);
}

// Input handler
static const IRComponentHandler g_input_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Input",
    .type = IR_COMPONENT_INPUT
};

void ir_register_input_handler(void) {
    ir_component_handler_register(&g_input_handler);
}

// Image handler
static const IRComponentHandler g_image_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Image",
    .type = IR_COMPONENT_IMAGE
};

void ir_register_image_handler(void) {
    ir_component_handler_register(&g_image_handler);
}

// Canvas handler (covers both Canvas and NativeCanvas)
static const IRComponentHandler g_canvas_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Canvas",
    .type = IR_COMPONENT_CANVAS
};

void ir_register_canvas_handler(void) {
    ir_component_handler_register(&g_canvas_handler);
}

// Checkbox handler
static const IRComponentHandler g_checkbox_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Checkbox",
    .type = IR_COMPONENT_CHECKBOX
};

void ir_register_checkbox_handler(void) {
    ir_component_handler_register(&g_checkbox_handler);
}

// Dropdown handler
static const IRComponentHandler g_dropdown_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Dropdown",
    .type = IR_COMPONENT_DROPDOWN
};

void ir_register_dropdown_handler(void) {
    ir_component_handler_register(&g_dropdown_handler);
}

// Table handler (covers all table components)
static const IRComponentHandler g_table_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Table",
    .type = IR_COMPONENT_TABLE
};

void ir_register_table_handler(void) {
    ir_component_handler_register(&g_table_handler);
}

// Modal handler
static const IRComponentHandler g_modal_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Modal",
    .type = IR_COMPONENT_MODAL
};

void ir_register_modal_handler(void) {
    ir_component_handler_register(&g_modal_handler);
}

// Misc handler (Markdown, Sprite, etc.)
static const IRComponentHandler g_misc_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "Misc",
    .type = IR_COMPONENT_MARKDOWN  // Representative type
};

void ir_register_misc_handler(void) {
    // Register for multiple misc types
    ir_component_handler_register(&g_misc_handler);

    // Create and register handlers for other misc types
    static IRComponentHandler sprite_handler = g_misc_handler;
    sprite_handler.name = "Sprite";
    sprite_handler.type = IR_COMPONENT_SPRITE;
    ir_component_handler_register(&sprite_handler);
}

// Tabs handler (covers all tab components)
static const IRComponentHandler g_tabs_handler = {
    .layout_component = NULL,
    .serialize = generic_serialize,
    .deserialize = NULL,
    .measure = NULL,
    .get_default_style = NULL,
    .apply_style = NULL,
    .validate = generic_validate,
    .to_string = generic_to_string,
    .name = "TabGroup",
    .type = IR_COMPONENT_TAB_GROUP
};

void ir_register_tabs_handler(void) {
    // Register for all tab-related types
    ir_component_handler_register(&g_tabs_handler);

    // Create handlers for other tab types
    static IRComponentHandler tab_bar_handler = g_tabs_handler;
    tab_bar_handler.name = "TabBar";
    tab_bar_handler.type = IR_COMPONENT_TAB_BAR;
    ir_component_handler_register(&tab_bar_handler);

    static IRComponentHandler tab_handler = g_tabs_handler;
    tab_handler.name = "Tab";
    tab_handler.type = IR_COMPONENT_TAB;
    ir_component_handler_register(&tab_handler);

    static IRComponentHandler tab_content_handler = g_tabs_handler;
    tab_content_handler.name = "TabContent";
    tab_content_handler.type = IR_COMPONENT_TAB_CONTENT;
    ir_component_handler_register(&tab_content_handler);

    static IRComponentHandler tab_panel_handler = g_tabs_handler;
    tab_panel_handler.name = "TabPanel";
    tab_panel_handler.type = IR_COMPONENT_TAB_PANEL;
    ir_component_handler_register(&tab_panel_handler);
}
