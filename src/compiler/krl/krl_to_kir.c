/**
 * @file krl_to_kir.c
 * @brief KRL to KIR Converter
 *
 * Converts KRL S-expressions to KIR JSON format.
 */

#include "krl_parser.h"
#include "kir_format.h"
#include "memory.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>

// =============================================================================
// NODE ID TRACKING
// =============================================================================

static int next_node_id = 1;

static int get_next_node_id(void) {
    return next_node_id++;
}

static void reset_node_id(void) {
    next_node_id = 1;
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static cJSON *create_location(int line, int column, const char *filename) {
    cJSON *loc = cJSON_CreateObject();
    cJSON_AddNumberToObject(loc, "line", line);
    cJSON_AddNumberToObject(loc, "column", column);
    cJSON_AddStringToObject(loc, "file", filename);
    return loc;
}

static bool is_symbol(KRLSExp *sexp, const char *name) {
    return sexp && sexp->type == KRL_SEXP_SYMBOL &&
           strcmp(sexp->data.symbol, name) == 0;
}

static bool starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

static bool is_color(const char *str) {
    if (str[0] != '#') return false;
    size_t len = strlen(str);
    if (len != 7 && len != 9) return false;  // #RRGGBB or #RRGGBBAA
    for (size_t i = 1; i < len; i++) {
        if (!isxdigit(str[i])) return false;
    }
    return true;
}

static bool is_unit_value(const char *str, double *out_value, char *out_unit, size_t unit_size) {
    char *endptr;
    double value = strtod(str, &endptr);

    if (endptr == str) return false;  // No number found
    if (*endptr == '\0') return false;  // No unit

    // Must have unit after number
    if (out_value) *out_value = value;
    if (out_unit) {
        strncpy(out_unit, endptr, unit_size - 1);
        out_unit[unit_size - 1] = '\0';
    }
    return true;
}

// Forward declarations
static cJSON *transpile_value(KRLSExp *sexp, const char *filename);
static cJSON *transpile_element(KRLSExp *sexp, const char *filename);

// =============================================================================
// VALUE TRANSPILATION
// =============================================================================

static cJSON *transpile_literal(KRLSExp *sexp, const char *filename) {
    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "LITERAL");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));

    switch (sexp->type) {
        case KRL_SEXP_STRING: {
            // Check if it's a color or unit value
            if (is_color(sexp->data.string)) {
                cJSON_AddStringToObject(node, "valueType", "COLOR");
                cJSON_AddStringToObject(node, "value", sexp->data.string);
            } else {
                double value;
                char unit[32];
                if (is_unit_value(sexp->data.string, &value, unit, sizeof(unit))) {
                    cJSON_AddStringToObject(node, "valueType", "UNIT");
                    cJSON_AddNumberToObject(node, "value", value);
                    cJSON_AddStringToObject(node, "unit", unit);
                } else {
                    cJSON_AddStringToObject(node, "valueType", "STRING");
                    cJSON_AddStringToObject(node, "value", sexp->data.string);
                }
            }
            break;
        }
        case KRL_SEXP_INTEGER:
            cJSON_AddStringToObject(node, "valueType", "INTEGER");
            cJSON_AddNumberToObject(node, "value", sexp->data.integer);
            break;
        case KRL_SEXP_FLOAT:
            cJSON_AddStringToObject(node, "valueType", "FLOAT");
            cJSON_AddNumberToObject(node, "value", sexp->data.float_val);
            break;
        case KRL_SEXP_BOOLEAN:
            cJSON_AddStringToObject(node, "valueType", "BOOLEAN");
            cJSON_AddBoolToObject(node, "value", sexp->data.boolean);
            break;
        case KRL_SEXP_SYMBOL:
            // Check if it's a color
            if (is_color(sexp->data.symbol)) {
                cJSON_AddStringToObject(node, "valueType", "COLOR");
                cJSON_AddStringToObject(node, "value", sexp->data.symbol);
            } else {
                // Check for boolean keywords
                if (strcmp(sexp->data.symbol, "true") == 0 || strcmp(sexp->data.symbol, "t") == 0) {
                    cJSON_AddStringToObject(node, "valueType", "BOOLEAN");
                    cJSON_AddBoolToObject(node, "value", true);
                } else if (strcmp(sexp->data.symbol, "false") == 0 || strcmp(sexp->data.symbol, "nil") == 0) {
                    cJSON_AddStringToObject(node, "valueType", "BOOLEAN");
                    cJSON_AddBoolToObject(node, "value", false);
                } else {
                    // Regular string symbol
                    cJSON_AddStringToObject(node, "valueType", "STRING");
                    cJSON_AddStringToObject(node, "value", sexp->data.symbol);
                }
            }
            break;
        default:
            cJSON_AddStringToObject(node, "valueType", "STRING");
            cJSON_AddStringToObject(node, "value", "");
            break;
    }

    return node;
}

static cJSON *transpile_variable(const char *var_name, int line, int column, const char *filename) {
    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "VARIABLE");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(line, column, filename));
    cJSON_AddStringToObject(node, "name", var_name + 1);  // Remove $
    return node;
}

static cJSON *transpile_binary_op(KRLSExp *sexp, const char *filename) {
    if (sexp->data.list.count < 3) return NULL;

    const char *op = sexp->data.list.items[0]->data.symbol;
    cJSON *left = transpile_value(sexp->data.list.items[1], filename);
    cJSON *right = transpile_value(sexp->data.list.items[2], filename);

    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "BINARY_OP");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));

    // Normalize operator
    if (strcmp(op, "and") == 0) {
        cJSON_AddStringToObject(node, "operator", "&&");
    } else if (strcmp(op, "or") == 0) {
        cJSON_AddStringToObject(node, "operator", "||");
    } else {
        cJSON_AddStringToObject(node, "operator", op);
    }

    cJSON_AddItemToObject(node, "left", left);
    cJSON_AddItemToObject(node, "right", right);

    return node;
}

static cJSON *transpile_member_access(KRLSExp *sexp, const char *filename) {
    if (sexp->data.list.count < 3) return NULL;

    cJSON *obj = transpile_value(sexp->data.list.items[1], filename);
    const char *prop = sexp->data.list.items[2]->data.symbol;

    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "MEMBER_ACCESS");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
    cJSON_AddItemToObject(node, "object", obj);
    cJSON_AddStringToObject(node, "property", prop);

    return node;
}

static cJSON *transpile_function_call(KRLSExp *sexp, const char *filename) {
    const char *func_name = sexp->data.list.items[0]->data.symbol;

    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "FUNCTION_CALL");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
    cJSON_AddStringToObject(node, "functionName", func_name);

    cJSON *args = cJSON_CreateArray();
    for (size_t i = 1; i < sexp->data.list.count; i++) {
        cJSON_AddItemToArray(args, transpile_value(sexp->data.list.items[i], filename));
    }
    cJSON_AddItemToObject(node, "arguments", args);

    return node;
}

static cJSON *transpile_expression(KRLSExp *sexp, const char *filename) {
    if (sexp->type != KRL_SEXP_LIST || sexp->data.list.count == 0) {
        return NULL;
    }

    const char *op = sexp->data.list.items[0]->data.symbol;

    // Binary operators
    const char *binary_ops[] = {"+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "and", "or"};
    for (size_t i = 0; i < sizeof(binary_ops) / sizeof(binary_ops[0]); i++) {
        if (strcmp(op, binary_ops[i]) == 0) {
            return transpile_binary_op(sexp, filename);
        }
    }

    // Member access
    if (strcmp(op, ".") == 0) {
        return transpile_member_access(sexp, filename);
    }

    // Array access
    if (strcmp(op, "[]") == 0 || strcmp(op, "at") == 0) {
        // Similar to member access but with index
        cJSON *array = transpile_value(sexp->data.list.items[1], filename);
        cJSON *index = transpile_value(sexp->data.list.items[2], filename);

        cJSON *node = cJSON_CreateObject();
        cJSON_AddStringToObject(node, "type", "ARRAY_ACCESS");
        cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
        cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
        cJSON_AddItemToObject(node, "array", array);
        cJSON_AddItemToObject(node, "index", index);
        return node;
    }

    // Function call
    return transpile_function_call(sexp, filename);
}

static cJSON *transpile_value(KRLSExp *sexp, const char *filename) {
    if (!sexp) return cJSON_CreateNull();

    // Variable reference
    if (sexp->type == KRL_SEXP_SYMBOL && sexp->data.symbol[0] == '$') {
        return transpile_variable(sexp->data.symbol, sexp->line, sexp->column, filename);
    }

    // Expression (list)
    if (sexp->type == KRL_SEXP_LIST && sexp->data.list.count > 0) {
        return transpile_expression(sexp, filename);
    }

    // Literal value
    return transpile_literal(sexp, filename);
}

// =============================================================================
// DIRECTIVE TRANSPILATION
// =============================================================================

static cJSON *transpile_property(const char *prop_name, KRLSExp *value, const char *filename, int line, int column) {
    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "PROPERTY");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(line, column, filename));
    cJSON_AddStringToObject(node, "name", prop_name);
    cJSON_AddItemToObject(node, "value", transpile_value(value, filename));
    return node;
}

static cJSON *transpile_style(KRLSExp *sexp, const char *filename) {
    if (sexp->data.list.count < 2) return NULL;

    const char *style_name = sexp->data.list.items[1]->data.string;

    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "STYLE_BLOCK");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
    cJSON_AddStringToObject(node, "name", style_name);

    cJSON *properties = cJSON_CreateArray();

    // Process property pairs (prop value)
    for (size_t i = 2; i < sexp->data.list.count; i++) {
        KRLSExp *item = sexp->data.list.items[i];
        if (item->type == KRL_SEXP_LIST && item->data.list.count == 2) {
            const char *prop_name = item->data.list.items[0]->data.symbol;
            KRLSExp *prop_value = item->data.list.items[1];

            // Remove : prefix if present
            if (prop_name[0] == ':') prop_name++;

            cJSON_AddItemToArray(properties, transpile_property(prop_name, prop_value, filename,
                                                               item->line, item->column));
        }
    }

    cJSON_AddItemToObject(node, "properties", properties);
    return node;
}

static cJSON *transpile_function(KRLSExp *sexp, const char *filename) {
    if (sexp->data.list.count < 5) return NULL;

    const char *language = sexp->data.list.items[1]->data.string;
    const char *name = sexp->data.list.items[2]->data.symbol;
    // params at index 3 (ignored for now)
    const char *code = sexp->data.list.items[4]->data.string;

    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "FUNCTION_DEFINITION");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
    cJSON_AddStringToObject(node, "language", language);
    cJSON_AddStringToObject(node, "name", name);
    cJSON_AddStringToObject(node, "code", code);

    return node;
}

static cJSON *transpile_for(KRLSExp *sexp, const char *filename) {
    if (sexp->data.list.count < 4) return NULL;

    const char *var_name = sexp->data.list.items[1]->data.symbol;
    // sexp->data.list.items[2] should be "in"
    const char *array_name = sexp->data.list.items[3]->data.symbol;

    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "FOR_DIRECTIVE");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
    cJSON_AddStringToObject(node, "varName", var_name);
    cJSON_AddStringToObject(node, "arrayName", array_name);

    cJSON *children = cJSON_CreateArray();
    for (size_t i = 4; i < sexp->data.list.count; i++) {
        cJSON *child = transpile_element(sexp->data.list.items[i], filename);
        if (child) cJSON_AddItemToArray(children, child);
    }
    cJSON_AddItemToObject(node, "children", children);

    return node;
}

static cJSON *transpile_if(KRLSExp *sexp, const char *filename) {
    if (sexp->data.list.count < 3) return NULL;

    cJSON *condition = transpile_value(sexp->data.list.items[1], filename);

    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "IF_DIRECTIVE");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
    cJSON_AddItemToObject(node, "condition", condition);

    cJSON *children = cJSON_CreateArray();
    for (size_t i = 2; i < sexp->data.list.count; i++) {
        cJSON *child = transpile_element(sexp->data.list.items[i], filename);
        if (child) cJSON_AddItemToArray(children, child);
    }
    cJSON_AddItemToObject(node, "children", children);

    return node;
}

static cJSON *transpile_const(KRLSExp *sexp, const char *filename) {
    if (sexp->data.list.count < 3) return NULL;

    const char *name = sexp->data.list.items[1]->data.symbol;
    cJSON *value = transpile_value(sexp->data.list.items[2], filename);

    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "CONST_DEFINITION");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
    cJSON_AddStringToObject(node, "name", name);
    cJSON_AddItemToObject(node, "value", value);

    return node;
}

static cJSON *transpile_directive(KRLSExp *sexp, const char *filename) {
    if (sexp->type != KRL_SEXP_LIST || sexp->data.list.count == 0) return NULL;

    const char *directive = sexp->data.list.items[0]->data.symbol;

    if (strcmp(directive, "for") == 0) {
        return transpile_for(sexp, filename);
    }
    if (strcmp(directive, "if") == 0) {
        return transpile_if(sexp, filename);
    }
    if (strcmp(directive, "const") == 0) {
        return transpile_const(sexp, filename);
    }
    if (strcmp(directive, "var") == 0) {
        cJSON *node = transpile_const(sexp, filename);  // Similar structure
        cJSON_ReplaceItemInObject(node, "type", cJSON_CreateString("VARIABLE_DEFINITION"));
        cJSON_ReplaceItemInObject(node, "value",
                                cJSON_DetachItemFromObject(node, "value"));
        cJSON_AddItemToObject(node, "initialValue",
                            cJSON_DetachItemFromObject(node, "value"));
        return node;
    }
    if (strcmp(directive, "state") == 0) {
        cJSON *node = transpile_const(sexp, filename);
        cJSON_ReplaceItemInObject(node, "type", cJSON_CreateString("STATE_DEFINITION"));
        cJSON *value = cJSON_DetachItemFromObject(node, "value");
        cJSON_AddItemToObject(node, "initialValue", value);
        return node;
    }

    return NULL;
}

// =============================================================================
// ELEMENT TRANSPILATION
// =============================================================================

static cJSON *transpile_element(KRLSExp *sexp, const char *filename) {
    if (sexp->type != KRL_SEXP_LIST || sexp->data.list.count == 0) {
        return NULL;
    }

    const char *head = sexp->data.list.items[0]->data.symbol;

    // Check for style
    if (strcmp(head, "style") == 0) {
        return transpile_style(sexp, filename);
    }

    // Check for function
    if (strcmp(head, "function") == 0) {
        return transpile_function(sexp, filename);
    }

    // Check for directives (const, var, for, if, state)
    if (strcmp(head, "const") == 0 || strcmp(head, "var") == 0 ||
        strcmp(head, "state") == 0 || strcmp(head, "for") == 0 ||
        strcmp(head, "if") == 0) {
        return transpile_directive(sexp, filename);
    }

    // Otherwise it's an element
    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "type", "ELEMENT");
    cJSON_AddNumberToObject(node, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(node, "location", create_location(sexp->line, sexp->column, filename));
    cJSON_AddStringToObject(node, "elementType", head);

    cJSON *properties = cJSON_CreateArray();
    cJSON *children = cJSON_CreateArray();

    // Process items - properties or children
    for (size_t i = 1; i < sexp->data.list.count; i++) {
        KRLSExp *item = sexp->data.list.items[i];

        if (item->type == KRL_SEXP_LIST) {
            // Check if it's a property (keyword-style: (:prop value) or (prop value))
            if (item->data.list.count == 2 && item->data.list.items[0]->type == KRL_SEXP_SYMBOL) {
                const char *first = item->data.list.items[0]->data.symbol;

                // Property if starts with : or lowercase letter
                if (first[0] == ':' || (first[0] >= 'a' && first[0] <= 'z')) {
                    const char *prop_name = first[0] == ':' ? first + 1 : first;
                    cJSON_AddItemToArray(properties, transpile_property(prop_name,
                                                                        item->data.list.items[1],
                                                                        filename, item->line, item->column));
                    continue;
                }
            }

            // Otherwise it's a child element
            cJSON *child = transpile_element(item, filename);
            if (child) cJSON_AddItemToArray(children, child);
        }
    }

    cJSON_AddItemToObject(node, "properties", properties);
    if (cJSON_GetArraySize(children) > 0) {
        cJSON_AddItemToObject(node, "children", children);
    } else {
        cJSON_Delete(children);
    }

    return node;
}

// =============================================================================
// TOP-LEVEL CONVERSION
// =============================================================================

static cJSON *create_kir_document(KRLSExp **sexps, size_t count, const char *source_file) {
    reset_node_id();

    cJSON *doc = cJSON_CreateObject();
    cJSON_AddStringToObject(doc, "version", "0.1.0");
    cJSON_AddStringToObject(doc, "format", "kir-json");

    // Metadata
    cJSON *metadata = cJSON_CreateObject();
    cJSON_AddStringToObject(metadata, "sourceFile", source_file);
    cJSON_AddStringToObject(metadata, "compiler", "krl-transpiler");
    cJSON_AddStringToObject(metadata, "compilerVersion", "0.1.0");

    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    cJSON_AddStringToObject(metadata, "timestamp", timestamp);

    cJSON_AddItemToObject(doc, "metadata", metadata);

    // Root node
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "ROOT");
    cJSON_AddNumberToObject(root, "nodeId", get_next_node_id());
    cJSON_AddItemToObject(root, "location", create_location(1, 1, source_file));

    cJSON *children = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *child = transpile_element(sexps[i], source_file);
        if (child) {
            cJSON_AddItemToArray(children, child);
        }
    }
    cJSON_AddItemToObject(root, "children", children);

    cJSON_AddItemToObject(doc, "root", root);

    return doc;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool krl_to_kir_file(KRLSExp **sexps, size_t count, const char *source_file, const char *output_path) {
    cJSON *doc = create_kir_document(sexps, count, source_file);

    char *json_str = cJSON_Print(doc);
    cJSON_Delete(doc);

    if (!json_str) {
        return false;
    }

    FILE *f = fopen(output_path, "w");
    if (!f) {
        free(json_str);
        return false;
    }

    fprintf(f, "%s\n", json_str);
    fclose(f);
    free(json_str);

    return true;
}

bool krl_compile_to_kir(const char *input_path, const char *output_path) {
    // Read source file
    FILE *f = fopen(input_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", input_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    // Parse
    KRLParser parser;
    krl_parser_init(&parser, source, input_path);

    size_t count;
    KRLSExp **sexps = krl_parse(&parser, &count);

    free(source);

    if (!sexps || parser.had_error) {
        fprintf(stderr, "Error: Failed to parse KRL file\n");
        return false;
    }

    // Convert to KIR
    bool success = krl_to_kir_file(sexps, count, input_path, output_path);

    // Clean up
    for (size_t i = 0; i < count; i++) {
        krl_sexp_free(sexps[i]);
    }
    free(sexps);

    return success;
}
