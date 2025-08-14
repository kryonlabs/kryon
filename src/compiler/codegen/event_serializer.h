/**
 * @file event_serializer.h
 * @brief Event handler serialization for KRB format
 */

#ifndef KRYON_EVENT_SERIALIZER_H
#define KRYON_EVENT_SERIALIZER_H

#include "codegen.h"

/**
 * @brief Write event handler to KRB format
 * @param codegen Code generator instance
 * @param event Event directive node
 * @return true on success, false on failure
 */
bool kryon_write_event_handler(KryonCodeGenerator *codegen, const KryonASTNode *event);

/**
 * @brief Write all event handlers in the AST to KRB format
 * @param codegen Code generator instance
 * @param ast_root Root AST node
 * @return true on success, false on failure
 */
bool kryon_write_event_section(KryonCodeGenerator *codegen, const KryonASTNode *ast_root);

/**
 * @brief Count event handlers in the AST
 * @param ast_root Root AST node
 * @return Number of event handlers
 */
uint32_t kryon_count_event_handlers(const KryonASTNode *ast_root);

#endif // KRYON_EVENT_SERIALIZER_H