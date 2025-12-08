#define _POSIX_C_SOURCE 200809L
#include "ir_executor.h"
#include "ir_expression.h"
#include "ir_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations
void ir_executor_set_var(IRExecutorContext* ctx, const char* name, int64_t value, uint32_t instance_id);
static void ir_executor_update_conditionals(IRExecutorContext* ctx);

// ============================================================================
// RUNTIME CONDITIONALS
// ============================================================================

#define IR_EXECUTOR_MAX_CONDITIONALS 32

typedef struct {
    IRExpression* condition;        // Parsed condition expression
    uint32_t* then_ids;             // Component IDs to show when true
    uint32_t then_count;
    uint32_t* else_ids;             // Component IDs to show when false
    uint32_t else_count;
} IRRuntimeConditional;

static IRRuntimeConditional g_conditionals[IR_EXECUTOR_MAX_CONDITIONALS];
static int g_conditional_count = 0;

// ============================================================================
// GLOBAL STATE
// ============================================================================

static IRExecutorContext* g_executor = NULL;

IRExecutorContext* ir_executor_get_global(void) {
    return g_executor;
}

void ir_executor_set_global(IRExecutorContext* ctx) {
    g_executor = ctx;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

IRExecutorContext* ir_executor_create(void) {
    IRExecutorContext* ctx = (IRExecutorContext*)calloc(1, sizeof(IRExecutorContext));
    if (!ctx) return NULL;

    ctx->logic = NULL;
    ctx->manifest = NULL;
    ctx->root = NULL;
    ctx->source_count = 0;
    ctx->current_event.component_id = 0;
    ctx->current_event.event_type = NULL;

    return ctx;
}

void ir_executor_destroy(IRExecutorContext* ctx) {
    if (!ctx) return;

    // Free source code strings
    for (int i = 0; i < ctx->source_count; i++) {
        free(ctx->sources[i].lang);
        free(ctx->sources[i].code);
    }

    // Note: We don't own logic, manifest, or root - they're set externally
    free(ctx);

    // Clear global if this was it
    if (g_executor == ctx) {
        g_executor = NULL;
    }
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ir_executor_set_logic(IRExecutorContext* ctx, IRLogicBlock* logic) {
    if (ctx) {
        ctx->logic = logic;
    }
}

void ir_executor_set_manifest(IRExecutorContext* ctx, IRReactiveManifest* manifest) {
    if (ctx) {
        ctx->manifest = manifest;
    }
}

// Helper: Initialize variables from Text components with text_expression
static void init_vars_from_text_recursive(IRExecutorContext* ctx, IRComponent* comp) {
    if (!comp) return;

    // If this is a Text component with text_expression, initialize the variable
    if (comp->type == IR_COMPONENT_TEXT && comp->text_expression && comp->text_content
        && comp->owner_instance_id != 0) {
        // Extract variable name from text_expression (e.g., "{{value}}" -> "value")
        const char* start = strstr(comp->text_expression, "{{");
        if (start) {
            const char* end = strstr(start, "}}");
            if (end) {
                int nameLen = end - start - 2;
                if (nameLen > 0 && nameLen < 64) {
                    char varName[65];
                    strncpy(varName, start + 2, nameLen);
                    varName[nameLen] = '\0';

                    // Parse initial value from text_content
                    int64_t value = atoll(comp->text_content);
                    ir_executor_set_var(ctx, varName, value, comp->owner_instance_id);
                    printf("[executor] Initialized var %s (instance %u) = %ld\n",
                           varName, comp->owner_instance_id, (long)value);
                }
            }
        }
    }

    // Recurse to children
    for (uint32_t i = 0; i < comp->child_count; i++) {
        init_vars_from_text_recursive(ctx, comp->children[i]);
    }
}

void ir_executor_set_root(IRExecutorContext* ctx, IRComponent* root) {
    if (ctx) {
        // Only initialize variables on first root set (avoid re-initialization on every event)
        bool is_new_root = (ctx->root != root);
        ctx->root = root;
        // Initialize variables from Text components (only on first call)
        // Note: Always initialize on new root, don't check var_count
        if (root && is_new_root) {
            init_vars_from_text_recursive(ctx, root);
        }
    }
}

void ir_executor_apply_initial_conditionals(IRExecutorContext* ctx) {
    if (!ctx || !ctx->root) return;
    printf("[executor] Applying initial conditionals (%d total)\n", g_conditional_count);
    ir_executor_update_conditionals(ctx);
}

void ir_executor_add_source(IRExecutorContext* ctx, const char* lang, const char* code) {
    if (!ctx || !lang || !code) return;
    if (ctx->source_count >= IR_EXECUTOR_MAX_SOURCES) return;

    // Check if language already exists, update if so
    for (int i = 0; i < ctx->source_count; i++) {
        if (strcmp(ctx->sources[i].lang, lang) == 0) {
            free(ctx->sources[i].code);
            ctx->sources[i].code = strdup(code);
            return;
        }
    }

    // Add new source
    ctx->sources[ctx->source_count].lang = strdup(lang);
    ctx->sources[ctx->source_count].code = strdup(code);
    ctx->source_count++;
}

const char* ir_executor_get_source(IRExecutorContext* ctx, const char* lang) {
    if (!ctx || !lang) return NULL;

    for (int i = 0; i < ctx->source_count; i++) {
        if (strcmp(ctx->sources[i].lang, lang) == 0) {
            return ctx->sources[i].code;
        }
    }
    return NULL;
}

// ============================================================================
// HANDLER EXECUTION (Phase 1: Print-based POC)
// ============================================================================

// Extract the actual function name from universal statements
// e.g., {"var": "handleButtonClick"} -> "handleButtonClick"
static const char* extract_handler_function_name(IRLogicFunction* func) {
    if (!func || !func->has_universal || func->statement_count == 0) {
        return NULL;
    }

    // Look for the var reference in statements
    for (int i = 0; i < func->statement_count; i++) {
        IRStatement* stmt = func->statements[i];
        if (!stmt) continue;

        // Check for assign statement with var expression (anonymous union)
        if (stmt->type == STMT_ASSIGN && stmt->assign.expr) {
            IRExpression* expr = stmt->assign.expr;
            if (expr->type == EXPR_VAR_REF) {
                return expr->var_ref.name;
            }
        }
    }

    return NULL;
}

// Get source code for a function - checks function-level sources first, then root-level
static const char* get_function_source(IRExecutorContext* ctx, IRLogicFunction* func, const char* lang) {
    // First check function-level sources (more specific)
    if (func && func->source_count > 0) {
        for (int i = 0; i < func->source_count; i++) {
            if (func->sources[i].language && strcmp(func->sources[i].language, lang) == 0) {
                return func->sources[i].source;
            }
        }
    }
    // Fall back to root-level sources
    return ir_executor_get_source(ctx, lang);
}

bool ir_executor_call_handler(IRExecutorContext* ctx, const char* handler_name) {
    if (!ctx || !handler_name) return false;

    printf("[executor] Executing handler: %s\n", handler_name);

    // Find the logic function
    IRLogicFunction* func = NULL;
    if (ctx->logic) {
        func = ir_logic_block_find_function(ctx->logic, handler_name);
    }

    if (func) {
        // FIRST: Try universal logic execution (new path)
        if (func->has_universal && func->statement_count > 0) {
            printf("[executor] Found universal logic with %d statements\n", func->statement_count);

            // Execute universal statements
            bool success = ir_executor_run_universal(ctx, func, ctx->current_instance_id);

            if (success) {
                // Update UI after state change
                ir_executor_update_text_components(ctx);
                return true;
            }
        }

        // FALLBACK: Try language-specific source code
        // Extract the actual function name from universal statements
        const char* func_name = extract_handler_function_name(func);
        if (func_name) {
            printf("[executor] Handler function: %s\n", func_name);
        } else {
            // If no universal statements, check if function has per-function source
            // The source might just be the function name reference
            const char* func_source = get_function_source(ctx, func, "nim");
            if (func_source) {
                func_name = func_source;  // Use source as function name
                printf("[executor] Handler function (from source): %s\n", func_name);
            }
        }

        // Try to find and execute source code
        // Priority: nim > lua > js > c

        // Phase 1: Just print the source code to prove we found it
        const char* nim_source = get_function_source(ctx, func, "nim");
        if (nim_source && func_name) {
            // Check if the source is a full proc definition
            char search_pattern[256];
            snprintf(search_pattern, sizeof(search_pattern), "proc %s", func_name);

            if (strstr(nim_source, search_pattern)) {
                printf("[executor] Found Nim handler '%s'\n", func_name);

                // For Phase 1 POC: Execute print statements by parsing simple echo
                // In a real implementation, this would call the Nim runtime

                // Simple pattern matching for echo "..."
                const char* echo_start = strstr(nim_source, "echo \"");
                if (echo_start) {
                    echo_start += 6;  // Skip 'echo "'
                    const char* echo_end = strchr(echo_start, '"');
                    if (echo_end) {
                        int len = echo_end - echo_start;
                        char* message = (char*)malloc(len + 1);
                        strncpy(message, echo_start, len);
                        message[len] = '\0';

                        // Print the echo output!
                        printf("%s\n", message);
                        free(message);
                        return true;
                    }
                }
            } else {
                // Source might just be the function name - log that we found the reference
                printf("[executor] Found handler reference: %s (source: %s)\n", handler_name, nim_source);
                // For now, we can't execute just a name reference without the full source
                // In the future, this could look up the function in a symbol table
            }
        }

        // Try Lua source
        const char* lua_source = get_function_source(ctx, func, "lua");
        if (lua_source && func_name) {
            char search_pattern[256];
            snprintf(search_pattern, sizeof(search_pattern), "function %s", func_name);

            if (strstr(lua_source, search_pattern)) {
                printf("[executor] Found Lua handler '%s'\n", func_name);

                // Simple pattern matching for print("...")
                const char* print_start = strstr(lua_source, "print(\"");
                if (print_start) {
                    print_start += 7;  // Skip 'print("'
                    const char* print_end = strchr(print_start, '"');
                    if (print_end) {
                        int len = print_end - print_start;
                        char* message = (char*)malloc(len + 1);
                        strncpy(message, print_start, len);
                        message[len] = '\0';

                        printf("%s\n", message);
                        free(message);
                        return true;
                    }
                }
            }
        }

        printf("[executor] No executable source found for handler\n");
    } else {
        printf("[executor] Handler not found in logic block\n");
    }

    return false;
}

// Helper: Find component by ID in tree
static IRComponent* find_component_by_id(IRComponent* root, uint32_t id) {
    if (!root) return NULL;
    if (root->id == id) return root;
    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* found = find_component_by_id(root->children[i], id);
        if (found) return found;
    }
    return NULL;
}

bool ir_executor_handle_event(IRExecutorContext* ctx,
                               uint32_t component_id,
                               const char* event_type) {
    if (!ctx || !event_type) return false;

    printf("[executor] Event '%s' on component %u\n", event_type, component_id);

    // Find the clicked component to get its owner_instance_id
    if (ctx->root) {
        IRComponent* clicked = find_component_by_id(ctx->root, component_id);
        if (clicked && clicked->owner_instance_id != 0) {
            ctx->current_instance_id = clicked->owner_instance_id;
            printf("[executor] Using owner_instance_id=%u for state\n", ctx->current_instance_id);
        }
    }

    // Store current event context
    ctx->current_event.component_id = component_id;
    ctx->current_event.event_type = event_type;

    // Look up the handler name from logic block
    if (!ctx->logic) {
        printf("[executor] No logic block set\n");
        return false;
    }

    const char* handler_name = ir_logic_block_get_handler(ctx->logic, component_id, event_type);
    if (!handler_name) {
        printf("[executor] No handler found for component %u event '%s'\n",
               component_id, event_type);
        return false;
    }

    printf("[executor] Found handler: %s\n", handler_name);

    // Execute the handler
    return ir_executor_call_handler(ctx, handler_name);
}

// Handle event by logic_id directly (used when component has been remapped)
bool ir_executor_handle_event_by_logic_id(IRExecutorContext* ctx,
                                           uint32_t component_id,
                                           const char* logic_id) {
    if (!ctx || !logic_id) return false;

    printf("[executor] Event with logic_id '%s' on component %u\n", logic_id, component_id);

    // Find the clicked component to get its owner_instance_id
    if (ctx->root) {
        IRComponent* clicked = find_component_by_id(ctx->root, component_id);
        if (clicked && clicked->owner_instance_id != 0) {
            ctx->current_instance_id = clicked->owner_instance_id;
            printf("[executor] Using owner_instance_id=%u for state\n", ctx->current_instance_id);
        }
    }

    // Store current event context
    ctx->current_event.component_id = component_id;
    ctx->current_event.event_type = "click";

    // First try to execute handler directly using logic_id
    if (ir_executor_call_handler(ctx, logic_id)) {
        return true;
    }

    // If direct call failed, try to lookup handler via event_bindings
    // logic_id format: "nim_button_<template_id>" -> extract template_id
    if (ctx->logic && strncmp(logic_id, "nim_button_", 11) == 0) {
        uint32_t template_id = (uint32_t)atoi(logic_id + 11);
        printf("[executor] Looking up handler for template_id %u from logic_id '%s'\n",
               template_id, logic_id);

        // Look up the handler name from event_bindings by template_id
        const char* handler_name = ir_logic_block_get_handler(ctx->logic, template_id, "click");
        if (handler_name) {
            printf("[executor] Found handler '%s' for template_id %u\n", handler_name, template_id);
            return ir_executor_call_handler(ctx, handler_name);
        }
    }

    // Also try for checkbox
    if (ctx->logic && strncmp(logic_id, "nim_checkbox_", 13) == 0) {
        uint32_t template_id = (uint32_t)atoi(logic_id + 13);
        const char* handler_name = ir_logic_block_get_handler(ctx->logic, template_id, "click");
        if (handler_name) {
            printf("[executor] Found handler '%s' for checkbox template_id %u\n", handler_name, template_id);
            return ir_executor_call_handler(ctx, handler_name);
        }
    }

    printf("[executor] No handler found for logic_id '%s'\n", logic_id);
    return false;
}

// ============================================================================
// FILE LOADING HELPER
// ============================================================================

#include "cJSON.h"

bool ir_executor_load_kir_file(IRExecutorContext* ctx, const char* kir_path) {
    if (!ctx || !kir_path) return false;

    // Read file contents
    FILE* f = fopen(kir_path, "rb");
    if (!f) {
        printf("[executor] Failed to open file: %s\n", kir_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        return false;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    // Parse JSON
    cJSON* root = cJSON_Parse(content);
    free(content);

    if (!root) {
        printf("[executor] Failed to parse JSON: %s\n", kir_path);
        return false;
    }

    // Parse logic block
    cJSON* logic = cJSON_GetObjectItem(root, "logic");
    if (logic) {
        IRLogicBlock* block = ir_logic_block_from_json(logic);
        if (block) {
            ir_executor_set_logic(ctx, block);
            printf("[executor] Loaded logic block from %s\n", kir_path);
        }
    }

    // Parse sources
    cJSON* sources = cJSON_GetObjectItem(root, "sources");
    if (sources) {
        cJSON* source = sources->child;
        while (source) {
            if (source->string && cJSON_IsString(source)) {
                ir_executor_add_source(ctx, source->string, source->valuestring);
                printf("[executor] Loaded source for language: %s\n", source->string);
            }
            source = source->next;
        }
    }

    // Parse reactive_manifest
    cJSON* manifest = cJSON_GetObjectItem(root, "reactive_manifest");
    if (manifest) {
        // Parse variables - initialize them
        cJSON* variables = cJSON_GetObjectItem(manifest, "variables");
        if (variables && cJSON_IsArray(variables)) {
            cJSON* varObj;
            cJSON_ArrayForEach(varObj, variables) {
                cJSON* nameJ = cJSON_GetObjectItem(varObj, "name");
                cJSON* initialJ = cJSON_GetObjectItem(varObj, "initial_value");

                if (nameJ && cJSON_IsString(nameJ)) {
                    const char* name = nameJ->valuestring;
                    int64_t initial = 0;

                    // Parse initial value
                    if (initialJ) {
                        if (cJSON_IsNumber(initialJ)) {
                            initial = (int64_t)initialJ->valuedouble;
                        } else if (cJSON_IsString(initialJ)) {
                            // Handle "true"/"false" or numeric strings
                            const char* str = initialJ->valuestring;
                            if (strcmp(str, "true") == 0) {
                                initial = 1;
                            } else if (strcmp(str, "false") == 0) {
                                initial = 0;
                            } else {
                                initial = atoll(str);
                            }
                        } else if (cJSON_IsBool(initialJ)) {
                            initial = cJSON_IsTrue(initialJ) ? 1 : 0;
                        }
                    }

                    // Don't call ir_executor_set_var here as it triggers update_conditionals
                    // and we haven't parsed conditionals yet. Directly add the variable.
                    if (ctx->var_count < IR_EXECUTOR_MAX_VARS) {
                        ctx->vars[ctx->var_count].name = strdup(name);
                        ctx->vars[ctx->var_count].value = initial;
                        ctx->vars[ctx->var_count].owner_component_id = 0;  // global
                        ctx->var_count++;
                        printf("[executor] Initialized var %s = %ld from manifest\n", name, (long)initial);
                    }
                }
            }
        }

        // Parse conditionals
        cJSON* conditionals = cJSON_GetObjectItem(manifest, "conditionals");
        if (conditionals && cJSON_IsArray(conditionals)) {
            g_conditional_count = 0;  // Reset

            cJSON* condObj;
            cJSON_ArrayForEach(condObj, conditionals) {
                if (g_conditional_count >= IR_EXECUTOR_MAX_CONDITIONALS) break;

                IRRuntimeConditional* cond = &g_conditionals[g_conditional_count];
                memset(cond, 0, sizeof(IRRuntimeConditional));

                // Parse condition expression
                cJSON* condExpr = cJSON_GetObjectItem(condObj, "condition");
                if (condExpr) {
                    cond->condition = ir_expr_from_json(condExpr);
                    if (cond->condition) {
                        printf("[executor] Parsed conditional expression\n");
                    }
                }

                // Parse then_children_ids
                cJSON* thenIds = cJSON_GetObjectItem(condObj, "then_children_ids");
                if (thenIds && cJSON_IsArray(thenIds)) {
                    int count = cJSON_GetArraySize(thenIds);
                    if (count > 0) {
                        cond->then_ids = (uint32_t*)malloc(count * sizeof(uint32_t));
                        cond->then_count = count;
                        for (int i = 0; i < count; i++) {
                            cJSON* item = cJSON_GetArrayItem(thenIds, i);
                            if (cJSON_IsNumber(item)) {
                                cond->then_ids[i] = (uint32_t)item->valuedouble;
                            }
                        }
                        printf("[executor] Conditional has %d then_children\n", count);
                    }
                }

                // Parse else_children_ids
                cJSON* elseIds = cJSON_GetObjectItem(condObj, "else_children_ids");
                if (elseIds && cJSON_IsArray(elseIds)) {
                    int count = cJSON_GetArraySize(elseIds);
                    if (count > 0) {
                        cond->else_ids = (uint32_t*)malloc(count * sizeof(uint32_t));
                        cond->else_count = count;
                        for (int i = 0; i < count; i++) {
                            cJSON* item = cJSON_GetArrayItem(elseIds, i);
                            if (cJSON_IsNumber(item)) {
                                cond->else_ids[i] = (uint32_t)item->valuedouble;
                            }
                        }
                        printf("[executor] Conditional has %d else_children\n", count);
                    }
                }

                g_conditional_count++;
            }
            printf("[executor] Loaded %d conditionals from manifest\n", g_conditional_count);
        }
    }

    cJSON_Delete(root);
    return true;
}

// ============================================================================
// VARIABLE MANAGEMENT
// ============================================================================

int64_t ir_executor_get_var(IRExecutorContext* ctx, const char* name, uint32_t instance_id) {
    if (!ctx || !name) return 0;

    // Look for variable with matching name and instance ID
    for (int i = 0; i < ctx->var_count; i++) {
        if (ctx->vars[i].owner_component_id == instance_id &&
            ctx->vars[i].name && strcmp(ctx->vars[i].name, name) == 0) {
            return ctx->vars[i].value;
        }
    }

    // Not found, return 0
    return 0;
}

void ir_executor_set_var(IRExecutorContext* ctx, const char* name, int64_t value, uint32_t instance_id) {
    if (!ctx || !name) return;

    // Look for existing variable
    for (int i = 0; i < ctx->var_count; i++) {
        if (ctx->vars[i].owner_component_id == instance_id &&
            ctx->vars[i].name && strcmp(ctx->vars[i].name, name) == 0) {
            ctx->vars[i].value = value;
            printf("[executor] Set var %s (instance %u) = %ld\n", name, instance_id, (long)value);
            // Update conditionals after variable change
            ir_executor_update_conditionals(ctx);
            return;
        }
    }

    // Create new variable
    if (ctx->var_count < IR_EXECUTOR_MAX_VARS) {
        ctx->vars[ctx->var_count].name = strdup(name);
        ctx->vars[ctx->var_count].value = value;
        ctx->vars[ctx->var_count].owner_component_id = instance_id;
        ctx->var_count++;
        printf("[executor] Created var %s (instance %u) = %ld\n", name, instance_id, (long)value);
        // Update conditionals after variable creation
        ir_executor_update_conditionals(ctx);
    }
}

// ============================================================================
// CONDITIONAL UPDATES
// ============================================================================

// Forward declaration of eval_expr (defined below)
static int64_t eval_expr(IRExecutorContext* ctx, IRExpression* expr, uint32_t instance_id);

static void ir_executor_update_conditionals(IRExecutorContext* ctx) {
    if (!ctx || !ctx->root) return;

    for (int i = 0; i < g_conditional_count; i++) {
        IRRuntimeConditional* cond = &g_conditionals[i];
        if (!cond->condition) continue;

        // Evaluate the condition
        int64_t result = eval_expr(ctx, cond->condition, 0);
        bool show_then = result != 0;

        printf("[executor] Conditional %d: result=%ld, show_then=%d\n", i, (long)result, show_then);

        // Update visibility of then_children
        for (uint32_t j = 0; j < cond->then_count; j++) {
            IRComponent* comp = find_component_by_id(ctx->root, cond->then_ids[j]);
            if (comp && comp->style) {
                bool old_visible = comp->style->visible;
                comp->style->visible = show_then;
                printf("[executor] Set component %u visible=%d (then)\n", cond->then_ids[j], show_then);
                // Mark parent for re-layout if visibility changed
                if (old_visible != show_then && comp->parent) {
                    ir_layout_mark_dirty(comp->parent);
                }
            }
        }

        // Update visibility of else_children
        for (uint32_t j = 0; j < cond->else_count; j++) {
            IRComponent* comp = find_component_by_id(ctx->root, cond->else_ids[j]);
            if (comp && comp->style) {
                bool old_visible = comp->style->visible;
                comp->style->visible = !show_then;
                printf("[executor] Set component %u visible=%d (else)\n", cond->else_ids[j], !show_then);
                // Mark parent for re-layout if visibility changed
                if (old_visible != !show_then && comp->parent) {
                    ir_layout_mark_dirty(comp->parent);
                }
            }
        }
    }
}

// ============================================================================
// EXPRESSION EVALUATION
// ============================================================================

static int64_t eval_expr(IRExecutorContext* ctx, IRExpression* expr, uint32_t instance_id) {
    if (!expr) return 0;

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            return expr->int_value;

        case EXPR_LITERAL_BOOL:
            return expr->bool_value ? 1 : 0;

        case EXPR_VAR_REF:
            return ir_executor_get_var(ctx, expr->var_ref.name, instance_id);

        case EXPR_BINARY: {
            int64_t left = eval_expr(ctx, expr->binary.left, instance_id);
            int64_t right = eval_expr(ctx, expr->binary.right, instance_id);

            switch (expr->binary.op) {
                case BINARY_OP_ADD: return left + right;
                case BINARY_OP_SUB: return left - right;
                case BINARY_OP_MUL: return left * right;
                case BINARY_OP_DIV: return right != 0 ? left / right : 0;
                case BINARY_OP_MOD: return right != 0 ? left % right : 0;
                case BINARY_OP_EQ:  return left == right;
                case BINARY_OP_NEQ: return left != right;
                case BINARY_OP_LT:  return left < right;
                case BINARY_OP_LTE: return left <= right;
                case BINARY_OP_GT:  return left > right;
                case BINARY_OP_GTE: return left >= right;
                case BINARY_OP_AND: return left && right;
                case BINARY_OP_OR:  return left || right;
                default: return 0;
            }
        }

        case EXPR_UNARY: {
            int64_t operand = eval_expr(ctx, expr->unary.operand, instance_id);
            switch (expr->unary.op) {
                case UNARY_OP_NEG: return -operand;
                case UNARY_OP_NOT: return !operand;
                default: return 0;
            }
        }

        default:
            return 0;
    }
}

// ============================================================================
// STATEMENT EXECUTION
// ============================================================================

static void exec_stmt(IRExecutorContext* ctx, IRStatement* stmt, uint32_t instance_id) {
    if (!stmt) return;

    switch (stmt->type) {
        case STMT_ASSIGN: {
            int64_t value = eval_expr(ctx, stmt->assign.expr, instance_id);
            ir_executor_set_var(ctx, stmt->assign.target, value, instance_id);
            break;
        }

        case STMT_ASSIGN_OP: {
            int64_t current = ir_executor_get_var(ctx, stmt->assign_op.target, instance_id);
            int64_t delta = eval_expr(ctx, stmt->assign_op.expr, instance_id);
            int64_t result = current;

            switch (stmt->assign_op.op) {
                case ASSIGN_OP_ADD: result = current + delta; break;
                case ASSIGN_OP_SUB: result = current - delta; break;
                case ASSIGN_OP_MUL: result = current * delta; break;
                case ASSIGN_OP_DIV: result = delta != 0 ? current / delta : 0; break;
            }

            ir_executor_set_var(ctx, stmt->assign_op.target, result, instance_id);
            break;
        }

        case STMT_IF: {
            int64_t cond = eval_expr(ctx, stmt->if_stmt.condition, instance_id);
            if (cond) {
                for (int i = 0; i < stmt->if_stmt.then_count; i++) {
                    exec_stmt(ctx, stmt->if_stmt.then_body[i], instance_id);
                }
            } else {
                for (int i = 0; i < stmt->if_stmt.else_count; i++) {
                    exec_stmt(ctx, stmt->if_stmt.else_body[i], instance_id);
                }
            }
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// UNIVERSAL LOGIC EXECUTION
// ============================================================================

bool ir_executor_run_universal(IRExecutorContext* ctx, IRLogicFunction* func, uint32_t instance_id) {
    if (!ctx || !func || !func->has_universal) return false;

    printf("[executor] Running universal logic for instance %u\n", instance_id);

    for (int i = 0; i < func->statement_count; i++) {
        exec_stmt(ctx, func->statements[i], instance_id);
    }

    return true;
}

// ============================================================================
// UI UPDATE
// ============================================================================

static void update_text_recursive(IRExecutorContext* ctx, IRComponent* comp, uint32_t instance_id) {
    if (!comp) return;

    // Debug: log component type
    if (comp->type == IR_COMPONENT_TEXT) {
        printf("[executor] Found Text component id=%u, owner=%u, text_expression=%s\n",
               comp->id, comp->owner_instance_id,
               comp->text_expression ? comp->text_expression : "(null)");
    }

    // Check if this is a Text component with a text_expression (reactive text)
    // Only update if it belongs to the same instance
    if (comp->type == IR_COMPONENT_TEXT && comp->text_expression &&
        comp->owner_instance_id == instance_id) {
        const char* expr = comp->text_expression;
        const char* start = strstr(expr, "{{");
        if (start) {
            const char* end = strstr(start, "}}");
            if (end) {
                // Extract variable name
                int nameLen = end - start - 2;
                if (nameLen > 0 && nameLen < 64) {
                    char varName[65];
                    strncpy(varName, start + 2, nameLen);
                    varName[nameLen] = '\0';

                    // Get value and update text
                    int64_t value = ir_executor_get_var(ctx, varName, instance_id);

                    // Build new text from expression template
                    char newText[256];
                    int prefixLen = start - expr;
                    strncpy(newText, expr, prefixLen);
                    newText[prefixLen] = '\0';

                    char valueStr[32];
                    snprintf(valueStr, sizeof(valueStr), "%ld", (long)value);
                    strcat(newText, valueStr);
                    strcat(newText, end + 2);

                    // Update the component's text_content (display text)
                    if (comp->text_content) {
                        free(comp->text_content);
                    }
                    comp->text_content = strdup(newText);
                    printf("[executor] Updated text: %s -> %s\n", expr, newText);
                }
            }
        }
    }

    // Recurse to children
    for (int i = 0; i < comp->child_count; i++) {
        update_text_recursive(ctx, comp->children[i], instance_id);
    }
}

void ir_executor_update_text_components(IRExecutorContext* ctx) {
    if (!ctx) {
        printf("[executor] update_text: ctx is NULL\n");
        return;
    }
    if (!ctx->root) {
        printf("[executor] update_text: ctx->root is NULL\n");
        return;
    }
    printf("[executor] update_text: starting with root id=%u, instance=%u\n",
           ctx->root->id, ctx->current_instance_id);

    // For now, update all text with instance 0 (simple case)
    // TODO: Track which instance owns which subtree
    update_text_recursive(ctx, ctx->root, ctx->current_instance_id);
}
