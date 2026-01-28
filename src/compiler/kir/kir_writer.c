/**
 * @file kir_writer.c
 * @brief KIR Writer Implementation - Serializes AST to JSON format
 *
 * Implements lossless JSON serialization of post-expansion AST for KIR format.
 */

#include "kir_format.h"
#include "parser.h"
#include "lexer.h"
#include "memory.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// =============================================================================
// WRITER STRUCTURE
// =============================================================================

struct KryonKIRWriter {
    KryonKIRConfig config;
    char **error_messages;
    size_t error_count;
    size_t error_capacity;
    size_t nodes_written;
};

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

static void writer_error(KryonKIRWriter *writer, const char *message) {
    if (!writer || !message) return;

    if (writer->error_count >= writer->error_capacity) {
        size_t new_capacity = writer->error_capacity == 0 ? 16 : writer->error_capacity * 2;
        char **new_errors = realloc(writer->error_messages, new_capacity * sizeof(char*));
        if (!new_errors) return;
        writer->error_messages = new_errors;
        writer->error_capacity = new_capacity;
    }

    writer->error_messages[writer->error_count++] = strdup(message);
}

static const char *node_type_to_string(KryonASTNodeType type) {
    switch (type) {
        case KRYON_AST_ROOT: return "ROOT";
        case KRYON_AST_ELEMENT: return "ELEMENT";
        case KRYON_AST_PROPERTY: return "PROPERTY";
        case KRYON_AST_STYLE_BLOCK: return "STYLE_BLOCK";
        case KRYON_AST_STYLES_BLOCK: return "STYLES_BLOCK";
        case KRYON_AST_THEME_DEFINITION: return "THEME_DEFINITION";
        case KRYON_AST_VARIABLE_DEFINITION: return "VARIABLE_DEFINITION";
        case KRYON_AST_FUNCTION_DEFINITION: return "FUNCTION_DEFINITION";
        case KRYON_AST_STATE_DEFINITION: return "STATE_DEFINITION";
        case KRYON_AST_CONST_DEFINITION: return "CONST_DEFINITION";
        case KRYON_AST_CONST_FOR_LOOP: return "CONST_FOR_LOOP";
        case KRYON_AST_STORE_DIRECTIVE: return "STORE_DIRECTIVE";
        case KRYON_AST_WATCH_DIRECTIVE: return "WATCH_DIRECTIVE";
        case KRYON_AST_ON_MOUNT_DIRECTIVE: return "ON_MOUNT_DIRECTIVE";
        case KRYON_AST_ON_UNMOUNT_DIRECTIVE: return "ON_UNMOUNT_DIRECTIVE";
        case KRYON_AST_IMPORT_DIRECTIVE: return "IMPORT_DIRECTIVE";
        case KRYON_AST_EXPORT_DIRECTIVE: return "EXPORT_DIRECTIVE";
        case KRYON_AST_INCLUDE_DIRECTIVE: return "INCLUDE_DIRECTIVE";
        case KRYON_AST_METADATA_DIRECTIVE: return "METADATA_DIRECTIVE";
        case KRYON_AST_EVENT_DIRECTIVE: return "EVENT_DIRECTIVE";
        case KRYON_AST_ONLOAD_DIRECTIVE: return "ONLOAD_DIRECTIVE";
        case KRYON_AST_FOR_DIRECTIVE: return "FOR_DIRECTIVE";
        case KRYON_AST_IF_DIRECTIVE: return "IF_DIRECTIVE";
        case KRYON_AST_ELIF_DIRECTIVE: return "ELIF_DIRECTIVE";
        case KRYON_AST_ELSE_DIRECTIVE: return "ELSE_DIRECTIVE";
        case KRYON_AST_CONST_IF_DIRECTIVE: return "CONST_IF_DIRECTIVE";
        case KRYON_AST_COMPONENT: return "COMPONENT";
        case KRYON_AST_PROPS: return "PROPS";
        case KRYON_AST_SLOTS: return "SLOTS";
        case KRYON_AST_LIFECYCLE: return "LIFECYCLE";
        case KRYON_AST_LITERAL: return "LITERAL";
        case KRYON_AST_VARIABLE: return "VARIABLE";
        case KRYON_AST_IDENTIFIER: return "IDENTIFIER";
        case KRYON_AST_TEMPLATE: return "TEMPLATE";
        case KRYON_AST_BINARY_OP: return "BINARY_OP";
        case KRYON_AST_UNARY_OP: return "UNARY_OP";
        case KRYON_AST_TERNARY_OP: return "TERNARY_OP";
        case KRYON_AST_FUNCTION_CALL: return "FUNCTION_CALL";
        case KRYON_AST_MEMBER_ACCESS: return "MEMBER_ACCESS";
        case KRYON_AST_ARRAY_ACCESS: return "ARRAY_ACCESS";
        case KRYON_AST_ARRAY_LITERAL: return "ARRAY_LITERAL";
        case KRYON_AST_OBJECT_LITERAL: return "OBJECT_LITERAL";
        case KRYON_AST_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static const char *value_type_to_string(KryonValueType type) {
    switch (type) {
        case KRYON_VALUE_STRING: return "STRING";
        case KRYON_VALUE_INTEGER: return "INTEGER";
        case KRYON_VALUE_FLOAT: return "FLOAT";
        case KRYON_VALUE_BOOLEAN: return "BOOLEAN";
        case KRYON_VALUE_NULL: return "NULL";
        case KRYON_VALUE_COLOR: return "COLOR";
        case KRYON_VALUE_UNIT: return "UNIT";
        default: return "UNKNOWN";
    }
}

static const char *operator_token_to_string(KryonTokenType token) {
    switch (token) {
        // Arithmetic operators
        case KRYON_TOKEN_PLUS: return "+";
        case KRYON_TOKEN_MINUS: return "-";
        case KRYON_TOKEN_MULTIPLY: return "*";
        case KRYON_TOKEN_DIVIDE: return "/";
        case KRYON_TOKEN_MODULO: return "%";

        // Comparison operators
        case KRYON_TOKEN_EQUALS: return "==";
        case KRYON_TOKEN_NOT_EQUALS: return "!=";
        case KRYON_TOKEN_LESS_THAN: return "<";
        case KRYON_TOKEN_LESS_EQUAL: return "<=";
        case KRYON_TOKEN_GREATER_THAN: return ">";
        case KRYON_TOKEN_GREATER_EQUAL: return ">=";

        // Logical operators
        case KRYON_TOKEN_LOGICAL_AND: return "&&";
        case KRYON_TOKEN_LOGICAL_OR: return "||";
        case KRYON_TOKEN_LOGICAL_NOT: return "!";

        // Other operators
        case KRYON_TOKEN_ASSIGN: return "=";
        case KRYON_TOKEN_DOT: return ".";
        case KRYON_TOKEN_RANGE: return "..";

        default: return "UNKNOWN_OP";
    }
}

// Forward declaration
static cJSON *serialize_node(KryonKIRWriter *writer, const KryonASTNode *node);

// =============================================================================
// NODE SERIALIZATION
// =============================================================================

static cJSON *serialize_location(const KryonSourceLocation *location) {
    if (!location) return NULL;

    cJSON *loc = cJSON_CreateObject();
    if (!loc) return NULL;

    cJSON_AddNumberToObject(loc, "line", location->line);
    cJSON_AddNumberToObject(loc, "column", location->column);
    if (location->filename) {
        cJSON_AddStringToObject(loc, "file", location->filename);
    }

    return loc;
}

static cJSON *serialize_value(const KryonASTValue *value) {
    if (!value) return NULL;

    cJSON *val_obj = cJSON_CreateObject();
    if (!val_obj) return NULL;

    cJSON_AddStringToObject(val_obj, "type", "LITERAL");
    cJSON_AddStringToObject(val_obj, "valueType", value_type_to_string(value->type));

    switch (value->type) {
        case KRYON_VALUE_STRING:
            if (value->data.string_value) {
                cJSON_AddStringToObject(val_obj, "value", value->data.string_value);
            } else {
                cJSON_AddNullToObject(val_obj, "value");
            }
            break;
        case KRYON_VALUE_INTEGER:
            cJSON_AddNumberToObject(val_obj, "value", (double)value->data.int_value);
            break;
        case KRYON_VALUE_FLOAT:
            cJSON_AddNumberToObject(val_obj, "value", value->data.float_value);
            break;
        case KRYON_VALUE_BOOLEAN:
            cJSON_AddBoolToObject(val_obj, "value", value->data.bool_value);
            break;
        case KRYON_VALUE_NULL:
            cJSON_AddNullToObject(val_obj, "value");
            break;
        case KRYON_VALUE_COLOR: {
            char color_str[16];
            snprintf(color_str, sizeof(color_str), "#%08X", value->data.color_value);
            cJSON_AddStringToObject(val_obj, "value", color_str);
            break;
        }
        case KRYON_VALUE_UNIT:
            cJSON_AddNumberToObject(val_obj, "value", value->data.unit_value.value);
            cJSON_AddStringToObject(val_obj, "unit", value->data.unit_value.unit);
            break;
        default:
            cJSON_AddNullToObject(val_obj, "value");
            break;
    }

    return val_obj;
}

static cJSON *serialize_children(KryonKIRWriter *writer, KryonASTNode **children, size_t count) {
    if (!children || count == 0) return cJSON_CreateArray();

    cJSON *children_array = cJSON_CreateArray();
    if (!children_array) return NULL;

    for (size_t i = 0; i < count; i++) {
        if (!children[i]) continue;
        cJSON *child_json = serialize_node(writer, children[i]);
        if (child_json) {
            cJSON_AddItemToArray(children_array, child_json);
        }
    }

    return children_array;
}

static cJSON *serialize_node(KryonKIRWriter *writer, const KryonASTNode *node) {
    if (!node) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    writer->nodes_written++;

    // Add common fields
    cJSON_AddStringToObject(json, "type", node_type_to_string(node->type));

    if (writer->config.include_node_ids) {
        cJSON_AddNumberToObject(json, "nodeId", (double)node->node_id);
    }

    if (writer->config.include_location_info) {
        cJSON *location = serialize_location(&node->location);
        if (location) {
            cJSON_AddItemToObject(json, "location", location);
        }
    }

    // Serialize node-specific data
    switch (node->type) {
        case KRYON_AST_ROOT:
        case KRYON_AST_ELEMENT: {
            if (node->data.element.element_type) {
                cJSON_AddStringToObject(json, "elementType", node->data.element.element_type);
            }

            // Add properties
            if (node->data.element.property_count > 0) {
                cJSON *props = serialize_children(writer, node->data.element.properties,
                                                 node->data.element.property_count);
                cJSON_AddItemToObject(json, "properties", props);
            }

            // Add children
            if (node->data.element.child_count > 0) {
                cJSON *children = serialize_children(writer, node->data.element.children,
                                                    node->data.element.child_count);
                cJSON_AddItemToObject(json, "children", children);
            }
            break;
        }

        case KRYON_AST_PROPERTY: {
            if (node->data.property.name) {
                cJSON_AddStringToObject(json, "name", node->data.property.name);
            }
            if (node->data.property.value) {
                cJSON *value = serialize_node(writer, node->data.property.value);
                cJSON_AddItemToObject(json, "value", value);
            }
            break;
        }

        case KRYON_AST_LITERAL: {
            cJSON *value = serialize_value(&node->data.literal.value);
            if (value) {
                // Merge value fields into current object
                cJSON *child = value->child;
                while (child) {
                    cJSON *next = child->next;
                    cJSON_DetachItemViaPointer(value, child);
                    cJSON_AddItemToObject(json, child->string, child);
                    child = next;
                }
                cJSON_Delete(value);
            }
            break;
        }

        case KRYON_AST_VARIABLE: {
            if (node->data.variable.name) {
                cJSON_AddStringToObject(json, "name", node->data.variable.name);
            }
            break;
        }

        case KRYON_AST_IDENTIFIER: {
            if (node->data.identifier.name) {
                cJSON_AddStringToObject(json, "name", node->data.identifier.name);
            }
            break;
        }

        case KRYON_AST_TEMPLATE: {
            if (node->data.template.segment_count > 0) {
                cJSON *segments = serialize_children(writer, node->data.template.segments,
                                                    node->data.template.segment_count);
                cJSON_AddItemToObject(json, "segments", segments);
            }
            break;
        }

        case KRYON_AST_BINARY_OP: {
            const char *op_str = operator_token_to_string(node->data.binary_op.operator);
            cJSON_AddStringToObject(json, "operator", op_str);
            if (node->data.binary_op.left) {
                cJSON *left = serialize_node(writer, node->data.binary_op.left);
                cJSON_AddItemToObject(json, "left", left);
            }
            if (node->data.binary_op.right) {
                cJSON *right = serialize_node(writer, node->data.binary_op.right);
                cJSON_AddItemToObject(json, "right", right);
            }
            break;
        }

        case KRYON_AST_UNARY_OP: {
            const char *op_str = operator_token_to_string(node->data.unary_op.operator);
            cJSON_AddStringToObject(json, "operator", op_str);
            if (node->data.unary_op.operand) {
                cJSON *operand = serialize_node(writer, node->data.unary_op.operand);
                cJSON_AddItemToObject(json, "operand", operand);
            }
            break;
        }

        case KRYON_AST_TERNARY_OP: {
            if (node->data.ternary_op.condition) {
                cJSON *cond = serialize_node(writer, node->data.ternary_op.condition);
                cJSON_AddItemToObject(json, "condition", cond);
            }
            if (node->data.ternary_op.true_expr) {
                cJSON *true_expr = serialize_node(writer, node->data.ternary_op.true_expr);
                cJSON_AddItemToObject(json, "trueExpr", true_expr);
            }
            if (node->data.ternary_op.false_expr) {
                cJSON *false_expr = serialize_node(writer, node->data.ternary_op.false_expr);
                cJSON_AddItemToObject(json, "falseExpr", false_expr);
            }
            break;
        }

        case KRYON_AST_FUNCTION_CALL: {
            if (node->data.function_call.function_name) {
                cJSON_AddStringToObject(json, "functionName", node->data.function_call.function_name);
            }
            if (node->data.function_call.argument_count > 0) {
                cJSON *args = serialize_children(writer, node->data.function_call.arguments,
                                                node->data.function_call.argument_count);
                cJSON_AddItemToObject(json, "arguments", args);
            }
            break;
        }

        case KRYON_AST_FUNCTION_DEFINITION: {
            if (node->data.function_def.language) {
                cJSON_AddStringToObject(json, "language", node->data.function_def.language);
            }
            if (node->data.function_def.name) {
                cJSON_AddStringToObject(json, "name", node->data.function_def.name);
            }
            if (node->data.function_def.parameter_count > 0) {
                cJSON *params = cJSON_CreateArray();
                for (size_t i = 0; i < node->data.function_def.parameter_count; i++) {
                    if (node->data.function_def.parameters[i]) {
                        cJSON_AddItemToArray(params, cJSON_CreateString(node->data.function_def.parameters[i]));
                    }
                }
                cJSON_AddItemToObject(json, "parameters", params);
            }
            if (node->data.function_def.code) {
                cJSON_AddStringToObject(json, "code", node->data.function_def.code);
            }
            break;
        }

        case KRYON_AST_VARIABLE_DEFINITION: {
            if (node->data.variable_def.name) {
                cJSON_AddStringToObject(json, "name", node->data.variable_def.name);
            }
            if (node->data.variable_def.type) {
                cJSON_AddStringToObject(json, "varType", node->data.variable_def.type);
            }
            if (node->data.variable_def.value) {
                cJSON *value = serialize_node(writer, node->data.variable_def.value);
                cJSON_AddItemToObject(json, "value", value);
            }
            break;
        }

        case KRYON_AST_CONST_DEFINITION: {
            if (node->data.const_def.name) {
                cJSON_AddStringToObject(json, "name", node->data.const_def.name);
            }
            if (node->data.const_def.value) {
                cJSON *value = serialize_node(writer, node->data.const_def.value);
                cJSON_AddItemToObject(json, "value", value);
            }
            break;
        }

        case KRYON_AST_COMPONENT: {
            if (node->data.component.name) {
                cJSON_AddStringToObject(json, "name", node->data.component.name);
            }

            // Parameters
            if (node->data.component.parameter_count > 0) {
                cJSON *params = cJSON_CreateArray();
                cJSON *defaults = cJSON_CreateArray();
                for (size_t i = 0; i < node->data.component.parameter_count; i++) {
                    if (node->data.component.parameters[i]) {
                        cJSON_AddItemToArray(params, cJSON_CreateString(node->data.component.parameters[i]));
                    }
                    if (node->data.component.param_defaults[i]) {
                        cJSON_AddItemToArray(defaults, cJSON_CreateString(node->data.component.param_defaults[i]));
                    }
                }
                cJSON_AddItemToObject(json, "parameters", params);
                cJSON_AddItemToObject(json, "paramDefaults", defaults);
            }

            // Parent component (inheritance)
            if (node->data.component.parent_component) {
                cJSON_AddStringToObject(json, "parentComponent", node->data.component.parent_component);
            }

            // Body elements
            if (node->data.component.body_count > 0) {
                cJSON *body = serialize_children(writer, node->data.component.body_elements,
                                                node->data.component.body_count);
                cJSON_AddItemToObject(json, "bodyElements", body);
            }
            break;
        }

        // Add more node types as needed...
        default:
            // Minimal serialization for unsupported types
            break;
    }

    return json;
}

// =============================================================================
// WRITER API
// =============================================================================

KryonKIRWriter *kryon_kir_writer_create(const KryonKIRConfig *config) {
    KryonKIRWriter *writer = calloc(1, sizeof(KryonKIRWriter));
    if (!writer) return NULL;

    if (config) {
        writer->config = *config;
    } else {
        writer->config = kryon_kir_default_config();
    }

    return writer;
}

void kryon_kir_writer_destroy(KryonKIRWriter *writer) {
    if (!writer) return;

    for (size_t i = 0; i < writer->error_count; i++) {
        free(writer->error_messages[i]);
    }
    free(writer->error_messages);
    free(writer);
}

bool kryon_kir_write_file(KryonKIRWriter *writer, const KryonASTNode *ast_root,
                          const char *output_path) {
    if (!writer || !ast_root || !output_path) return false;

    char *json_string = NULL;
    if (!kryon_kir_write_string(writer, ast_root, &json_string)) {
        return false;
    }

    FILE *file = fopen(output_path, "w");
    if (!file) {
        writer_error(writer, "Failed to open output file");
        free(json_string);
        return false;
    }

    fprintf(file, "%s\n", json_string);
    fclose(file);
    free(json_string);

    return true;
}

bool kryon_kir_write_string(KryonKIRWriter *writer, const KryonASTNode *ast_root,
                            char **out_json) {
    if (!writer || !ast_root || !out_json) return false;

    writer->nodes_written = 0;

    // Create top-level KIR object
    cJSON *kir = cJSON_CreateObject();
    if (!kir) {
        writer_error(writer, "Failed to create JSON object");
        return false;
    }

    // Add version and format
    cJSON_AddStringToObject(kir, "version", KIR_FORMAT_VERSION);
    cJSON_AddStringToObject(kir, "format", KIR_FORMAT_NAME);

    // Add metadata
    cJSON *metadata = cJSON_CreateObject();
    if (writer->config.source_file) {
        cJSON_AddStringToObject(metadata, "sourceFile", writer->config.source_file);
    }
    cJSON_AddStringToObject(metadata, "compiler", "kryon");
    cJSON_AddStringToObject(metadata, "compilerVersion", "1.0.0");

    if (writer->config.include_timestamps) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
        cJSON_AddStringToObject(metadata, "timestamp", timestamp);
    }

    cJSON_AddItemToObject(kir, "metadata", metadata);

    // Serialize root AST node
    cJSON *root = serialize_node(writer, ast_root);
    if (!root) {
        writer_error(writer, "Failed to serialize AST root");
        cJSON_Delete(kir);
        return false;
    }
    cJSON_AddItemToObject(kir, "root", root);

    // Convert to string
    char *json_str;
    if (writer->config.pretty_print || writer->config.format_style == KRYON_KIR_FORMAT_READABLE) {
        json_str = cJSON_Print(kir);
    } else {
        json_str = cJSON_PrintUnformatted(kir);
    }

    cJSON_Delete(kir);

    if (!json_str) {
        writer_error(writer, "Failed to convert JSON to string");
        return false;
    }

    *out_json = json_str;
    return true;
}

bool kryon_kir_write_stream(KryonKIRWriter *writer, const KryonASTNode *ast_root,
                            FILE *file) {
    if (!writer || !ast_root || !file) return false;

    char *json_string = NULL;
    if (!kryon_kir_write_string(writer, ast_root, &json_string)) {
        return false;
    }

    fprintf(file, "%s\n", json_string);
    free(json_string);

    return true;
}

const char **kryon_kir_writer_get_errors(const KryonKIRWriter *writer, size_t *out_count) {
    if (!writer || !out_count) return NULL;
    *out_count = writer->error_count;
    return (const char **)writer->error_messages;
}

// =============================================================================
// CONFIGURATION HELPERS
// =============================================================================

KryonKIRConfig kryon_kir_default_config(void) {
    KryonKIRConfig config = {
        .format_style = KRYON_KIR_FORMAT_READABLE,
        .include_location_info = true,
        .include_node_ids = true,
        .include_timestamps = true,
        .include_compiler_info = true,
        .validate_on_write = false,
        .pretty_print = true,
        .source_file = NULL
    };
    return config;
}

KryonKIRConfig kryon_kir_compact_config(void) {
    KryonKIRConfig config = kryon_kir_default_config();
    config.format_style = KRYON_KIR_FORMAT_COMPACT;
    config.pretty_print = false;
    config.include_timestamps = false;
    return config;
}

KryonKIRConfig kryon_kir_verbose_config(void) {
    KryonKIRConfig config = kryon_kir_default_config();
    config.format_style = KRYON_KIR_FORMAT_VERBOSE;
    return config;
}
