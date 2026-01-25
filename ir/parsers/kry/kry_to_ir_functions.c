/**
 * Kryon .kry Function Statement Conversion
 *
 * Handles conversion of function declarations and statements to IR.
 * Extracted from kry_to_ir.c for modularity.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_to_ir_functions.h"
#include "kry_to_ir_expressions.h"
#include "kry_struct_to_ir.h"
#include "../../include/ir_expression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Function Statement Conversion Helpers
// ============================================================================

// Convert a list of child statements to an array of IRStatement*
// Returns the count of converted statements
int kry_convert_child_statements(ConversionContext* ctx, KryNode* first_child,
                                    IRLogicFunction* func, IRStatement*** out_stmts) {
    if (!first_child || !out_stmts) {
        *out_stmts = NULL;
        return 0;
    }

    // First pass: count statements
    int count = 0;
    KryNode* child = first_child;
    while (child) {
        count++;
        child = child->next_sibling;
    }

    if (count == 0) {
        *out_stmts = NULL;
        return 0;
    }

    // Allocate array
    IRStatement** stmts = calloc(count, sizeof(IRStatement*));
    if (!stmts) {
        *out_stmts = NULL;
        return 0;
    }

    // Second pass: convert statements
    child = first_child;
    int actual_count = 0;
    for (int i = 0; i < count && child; i++) {
        IRStatement* ir_stmt = kry_convert_function_stmt(ctx, child, func);
        if (ir_stmt) {
            stmts[actual_count++] = ir_stmt;
        }
        child = child->next_sibling;
    }

    *out_stmts = stmts;
    return actual_count;
}

// Convert a single function body statement to IRStatement
// This is the recursive helper for handling nested control flow
IRStatement* kry_convert_function_stmt(ConversionContext* ctx, KryNode* stmt, IRLogicFunction* func) {
    if (!stmt) return NULL;

    IRStatement* ir_stmt = NULL;

    switch (stmt->type) {
        case KRY_NODE_RETURN_STMT: {
            // Handle return statement
            IRExpression* return_value = NULL;
            if (stmt->return_expr && stmt->return_expr->value) {
                return_value = kry_value_to_ir_expr(stmt->return_expr->value);
            }
            ir_stmt = ir_stmt_return(return_value);
            break;
        }

        case KRY_NODE_DELETE_STMT: {
            // Delete statement: delete <expression>
            IRExpression* target_expr = NULL;
            if (stmt->delete_target && stmt->delete_target->value) {
                target_expr = kry_value_to_ir_expr(stmt->delete_target->value);
            }
            if (target_expr) {
                ir_stmt = ir_stmt_delete(target_expr);
            }
            break;
        }

        case KRY_NODE_VAR_DECL:
        case KRY_NODE_PROPERTY: {
            // Variable declaration (const/let/var name = value) or
            // Property assignment (name = value) - both have same structure
            if (stmt->name && stmt->value) {
                IRExpression* value_expr = kry_value_to_ir_expr(stmt->value);
                if (value_expr) {
                    ir_stmt = ir_stmt_assign(stmt->name, value_expr);
                }
            }
            break;
        }

        case KRY_NODE_IF: {
            // If statement: if condition { then_body } else { else_body }
            IRExpression* condition = kry_value_to_ir_expr(stmt->value);

            // Convert then-branch (children of the if node)
            IRStatement** then_stmts = NULL;
            int then_count = kry_convert_child_statements(ctx, stmt->first_child, func, &then_stmts);

            // Convert else-branch (children of the else_branch node)
            IRStatement** else_stmts = NULL;
            int else_count = 0;
            if (stmt->else_branch) {
                else_count = kry_convert_child_statements(ctx, stmt->else_branch->first_child, func, &else_stmts);
            }

            ir_stmt = ir_stmt_if(condition, then_stmts, then_count, else_stmts, else_count);

            // Free the arrays (ir_stmt_if makes copies)
            free(then_stmts);
            free(else_stmts);
            break;
        }

        case KRY_NODE_FOR_LOOP:
        case KRY_NODE_FOR_EACH: {
            // For loop: for item in collection { body }
            // Both FOR_LOOP and FOR_EACH have same structure in function context
            const char* item_name = stmt->name;
            IRExpression* iterable = kry_value_to_ir_expr(stmt->value);

            // Convert body statements
            IRStatement** body_stmts = NULL;
            int body_count = kry_convert_child_statements(ctx, stmt->first_child, func, &body_stmts);

            ir_stmt = ir_stmt_for_each(item_name, iterable, body_stmts, body_count);

            // Free the array (ir_stmt_for_each makes copies)
            free(body_stmts);
            break;
        }

        case KRY_NODE_CODE_BLOCK: {
            // Platform-specific code block: @lua { ... } or @js { ... }
            // Add the embedded source to the function
            if (stmt->code_language && stmt->code_source) {
                ir_logic_function_add_source(func, stmt->code_language, stmt->code_source);
            }
            // CODE_BLOCK doesn't produce an IRStatement itself
            ir_stmt = NULL;
            break;
        }

        case KRY_NODE_COMPONENT: {
            // Nested components in function body - ignore for now
            // These could be embedded UI components in the future
            ir_stmt = NULL;
            break;
        }

        default:
            // Unknown statement type - skip
            ir_stmt = NULL;
            break;
    }

    return ir_stmt;
}

// Convert return statement to IRStatement
// Returns NULL if node is not a return statement
IRStatement* kry_convert_return_stmt(KryNode* node) {
    if (!node || node->type != KRY_NODE_RETURN_STMT) {
        return NULL;
    }

    // Create return statement with optional expression
    IRExpression* return_value = NULL;

    if (node->return_expr && node->return_expr->value) {
        // Convert the return expression to IRExpression
        // For now, we'll create a simple identifier or literal expression
        KryValue* value = node->return_expr->value;

        if (value->type == KRY_VALUE_IDENTIFIER) {
            return_value = ir_expr_var(value->identifier);
        } else if (value->type == KRY_VALUE_STRING) {
            return_value = ir_expr_string(value->string_value);
        } else if (value->type == KRY_VALUE_NUMBER) {
            return_value = ir_expr_float(value->number_value);
        } else if (value->type == KRY_VALUE_EXPRESSION) {
            // For expression blocks, create an identifier expression
            return_value = ir_expr_var(value->expression);
        }
    }

    return ir_stmt_return(return_value);
}

// Convert module-level return statement to IRModuleExport entries
// Returns true on success, false on failure
bool kry_convert_module_return(ConversionContext* ctx, KryNode* node) {
    if (!node || node->type != KRY_NODE_MODULE_RETURN) {
        return false;
    }


    if (!ctx->source_structures) {
        return false;
    }

    // For each export name, find the declaration and create IRModuleExport
    for (int i = 0; i < node->export_count; i++) {
        char* export_name = node->export_names[i];
        if (!export_name) continue;

        // Find the declaration in current scope
        // Search through children of the root/component for matching name
        KryNode* decl = NULL;
        KryNode* child = ctx->ast_root->first_child;

        while (child) {
            if ((child->type == KRY_NODE_VAR_DECL && strcmp(child->name, export_name) == 0) ||
                (child->type == KRY_NODE_FUNCTION_DECL && strcmp(child->func_name, export_name) == 0) ||
                (child->type == KRY_NODE_STRUCT_DECL && strcmp(child->struct_name, export_name) == 0)) {
                decl = child;
                break;
            }
            child = child->next_sibling;
        }

        if (!decl) {
            continue;
        }

        // Create IRModuleExport
        IRModuleExport* export = (IRModuleExport*)calloc(1, sizeof(IRModuleExport));
        if (!export) continue;

        export->name = strdup(export_name);
        export->line = node->line;

        if (decl->type == KRY_NODE_VAR_DECL) {
            // Export constant
            export->type = "constant";
            // Convert value to JSON string
            if (decl->value) {
                export->value_json = kry_value_to_json(decl->value);
            }
        } else if (decl->type == KRY_NODE_FUNCTION_DECL) {
            // Export function
            export->type = "function";
            export->function_name = strdup(decl->func_name);
        } else if (decl->type == KRY_NODE_STRUCT_DECL) {
            // Export struct type
            export->type = "struct";

            // Convert struct to IRStructType
            IRStructType* struct_type = kry_convert_struct_decl(ctx, decl);
            export->struct_def = struct_type;

        }

        // Add to source structures
        // Expand exports array if needed
        if (ctx->source_structures->export_count >= ctx->source_structures->export_capacity) {
            ctx->source_structures->export_capacity = (ctx->source_structures->export_capacity == 0) ? 16 : ctx->source_structures->export_capacity * 2;
            ctx->source_structures->exports = (IRModuleExport**)realloc(
                ctx->source_structures->exports,
                sizeof(IRModuleExport*) * ctx->source_structures->export_capacity
            );
        }

        ctx->source_structures->exports[ctx->source_structures->export_count++] = export;
    }

    return true;
}

// Convert function declaration to IRLogicFunction
// Returns NULL if node is not a function declaration
IRLogicFunction* kry_convert_function_decl(ConversionContext* ctx, KryNode* node, const char* scope) {
    if (!node || node->type != KRY_NODE_FUNCTION_DECL) {
        return NULL;
    }


    // Create function name with scope prefix
    char func_name[512];
    if (scope && scope[0] != '\0') {
        snprintf(func_name, sizeof(func_name), "%s:%s", scope, node->func_name);
    } else {
        snprintf(func_name, sizeof(func_name), "%s", node->func_name);
    }

    // Create IRLogicFunction
    IRLogicFunction* func = ir_logic_function_create(func_name);
    if (!func) {
        return NULL;
    }

    // Set return type
    if (node->func_return_type) {
        func->return_type = strdup(node->func_return_type);
    }

    // Add parameters
    for (int i = 0; i < node->param_count; i++) {
        KryNode* param = node->func_params[i];
        if (param && param->name) {
            // Use the type from var_type field if available, otherwise default to "any"
            const char* param_type = param->var_type ? param->var_type : "any";
            ir_logic_function_add_param(func, param->name, param_type);
        }
    }

    // Convert function body statements using the recursive helper
    if (node->func_body) {
        KryNode* stmt = node->func_body->first_child;
        while (stmt) {
            // Use the unified statement converter which handles:
            // - KRY_NODE_RETURN_STMT
            // - KRY_NODE_VAR_DECL
            // - KRY_NODE_IF (with nested then/else branches)
            // - KRY_NODE_FOR_LOOP (for i in range)
            // - KRY_NODE_FOR_EACH (for item in collection)
            // - KRY_NODE_CODE_BLOCK (@lua/@js blocks)
            // - KRY_NODE_COMPONENT (ignored)
            IRStatement* ir_stmt = kry_convert_function_stmt(ctx, stmt, func);
            if (ir_stmt) {
                ir_logic_function_add_stmt(func, ir_stmt);
            }

            stmt = stmt->next_sibling;
        }
    }

    return func;
}
