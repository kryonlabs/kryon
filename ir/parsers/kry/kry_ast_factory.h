/**
 * KRY AST Factory - Node and value creation functions
 *
 * Provides factory functions for creating AST nodes and values
 * using the parser's chunk allocator.
 */

#ifndef KRY_AST_FACTORY_H
#define KRY_AST_FACTORY_H

#include "kry_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Node Creation
// ============================================================================

/**
 * Create a new AST node of the specified type
 *
 * @param parser Parser context for allocation
 * @param type Node type
 * @return New node, or NULL on allocation failure
 */
KryNode* kry_node_create(KryParser* parser, KryNodeType type);

/**
 * Append a child node to a parent
 *
 * @param parent Parent node
 * @param child Child node to append
 */
void kry_node_append_child(KryNode* parent, KryNode* child);

/**
 * Create a code block node
 *
 * @param parser Parser context
 * @param language Language identifier (e.g., "lua", "c", "js")
 * @param source Source code content
 * @return New code block node
 */
KryNode* kry_node_create_code_block(KryParser* parser, const char* language, const char* source);

// ============================================================================
// Value Creation
// ============================================================================

/**
 * Create a string value
 */
KryValue* kry_value_create_string(KryParser* parser, const char* str);

/**
 * Create a number value
 */
KryValue* kry_value_create_number(KryParser* parser, double num);

/**
 * Create an identifier value
 */
KryValue* kry_value_create_identifier(KryParser* parser, const char* id);

/**
 * Create an expression value
 */
KryValue* kry_value_create_expression(KryParser* parser, const char* expr);

/**
 * Create an array value
 */
KryValue* kry_value_create_array(KryParser* parser, KryValue** elements, size_t count);

/**
 * Create an object value
 */
KryValue* kry_value_create_object(KryParser* parser, char** keys, KryValue** values, size_t count);

/**
 * Create a struct instance value
 */
KryValue* kry_value_create_struct_instance(KryParser* parser, const char* struct_name,
                                            char** field_names, KryValue** field_values, size_t count);

/**
 * Create a range value
 */
KryValue* kry_value_create_range(KryParser* parser, KryValue* start, KryValue* end);

#ifdef __cplusplus
}
#endif

#endif // KRY_AST_FACTORY_H
