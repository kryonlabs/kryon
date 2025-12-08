#define _POSIX_C_SOURCE 200809L
#include "ir_expression.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static char* safe_strdup(const char* str) {
    if (!str) return NULL;
    return strdup(str);
}

// ============================================================================
// EXPRESSION CONSTRUCTORS
// ============================================================================

IRExpression* ir_expr_int(int64_t value) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL_INT;
    expr->int_value = value;
    return expr;
}

IRExpression* ir_expr_float(double value) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL_FLOAT;
    expr->float_value = value;
    return expr;
}

IRExpression* ir_expr_string(const char* value) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL_STRING;
    expr->string_value = safe_strdup(value);
    return expr;
}

IRExpression* ir_expr_bool(bool value) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL_BOOL;
    expr->bool_value = value;
    return expr;
}

IRExpression* ir_expr_null(void) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL_NULL;
    return expr;
}

IRExpression* ir_expr_var(const char* name) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_VAR_REF;
    expr->var_ref.name = safe_strdup(name);
    expr->var_ref.scope = NULL;
    return expr;
}

IRExpression* ir_expr_var_scoped(const char* scope, const char* name) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_VAR_REF;
    expr->var_ref.name = safe_strdup(name);
    expr->var_ref.scope = safe_strdup(scope);
    return expr;
}

IRExpression* ir_expr_property(const char* object, const char* field) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_PROPERTY;
    expr->property.object = safe_strdup(object);
    expr->property.field = safe_strdup(field);
    return expr;
}

IRExpression* ir_expr_index(IRExpression* array, IRExpression* index) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_INDEX;
    expr->index_access.array = array;
    expr->index_access.index = index;
    return expr;
}

IRExpression* ir_expr_binary(IRBinaryOp op, IRExpression* left, IRExpression* right) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_BINARY;
    expr->binary.op = op;
    expr->binary.left = left;
    expr->binary.right = right;
    return expr;
}

IRExpression* ir_expr_unary(IRUnaryOp op, IRExpression* operand) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_UNARY;
    expr->unary.op = op;
    expr->unary.operand = operand;
    return expr;
}

IRExpression* ir_expr_ternary(IRExpression* condition, IRExpression* then_expr, IRExpression* else_expr) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_TERNARY;
    expr->ternary.condition = condition;
    expr->ternary.then_expr = then_expr;
    expr->ternary.else_expr = else_expr;
    return expr;
}

IRExpression* ir_expr_call(const char* function, IRExpression** args, int arg_count) {
    IRExpression* expr = calloc(1, sizeof(IRExpression));
    if (!expr) return NULL;
    expr->type = EXPR_CALL;
    expr->call.function = safe_strdup(function);
    expr->call.arg_count = arg_count;
    if (arg_count > 0 && args) {
        expr->call.args = calloc(arg_count, sizeof(IRExpression*));
        if (expr->call.args) {
            memcpy(expr->call.args, args, arg_count * sizeof(IRExpression*));
        }
    } else {
        expr->call.args = NULL;
    }
    return expr;
}

// Convenience binary operators
IRExpression* ir_expr_add(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_ADD, left, right);
}

IRExpression* ir_expr_sub(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_SUB, left, right);
}

IRExpression* ir_expr_mul(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_MUL, left, right);
}

IRExpression* ir_expr_div(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_DIV, left, right);
}

IRExpression* ir_expr_eq(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_EQ, left, right);
}

IRExpression* ir_expr_neq(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_NEQ, left, right);
}

IRExpression* ir_expr_lt(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_LT, left, right);
}

IRExpression* ir_expr_gt(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_GT, left, right);
}

IRExpression* ir_expr_and(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_AND, left, right);
}

IRExpression* ir_expr_or(IRExpression* left, IRExpression* right) {
    return ir_expr_binary(BINARY_OP_OR, left, right);
}

// ============================================================================
// STATEMENT CONSTRUCTORS
// ============================================================================

IRStatement* ir_stmt_assign(const char* target, IRExpression* expr) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_ASSIGN;
    stmt->assign.target = safe_strdup(target);
    stmt->assign.target_scope = NULL;
    stmt->assign.expr = expr;
    return stmt;
}

IRStatement* ir_stmt_assign_scoped(const char* scope, const char* target, IRExpression* expr) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_ASSIGN;
    stmt->assign.target = safe_strdup(target);
    stmt->assign.target_scope = safe_strdup(scope);
    stmt->assign.expr = expr;
    return stmt;
}

IRStatement* ir_stmt_assign_op(const char* target, IRAssignOp op, IRExpression* expr) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_ASSIGN_OP;
    stmt->assign_op.target = safe_strdup(target);
    stmt->assign_op.target_scope = NULL;
    stmt->assign_op.op = op;
    stmt->assign_op.expr = expr;
    return stmt;
}

IRStatement* ir_stmt_if(IRExpression* condition, IRStatement** then_body, int then_count,
                        IRStatement** else_body, int else_count) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_IF;
    stmt->if_stmt.condition = condition;
    stmt->if_stmt.then_count = then_count;
    stmt->if_stmt.else_count = else_count;

    if (then_count > 0 && then_body) {
        stmt->if_stmt.then_body = calloc(then_count, sizeof(IRStatement*));
        if (stmt->if_stmt.then_body) {
            memcpy(stmt->if_stmt.then_body, then_body, then_count * sizeof(IRStatement*));
        }
    }

    if (else_count > 0 && else_body) {
        stmt->if_stmt.else_body = calloc(else_count, sizeof(IRStatement*));
        if (stmt->if_stmt.else_body) {
            memcpy(stmt->if_stmt.else_body, else_body, else_count * sizeof(IRStatement*));
        }
    }

    return stmt;
}

IRStatement* ir_stmt_while(IRExpression* condition, IRStatement** body, int body_count) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_WHILE;
    stmt->while_stmt.condition = condition;
    stmt->while_stmt.body_count = body_count;

    if (body_count > 0 && body) {
        stmt->while_stmt.body = calloc(body_count, sizeof(IRStatement*));
        if (stmt->while_stmt.body) {
            memcpy(stmt->while_stmt.body, body, body_count * sizeof(IRStatement*));
        }
    }

    return stmt;
}

IRStatement* ir_stmt_for_each(const char* item, IRExpression* iterable,
                              IRStatement** body, int body_count) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_FOR_EACH;
    stmt->for_each.item_name = safe_strdup(item);
    stmt->for_each.iterable = iterable;
    stmt->for_each.body_count = body_count;

    if (body_count > 0 && body) {
        stmt->for_each.body = calloc(body_count, sizeof(IRStatement*));
        if (stmt->for_each.body) {
            memcpy(stmt->for_each.body, body, body_count * sizeof(IRStatement*));
        }
    }

    return stmt;
}

IRStatement* ir_stmt_call(const char* function, IRExpression** args, int arg_count) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_CALL;
    stmt->call.function = safe_strdup(function);
    stmt->call.arg_count = arg_count;

    if (arg_count > 0 && args) {
        stmt->call.args = calloc(arg_count, sizeof(IRExpression*));
        if (stmt->call.args) {
            memcpy(stmt->call.args, args, arg_count * sizeof(IRExpression*));
        }
    }

    return stmt;
}

IRStatement* ir_stmt_return(IRExpression* value) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_RETURN;
    stmt->return_stmt.value = value;
    return stmt;
}

IRStatement* ir_stmt_break(void) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_BREAK;
    return stmt;
}

IRStatement* ir_stmt_continue(void) {
    IRStatement* stmt = calloc(1, sizeof(IRStatement));
    if (!stmt) return NULL;
    stmt->type = STMT_CONTINUE;
    return stmt;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void ir_expr_free(IRExpression* expr) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_LITERAL_STRING:
            free(expr->string_value);
            break;
        case EXPR_VAR_REF:
            free(expr->var_ref.name);
            free(expr->var_ref.scope);
            break;
        case EXPR_PROPERTY:
            free(expr->property.object);
            free(expr->property.field);
            break;
        case EXPR_INDEX:
            ir_expr_free(expr->index_access.array);
            ir_expr_free(expr->index_access.index);
            break;
        case EXPR_BINARY:
            ir_expr_free(expr->binary.left);
            ir_expr_free(expr->binary.right);
            break;
        case EXPR_UNARY:
            ir_expr_free(expr->unary.operand);
            break;
        case EXPR_TERNARY:
            ir_expr_free(expr->ternary.condition);
            ir_expr_free(expr->ternary.then_expr);
            ir_expr_free(expr->ternary.else_expr);
            break;
        case EXPR_CALL:
            free(expr->call.function);
            for (int i = 0; i < expr->call.arg_count; i++) {
                ir_expr_free(expr->call.args[i]);
            }
            free(expr->call.args);
            break;
        default:
            break;
    }

    free(expr);
}

void ir_stmt_free(IRStatement* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case STMT_ASSIGN:
            free(stmt->assign.target);
            free(stmt->assign.target_scope);
            ir_expr_free(stmt->assign.expr);
            break;
        case STMT_ASSIGN_OP:
            free(stmt->assign_op.target);
            free(stmt->assign_op.target_scope);
            ir_expr_free(stmt->assign_op.expr);
            break;
        case STMT_IF:
            ir_expr_free(stmt->if_stmt.condition);
            for (int i = 0; i < stmt->if_stmt.then_count; i++) {
                ir_stmt_free(stmt->if_stmt.then_body[i]);
            }
            free(stmt->if_stmt.then_body);
            for (int i = 0; i < stmt->if_stmt.else_count; i++) {
                ir_stmt_free(stmt->if_stmt.else_body[i]);
            }
            free(stmt->if_stmt.else_body);
            break;
        case STMT_WHILE:
            ir_expr_free(stmt->while_stmt.condition);
            for (int i = 0; i < stmt->while_stmt.body_count; i++) {
                ir_stmt_free(stmt->while_stmt.body[i]);
            }
            free(stmt->while_stmt.body);
            break;
        case STMT_FOR_EACH:
            free(stmt->for_each.item_name);
            ir_expr_free(stmt->for_each.iterable);
            for (int i = 0; i < stmt->for_each.body_count; i++) {
                ir_stmt_free(stmt->for_each.body[i]);
            }
            free(stmt->for_each.body);
            break;
        case STMT_CALL:
            free(stmt->call.function);
            for (int i = 0; i < stmt->call.arg_count; i++) {
                ir_expr_free(stmt->call.args[i]);
            }
            free(stmt->call.args);
            break;
        case STMT_RETURN:
            ir_expr_free(stmt->return_stmt.value);
            break;
        default:
            break;
    }

    free(stmt);
}

void ir_stmt_array_free(IRStatement** stmts, int count) {
    if (!stmts) return;
    for (int i = 0; i < count; i++) {
        ir_stmt_free(stmts[i]);
    }
    free(stmts);
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

static const char* binary_op_to_string(IRBinaryOp op) {
    switch (op) {
        case BINARY_OP_ADD: return "add";
        case BINARY_OP_SUB: return "sub";
        case BINARY_OP_MUL: return "mul";
        case BINARY_OP_DIV: return "div";
        case BINARY_OP_MOD: return "mod";
        case BINARY_OP_EQ: return "eq";
        case BINARY_OP_NEQ: return "neq";
        case BINARY_OP_LT: return "lt";
        case BINARY_OP_LTE: return "lte";
        case BINARY_OP_GT: return "gt";
        case BINARY_OP_GTE: return "gte";
        case BINARY_OP_AND: return "and";
        case BINARY_OP_OR: return "or";
        case BINARY_OP_CONCAT: return "concat";
        default: return "unknown";
    }
}

static IRBinaryOp string_to_binary_op(const char* str) {
    if (!str) return BINARY_OP_ADD;
    if (strcmp(str, "add") == 0) return BINARY_OP_ADD;
    if (strcmp(str, "sub") == 0) return BINARY_OP_SUB;
    if (strcmp(str, "mul") == 0) return BINARY_OP_MUL;
    if (strcmp(str, "div") == 0) return BINARY_OP_DIV;
    if (strcmp(str, "mod") == 0) return BINARY_OP_MOD;
    if (strcmp(str, "eq") == 0) return BINARY_OP_EQ;
    if (strcmp(str, "neq") == 0) return BINARY_OP_NEQ;
    if (strcmp(str, "lt") == 0) return BINARY_OP_LT;
    if (strcmp(str, "lte") == 0) return BINARY_OP_LTE;
    if (strcmp(str, "gt") == 0) return BINARY_OP_GT;
    if (strcmp(str, "gte") == 0) return BINARY_OP_GTE;
    if (strcmp(str, "and") == 0) return BINARY_OP_AND;
    if (strcmp(str, "or") == 0) return BINARY_OP_OR;
    if (strcmp(str, "concat") == 0) return BINARY_OP_CONCAT;
    return BINARY_OP_ADD;
}

static const char* unary_op_to_string(IRUnaryOp op) {
    switch (op) {
        case UNARY_OP_NEG: return "neg";
        case UNARY_OP_NOT: return "not";
        default: return "unknown";
    }
}

static IRUnaryOp string_to_unary_op(const char* str) {
    if (!str) return UNARY_OP_NEG;
    if (strcmp(str, "neg") == 0) return UNARY_OP_NEG;
    if (strcmp(str, "not") == 0) return UNARY_OP_NOT;
    return UNARY_OP_NEG;
}

static const char* assign_op_to_string(IRAssignOp op) {
    switch (op) {
        case ASSIGN_OP_ADD: return "assign_add";
        case ASSIGN_OP_SUB: return "assign_sub";
        case ASSIGN_OP_MUL: return "assign_mul";
        case ASSIGN_OP_DIV: return "assign_div";
        default: return "assign_add";
    }
}

static IRAssignOp string_to_assign_op(const char* str) {
    if (!str) return ASSIGN_OP_ADD;
    if (strcmp(str, "assign_add") == 0) return ASSIGN_OP_ADD;
    if (strcmp(str, "assign_sub") == 0) return ASSIGN_OP_SUB;
    if (strcmp(str, "assign_mul") == 0) return ASSIGN_OP_MUL;
    if (strcmp(str, "assign_div") == 0) return ASSIGN_OP_DIV;
    return ASSIGN_OP_ADD;
}

cJSON* ir_expr_to_json(IRExpression* expr) {
    if (!expr) return cJSON_CreateNull();

    cJSON* json;

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            return cJSON_CreateNumber((double)expr->int_value);

        case EXPR_LITERAL_FLOAT:
            return cJSON_CreateNumber(expr->float_value);

        case EXPR_LITERAL_STRING:
            return cJSON_CreateString(expr->string_value ? expr->string_value : "");

        case EXPR_LITERAL_BOOL:
            return cJSON_CreateBool(expr->bool_value);

        case EXPR_LITERAL_NULL:
            return cJSON_CreateNull();

        case EXPR_VAR_REF:
            json = cJSON_CreateObject();
            if (expr->var_ref.scope) {
                char full_name[512];
                snprintf(full_name, sizeof(full_name), "%s:%s",
                         expr->var_ref.scope, expr->var_ref.name);
                cJSON_AddStringToObject(json, "var", full_name);
            } else {
                cJSON_AddStringToObject(json, "var", expr->var_ref.name ? expr->var_ref.name : "");
            }
            return json;

        case EXPR_PROPERTY:
            json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "prop", expr->property.object ? expr->property.object : "");
            cJSON_AddStringToObject(json, "field", expr->property.field ? expr->property.field : "");
            return json;

        case EXPR_INDEX:
            json = cJSON_CreateObject();
            cJSON_AddItemToObject(json, "index", ir_expr_to_json(expr->index_access.array));
            cJSON_AddItemToObject(json, "at", ir_expr_to_json(expr->index_access.index));
            return json;

        case EXPR_BINARY:
            json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "op", binary_op_to_string(expr->binary.op));
            cJSON_AddItemToObject(json, "left", ir_expr_to_json(expr->binary.left));
            cJSON_AddItemToObject(json, "right", ir_expr_to_json(expr->binary.right));
            return json;

        case EXPR_UNARY:
            json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "op", unary_op_to_string(expr->unary.op));
            cJSON_AddItemToObject(json, "operand", ir_expr_to_json(expr->unary.operand));
            return json;

        case EXPR_TERNARY:
            json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "op", "ternary");
            cJSON_AddItemToObject(json, "condition", ir_expr_to_json(expr->ternary.condition));
            cJSON_AddItemToObject(json, "then", ir_expr_to_json(expr->ternary.then_expr));
            cJSON_AddItemToObject(json, "else", ir_expr_to_json(expr->ternary.else_expr));
            return json;

        case EXPR_CALL:
            json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "op", "call");
            cJSON_AddStringToObject(json, "function", expr->call.function ? expr->call.function : "");
            cJSON* args = cJSON_CreateArray();
            for (int i = 0; i < expr->call.arg_count; i++) {
                cJSON_AddItemToArray(args, ir_expr_to_json(expr->call.args[i]));
            }
            cJSON_AddItemToObject(json, "args", args);
            return json;

        default:
            return cJSON_CreateNull();
    }
}

cJSON* ir_stmt_to_json(IRStatement* stmt) {
    if (!stmt) return cJSON_CreateNull();

    cJSON* json = cJSON_CreateObject();

    switch (stmt->type) {
        case STMT_ASSIGN:
            cJSON_AddStringToObject(json, "op", "assign");
            if (stmt->assign.target_scope) {
                char full_target[512];
                snprintf(full_target, sizeof(full_target), "%s:%s",
                         stmt->assign.target_scope, stmt->assign.target);
                cJSON_AddStringToObject(json, "target", full_target);
            } else {
                cJSON_AddStringToObject(json, "target", stmt->assign.target ? stmt->assign.target : "");
            }
            cJSON_AddItemToObject(json, "expr", ir_expr_to_json(stmt->assign.expr));
            break;

        case STMT_ASSIGN_OP:
            cJSON_AddStringToObject(json, "op", assign_op_to_string(stmt->assign_op.op));
            cJSON_AddStringToObject(json, "target", stmt->assign_op.target ? stmt->assign_op.target : "");
            cJSON_AddItemToObject(json, "expr", ir_expr_to_json(stmt->assign_op.expr));
            break;

        case STMT_IF: {
            cJSON_AddStringToObject(json, "op", "if");
            cJSON_AddItemToObject(json, "condition", ir_expr_to_json(stmt->if_stmt.condition));

            cJSON* then_arr = cJSON_CreateArray();
            for (int i = 0; i < stmt->if_stmt.then_count; i++) {
                cJSON_AddItemToArray(then_arr, ir_stmt_to_json(stmt->if_stmt.then_body[i]));
            }
            cJSON_AddItemToObject(json, "then", then_arr);

            if (stmt->if_stmt.else_count > 0) {
                cJSON* else_arr = cJSON_CreateArray();
                for (int i = 0; i < stmt->if_stmt.else_count; i++) {
                    cJSON_AddItemToArray(else_arr, ir_stmt_to_json(stmt->if_stmt.else_body[i]));
                }
                cJSON_AddItemToObject(json, "else", else_arr);
            }
            break;
        }

        case STMT_WHILE: {
            cJSON_AddStringToObject(json, "op", "while");
            cJSON_AddItemToObject(json, "condition", ir_expr_to_json(stmt->while_stmt.condition));

            cJSON* body_arr = cJSON_CreateArray();
            for (int i = 0; i < stmt->while_stmt.body_count; i++) {
                cJSON_AddItemToArray(body_arr, ir_stmt_to_json(stmt->while_stmt.body[i]));
            }
            cJSON_AddItemToObject(json, "body", body_arr);
            break;
        }

        case STMT_FOR_EACH: {
            cJSON_AddStringToObject(json, "op", "for_each");
            cJSON_AddStringToObject(json, "item", stmt->for_each.item_name ? stmt->for_each.item_name : "");
            cJSON_AddItemToObject(json, "in", ir_expr_to_json(stmt->for_each.iterable));

            cJSON* body_arr = cJSON_CreateArray();
            for (int i = 0; i < stmt->for_each.body_count; i++) {
                cJSON_AddItemToArray(body_arr, ir_stmt_to_json(stmt->for_each.body[i]));
            }
            cJSON_AddItemToObject(json, "body", body_arr);
            break;
        }

        case STMT_CALL: {
            cJSON_AddStringToObject(json, "op", "call");
            cJSON_AddStringToObject(json, "function", stmt->call.function ? stmt->call.function : "");

            cJSON* args_arr = cJSON_CreateArray();
            for (int i = 0; i < stmt->call.arg_count; i++) {
                cJSON_AddItemToArray(args_arr, ir_expr_to_json(stmt->call.args[i]));
            }
            cJSON_AddItemToObject(json, "args", args_arr);
            break;
        }

        case STMT_RETURN:
            cJSON_AddStringToObject(json, "op", "return");
            if (stmt->return_stmt.value) {
                cJSON_AddItemToObject(json, "value", ir_expr_to_json(stmt->return_stmt.value));
            }
            break;

        case STMT_BREAK:
            cJSON_AddStringToObject(json, "op", "break");
            break;

        case STMT_CONTINUE:
            cJSON_AddStringToObject(json, "op", "continue");
            break;
    }

    return json;
}

IRExpression* ir_expr_from_json(cJSON* json) {
    if (!json) return NULL;

    // Literal number
    if (cJSON_IsNumber(json)) {
        double val = json->valuedouble;
        // Check if it's an integer
        if (val == (int64_t)val) {
            return ir_expr_int((int64_t)val);
        }
        return ir_expr_float(val);
    }

    // Literal string
    if (cJSON_IsString(json)) {
        return ir_expr_string(json->valuestring);
    }

    // Literal bool
    if (cJSON_IsBool(json)) {
        return ir_expr_bool(cJSON_IsTrue(json));
    }

    // Literal null
    if (cJSON_IsNull(json)) {
        return ir_expr_null();
    }

    // Object types
    if (cJSON_IsObject(json)) {
        // Variable reference: {"var": "name"}
        cJSON* var = cJSON_GetObjectItem(json, "var");
        if (var && cJSON_IsString(var)) {
            const char* name = var->valuestring;
            // Check for scoped name like "Counter:value"
            char* colon = strchr(name, ':');
            if (colon) {
                size_t scope_len = colon - name;
                char scope[256];
                strncpy(scope, name, scope_len);
                scope[scope_len] = '\0';
                return ir_expr_var_scoped(scope, colon + 1);
            }
            return ir_expr_var(name);
        }

        // Property access: {"prop": "obj", "field": "name"}
        cJSON* prop = cJSON_GetObjectItem(json, "prop");
        cJSON* field = cJSON_GetObjectItem(json, "field");
        if (prop && field && cJSON_IsString(prop) && cJSON_IsString(field)) {
            return ir_expr_property(prop->valuestring, field->valuestring);
        }

        // Index access: {"index": expr, "at": n}
        cJSON* index_arr = cJSON_GetObjectItem(json, "index");
        cJSON* at = cJSON_GetObjectItem(json, "at");
        if (index_arr && at) {
            return ir_expr_index(ir_expr_from_json(index_arr), ir_expr_from_json(at));
        }

        // Operations with "op" field
        cJSON* op = cJSON_GetObjectItem(json, "op");
        if (op && cJSON_IsString(op)) {
            const char* op_str = op->valuestring;

            // Call expression
            if (strcmp(op_str, "call") == 0) {
                cJSON* func = cJSON_GetObjectItem(json, "function");
                cJSON* args = cJSON_GetObjectItem(json, "args");
                const char* func_name = func && cJSON_IsString(func) ? func->valuestring : "";
                int arg_count = 0;
                IRExpression** arg_exprs = NULL;

                if (args && cJSON_IsArray(args)) {
                    arg_count = cJSON_GetArraySize(args);
                    if (arg_count > 0) {
                        arg_exprs = calloc(arg_count, sizeof(IRExpression*));
                        for (int i = 0; i < arg_count; i++) {
                            arg_exprs[i] = ir_expr_from_json(cJSON_GetArrayItem(args, i));
                        }
                    }
                }

                IRExpression* result = ir_expr_call(func_name, arg_exprs, arg_count);
                free(arg_exprs);
                return result;
            }

            // Ternary
            if (strcmp(op_str, "ternary") == 0) {
                cJSON* cond = cJSON_GetObjectItem(json, "condition");
                cJSON* then_expr = cJSON_GetObjectItem(json, "then");
                cJSON* else_expr = cJSON_GetObjectItem(json, "else");
                return ir_expr_ternary(
                    ir_expr_from_json(cond),
                    ir_expr_from_json(then_expr),
                    ir_expr_from_json(else_expr)
                );
            }

            // Unary operators - check both "operand" and "expr" field names
            cJSON* operand = cJSON_GetObjectItem(json, "operand");
            if (!operand) {
                operand = cJSON_GetObjectItem(json, "expr");  // Also accept "expr" for backwards compat
            }
            if (operand && !cJSON_GetObjectItem(json, "left")) {  // Make sure it's not a binary op
                IRUnaryOp unary_op = string_to_unary_op(op_str);
                return ir_expr_unary(unary_op, ir_expr_from_json(operand));
            }

            // Binary operators
            cJSON* left = cJSON_GetObjectItem(json, "left");
            cJSON* right = cJSON_GetObjectItem(json, "right");
            if (left && right) {
                IRBinaryOp binary_op = string_to_binary_op(op_str);
                return ir_expr_binary(binary_op, ir_expr_from_json(left), ir_expr_from_json(right));
            }
        }
    }

    return ir_expr_null();
}

IRStatement* ir_stmt_from_json(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    cJSON* op = cJSON_GetObjectItem(json, "op");
    if (!op || !cJSON_IsString(op)) return NULL;

    const char* op_str = op->valuestring;

    // Assignment
    if (strcmp(op_str, "assign") == 0) {
        cJSON* target = cJSON_GetObjectItem(json, "target");
        cJSON* expr = cJSON_GetObjectItem(json, "expr");
        if (!target || !cJSON_IsString(target)) return NULL;

        const char* target_str = target->valuestring;
        // Check for scoped target
        char* colon = strchr(target_str, ':');
        if (colon) {
            size_t scope_len = colon - target_str;
            char scope[256];
            strncpy(scope, target_str, scope_len);
            scope[scope_len] = '\0';
            return ir_stmt_assign_scoped(scope, colon + 1, ir_expr_from_json(expr));
        }
        return ir_stmt_assign(target_str, ir_expr_from_json(expr));
    }

    // Compound assignment
    if (strncmp(op_str, "assign_", 7) == 0) {
        cJSON* target = cJSON_GetObjectItem(json, "target");
        cJSON* expr = cJSON_GetObjectItem(json, "expr");
        if (!target || !cJSON_IsString(target)) return NULL;
        return ir_stmt_assign_op(target->valuestring, string_to_assign_op(op_str), ir_expr_from_json(expr));
    }

    // If statement
    if (strcmp(op_str, "if") == 0) {
        cJSON* condition = cJSON_GetObjectItem(json, "condition");
        cJSON* then_body = cJSON_GetObjectItem(json, "then");
        cJSON* else_body = cJSON_GetObjectItem(json, "else");

        int then_count = 0;
        IRStatement** then_stmts = ir_stmts_from_json(then_body, &then_count);

        int else_count = 0;
        IRStatement** else_stmts = ir_stmts_from_json(else_body, &else_count);

        IRStatement* result = ir_stmt_if(
            ir_expr_from_json(condition),
            then_stmts, then_count,
            else_stmts, else_count
        );

        free(then_stmts);
        free(else_stmts);
        return result;
    }

    // While loop
    if (strcmp(op_str, "while") == 0) {
        cJSON* condition = cJSON_GetObjectItem(json, "condition");
        cJSON* body = cJSON_GetObjectItem(json, "body");

        int body_count = 0;
        IRStatement** body_stmts = ir_stmts_from_json(body, &body_count);

        IRStatement* result = ir_stmt_while(ir_expr_from_json(condition), body_stmts, body_count);
        free(body_stmts);
        return result;
    }

    // For-each loop
    if (strcmp(op_str, "for_each") == 0) {
        cJSON* item = cJSON_GetObjectItem(json, "item");
        cJSON* in_expr = cJSON_GetObjectItem(json, "in");
        cJSON* body = cJSON_GetObjectItem(json, "body");

        if (!item || !cJSON_IsString(item)) return NULL;

        int body_count = 0;
        IRStatement** body_stmts = ir_stmts_from_json(body, &body_count);

        IRStatement* result = ir_stmt_for_each(
            item->valuestring,
            ir_expr_from_json(in_expr),
            body_stmts, body_count
        );
        free(body_stmts);
        return result;
    }

    // Function call statement
    if (strcmp(op_str, "call") == 0) {
        cJSON* func = cJSON_GetObjectItem(json, "function");
        cJSON* args = cJSON_GetObjectItem(json, "args");

        if (!func || !cJSON_IsString(func)) return NULL;

        int arg_count = 0;
        IRExpression** arg_exprs = NULL;

        if (args && cJSON_IsArray(args)) {
            arg_count = cJSON_GetArraySize(args);
            if (arg_count > 0) {
                arg_exprs = calloc(arg_count, sizeof(IRExpression*));
                for (int i = 0; i < arg_count; i++) {
                    arg_exprs[i] = ir_expr_from_json(cJSON_GetArrayItem(args, i));
                }
            }
        }

        IRStatement* result = ir_stmt_call(func->valuestring, arg_exprs, arg_count);
        free(arg_exprs);
        return result;
    }

    // Return
    if (strcmp(op_str, "return") == 0) {
        cJSON* value = cJSON_GetObjectItem(json, "value");
        return ir_stmt_return(value ? ir_expr_from_json(value) : NULL);
    }

    // Break
    if (strcmp(op_str, "break") == 0) {
        return ir_stmt_break();
    }

    // Continue
    if (strcmp(op_str, "continue") == 0) {
        return ir_stmt_continue();
    }

    return NULL;
}

IRStatement** ir_stmts_from_json(cJSON* json_array, int* out_count) {
    *out_count = 0;
    if (!json_array || !cJSON_IsArray(json_array)) return NULL;

    int count = cJSON_GetArraySize(json_array);
    if (count == 0) return NULL;

    IRStatement** stmts = calloc(count, sizeof(IRStatement*));
    if (!stmts) return NULL;

    for (int i = 0; i < count; i++) {
        stmts[i] = ir_stmt_from_json(cJSON_GetArrayItem(json_array, i));
    }

    *out_count = count;
    return stmts;
}

// ============================================================================
// DEBUG / PRINT
// ============================================================================

void ir_expr_print(IRExpression* expr) {
    if (!expr) {
        printf("null");
        return;
    }

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            printf("%lld", (long long)expr->int_value);
            break;
        case EXPR_LITERAL_FLOAT:
            printf("%g", expr->float_value);
            break;
        case EXPR_LITERAL_STRING:
            printf("\"%s\"", expr->string_value ? expr->string_value : "");
            break;
        case EXPR_LITERAL_BOOL:
            printf("%s", expr->bool_value ? "true" : "false");
            break;
        case EXPR_LITERAL_NULL:
            printf("null");
            break;
        case EXPR_VAR_REF:
            if (expr->var_ref.scope) {
                printf("%s:%s", expr->var_ref.scope, expr->var_ref.name);
            } else {
                printf("%s", expr->var_ref.name ? expr->var_ref.name : "?");
            }
            break;
        case EXPR_PROPERTY:
            printf("%s.%s", expr->property.object ? expr->property.object : "?",
                   expr->property.field ? expr->property.field : "?");
            break;
        case EXPR_INDEX:
            ir_expr_print(expr->index_access.array);
            printf("[");
            ir_expr_print(expr->index_access.index);
            printf("]");
            break;
        case EXPR_BINARY:
            printf("(");
            ir_expr_print(expr->binary.left);
            printf(" %s ", binary_op_to_string(expr->binary.op));
            ir_expr_print(expr->binary.right);
            printf(")");
            break;
        case EXPR_UNARY:
            printf("%s(", unary_op_to_string(expr->unary.op));
            ir_expr_print(expr->unary.operand);
            printf(")");
            break;
        case EXPR_TERNARY:
            printf("(");
            ir_expr_print(expr->ternary.condition);
            printf(" ? ");
            ir_expr_print(expr->ternary.then_expr);
            printf(" : ");
            ir_expr_print(expr->ternary.else_expr);
            printf(")");
            break;
        case EXPR_CALL:
            printf("%s(", expr->call.function ? expr->call.function : "?");
            for (int i = 0; i < expr->call.arg_count; i++) {
                if (i > 0) printf(", ");
                ir_expr_print(expr->call.args[i]);
            }
            printf(")");
            break;
    }
}

void ir_stmt_print(IRStatement* stmt, int indent) {
    if (!stmt) return;

    for (int i = 0; i < indent; i++) printf("  ");

    switch (stmt->type) {
        case STMT_ASSIGN:
            if (stmt->assign.target_scope) {
                printf("%s:%s = ", stmt->assign.target_scope, stmt->assign.target);
            } else {
                printf("%s = ", stmt->assign.target ? stmt->assign.target : "?");
            }
            ir_expr_print(stmt->assign.expr);
            printf("\n");
            break;

        case STMT_ASSIGN_OP:
            printf("%s %s ", stmt->assign_op.target ? stmt->assign_op.target : "?",
                   assign_op_to_string(stmt->assign_op.op));
            ir_expr_print(stmt->assign_op.expr);
            printf("\n");
            break;

        case STMT_IF:
            printf("if ");
            ir_expr_print(stmt->if_stmt.condition);
            printf(" then\n");
            for (int i = 0; i < stmt->if_stmt.then_count; i++) {
                ir_stmt_print(stmt->if_stmt.then_body[i], indent + 1);
            }
            if (stmt->if_stmt.else_count > 0) {
                for (int i = 0; i < indent; i++) printf("  ");
                printf("else\n");
                for (int i = 0; i < stmt->if_stmt.else_count; i++) {
                    ir_stmt_print(stmt->if_stmt.else_body[i], indent + 1);
                }
            }
            for (int i = 0; i < indent; i++) printf("  ");
            printf("end\n");
            break;

        case STMT_WHILE:
            printf("while ");
            ir_expr_print(stmt->while_stmt.condition);
            printf(" do\n");
            for (int i = 0; i < stmt->while_stmt.body_count; i++) {
                ir_stmt_print(stmt->while_stmt.body[i], indent + 1);
            }
            for (int i = 0; i < indent; i++) printf("  ");
            printf("end\n");
            break;

        case STMT_FOR_EACH:
            printf("for %s in ", stmt->for_each.item_name ? stmt->for_each.item_name : "?");
            ir_expr_print(stmt->for_each.iterable);
            printf(" do\n");
            for (int i = 0; i < stmt->for_each.body_count; i++) {
                ir_stmt_print(stmt->for_each.body[i], indent + 1);
            }
            for (int i = 0; i < indent; i++) printf("  ");
            printf("end\n");
            break;

        case STMT_CALL:
            printf("%s(", stmt->call.function ? stmt->call.function : "?");
            for (int i = 0; i < stmt->call.arg_count; i++) {
                if (i > 0) printf(", ");
                ir_expr_print(stmt->call.args[i]);
            }
            printf(")\n");
            break;

        case STMT_RETURN:
            printf("return ");
            if (stmt->return_stmt.value) {
                ir_expr_print(stmt->return_stmt.value);
            }
            printf("\n");
            break;

        case STMT_BREAK:
            printf("break\n");
            break;

        case STMT_CONTINUE:
            printf("continue\n");
            break;
    }
}
