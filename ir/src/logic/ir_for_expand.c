// Enable strdup in C99 mode
#define _POSIX_C_SOURCE 200809L

#include "ir_for_expand.h"
#include "ir_for.h"
#include "../include/ir_core.h"
#include "../include/ir_builder.h"
#include "../include/ir_layout.h"
#include "cJSON.h"
#include "../utils/ir_json_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Forward declaration
static void expand_for_with_parent_internal(IRComponent* component, IRSourceStructures* source_structures, IRComponent* parent, int child_index);

// Forward declare layout invalidation
extern void ir_layout_invalidate_subtree(IRComponent* root);

// ============================================================================
// Component Deep Copy (moved from ir_serialization.c)
// ============================================================================

IRComponent* ir_component_deep_copy(IRComponent* src) {
    if (!src) return NULL;

    IRComponent* dest = ir_create_component(src->type);
    if (!dest) {
        return NULL;
    }

    // Copy primitive fields
    dest->selector_type = src->selector_type;
    dest->child_count = src->child_count;
    dest->child_capacity = src->child_count;
    dest->owner_instance_id = src->owner_instance_id;
    dest->is_disabled = src->is_disabled;
    // Note: dirty_flags now consolidated in layout_state, not copied here

    // Copy string fields
    if (src->tag) dest->tag = strdup(src->tag);
    if (src->css_class) dest->css_class = strdup(src->css_class);
    if (src->text_content) dest->text_content = strdup(src->text_content);
    if (src->text_expression) dest->text_expression = strdup(src->text_expression);
    if (src->custom_data) dest->custom_data = strdup(src->custom_data);
    if (src->component_ref) dest->component_ref = strdup(src->component_ref);
    if (src->component_props) dest->component_props = strdup(src->component_props);
    if (src->module_ref) dest->module_ref = strdup(src->module_ref);
    if (src->export_name) dest->export_name = strdup(src->export_name);
    if (src->scope) dest->scope = strdup(src->scope);
    if (src->visible_condition) dest->visible_condition = strdup(src->visible_condition);

    // Copy for_def if present (NEW unified field)
    if (src->for_def) {
        dest->for_def = ir_for_def_copy(src->for_def);
    }

    // Copy source_metadata
    if (src->source_metadata.generated_by) {
        dest->source_metadata.generated_by = strdup(src->source_metadata.generated_by);
    }
    dest->source_metadata.iteration_index = src->source_metadata.iteration_index;
    dest->source_metadata.is_template = src->source_metadata.is_template;
    dest->visible_when_true = src->visible_when_true;

    // Copy style if present
    if (src->style) {
        dest->style = ir_create_style();
        if (dest->style && src->style) {
            memcpy(dest->style, src->style, sizeof(IRStyle));
            // Deep copy style strings
            if (src->style->background_image) dest->style->background_image = strdup(src->style->background_image);
            if (src->style->background.var_name) dest->style->background.var_name = strdup(src->style->background.var_name);
            if (src->style->text_fill_color.var_name) dest->style->text_fill_color.var_name = strdup(src->style->text_fill_color.var_name);
            if (src->style->border_color.var_name) dest->style->border_color.var_name = strdup(src->style->border_color.var_name);
            if (src->style->font.family) dest->style->font.family = strdup(src->style->font.family);
            if (src->style->container_name) dest->style->container_name = strdup(src->style->container_name);

            // Copy gradient if present
            if (src->style->background.type == IR_COLOR_GRADIENT && src->style->background.data.gradient) {
                dest->style->background.data.gradient = malloc(sizeof(IRGradient));
                if (dest->style->background.data.gradient) {
                    memcpy(dest->style->background.data.gradient, src->style->background.data.gradient, sizeof(IRGradient));
                }
            }
        }
    }

    // Copy layout if present
    if (src->layout) {
        dest->layout = ir_create_layout();
        if (dest->layout && src->layout) {
            memcpy(dest->layout, src->layout, sizeof(IRLayout));
        }
    }

    // NOTE: Do NOT copy layout_state! For expanded copies need fresh layout calculation.
    dest->layout_state = NULL;

    // Copy events if present
    if (src->events) {
        IREvent** dest_next_ptr = &dest->events;
        IREvent* ev = src->events;
        while (ev) {
            IREvent* new_event = ir_create_event(ev->type, ev->logic_id, NULL);
            if (new_event) {
                if (ev->handler_source) {
                    new_event->handler_source = calloc(1, sizeof(IRHandlerSource));
                    if (new_event->handler_source) {
                        new_event->handler_source->language = ev->handler_source->language ? strdup(ev->handler_source->language) : NULL;
                        new_event->handler_source->code = ev->handler_source->code ? strdup(ev->handler_source->code) : NULL;
                        new_event->handler_source->file = ev->handler_source->file ? strdup(ev->handler_source->file) : NULL;
                        new_event->handler_source->line = ev->handler_source->line;
                        new_event->handler_source->uses_closures = ev->handler_source->uses_closures;
                        new_event->handler_source->closure_var_count = ev->handler_source->closure_var_count;

                        if (ev->handler_source->closure_vars && ev->handler_source->closure_var_count > 0) {
                            new_event->handler_source->closure_vars = calloc(ev->handler_source->closure_var_count, sizeof(char*));
                            for (int i = 0; i < ev->handler_source->closure_var_count; i++) {
                                if (ev->handler_source->closure_vars[i]) {
                                    new_event->handler_source->closure_vars[i] = strdup(ev->handler_source->closure_vars[i]);
                                }
                            }
                        }
                    }
                }
                *dest_next_ptr = new_event;
                dest_next_ptr = &new_event->next;
            }
            ev = ev->next;
        }
    }

    // Recursively copy children
    if (src->child_count > 0) {
        dest->children = calloc(src->child_count, sizeof(IRComponent*));
        if (dest->children) {
            for (uint32_t i = 0; i < src->child_count; i++) {
                dest->children[i] = ir_component_deep_copy(src->children[i]);
                if (dest->children[i]) {
                    dest->children[i]->parent = dest;
                }
            }
        } else {
            dest->child_count = 0;
        }
    }

    return dest;
}

// ============================================================================
// Binding Resolution
// ============================================================================

// Helper: Extract nested field from cJSON object (e.g., "item.nested.field")
static cJSON* get_nested_field(cJSON* obj, const char* path) {
    if (!obj || !path) return NULL;

    char* path_copy = strdup(path);
    char* token = strtok(path_copy, ".");
    cJSON* current = obj;

    while (token && current) {
        current = cJSON_GetObjectItem(current, token);
        token = strtok(NULL, ".");
    }

    free(path_copy);
    return current;
}

char* ir_for_resolve_binding(const char* expression, IRForContext* ctx) {
    if (!expression || !ctx) return NULL;

    // Handle index binding
    if (ctx->index_name && strcmp(expression, ctx->index_name) == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", ctx->current_index);
        return strdup(buf);
    }

    // Handle item field binding (e.g., "item.name" or "day.dayNumber")
    if (ctx->item_name && ctx->current_item) {
        size_t item_len = strlen(ctx->item_name);

        // Check if expression starts with item_name followed by '.'
        if (strncmp(expression, ctx->item_name, item_len) == 0 &&
            expression[item_len] == '.') {
            const char* field_path = expression + item_len + 1;
            cJSON* field = get_nested_field(ctx->current_item, field_path);

            if (field) {
                if (cJSON_IsString(field)) {
                    return strdup(field->valuestring);
                } else if (cJSON_IsNumber(field)) {
                    char buf[64];
                    double val = cJSON_GetNumberValue(field);
                    if (val == (int)val) {
                        snprintf(buf, sizeof(buf), "%d", (int)val);
                    } else {
                        snprintf(buf, sizeof(buf), "%g", val);
                    }
                    return strdup(buf);
                } else if (cJSON_IsBool(field)) {
                    return strdup(cJSON_IsTrue(field) ? "true" : "false");
                } else if (cJSON_IsNull(field)) {
                    return strdup("null");
                }
            }
        }

        // Direct item reference (when item is a primitive)
        if (strcmp(expression, ctx->item_name) == 0) {
            if (cJSON_IsString(ctx->current_item)) {
                return strdup(ctx->current_item->valuestring);
            } else if (cJSON_IsNumber(ctx->current_item)) {
                char buf[64];
                double val = cJSON_GetNumberValue(ctx->current_item);
                if (val == (int)val) {
                    snprintf(buf, sizeof(buf), "%d", (int)val);
                } else {
                    snprintf(buf, sizeof(buf), "%g", val);
                }
                return strdup(buf);
            }
        }
    }

    // Return expression as-is if can't resolve
    return strdup(expression);
}

// ============================================================================
// Property Setters
// ============================================================================

bool ir_for_set_property(IRComponent* component, const char* property_path, const char* value) {
    if (!component || !property_path || !value) return false;

    // text_content property
    if (strcmp(property_path, "text_content") == 0) {
        free(component->text_content);
        component->text_content = strdup(value);
        return true;
    }

    // style.opacity
    if (strcmp(property_path, "style.opacity") == 0) {
        if (!component->style) {
            component->style = ir_create_style();
        }
        if (component->style) {
            component->style->opacity = atof(value);
            return true;
        }
    }

    // style.background (hex color)
    if (strcmp(property_path, "style.background") == 0) {
        if (!component->style) {
            component->style = ir_create_style();
        }
        if (component->style) {
            // Parse hex color (e.g., "#FF0000")
            if (value[0] == '#' && strlen(value) >= 7) {
                unsigned int r, g, b;
                sscanf(value + 1, "%02x%02x%02x", &r, &g, &b);
                component->style->background.type = IR_COLOR_SOLID;
                component->style->background.data.r = r;
                component->style->background.data.g = g;
                component->style->background.data.b = b;
                component->style->background.data.a = 255;
                return true;
            }
        }
    }

    // custom_data
    if (strcmp(property_path, "custom_data") == 0) {
        free(component->custom_data);
        component->custom_data = strdup(value);
        return true;
    }

    return false;
}

// ============================================================================
// Binding Application
// ============================================================================

void ir_for_apply_bindings(IRComponent* component, IRForDef* def, IRForContext* ctx) {
    if (!component || !def || !ctx) return;

    // Apply each binding
    for (uint32_t i = 0; i < def->tmpl.binding_count; i++) {
        IRForBinding* binding = &def->tmpl.bindings[i];

        if (binding->is_computed) {
            // Computed bindings need expression evaluation (not implemented yet)
            // For now, skip computed bindings
            continue;
        }

        // Resolve binding value
        char* value = ir_for_resolve_binding(binding->source_expression, ctx);
        if (value) {
            ir_for_set_property(component, binding->target_property, value);
            free(value);
        }
    }
}

// ============================================================================
// Legacy Text Update (for backward compatibility)
// ============================================================================

static void update_for_component_text_recursive(IRComponent* component, cJSON* data_item, const char* item_name);

static void update_for_component_text_legacy(IRComponent* component, cJSON* data_item, const char* item_name) {
    if (!component || !data_item) return;

    // Skip arrays (nested For data)
    if (cJSON_IsArray(data_item)) {
        return;
    }

    // Get calendar-specific fields (legacy support)
    cJSON* day_number = cJSON_GetObjectItem(data_item, "dayNumber");
    cJSON* is_current_month = cJSON_GetObjectItem(data_item, "isCurrentMonth");
    cJSON* is_completed = cJSON_GetObjectItem(data_item, "isCompleted");
    cJSON* is_today = cJSON_GetObjectItem(data_item, "isToday");
    cJSON* date_str = cJSON_GetObjectItem(data_item, "date");
    cJSON* theme_color = cJSON_GetObjectItem(data_item, "themeColor");

    if ((component->type == IR_COMPONENT_BUTTON || component->type == IR_COMPONENT_TEXT) && day_number) {
        if (cJSON_IsNumber(day_number)) {
            int day_num = (int)cJSON_GetNumberValue(day_number);

            bool current_month = is_current_month && cJSON_IsTrue(is_current_month);
            bool completed = is_completed && cJSON_IsTrue(is_completed);
            bool today = is_today && cJSON_IsTrue(is_today);

            // Set text content (only show day number for current month)
            free(component->text_content);
            if (current_month) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", day_num);
                component->text_content = strdup(buf);
            } else {
                component->text_content = strdup("");
            }

            // Ensure style exists
            if (!component->style) {
                component->style = ir_create_style();
            }

            if (component->style) {
                // Set backgroundColor based on completion status
                if (completed) {
                    // Use theme color from day data for completed days
                    component->style->background.type = IR_COLOR_SOLID;
                    if (theme_color && cJSON_IsString(theme_color)) {
                        const char* hex = theme_color->valuestring;
                        if (hex[0] == '#' && strlen(hex) >= 7) {
                            unsigned int r, g, b;
                            sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
                            component->style->background.data.r = r;
                            component->style->background.data.g = g;
                            component->style->background.data.b = b;
                            component->style->background.data.a = 255;
                        }
                    } else {
                        // Fallback purple if no theme color
                        component->style->background.data.r = 0x6b;
                        component->style->background.data.g = 0x5b;
                        component->style->background.data.b = 0x95;
                        component->style->background.data.a = 255;
                    }
                } else if (!current_month) {
                    // Grayed out for non-current month
                    component->style->background.type = IR_COLOR_SOLID;
                    component->style->background.data.r = 0x2d;
                    component->style->background.data.g = 0x2d;
                    component->style->background.data.b = 0x2d;
                    component->style->background.data.a = 255;
                } else {
                    // Default background for current month (not completed)
                    component->style->background.type = IR_COLOR_SOLID;
                    component->style->background.data.r = 0x3d;
                    component->style->background.data.g = 0x3d;
                    component->style->background.data.b = 0x3d;
                    component->style->background.data.a = 255;
                }

                // Set border for today - use theme color
                if (today && current_month) {
                    component->style->border.width = 2;
                    component->style->border.color.type = IR_COLOR_SOLID;
                    if (theme_color && cJSON_IsString(theme_color)) {
                        const char* hex = theme_color->valuestring;
                        if (hex[0] == '#' && strlen(hex) >= 7) {
                            unsigned int r, g, b;
                            sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
                            component->style->border.color.data.r = r;
                            component->style->border.color.data.g = g;
                            component->style->border.color.data.b = b;
                            component->style->border.color.data.a = 255;
                        }
                    } else {
                        // Fallback blue if no theme color
                        component->style->border.color.data.r = 0x4a;
                        component->style->border.color.data.g = 0x90;
                        component->style->border.color.data.b = 0xe2;
                        component->style->border.color.data.a = 255;
                    }
                }

                // Set disabled state - future dates or non-current month
                bool is_future = false;
                if (date_str && cJSON_IsString(date_str) && strlen(date_str->valuestring) > 0) {
                    // Parse date and compare to today
                    int year, month, day;
                    if (sscanf(date_str->valuestring, "%d-%d-%d", &year, &month, &day) == 3) {
                        time_t now = time(NULL);
                        struct tm* today_tm = localtime(&now);
                        int today_year = today_tm->tm_year + 1900;
                        int today_month = today_tm->tm_mon + 1;
                        int today_day = today_tm->tm_mday;

                        if (year > today_year ||
                            (year == today_year && month > today_month) ||
                            (year == today_year && month == today_month && day > today_day)) {
                            is_future = true;
                        }
                    }
                }

                // Disabled if future OR not current month
                component->is_disabled = is_future || !current_month;

                // Visual feedback for disabled state (reduce opacity)
                if (component->is_disabled) {
                    component->style->opacity = 0.5f;
                } else {
                    component->style->opacity = 1.0f;
                }

                // Update custom_data with the actual date from this iteration
                // This is critical for For click handlers to know which day was clicked
                if (date_str && cJSON_IsString(date_str)) {
                    // Parse existing custom_data if present
                    cJSON* custom_json = component->custom_data ? cJSON_Parse(component->custom_data) : NULL;
                    if (!custom_json) {
                        custom_json = cJSON_CreateObject();
                    }

                    // Get or create the "data" object
                    cJSON* data_obj = cJSON_GetObjectItem(custom_json, "data");
                    if (!data_obj) {
                        data_obj = cJSON_CreateObject();
                        cJSON_AddItemToObject(custom_json, "data", data_obj);
                    }

                    // Update the date field
                    cJSON_DeleteItemFromObject(data_obj, "date");
                    cJSON_AddStringOrNull(data_obj, "date", date_str ? date_str->valuestring : NULL);

                    // Also add isCompleted for the handler to know current state
                    cJSON_DeleteItemFromObject(data_obj, "isCompleted");
                    cJSON_AddBoolToObject(data_obj, "isCompleted", completed);

                    // Serialize back and update component
                    char* new_custom_data = cJSON_PrintUnformatted(custom_json);
                    if (new_custom_data) {
                        free(component->custom_data);
                        component->custom_data = new_custom_data;
                    }
                    cJSON_Delete(custom_json);
                }
            }
        }
    }

    // Recursively update children
    for (uint32_t i = 0; i < component->child_count; i++) {
        if (component->children[i]) {
            update_for_component_text_recursive(component->children[i], data_item, item_name);
        }
    }
}

static void update_for_component_text_recursive(IRComponent* component, cJSON* data_item, const char* item_name) {
    update_for_component_text_legacy(component, data_item, item_name);
}

// ============================================================================
// Nested For Source Update
// ============================================================================

static void update_nested_for_source(IRComponent* component, cJSON* source_array) {
    if (!component || !source_array) return;

    // Handle For components
    if (component->type == IR_COMPONENT_FOR_LOOP) {
        // Update for_def if present
        if (component->for_def) {
            free(component->for_def->source.literal_json);
            component->for_def->source.literal_json = cJSON_PrintUnformatted(source_array);
            component->for_def->source.type = FOR_SOURCE_LITERAL;
        }
        return;
    }

    // Recurse into children
    for (uint32_t i = 0; i < component->child_count; i++) {
        update_nested_for_source(component->children[i], source_array);
    }
}

// ============================================================================
// Tree Manipulation
// ============================================================================

void ir_for_replace_in_parent(IRComponent* parent, int for_index,
                               IRComponent** expanded_children, uint32_t expanded_count) {
    if (!expanded_children || expanded_count == 0) return;

    if (!parent) {
        return;
    }

    // Save reference to the For component being replaced
    IRComponent* old_for = parent->children[for_index];

    // Allocate new children array
    int new_count = parent->child_count - 1 + expanded_count;
    IRComponent** new_children = calloc(new_count, sizeof(IRComponent*));
    if (!new_children) return;

    // Copy children before For
    int dest_idx = 0;
    for (int i = 0; i < for_index; i++) {
        new_children[dest_idx++] = parent->children[i];
    }

    // Add expanded children and fix parent pointers
    for (uint32_t i = 0; i < expanded_count; i++) {
        new_children[dest_idx++] = expanded_children[i];
        if (expanded_children[i]) {
            // Always update parent to actual parent (was temporarily set to For)
            expanded_children[i]->parent = parent;
        }
    }

    // Copy children after For
    for (int i = for_index + 1; i < (int)parent->child_count; i++) {
        new_children[dest_idx++] = parent->children[i];
    }

    free(parent->children);
    parent->children = new_children;
    parent->child_count = new_count;

    // Free the old For component
    if (old_for) {
        old_for->children = NULL;
        old_for->child_count = 0;
        ir_destroy_component(old_for);
    }
}

// ============================================================================
// Main Expansion Implementation
// ============================================================================

uint32_t ir_for_expand_single(IRComponent* for_comp, IRSourceStructures* source_structures, IRComponent*** out_children) {
    if (!for_comp || !out_children) return 0;
    *out_children = NULL;

    // Handle For components
    if (for_comp->type != IR_COMPONENT_FOR_LOOP) return 0;

    // Get source data
    cJSON* source_array = NULL;
    bool should_free_source = false;

    // Try new for_def first
    if (for_comp->for_def) {
        source_array = ir_for_get_source_data(for_comp->for_def);
        should_free_source = true;

        // If still NULL, try resolving as variable reference
        if (!source_array && source_structures) {
            source_array = ir_for_resolve_variable_source(for_comp->for_def, source_structures);
            should_free_source = true;
        }
    }

    // Fall back to legacy each_source field
    if (!source_array && for_comp->each_source) {
        // Skip runtime markers
        if (strcmp(for_comp->each_source, "__runtime__") == 0 ||
            strcmp(for_comp->each_source, "\"__runtime__\"") == 0) {
            return 0;
        }
        source_array = cJSON_Parse(for_comp->each_source);
        should_free_source = true;
    }

    // Fall back to custom_data (legacy)
    if (!source_array && for_comp->custom_data) {
        cJSON* custom_json = cJSON_Parse(for_comp->custom_data);
        if (custom_json) {
            cJSON* each_source_item = cJSON_GetObjectItem(custom_json, "each_source");
            if (each_source_item && cJSON_IsArray(each_source_item)) {
                source_array = cJSON_Duplicate(each_source_item, 1);
                should_free_source = true;
            } else if (each_source_item && cJSON_IsString(each_source_item)) {
                const char* marker = each_source_item->valuestring;
                if (strcmp(marker, "__runtime__") == 0) {
                    cJSON_Delete(custom_json);
                    return 0;
                }
            }
            cJSON_Delete(custom_json);
        }
    }

    if (!source_array || !cJSON_IsArray(source_array)) {
        if (should_free_source && source_array) cJSON_Delete(source_array);
        return 0;
    }

    int num_items = cJSON_GetArraySize(source_array);
    if (num_items == 0) {
        if (should_free_source) cJSON_Delete(source_array);
        return 0;
    }

    // Get template
    if (for_comp->child_count == 0 || !for_comp->children[0]) {
        if (should_free_source) cJSON_Delete(source_array);
        return 0;
    }

    IRComponent* tmpl = for_comp->children[0];

    // Prepare context - use for_def if available, fall back to legacy fields
    IRForContext ctx = {
        .data_array = source_array,
        .item_name = "item",
        .index_name = "index"
    };

    if (for_comp->for_def) {
        ctx.item_name = for_comp->for_def->item_name ? for_comp->for_def->item_name : "item";
        ctx.index_name = for_comp->for_def->index_name ? for_comp->for_def->index_name : "index";
    } else if (for_comp->each_item_name) {
        ctx.item_name = for_comp->each_item_name;
        ctx.index_name = for_comp->each_index_name ? for_comp->each_index_name : "index";
    }

    // Allocate children array
    IRComponent** new_children = calloc(num_items, sizeof(IRComponent*));
    if (!new_children) {
        if (should_free_source) cJSON_Delete(source_array);
        return 0;
    }

    uint32_t expanded_count = 0;

    for (int i = 0; i < num_items; i++) {
        IRComponent* copy = ir_component_deep_copy(tmpl);
        if (!copy) {
            continue;
        }

        copy->parent = for_comp;
        copy->source_metadata.iteration_index = i;

        cJSON* data_item = cJSON_GetArrayItem(source_array, i);
        ctx.current_index = i;
        ctx.current_item = data_item;

        if (data_item) {
            // Apply bindings if we have a for_def
            if (for_comp->for_def && for_comp->for_def->tmpl.binding_count > 0) {
                ir_for_apply_bindings(copy, for_comp->for_def, &ctx);
            }

            // Also apply legacy text updates for backward compatibility
            if (cJSON_IsObject(data_item)) {
                update_for_component_text_legacy(copy, data_item, ctx.item_name);
            } else if (cJSON_IsArray(data_item)) {
                // Nested array - propagate to nested For
                update_nested_for_source(copy, data_item);
            }
        }

        new_children[expanded_count++] = copy;
    }

    if (should_free_source) cJSON_Delete(source_array);

    *out_children = new_children;
    return expanded_count;
}

static void expand_for_with_parent_internal(IRComponent* component, IRSourceStructures* source_structures, IRComponent* parent, int child_index) {
    if (!component) return;

    // Handle For components
    if (component->type != IR_COMPONENT_FOR_LOOP) {
        // Not a For - process children
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (component->children[i]) {
                expand_for_with_parent_internal(component->children[i], source_structures, component, (int)i);
            }
        }
        return;
    }

    // Check if this is a runtime For loop (should not be expanded)
    if (component->for_def && !component->for_def->is_compile_time) {
        // Skip expansion for runtime loops - just process children
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (component->children[i]) {
                expand_for_with_parent_internal(component->children[i], source_structures, component, (int)i);
            }
        }
        return;
    }

    // Expand this For
    IRComponent** expanded_children = NULL;
    uint32_t expanded_count = ir_for_expand_single(component, source_structures, &expanded_children);

    if (expanded_count > 0 && expanded_children) {
        // Update component with expanded children
        component->children = expanded_children;
        component->child_count = expanded_count;

        // Expand nested For in each copy
        for (uint32_t i = 0; i < expanded_count; i++) {
            if (expanded_children[i]) {
                expand_for_with_parent_internal(expanded_children[i], source_structures, component, (int)i);
            }
        }

        if (parent) {
            // Normal case: replace For with children in parent
            ir_for_replace_in_parent(parent, child_index, expanded_children, expanded_count);
        }
        // else: Root-level For - keep children in the For as a wrapper
    } else if (component->child_count > 0 && parent) {
        // No data to expand - first recursively expand any nested For in template children
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (component->children[i]) {
                expand_for_with_parent_internal(component->children[i], source_structures, component, (int)i);
            }
        }
    }
}

void ir_for_expand_tree(IRComponent* root, IRSourceStructures* source_structures) {
    if (!root) {
        return;
    }

    expand_for_with_parent_internal(root, source_structures, NULL, 0);

    // Invalidate layout for entire tree after expansion
    // This ensures expanded children get proper layout computation
    ir_layout_invalidate_subtree(root);
}
