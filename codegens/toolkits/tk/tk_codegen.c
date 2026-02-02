/**
 * @file tk_codegen.c
 * @brief Tk Toolkit Code Generator Implementation
 *
 * Extracted from tcltk/tcltk_from_wir.c to provide Tk-specific widget generation.
 * This implements the toolkit side of the language+toolkit separation.
 */

#define _POSIX_C_SOURCE 200809L

#include "tk_codegen.h"
#include "../../wir/wir.h"
#include "../../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration from tcltk codegen (color normalization)
extern char* tcltk_normalize_color(const char* color);

// ============================================================================
// Tk Widget Type Mapping
// ============================================================================

/**
 * Map KIR component types to Tk widget types
 */
static const char* map_widget_type_to_tk(const char* kir_type) {
    if (!kir_type) return "frame";  // Default

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
// Tk Widget Generation
// ============================================================================

/**
 * Generate Tk widget creation command
 * @param output Output file
 * @param widget WIR widget
 * @return true on success, false on failure
 */
bool tk_generate_widget_creation(FILE* output, WIRWidget* widget) {
    if (!output || !widget || !widget->id) {
        return false;
    }

    // Map widget type to Tk type
    const char* tk_type = map_widget_type_to_tk(widget->widget_type);

    // Generate widget creation command
    fprintf(output, "%s .%s\n", tk_type, widget->id);

    return true;
}

/**
 * Generate Tk property assignment
 * @param output Output file
 * @param widget_id Widget ID
 * @param property Property name
 * @param value Property value (cJSON)
 * @return true on success, false on failure
 */
bool tk_generate_property_assignment(FILE* output, const char* widget_id,
                                     const char* property, cJSON* value) {
    if (!output || !widget_id || !property || !value) {
        return false;
    }

    // Handle different value types
    if (cJSON_IsString(value)) {
        if (strcmp(property, "text") == 0) {
            fprintf(output, ".%s configure -text {%s}\n", widget_id, value->valuestring);
        } else if (strcmp(property, "background") == 0) {
            char* norm_color = tcltk_normalize_color(value->valuestring);
            if (norm_color) {
                fprintf(output, ".%s configure -background %s\n", widget_id, norm_color);
                free(norm_color);
            }
        } else if (strcmp(property, "foreground") == 0) {
            char* norm_color = tcltk_normalize_color(value->valuestring);
            if (norm_color) {
                fprintf(output, ".%s configure -foreground %s\n", widget_id, norm_color);
                free(norm_color);
            }
        } else if (strcmp(property, "placeholder") == 0) {
            // Entry widget placeholder
            fprintf(output, ".%s configure -placeholder {%s}\n", widget_id, value->valuestring);
        }
    } else if (cJSON_IsNumber(value)) {
        if (strcmp(property, "width") == 0 || strcmp(property, "height") == 0) {
            fprintf(output, ".%s configure -%s %d\n", widget_id, property, value->valueint);
        } else if (strcmp(property, "value") == 0) {
            fprintf(output, ".%s configure -%s %d\n", widget_id, property, value->valueint);
        }
    } else if (cJSON_IsBool(value)) {
        if (strcmp(property, "enabled") == 0) {
            fprintf(output, ".%s configure -state %s\n", widget_id,
                    cJSON_IsTrue(value) ? "normal" : "disabled");
        } else if (strcmp(property, "visible") == 0) {
            // Map visible to normal state (Tk doesn't have a direct visible property)
            fprintf(output, ".%s configure -state %s\n", widget_id,
                    cJSON_IsTrue(value) ? "normal" : "disabled");
        }
    } else if (cJSON_IsObject(value)) {
        // Handle nested objects like font, border
        if (strcmp(property, "font") == 0) {
            cJSON* family = cJSON_GetObjectItem(value, "family");
            cJSON* size = cJSON_GetObjectItem(value, "size");
            if (family && cJSON_IsString(family) && size && cJSON_IsNumber(size)) {
                fprintf(output, ".%s configure -font {%s %d}\n",
                        widget_id, family->valuestring, (int)size->valuedouble);
            }
        } else if (strcmp(property, "border") == 0) {
            cJSON* width_val = cJSON_GetObjectItem(value, "width");
            cJSON* color = cJSON_GetObjectItem(value, "color");
            if (width_val && cJSON_IsNumber(width_val)) {
                fprintf(output, ".%s configure -borderwidth %d\n",
                        widget_id, (int)width_val->valuedouble);
            }
            if (color && cJSON_IsString(color)) {
                char* norm_color = tcltk_normalize_color(color->valuestring);
                if (norm_color) {
                    fprintf(output, ".%s configure -highlightbackground %s\n",
                            widget_id, norm_color);
                    free(norm_color);
                }
            }
        } else if (strcmp(property, "width") == 0 || strcmp(property, "height") == 0) {
            cJSON* value_num = cJSON_GetObjectItem(value, "value");
            if (value_num && cJSON_IsNumber(value_num)) {
                fprintf(output, ".%s configure -%s %d\n",
                        widget_id, property, (int)value_num->valuedouble);
            }
        }
    }

    return true;
}

/**
 * Generate Tk layout command
 * @param output Output file
 * @param layout WIR layout options (extracted from widget json)
 * @return true on success, false on failure
 */
bool tk_generate_layout_command(FILE* output, void* layout_ptr) {
    if (!output || !layout_ptr) {
        return false;
    }

    // Cast back to cJSON (we're passing widget->json->layout)
    cJSON* layout = (cJSON*)layout_ptr;

    cJSON* type = cJSON_GetObjectItem(layout, "type");
    cJSON* options = cJSON_GetObjectItem(layout, "options");
    if (!type || !options) {
        return false;
    }

    const char* layout_type = type->valuestring;

    if (strcmp(layout_type, "pack") == 0) {
        // Pack layout
        fprintf(output, "pack configure .widget");  // Widget ID will be substituted

        cJSON* side = cJSON_GetObjectItem(options, "side");
        if (side && cJSON_IsString(side)) {
            fprintf(output, " -side %s", side->valuestring);
        }

        cJSON* fill = cJSON_GetObjectItem(options, "fill");
        if (fill && cJSON_IsString(fill)) {
            fprintf(output, " -fill %s", fill->valuestring);
        }

        cJSON* expand = cJSON_GetObjectItem(options, "expand");
        if (expand && cJSON_IsBool(expand) && cJSON_IsTrue(expand)) {
            fprintf(output, " -expand yes");
        }

        cJSON* anchor = cJSON_GetObjectItem(options, "anchor");
        if (anchor && cJSON_IsString(anchor)) {
            fprintf(output, " -anchor %s", anchor->valuestring);
        }

        cJSON* padx = cJSON_GetObjectItem(options, "padx");
        if (padx && cJSON_IsNumber(padx)) {
            fprintf(output, " -padx %d", padx->valueint);
        }

        cJSON* pady = cJSON_GetObjectItem(options, "pady");
        if (pady && cJSON_IsNumber(pady)) {
            fprintf(output, " -pady %d", pady->valueint);
        }

        fprintf(output, "\n");

    } else if (strcmp(layout_type, "grid") == 0) {
        // Grid layout
        fprintf(output, "grid configure .widget");

        cJSON* row = cJSON_GetObjectItem(options, "row");
        if (row && cJSON_IsNumber(row)) {
            fprintf(output, " -row %d", row->valueint);
        }

        cJSON* column = cJSON_GetObjectItem(options, "column");
        if (column && cJSON_IsNumber(column)) {
            fprintf(output, " -column %d", column->valueint);
        }

        cJSON* rowspan = cJSON_GetObjectItem(options, "rowspan");
        if (rowspan && cJSON_IsNumber(rowspan) && rowspan->valueint > 1) {
            fprintf(output, " -rowspan %d", rowspan->valueint);
        }

        cJSON* columnspan = cJSON_GetObjectItem(options, "columnspan");
        if (columnspan && cJSON_IsNumber(columnspan) && columnspan->valueint > 1) {
            fprintf(output, " -columnspan %d", columnspan->valueint);
        }

        cJSON* sticky = cJSON_GetObjectItem(options, "sticky");
        if (sticky && cJSON_IsString(sticky) && strlen(sticky->valuestring) > 0) {
            fprintf(output, " -sticky %s", sticky->valuestring);
        }

        fprintf(output, "\n");

    } else if (strcmp(layout_type, "place") == 0) {
        // Place layout (absolute positioning)
        fprintf(output, "place .widget");

        cJSON* x = cJSON_GetObjectItem(options, "x");
        if (x && cJSON_IsNumber(x)) {
            fprintf(output, " -x %d", x->valueint);
        }

        cJSON* y = cJSON_GetObjectItem(options, "y");
        if (y && cJSON_IsNumber(y)) {
            fprintf(output, " -y %d", y->valueint);
        }

        cJSON* width = cJSON_GetObjectItem(options, "width");
        if (width && cJSON_IsNumber(width)) {
            fprintf(output, " -width %d", width->valueint);
        }

        cJSON* height = cJSON_GetObjectItem(options, "height");
        if (height && cJSON_IsNumber(height)) {
            fprintf(output, " -height %d", height->valueint);
        }

        cJSON* anchor = cJSON_GetObjectItem(options, "anchor");
        if (anchor && cJSON_IsString(anchor)) {
            fprintf(output, " -anchor %s", anchor->valuestring);
        }

        fprintf(output, "\n");
    }

    return true;
}

/**
 * Generate Tk event binding
 * @param output Output file
 * @param widget_id Widget ID
 * @param event Event name
 * @param handler Handler code
 * @return true on success, false on failure
 */
bool tk_generate_event_binding(FILE* output, const char* widget_id,
                                const char* event, const char* handler) {
    if (!output || !widget_id || !event || !handler) {
        return false;
    }

    // Map event names to Tk event syntax
    const char* tk_event = event;
    if (strcmp(event, "click") == 0) {
        tk_event = "Button-1";
    } else if (strcmp(event, "doubleclick") == 0) {
        tk_event = "Double-Button-1";
    } else if (strcmp(event, "keypress") == 0) {
        tk_event = "Key";
    } else if (strcmp(event, "keyrelease") == 0) {
        tk_event = "KeyRelease";
    } else if (strcmp(event, "focus") == 0) {
        tk_event = "FocusIn";
    } else if (strcmp(event, "focusout") == 0) {
        tk_event = "FocusOut";
    }

    fprintf(output, "bind .%s <%s> {%s}\n", widget_id, tk_event, handler);

    return true;
}
