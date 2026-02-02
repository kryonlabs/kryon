/**
 * C Code Generator - Expression and Statement Transpilation
 *
 * Converts KIR expressions and statements to C code.
 */

#define _DEFAULT_SOURCE

#include "ir_c_expression.h"
#include "ir_c_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Variable Tracking Implementation
// ============================================================================

void c_reset_local_vars(CCodegenContext* ctx) {
    if (ctx) {
        ctx->local_var_count = 0;
    }
}

bool c_is_local_var(CCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return false;
    for (int i = 0; i < ctx->local_var_count; i++) {
        if (strcmp(ctx->local_vars[i], name) == 0) {
            return true;
        }
    }
    return false;
}

void c_add_local_var(CCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return;
    if (ctx->local_var_count >= 64) return;  // Overflow protection
    if (c_is_local_var(ctx, name)) return;   // Already declared

    strncpy(ctx->local_vars[ctx->local_var_count], name, 255);
    ctx->local_vars[ctx->local_var_count][255] = '\0';
    ctx->local_var_count++;
}

bool c_is_global_var(CCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return false;
    for (int i = 0; i < ctx->global_var_count; i++) {
        if (strcmp(ctx->global_vars[i], name) == 0) {
            return true;
        }
    }
    return false;
}

void c_init_global_vars(CCodegenContext* ctx) {
    if (!ctx || !ctx->root_json) return;

    ctx->global_var_count = 0;

    // Get global variables from source_structures.const_declarations
    cJSON* source_structures = cJSON_GetObjectItem(ctx->root_json, "source_structures");
    if (!source_structures) return;

    cJSON* const_decls = cJSON_GetObjectItem(source_structures, "const_declarations");
    if (!const_decls || !cJSON_IsArray(const_decls)) return;

    cJSON* decl;
    cJSON_ArrayForEach(decl, const_decls) {
        cJSON* name = cJSON_GetObjectItem(decl, "name");
        cJSON* scope = cJSON_GetObjectItem(decl, "scope");

        if (name && cJSON_IsString(name)) {
            // Check if global scope
            if (!scope || (cJSON_IsString(scope) && strcmp(scope->valuestring, "global") == 0)) {
                if (ctx->global_var_count < 64) {
                    strncpy(ctx->global_vars[ctx->global_var_count], name->valuestring, 255);
                    ctx->global_vars[ctx->global_var_count][255] = '\0';
                    ctx->global_var_count++;
                }
            }
        }
    }

    // Also add reactive variables as globals
    if (ctx->reactive_vars && cJSON_IsArray(ctx->reactive_vars)) {
        cJSON* var;
        cJSON_ArrayForEach(var, ctx->reactive_vars) {
            cJSON* name = cJSON_GetObjectItem(var, "name");
            if (name && cJSON_IsString(name) && ctx->global_var_count < 64) {
                strncpy(ctx->global_vars[ctx->global_var_count], name->valuestring, 255);
                ctx->global_vars[ctx->global_var_count][255] = '\0';
                ctx->global_var_count++;
            }
        }
    }
}

const char* c_infer_type(cJSON* expr) {
    if (!expr) return "void*";

    // Check for method_call to infer return type
    cJSON* op = cJSON_GetObjectItem(expr, "op");
    if (op && cJSON_IsString(op)) {
        const char* op_str = op->valuestring;

        if (strcmp(op_str, "method_call") == 0) {
            cJSON* receiver = cJSON_GetObjectItem(expr, "receiver");
            cJSON* method = cJSON_GetObjectItem(expr, "method");

            if (receiver && method && cJSON_IsString(method)) {
                cJSON* var = cJSON_GetObjectItem(receiver, "var");
                if (var && cJSON_IsString(var)) {
                    // Storage.load returns void* (array)
                    if (strcmp(var->valuestring, "Storage") == 0) {
                        return "void*";
                    }
                    // DateTime.today returns KryonDate*
                    if (strcmp(var->valuestring, "DateTime") == 0) {
                        if (strcmp(method->valuestring, "today") == 0 ||
                            strcmp(method->valuestring, "now") == 0) {
                            return "KryonDate*";
                        }
                    }
                }
            }
        }

        // String literal
        if (strcmp(op_str, "string") == 0) return "const char*";

        // Number literal
        if (strcmp(op_str, "number") == 0) return "int";
    }

    // Check for simple types
    if (cJSON_IsString(expr)) return "const char*";
    if (cJSON_IsNumber(expr)) return "int";
    if (cJSON_IsBool(expr)) return "bool";

    return "void*";  // Default to void*
}

// ============================================================================
// Expression Conversion
// ============================================================================

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
 * Helper function for binary operations
 * Formats left and right expressions with an operator
 * Returns malloc'd string (via asprintf), caller must free
 */
static char* c_format_binary_op(cJSON* expr, const char* op) {
    cJSON* left_node = cJSON_GetObjectItem(expr, "left");
    cJSON* right_node = cJSON_GetObjectItem(expr, "right");

    char* left = c_expr_to_c(left_node);
    char* right = c_expr_to_c(right_node);

    char* result = NULL;
    asprintf(&result, "(%s %s %s)", left, op, right);

    free(left);
    free(right);
    return result;
}

/**
 * Helper function for unary operations
 * Formats an expression with a prefix operator
 * Returns malloc'd string (via asprintf), caller must free
 */
static char* c_format_unary_op(cJSON* expr, const char* prefix) {
    char* operand = c_expr_to_c(cJSON_GetObjectItem(expr, "operand"));
    char* result = NULL;
    asprintf(&result, "%s%s", prefix, operand);
    free(operand);
    return result;
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
        const char* var_name = var->valuestring;

        // Handle parser bugs - {"var": "}"} from broken object literal parsing
        if (strcmp(var_name, "}") == 0 || strcmp(var_name, "{") == 0 ||
            strcmp(var_name, "{}") == 0) {
            return strdup("NULL");
        }

        return strdup(var_name);
    }

    // Non-op based index access: {"index": arr, "at": idx}
    // This pattern is used in KIR for array indexing without explicit "op" field
    cJSON* index_arr = cJSON_GetObjectItem(expr, "index");
    cJSON* at_expr = cJSON_GetObjectItem(expr, "at");
    if (index_arr && at_expr) {
        char* arr_c = c_expr_to_c(index_arr);
        char* at_c = c_expr_to_c(at_expr);
        char* result = NULL;
        asprintf(&result, "%s[%s]", arr_c, at_c);
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
        return c_format_binary_op(expr, "+");
    }
    if (strcmp(op_str, "sub") == 0) {
        return c_format_binary_op(expr, "-");
    }
    if (strcmp(op_str, "mul") == 0) {
        return c_format_binary_op(expr, "*");
    }
    if (strcmp(op_str, "div") == 0) {
        return c_format_binary_op(expr, "/");
    }
    if (strcmp(op_str, "mod") == 0) {
        return c_format_binary_op(expr, "%");
    }

    // Comparison operations
    if (strcmp(op_str, "eq") == 0) {
        return c_format_binary_op(expr, "==");
    }
    if (strcmp(op_str, "ne") == 0) {
        return c_format_binary_op(expr, "!=");
    }
    if (strcmp(op_str, "lt") == 0) {
        return c_format_binary_op(expr, "<");
    }
    if (strcmp(op_str, "gt") == 0) {
        return c_format_binary_op(expr, ">");
    }
    if (strcmp(op_str, "le") == 0) {
        return c_format_binary_op(expr, "<=");
    }
    if (strcmp(op_str, "ge") == 0) {
        return c_format_binary_op(expr, ">=");
    }

    // Logical operations
    if (strcmp(op_str, "and") == 0) {
        return c_format_binary_op(expr, "&&");
    }
    if (strcmp(op_str, "or") == 0) {
        return c_format_binary_op(expr, "||");
    }

    // Unary operations
    if (strcmp(op_str, "not") == 0) {
        return c_format_unary_op(expr, "(!");
    }

    // Unary minus
    if (strcmp(op_str, "neg") == 0) {
        return c_format_unary_op(expr, "(-");
    }

    // Member access: obj.property or obj->property for pointers
    if (strcmp(op_str, "member_access") == 0) {
        cJSON* object = cJSON_GetObjectItem(expr, "object");
        char* obj = c_expr_to_c(object);
        cJSON* prop = cJSON_GetObjectItem(expr, "property");
        const char* prop_str = prop && cJSON_IsString(prop) ? prop->valuestring : "unknown";

        // Trim trailing whitespace from property name (parser bug: "length " instead of "length")
        char prop_clean[256];
        strncpy(prop_clean, prop_str, sizeof(prop_clean) - 1);
        prop_clean[sizeof(prop_clean) - 1] = '\0';
        size_t len = strlen(prop_clean);
        while (len > 0 && (prop_clean[len-1] == ' ' || prop_clean[len-1] == '\t')) {
            prop_clean[--len] = '\0';
        }

        // Special case: array.length -> array_count in C (for void* arrays)
        cJSON* obj_var = cJSON_GetObjectItem(object, "var");
        if (obj_var && cJSON_IsString(obj_var) && strcmp(prop_clean, "length") == 0) {
            // Convert array.length to array_count
            char* result = NULL;
            asprintf(&result, "%s_count", obj);
            free(obj);
            return result;
        }

        // Determine if we should use -> (pointer access) or . (value access)
        // Use -> if the object is a simple variable (likely a pointer in our context)
        // Use . if it's already a member access (nested struct access)
        bool use_arrow = (obj_var && cJSON_IsString(obj_var));  // Simple variable reference

        // But don't use arrow for known non-pointer types like DateTime modules
        if (use_arrow && obj_var) {
            const char* var_name = obj_var->valuestring;
            if (strcmp(var_name, "Storage") == 0 || strcmp(var_name, "DateTime") == 0 ||
                strcmp(var_name, "Math") == 0 || strcmp(var_name, "UUID") == 0) {
                use_arrow = false;  // Module/namespace access, use .
            }
        }

        char* result = NULL;
        asprintf(&result, "%s%s%s", obj, use_arrow ? "->" : ".", prop_clean);
        free(obj);
        return result;
    }

    // Index access: arr[index]
    if (strcmp(op_str, "index_access") == 0) {
        char* obj = c_expr_to_c(cJSON_GetObjectItem(expr, "object"));
        char* index = c_expr_to_c(cJSON_GetObjectItem(expr, "index"));
        char* result = NULL;
        asprintf(&result, "%s[%s]", obj, index);
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

    // Object literal: {} - empty or with properties
    // In C, we represent as NULL or a struct initializer
    if (strcmp(op_str, "object_literal") == 0) {
        return strdup("NULL /* object literal */");
    }

    // Method call: receiver.method(args)
    // Generate C-style function call: receiver_method(args)
    if (strcmp(op_str, "method_call") == 0) {
        cJSON* receiver = cJSON_GetObjectItem(expr, "receiver");
        cJSON* method = cJSON_GetObjectItem(expr, "method");
        cJSON* args = cJSON_GetObjectItem(expr, "args");

        char* recv_str = receiver ? c_expr_to_c(receiver) : strdup("this");
        const char* method_str = method && cJSON_IsString(method) ? method->valuestring : "method";

        // Build args string
        // Use receiver_method format for C-style calls
        size_t total_len = strlen(recv_str) + strlen(method_str) + 4;  // _() \0
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
        // Generate C-style: Receiver_method(args) instead of receiver.method(args)
        sprintf(result, "%s_%s(", recv_str, method_str);
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
 * Convert KIR statement to C code with context for variable tracking
 * Returns false on unsupported construct (hard error)
 */
bool c_stmt_to_c_ctx(struct CCodegenContext* ctx, FILE* output, cJSON* stmt, int indent, const char* output_path) {
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
        } else if (strcmp(var_type, "auto") == 0 && init) {
            // Try to infer from init expression
            c_type = c_infer_type(init);
        }

        fprintf(output, "%*s%s %s", indent, "", c_type, var_name);
        if (init) {
            char* init_c = c_expr_to_c(init);
            fprintf(output, " = %s", init_c);
            free(init_c);
        }
        fprintf(output, ";\n");

        // Track this as a local variable
        if (ctx) {
            c_add_local_var(ctx, var_name);
        }
        return true;
    }

    // Assignment: target = expr;
    if (strcmp(op_str, "assign") == 0) {
        cJSON* target = cJSON_GetObjectItem(stmt, "target");
        cJSON* expr = cJSON_GetObjectItem(stmt, "expr");

        // Target is either a string (variable name) or an expression (for member/index access)
        char* target_c = NULL;
        bool is_simple_var = false;
        bool has_member_access = false;

        if (cJSON_IsString(target)) {
            const char* target_str = target->valuestring;

            // Check if target string contains member access (e.g., "habit.completions")
            const char* dot = strchr(target_str, '.');
            if (dot) {
                // Convert "habit.property" to "habit->property" for pointer types
                size_t obj_len = dot - target_str;
                char obj_name[256];
                strncpy(obj_name, target_str, obj_len);
                obj_name[obj_len] = '\0';
                const char* prop_name = dot + 1;

                // Allocate and build the target with arrow operator
                target_c = malloc(strlen(target_str) + 2);
                sprintf(target_c, "%s->%s", obj_name, prop_name);
                has_member_access = true;
            } else {
                target_c = strdup(target_str);
                is_simple_var = true;
            }
        } else {
            target_c = c_expr_to_c(target);
        }

        char* expr_c = expr ? c_expr_to_c(expr) : strdup("NULL");

        // Check if target is a reactive variable (need to use kryon_signal_set)
        bool is_reactive = false;
        char* signal_name = NULL;
        if (ctx && is_simple_var) {
            // Check if this is a reactive variable
            if (ctx->reactive_vars && cJSON_IsArray(ctx->reactive_vars)) {
                cJSON* var;
                cJSON_ArrayForEach(var, ctx->reactive_vars) {
                    cJSON* vname = cJSON_GetObjectItem(var, "name");
                    cJSON* vscope = cJSON_GetObjectItem(var, "scope");
                    if (vname && vname->valuestring && strcmp(vname->valuestring, target_c) == 0) {
                        is_reactive = true;
                        // Build signal name based on scope
                        const char* scope = (vscope && vscope->valuestring) ? vscope->valuestring : "component";
                        signal_name = malloc(strlen(target_c) + strlen(scope) + 16);
                        if (strcmp(scope, "global") == 0) {
                            sprintf(signal_name, "%s_global_signal", target_c);
                        } else if (strcmp(scope, "component") != 0) {
                            sprintf(signal_name, "%s_%s_signal", target_c, scope);
                        } else {
                            sprintf(signal_name, "%s_signal", target_c);
                        }
                        break;
                    }
                }
            }
        }

        // Check if this is a simple variable that needs declaration
        if (is_reactive && signal_name) {
            // Reactive variable - use signal setter
            fprintf(output, "%*skryon_signal_set(%s, %s);\n", indent, "", signal_name, expr_c);
            free(signal_name);
        } else if (is_simple_var && ctx) {
            // Check if global or already declared as local
            if (c_is_global_var(ctx, target_c) || c_is_local_var(ctx, target_c)) {
                // Already declared, just assign
                fprintf(output, "%*s%s = %s;\n", indent, "", target_c, expr_c);
            } else {
                // Needs declaration - infer type from expression
                const char* inferred_type = c_infer_type(expr);
                fprintf(output, "%*s%s %s = %s;\n", indent, "", inferred_type, target_c, expr_c);
                c_add_local_var(ctx, target_c);
            }
        } else {
            // Complex target or member access - just assign
            fprintf(output, "%*s%s = %s;\n", indent, "", target_c, expr_c);
        }

        free(target_c);
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
                if (!c_stmt_to_c_ctx(ctx, output, then_stmt, indent + 4, output_path)) return false;
            }
        }

        if (else_block && cJSON_IsArray(else_block) && cJSON_GetArraySize(else_block) > 0) {
            fprintf(output, "%*s} else {\n", indent, "");
            cJSON* else_stmt;
            cJSON_ArrayForEach(else_stmt, else_block) {
                if (!c_stmt_to_c_ctx(ctx, output, else_stmt, indent + 4, output_path)) return false;
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
                if (!c_stmt_to_c_ctx(ctx, output, body_stmt, indent + 4, output_path)) return false;
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
                if (!c_stmt_to_c_ctx(ctx, output, body_stmt, indent + 4, output_path)) return false;
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
            // Array iteration: for (int _idx = 0; _idx < arr_count; _idx++)
            char* arr_c = c_expr_to_c(in_expr);

            // Generate index variable with unique name
            // Use _count suffix to match the global variable naming convention
            fprintf(output, "%*sfor (int _%s_idx = 0; _%s_idx < %s_count; _%s_idx++) {\n",
                    indent, "", item_name, item_name, arr_c, item_name);

            // Infer element type from item name (e.g., "habit" -> "Habit")
            char base_type[128];
            char ptr_type[128];
            if (item_name[0] >= 'a' && item_name[0] <= 'z') {
                // Capitalize first letter for the type name
                snprintf(base_type, sizeof(base_type), "%c%s",
                         item_name[0] - 32, item_name + 1);
                snprintf(ptr_type, sizeof(ptr_type), "%s*", base_type);
            } else {
                strcpy(base_type, "void");
                strcpy(ptr_type, "void*");
            }

            // Generate item assignment: Habit* habit = &((Habit*)arr)[idx]
            fprintf(output, "%*s    %s %s = &((%s*)%s)[_%s_idx];\n",
                    indent, "", ptr_type, item_name, base_type, arr_c, item_name);

            free(arr_c);
        }

        // Generate body statements
        if (body && cJSON_IsArray(body)) {
            cJSON* body_stmt;
            cJSON_ArrayForEach(body_stmt, body) {
                if (!c_stmt_to_c_ctx(ctx, output, body_stmt, indent + 4, output_path)) {
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
                if (!c_stmt_to_c_ctx(ctx, output, s, indent + 4, output_path)) return false;
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

/**
 * Convert KIR statement to C code, write to output (backward compatible wrapper)
 * Returns false on unsupported construct (hard error)
 */
bool c_stmt_to_c(FILE* output, cJSON* stmt, int indent, const char* output_path) {
    return c_stmt_to_c_ctx(NULL, output, stmt, indent, output_path);
}
