/**
 * @file element_serializer.h
 * @brief Element serialization interface for KRB format
 */

#ifndef KRYON_ELEMENT_SERIALIZER_H
#define KRYON_ELEMENT_SERIALIZER_H

#include "codegen.h"
#include "parser.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Write element instance to KRB format
 * @param codegen Code generator context
 * @param element Element node to write
 * @param ast_root Root of AST for component lookups
 * @return true on success
 */
bool kryon_write_element_instance(KryonCodeGenerator *codegen, 
                                  const KryonASTNode *element, 
                                  const KryonASTNode *ast_root);

/**
 * @brief Write element node to KRB format
 * @param codegen Code generator context
 * @param element Element node to write
 * @param ast_root Root of AST for component lookups
 * @return true on success
 */
bool kryon_write_element_node(KryonCodeGenerator *codegen, 
                              const KryonASTNode *element, 
                              const KryonASTNode *ast_root);

/**
 * @brief Write property node to KRB format
 * @param codegen Code generator context
 * @param property Property node to write
 * @return true on success
 */
bool kryon_write_property_node(KryonCodeGenerator *codegen, 
                               const KryonASTNode *property);

/**
 * @brief Count expanded children (handling const_for loops)
 * @param codegen Code generator context
 * @param element Element to count children for
 * @return Number of expanded children
 */
uint16_t kryon_count_expanded_children(KryonCodeGenerator *codegen, 
                                       const KryonASTNode *element);

/**
 * @brief Count elements recursively in AST
 * @param node Root node to count from
 * @return Total number of elements
 */
uint32_t kryon_count_elements_recursive(const KryonASTNode *node);

/**
 * @brief Count properties recursively in AST
 * @param node Root node to count from
 * @return Total number of properties
 */
uint32_t kryon_count_properties_recursive(const KryonASTNode *node);

#endif // KRYON_ELEMENT_SERIALIZER_H