/**
 * @file directive_serializer.h
 * @brief Directive serialization interface for KRB format
 */

#ifndef KRYON_DIRECTIVE_SERIALIZER_H
#define KRYON_DIRECTIVE_SERIALIZER_H

#include "internal/codegen.h"
#include "internal/parser.h"
#include <stdbool.h>

/**
 * @brief Write variable definition node to KRB format
 * @param codegen Code generator context
 * @param variable Variable node to write
 * @return true on success
 */
bool kryon_write_variable_node(KryonCodeGenerator *codegen, 
                               const KryonASTNode *variable);

/**
 * @brief Write single variable to KRB format
 * @param codegen Code generator context
 * @param variable Variable node to write
 * @return true on success
 */
bool kryon_write_single_variable(KryonCodeGenerator *codegen, 
                                 const KryonASTNode *variable);

/**
 * @brief Write function definition node to KRB format
 * @param codegen Code generator context
 * @param function Function node to write
 * @return true on success
 */
bool kryon_write_function_node(KryonCodeGenerator *codegen, 
                               const KryonASTNode *function);

/**
 * @brief Write component definition node to KRB format
 * @param codegen Code generator context
 * @param component Component node to write
 * @return true on success
 */
bool kryon_write_component_node(KryonCodeGenerator *codegen, 
                                const KryonASTNode *component);

/**
 * @brief Write style definition node to KRB format
 * @param codegen Code generator context
 * @param style Style node to write
 * @return true on success
 */
bool kryon_write_style_node(KryonCodeGenerator *codegen, 
                            const KryonASTNode *style);

/**
 * @brief Write theme definition node to KRB format
 * @param codegen Code generator context
 * @param theme Theme node to write
 * @return true on success
 */
bool kryon_write_theme_node(KryonCodeGenerator *codegen, 
                            const KryonASTNode *theme);

/**
 * @brief Write metadata node to KRB format
 * @param codegen Code generator context
 * @param metadata Metadata node to write
 * @return true on success
 */
bool kryon_write_metadata_node(KryonCodeGenerator *codegen, 
                               const KryonASTNode *metadata);

/**
 * @brief Write constant definition (compile-time processing)
 * @param codegen Code generator context
 * @param const_def Constant definition node
 * @return true on success
 */
bool kryon_write_const_definition(KryonCodeGenerator *codegen, 
                                  const KryonASTNode *const_def);

#endif // KRYON_DIRECTIVE_SERIALIZER_H