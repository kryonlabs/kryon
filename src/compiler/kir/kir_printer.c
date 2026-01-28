/**
 * @file kir_printer.c
 * @brief KIR Pretty-Printer Implementation
 */

#include "kir_printer.h"
#include "kir_format.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

// =============================================================================
// INTERNAL STRUCTURES
// =============================================================================

#define MAX_PRINTER_ERRORS 100
#define INITIAL_BUFFER_SIZE 8192

struct KryonKIRPrinter {
    KryonPrinterConfig config;
    KryonPrinterStats stats;
    char **errors;
    size_t error_count;
    size_t error_capacity;

    // Output buffer
    char *buffer;
    size_t buffer_size;
    size_t buffer_capacity;

    // Current state
    size_t current_indent;
    bool at_line_start;
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static void add_error(KryonKIRPrinter *printer, const char *format, ...) {
    if (!printer || printer->error_count >= MAX_PRINTER_ERRORS) {
        return;
    }

    if (printer->error_count >= printer->error_capacity) {
        size_t new_capacity = printer->error_capacity == 0 ? 10 : printer->error_capacity * 2;
        char **new_errors = realloc(printer->errors, new_capacity * sizeof(char*));
        if (!new_errors) return;
        printer->errors = new_errors;
        printer->error_capacity = new_capacity;
    }

    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    printer->errors[printer->error_count] = strdup(buffer);
    if (printer->errors[printer->error_count]) {
        printer->error_count++;
    }
}

static bool ensure_buffer_capacity(KryonKIRPrinter *printer, size_t additional) {
    size_t needed = printer->buffer_size + additional;
    if (needed <= printer->buffer_capacity) {
        return true;
    }

    size_t new_capacity = printer->buffer_capacity * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    char *new_buffer = realloc(printer->buffer, new_capacity);
    if (!new_buffer) {
        add_error(printer, "Failed to allocate buffer memory");
        return false;
    }

    printer->buffer = new_buffer;
    printer->buffer_capacity = new_capacity;
    return true;
}

static void emit(KryonKIRPrinter *printer, const char *format, ...) {
    va_list args;
    va_start(args, format);

    // Calculate needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return;
    }

    // Ensure buffer capacity
    if (!ensure_buffer_capacity(printer, needed + 1)) {
        va_end(args);
        return;
    }

    // Write to buffer
    vsnprintf(printer->buffer + printer->buffer_size, needed + 1, format, args);
    printer->buffer_size += needed;

    va_end(args);
}

static void emit_indent(KryonKIRPrinter *printer) {
    if (!printer->at_line_start) {
        return;
    }

    if (printer->config.use_tabs) {
        for (size_t i = 0; i < printer->current_indent; i++) {
            emit(printer, "\t");
        }
    } else {
        for (size_t i = 0; i < printer->current_indent * printer->config.indent_size; i++) {
            emit(printer, " ");
        }
    }

    printer->at_line_start = false;
}

static void emit_newline(KryonKIRPrinter *printer) {
    emit(printer, "\n");
    printer->at_line_start = true;
    printer->stats.lines_generated++;
}

static void indent(KryonKIRPrinter *printer) {
    printer->current_indent++;
}

static void dedent(KryonKIRPrinter *printer) {
    if (printer->current_indent > 0) {
        printer->current_indent--;
    }
}

// =============================================================================
// VALUE PRINTING
// =============================================================================

static void print_value(KryonKIRPrinter *printer, const KryonASTValue *value) {
    switch (value->type) {
        case KRYON_VALUE_STRING:
            emit(printer, "\"%s\"", value->data.string_value ? value->data.string_value : "");
            break;

        case KRYON_VALUE_INTEGER:
            emit(printer, "%lld", (long long)value->data.int_value);
            break;

        case KRYON_VALUE_FLOAT:
            emit(printer, "%.2f", value->data.float_value);
            break;

        case KRYON_VALUE_BOOLEAN:
            emit(printer, "%s", value->data.bool_value ? "true" : "false");
            break;

        case KRYON_VALUE_NULL:
            emit(printer, "null");
            break;

        case KRYON_VALUE_COLOR:
            // Format as hex color
            emit(printer, "\"#%08X\"", value->data.color_value);
            break;

        case KRYON_VALUE_UNIT:
            emit(printer, "%.2f%s", value->data.unit_value.value, value->data.unit_value.unit);
            break;

        default:
            emit(printer, "unknown");
            break;
    }
}

// =============================================================================
// EXPRESSION PRINTING
// =============================================================================

static void print_expression(KryonKIRPrinter *printer, const KryonASTNode *node);

static void print_binary_op(KryonKIRPrinter *printer, const KryonASTNode *node) {
    emit(printer, "(");
    print_expression(printer, node->data.binary_op.left);

    // Get operator symbol
    const char *op_str = " ?? ";
    switch (node->data.binary_op.operator) {
        case KRYON_TOKEN_PLUS: op_str = " + "; break;
        case KRYON_TOKEN_MINUS: op_str = " - "; break;
        case KRYON_TOKEN_MULTIPLY: op_str = " * "; break;
        case KRYON_TOKEN_DIVIDE: op_str = " / "; break;
        case KRYON_TOKEN_MODULO: op_str = " % "; break;
        case KRYON_TOKEN_EQUALS: op_str = " == "; break;
        case KRYON_TOKEN_NOT_EQUALS: op_str = " != "; break;
        case KRYON_TOKEN_LESS_THAN: op_str = " < "; break;
        case KRYON_TOKEN_LESS_EQUAL: op_str = " <= "; break;
        case KRYON_TOKEN_GREATER_THAN: op_str = " > "; break;
        case KRYON_TOKEN_GREATER_EQUAL: op_str = " >= "; break;
        case KRYON_TOKEN_LOGICAL_AND: op_str = " && "; break;
        case KRYON_TOKEN_LOGICAL_OR: op_str = " || "; break;
        default: break;
    }

    emit(printer, "%s", op_str);
    print_expression(printer, node->data.binary_op.right);
    emit(printer, ")");
}

static void print_expression(KryonKIRPrinter *printer, const KryonASTNode *node) {
    if (!node) {
        emit(printer, "null");
        return;
    }

    switch (node->type) {
        case KRYON_AST_LITERAL:
            print_value(printer, &node->data.literal.value);
            break;

        case KRYON_AST_VARIABLE:
            emit(printer, "$%s", node->data.variable.name);
            break;

        case KRYON_AST_IDENTIFIER:
            emit(printer, "%s", node->data.identifier.name);
            break;

        case KRYON_AST_BINARY_OP:
            print_binary_op(printer, node);
            break;

        case KRYON_AST_UNARY_OP:
            emit(printer, "!");
            print_expression(printer, node->data.unary_op.operand);
            break;

        case KRYON_AST_FUNCTION_CALL:
            emit(printer, "%s(", node->data.function_call.function_name);
            for (size_t i = 0; i < node->data.function_call.argument_count; i++) {
                if (i > 0) emit(printer, ", ");
                print_expression(printer, node->data.function_call.arguments[i]);
            }
            emit(printer, ")");
            break;

        case KRYON_AST_TEMPLATE: {
            emit(printer, "`");
            for (size_t i = 0; i < node->data.template.segment_count; i++) {
                KryonASTNode *segment = node->data.template.segments[i];
                if (segment->type == KRYON_AST_LITERAL) {
                    emit(printer, "%s", segment->data.literal.value.data.string_value);
                } else if (segment->type == KRYON_AST_VARIABLE) {
                    emit(printer, "${%s}", segment->data.variable.name);
                }
            }
            emit(printer, "`");
            break;
        }

        default:
            emit(printer, "<?>");
            break;
    }
}

// =============================================================================
// PROPERTY PRINTING
// =============================================================================

static void print_property(KryonKIRPrinter *printer, const KryonASTNode *prop_node) {
    if (!prop_node || prop_node->type != KRYON_AST_PROPERTY) {
        return;
    }

    emit_indent(printer);
    emit(printer, "%s = ", prop_node->data.property.name);
    print_expression(printer, prop_node->data.property.value);
    emit_newline(printer);

    printer->stats.properties_printed++;
}

// =============================================================================
// ELEMENT PRINTING
// =============================================================================

static void print_element(KryonKIRPrinter *printer, const KryonASTNode *element);

static void print_element(KryonKIRPrinter *printer, const KryonASTNode *element) {
    if (!element || element->type != KRYON_AST_ELEMENT) {
        return;
    }

    emit_indent(printer);
    emit(printer, "%s {", element->data.element.element_type);
    emit_newline(printer);

    indent(printer);

    // Print properties
    for (size_t i = 0; i < element->data.element.property_count; i++) {
        print_property(printer, element->data.element.properties[i]);
    }

    // Print child elements
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        if (i == 0 && element->data.element.property_count > 0) {
            if (!printer->config.compact_mode) {
                emit_newline(printer);
            }
        }
        print_element(printer, element->data.element.children[i]);
    }

    dedent(printer);

    emit_indent(printer);
    emit(printer, "}");
    emit_newline(printer);

    printer->stats.elements_printed++;
}

// =============================================================================
// ROOT PRINTING
// =============================================================================

static bool print_ast(KryonKIRPrinter *printer, const KryonASTNode *ast) {
    if (!ast) {
        add_error(printer, "NULL AST provided");
        return false;
    }

    // Reset stats
    printer->stats.elements_printed = 0;
    printer->stats.properties_printed = 0;
    printer->stats.lines_generated = 0;

    // Reset buffer
    printer->buffer_size = 0;
    printer->current_indent = 0;
    printer->at_line_start = true;

    if (ast->type == KRYON_AST_ROOT) {
        // Print all children of root
        for (size_t i = 0; i < ast->data.element.child_count; i++) {
            print_element(printer, ast->data.element.children[i]);
            if (!printer->config.compact_mode && i < ast->data.element.child_count - 1) {
                emit_newline(printer);
            }
        }
    } else if (ast->type == KRYON_AST_ELEMENT) {
        // Print single element
        print_element(printer, ast);
    } else {
        add_error(printer, "Unsupported root node type: %d", ast->type);
        return false;
    }

    printer->stats.total_bytes = printer->buffer_size;
    return true;
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

KryonPrinterConfig kryon_printer_default_config(void) {
    KryonPrinterConfig config = {
        .indent_size = 2,
        .use_tabs = false,
        .compact_mode = false,
        .preserve_comments = false,
        .explicit_types = false,
        .sort_properties = false,
        .max_line_length = 0
    };
    return config;
}

KryonPrinterConfig kryon_printer_compact_config(void) {
    KryonPrinterConfig config = kryon_printer_default_config();
    config.compact_mode = true;
    config.indent_size = 0;
    return config;
}

KryonPrinterConfig kryon_printer_readable_config(void) {
    KryonPrinterConfig config = kryon_printer_default_config();
    config.indent_size = 4;
    return config;
}

KryonKIRPrinter *kryon_printer_create(const KryonPrinterConfig *config) {
    KryonKIRPrinter *printer = calloc(1, sizeof(KryonKIRPrinter));
    if (!printer) {
        return NULL;
    }

    if (config) {
        printer->config = *config;
    } else {
        printer->config = kryon_printer_default_config();
    }

    printer->buffer_capacity = INITIAL_BUFFER_SIZE;
    printer->buffer = malloc(printer->buffer_capacity);
    if (!printer->buffer) {
        free(printer);
        return NULL;
    }

    printer->buffer[0] = '\0';
    printer->at_line_start = true;

    return printer;
}

void kryon_printer_destroy(KryonKIRPrinter *printer) {
    if (!printer) return;

    // Free errors
    for (size_t i = 0; i < printer->error_count; i++) {
        free(printer->errors[i]);
    }
    free(printer->errors);

    // Free buffer
    free(printer->buffer);

    free(printer);
}

bool kryon_printer_print_string(KryonKIRPrinter *printer,
                                const KryonASTNode *ast,
                                char **out_source) {
    if (!printer || !ast || !out_source) {
        return false;
    }

    if (!print_ast(printer, ast)) {
        return false;
    }

    *out_source = strdup(printer->buffer);
    return *out_source != NULL;
}

bool kryon_printer_print_file(KryonKIRPrinter *printer,
                              const KryonASTNode *ast,
                              const char *output_path) {
    if (!printer || !ast || !output_path) {
        return false;
    }

    if (!print_ast(printer, ast)) {
        return false;
    }

    FILE *file = fopen(output_path, "w");
    if (!file) {
        add_error(printer, "Failed to open output file: %s", output_path);
        return false;
    }

    size_t written = fwrite(printer->buffer, 1, printer->buffer_size, file);
    fclose(file);

    if (written != printer->buffer_size) {
        add_error(printer, "Failed to write complete output");
        return false;
    }

    return true;
}

bool kryon_printer_kir_to_source(KryonKIRPrinter *printer,
                                 const char *kir_file_path,
                                 const char *output_path) {
    if (!printer || !kir_file_path || !output_path) {
        return false;
    }

    // Read KIR file
    KryonKIRReader *reader = kryon_kir_reader_create(NULL);
    if (!reader) {
        add_error(printer, "Failed to create KIR reader");
        return false;
    }

    KryonASTNode *ast = NULL;
    if (!kryon_kir_read_file(reader, kir_file_path, &ast)) {
        size_t kir_error_count;
        const char **kir_errors = kryon_kir_reader_get_errors(reader, &kir_error_count);
        for (size_t i = 0; i < kir_error_count; i++) {
            add_error(printer, "KIR read error: %s", kir_errors[i]);
        }
        kryon_kir_reader_destroy(reader);
        return false;
    }

    kryon_kir_reader_destroy(reader);

    // Print to file
    bool result = kryon_printer_print_file(printer, ast, output_path);

    // Note: AST cleanup would go here if we had a destroy function

    return result;
}

const char **kryon_printer_get_errors(const KryonKIRPrinter *printer,
                                      size_t *count) {
    if (!printer || !count) {
        if (count) *count = 0;
        return NULL;
    }

    *count = printer->error_count;
    return (const char **)printer->errors;
}

const KryonPrinterStats *kryon_printer_get_stats(const KryonKIRPrinter *printer) {
    return printer ? &printer->stats : NULL;
}

bool kryon_print_ast_simple(const KryonASTNode *ast, char **out_source) {
    KryonKIRPrinter *printer = kryon_printer_create(NULL);
    if (!printer) {
        return false;
    }

    bool result = kryon_printer_print_string(printer, ast, out_source);
    kryon_printer_destroy(printer);

    return result;
}

bool kryon_print_kir_simple(const char *kir_file_path, const char *output_path) {
    KryonKIRPrinter *printer = kryon_printer_create(NULL);
    if (!printer) {
        return false;
    }

    bool result = kryon_printer_kir_to_source(printer, kir_file_path, output_path);
    kryon_printer_destroy(printer);

    return result;
}
