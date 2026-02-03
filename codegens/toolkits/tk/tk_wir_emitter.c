/**
 * @file tk_wir_emitter.c
 * @brief Tk Toolkit WIR Emitter Implementation
 *
 * Implementation of the WIRToolkitEmitter interface for Tk.
 */

#define _POSIX_C_SOURCE 200809L

#include "tk_wir_emitter.h"
#include "tk_codegen.h"
#include "../../codegen_common.h"
#include "../../common/color_utils.h"
#include "../../wir/wir.h"
#include "../../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for Tcl string quoting function
// This is defined in tcl_wir_emitter.c but we need it here for Tk-specific quoting
extern char* tcl_quote_string(const char* input);

// ============================================================================
// Static Emitter Instance
// ============================================================================

/**
 * Tk toolkit emitter instance.
 * Wraps the vtable and any context data.
 */
typedef struct {
    WIRToolkitEmitter base;  /**< Base emitter interface */
} TkWIREmitter;

// ============================================================================
// Widget Type Mapping
// ============================================================================

const char* tk_map_widget_type(const char* kir_type) {
    if (!kir_type) {
        return "frame";  // Default
    }

    if (strcmp(kir_type, "Button") == 0) return "button";
    if (strcmp(kir_type, "Text") == 0) return "label";
    if (strcmp(kir_type, "Input") == 0) return "entry";
    if (strcmp(kir_type, "Container") == 0 ||
        strcmp(kir_type, "Column") == 0 ||
        strcmp(kir_type, "Row") == 0 ||
        strcmp(kir_type, "Scroll") == 0) return "frame";
    if (strcmp(kir_type, "TextArea") == 0) return "text";
    if (strcmp(kir_type, "Checkbox") == 0) return "checkbutton";
    if (strcmp(kir_type, "Radio") == 0) return "radiobutton";
    if (strcmp(kir_type, "Image") == 0) return "label";
    if (strcmp(kir_type, "Canvas") == 0) return "canvas";
    if (strcmp(kir_type, "Select") == 0) return "ttk::combobox";
    if (strcmp(kir_type, "List") == 0) return "listbox";
    if (strcmp(kir_type, "Progress") == 0) return "ttk::progressbar";
    if (strcmp(kir_type, "Slider") == 0) return "scale";
    if (strcmp(kir_type, "Notebook") == 0) return "ttk::notebook";
    if (strcmp(kir_type, "Tree") == 0) return "ttk::treeview";

    return "frame";  // Default fallback
}

// ============================================================================
// Event Name Mapping
// ============================================================================

const char* tk_map_event_name(const char* event) {
    if (!event) {
        return "Button-1";  // Default
    }

    if (strcmp(event, "click") == 0) return "Button-1";
    if (strcmp(event, "doubleclick") == 0) return "Double-Button-1";
    if (strcmp(event, "rightclick") == 0) return "Button-3";
    if (strcmp(event, "keypress") == 0) return "Key";
    if (strcmp(event, "keyrelease") == 0) return "KeyRelease";
    if (strcmp(event, "focus") == 0) return "FocusIn";
    if (strcmp(event, "focusout") == 0) return "FocusOut";
    if (strcmp(event, "mouseenter") == 0) return "Enter";
    if (strcmp(event, "mouseleave") == 0) return "Leave";
    if (strcmp(event, "motion") == 0) return "Motion";
    if (strcmp(event, "buttonpress") == 0) return "ButtonPress";
    if (strcmp(event, "buttonrelease") == 0) return "ButtonRelease";

    return event;  // Return as-is if unknown
}

// ============================================================================
// Emitter VTable Implementation
// ============================================================================

/**
 * Emit widget creation command.
 */
static bool tk_emit_widget_creation(StringBuilder* sb, WIRWidget* widget) {
    if (!sb || !widget || !widget->id) {
        fprintf(stderr, "[ERROR] tk_emit_widget_creation: Invalid parameters (sb=%p, widget=%p, widget->id=%p)\n",
                sb, widget, widget ? widget->id : NULL);
        return false;
    }

    // Map widget type to Tk type
    const char* tk_type = tk_map_widget_type(widget->kir_type);
    if (!tk_type) {
        fprintf(stderr, "[ERROR] tk_map_widget_type returned NULL for kir_type=%s\n",
                widget->kir_type ? widget->kir_type : "NULL");
        return false;
    }

    // Generate widget creation command
    sb_append_fmt(sb, "%s .%s\n", tk_type, widget->id);
    return true;
}

/**
 * Emit property assignment.
 */
static bool tk_emit_property_assignment(StringBuilder* sb, const char* widget_id,
                                         const char* property_name, cJSON* value,
                                         const char* widget_type) {
    if (!sb || !widget_id || !property_name || !value) {
        return false;
    }

    // Filter properties that are not valid for certain widget types
    // Frames don't support text, foreground, or placeholder by default
    if (widget_type && strcmp(widget_type, "frame") == 0) {
        if (strcmp(property_name, "text") == 0 ||
            strcmp(property_name, "foreground") == 0 ||
            strcmp(property_name, "placeholder") == 0) {
            return true;  // Skip these properties for frames
        }
    }

    // Handle different value types
    if (cJSON_IsString(value)) {
        const char* val_str = value->valuestring;
        if (!val_str) {
            fprintf(stderr, "[ERROR] Property '%s' has NULL valuestring\n", property_name);
            return false;
        }
        if (strcmp(property_name, "text") == 0) {
            char* quoted = tcl_quote_string(val_str);
            sb_append_fmt(sb, ".%s configure -text {%s}\n", widget_id, val_str);
            if (quoted) free(quoted);
        } else if (strcmp(property_name, "background") == 0) {
            char* norm_color = color_normalize_for_tcl(val_str);
            if (norm_color) {
                sb_append_fmt(sb, ".%s configure -background %s\n", widget_id, norm_color);
                free(norm_color);
            }
        } else if (strcmp(property_name, "foreground") == 0) {
            char* norm_color = color_normalize_for_tcl(val_str);
            if (norm_color) {
                sb_append_fmt(sb, ".%s configure -foreground %s\n", widget_id, norm_color);
                free(norm_color);
            }
        } else if (strcmp(property_name, "placeholder") == 0) {
            // Entry widget placeholder
            char* quoted = tcl_quote_string(val_str);
            sb_append_fmt(sb, ".%s configure -placeholder {%s}\n", widget_id, val_str);
            if (quoted) free(quoted);
        } else {
            // Generic string property
            char* quoted = tcl_quote_string(val_str);
            sb_append_fmt(sb, ".%s configure -%s {%s}\n", widget_id, property_name, val_str);
            if (quoted) free(quoted);
        }
    } else if (cJSON_IsNumber(value)) {
        if (strcmp(property_name, "width") == 0 || strcmp(property_name, "height") == 0) {
            sb_append_fmt(sb, ".%s configure -%s %d\n", widget_id, property_name, value->valueint);
        } else if (strcmp(property_name, "value") == 0) {
            sb_append_fmt(sb, ".%s configure -%s %d\n", widget_id, property_name, value->valueint);
        } else {
            sb_append_fmt(sb, ".%s configure -%s %d\n", widget_id, property_name, value->valueint);
        }
    } else if (cJSON_IsBool(value)) {
        if (strcmp(property_name, "enabled") == 0) {
            sb_append_fmt(sb, ".%s configure -state %s\n", widget_id,
                         cJSON_IsTrue(value) ? "normal" : "disabled");
        } else if (strcmp(property_name, "visible") == 0) {
            // Map visible to normal state (Tk doesn't have a direct visible property)
            sb_append_fmt(sb, ".%s configure -state %s\n", widget_id,
                         cJSON_IsTrue(value) ? "normal" : "disabled");
        }
    } else if (cJSON_IsObject(value)) {
        // Handle nested objects like font, border
        if (strcmp(property_name, "font") == 0) {
            cJSON* family = cJSON_GetObjectItem(value, "family");
            cJSON* size = cJSON_GetObjectItem(value, "size");
            if (family && cJSON_IsString(family) && size && cJSON_IsNumber(size)) {
                sb_append_fmt(sb, ".%s configure -font {%s %d}\n",
                             widget_id, family->valuestring, (int)size->valuedouble);
            }
        } else if (strcmp(property_name, "border") == 0) {
            cJSON* width_val = cJSON_GetObjectItem(value, "width");
            cJSON* color = cJSON_GetObjectItem(value, "color");
            if (width_val && cJSON_IsNumber(width_val)) {
                sb_append_fmt(sb, ".%s configure -borderwidth %d\n",
                             widget_id, (int)width_val->valuedouble);
            }
            if (color && cJSON_IsString(color)) {
                char* norm_color = color_normalize_for_tcl(color->valuestring);
                if (norm_color) {
                    sb_append_fmt(sb, ".%s configure -highlightbackground %s\n",
                                 widget_id, norm_color);
                    free(norm_color);
                }
            }
        } else if (strcmp(property_name, "width") == 0 || strcmp(property_name, "height") == 0) {
            cJSON* value_num = cJSON_GetObjectItem(value, "value");
            if (value_num && cJSON_IsNumber(value_num)) {
                sb_append_fmt(sb, ".%s configure -%s %d\n",
                             widget_id, property_name, (int)value_num->valuedouble);
            }
        }
    }

    return true;
}

/**
 * Emit layout command.
 */
static bool tk_emit_layout_command(StringBuilder* sb, WIRWidget* widget) {
    if (!sb || !widget || !widget->json) {
        return false;
    }

    cJSON* layout = cJSON_GetObjectItem(widget->json, "layout");
    if (!layout) {
        return false;  // Root widget has no layout
    }

    cJSON* type = cJSON_GetObjectItem(layout, "type");
    cJSON* options = cJSON_GetObjectItem(layout, "options");
    if (!type || !options) {
        return false;
    }

    const char* layout_type = type->valuestring;

    if (strcmp(layout_type, "pack") == 0) {
        // Pack layout
        sb_append_fmt(sb, "pack .%s", widget->id);

        cJSON* side = cJSON_GetObjectItem(options, "side");
        if (side && cJSON_IsString(side)) {
            sb_append_fmt(sb, " -side %s", side->valuestring);
        }

        cJSON* fill = cJSON_GetObjectItem(options, "fill");
        if (fill && cJSON_IsString(fill)) {
            sb_append_fmt(sb, " -fill %s", fill->valuestring);
        }

        cJSON* expand = cJSON_GetObjectItem(options, "expand");
        if (expand && cJSON_IsBool(expand) && cJSON_IsTrue(expand)) {
            sb_append_fmt(sb, " -expand yes");
        }

        cJSON* anchor = cJSON_GetObjectItem(options, "anchor");
        if (anchor && cJSON_IsString(anchor)) {
            sb_append_fmt(sb, " -anchor %s", anchor->valuestring);
        }

        cJSON* padx = cJSON_GetObjectItem(options, "padx");
        if (padx && cJSON_IsNumber(padx)) {
            sb_append_fmt(sb, " -padx %d", padx->valueint);
        }

        cJSON* pady = cJSON_GetObjectItem(options, "pady");
        if (pady && cJSON_IsNumber(pady)) {
            sb_append_fmt(sb, " -pady %d", pady->valueint);
        }

        sb_append_fmt(sb, "\n");

    } else if (strcmp(layout_type, "grid") == 0) {
        // Grid layout
        sb_append_fmt(sb, "grid .%s", widget->id);

        cJSON* row = cJSON_GetObjectItem(options, "row");
        if (row && cJSON_IsNumber(row)) {
            sb_append_fmt(sb, " -row %d", row->valueint);
        }

        cJSON* column = cJSON_GetObjectItem(options, "column");
        if (column && cJSON_IsNumber(column)) {
            sb_append_fmt(sb, " -column %d", column->valueint);
        }

        cJSON* rowspan = cJSON_GetObjectItem(options, "rowspan");
        if (rowspan && cJSON_IsNumber(rowspan) && rowspan->valueint > 1) {
            sb_append_fmt(sb, " -rowspan %d", rowspan->valueint);
        }

        cJSON* columnspan = cJSON_GetObjectItem(options, "columnspan");
        if (columnspan && cJSON_IsNumber(columnspan) && columnspan->valueint > 1) {
            sb_append_fmt(sb, " -columnspan %d", columnspan->valueint);
        }

        cJSON* sticky = cJSON_GetObjectItem(options, "sticky");
        if (sticky && cJSON_IsString(sticky) && strlen(sticky->valuestring) > 0) {
            sb_append_fmt(sb, " -sticky %s", sticky->valuestring);
        }

        sb_append_fmt(sb, "\n");

    } else if (strcmp(layout_type, "place") == 0) {
        // Place layout (absolute positioning)
        sb_append_fmt(sb, "place .%s", widget->id);

        cJSON* x = cJSON_GetObjectItem(options, "x");
        if (x && cJSON_IsNumber(x)) {
            sb_append_fmt(sb, " -x %d", x->valueint);
        }

        cJSON* y = cJSON_GetObjectItem(options, "y");
        if (y && cJSON_IsNumber(y)) {
            sb_append_fmt(sb, " -y %d", y->valueint);
        }

        cJSON* width = cJSON_GetObjectItem(options, "width");
        if (width && cJSON_IsNumber(width)) {
            sb_append_fmt(sb, " -width %d", width->valueint);
        }

        cJSON* height = cJSON_GetObjectItem(options, "height");
        if (height && cJSON_IsNumber(height)) {
            sb_append_fmt(sb, " -height %d", height->valueint);
        }

        cJSON* anchor = cJSON_GetObjectItem(options, "anchor");
        if (anchor && cJSON_IsString(anchor)) {
            sb_append_fmt(sb, " -anchor %s", anchor->valuestring);
        }

        sb_append_fmt(sb, "\n");
    }

    return true;
}

/**
 * Emit event binding.
 */
static bool tk_emit_event_binding(StringBuilder* sb, WIRHandler* handler) {
    if (!sb || !handler || !handler->widget_id || !handler->event_type) {
        return false;
    }

    // Get implementations
    cJSON* impls = cJSON_GetObjectItem(handler->json, "implementations");
    if (!impls) {
        return false;
    }

    cJSON* tcl_impl = cJSON_GetObjectItem(impls, "tcl");
    if (!tcl_impl || !cJSON_IsString(tcl_impl)) {
        // No Tcl implementation - skip
        return true;
    }

    // Map event name to Tk event syntax
    const char* tk_event = tk_map_event_name(handler->event_type);

    // Output binding
    sb_append_fmt(sb, "bind .%s <%s> {%s}\n", handler->widget_id, tk_event, tcl_impl->valuestring);

    return true;
}

/**
 * Free the emitter.
 */
static void tk_emitter_free(WIRToolkitEmitter* emitter) {
    if (!emitter) {
        return;
    }

    free(emitter);
}

// ============================================================================
// Public API
// ============================================================================

WIRToolkitEmitter* tk_wir_toolkit_emitter_create(void) {
    TkWIREmitter* emitter = calloc(1, sizeof(TkWIREmitter));
    if (!emitter) {
        return NULL;
    }

    // Set up vtable
    emitter->base.name = "tk";
    emitter->base.emit_widget_creation = tk_emit_widget_creation;
    emitter->base.emit_property_assignment = tk_emit_property_assignment;
    emitter->base.emit_layout_command = tk_emit_layout_command;
    emitter->base.emit_event_binding = tk_emit_event_binding;
    emitter->base.map_widget_type = tk_map_widget_type;
    emitter->base.map_event_name = tk_map_event_name;
    emitter->base.free = tk_emitter_free;

    return (WIRToolkitEmitter*)emitter;
}

void tk_wir_toolkit_emitter_free(WIRToolkitEmitter* emitter) {
    if (!emitter) {
        return;
    }

    if (emitter->free) {
        emitter->free(emitter);
    } else {
        free(emitter);
    }
}

// ============================================================================
// Registration
// ============================================================================

// Static instance for registration
static WIRToolkitEmitter* g_tk_toolkit_emitter = NULL;

void tk_wir_emitter_init(void) {
    if (g_tk_toolkit_emitter) {
        return;  // Already initialized
    }

    g_tk_toolkit_emitter = tk_wir_toolkit_emitter_create();
    if (g_tk_toolkit_emitter) {
        wir_composer_register_toolkit("tk", g_tk_toolkit_emitter);
    }
}

void tk_wir_emitter_cleanup(void) {
    if (g_tk_toolkit_emitter) {
        tk_wir_toolkit_emitter_free(g_tk_toolkit_emitter);
        g_tk_toolkit_emitter = NULL;
    }
}
