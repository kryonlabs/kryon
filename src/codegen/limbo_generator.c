/**
 * @file limbo_generator.c
 * @brief Kryon Intermediate Representation (KIR) to Limbo Code Generator
 *
 * This module generates native Limbo source code from KIR files,
 * enabling Kryon apps to run natively in TaijiOS/Inferno with full
 * module access (sys, draw, dial, etc.).
 *
 * Pipeline: .kry → .kir → .b (Limbo) → .dis (bytecode)
 */

#include "lib9.h"
#include "kir_format.h"
#include "parser.h"
#include "error.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>

// Forward declarations
typedef struct LimboGenerator LimboGenerator;
typedef struct WidgetInfo WidgetInfo;

/**
 * Widget information for tracking during code generation
 */
struct WidgetInfo {
    char *id;                // Unique widget identifier
    char *type;              // Widget type (Button, Text, etc.)
    char *tk_path;           // Tk widget path (.p.w.w0, etc.)
    char *event_handler;     // Event handler function name (if any)
    int index;               // Index in parent
};

/**
 * Limbo code generator state
 */
struct LimboGenerator {
    FILE *output;            // Output file stream
    int indent_level;        // Current indentation level
    int widget_counter;      // Counter for unique widget IDs
    int handler_counter;     // Counter for unique handler names

    // State tracking
    bool needs_sys;          // Whether sys module is needed
    bool needs_draw;         // Whether draw module is needed
    bool needs_dial;         // Whether dial module is needed
    bool needs_tk;           // Whether tk module is needed
    bool needs_tkclient;     // Whether tkclient module is needed

    // Widget tracking
    WidgetInfo *widgets;     // Array of widgets
    size_t widget_count;
    size_t widget_capacity;

    // Error tracking
    char *error_message;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Write indented line to output
 */
static void write_line(LimboGenerator *gen, const char *fmt, ...) {
    // Write indentation
    for (int i = 0; i < gen->indent_level; i++) {
        fprintf(gen->output, "\t");
    }

    // Write formatted content
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->output, fmt, args);
    va_end(args);

    fprintf(gen->output, "\n");
}

/**
 * Escape string for Limbo (handle quotes, newlines, etc.)
 */
static char *escape_string(const char *str) {
    if (!str) return strdup("");

    size_t len = strlen(str);
    char *result = malloc(len * 2 + 1);  // Worst case: every char needs escaping
    if (!result) return NULL;

    char *dst = result;
    for (const char *src = str; *src; src++) {
        switch (*src) {
            case '"':  *dst++ = '\\'; *dst++ = '"'; break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
            case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
            case '\t': *dst++ = '\\'; *dst++ = 't'; break;
            default:   *dst++ = *src; break;
        }
    }
    *dst = '\0';

    return result;
}

/**
 * Convert Kryon element type to Tk widget type
 */
static const char *kryon_to_tk_widget(const char *kryon_type) {
    if (strcmp(kryon_type, "Button") == 0) return "button";
    if (strcmp(kryon_type, "Text") == 0) return "label";
    if (strcmp(kryon_type, "Input") == 0) return "entry";
    if (strcmp(kryon_type, "Checkbox") == 0) return "checkbutton";
    if (strcmp(kryon_type, "Container") == 0) return "frame";
    if (strcmp(kryon_type, "Row") == 0) return "frame";
    if (strcmp(kryon_type, "Column") == 0) return "frame";
    if (strcmp(kryon_type, "Center") == 0) return "frame";
    return "frame";  // Default
}

// =============================================================================
// Generator Creation/Destruction
// =============================================================================

/**
 * Create a new Limbo code generator
 */
static LimboGenerator *limbo_generator_create(FILE *output) {
    LimboGenerator *gen = malloc(sizeof(LimboGenerator));
    if (!gen) return NULL;

    memset(gen, 0, sizeof(LimboGenerator));
    gen->output = output;
    gen->indent_level = 0;
    gen->widget_counter = 0;
    gen->handler_counter = 0;

    // Always need these for Tk apps
    gen->needs_tk = true;
    gen->needs_tkclient = true;
    gen->needs_draw = true;
    gen->needs_sys = true;

    // Allocate widget tracking array
    gen->widget_capacity = 64;
    gen->widgets = malloc(sizeof(WidgetInfo) * gen->widget_capacity);
    if (!gen->widgets) {
        free(gen);
        return NULL;
    }

    return gen;
}

/**
 * Destroy Limbo code generator
 */
static void limbo_generator_destroy(LimboGenerator *gen) {
    if (!gen) return;

    // Free widget tracking
    for (size_t i = 0; i < gen->widget_count; i++) {
        free(gen->widgets[i].id);
        free(gen->widgets[i].type);
        free(gen->widgets[i].tk_path);
        free(gen->widgets[i].event_handler);
    }
    free(gen->widgets);

    free(gen->error_message);
    free(gen);
}

// =============================================================================
// Code Generation - Header
// =============================================================================

/**
 * Generate module name from filename
 */
static char *generate_module_name(const char *filename) {
    // Extract base name without extension
    const char *base = strrchr(filename, '/');
    if (!base) base = filename;
    else base++;

    // Remove extension
    char *name = strdup(base);
    char *dot = strrchr(name, '.');
    if (dot) *dot = '\0';

    // Capitalize first letter
    if (name[0] >= 'a' && name[0] <= 'z') {
        name[0] = name[0] - 'a' + 'A';
    }

    return name;
}

/**
 * Generate Limbo source file header (implement, includes)
 */
static bool generate_header(LimboGenerator *gen, const char *source_file) {
    char *module_name = generate_module_name(source_file);

    write_line(gen, "# Generated from %s by Kryon compiler", source_file);
    write_line(gen, "implement %s;", module_name);
    write_line(gen, "");

    // Module includes
    write_line(gen, "include \"sys.m\";");
    write_line(gen, "\tsys: Sys;");

    write_line(gen, "include \"draw.m\";");
    write_line(gen, "\tdraw: Draw;");
    write_line(gen, "\tContext, Display, Screen: import draw;");
    write_line(gen, "");

    write_line(gen, "include \"tk.m\";");
    write_line(gen, "\ttk: Tk;");
    write_line(gen, "\tToplevel: import tk;");
    write_line(gen, "");

    write_line(gen, "include \"tkclient.m\";");
    write_line(gen, "\ttkclient: Tkclient;");
    write_line(gen, "");

    // Module declaration
    write_line(gen, "%s: module {", module_name);
    gen->indent_level++;
    write_line(gen, "init: fn(ctxt: ref Draw->Context, argv: list of string);");
    gen->indent_level--;
    write_line(gen, "};");
    write_line(gen, "");

    free(module_name);
    return true;
}

// =============================================================================
// Code Generation - Widget Processing
// =============================================================================

/**
 * Get property value as string from AST node
 */
static char *get_property_string(const KryonASTNode *element, const char *prop_name) {
    if (!element || element->type != KRYON_AST_ELEMENT) return NULL;

    for (size_t i = 0; i < element->data.element.property_count; i++) {
        KryonASTNode *prop = element->data.element.properties[i];
        if (prop && prop->type == KRYON_AST_PROPERTY) {
            if (strcmp(prop->data.property.name, prop_name) == 0) {
                KryonASTNode *value = prop->data.property.value;
                if (value && value->type == KRYON_AST_LITERAL) {
                    if (value->data.literal.value_type == KRYON_VALUE_STRING) {
                        return strdup(value->data.literal.string_value);
                    } else if (value->data.literal.value_type == KRYON_VALUE_INTEGER) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%d", value->data.literal.int_value);
                        return strdup(buf);
                    }
                }
            }
        }
    }

    return NULL;
}

/**
 * Generate Tk widget creation command
 */
static void generate_widget_creation(LimboGenerator *gen, const KryonASTNode *element,
                                     const char *widget_path, const char *parent_path) {
    const char *element_type = element->data.element.element_type;
    const char *tk_widget = kryon_to_tk_widget(element_type);

    // Create widget
    write_line(gen, "tk->cmd(t, \"%s %s\");", tk_widget, widget_path);

    // Apply text property if present
    char *text = get_property_string(element, "text");
    if (text) {
        char *escaped = escape_string(text);
        write_line(gen, "tk->cmd(t, \"%s configure -text {%s}\");", widget_path, escaped);
        free(escaped);
        free(text);
    }

    // Apply background color if present
    char *bg_color = get_property_string(element, "backgroundColor");
    if (bg_color) {
        write_line(gen, "tk->cmd(t, \"%s configure -background %s\");", widget_path, bg_color);
        free(bg_color);
    }

    // Apply dimensions if present
    char *width = get_property_string(element, "width");
    char *height = get_property_string(element, "height");
    if (width) {
        write_line(gen, "tk->cmd(t, \"%s configure -width %s\");", widget_path, width);
        free(width);
    }
    if (height) {
        write_line(gen, "tk->cmd(t, \"%s configure -height %s\");", widget_path, height);
        free(height);
    }

    // Pack widget
    write_line(gen, "tk->cmd(t, \"pack %s -side top\");", widget_path);
}

/**
 * Recursively generate widgets from AST
 */
static void generate_widgets_recursive(LimboGenerator *gen, const KryonASTNode *element,
                                       const char *parent_path, int *widget_counter) {
    if (!element || element->type != KRYON_AST_ELEMENT) return;

    // Generate widget path
    char widget_path[256];
    snprintf(widget_path, sizeof(widget_path), "%s.w%d", parent_path, (*widget_counter)++);

    // Generate this widget
    generate_widget_creation(gen, element, widget_path, parent_path);
    write_line(gen, "");

    // Check for event handlers
    char *on_click = get_property_string(element, "onClick");
    if (on_click) {
        // Store for later event binding
        // TODO: Add to widget tracking
        free(on_click);
    }

    // Recursively generate children
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        generate_widgets_recursive(gen, element->data.element.children[i],
                                   widget_path, widget_counter);
    }
}

// =============================================================================
// Code Generation - Main Init Function
// =============================================================================

/**
 * Find the App element in AST
 */
static const KryonASTNode *find_app_element(const KryonASTNode *root) {
    if (!root || root->type != KRYON_AST_ROOT) return NULL;

    for (size_t i = 0; i < root->data.element.child_count; i++) {
        KryonASTNode *child = root->data.element.children[i];
        if (child && child->type == KRYON_AST_ELEMENT) {
            if (strcmp(child->data.element.element_type, "App") == 0) {
                return child;
            }
        }
    }

    return NULL;
}

/**
 * Generate init function
 */
static bool generate_init_function(LimboGenerator *gen, const KryonASTNode *root) {
    const KryonASTNode *app = find_app_element(root);
    if (!app) {
        gen->error_message = strdup("No App element found in AST");
        return false;
    }

    // Get window title
    char *title = get_property_string(app, "windowTitle");
    if (!title) title = strdup("Kryon App");

    write_line(gen, "init(ctxt: ref Draw->Context, argv: list of string) {");
    gen->indent_level++;

    // Module loading
    write_line(gen, "sys = load Sys Sys->PATH;");
    write_line(gen, "if (ctxt == nil) {");
    gen->indent_level++;
    write_line(gen, "sys->fprint(sys->fildes(2), \"no window context\\n\");");
    write_line(gen, "raise \"fail:bad context\";");
    gen->indent_level--;
    write_line(gen, "}");
    write_line(gen, "");

    write_line(gen, "draw = load Draw Draw->PATH;");
    write_line(gen, "tk = load Tk Tk->PATH;");
    write_line(gen, "tkclient = load Tkclient Tkclient->PATH;");
    write_line(gen, "tkclient->init();");
    write_line(gen, "");

    write_line(gen, "sys->pctl(Sys->NEWPGRP, nil);");
    write_line(gen, "");

    // Create main window
    char *escaped_title = escape_string(title);
    write_line(gen, "(t, titlechan) := tkclient->toplevel(ctxt, \"\", \"%s\", Tkclient->Appl);",
               escaped_title);
    free(escaped_title);
    free(title);
    write_line(gen, "");

    // Create main panel
    write_line(gen, "tk->cmd(t, \"panel .p\");");
    write_line(gen, "tk->cmd(t, \"pack .p -fill both -expand 1\");");
    write_line(gen, "");

    // Generate widgets
    write_line(gen, "# Create widgets");
    int widget_counter = 0;
    for (size_t i = 0; i < app->data.element.child_count; i++) {
        generate_widgets_recursive(gen, app->data.element.children[i],
                                   ".p", &widget_counter);
    }

    // Show window
    write_line(gen, "tkclient->onscreen(t, nil);");
    write_line(gen, "tkclient->startinput(t, \"kbd\" :: \"ptr\" :: nil);");
    write_line(gen, "");

    // Event loop
    write_line(gen, "# Event loop");
    write_line(gen, "for(;;) alt {");
    gen->indent_level++;
    write_line(gen, "s := <-t.ctxt.kbd =>");
    gen->indent_level++;
    write_line(gen, "tk->keyboard(t, s);");
    gen->indent_level--;
    write_line(gen, "s := <-t.ctxt.ptr =>");
    gen->indent_level++;
    write_line(gen, "tk->pointer(t, *s);");
    gen->indent_level--;
    write_line(gen, "s := <-t.ctxt.ctl or");
    write_line(gen, "s = <-t.wreq or");
    write_line(gen, "s = <-titlechan =>");
    gen->indent_level++;
    write_line(gen, "tkclient->wmctl(t, s);");
    gen->indent_level--;
    gen->indent_level--;
    write_line(gen, "}");

    gen->indent_level--;
    write_line(gen, "}");

    return true;
}

// =============================================================================
// Public API
// =============================================================================

/**
 * Generate Limbo source code from KIR file
 */
bool kryon_generate_limbo_from_kir(const char *kir_file, const char *output_file) {
    // Read KIR file
    KryonKIRReader *reader = kryon_kir_reader_create(NULL);
    if (!reader) {
        fprint(2, "Error: Failed to create KIR reader\n");
        return false;
    }

    KryonASTNode *ast = NULL;
    if (!kryon_kir_read_file(reader, kir_file, &ast)) {
        size_t error_count;
        const char **errors = kryon_kir_reader_get_errors(reader, &error_count);
        fprint(2, "Error: Failed to read KIR file:\n");
        for (size_t i = 0; i < error_count; i++) {
            fprint(2, "  %s\n", errors[i]);
        }
        kryon_kir_reader_destroy(reader);
        return false;
    }

    kryon_kir_reader_destroy(reader);

    // Open output file
    FILE *output = fopen(output_file, "w");
    if (!output) {
        fprint(2, "Error: Failed to open output file: %s\n", output_file);
        return false;
    }

    // Create generator
    LimboGenerator *gen = limbo_generator_create(output);
    if (!gen) {
        fprint(2, "Error: Failed to create Limbo generator\n");
        fclose(output);
        return false;
    }

    // Generate code
    bool success = true;
    if (!generate_header(gen, kir_file)) {
        fprint(2, "Error: Failed to generate header\n");
        success = false;
    } else if (!generate_init_function(gen, ast)) {
        fprint(2, "Error: Failed to generate init function: %s\n",
               gen->error_message ? gen->error_message : "unknown");
        success = false;
    }

    // Cleanup
    limbo_generator_destroy(gen);
    fclose(output);

    if (!success) {
        // Remove incomplete output file
        remove(output_file);
    }

    return success;
}
