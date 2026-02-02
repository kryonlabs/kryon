/**
 * @file wir_builder.c
 * @brief WIR Builder Implementation
 *
 * Implementation of KIR to WIR transformation.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#define _POSIX_C_SOURCE 200809L

#include "wir_builder.h"
#include "../codegen_common.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// Forward Declarations
// ============================================================================

static const char* extract_main_axis_alignment(cJSON* component);
static const char* extract_cross_axis_alignment(cJSON* component);
static WIRPackOptions* wir_compute_pack_options_with_alignment(
    cJSON* component, const char* parent_type, const char* main_align,
    const char* cross_align, int child_index, int total_children);

// For Loop Expansion
static void expand_for_loops_in_json(cJSON* component, cJSON* source_structures, bool verbose);
static void expand_single_for_loop_json(cJSON* for_component, cJSON* source_structures, bool verbose);
static void apply_bindings_to_component(cJSON* component, const char* item_name,
                                       cJSON* data_item, int index);
static char* resolve_binding_expression(const char* expr, cJSON* data_item,
                                       const char* item_name, int index);
static cJSON* resolve_variable_source(const char* var_name, cJSON* source_structures);
static char* get_nested_field_value(cJSON* obj, const char* field_path);

// ============================================================================
// Builder Context
// ============================================================================

WIRBuilderContext* wir_builder_create(WIRRoot* root) {
    if (!root) {
        wir_error("Cannot create builder context with NULL root");
        return NULL;
    }

    WIRBuilderContext* ctx = calloc(1, sizeof(WIRBuilderContext));
    if (!ctx) {
        wir_error("Failed to allocate builder context");
        return NULL;
    }

    ctx->root = root;
    ctx->widget_id_counter = 0;
    ctx->handler_id_counter = 0;
    ctx->root_background = root->background;
    ctx->verbose = false;

    // Create handler tracker
    ctx->handlers = codegen_handler_tracker_create();
    if (!ctx->handlers) {
        free(ctx);
        wir_error("Failed to create handler tracker");
        return NULL;
    }

    return ctx;
}

void wir_builder_free(WIRBuilderContext* ctx) {
    if (!ctx) return;

    if (ctx->handlers) {
        codegen_handler_tracker_free(ctx->handlers);
    }

    free(ctx);
}

// ============================================================================
// Main Build Functions
// ============================================================================

WIRRoot* wir_build_from_kir(const char* kir_json, bool verbose) {
    if (!kir_json) {
        wir_error("NULL KIR JSON provided");
        return NULL;
    }

    // Parse KIR JSON
    cJSON* kir_root = codegen_parse_kir_json(kir_json);
    if (!kir_root) {
        wir_error("Failed to parse KIR JSON");
        return NULL;
    }

    // Build from cJSON
    WIRRoot* root = wir_build_from_cJSON(kir_root, verbose);
    if (!root) {
        cJSON_Delete(kir_root);
        return NULL;
    }

    // Store the KIR cJSON root in the WIRRoot so it can be freed later
    // Note: If root->json was already set by wir_root_from_cJSON, we use that
    // Otherwise, we store kir_root for cleanup
    if (!root->json) {
        root->json = kir_root;
    } else {
        // root->json was already set (likely a borrowed reference)
        // We need to delete kir_root since it won't be cleaned up otherwise
        cJSON_Delete(kir_root);
    }

    return root;
}

WIRRoot* wir_build_from_cJSON(cJSON* kir_root, bool verbose) {
    if (!kir_root) {
        wir_error("NULL KIR root provided");
        return NULL;
    }

    // Create WIR root
    WIRRoot* root = wir_root_from_cJSON(kir_root);
    if (!root) {
        wir_error("Failed to create WIR root");
        return NULL;
    }

    // Create builder context
    WIRBuilderContext* ctx = wir_builder_create(root);
    if (!ctx) {
        wir_root_free(root);
        return NULL;
    }

    ctx->verbose = verbose;

    // Extract source_structures for variable resolution
    cJSON* source_structures = cJSON_GetObjectItem(kir_root, "source_structures");

    // Get root component - try "root" first, then "app", then "component"
    cJSON* root_component_json = cJSON_GetObjectItem(kir_root, "root");
    if (!root_component_json) {
        root_component_json = cJSON_GetObjectItem(kir_root, "app");
    }
    if (!root_component_json) {
        root_component_json = cJSON_GetObjectItem(kir_root, "component");
    }
    if (!root_component_json) {
        wir_error("No root component found in KIR");
        wir_builder_free(ctx);
        wir_root_free(root);
        return NULL;
    }

    // EXPAND FOR LOOPS (compile-time expansion)
    expand_for_loops_in_json(root_component_json, source_structures, verbose);

    // Transform the KIR component tree
    if (!wir_builder_transform_root(ctx, kir_root)) {
        wir_error("Failed to transform KIR to WIR");
        wir_builder_free(ctx);
        wir_root_free(root);
        return NULL;
    }

    // Extract data bindings from logic block
    cJSON* logic_block = codegen_extract_logic_block(kir_root);
    if (logic_block) {
        wir_builder_extract_data_bindings(ctx, logic_block);
    }

    // Free context
    wir_builder_free(ctx);

    return root;
}

bool wir_builder_transform_root(WIRBuilderContext* ctx, cJSON* kir_root) {
    if (!ctx || !kir_root) {
        wir_error("Invalid arguments to transform_root");
        return false;
    }

    // Extract root component from KIR
    cJSON* root_component = codegen_extract_root_component(kir_root);
    if (!root_component) {
        wir_error("No root component found in KIR");
        return false;
    }

    // Transform root component (no parent, no parent background)
    WIRWidget* root_widget = wir_builder_transform_component(
        ctx, root_component, NULL, ctx->root_background, NULL, 0, 1);
    if (!root_widget) {
        wir_error("Failed to transform root component");
        return false;
    }

    // Serialize root widget to JSON and add to widgets array
    cJSON* widget_json = wir_widget_to_json(root_widget);
    if (!widget_json) {
        wir_widget_free(root_widget);
        return false;
    }

    cJSON_AddItemToArray(ctx->root->widgets, widget_json);

    // Free the widget wrapper (JSON is now owned by the array)
    free((void*)root_widget->id);  // Free our copy of ID
    free(root_widget);

    return true;
}

// ============================================================================
// Component Transformation
// ============================================================================

WIRWidget* wir_builder_transform_component(WIRBuilderContext* ctx,
                                             cJSON* component,
                                             const char* parent_id,
                                             const char* parent_bg,
                                             cJSON* parent_component,
                                             int child_index,
                                             int total_children) {
    if (!ctx || !component) {
        return NULL;
    }

    // Create widget wrapper
    WIRWidget* widget = wir_widget_create(component, parent_id, &ctx->widget_id_counter);
    if (!widget) {
        return NULL;
    }

    // Resolve background color (with inheritance)
    widget->background = wir_resolve_background(component, parent_bg);

    // Normalize properties
    cJSON* width = cJSON_GetObjectItem(component, "width");
    if (width) {
        widget->width = wir_normalize_size(width);
    }

    cJSON* height = cJSON_GetObjectItem(component, "height");
    if (height) {
        widget->height = wir_normalize_size(height);
    }

    widget->font = wir_normalize_font(component);
    widget->border = wir_normalize_border(component);

    // Compute layout options (only if not root)
    if (parent_id && parent_component) {
        // Get parent type
        cJSON* parent_type_json = cJSON_GetObjectItem(parent_component, "type");
        const char* parent_type = parent_type_json && cJSON_IsString(parent_type_json)
                                  ? parent_type_json->valuestring : "Container";

        // Extract parent alignment properties
        const char* main_align = extract_main_axis_alignment(parent_component);
        const char* cross_align = extract_cross_axis_alignment(parent_component);

        // Compute layout with alignment info
        widget->layout = wir_builder_compute_layout_with_alignment(
            ctx, widget, component, parent_type, main_align, cross_align,
            child_index, total_children);
    }

    // Extract event handlers
    wir_builder_extract_handlers(ctx, widget, component);

    // Recursively process children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        widget->children_ids = cJSON_CreateArray();

        int idx = 0;
        int total = cJSON_GetArraySize(children);

        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            // Transform child component
            WIRWidget* child_widget = wir_builder_transform_component(
                ctx, child, widget->id, widget->background, component, idx, total);

            if (child_widget) {
                // Serialize child widget to JSON
                cJSON* child_json = wir_widget_to_json(child_widget);
                if (child_json) {
                    cJSON_AddItemToArray(ctx->root->widgets, child_json);

                    // Add child ID to parent's children list
                    cJSON_AddItemToArray(widget->children_ids, cJSON_CreateString(child_widget->id));
                }

                // Free child widget wrapper
                free((void*)child_widget->id);
                free(child_widget);

                idx++;
            }
        }
    }

    return widget;
}

// ============================================================================
// Property Normalization
// ============================================================================

WIRSize* wir_normalize_size(cJSON* value) {
    if (!value) return NULL;

    WIRSize* size = calloc(1, sizeof(WIRSize));
    if (!size) return NULL;

    if (cJSON_IsNumber(value)) {
        size->value = value->valuedouble;
        size->unit = "px";  // Default unit for numbers
    } else if (cJSON_IsString(value)) {
        const char* str = value->valuestring;

        // Parse value
        if (sscanf(str, "%lf", &size->value) == 1) {
            // Determine unit
            if (strstr(str, "px")) {
                size->unit = "px";
            } else if (strstr(str, "%")) {
                size->unit = "%";
            } else if (strstr(str, "em")) {
                size->unit = "em";
            } else {
                size->unit = "px";  // Default
            }
        } else {
            // Failed to parse
            free(size);
            return NULL;
        }
    } else {
        free(size);
        return NULL;
    }

    return size;
}

WIRFont* wir_normalize_font(cJSON* component) {
    if (!component) return NULL;

    cJSON* font = cJSON_GetObjectItem(component, "font");
    if (!font) return NULL;

    WIRFont* wir_font = calloc(1, sizeof(WIRFont));
    if (!wir_font) return NULL;

    // Font family
    if (cJSON_IsString(font)) {
        wir_font->family = font->valuestring;
        wir_font->size = 12.0;
        wir_font->size_unit = "px";
    } else if (cJSON_IsObject(font)) {
        cJSON* family = cJSON_GetObjectItem(font, "family");
        wir_font->family = (family && cJSON_IsString(family)) ? family->valuestring : NULL;

        cJSON* size = cJSON_GetObjectItem(font, "size");
        if (size) {
            if (cJSON_IsNumber(size)) {
                wir_font->size = size->valuedouble;
                wir_font->size_unit = "px";
            } else if (cJSON_IsString(size)) {
                sscanf(size->valuestring, "%lf", &wir_font->size);
                wir_font->size_unit = "px";  // TODO: Parse unit from string
            }
        }

        cJSON* weight = cJSON_GetObjectItem(font, "weight");
        wir_font->weight = (weight && cJSON_IsString(weight)) ? weight->valuestring : NULL;

        cJSON* style = cJSON_GetObjectItem(font, "style");
        wir_font->style = (style && cJSON_IsString(style)) ? style->valuestring : NULL;
    } else {
        free(wir_font);
        return NULL;
    }

    return wir_font;
}

WIRBorder* wir_normalize_border(cJSON* component) {
    if (!component) return NULL;

    cJSON* border = cJSON_GetObjectItem(component, "border");
    if (!border) return NULL;

    // Border can be a simple string (color) or object with full properties
    WIRBorder* wir_border = calloc(1, sizeof(WIRBorder));
    if (!wir_border) return NULL;

    if (cJSON_IsString(border)) {
        wir_border->color = border->valuestring;
        wir_border->width = 1;
        wir_border->style = "solid";
    } else if (cJSON_IsObject(border)) {
        cJSON* width = cJSON_GetObjectItem(border, "width");
        wir_border->width = (width && cJSON_IsNumber(width)) ? width->valueint : 1;

        cJSON* color = cJSON_GetObjectItem(border, "color");
        wir_border->color = (color && cJSON_IsString(color)) ? color->valuestring : NULL;

        cJSON* style = cJSON_GetObjectItem(border, "style");
        wir_border->style = (style && cJSON_IsString(style)) ? style->valuestring : "solid";
    } else {
        free(wir_border);
        return NULL;
    }

    return wir_border;
}

const char* wir_resolve_background(cJSON* component, const char* parent_bg) {
    if (!component) return parent_bg;

    cJSON* bg = cJSON_GetObjectItem(component, "background");
    if (bg && cJSON_IsString(bg)) {
        // Check if transparent
        if (!codegen_is_transparent_color(bg->valuestring)) {
            return bg->valuestring;
        }
    }

    // Inherit from parent
    return parent_bg;
}

// ============================================================================
// Layout Resolution
// ============================================================================

/**
 * Extract mainAxisAlignment from container component.
 */
static const char* extract_main_axis_alignment(cJSON* component) {
    if (!component) return NULL;

    // Try various property names for main axis alignment
    const char* align_names[] = {"mainAxisAlignment", "justifyContent",
                                   "justify", "mainAlign", NULL};

    for (int i = 0; align_names[i]; i++) {
        cJSON* align = cJSON_GetObjectItem(component, align_names[i]);
        if (align && cJSON_IsString(align)) {
            return align->valuestring;
        }
    }

    return NULL;
}

/**
 * Extract crossAxisAlignment from container component.
 */
static const char* extract_cross_axis_alignment(cJSON* component) {
    if (!component) return NULL;

    // Try various property names for cross axis alignment
    const char* align_names[] = {"crossAxisAlignment", "alignItems",
                                   "align", "crossAlign", NULL};

    for (int i = 0; align_names[i]; i++) {
        cJSON* align = cJSON_GetObjectItem(component, align_names[i]);
        if (align && cJSON_IsString(align)) {
            return align->valuestring;
        }
    }

    return NULL;
}

WIRLayoutOptions* wir_builder_compute_layout(WIRBuilderContext* ctx,
                                               WIRWidget* widget,
                                               cJSON* component,
                                               const char* parent_type,
                                               int child_index,
                                               int total_children) {
    if (!component) return NULL;

    // Detect layout type from component properties
    CodegenLayoutType layout_type = codegen_detect_layout_type(component);

    WIRLayoutOptions* layout = calloc(1, sizeof(WIRLayoutOptions));
    if (!layout) return NULL;

    switch (layout_type) {
        case CODEGEN_LAYOUT_PACK:
            layout->type = WIR_LAYOUT_PACK;
            layout->pack = *wir_compute_pack_options(component, parent_type, child_index, total_children);
            break;

        case CODEGEN_LAYOUT_GRID:
            layout->type = WIR_LAYOUT_GRID;
            layout->grid = *wir_compute_grid_options(component);
            break;

        case CODEGEN_LAYOUT_PLACE:
            layout->type = WIR_LAYOUT_PLACE;
            layout->place = *wir_compute_place_options(component);
            break;

        default:
            layout->type = WIR_LAYOUT_PACK;
            layout->pack = *wir_compute_pack_options(component, parent_type, child_index, total_children);
            break;
    }

    return layout;
}

WIRPackOptions* wir_compute_pack_options(cJSON* component,
                                           const char* parent_type,
                                           int child_index,
                                           int total_children) {
    static WIRPackOptions options;
    memset(&options, 0, sizeof(options));

    // Handle Center container specially
    if (parent_type && strcmp(parent_type, "Center") == 0) {
        options.expand = true;
        options.anchor = "center";
        options.fill = "both";
        return &options;
    }

    // Determine side based on parent type
    if (parent_type && strcmp(parent_type, "Row") == 0) {
        options.side = "left";
    } else if (parent_type && strcmp(parent_type, "Column") == 0) {
        options.side = "top";
    } else {
        options.side = "top";  // Default
    }

    // Default values
    options.fill = "none";
    options.expand = false;
    options.anchor = NULL;
    options.padx = 0;
    options.pady = 0;

    // Check for explicit padding
    cJSON* padding = cJSON_GetObjectItem(component, "padding");
    if (padding && cJSON_IsNumber(padding)) {
        options.padx = padding->valueint;
        options.pady = padding->valueint;
    } else if (padding && cJSON_IsObject(padding)) {
        cJSON* padx = cJSON_GetObjectItem(padding, "x");
        cJSON* pady = cJSON_GetObjectItem(padding, "y");
        if (padx && cJSON_IsNumber(padx)) {
            options.padx = padx->valueint;
        }
        if (pady && cJSON_IsNumber(pady)) {
            options.pady = pady->valueint;
        }
    }

    // Check for explicit gap (as padding for children)
    cJSON* gap = cJSON_GetObjectItem(component, "gap");
    if (gap && cJSON_IsNumber(gap)) {
        // Add gap as padding between items
        // Only add to trailing edge (not first item)
        if (child_index > 0) {
            if (parent_type && strcmp(parent_type, "Row") == 0) {
                options.padx = gap->valueint;  // Horizontal gap
            } else if (parent_type && strcmp(parent_type, "Column") == 0) {
                options.pady = gap->valueint;  // Vertical gap
            }
        }
    }

    return &options;
}

WIRGridOptions* wir_compute_grid_options(cJSON* component) {
    static WIRGridOptions options;
    memset(&options, 0, sizeof(options));

    cJSON* row = cJSON_GetObjectItem(component, "row");
    if (row && cJSON_IsNumber(row)) {
        options.row = row->valueint;
    }

    cJSON* column = cJSON_GetObjectItem(component, "column");
    if (column && cJSON_IsNumber(column)) {
        options.column = column->valueint;
    }

    cJSON* rowspan = cJSON_GetObjectItem(component, "rowspan");
    if (rowspan && cJSON_IsNumber(rowspan)) {
        options.rowspan = rowspan->valueint;
    } else {
        options.rowspan = 1;
    }

    cJSON* columnspan = cJSON_GetObjectItem(component, "columnspan");
    if (columnspan && cJSON_IsNumber(columnspan)) {
        options.columnspan = columnspan->valueint;
    } else {
        options.columnspan = 1;
    }

    options.sticky = "";  // Default

    return &options;
}

WIRPlaceOptions* wir_compute_place_options(cJSON* component) {
    static WIRPlaceOptions options;
    memset(&options, 0, sizeof(options));

    cJSON* left = cJSON_GetObjectItem(component, "left");
    if (left && cJSON_IsNumber(left)) {
        options.x = left->valueint;
    }

    cJSON* top = cJSON_GetObjectItem(component, "top");
    if (top && cJSON_IsNumber(top)) {
        options.y = top->valueint;
    }

    cJSON* width = cJSON_GetObjectItem(component, "width");
    if (width && cJSON_IsNumber(width)) {
        options.width = width->valueint;
    }

    cJSON* height = cJSON_GetObjectItem(component, "height");
    if (height && cJSON_IsNumber(height)) {
        options.height = height->valueint;
    }

    options.anchor = "center";  // Default

    return &options;
}

WIRLayoutOptions* wir_builder_compute_layout_with_alignment(
    WIRBuilderContext* ctx,
    WIRWidget* widget,
    cJSON* component,
    const char* parent_type,
    const char* main_align,
    const char* cross_align,
    int child_index,
    int total_children) {
    if (!component) return NULL;

    // Detect layout type from component properties
    CodegenLayoutType layout_type = codegen_detect_layout_type(component);

    WIRLayoutOptions* layout = calloc(1, sizeof(WIRLayoutOptions));
    if (!layout) return NULL;

    switch (layout_type) {
        case CODEGEN_LAYOUT_PACK:
            layout->type = WIR_LAYOUT_PACK;
            {
                WIRPackOptions* opts = wir_compute_pack_options_with_alignment(
                    component, parent_type, main_align, cross_align, child_index, total_children);
                if (opts) layout->pack = *opts;
            }
            break;

        case CODEGEN_LAYOUT_GRID:
            layout->type = WIR_LAYOUT_GRID;
            {
                WIRGridOptions* opts = wir_compute_grid_options(component);
                if (opts) layout->grid = *opts;
            }
            break;

        case CODEGEN_LAYOUT_PLACE:
            layout->type = WIR_LAYOUT_PLACE;
            {
                WIRPlaceOptions* opts = wir_compute_place_options(component);
                if (opts) layout->place = *opts;
            }
            break;

        default:
            layout->type = WIR_LAYOUT_PACK;
            {
                WIRPackOptions* opts = wir_compute_pack_options_with_alignment(
                    component, parent_type, main_align, cross_align, child_index, total_children);
                if (opts) layout->pack = *opts;
            }
            break;
    }

    return layout;
}

/**
 * Compute pack layout options with alignment support.
 */
static WIRPackOptions* wir_compute_pack_options_with_alignment(
    cJSON* component,
    const char* parent_type,
    const char* main_align,
    const char* cross_align,
    int child_index,
    int total_children) {

    static WIRPackOptions options;
    memset(&options, 0, sizeof(options));

    // Handle Center container specially
    if (parent_type && strcmp(parent_type, "Center") == 0) {
        options.expand = true;
        options.anchor = "center";
        options.fill = "both";
        return &options;
    }

    // Determine side based on parent type
    if (parent_type && strcmp(parent_type, "Row") == 0) {
        options.side = "left";
    } else if (parent_type && strcmp(parent_type, "Column") == 0) {
        options.side = "top";
    } else {
        options.side = "top";  // Default
    }

    // Default values
    options.fill = "none";
    options.expand = false;
    options.anchor = NULL;
    options.padx = 0;
    options.pady = 0;

    // Handle mainAxisAlignment (justifyContent)
    if (main_align) {
        if (strcmp(main_align, "center") == 0) {
            options.expand = true;
            options.anchor = "center";
        } else if (strcmp(main_align, "end") == 0) {
            // Reverse side for end alignment
            if (strcmp(parent_type, "Row") == 0) {
                options.side = "right";
            } else if (strcmp(parent_type, "Column") == 0) {
                options.side = "bottom";
            }
        } else if (strcmp(main_align, "space-between") == 0 ||
                   strcmp(main_align, "spaceAround") == 0 ||
                   strcmp(main_align, "spaceEvenly") == 0) {
            options.fill = "both";
            options.expand = true;
        }
    }

    // Handle crossAxisAlignment (alignItems)
    if (cross_align && parent_type) {
        if (strcmp(cross_align, "center") == 0) {
            if (!options.anchor) {
                options.anchor = "center";
            }
        } else if (strcmp(cross_align, "start") == 0) {
            if (strcmp(parent_type, "Row") == 0) {
                if (!options.anchor) options.anchor = "n";
            } else if (strcmp(parent_type, "Column") == 0) {
                if (!options.anchor) options.anchor = "w";
            }
        } else if (strcmp(cross_align, "end") == 0) {
            if (strcmp(parent_type, "Row") == 0) {
                if (!options.anchor) options.anchor = "s";
            } else if (strcmp(parent_type, "Column") == 0) {
                if (!options.anchor) options.anchor = "e";
            }
        } else if (strcmp(cross_align, "stretch") == 0) {
            if (strcmp(parent_type, "Row") == 0) {
                options.fill = "y";  // Fill vertically for Row
            } else if (strcmp(parent_type, "Column") == 0) {
                options.fill = "x";  // Fill horizontally for Column
            }
        }
    }

    // Check for explicit padding
    cJSON* padding = cJSON_GetObjectItem(component, "padding");
    if (padding && cJSON_IsNumber(padding)) {
        options.padx = padding->valueint;
        options.pady = padding->valueint;
    } else if (padding && cJSON_IsObject(padding)) {
        cJSON* padx = cJSON_GetObjectItem(padding, "x");
        cJSON* pady = cJSON_GetObjectItem(padding, "y");
        if (padx && cJSON_IsNumber(padx)) {
            options.padx = padx->valueint;
        }
        if (pady && cJSON_IsNumber(pady)) {
            options.pady = pady->valueint;
        }
    }

    // Check for explicit gap (as padding for children)
    cJSON* gap = cJSON_GetObjectItem(component, "gap");
    if (gap && cJSON_IsNumber(gap) && child_index > 0) {
        // Add gap as padding between items (not on first child)
        if (parent_type && strcmp(parent_type, "Row") == 0) {
            options.padx = gap->valueint;  // Horizontal gap
        } else if (parent_type && strcmp(parent_type, "Column") == 0) {
            options.pady = gap->valueint;  // Vertical gap
        }
    }

    return &options;
}

// ============================================================================
// Event Handler Extraction
// ============================================================================

bool wir_builder_extract_handlers(WIRBuilderContext* ctx,
                                    WIRWidget* widget,
                                    cJSON* component) {
    if (!ctx || !widget || !component) return false;

    cJSON* events = cJSON_GetObjectItem(component, "events");
    if (!events || !cJSON_IsArray(events)) {
        return true;  // No events is OK
    }

    widget->events = cJSON_CreateArray();

    cJSON* event = NULL;
    cJSON_ArrayForEach(event, events) {
        cJSON* handler_name = cJSON_GetObjectItem(event, "handler");
        if (!handler_name || !cJSON_IsString(handler_name)) {
            continue;
        }

        // Check if handler was already generated
        if (codegen_handler_tracker_contains(ctx->handlers, handler_name->valuestring)) {
            // Add reference to existing handler
            // For now, just add the handler ID
            cJSON* handler_ref = cJSON_CreateString(handler_name->valuestring);
            cJSON_AddItemToArray(widget->events, handler_ref);
            continue;
        }

        // Create new handler
        WIRHandler* handler = wir_handler_create(event, widget->id, &ctx->handler_id_counter);
        if (!handler) {
            continue;
        }

        // Mark handler as generated
        codegen_handler_tracker_mark(ctx->handlers, handler_name->valuestring);

        // Serialize handler to JSON
        cJSON* handler_json = wir_handler_to_json(handler);
        if (handler_json) {
            cJSON_AddItemToArray(ctx->root->handlers, handler_json);

            // Add reference to widget's events
            cJSON* handler_ref = cJSON_CreateString(handler->id);
            cJSON_AddItemToArray(widget->events, handler_ref);
        }

        // Free handler wrapper
        free((void*)handler->id);
        free(handler);
    }

    return true;
}

char* wir_generate_handler_code(cJSON* handler_json, const char* language) {
    // Placeholder - actual implementations will be in emitter code
    (void)handler_json;
    (void)language;
    return NULL;
}

// ============================================================================
// Data Binding Extraction
// ============================================================================

bool wir_builder_extract_data_bindings(WIRBuilderContext* ctx, cJSON* logic_block) {
    if (!ctx || !logic_block) return false;

    // TODO: Extract data bindings from logic block
    // For now, just return success
    return true;
}

// ============================================================================
// JSON Serialization
// ============================================================================

cJSON* wir_widget_to_json(WIRWidget* widget) {
    if (!widget) return NULL;

    cJSON* json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "id", widget->id);
    cJSON_AddStringToObject(json, "tk_type", widget->widget_type);
    if (widget->kir_type) {
        cJSON_AddStringToObject(json, "kir_type", widget->kir_type);
    }

    // Properties
    cJSON* props = cJSON_CreateObject();

    if (widget->text) {
        cJSON_AddStringToObject(props, "text", widget->text);
    }
    if (widget->background) {
        cJSON_AddStringToObject(props, "background", widget->background);
    }
    if (widget->foreground) {
        cJSON_AddStringToObject(props, "foreground", widget->foreground);
    }
    if (widget->image) {
        cJSON_AddStringToObject(props, "image", widget->image);
    }

    if (widget->width) {
        cJSON_AddItemToObject(props, "width", wir_size_to_json(widget->width));
    }
    if (widget->height) {
        cJSON_AddItemToObject(props, "height", wir_size_to_json(widget->height));
    }
    if (widget->font) {
        cJSON_AddItemToObject(props, "font", wir_font_to_json(widget->font));
    }
    if (widget->border) {
        cJSON_AddItemToObject(props, "border", wir_border_to_json(widget->border));
    }

    cJSON_AddItemToObject(json, "properties", props);

    // Layout
    if (widget->layout) {
        cJSON* layout = wir_layout_to_json(widget->layout);
        if (widget->parent_id) {
            cJSON_AddStringToObject(layout, "parent", widget->parent_id);
        }
        cJSON_AddItemToObject(json, "layout", layout);
    }

    // Children
    if (widget->children_ids) {
        cJSON_AddItemToObject(json, "children", cJSON_Duplicate(widget->children_ids, true));
    }

    // Events
    if (widget->events) {
        cJSON_AddItemToObject(json, "events", cJSON_Duplicate(widget->events, true));
    }

    return json;
}

cJSON* wir_handler_to_json(WIRHandler* handler) {
    if (!handler) return NULL;

    cJSON* json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "id", handler->id);
    cJSON_AddStringToObject(json, "event_type", handler->event_type);
    if (handler->widget_id) {
        cJSON_AddStringToObject(json, "widget_id", handler->widget_id);
    }

    if (handler->implementations) {
        cJSON_AddItemToObject(json, "implementations", cJSON_Duplicate(handler->implementations, true));
    }

    return json;
}

cJSON* wir_size_to_json(WIRSize* size) {
    if (!size) return NULL;

    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "value", size->value);
    if (size->unit) {
        cJSON_AddStringToObject(json, "unit", size->unit);
    }
    return json;
}

cJSON* wir_font_to_json(WIRFont* font) {
    if (!font) return NULL;

    cJSON* json = cJSON_CreateObject();

    if (font->family) {
        cJSON_AddStringToObject(json, "family", font->family);
    }
    cJSON_AddNumberToObject(json, "size", font->size);
    if (font->size_unit) {
        cJSON_AddStringToObject(json, "size_unit", font->size_unit);
    }
    if (font->weight) {
        cJSON_AddStringToObject(json, "weight", font->weight);
    }
    if (font->style) {
        cJSON_AddStringToObject(json, "style", font->style);
    }

    return json;
}

cJSON* wir_border_to_json(WIRBorder* border) {
    if (!border) return NULL;

    cJSON* json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "width", border->width);
    if (border->color) {
        cJSON_AddStringToObject(json, "color", border->color);
    }
    if (border->style) {
        cJSON_AddStringToObject(json, "style", border->style);
    }

    return json;
}

cJSON* wir_layout_to_json(WIRLayoutOptions* layout) {
    if (!layout) return NULL;

    cJSON* json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "type", wir_layout_type_to_string(layout->type));

    cJSON* options = cJSON_CreateObject();

    switch (layout->type) {
        case WIR_LAYOUT_PACK: {
            cJSON_AddStringToObject(options, "side", layout->pack.side);
            if (layout->pack.fill) {
                cJSON_AddStringToObject(options, "fill", layout->pack.fill);
            }
            if (layout->pack.expand) {
                cJSON_AddBoolToObject(options, "expand", true);
            }
            if (layout->pack.anchor) {
                cJSON_AddStringToObject(options, "anchor", layout->pack.anchor);
            }
            if (layout->pack.padx > 0) {
                cJSON_AddNumberToObject(options, "padx", layout->pack.padx);
            }
            if (layout->pack.pady > 0) {
                cJSON_AddNumberToObject(options, "pady", layout->pack.pady);
            }
            break;
        }
        case WIR_LAYOUT_GRID: {
            cJSON_AddNumberToObject(options, "row", layout->grid.row);
            cJSON_AddNumberToObject(options, "column", layout->grid.column);
            if (layout->grid.rowspan > 1) {
                cJSON_AddNumberToObject(options, "rowspan", layout->grid.rowspan);
            }
            if (layout->grid.columnspan > 1) {
                cJSON_AddNumberToObject(options, "columnspan", layout->grid.columnspan);
            }
            if (layout->grid.sticky && strlen(layout->grid.sticky) > 0) {
                cJSON_AddStringToObject(options, "sticky", layout->grid.sticky);
            }
            break;
        }
        case WIR_LAYOUT_PLACE: {
            cJSON_AddNumberToObject(options, "x", layout->place.x);
            cJSON_AddNumberToObject(options, "y", layout->place.y);
            if (layout->place.width > 0) {
                cJSON_AddNumberToObject(options, "width", layout->place.width);
            }
            if (layout->place.height > 0) {
                cJSON_AddNumberToObject(options, "height", layout->place.height);
            }
            if (layout->place.anchor) {
                cJSON_AddStringToObject(options, "anchor", layout->place.anchor);
            }
            break;
        }
        default:
            break;
    }

    cJSON_AddItemToObject(json, "options", options);

    return json;
}

// ============================================================================
// For Loop Expansion
// ============================================================================

static void expand_for_loops_in_json(cJSON* component, cJSON* source_structures, bool verbose) {
    if (!component) return;

    // Get component type
    cJSON* type_json = cJSON_GetObjectItem(component, "type");
    if (!type_json || !cJSON_IsString(type_json)) return;

    const char* type = type_json->valuestring;

    // Check if this is a For loop
    if (strcmp(type, "For") == 0 || strcmp(type, "ForEach") == 0) {
        // Expand this For loop
        expand_single_for_loop_json(component, source_structures, verbose);
        return;  // Don't recurse - expansion replaces the For node
    }

    // Not a For loop - recurse into children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            expand_for_loops_in_json(child, source_structures, verbose);
        }
    }
}

static void expand_single_for_loop_json(cJSON* for_component, cJSON* source_structures, bool verbose) {
    // Get for_def structure (KIR uses "for" key)
    cJSON* for_def = cJSON_GetObjectItem(for_component, "for");
    if (!for_def) {
        if (verbose) {
            fprintf(stderr, "[WIR] WARNING: For loop missing for_def structure\n");
        }
        return;
    }

    // Get item name and source
    cJSON* item_name_json = cJSON_GetObjectItem(for_def, "item_name");
    cJSON* source_json = cJSON_GetObjectItem(for_def, "source");
    cJSON* is_compile_time_json = cJSON_GetObjectItem(for_def, "is_compile_time");

    if (!item_name_json || !cJSON_IsString(item_name_json)) {
        if (verbose) {
            fprintf(stderr, "[WIR] WARNING: For loop missing item_name\n");
        }
        return;
    }

    const char* item_name = item_name_json->valuestring;

    // Check if this is a compile-time loop (should expand)
    bool is_compile_time = is_compile_time_json ? cJSON_IsTrue(is_compile_time_json) : true;

    if (!is_compile_time) {
        if (verbose) {
            fprintf(stderr, "[WIR] WARNING: Skipping runtime For loop (item=%s)\n", item_name);
        }
        return;
    }

    // Get source data
    cJSON* source_data = NULL;
    if (source_json) {
        cJSON* literal_json = cJSON_GetObjectItem(source_json, "literal_json");
        cJSON* expression = cJSON_GetObjectItem(source_json, "expression");

        if (literal_json && cJSON_IsString(literal_json)) {
            // Parse literal JSON array
            source_data = cJSON_Parse(literal_json->valuestring);
        } else if (expression && cJSON_IsString(expression)) {
            // Resolve from source_structures (const_declarations)
            source_data = resolve_variable_source(expression->valuestring, source_structures);
        }
    }

    if (!source_data || !cJSON_IsArray(source_data)) {
        if (verbose) {
            fprintf(stderr, "[WIR] WARNING: No source data for For loop (item=%s)\n", item_name);
        }
        if (source_data) cJSON_Delete(source_data);
        return;
    }

    // Get template (first child)
    cJSON* children = cJSON_GetObjectItem(for_component, "children");
    if (!children || !cJSON_IsArray(children) || cJSON_GetArraySize(children) == 0) {
        if (verbose) {
            fprintf(stderr, "[WIR] WARNING: For loop has no template children (item=%s)\n", item_name);
        }
        cJSON_Delete(source_data);
        return;
    }

    cJSON* template = cJSON_GetArrayItem(children, 0);
    if (!template) {
        if (verbose) {
            fprintf(stderr, "[WIR] WARNING: For loop template is NULL\n");
        }
        cJSON_Delete(source_data);
        return;
    }

    // Expand: create a copy of template for each item
    int num_items = cJSON_GetArraySize(source_data);
    cJSON* expanded_children = cJSON_CreateArray();

    for (int i = 0; i < num_items; i++) {
        cJSON* data_item = cJSON_GetArrayItem(source_data, i);
        if (!data_item) {
            if (verbose) {
                fprintf(stderr, "[WIR] WARNING: data_item at index %d is NULL\n", i);
            }
            continue;
        }

        cJSON* copy = cJSON_Duplicate(template, 1);
        if (!copy) {
            if (verbose) {
                fprintf(stderr, "[WIR] WARNING: Failed to duplicate template at index %d\n", i);
            }
            continue;
        }

        // Apply bindings to the copy
        apply_bindings_to_component(copy, item_name, data_item, i);

        cJSON_AddItemToArray(expanded_children, copy);
    }

    // Replace For component's type with Container (make it a regular container)
    cJSON* type_str = cJSON_CreateString("Container");
    if (type_str) {
        cJSON_ReplaceItemInObject(for_component, "type", type_str);
    }

    // Replace children with expanded children
    cJSON_ReplaceItemInObject(for_component, "children", expanded_children);

    // Remove for_def (no longer needed) - KIR uses "for" key
    cJSON_DeleteItemFromObject(for_component, "for");

    cJSON_Delete(source_data);
}

static void apply_bindings_to_component(cJSON* component, const char* item_name,
                                       cJSON* data_item, int index) {
    if (!component || !data_item) return;

    // First, check for property_bindings and apply them
    cJSON* property_bindings = cJSON_GetObjectItem(component, "property_bindings");
    if (property_bindings) {
        // Iterate through the property_bindings object to find each property name
        // Store properties to update in a list to avoid modifying while iterating
        struct {
            char* prop_name;
            char* resolved_value;
        } updates[32];
        int update_count = 0;

        cJSON* prop_name_item = NULL;
        cJSON_ArrayForEach(prop_name_item, property_bindings) {
            // prop_name_item->string contains the property name (key)
            // prop_name_item itself is the binding info object (value)
            if (!prop_name_item || !prop_name_item->string) continue;
            if (update_count >= 32) break;

            const char* prop_name = prop_name_item->string;  // This is the property name
            cJSON* binding_info = prop_name_item;  // This is the binding object

            cJSON* source_expr = cJSON_GetObjectItem(binding_info, "source_expr");
            if (source_expr && cJSON_IsString(source_expr)) {
                const char* expr = source_expr->valuestring;

                // Check if this is a binding expression we need to resolve
                if (strncmp(expr, item_name, strlen(item_name)) == 0) {
                    char* resolved = resolve_binding_expression(expr, data_item, item_name, index);
                    if (resolved && strcmp(resolved, expr) != 0) {
                        // Store for later update
                        updates[update_count].prop_name = strdup(prop_name);
                        updates[update_count].resolved_value = resolved;
                        update_count++;
                    } else {
                        if (resolved) free(resolved);
                    }
                }
            }
        }

        // Now apply the updates
        for (int i = 0; i < update_count; i++) {
            cJSON* replacement = cJSON_CreateString(updates[i].resolved_value);
            if (replacement) {
                cJSON_ReplaceItemInObject(component, updates[i].prop_name, replacement);
            }
            free(updates[i].prop_name);
            free(updates[i].resolved_value);
        }
    }

    // Remove property_bindings after applying (no longer needed)
    cJSON_DeleteItemFromObject(component, "property_bindings");

    // Recursively apply to children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            apply_bindings_to_component(child, item_name, data_item, index);
        }
    }
}

static char* resolve_binding_expression(const char* expr, cJSON* data_item,
                                       const char* item_name, int index) {
    if (!expr) return NULL;

    // Check for index reference
    if (strcmp(expr, "index") == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", index);
        return strdup(buf);
    }

    // Check for item field reference (e.g., "item.name")
    if (strncmp(expr, item_name, strlen(item_name)) == 0) {
        if (expr[strlen(item_name)] == '.') {
            // Field access: item.name
            const char* field = expr + strlen(item_name) + 1;
            return get_nested_field_value(data_item, field);
        } else if (expr[strlen(item_name)] == '[') {
            // Array access: item.colors[0]
            const char* bracket = expr + strlen(item_name);
            return get_nested_field_value(data_item, bracket);
        } else if (expr[strlen(item_name)] == '\0') {
            // Direct item reference (primitive array)
            if (cJSON_IsString(data_item)) {
                return strdup(data_item->valuestring);
            } else if (cJSON_IsNumber(data_item)) {
                char buf[64];
                double val = cJSON_GetNumberValue(data_item);
                snprintf(buf, sizeof(buf), "%g", val);
                return strdup(buf);
            }
        }
    }

    // Can't resolve - return expression as-is
    return strdup(expr);
}

static char* get_nested_field_value(cJSON* obj, const char* field_path) {
    if (!obj || !field_path) return NULL;

    // Parse field path (supports "name", "colors[0]", "colors.0")
    char path[256];
    strncpy(path, field_path, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    cJSON* current = obj;
    char* token = path;
    char* rest = NULL;

    while (token && current) {
        // Check for array access [N]
        char* bracket = strchr(token, '[');
        if (bracket) {
            *bracket = '\0';  // Null-terminate field name
            int index = atoi(bracket + 1);

            // Get array field
            if (strlen(token) > 0) {
                current = cJSON_GetObjectItem(current, token);
            }

            // Get array element
            if (current && cJSON_IsArray(current)) {
                current = cJSON_GetArrayItem(current, index);
            } else {
                return NULL;
            }
            break;
        }

        // Regular field access
        rest = strchr(token, '.');
        if (rest) {
            *rest = '\0';
            rest++;
        }

        current = cJSON_GetObjectItem(current, token);
        token = rest;
    }

    if (!current) return NULL;

    // Convert to string
    if (cJSON_IsString(current)) {
        return strdup(current->valuestring);
    } else if (cJSON_IsNumber(current)) {
        char buf[64];
        double val = cJSON_GetNumberValue(current);
        snprintf(buf, sizeof(buf), "%g", val);
        return strdup(buf);
    } else if (cJSON_IsTrue(current)) {
        return strdup("true");
    } else if (cJSON_IsFalse(current)) {
        return strdup("false");
    }

    return NULL;
}

static cJSON* resolve_variable_source(const char* var_name, cJSON* source_structures) {
    if (!var_name || !source_structures) return NULL;

    // Get const_declarations array
    cJSON* const_decls = cJSON_GetObjectItem(source_structures, "const_declarations");
    if (!const_decls || !cJSON_IsArray(const_decls)) {
        return NULL;
    }

    // Find the variable
    cJSON* decl = NULL;
    cJSON_ArrayForEach(decl, const_decls) {
        cJSON* name = cJSON_GetObjectItem(decl, "name");
        if (name && cJSON_IsString(name) && strcmp(name->valuestring, var_name) == 0) {
            // Found it - parse the value
            // KIR uses "value_json" key
            cJSON* value = cJSON_GetObjectItem(decl, "value_json");
            if (value && cJSON_IsString(value)) {
                return cJSON_Parse(value->valuestring);
            }
            // Fallback to "value" key for compatibility
            value = cJSON_GetObjectItem(decl, "value");
            if (value && cJSON_IsString(value)) {
                return cJSON_Parse(value->valuestring);
            }
        }
    }

    return NULL;
}
