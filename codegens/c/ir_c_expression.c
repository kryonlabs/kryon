/**
 * C Code Generator - Expression and Statement Transpilation
 *
 * Converts KIR expressions and statements to C code.
 */

#include "ir_c_expression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Check if expression is a __range__ call and extract start/end
 * Returns true if it's a range, false otherwise
 */
bool c_is_range_call(cJSON* expr, cJSON** out_start, cJSON** out_end) {
    if (!expr) return false;

    cJSON* op = cJSON_GetObjectItem(expr, "op");
    if (!op || !cJSON_IsString(op) || strcmp(op->valuestring, "call") != 0) {
        return false;
    }

    cJSON* func = cJSON_GetObjectItem(expr, "function");
    if (!func || !cJSON_IsString(func) || strcmp(func->valuestring, "__range__") != 0) {
        return false;
    }

    cJSON* args = cJSON_GetObjectItem(expr, "args");
    if (!args || !cJSON_IsArray(args) || cJSON_GetArraySize(args) < 2) {
        return false;
    }

    *out_start = cJSON_GetArrayItem(args, 0);
    *out_end = cJSON_GetArrayItem(args, 1);
    return true;
}

/**
 * Convert KIR expression to C code string
 * Returns malloc'd string, caller must free
 */
char* c_expr_to_c(cJSON* expr) {
    if (!expr) return strdup("NULL");

    // String literal
    if (cJSON_IsString(expr)) {
        // Escape quotes in string
        const char* str = expr->valuestring;
        size_t len = strlen(str);
        char* result = malloc(len * 2 + 3);  // Worst case: all chars escaped + quotes + null
        char* p = result;
        *p++ = '"';
        for (size_t i = 0; i < len; i++) {
            if (str[i] == '"' || str[i] == '\\') {
                *p++ = '\\';
            }
            *p++ = str[i];
        }
        *p++ = '"';
        *p = '\0';
        return result;
    }

    // Number literal
    if (cJSON_IsNumber(expr)) {
        char* result = malloc(32);
        if (expr->valuedouble == (int)expr->valuedouble) {
            sprintf(result, "%d", expr->valueint);
        } else {
            sprintf(result, "%g", expr->valuedouble);
        }
        return result;
    }

    // Boolean
    if (cJSON_IsBool(expr)) {
        return strdup(cJSON_IsTrue(expr) ? "true" : "false");
    }

    // Null
    if (cJSON_IsNull(expr)) {
        return strdup("NULL");
    }

    // Variable reference: {"var": "name"}
    cJSON* var = cJSON_GetObjectItem(expr, "var");
    if (var && cJSON_IsString(var)) {
        return strdup(var->valuestring);
    }

    // Non-op based index access: {"index": arr, "at": idx}
    // This pattern is used in KIR for array indexing without explicit "op" field
    cJSON* index_arr = cJSON_GetObjectItem(expr, "index");
    cJSON* at_expr = cJSON_GetObjectItem(expr, "at");
    if (index_arr && at_expr) {
        char* arr_c = c_expr_to_c(index_arr);
        char* at_c = c_expr_to_c(at_expr);
        char* result = malloc(strlen(arr_c) + strlen(at_c) + 4);
        sprintf(result, "%s[%s]", arr_c, at_c);
        free(arr_c); free(at_c);
        return result;
    }

    // Operation
    cJSON* op = cJSON_GetObjectItem(expr, "op");
    if (!op || !cJSON_IsString(op)) {
        return strdup("/* unknown expr */");
    }

    const char* op_str = op->valuestring;

    // Binary arithmetic operations
    if (strcmp(op_str, "add") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s + %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "sub") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s - %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "mul") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s * %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "div") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s / %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "mod") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s %% %s)", left, right);
        free(left); free(right);
        return result;
    }

    // Comparison operations
    if (strcmp(op_str, "eq") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s == %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "ne") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s != %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "lt") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s < %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "gt") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s > %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "le") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s <= %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "ge") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s >= %s)", left, right);
        free(left); free(right);
        return result;
    }

    // Logical operations
    if (strcmp(op_str, "and") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s && %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "or") == 0) {
        char* left = c_expr_to_c(cJSON_GetObjectItem(expr, "left"));
        char* right = c_expr_to_c(cJSON_GetObjectItem(expr, "right"));
        char* result = malloc(strlen(left) + strlen(right) + 10);
        sprintf(result, "(%s || %s)", left, right);
        free(left); free(right);
        return result;
    }
    if (strcmp(op_str, "not") == 0) {
        char* operand = c_expr_to_c(cJSON_GetObjectItem(expr, "operand"));
        char* result = malloc(strlen(operand) + 5);
        sprintf(result, "(!%s)", operand);
        free(operand);
        return result;
    }

    // Unary minus
    if (strcmp(op_str, "neg") == 0) {
        char* operand = c_expr_to_c(cJSON_GetObjectItem(expr, "operand"));
        char* result = malloc(strlen(operand) + 5);
        sprintf(result, "(-%s)", operand);
        free(operand);
        return result;
    }

    // Member access: obj.property
    if (strcmp(op_str, "member_access") == 0) {
        char* obj = c_expr_to_c(cJSON_GetObjectItem(expr, "object"));
        cJSON* prop = cJSON_GetObjectItem(expr, "property");
        const char* prop_str = prop && cJSON_IsString(prop) ? prop->valuestring : "unknown";
        char* result = malloc(strlen(obj) + strlen(prop_str) + 2);
        sprintf(result, "%s.%s", obj, prop_str);
        free(obj);
        return result;
    }

    // Index access: arr[index]
    if (strcmp(op_str, "index_access") == 0) {
        char* obj = c_expr_to_c(cJSON_GetObjectItem(expr, "object"));
        char* index = c_expr_to_c(cJSON_GetObjectItem(expr, "index"));
        char* result = malloc(strlen(obj) + strlen(index) + 4);
        sprintf(result, "%s[%s]", obj, index);
        free(obj); free(index);
        return result;
    }

    // Array literal: [a, b, c]
    if (strcmp(op_str, "array_literal") == 0) {
        cJSON* elements = cJSON_GetObjectItem(expr, "elements");
        if (!elements || !cJSON_IsArray(elements)) {
            return strdup("{}");
        }

        // Calculate needed size
        size_t total_len = 3;  // { } \0
        int count = cJSON_GetArraySize(elements);
        char** elem_strs = malloc(count * sizeof(char*));

        for (int i = 0; i < count; i++) {
            elem_strs[i] = c_expr_to_c(cJSON_GetArrayItem(elements, i));
            total_len += strlen(elem_strs[i]) + 2;  // ", "
        }

        char* result = malloc(total_len);
        char* p = result;
        *p++ = '{';
        for (int i = 0; i < count; i++) {
            if (i > 0) {
                *p++ = ',';
                *p++ = ' ';
            }
            strcpy(p, elem_strs[i]);
            p += strlen(elem_strs[i]);
            free(elem_strs[i]);
        }
        *p++ = '}';
        *p = '\0';
        free(elem_strs);
        return result;
    }

    // Function call: func(args)
    if (strcmp(op_str, "call") == 0) {
        cJSON* func = cJSON_GetObjectItem(expr, "function");
        cJSON* args = cJSON_GetObjectItem(expr, "args");

        char* func_str = func ? c_expr_to_c(func) : strdup("unknown_func");

        // Build args string
        size_t total_len = strlen(func_str) + 3;  // () \0
        int arg_count = args ? cJSON_GetArraySize(args) : 0;
        char** arg_strs = NULL;

        if (arg_count > 0) {
            arg_strs = malloc(arg_count * sizeof(char*));
            for (int i = 0; i < arg_count; i++) {
                arg_strs[i] = c_expr_to_c(cJSON_GetArrayItem(args, i));
                total_len += strlen(arg_strs[i]) + 2;
            }
        }

        char* result = malloc(total_len);
        char* p = result;
        strcpy(p, func_str);
        p += strlen(func_str);
        *p++ = '(';
        for (int i = 0; i < arg_count; i++) {
            if (i > 0) {
                *p++ = ',';
                *p++ = ' ';
            }
            strcpy(p, arg_strs[i]);
            p += strlen(arg_strs[i]);
            free(arg_strs[i]);
        }
        *p++ = ')';
        *p = '\0';

        free(func_str);
        free(arg_strs);
        return result;
    }

    // Method call: receiver.method(args)
    if (strcmp(op_str, "method_call") == 0) {
        cJSON* receiver = cJSON_GetObjectItem(expr, "receiver");
        cJSON* method = cJSON_GetObjectItem(expr, "method");
        cJSON* args = cJSON_GetObjectItem(expr, "args");

        char* recv_str = receiver ? c_expr_to_c(receiver) : strdup("this");
        const char* method_str = method && cJSON_IsString(method) ? method->valuestring : "method";

        // Build args string
        size_t total_len = strlen(recv_str) + strlen(method_str) + 4;  // .() \0
        int arg_count = args ? cJSON_GetArraySize(args) : 0;
        char** arg_strs = NULL;

        if (arg_count > 0) {
            arg_strs = malloc(arg_count * sizeof(char*));
            for (int i = 0; i < arg_count; i++) {
                arg_strs[i] = c_expr_to_c(cJSON_GetArrayItem(args, i));
                total_len += strlen(arg_strs[i]) + 2;
            }
        }

        char* result = malloc(total_len);
        sprintf(result, "%s.%s(", recv_str, method_str);
        char* p = result + strlen(result);
        for (int i = 0; i < arg_count; i++) {
            if (i > 0) {
                *p++ = ',';
                *p++ = ' ';
            }
            strcpy(p, arg_strs[i]);
            p += strlen(arg_strs[i]);
            free(arg_strs[i]);
        }
        *p++ = ')';
        *p = '\0';

        free(recv_str);
        free(arg_strs);
        return result;
    }

    // Ternary/conditional: cond ? then : else
    if (strcmp(op_str, "ternary") == 0 || strcmp(op_str, "conditional") == 0) {
        char* cond = c_expr_to_c(cJSON_GetObjectItem(expr, "condition"));
        char* then_expr = c_expr_to_c(cJSON_GetObjectItem(expr, "then"));
        char* else_expr = c_expr_to_c(cJSON_GetObjectItem(expr, "else"));
        char* result = malloc(strlen(cond) + strlen(then_expr) + strlen(else_expr) + 10);
        sprintf(result, "(%s ? %s : %s)", cond, then_expr, else_expr);
        free(cond); free(then_expr); free(else_expr);
        return result;
    }

    // Unsupported operation - return a comment to indicate what's missing
    char* result = malloc(strlen(op_str) + 30);
    sprintf(result, "/* UNSUPPORTED: %s */", op_str);
    return result;
}

/**
 * Convert KIR statement to C code, write to output
 * Returns false on unsupported construct (hard error)
 */
bool c_stmt_to_c(FILE* output, cJSON* stmt, int indent, const char* output_path) {
    if (!stmt || !cJSON_IsObject(stmt)) return true;

    cJSON* op = cJSON_GetObjectItem(stmt, "op");
    if (!op || !cJSON_IsString(op)) return true;

    const char* op_str = op->valuestring;

    // Variable declaration: let/const/var name = expr;
    if (strcmp(op_str, "var_decl") == 0 || strcmp(op_str, "let") == 0 || strcmp(op_str, "const") == 0) {
        cJSON* name = cJSON_GetObjectItem(stmt, "name");
        cJSON* type = cJSON_GetObjectItem(stmt, "type");
        cJSON* init = cJSON_GetObjectItem(stmt, "init");

        const char* var_name = name && cJSON_IsString(name) ? name->valuestring : "var";
        const char* var_type = type && cJSON_IsString(type) ? type->valuestring : "auto";

        // Map KIR types to C types
        const char* c_type = "void*";
        if (strcmp(var_type, "string") == 0) {
            c_type = "const char*";
        } else if (strcmp(var_type, "int") == 0 || strcmp(var_type, "number") == 0) {
            c_type = "int";
        } else if (strcmp(var_type, "float") == 0 || strcmp(var_type, "double") == 0) {
            c_type = "double";
        } else if (strcmp(var_type, "bool") == 0) {
            c_type = "bool";
        } else if (strcmp(var_type, "auto") == 0) {
            // Try to infer from init expression
            c_type = "auto";  // C23 supports this, or we use void*
        }

        fprintf(output, "%*s%s %s", indent, "", c_type, var_name);
        if (init) {
            char* init_c = c_expr_to_c(init);
            fprintf(output, " = %s", init_c);
            free(init_c);
        }
        fprintf(output, ";\n");
        return true;
    }

    // Assignment: target = expr;
    if (strcmp(op_str, "assign") == 0) {
        cJSON* target = cJSON_GetObjectItem(stmt, "target");
        cJSON* expr = cJSON_GetObjectItem(stmt, "expr");

        // Target is either a string (variable name) or an expression (for member/index access)
        const char* target_str;
        char* target_c = NULL;
        if (cJSON_IsString(target)) {
            target_str = target->valuestring;
        } else {
            target_c = c_expr_to_c(target);
            target_str = target_c;
        }

        char* expr_c = expr ? c_expr_to_c(expr) : strdup("NULL");
        fprintf(output, "%*s%s = %s;\n", indent, "", target_str, expr_c);
        if (target_c) free(target_c);
        free(expr_c);
        return true;
    }

    // Return: return expr;
    if (strcmp(op_str, "return") == 0) {
        cJSON* value = cJSON_GetObjectItem(stmt, "value");
        if (value) {
            char* expr_c = c_expr_to_c(value);
            fprintf(output, "%*sreturn %s;\n", indent, "", expr_c);
            free(expr_c);
        } else {
            fprintf(output, "%*sreturn;\n", indent, "");
        }
        return true;
    }

    // Expression statement (function call, etc.)
    if (strcmp(op_str, "expr_stmt") == 0) {
        cJSON* expr = cJSON_GetObjectItem(stmt, "expr");
        if (expr) {
            char* expr_c = c_expr_to_c(expr);
            fprintf(output, "%*s%s;\n", indent, "", expr_c);
            free(expr_c);
        }
        return true;
    }

    // If statement
    if (strcmp(op_str, "if") == 0) {
        cJSON* cond = cJSON_GetObjectItem(stmt, "condition");
        cJSON* then_block = cJSON_GetObjectItem(stmt, "then");
        cJSON* else_block = cJSON_GetObjectItem(stmt, "else");

        char* cond_c = c_expr_to_c(cond);
        fprintf(output, "%*sif (%s) {\n", indent, "", cond_c);
        free(cond_c);

        // Process then statements
        if (then_block && cJSON_IsArray(then_block)) {
            cJSON* then_stmt;
            cJSON_ArrayForEach(then_stmt, then_block) {
                if (!c_stmt_to_c(output, then_stmt, indent + 4, output_path)) return false;
            }
        }

        if (else_block && cJSON_IsArray(else_block) && cJSON_GetArraySize(else_block) > 0) {
            fprintf(output, "%*s} else {\n", indent, "");
            cJSON* else_stmt;
            cJSON_ArrayForEach(else_stmt, else_block) {
                if (!c_stmt_to_c(output, else_stmt, indent + 4, output_path)) return false;
            }
        }
        fprintf(output, "%*s}\n", indent, "");
        return true;
    }

    // While loop
    if (strcmp(op_str, "while") == 0) {
        cJSON* cond = cJSON_GetObjectItem(stmt, "condition");
        cJSON* body = cJSON_GetObjectItem(stmt, "body");

        char* cond_c = c_expr_to_c(cond);
        fprintf(output, "%*swhile (%s) {\n", indent, "", cond_c);
        free(cond_c);

        if (body && cJSON_IsArray(body)) {
            cJSON* body_stmt;
            cJSON_ArrayForEach(body_stmt, body) {
                if (!c_stmt_to_c(output, body_stmt, indent + 4, output_path)) return false;
            }
        }
        fprintf(output, "%*s}\n", indent, "");
        return true;
    }

    // For loop (classic for, not for-each)
    if (strcmp(op_str, "for") == 0) {
        cJSON* init = cJSON_GetObjectItem(stmt, "init");
        cJSON* cond = cJSON_GetObjectItem(stmt, "condition");
        cJSON* update = cJSON_GetObjectItem(stmt, "update");
        cJSON* body = cJSON_GetObjectItem(stmt, "body");

        char* init_c = init ? c_expr_to_c(init) : strdup("");
        char* cond_c = cond ? c_expr_to_c(cond) : strdup("true");
        char* update_c = update ? c_expr_to_c(update) : strdup("");

        fprintf(output, "%*sfor (%s; %s; %s) {\n", indent, "", init_c, cond_c, update_c);
        free(init_c); free(cond_c); free(update_c);

        if (body && cJSON_IsArray(body)) {
            cJSON* body_stmt;
            cJSON_ArrayForEach(body_stmt, body) {
                if (!c_stmt_to_c(output, body_stmt, indent + 4, output_path)) return false;
            }
        }
        fprintf(output, "%*s}\n", indent, "");
        return true;
    }

    // For-each loop
    if (strcmp(op_str, "for_each") == 0) {
        cJSON* item = cJSON_GetObjectItem(stmt, "item");
        cJSON* in_expr = cJSON_GetObjectItem(stmt, "in");
        cJSON* body = cJSON_GetObjectItem(stmt, "body");

        const char* item_name = item && cJSON_IsString(item) ? item->valuestring : "item";

        // Check if it's a range-based loop: __range__(start, end)
        cJSON* range_start = NULL;
        cJSON* range_end = NULL;

        if (c_is_range_call(in_expr, &range_start, &range_end)) {
            // Generate: for (int item = start; item < end; item++)
            char* start_c = c_expr_to_c(range_start);
            char* end_c = c_expr_to_c(range_end);

            fprintf(output, "%*sfor (int %s = %s; %s < %s; %s++) {\n",
                    indent, "", item_name, start_c, item_name, end_c, item_name);

            free(start_c);
            free(end_c);
        } else {
            // Array iteration: for (int _idx = 0; _idx < arr_len; _idx++)
            char* arr_c = c_expr_to_c(in_expr);

            // Generate index variable with unique name
            fprintf(output, "%*sfor (int _%s_idx = 0; _%s_idx < %s_len; _%s_idx++) {\n",
                    indent, "", item_name, item_name, arr_c, item_name);

            // Generate item assignment: type item = arr[_idx]
            fprintf(output, "%*s    %s = %s[_%s_idx];\n",
                    indent, "", item_name, arr_c, item_name);

            free(arr_c);
        }

        // Generate body statements
        if (body && cJSON_IsArray(body)) {
            cJSON* body_stmt;
            cJSON_ArrayForEach(body_stmt, body) {
                if (!c_stmt_to_c(output, body_stmt, indent + 4, output_path)) {
                    return false;
                }
            }
        }

        fprintf(output, "%*s}\n", indent, "");
        return true;
    }

    // Break statement
    if (strcmp(op_str, "break") == 0) {
        fprintf(output, "%*sbreak;\n", indent, "");
        return true;
    }

    // Continue statement
    if (strcmp(op_str, "continue") == 0) {
        fprintf(output, "%*scontinue;\n", indent, "");
        return true;
    }

    // Block statement
    if (strcmp(op_str, "block") == 0) {
        cJSON* stmts = cJSON_GetObjectItem(stmt, "statements");
        fprintf(output, "%*s{\n", indent, "");
        if (stmts && cJSON_IsArray(stmts)) {
            cJSON* s;
            cJSON_ArrayForEach(s, stmts) {
                if (!c_stmt_to_c(output, s, indent + 4, output_path)) return false;
            }
        }
        fprintf(output, "%*s}\n", indent, "");
        return true;
    }

    // Unknown statement - HARD ERROR
    fprintf(stderr, "[C Codegen] Error: Unsupported statement op '%s' in C codegen.\n", op_str);
    fprintf(stderr, "            File: %s\n", output_path ? output_path : "unknown");
    return false;
}
