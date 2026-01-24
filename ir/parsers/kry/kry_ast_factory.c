/**
 * KRY AST Factory - Node and value creation implementation
 */

#include "kry_ast_factory.h"
#include "kry_allocator.h"

// ============================================================================
// Node Creation
// ============================================================================

KryNode* kry_node_create(KryParser* parser, KryNodeType type) {
    KryNode* node = (KryNode*)kry_alloc(parser, sizeof(KryNode));
    if (!node) return NULL;

    node->type = type;
    node->parent = NULL;
    node->first_child = NULL;
    node->last_child = NULL;
    node->prev_sibling = NULL;
    node->next_sibling = NULL;
    node->name = NULL;
    node->value = NULL;
    node->is_component_definition = false;
    node->arguments = NULL;
    node->var_type = NULL;
    node->state_type = NULL;
    node->else_branch = NULL;
    node->code_language = NULL;
    node->code_source = NULL;
    node->import_module = NULL;
    node->import_name = NULL;
    node->func_name = NULL;
    node->func_return_type = NULL;
    node->func_params = NULL;
    node->param_count = 0;
    node->func_body = NULL;
    node->return_expr = NULL;
    node->extends_parent = NULL;
    node->line = parser->line;
    node->column = parser->column;

    return node;
}

void kry_node_append_child(KryNode* parent, KryNode* child) {
    if (!parent || !child) return;

    child->parent = parent;
    child->next_sibling = NULL;

    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
        child->prev_sibling = NULL;
    } else {
        child->prev_sibling = parent->last_child;
        parent->last_child->next_sibling = child;
        parent->last_child = child;
    }
}

KryNode* kry_node_create_code_block(KryParser* parser, const char* language, const char* source) {
    KryNode* node = kry_node_create(parser, KRY_NODE_CODE_BLOCK);
    if (!node) return NULL;

    node->code_language = kry_strdup(parser, language);
    node->code_source = kry_strdup(parser, source);
    return node;
}

// ============================================================================
// Value Creation
// ============================================================================

KryValue* kry_value_create_string(KryParser* parser, const char* str) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_STRING;
    value->string_value = kry_strdup(parser, str);
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_number(KryParser* parser, double num) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_NUMBER;
    value->number_value = num;
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_identifier(KryParser* parser, const char* id) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_IDENTIFIER;
    value->identifier = kry_strdup(parser, id);
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_expression(KryParser* parser, const char* expr) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_EXPRESSION;
    value->expression = kry_strdup(parser, expr);
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_array(KryParser* parser, KryValue** elements, size_t count) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_ARRAY;
    value->array.elements = elements;
    value->array.count = count;
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_object(KryParser* parser, char** keys, KryValue** values, size_t count) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_OBJECT;
    value->object.keys = keys;
    value->object.values = values;
    value->object.count = count;
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_struct_instance(KryParser* parser, const char* struct_name,
                                            char** field_names, KryValue** field_values, size_t count) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_STRUCT_INSTANCE;
    value->struct_instance.struct_name = kry_strdup(parser, struct_name);
    value->struct_instance.field_names = field_names;
    value->struct_instance.field_values = field_values;
    value->struct_instance.field_count = count;
    value->is_percentage = false;
    return value;
}

KryValue* kry_value_create_range(KryParser* parser, KryValue* start, KryValue* end) {
    KryValue* value = (KryValue*)kry_alloc(parser, sizeof(KryValue));
    if (!value) return NULL;

    value->type = KRY_VALUE_RANGE;
    value->range.start = start;
    value->range.end = end;
    value->is_percentage = false;
    return value;
}
