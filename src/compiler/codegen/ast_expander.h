/**
 * @file ast_expander.h
 * @brief AST expansion interface for components
 */

#ifndef KRYON_AST_EXPANDER_H
#define KRYON_AST_EXPANDER_H

#include "codegen.h"
#include "parser.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Find component definition in AST
 * @param component_name Name of component to find
 * @param ast_root Root of AST to search in
 * @return Component definition node or NULL if not found
 */
KryonASTNode *kryon_find_component_definition(const char *component_name, 
                                              const KryonASTNode *ast_root);

/**
 * @brief Expand component instance by substituting parameters
 * @param component_instance Component instance to expand
 * @param ast_root Root of AST for component definition lookup
 * @return Expanded component body or NULL on failure
 */
KryonASTNode *kryon_expand_component_instance(const KryonASTNode *component_instance, 
                                              const KryonASTNode *ast_root);

/**
 * @brief Write @if directive as runtime conditional
 * @param codegen Code generator context
 * @param if_directive If directive node
 * @param ast_root Root of AST
 * @return true on success
 */
bool kryon_write_if_directive(KryonCodeGenerator *codegen,
                              const KryonASTNode *if_directive,
                              const KryonASTNode *ast_root);

/**
 * @brief Substitute template variables in AST nodes
 * @param node The AST node to process
 * @param var_name The variable name to substitute (e.g., "alignment")
 * @param var_value The variable value (object literal with properties)
 * @return New AST node with substituted values
 */
KryonASTNode *kryon_substitute_template_vars(const KryonASTNode *node, const char *var_name, const KryonASTNode *var_value, KryonCodeGenerator *codegen);

/**
 * @brief Serialize condition expression to string for @if directives
 * @param condition The condition AST node to serialize
 * @return Serialized condition string (caller must free) or NULL on error
 */
char *serialize_condition_expression(const KryonASTNode *condition);

/**
 * @brief Resolve component inheritance by merging parent and child components
 * @param component_def The child component definition (with parent_component set)
 * @param ast_root The AST root to search for parent definitions
 * @return A new merged component definition, or the original if no parent
 */
KryonASTNode *kryon_resolve_component_inheritance(const KryonASTNode *component_def, const KryonASTNode *ast_root);

#endif // KRYON_AST_EXPANDER_H