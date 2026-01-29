/**

 * @file kir_reader.c
 * @brief KIR Reader Implementation - Parses JSON to AST
 *
 * Implements JSON deserialization to reconstruct AST from KIR format.
 */
#include "lib9.h"


#include "kir_format.h"
#include "parser.h"
#include "lexer.h"
#include "memory.h"
#include "cJSON.h"
#include <stdbool.h>

// =============================================================================
// READER STRUCTURE
// =============================================================================

struct KryonKIRReader {
    KryonKIRConfig config;
    char **error_messages;
    size_t error_count;
    size_t error_capacity;
    size_t nodes_read;
    int format_major;
    int format_minor;
    int format_patch;
};

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

static void reader_error(KryonKIRReader *reader, const char *message) {
    if (!reader || !message) return;

    if (reader->error_count >= reader->error_capacity) {
        size_t new_capacity = reader->error_capacity == 0 ? 16 : reader->error_capacity * 2;
        char **new_errors = realloc(reader->error_messages, new_capacity * sizeof(char*));
        if (!new_errors) return;
        reader->error_messages = new_errors;
        reader->error_capacity = new_capacity;
    }

    reader->error_messages[reader->error_count++] = strdup(message);
}

static KryonASTNodeType string_to_node_type(const char *type_str) {
    if (!type_str) return KRYON_AST_ERROR;

    if (strcmp(type_str, "ROOT") == 0) return KRYON_AST_ROOT;
    if (strcmp(type_str, "ELEMENT") == 0) return KRYON_AST_ELEMENT;
    if (strcmp(type_str, "PROPERTY") == 0) return KRYON_AST_PROPERTY;
    if (strcmp(type_str, "STYLE_BLOCK") == 0) return KRYON_AST_STYLE_BLOCK;
    if (strcmp(type_str, "STYLES_BLOCK") == 0) return KRYON_AST_STYLES_BLOCK;
    if (strcmp(type_str, "THEME_DEFINITION") == 0) return KRYON_AST_THEME_DEFINITION;
    if (strcmp(type_str, "VARIABLE_DEFINITION") == 0) return KRYON_AST_VARIABLE_DEFINITION;
    if (strcmp(type_str, "FUNCTION_DEFINITION") == 0) return KRYON_AST_FUNCTION_DEFINITION;
    if (strcmp(type_str, "STATE_DEFINITION") == 0) return KRYON_AST_STATE_DEFINITION;
    if (strcmp(type_str, "CONST_DEFINITION") == 0) return KRYON_AST_CONST_DEFINITION;
    if (strcmp(type_str, "FOR_DIRECTIVE") == 0) return KRYON_AST_FOR_DIRECTIVE;
    if (strcmp(type_str, "IF_DIRECTIVE") == 0) return KRYON_AST_IF_DIRECTIVE;
    if (strcmp(type_str, "COMPONENT") == 0) return KRYON_AST_COMPONENT;
    if (strcmp(type_str, "LITERAL") == 0) return KRYON_AST_LITERAL;
    if (strcmp(type_str, "VARIABLE") == 0) return KRYON_AST_VARIABLE;
    if (strcmp(type_str, "IDENTIFIER") == 0) return KRYON_AST_IDENTIFIER;
    if (strcmp(type_str, "TEMPLATE") == 0) return KRYON_AST_TEMPLATE;
    if (strcmp(type_str, "BINARY_OP") == 0) return KRYON_AST_BINARY_OP;
    if (strcmp(type_str, "UNARY_OP") == 0) return KRYON_AST_UNARY_OP;
    if (strcmp(type_str, "TERNARY_OP") == 0) return KRYON_AST_TERNARY_OP;
    if (strcmp(type_str, "FUNCTION_CALL") == 0) return KRYON_AST_FUNCTION_CALL;
    if (strcmp(type_str, "MEMBER_ACCESS") == 0) return KRYON_AST_MEMBER_ACCESS;
    if (strcmp(type_str, "ARRAY_ACCESS") == 0) return KRYON_AST_ARRAY_ACCESS;
    if (strcmp(type_str, "ARRAY_LITERAL") == 0) return KRYON_AST_ARRAY_LITERAL;
    if (strcmp(type_str, "OBJECT_LITERAL") == 0) return KRYON_AST_OBJECT_LITERAL;

    return KRYON_AST_ERROR;
}

static KryonValueType string_to_value_type(const char *type_str) {
    if (!type_str) return KRYON_VALUE_NULL;

    if (strcmp(type_str, "STRING") == 0) return KRYON_VALUE_STRING;
    if (strcmp(type_str, "INTEGER") == 0) return KRYON_VALUE_INTEGER;
    if (strcmp(type_str, "FLOAT") == 0) return KRYON_VALUE_FLOAT;
    if (strcmp(type_str, "BOOLEAN") == 0) return KRYON_VALUE_BOOLEAN;
    if (strcmp(type_str, "NULL") == 0) return KRYON_VALUE_NULL;
    if (strcmp(type_str, "COLOR") == 0) return KRYON_VALUE_COLOR;
    if (strcmp(type_str, "UNIT") == 0) return KRYON_VALUE_UNIT;

    return KRYON_VALUE_NULL;
}

static KryonTokenType string_to_operator_token(const char *op_str) {
    if (!op_str) return KRYON_TOKEN_EOF;

    // Arithmetic operators
    if (strcmp(op_str, "+") == 0) return KRYON_TOKEN_PLUS;
    if (strcmp(op_str, "-") == 0) return KRYON_TOKEN_MINUS;
    if (strcmp(op_str, "*") == 0) return KRYON_TOKEN_MULTIPLY;
    if (strcmp(op_str, "/") == 0) return KRYON_TOKEN_DIVIDE;
    if (strcmp(op_str, "%") == 0) return KRYON_TOKEN_MODULO;

    // Comparison operators
    if (strcmp(op_str, "==") == 0) return KRYON_TOKEN_EQUALS;
    if (strcmp(op_str, "!=") == 0) return KRYON_TOKEN_NOT_EQUALS;
    if (strcmp(op_str, "<") == 0) return KRYON_TOKEN_LESS_THAN;
    if (strcmp(op_str, "<=") == 0) return KRYON_TOKEN_LESS_EQUAL;
    if (strcmp(op_str, ">") == 0) return KRYON_TOKEN_GREATER_THAN;
    if (strcmp(op_str, ">=") == 0) return KRYON_TOKEN_GREATER_EQUAL;

    // Logical operators
    if (strcmp(op_str, "&&") == 0) return KRYON_TOKEN_LOGICAL_AND;
    if (strcmp(op_str, "||") == 0) return KRYON_TOKEN_LOGICAL_OR;
    if (strcmp(op_str, "!") == 0) return KRYON_TOKEN_LOGICAL_NOT;

    // Other operators
    if (strcmp(op_str, "=") == 0) return KRYON_TOKEN_ASSIGN;
    if (strcmp(op_str, ".") == 0) return KRYON_TOKEN_DOT;
    if (strcmp(op_str, "..") == 0) return KRYON_TOKEN_RANGE;

    return KRYON_TOKEN_EOF;
}

// Forward declaration
static KryonASTNode *deserialize_node(KryonKIRReader *reader, cJSON *json);

// =============================================================================
// NODE DESERIALIZATION
// =============================================================================

static void parse_location(cJSON *loc_json, KryonSourceLocation *location) {
    if (!loc_json || !location) return;

    cJSON *line = cJSON_GetObjectItem(loc_json, "line");
    cJSON *column = cJSON_GetObjectItem(loc_json, "column");
    cJSON *file = cJSON_GetObjectItem(loc_json, "file");

    if (line && cJSON_IsNumber(line)) {
        location->line = (size_t)line->valueint;
    }
    if (column && cJSON_IsNumber(column)) {
        location->column = (size_t)column->valueint;
    }
    if (file && cJSON_IsString(file)) {
        location->filename = strdup(file->valuestring);
    }
}

static KryonASTValue parse_value(cJSON *json) {
    KryonASTValue value = {0};

    cJSON *value_type = cJSON_GetObjectItem(json, "valueType");
    if (!value_type || !cJSON_IsString(value_type)) {
        value.type = KRYON_VALUE_NULL;
        return value;
    }

    value.type = string_to_value_type(value_type->valuestring);

    cJSON *val = cJSON_GetObjectItem(json, "value");

    switch (value.type) {
        case KRYON_VALUE_STRING:
            if (val && cJSON_IsString(val)) {
                value.data.string_value = strdup(val->valuestring);
            }
            break;

        case KRYON_VALUE_INTEGER:
            if (val && cJSON_IsNumber(val)) {
                value.data.int_value = (int64_t)val->valuedouble;
            }
            break;

        case KRYON_VALUE_FLOAT:
            if (val && cJSON_IsNumber(val)) {
                value.data.float_value = val->valuedouble;
            }
            break;

        case KRYON_VALUE_BOOLEAN:
            if (val) {
                value.data.bool_value = cJSON_IsTrue(val);
            }
            break;

        case KRYON_VALUE_COLOR:
            if (val && cJSON_IsString(val)) {
                // Parse hex color string (e.g., "#FF5733FF")
                unsigned int color;
                if (sscanf(val->valuestring, "#%X", &color) == 1) {
                    value.data.color_value = color;
                }
            }
            break;

        case KRYON_VALUE_UNIT: {
            if (val && cJSON_IsNumber(val)) {
                value.data.unit_value.value = val->valuedouble;
            }
            cJSON *unit = cJSON_GetObjectItem(json, "unit");
            if (unit && cJSON_IsString(unit)) {
                strncpy(value.data.unit_value.unit, unit->valuestring, 7);
                value.data.unit_value.unit[7] = '\0';
            }
            break;
        }

        case KRYON_VALUE_NULL:
        default:
            break;
    }

    return value;
}

static KryonASTNode **deserialize_array(KryonKIRReader *reader, cJSON *array, size_t *out_count) {
    if (!array || !cJSON_IsArray(array)) {
        *out_count = 0;
        return NULL;
    }

    size_t count = cJSON_GetArraySize(array);
    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    KryonASTNode **nodes = calloc(count, sizeof(KryonASTNode*));
    if (!nodes) {
        *out_count = 0;
        return NULL;
    }

    size_t valid_count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        KryonASTNode *node = deserialize_node(reader, item);
        if (node) {
            nodes[valid_count++] = node;
        }
    }

    *out_count = valid_count;
    return nodes;
}

static KryonASTNode *deserialize_node(KryonKIRReader *reader, cJSON *json) {
    if (!json || !cJSON_IsObject(json)) {
        reader_error(reader, "Invalid JSON node");
        return NULL;
    }

    reader->nodes_read++;

    // Get node type
    cJSON *type_json = cJSON_GetObjectItem(json, "type");
    if (!type_json || !cJSON_IsString(type_json)) {
        reader_error(reader, "Missing or invalid 'type' field");
        return NULL;
    }

    KryonASTNodeType type = string_to_node_type(type_json->valuestring);
    if (type == KRYON_AST_ERROR) {
        char error_msg[256];
        snprint(error_msg, sizeof(error_msg), "Unknown node type: %s", type_json->valuestring);
        reader_error(reader, error_msg);
        return NULL;
    }

    // Allocate node
    KryonASTNode *node = calloc(1, sizeof(KryonASTNode));
    if (!node) {
        reader_error(reader, "Failed to allocate AST node");
        return NULL;
    }
    node->type = type;

    // Parse common fields
    cJSON *node_id = cJSON_GetObjectItem(json, "nodeId");
    if (node_id && cJSON_IsNumber(node_id)) {
        node->node_id = (size_t)node_id->valuedouble;
    }

    cJSON *location = cJSON_GetObjectItem(json, "location");
    if (location) {
        parse_location(location, &node->location);
    }

    // Parse node-specific data
    switch (type) {
        case KRYON_AST_ROOT:
        case KRYON_AST_ELEMENT: {
            cJSON *element_type = cJSON_GetObjectItem(json, "elementType");
            if (element_type && cJSON_IsString(element_type)) {
                node->data.element.element_type = strdup(element_type->valuestring);
            }

            // Parse properties
            cJSON *properties = cJSON_GetObjectItem(json, "properties");
            if (properties) {
                node->data.element.properties = deserialize_array(reader, properties,
                                                                  &node->data.element.property_count);
                node->data.element.property_capacity = node->data.element.property_count;
            }

            // Parse children
            cJSON *children = cJSON_GetObjectItem(json, "children");
            if (children) {
                node->data.element.children = deserialize_array(reader, children,
                                                               &node->data.element.child_count);
                node->data.element.child_capacity = node->data.element.child_count;
            }
            break;
        }

        case KRYON_AST_PROPERTY: {
            cJSON *name = cJSON_GetObjectItem(json, "name");
            if (name && cJSON_IsString(name)) {
                node->data.property.name = strdup(name->valuestring);
            }

            cJSON *value = cJSON_GetObjectItem(json, "value");
            if (value) {
                node->data.property.value = deserialize_node(reader, value);
            }
            break;
        }

        case KRYON_AST_LITERAL: {
            node->data.literal.value = parse_value(json);
            break;
        }

        case KRYON_AST_VARIABLE: {
            cJSON *name = cJSON_GetObjectItem(json, "name");
            if (name && cJSON_IsString(name)) {
                node->data.variable.name = strdup(name->valuestring);
            }
            break;
        }

        case KRYON_AST_IDENTIFIER: {
            cJSON *name = cJSON_GetObjectItem(json, "name");
            if (name && cJSON_IsString(name)) {
                node->data.identifier.name = strdup(name->valuestring);
            }
            break;
        }

        case KRYON_AST_TEMPLATE: {
            cJSON *segments = cJSON_GetObjectItem(json, "segments");
            if (segments) {
                node->data.template.segments = deserialize_array(reader, segments,
                                                                &node->data.template.segment_count);
                node->data.template.segment_capacity = node->data.template.segment_count;
            }
            break;
        }

        case KRYON_AST_BINARY_OP: {
            cJSON *op = cJSON_GetObjectItem(json, "operator");
            if (op && cJSON_IsString(op)) {
                node->data.binary_op.operator = string_to_operator_token(op->valuestring);
            }

            cJSON *left = cJSON_GetObjectItem(json, "left");
            if (left) {
                node->data.binary_op.left = deserialize_node(reader, left);
            }

            cJSON *right = cJSON_GetObjectItem(json, "right");
            if (right) {
                node->data.binary_op.right = deserialize_node(reader, right);
            }
            break;
        }

        case KRYON_AST_UNARY_OP: {
            cJSON *op = cJSON_GetObjectItem(json, "operator");
            if (op && cJSON_IsString(op)) {
                node->data.unary_op.operator = string_to_operator_token(op->valuestring);
            }

            cJSON *operand = cJSON_GetObjectItem(json, "operand");
            if (operand) {
                node->data.unary_op.operand = deserialize_node(reader, operand);
            }
            break;
        }

        case KRYON_AST_TERNARY_OP: {
            cJSON *condition = cJSON_GetObjectItem(json, "condition");
            if (condition) {
                node->data.ternary_op.condition = deserialize_node(reader, condition);
            }

            cJSON *true_expr = cJSON_GetObjectItem(json, "trueExpr");
            if (true_expr) {
                node->data.ternary_op.true_expr = deserialize_node(reader, true_expr);
            }

            cJSON *false_expr = cJSON_GetObjectItem(json, "falseExpr");
            if (false_expr) {
                node->data.ternary_op.false_expr = deserialize_node(reader, false_expr);
            }
            break;
        }

        case KRYON_AST_FUNCTION_CALL: {
            cJSON *func_name = cJSON_GetObjectItem(json, "functionName");
            if (func_name && cJSON_IsString(func_name)) {
                node->data.function_call.function_name = strdup(func_name->valuestring);
            }

            cJSON *arguments = cJSON_GetObjectItem(json, "arguments");
            if (arguments) {
                node->data.function_call.arguments = deserialize_array(reader, arguments,
                                                                       &node->data.function_call.argument_count);
                node->data.function_call.argument_capacity = node->data.function_call.argument_count;
            }
            break;
        }

        case KRYON_AST_FUNCTION_DEFINITION: {
            cJSON *language = cJSON_GetObjectItem(json, "language");
            if (language && cJSON_IsString(language)) {
                node->data.function_def.language = strdup(language->valuestring);
            }

            cJSON *name = cJSON_GetObjectItem(json, "name");
            if (name && cJSON_IsString(name)) {
                node->data.function_def.name = strdup(name->valuestring);
            }

            cJSON *parameters = cJSON_GetObjectItem(json, "parameters");
            if (parameters && cJSON_IsArray(parameters)) {
                size_t param_count = cJSON_GetArraySize(parameters);
                node->data.function_def.parameter_count = param_count;
                node->data.function_def.parameters = calloc(param_count, sizeof(char*));

                size_t i = 0;
                cJSON *param = NULL;
                cJSON_ArrayForEach(param, parameters) {
                    if (cJSON_IsString(param)) {
                        node->data.function_def.parameters[i++] = strdup(param->valuestring);
                    }
                }
            }

            cJSON *code = cJSON_GetObjectItem(json, "code");
            if (code && cJSON_IsString(code)) {
                node->data.function_def.code = strdup(code->valuestring);
            }
            break;
        }

        case KRYON_AST_VARIABLE_DEFINITION: {
            cJSON *name = cJSON_GetObjectItem(json, "name");
            if (name && cJSON_IsString(name)) {
                node->data.variable_def.name = strdup(name->valuestring);
            }

            cJSON *var_type = cJSON_GetObjectItem(json, "varType");
            if (var_type && cJSON_IsString(var_type)) {
                node->data.variable_def.type = strdup(var_type->valuestring);
            }

            cJSON *value = cJSON_GetObjectItem(json, "value");
            if (value) {
                node->data.variable_def.value = deserialize_node(reader, value);
            }
            break;
        }

        case KRYON_AST_CONST_DEFINITION: {
            cJSON *name = cJSON_GetObjectItem(json, "name");
            if (name && cJSON_IsString(name)) {
                node->data.const_def.name = strdup(name->valuestring);
            }

            cJSON *value = cJSON_GetObjectItem(json, "value");
            if (value) {
                node->data.const_def.value = deserialize_node(reader, value);
            }
            break;
        }

        case KRYON_AST_COMPONENT: {
            cJSON *name = cJSON_GetObjectItem(json, "name");
            if (name && cJSON_IsString(name)) {
                node->data.component.name = strdup(name->valuestring);
            }

            cJSON *parameters = cJSON_GetObjectItem(json, "parameters");
            cJSON *param_defaults = cJSON_GetObjectItem(json, "paramDefaults");

            if (parameters && cJSON_IsArray(parameters)) {
                size_t param_count = cJSON_GetArraySize(parameters);
                node->data.component.parameter_count = param_count;
                node->data.component.parameters = calloc(param_count, sizeof(char*));
                node->data.component.param_defaults = calloc(param_count, sizeof(char*));

                size_t i = 0;
                cJSON *param = NULL;
                cJSON_ArrayForEach(param, parameters) {
                    if (cJSON_IsString(param)) {
                        node->data.component.parameters[i++] = strdup(param->valuestring);
                    }
                }

                if (param_defaults && cJSON_IsArray(param_defaults)) {
                    i = 0;
                    cJSON *def = NULL;
                    cJSON_ArrayForEach(def, param_defaults) {
                        if (cJSON_IsString(def)) {
                            node->data.component.param_defaults[i++] = strdup(def->valuestring);
                        }
                    }
                }
            }

            cJSON *parent = cJSON_GetObjectItem(json, "parentComponent");
            if (parent && cJSON_IsString(parent)) {
                node->data.component.parent_component = strdup(parent->valuestring);
            }

            // Handle stateVars, functions, and uiRoot from KRL output
            cJSON *state_vars = cJSON_GetObjectItem(json, "stateVars");
            if (state_vars) {
                node->data.component.state_vars = deserialize_array(reader, state_vars,
                                                                    &node->data.component.state_count);
            }

            cJSON *functions = cJSON_GetObjectItem(json, "functions");
            if (functions) {
                node->data.component.functions = deserialize_array(reader, functions,
                                                                   &node->data.component.function_count);
            }

            // Handle uiRoot from KRL output - convert to body_elements array
            cJSON *ui_root = cJSON_GetObjectItem(json, "uiRoot");
            if (ui_root) {
                KryonASTNode *root_node = deserialize_node(reader, ui_root);
                if (root_node) {
                    node->data.component.body_elements = calloc(1, sizeof(KryonASTNode*));
                    node->data.component.body_elements[0] = root_node;
                    node->data.component.body_count = 1;
                    node->data.component.body_capacity = 1;
                }
            }

            // Also handle legacy bodyElements for compatibility (overrides uiRoot if present)
            cJSON *body = cJSON_GetObjectItem(json, "bodyElements");
            if (body) {
                node->data.component.body_elements = deserialize_array(reader, body,
                                                                       &node->data.component.body_count);
                node->data.component.body_capacity = node->data.component.body_count;
            }
            break;
        }

        case KRYON_AST_ARRAY_LITERAL: {
            cJSON *elements = cJSON_GetObjectItem(json, "elements");
            if (elements) {
                node->data.array_literal.elements = deserialize_array(reader, elements,
                                                                      &node->data.array_literal.element_count);
                node->data.array_literal.element_capacity = node->data.array_literal.element_count;
            }
            break;
        }

        case KRYON_AST_OBJECT_LITERAL: {
            cJSON *properties = cJSON_GetObjectItem(json, "properties");
            if (properties) {
                node->data.object_literal.properties = deserialize_array(reader, properties,
                                                                         &node->data.object_literal.property_count);
                node->data.object_literal.property_capacity = node->data.object_literal.property_count;
            }
            break;
        }

        case KRYON_AST_FOR_DIRECTIVE: {
            cJSON *var_name = cJSON_GetObjectItem(json, "varName");
            if (var_name && cJSON_IsString(var_name)) {
                node->data.for_loop.var_name = strdup(var_name->valuestring);
            }

            cJSON *index_var = cJSON_GetObjectItem(json, "indexVarName");
            if (index_var && cJSON_IsString(index_var)) {
                node->data.for_loop.index_var_name = strdup(index_var->valuestring);
            }

            cJSON *array_name = cJSON_GetObjectItem(json, "arrayName");
            if (array_name && cJSON_IsString(array_name)) {
                node->data.for_loop.array_name = strdup(array_name->valuestring);
            }

            cJSON *body = cJSON_GetObjectItem(json, "body");
            if (body) {
                node->data.for_loop.body = deserialize_array(reader, body,
                                                             &node->data.for_loop.body_count);
                node->data.for_loop.body_capacity = node->data.for_loop.body_count;
            }
            break;
        }

        case KRYON_AST_IF_DIRECTIVE: {
            cJSON *condition = cJSON_GetObjectItem(json, "condition");
            if (condition) {
                node->data.conditional.condition = deserialize_node(reader, condition);
            }

            cJSON *then_branch = cJSON_GetObjectItem(json, "thenBranch");
            if (then_branch) {
                node->data.conditional.then_body = deserialize_array(reader, then_branch,
                                                                     &node->data.conditional.then_count);
                node->data.conditional.then_capacity = node->data.conditional.then_count;
            }

            cJSON *else_branch = cJSON_GetObjectItem(json, "elseBranch");
            if (else_branch) {
                node->data.conditional.else_body = deserialize_array(reader, else_branch,
                                                                     &node->data.conditional.else_count);
                node->data.conditional.else_capacity = node->data.conditional.else_count;
            }
            break;
        }

        // Add more cases as needed...
        default:
            // Unsupported node type - return empty node
            break;
    }

    return node;
}

// =============================================================================
// READER API
// =============================================================================

KryonKIRReader *kryon_kir_reader_create(const KryonKIRConfig *config) {
    KryonKIRReader *reader = calloc(1, sizeof(KryonKIRReader));
    if (!reader) return NULL;

    if (config) {
        reader->config = *config;
    } else {
        reader->config = kryon_kir_default_config();
    }

    return reader;
}

void kryon_kir_reader_destroy(KryonKIRReader *reader) {
    if (!reader) return;

    for (size_t i = 0; i < reader->error_count; i++) {
        free(reader->error_messages[i]);
    }
    free(reader->error_messages);
    free(reader);
}

bool kryon_kir_read_file(KryonKIRReader *reader, const char *input_path,
                         KryonASTNode **out_ast) {
    if (!reader || !input_path || !out_ast) return false;

    // Read file content
    FILE *file = fopen(input_path, "r");
    if (!file) {
        reader_error(reader, "Failed to open input file");
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_string = malloc(file_size + 1);
    if (!json_string) {
        reader_error(reader, "Failed to allocate memory for file content");
        fclose(file);
        return false;
    }

    size_t read_size = fread(json_string, 1, file_size, file);
    json_string[read_size] = '\0';
    fclose(file);

    bool result = kryon_kir_read_string(reader, json_string, out_ast);
    free(json_string);

    return result;
}

bool kryon_kir_read_string(KryonKIRReader *reader, const char *json_string,
                           KryonASTNode **out_ast) {
    if (!reader || !json_string || !out_ast) return false;

    reader->nodes_read = 0;

    // Parse JSON
    cJSON *kir = cJSON_Parse(json_string);
    if (!kir) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            char error_msg[512];
            snprint(error_msg, sizeof(error_msg), "JSON parse error: %s", error_ptr);
            reader_error(reader, error_msg);
        } else {
            reader_error(reader, "Failed to parse JSON");
        }
        return false;
    }

    // Validate top-level structure
    cJSON *version = cJSON_GetObjectItem(kir, "version");
    if (!version || !cJSON_IsString(version)) {
        reader_error(reader, "Missing or invalid 'version' field");
        cJSON_Delete(kir);
        return false;
    }

    // Version validation removed for alpha
    // Accept any version string during alpha development
    // Optional: log version for debugging
    #ifdef DEBUG
        fprintf(stderr, "KIR version: %s\n", version->valuestring);
    #endif

    // Parse root node
    cJSON *root = cJSON_GetObjectItem(kir, "root");
    if (!root) {
        reader_error(reader, "Missing 'root' field");
        cJSON_Delete(kir);
        return false;
    }

    KryonASTNode *ast_root = deserialize_node(reader, root);
    cJSON_Delete(kir);

    if (!ast_root) {
        reader_error(reader, "Failed to deserialize root node");
        return false;
    }

    *out_ast = ast_root;
    return true;
}

bool kryon_kir_read_stream(KryonKIRReader *reader, FILE *file,
                           KryonASTNode **out_ast) {
    if (!reader || !file || !out_ast) return false;

    // Read entire stream
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_string = malloc(file_size + 1);
    if (!json_string) {
        reader_error(reader, "Failed to allocate memory");
        return false;
    }

    size_t read_size = fread(json_string, 1, file_size, file);
    json_string[read_size] = '\0';

    bool result = kryon_kir_read_string(reader, json_string, out_ast);
    free(json_string);

    return result;
}

const char **kryon_kir_reader_get_errors(const KryonKIRReader *reader, size_t *out_count) {
    if (!reader || !out_count) return NULL;
    *out_count = reader->error_count;
    return (const char **)reader->error_messages;
}

bool kryon_kir_get_version(KryonKIRReader *reader, const char *input_path,
                           int *major, int *minor, int *patch) {
    if (!reader || !input_path) return false;

    FILE *file = fopen(input_path, "r");
    if (!file) return false;

    // Read enough to get version
    char buffer[1024];
    size_t read_size = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[read_size] = '\0';
    fclose(file);

    // Quick parse for version field
    cJSON *kir = cJSON_Parse(buffer);
    if (!kir) return false;

    cJSON *version = cJSON_GetObjectItem(kir, "version");
    bool success = false;

    if (version && cJSON_IsString(version)) {
        int maj, min, pat;
        if (sscanf(version->valuestring, "%d.%d.%d", &maj, &min, &pat) == 3) {
            if (major) *major = maj;
            if (minor) *minor = min;
            if (patch) *patch = pat;
            success = true;
        }
    }

    cJSON_Delete(kir);
    return success;
}
