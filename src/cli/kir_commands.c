/**

 * @file kir_commands.c
 * @brief KIR Utility Commands Implementation
 */
#include "lib9.h"


#include "kir_format.h"
#include "parser.h"
#include "error.h"
#include "memory.h"
#include <stdbool.h>
#include <getopt.h>

// =============================================================================
// AST STATISTICS
// =============================================================================

typedef struct {
    size_t total_nodes;
    size_t elements;
    size_t properties;
    size_t literals;
    size_t variables;
    size_t function_calls;
    size_t binary_ops;
    size_t max_depth;
    size_t current_depth;
} ASTStats;

static void collect_stats(const KryonASTNode *node, ASTStats *stats) {
    if (!node) return;

    stats->total_nodes++;
    stats->current_depth++;
    if (stats->current_depth > stats->max_depth) {
        stats->max_depth = stats->current_depth;
    }

    switch (node->type) {
        case KRYON_AST_ELEMENT:
            stats->elements++;
            // Process properties
            for (size_t i = 0; i < node->data.element.property_count; i++) {
                collect_stats(node->data.element.properties[i], stats);
            }
            // Process children
            for (size_t i = 0; i < node->data.element.child_count; i++) {
                collect_stats(node->data.element.children[i], stats);
            }
            break;

        case KRYON_AST_PROPERTY:
            stats->properties++;
            collect_stats(node->data.property.value, stats);
            break;

        case KRYON_AST_LITERAL:
            stats->literals++;
            break;

        case KRYON_AST_VARIABLE:
        case KRYON_AST_IDENTIFIER:
            stats->variables++;
            break;

        case KRYON_AST_FUNCTION_CALL:
            stats->function_calls++;
            for (size_t i = 0; i < node->data.function_call.argument_count; i++) {
                collect_stats(node->data.function_call.arguments[i], stats);
            }
            break;

        case KRYON_AST_BINARY_OP:
            stats->binary_ops++;
            collect_stats(node->data.binary_op.left, stats);
            collect_stats(node->data.binary_op.right, stats);
            break;

        case KRYON_AST_UNARY_OP:
            collect_stats(node->data.unary_op.operand, stats);
            break;

        case KRYON_AST_TERNARY_OP:
            collect_stats(node->data.ternary_op.condition, stats);
            collect_stats(node->data.ternary_op.true_expr, stats);
            collect_stats(node->data.ternary_op.false_expr, stats);
            break;

        default:
            break;
    }

    stats->current_depth--;
}

// =============================================================================
// KIR-STATS COMMAND
// =============================================================================

KryonResult kir_stats_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    bool verbose = false;

    // Initialize error system
    if (kryon_error_init() != KRYON_SUCCESS) {
        fprint(2, "Fatal: Could not initialize error system.\n");
        return KRYON_ERROR_PLATFORM_ERROR;
    }

    // --- Argument Parsing ---
    static struct option long_options[] = {
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'v':
                verbose = true;
                break;
            case 'h':
                fprintf(stderr, "Usage: kryon kir-stats <file.kir> [options]\n");
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -v, --verbose        Show detailed statistics\n");
                fprintf(stderr, "  -h, --help           Show this help message\n");
                return KRYON_SUCCESS;
            default:
                return KRYON_ERROR_INVALID_ARGUMENT;
        }
    }

    // Get input file
    if (optind >= argc) {
        fprint(2, "Error: No input file specified\n");
        fprint(2, "Usage: kryon kir-stats <file.kir>\n");
        return KRYON_ERROR_INVALID_ARGUMENT;
    }

    input_file = argv[optind];

    // Read KIR file
    KryonKIRReader *reader = kryon_kir_reader_create(NULL);
    if (!reader) {
        fprint(2, "Error: Failed to create KIR reader\n");
        return KRYON_ERROR_COMPILATION_FAILED;
    }

    KryonASTNode *ast = NULL;
    if (!kryon_kir_read_file(reader, input_file, &ast)) {
        size_t error_count;
        const char **errors = kryon_kir_reader_get_errors(reader, &error_count);
        fprint(2, "Error: Failed to read KIR file:\n");
        for (size_t i = 0; i < error_count; i++) {
            fprint(2, "  %s\n", errors[i]);
        }
        kryon_kir_reader_destroy(reader);
        return KRYON_ERROR_COMPILATION_FAILED;
    }

    kryon_kir_reader_destroy(reader);

    // Collect statistics
    ASTStats stats = {0};
    collect_stats(ast, &stats);

    // Print statistics
    fprintf(stderr, "KIR Statistics: %s\n", input_file);
    fprintf(stderr, "================================\n");
    fprintf(stderr, "Total nodes:      %zu\n", stats.total_nodes);
    fprintf(stderr, "Elements:         %zu\n", stats.elements);
    fprintf(stderr, "Properties:       %zu\n", stats.properties);
    fprintf(stderr, "Literals:         %zu\n", stats.literals);
    fprintf(stderr, "Variables:        %zu\n", stats.variables);
    fprintf(stderr, "Function calls:   %zu\n", stats.function_calls);
    fprintf(stderr, "Binary ops:       %zu\n", stats.binary_ops);
    fprintf(stderr, "Max depth:        %zu\n", stats.max_depth);

    if (verbose) {
        fprintf(stderr, "\nRatios:\n");
        if (stats.elements > 0) {
            fprintf(stderr, "  Props/element:  %.2f\n", (double)stats.properties / stats.elements);
            fprintf(stderr, "  Children/elem:  %.2f\n", (double)(stats.elements - 1) / stats.elements);
        }
    }

    return KRYON_SUCCESS;
}

// =============================================================================
// KIR-VALIDATE COMMAND
// =============================================================================

KryonResult kir_validate_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    bool verbose = false;

    // Initialize error system
    if (kryon_error_init() != KRYON_SUCCESS) {
        fprint(2, "Fatal: Could not initialize error system.\n");
        return KRYON_ERROR_PLATFORM_ERROR;
    }

    // --- Argument Parsing ---
    static struct option long_options[] = {
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'v':
                verbose = true;
                break;
            case 'h':
                fprintf(stderr, "Usage: kryon kir-validate <file.kir> [options]\n");
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -v, --verbose        Show detailed validation info\n");
                fprintf(stderr, "  -h, --help           Show this help message\n");
                return KRYON_SUCCESS;
            default:
                return KRYON_ERROR_INVALID_ARGUMENT;
        }
    }

    // Get input file
    if (optind >= argc) {
        fprint(2, "Error: No input file specified\n");
        fprint(2, "Usage: kryon kir-validate <file.kir>\n");
        return KRYON_ERROR_INVALID_ARGUMENT;
    }

    input_file = argv[optind];

    if (verbose) {
        fprintf(stderr, "Validating: %s\n", input_file);
    }

    // Read and validate KIR file
    KryonKIRReader *reader = kryon_kir_reader_create(NULL);
    if (!reader) {
        fprint(2, "Error: Failed to create KIR reader\n");
        return KRYON_ERROR_COMPILATION_FAILED;
    }

    KryonASTNode *ast = NULL;
    if (!kryon_kir_read_file(reader, input_file, &ast)) {
        size_t error_count;
        const char **errors = kryon_kir_reader_get_errors(reader, &error_count);
        fprint(2, "❌ Validation FAILED:\n");
        for (size_t i = 0; i < error_count; i++) {
            fprint(2, "  • %s\n", errors[i]);
        }
        kryon_kir_reader_destroy(reader);
        return KRYON_ERROR_COMPILATION_FAILED;
    }

    kryon_kir_reader_destroy(reader);

    // Additional AST validation could go here
    fprintf(stderr, "✅ Validation PASSED: %s\n", input_file);

    if (verbose) {
        ASTStats stats = {0};
        collect_stats(ast, &stats);
        fprintf(stderr, "  Nodes: %zu, Elements: %zu, Properties: %zu\n",
               stats.total_nodes, stats.elements, stats.properties);
    }

    return KRYON_SUCCESS;
}

// =============================================================================
// KIR-DUMP COMMAND
// =============================================================================

static void dump_indent(int level) {
    for (int i = 0; i < level; i++) {
        fprintf(stderr, "  ");
    }
}

static void dump_node(const KryonASTNode *node, int level);

static void dump_value(const KryonASTValue *value) {
    switch (value->type) {
        case KRYON_VALUE_STRING:
            fprintf(stderr, "\"%s\"", value->data.string_value ? value->data.string_value : "");
            break;
        case KRYON_VALUE_INTEGER:
            fprintf(stderr, "%lld", (long long)value->data.int_value);
            break;
        case KRYON_VALUE_FLOAT:
            fprintf(stderr, "%.2f", value->data.float_value);
            break;
        case KRYON_VALUE_BOOLEAN:
            fprintf(stderr, "%s", value->data.bool_value ? "true" : "false");
            break;
        case KRYON_VALUE_NULL:
            fprintf(stderr, "null");
            break;
        case KRYON_VALUE_COLOR:
            fprintf(stderr, "#%08X", value->data.color_value);
            break;
        case KRYON_VALUE_UNIT:
            fprintf(stderr, "%.2f%s", value->data.unit_value.value, value->data.unit_value.unit);
            break;
        default:
            fprintf(stderr, "<?>");
            break;
    }
}

static void dump_node(const KryonASTNode *node, int level) {
    if (!node) {
        dump_indent(level);
        fprintf(stderr, "(null)\n");
        return;
    }

    dump_indent(level);

    switch (node->type) {
        case KRYON_AST_ROOT:
            fprintf(stderr, "ROOT (%zu children)\n", node->data.element.child_count);
            for (size_t i = 0; i < node->data.element.child_count; i++) {
                dump_node(node->data.element.children[i], level + 1);
            }
            break;

        case KRYON_AST_ELEMENT:
            fprintf(stderr, "ELEMENT: %s (%zu props, %zu children)\n",
                   node->data.element.element_type,
                   node->data.element.property_count,
                   node->data.element.child_count);
            // Properties
            for (size_t i = 0; i < node->data.element.property_count; i++) {
                dump_node(node->data.element.properties[i], level + 1);
            }
            // Children
            for (size_t i = 0; i < node->data.element.child_count; i++) {
                dump_node(node->data.element.children[i], level + 1);
            }
            break;

        case KRYON_AST_PROPERTY:
            fprintf(stderr, "PROP: %s = ", node->data.property.name);
            dump_node(node->data.property.value, 0);
            break;

        case KRYON_AST_LITERAL:
            fprintf(stderr, "LITERAL: ");
            dump_value(&node->data.literal.value);
            fprintf(stderr, "\n");
            break;

        case KRYON_AST_VARIABLE:
            fprintf(stderr, "VAR: $%s\n", node->data.variable.name);
            break;

        case KRYON_AST_IDENTIFIER:
            fprintf(stderr, "ID: %s\n", node->data.identifier.name);
            break;

        case KRYON_AST_BINARY_OP:
            fprintf(stderr, "BINARY_OP\n");
            dump_node(node->data.binary_op.left, level + 1);
            dump_node(node->data.binary_op.right, level + 1);
            break;

        case KRYON_AST_FUNCTION_CALL:
            fprintf(stderr, "CALL: %s(%zu args)\n",
                   node->data.function_call.function_name,
                   node->data.function_call.argument_count);
            for (size_t i = 0; i < node->data.function_call.argument_count; i++) {
                dump_node(node->data.function_call.arguments[i], level + 1);
            }
            break;

        default:
            fprintf(stderr, "NODE: type=%d\n", node->type);
            break;
    }
}

KryonResult kir_dump_command(int argc, char *argv[]) {
    const char *input_file = NULL;

    // Initialize error system
    if (kryon_error_init() != KRYON_SUCCESS) {
        fprint(2, "Fatal: Could not initialize error system.\n");
        return KRYON_ERROR_PLATFORM_ERROR;
    }

    // --- Argument Parsing ---
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
                fprintf(stderr, "Usage: kryon kir-dump <file.kir>\n");
                fprintf(stderr, "Pretty-print KIR file structure in human-readable format\n");
                return KRYON_SUCCESS;
            default:
                return KRYON_ERROR_INVALID_ARGUMENT;
        }
    }

    // Get input file
    if (optind >= argc) {
        fprint(2, "Error: No input file specified\n");
        fprint(2, "Usage: kryon kir-dump <file.kir>\n");
        return KRYON_ERROR_INVALID_ARGUMENT;
    }

    input_file = argv[optind];

    // Read KIR file
    KryonKIRReader *reader = kryon_kir_reader_create(NULL);
    if (!reader) {
        fprint(2, "Error: Failed to create KIR reader\n");
        return KRYON_ERROR_COMPILATION_FAILED;
    }

    KryonASTNode *ast = NULL;
    if (!kryon_kir_read_file(reader, input_file, &ast)) {
        size_t error_count;
        const char **errors = kryon_kir_reader_get_errors(reader, &error_count);
        fprint(2, "Error: Failed to read KIR file:\n");
        for (size_t i = 0; i < error_count; i++) {
            fprint(2, "  %s\n", errors[i]);
        }
        kryon_kir_reader_destroy(reader);
        return KRYON_ERROR_COMPILATION_FAILED;
    }

    kryon_kir_reader_destroy(reader);

    // Dump AST
    fprintf(stderr, "=== KIR Dump: %s ===\n\n", input_file);
    dump_node(ast, 0);

    return KRYON_SUCCESS;
}

// =============================================================================
// KIR-DIFF COMMAND
// =============================================================================

static bool compare_values(const KryonASTValue *v1, const KryonASTValue *v2) {
    if (v1->type != v2->type) return false;

    switch (v1->type) {
        case KRYON_VALUE_STRING:
            return strcmp(v1->data.string_value ? v1->data.string_value : "",
                         v2->data.string_value ? v2->data.string_value : "") == 0;
        case KRYON_VALUE_INTEGER:
            return v1->data.int_value == v2->data.int_value;
        case KRYON_VALUE_FLOAT:
            return v1->data.float_value == v2->data.float_value;
        case KRYON_VALUE_BOOLEAN:
            return v1->data.bool_value == v2->data.bool_value;
        case KRYON_VALUE_COLOR:
            return v1->data.color_value == v2->data.color_value;
        default:
            return true;
    }
}

static bool compare_nodes(const KryonASTNode *n1, const KryonASTNode *n2, int *diff_count) {
    if (!n1 && !n2) return true;
    if (!n1 || !n2) {
        (*diff_count)++;
        return false;
    }

    if (n1->type != n2->type) {
        (*diff_count)++;
        return false;
    }

    switch (n1->type) {
        case KRYON_AST_ELEMENT:
            if (strcmp(n1->data.element.element_type, n2->data.element.element_type) != 0) {
                (*diff_count)++;
                return false;
            }
            if (n1->data.element.property_count != n2->data.element.property_count ||
                n1->data.element.child_count != n2->data.element.child_count) {
                (*diff_count)++;
                return false;
            }
            // Compare properties
            for (size_t i = 0; i < n1->data.element.property_count; i++) {
                compare_nodes(n1->data.element.properties[i], n2->data.element.properties[i], diff_count);
            }
            // Compare children
            for (size_t i = 0; i < n1->data.element.child_count; i++) {
                compare_nodes(n1->data.element.children[i], n2->data.element.children[i], diff_count);
            }
            break;

        case KRYON_AST_PROPERTY:
            if (strcmp(n1->data.property.name, n2->data.property.name) != 0) {
                (*diff_count)++;
                return false;
            }
            compare_nodes(n1->data.property.value, n2->data.property.value, diff_count);
            break;

        case KRYON_AST_LITERAL:
            if (!compare_values(&n1->data.literal.value, &n2->data.literal.value)) {
                (*diff_count)++;
                return false;
            }
            break;

        default:
            break;
    }

    return *diff_count == 0;
}

KryonResult kir_diff_command(int argc, char *argv[]) {
    const char *file1 = NULL;
    const char *file2 = NULL;

    // Initialize error system
    if (kryon_error_init() != KRYON_SUCCESS) {
        fprint(2, "Fatal: Could not initialize error system.\n");
        return KRYON_ERROR_PLATFORM_ERROR;
    }

    // --- Argument Parsing ---
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
                fprintf(stderr, "Usage: kryon kir-diff <file1.kir> <file2.kir>\n");
                fprintf(stderr, "Compare two KIR files structurally\n");
                return KRYON_SUCCESS;
            default:
                return KRYON_ERROR_INVALID_ARGUMENT;
        }
    }

    // Get input files
    if (optind + 1 >= argc) {
        fprint(2, "Error: Two input files required\n");
        fprint(2, "Usage: kryon kir-diff <file1.kir> <file2.kir>\n");
        return KRYON_ERROR_INVALID_ARGUMENT;
    }

    file1 = argv[optind];
    file2 = argv[optind + 1];

    // Read both KIR files
    KryonKIRReader *reader1 = kryon_kir_reader_create(NULL);
    KryonKIRReader *reader2 = kryon_kir_reader_create(NULL);

    if (!reader1 || !reader2) {
        fprint(2, "Error: Failed to create KIR readers\n");
        if (reader1) kryon_kir_reader_destroy(reader1);
        if (reader2) kryon_kir_reader_destroy(reader2);
        return KRYON_ERROR_COMPILATION_FAILED;
    }

    KryonASTNode *ast1 = NULL;
    KryonASTNode *ast2 = NULL;

    bool success = true;

    if (!kryon_kir_read_file(reader1, file1, &ast1)) {
        fprint(2, "Error: Failed to read %s\n", file1);
        success = false;
    }

    if (!kryon_kir_read_file(reader2, file2, &ast2)) {
        fprint(2, "Error: Failed to read %s\n", file2);
        success = false;
    }

    if (!success) {
        kryon_kir_reader_destroy(reader1);
        kryon_kir_reader_destroy(reader2);
        return KRYON_ERROR_COMPILATION_FAILED;
    }

    kryon_kir_reader_destroy(reader1);
    kryon_kir_reader_destroy(reader2);

    // Compare ASTs
    int diff_count = 0;
    bool identical = compare_nodes(ast1, ast2, &diff_count);

    fprintf(stderr, "Comparing: %s <-> %s\n", file1, file2);
    fprintf(stderr, "================================\n");

    if (identical) {
        fprintf(stderr, "✅ Files are structurally identical\n");
    } else {
        fprintf(stderr, "❌ Files differ (%d differences found)\n", diff_count);
    }

    return identical ? KRYON_SUCCESS : KRYON_ERROR_COMPILATION_FAILED;
}
