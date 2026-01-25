/**
 * Kryon .kry Function Statement Conversion
 *
 * Handles conversion of function declarations and statements to IR.
 * Extracted from kry_to_ir.c for modularity.
 */

#ifndef KRY_TO_IR_FUNCTIONS_H
#define KRY_TO_IR_FUNCTIONS_H

#include "kry_ast.h"
#include "kry_to_ir_internal.h"
#include "../../include/ir_logic.h"

// Convert a list of child statements to an array of IRStatement*
// Returns the count of converted statements
int kry_convert_child_statements(ConversionContext* ctx, KryNode* first_child,
                                 IRLogicFunction* func, IRStatement*** out_stmts);

// Convert a single function body statement to IRStatement
// This is the recursive helper for handling nested control flow
IRStatement* kry_convert_function_stmt(ConversionContext* ctx, KryNode* stmt, IRLogicFunction* func);

// Convert return statement to IRStatement
// Returns NULL if node is not a return statement
IRStatement* kry_convert_return_stmt(KryNode* node);

// Convert module-level return statement to IRModuleExport entries
// Returns true on success, false on failure
bool kry_convert_module_return(ConversionContext* ctx, KryNode* node);

// Convert function declaration to IRLogicFunction
// Returns NULL if node is not a function declaration
IRLogicFunction* kry_convert_function_decl(ConversionContext* ctx, KryNode* node, const char* scope);

#endif // KRY_TO_IR_FUNCTIONS_H
