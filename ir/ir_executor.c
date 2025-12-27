#define _POSIX_C_SOURCE 200809L
#include "ir_executor.h"
#include "ir_expression.h"
#include "ir_core.h"
#include "ir_builder.h"
#include "ir_serialization.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations
static void ir_executor_update_conditionals(IRExecutorContext* ctx);

// ============================================================================
// VALUE HELPER FUNCTIONS
// ============================================================================

// Create an integer value
static IRValue ir_value_int(int64_t val) {
    IRValue v = {0};
    v.type = VAR_TYPE_INT;
    v.int_val = val;
    return v;
}

// Create a string value (takes ownership of string via strdup)
static IRValue ir_value_string(const char* str) {
    IRValue v = {0};
    v.type = VAR_TYPE_STRING;
    v.string_val = str ? strdup(str) : NULL;
    return v;
}

// Create an empty array value
static IRValue ir_value_array_empty(void) {
    IRValue v = {0};
    v.type = VAR_TYPE_ARRAY;
    v.array_val.items = (IRValue*)calloc(8, sizeof(IRValue));  // Initial capacity: 8
    v.array_val.count = 0;
    v.array_val.capacity = 8;
    return v;
}

// Create a null value
static IRValue ir_value_null(void) {
    IRValue v = {0};
    v.type = VAR_TYPE_NULL;
    return v;
}

// Free an IRValue and its contents (recursive for arrays)
static void ir_value_free(IRValue* val) {
    if (!val) return;

    if (val->type == VAR_TYPE_STRING && val->string_val) {
        free(val->string_val);
        val->string_val = NULL;
    } else if (val->type == VAR_TYPE_ARRAY && val->array_val.items) {
        // Recursively free array elements
        for (int i = 0; i < val->array_val.count; i++) {
            ir_value_free(&val->array_val.items[i]);
        }
        free(val->array_val.items);
        val->array_val.items = NULL;
    }
}

// Deep copy an IRValue
static IRValue ir_value_copy(const IRValue* src) {
    if (!src) return ir_value_null();

    IRValue dst = {0};
    dst.type = src->type;

    switch (src->type) {
        case VAR_TYPE_INT:
            dst.int_val = src->int_val;
            break;

        case VAR_TYPE_STRING:
            dst.string_val = src->string_val ? strdup(src->string_val) : NULL;
            break;

        case VAR_TYPE_ARRAY:
            dst.array_val.capacity = src->array_val.capacity;
            dst.array_val.count = src->array_val.count;
            dst.array_val.items = (IRValue*)calloc(dst.array_val.capacity, sizeof(IRValue));
            for (int i = 0; i < src->array_val.count; i++) {
                dst.array_val.items[i] = ir_value_copy(&src->array_val.items[i]);
            }
            break;

        case VAR_TYPE_NULL:
        default:
            break;
    }

    return dst;
}

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
// BUILT-IN ARRAY FUNCTIONS
// ============================================================================

// array_add(arr, value) - Append element to array
static IRValue builtin_array_add(IRExecutorContext* ctx, IRValue* args, int arg_count, uint32_t instance_id) {
    (void)ctx;  // Unused
    (void)instance_id;  // Unused

    if (arg_count != 2) {
        printf("[executor] array_add: expected 2 arguments, got %d\n", arg_count);
        return ir_value_null();
    }

    if (args[0].type != VAR_TYPE_ARRAY) {
        printf("[executor] array_add: first argument must be an array (got type %d)\n", args[0].type);
        return ir_value_null();
    }

    // Debug output
    printf("[executor] array_add: Adding element to array (current count=%d, capacity=%d)\n",
           args[0].array_val.count, args[0].array_val.capacity);

    if (args[1].type == VAR_TYPE_STRING) {
        // Reject empty strings
        if (!args[1].string_val || strlen(args[1].string_val) == 0) {
            printf("[executor] array_add: Skipping empty string\n");
            return ir_value_null();
        }
        printf("[executor] array_add: Adding string value: '%s'\n", args[1].string_val);
    } else if (args[1].type == VAR_TYPE_INT) {
        printf("[executor] array_add: Adding int value: %ld\n", (long)args[1].int_val);
    }

    // Resize if needed
    if (args[0].array_val.count >= args[0].array_val.capacity) {
        args[0].array_val.capacity *= 2;
        args[0].array_val.items = (IRValue*)realloc(args[0].array_val.items,
            args[0].array_val.capacity * sizeof(IRValue));
        printf("[executor] array_add: Resized array to capacity %d\n", args[0].array_val.capacity);
    }

    // Append value (deep copy)
    args[0].array_val.items[args[0].array_val.count++] = ir_value_copy(&args[1]);
    printf("[executor] array_add: Array now has %d elements\n", args[0].array_val.count);

    return ir_value_null();  // Void return
}

// array_remove(arr, index) - Remove element at index
static IRValue builtin_array_remove(IRExecutorContext* ctx, IRValue* args, int arg_count, uint32_t instance_id) {
    (void)ctx;  // Unused
    (void)instance_id;  // Unused

    if (arg_count != 2) {
        printf("[executor] array_remove: expected 2 arguments, got %d\n", arg_count);
        return ir_value_null();
    }

    if (args[0].type != VAR_TYPE_ARRAY || args[1].type != VAR_TYPE_INT) {
        printf("[executor] array_remove: invalid arguments\n");
        return ir_value_null();
    }

    int index = (int)args[1].int_val;
    if (index < 0 || index >= args[0].array_val.count) {
        printf("[executor] array_remove: index %d out of bounds\n", index);
        return ir_value_null();
    }

    // Free the value being removed
    ir_value_free(&args[0].array_val.items[index]);

    // Shift remaining elements
    for (int i = index; i < args[0].array_val.count - 1; i++) {
        args[0].array_val.items[i] = args[0].array_val.items[i + 1];
    }
    args[0].array_val.count--;

    return ir_value_null();
}

// array_length(arr) - Get array size
static IRValue builtin_array_length(IRExecutorContext* ctx, IRValue* args, int arg_count, uint32_t instance_id) {
    (void)ctx;  // Unused
    (void)instance_id;  // Unused

    if (arg_count != 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_int(0);
    }

    return ir_value_int(args[0].array_val.count);
}

// array_clear(arr) - Remove all elements
static IRValue builtin_array_clear(IRExecutorContext* ctx, IRValue* args, int arg_count, uint32_t instance_id) {
    (void)ctx;  // Unused
    (void)instance_id;  // Unused

    if (arg_count != 1 || args[0].type != VAR_TYPE_ARRAY) {
        return ir_value_null();
    }

    // Free all elements
    for (int i = 0; i < args[0].array_val.count; i++) {
        ir_value_free(&args[0].array_val.items[i]);
    }
    args[0].array_val.count = 0;

    return ir_value_null();
}

// ============================================================================
// BUILT-IN FUNCTION REGISTRY
// ============================================================================

static void ir_executor_register_builtin(IRExecutorContext* ctx, const char* name, IRBuiltinFunc handler) {
    if (!ctx || !name || !handler) return;

    // Resize if needed
    if (ctx->builtin_count >= ctx->builtin_capacity) {
        ctx->builtin_capacity = ctx->builtin_capacity == 0 ? 16 : ctx->builtin_capacity * 2;
        ctx->builtins = (IRBuiltinEntry*)realloc(ctx->builtins, ctx->builtin_capacity * sizeof(IRBuiltinEntry));
    }

    ctx->builtins[ctx->builtin_count].name = strdup(name);
    ctx->builtins[ctx->builtin_count].handler = handler;
    ctx->builtin_count++;
}

static IRBuiltinFunc ir_executor_find_builtin(IRExecutorContext* ctx, const char* name) {
    if (!ctx || !name) return NULL;

    for (int i = 0; i < ctx->builtin_count; i++) {
        if (strcmp(ctx->builtins[i].name, name) == 0) {
            return ctx->builtins[i].handler;
        }
    }

    return NULL;
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

    // Initialize builtin function registry
    ctx->builtins = NULL;
    ctx->builtin_count = 0;
    ctx->builtin_capacity = 0;

    // Register built-in array functions
    ir_executor_register_builtin(ctx, "add", builtin_array_add);
    ir_executor_register_builtin(ctx, "remove", builtin_array_remove);
    ir_executor_register_builtin(ctx, "length", builtin_array_length);
    ir_executor_register_builtin(ctx, "clear", builtin_array_clear);

    return ctx;
}

void ir_executor_destroy(IRExecutorContext* ctx) {
    if (!ctx) return;

    // Free source code strings
    for (int i = 0; i < ctx->source_count; i++) {
        free(ctx->sources[i].lang);
        free(ctx->sources[i].code);
    }

    // Free variable values (including strings and arrays)
    for (int i = 0; i < ctx->var_count; i++) {
        free(ctx->vars[i].name);
        ir_value_free(&ctx->vars[i].value);
    }

    // Free builtin function registry
    for (int i = 0; i < ctx->builtin_count; i++) {
        free(ctx->builtins[i].name);
    }
    free(ctx->builtins);

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
                    ir_executor_set_var_int(ctx, varName, value, comp->owner_instance_id);
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

// ============================================================================
// FOR-LOOP RENDERING
// ============================================================================

// Forward declarations
static IRExecutorVar* ir_executor_find_var(IRExecutorContext* ctx, const char* name, uint32_t instance_id);
static IRValue eval_expr_value(IRExecutorContext* ctx, IRExpression* expr, uint32_t instance_id);
static void update_text_recursive(IRExecutorContext* ctx, IRComponent* comp, uint32_t instance_id);

// Recursively reassign unique IDs to all components in a tree
static void reassign_component_ids(IRComponent* comp) {
    if (!comp || !g_ir_context) return;

    // Assign new unique ID
    uint32_t old_id = comp->id;
    comp->id = g_ir_context->next_component_id++;

    printf("[executor]       Reassigned ID: %u -> %u (type=%d)\n", old_id, comp->id, comp->type);

    // Recursively reassign children
    for (uint32_t i = 0; i < comp->child_count; i++) {
        if (comp->children[i]) {
            reassign_component_ids(comp->children[i]);
        }
    }
}

static void ir_executor_render_for_loops(IRExecutorContext* ctx) {
    if (!ctx || !ctx->root) return;
    if (ctx->for_loop_count == 0) return;

    printf("[executor] Rendering %d for-loop(s)\n", ctx->for_loop_count);
    printf("[executor] Root component ID: %u, type: %d\n", ctx->root->id, ctx->root->type);

    for (int i = 0; i < ctx->for_loop_count; i++) {
        IRReactiveForLoop* loop = &ctx->for_loops[i];

        // Get collection array variable
        IRExecutorVar* array_var = ir_executor_find_var(ctx, loop->collection_expr, 0);
        if (!array_var || array_var->value.type != VAR_TYPE_ARRAY) {
            printf("[executor] ERROR: For-loop collection '%s' is not an array\n",
                   loop->collection_expr ? loop->collection_expr : "null");
            continue;
        }

        // Find parent container
        printf("[executor]   Looking for parent #%u starting from root #%u (child_count=%u)\n",
               loop->parent_component_id, ctx->root->id, ctx->root->child_count);

        // Debug: Print first level of children
        if (ctx->root->child_count > 0 && ctx->root->children) {
            printf("[executor]   Root children: ");
            for (uint32_t j = 0; j < ctx->root->child_count; j++) {
                if (ctx->root->children[j]) {
                    printf("#%u(%u children) ", ctx->root->children[j]->id, ctx->root->children[j]->child_count);
                }
            }
            printf("\n");
        }

        IRComponent* parent = ir_find_component_by_id(ctx->root, loop->parent_component_id);
        if (!parent) {
            printf("[executor] ERROR: For-loop parent #%u not found\n",
                   loop->parent_component_id);
            printf("[executor]   Tree traversal failed - component may not be in tree\n");
            continue;
        }
        printf("[executor]   Found parent #%u\n", parent->id);

        int item_count = array_var->value.array_val.count;
        printf("[executor]   Rendering loop #%d: %d items in '%s', parent #%u\n",
               i, item_count, loop->collection_expr, loop->parent_component_id);

        // Clear existing children (temporarily skip destruction to avoid crash)
        // TODO: Need to properly clean up old components without corrupting memory
        parent->child_count = 0;
        parent->child_capacity = 0;
        if (parent->children) {
            free(parent->children);
            parent->children = NULL;
        }

        // Debug: Log array state before rendering
        printf("[executor]   Array state: ptr=%p, count=%d, capacity=%d\n",
               array_var->value.array_val.items,
               array_var->value.array_val.count,
               array_var->value.array_val.capacity);

        // Render each array item
        for (int j = 0; j < item_count; j++) {
            // Deserialize template for each item
            if (!loop->item_template) {
                printf("[executor] ERROR: Loop has no item template\n");
                continue;
            }

            IRComponent* instance = ir_deserialize_json(loop->item_template);
            if (!instance) {
                printf("[executor] ERROR: Failed to deserialize template instance\n");
                continue;
            }

            // Reassign unique IDs to avoid conflicts (template IDs are reused)
            printf("[executor]     Reassigning IDs for instance %d:\n", j);
            reassign_component_ids(instance);

            // Set loop variable scoped to this component instance
            // This allows text expressions like {"var": "todo"} to resolve
            if (loop->loop_variable_name) {
                printf("[executor]   About to access array[%d], array ptr=%p\n", j, array_var->value.array_val.items);
                IRValue item_val = array_var->value.array_val.items[j];
                printf("[executor]   Setting var '%s' (instance %u) from array[%d]: type=%d",
                       loop->loop_variable_name, instance->id, j, item_val.type);
                if (item_val.type == VAR_TYPE_STRING) {
                    printf(", value='%s'\n", item_val.string_val ? item_val.string_val : "(null)");
                } else if (item_val.type == VAR_TYPE_INT) {
                    printf(", value=%ld\n", (long)item_val.int_val);
                } else {
                    printf(", value=(unknown type)\n");
                }

                ir_executor_set_var(ctx, loop->loop_variable_name,
                                   item_val,
                                   instance->id);
            }

            // Add to parent
            ir_add_child(parent, instance);

            // Evaluate text expressions using the loop variable
            update_text_recursive(ctx, instance, instance->id);

            printf("[executor]     Created instance #%u for item %d\n", instance->id, j);
        }

        // Update child_component_ids tracking
        loop->child_count = parent->child_count;
        free(loop->child_component_ids);
        loop->child_component_ids = NULL;
        if (loop->child_count > 0) {
            loop->child_component_ids = (uint32_t*)malloc(loop->child_count * sizeof(uint32_t));
            for (uint32_t j = 0; j < loop->child_count; j++) {
                loop->child_component_ids[j] = parent->children[j]->id;
            }
        }

        // Mark parent for layout recalculation after modifying children
        if (parent) {
            ir_layout_mark_dirty(parent);
        }
    }
}

// Initialize Input component values from bound variables
static void init_input_bindings_recursive(IRExecutorContext* ctx, IRComponent* comp) {
    if (!comp) return;

    // Check if this is an Input component with a value binding
    if (comp->type == IR_COMPONENT_INPUT && comp->text_expression) {
        const char* expr = comp->text_expression;

        // Parse {{varname}} format
        if (strncmp(expr, "{{", 2) == 0 && expr[strlen(expr)-2] == '}' && expr[strlen(expr)-1] == '}') {
            // Extract variable name
            char var_name[256];
            size_t len = strlen(expr) - 4;  // Remove {{ and }}
            if (len < sizeof(var_name)) {
                strncpy(var_name, expr + 2, len);
                var_name[len] = '\0';

                // Look up the variable
                IRExecutorVar* var = ir_executor_find_var(ctx, var_name, 0);
                if (var && var->value.type == VAR_TYPE_STRING) {
                    // Set Input text from variable
                    if (comp->text_content) free(comp->text_content);
                    comp->text_content = strdup(var->value.string_val ? var->value.string_val : "");
                    printf("[executor] Initialized Input #%u from var '%s' = '%s'\n",
                           comp->id, var_name, comp->text_content);
                }
            }
        }
    }

    // Recurse to children
    for (uint32_t i = 0; i < comp->child_count; i++) {
        init_input_bindings_recursive(ctx, comp->children[i]);
    }
}

// Update Input components when their bound variables change
static void update_input_bindings_recursive(IRExecutorContext* ctx, IRComponent* comp) {
    if (!comp) return;

    // Check if this is an Input component with a value binding
    if (comp->type == IR_COMPONENT_INPUT && comp->text_expression) {
        const char* expr = comp->text_expression;

        // Parse {{varname}} format
        if (strncmp(expr, "{{", 2) == 0 && expr[strlen(expr)-2] == '}' && expr[strlen(expr)-1] == '}') {
            // Extract variable name
            char var_name[256];
            size_t len = strlen(expr) - 4;  // Remove {{ and }}
            if (len < sizeof(var_name)) {
                strncpy(var_name, expr + 2, len);
                var_name[len] = '\0';

                // Look up the variable
                IRExecutorVar* var = ir_executor_find_var(ctx, var_name, 0);
                if (var && var->value.type == VAR_TYPE_STRING) {
                    // Update Input text from variable
                    if (comp->text_content) free(comp->text_content);
                    comp->text_content = strdup(var->value.string_val ? var->value.string_val : "");
                    printf("[executor] Updated Input #%u from var '%s' = '%s'\n",
                           comp->id, var_name, comp->text_content);
                }
            }
        }
    }

    // Recurse to children
    for (uint32_t i = 0; i < comp->child_count; i++) {
        update_input_bindings_recursive(ctx, comp->children[i]);
    }
}

// Sync Input component text back to bound variable
// Called by backend when Input text changes
void ir_executor_sync_input_to_var(IRExecutorContext* ctx, IRComponent* input_comp) {
    if (!ctx || !input_comp || input_comp->type != IR_COMPONENT_INPUT) return;
    if (!input_comp->text_expression) return;

    const char* expr = input_comp->text_expression;

    // Parse {{varname}} format
    if (strncmp(expr, "{{", 2) == 0 && expr[strlen(expr)-2] == '}' && expr[strlen(expr)-1] == '}') {
        // Extract variable name
        char var_name[256];
        size_t len = strlen(expr) - 4;  // Remove {{ and }}
        if (len < sizeof(var_name)) {
            strncpy(var_name, expr + 2, len);
            var_name[len] = '\0';

            // Update the variable
            IRValue new_val = {
                .type = VAR_TYPE_STRING,
                .string_val = input_comp->text_content ? strdup(input_comp->text_content) : strdup("")
            };

            IRExecutorVar* var = ir_executor_find_var(ctx, var_name, 0);
            if (var) {
                // Free old value
                if (var->value.type == VAR_TYPE_STRING && var->value.string_val) {
                    free(var->value.string_val);
                }
                var->value = new_val;
                printf("[executor] Synced Input #%u to var '%s' = '%s'\n",
                       input_comp->id, var_name, new_val.string_val);

                // CRITICAL: Update all text components that reference this variable
                // This triggers layout invalidation for reactive text expressions
                ir_executor_update_text_components(ctx);
            }
        }
    }
}

void ir_executor_apply_initial_conditionals(IRExecutorContext* ctx) {
    if (!ctx || !ctx->root) return;
    printf("[executor] Applying initial conditionals (%d total)\n", g_conditional_count);
    ir_executor_update_conditionals(ctx);

    // Render for-loops after conditionals
    ir_executor_render_for_loops(ctx);

    // Initialize Input value bindings
    init_input_bindings_recursive(ctx, ctx->root);
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
                update_input_bindings_recursive(ctx, ctx->root);
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
                cJSON* typeJ = cJSON_GetObjectItem(varObj, "type");
                cJSON* initialJ = cJSON_GetObjectItem(varObj, "initial_value");

                if (nameJ && cJSON_IsString(nameJ)) {
                    const char* name = nameJ->valuestring;
                    const char* type_str = typeJ && cJSON_IsString(typeJ) ? typeJ->valuestring : "int";

                    IRValue value;

                    // Parse based on type
                    if (strcmp(type_str, "array") == 0) {
                        // Parse array variable
                        value = ir_value_array_empty();

                        cJSON* array_json = NULL;
                        bool should_delete = false;

                        if (initialJ) {
                            if (cJSON_IsArray(initialJ)) {
                                array_json = initialJ;
                            } else if (cJSON_IsString(initialJ)) {
                                // Parse stringified JSON array
                                const char* json_str = initialJ->valuestring;
                                array_json = cJSON_Parse(json_str);
                                should_delete = true;
                            }
                        }

                        if (array_json && cJSON_IsArray(array_json)) {
                            int count = cJSON_GetArraySize(array_json);
                            for (int i = 0; i < count; i++) {
                                cJSON* elem = cJSON_GetArrayItem(array_json, i);
                                IRValue elem_val;

                                if (cJSON_IsNumber(elem)) {
                                    elem_val = ir_value_int((int64_t)elem->valuedouble);
                                } else if (cJSON_IsString(elem)) {
                                    elem_val = ir_value_string(elem->valuestring);
                                } else if (cJSON_IsBool(elem)) {
                                    elem_val = ir_value_int(cJSON_IsTrue(elem) ? 1 : 0);
                                } else {
                                    continue;  // Skip unsupported types
                                }

                                // Resize if needed
                                if (value.array_val.count >= value.array_val.capacity) {
                                    value.array_val.capacity *= 2;
                                    value.array_val.items = (IRValue*)realloc(value.array_val.items,
                                        value.array_val.capacity * sizeof(IRValue));
                                }

                                value.array_val.items[value.array_val.count++] = elem_val;
                            }
                        }

                        if (should_delete && array_json) {
                            cJSON_Delete(array_json);
                        }

                        printf("[executor] Initialized array var %s with %d elements from manifest\n",
                               name, value.array_val.count);

                    } else if (strcmp(type_str, "string") == 0) {
                        // Parse string variable
                        const char* str_val = initialJ && cJSON_IsString(initialJ) ? initialJ->valuestring : "";
                        value = ir_value_string(str_val);
                        printf("[executor] Initialized string var %s = \"%s\" from manifest\n", name, str_val);

                    } else {
                        // Parse integer variable (default)
                        int64_t initial = 0;

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

                        value = ir_value_int(initial);
                        printf("[executor] Initialized int var %s = %ld from manifest\n", name, (long)initial);
                    }

                    // Don't call ir_executor_set_var here as it triggers update_conditionals
                    // and we haven't parsed conditionals yet. Directly add the variable.
                    if (ctx->var_count < IR_EXECUTOR_MAX_VARS) {
                        ctx->vars[ctx->var_count].name = strdup(name);
                        ctx->vars[ctx->var_count].value = value;
                        ctx->vars[ctx->var_count].owner_component_id = 0;  // global
                        ctx->var_count++;
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
                    // Check if condition is a JSON string (Nim format) or object (.kry format)
                    if (cJSON_IsString(condExpr)) {
                        // Nim exports condition as JSON string: "{\"var\":\"showMessage\"}"
                        // Parse it first, then convert to expression
                        cJSON* parsedCondition = cJSON_Parse(condExpr->valuestring);
                        if (parsedCondition) {
                            cond->condition = ir_expr_from_json(parsedCondition);
                            cJSON_Delete(parsedCondition);  // Free parsed JSON
                        } else {
                            printf("[executor] Failed to parse condition JSON string: %s\n", condExpr->valuestring);
                        }
                    } else {
                        // .kry format: condition is already a JSON object
                        cond->condition = ir_expr_from_json(condExpr);
                    }

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

        // Parse foreach_loops from manifest
        printf("[executor] Looking for foreach_loops in manifest...\n");
        cJSON* foreach_array = cJSON_GetObjectItem(manifest, "foreach_loops");
        printf("[executor] foreach_array = %p, isArray = %d\n", (void*)foreach_array,
               foreach_array ? cJSON_IsArray(foreach_array) : 0);
        if (foreach_array && cJSON_IsArray(foreach_array)) {
            int count = cJSON_GetArraySize(foreach_array);
            ctx->for_loop_count = count;
            ctx->for_loops = (IRReactiveForLoop*)calloc(count, sizeof(IRReactiveForLoop));

            printf("[executor] Parsing %d for-loop(s) from manifest\n", count);

            for (int i = 0; i < count; i++) {
                cJSON* loop_json = cJSON_GetArrayItem(foreach_array, i);
                IRReactiveForLoop* loop = &ctx->for_loops[i];

                // Initialize
                memset(loop, 0, sizeof(IRReactiveForLoop));

                // Parse parent_id
                cJSON* parent_id = cJSON_GetObjectItem(loop_json, "parent_id");
                loop->parent_component_id = parent_id ? (uint32_t)parent_id->valueint : 0;

                // Parse iterable (collection expression)
                cJSON* iterable = cJSON_GetObjectItem(loop_json, "iterable");
                if (iterable) {
                    // Assume simple {"var": "varname"} format
                    cJSON* var_obj = cJSON_GetObjectItem(iterable, "var");
                    if (var_obj && var_obj->valuestring) {
                        loop->collection_expr = strdup(var_obj->valuestring);
                    }
                }

                // Parse loop variable name
                cJSON* variable = cJSON_GetObjectItem(loop_json, "variable");
                if (variable && variable->valuestring) {
                    loop->loop_variable_name = strdup(variable->valuestring);
                }

                // Parse template (store as JSON string)
                cJSON* template_json = cJSON_GetObjectItem(loop_json, "template");
                if (template_json) {
                    char* template_str = cJSON_PrintUnformatted(template_json);
                    loop->item_template = template_str;
                }

                printf("[executor]   Loop %d: parent=%u, collection=%s, var=%s\n",
                       i, loop->parent_component_id,
                       loop->collection_expr ? loop->collection_expr : "null",
                       loop->loop_variable_name ? loop->loop_variable_name : "null");
            }
        } else {
            ctx->for_loop_count = 0;
            ctx->for_loops = NULL;
        }
    }

    cJSON_Delete(root);
    return true;
}

// ============================================================================
// VARIABLE MANAGEMENT
// ============================================================================

// Find a variable pointer by name and instance ID
static IRExecutorVar* ir_executor_find_var(IRExecutorContext* ctx, const char* name, uint32_t instance_id) {
    if (!ctx || !name) return NULL;

    // Check instance-specific vars first
    for (int i = 0; i < ctx->var_count; i++) {
        if (ctx->vars[i].owner_component_id == instance_id &&
            ctx->vars[i].name && strcmp(ctx->vars[i].name, name) == 0) {
            return &ctx->vars[i];
        }
    }

    // Check global vars (instance_id == 0)
    for (int i = 0; i < ctx->var_count; i++) {
        if (ctx->vars[i].owner_component_id == 0 &&
            ctx->vars[i].name && strcmp(ctx->vars[i].name, name) == 0) {
            return &ctx->vars[i];
        }
    }

    return NULL;
}

// Get a variable's value (returns copy, null if not found)
IRValue ir_executor_get_var(IRExecutorContext* ctx, const char* name, uint32_t instance_id) {
    IRExecutorVar* var = ir_executor_find_var(ctx, name, instance_id);
    return var ? ir_value_copy(&var->value) : ir_value_null();
}

// Set a variable's value (creates if doesn't exist)
void ir_executor_set_var(IRExecutorContext* ctx, const char* name, IRValue value, uint32_t instance_id) {
    if (!ctx || !name) return;

    IRExecutorVar* var = ir_executor_find_var(ctx, name, instance_id);

    if (var) {
        // Update existing variable
        ir_value_free(&var->value);
        var->value = ir_value_copy(&value);
        printf("[executor] Set var %s (instance %u)\n", name, instance_id);
    } else {
        // Create new variable
        if (ctx->var_count >= IR_EXECUTOR_MAX_VARS) {
            printf("[executor] ERROR: Variable limit reached\n");
            return;
        }

        ctx->vars[ctx->var_count].name = strdup(name);
        ctx->vars[ctx->var_count].value = ir_value_copy(&value);
        ctx->vars[ctx->var_count].owner_component_id = instance_id;
        ctx->var_count++;
        printf("[executor] Created var %s (instance %u)\n", name, instance_id);
    }

    // Trigger conditional updates for reactive UI
    ir_executor_update_conditionals(ctx);
}

// Backwards-compatible integer getter
int64_t ir_executor_get_var_int(IRExecutorContext* ctx, const char* name, uint32_t instance_id) {
    IRExecutorVar* var = ir_executor_find_var(ctx, name, instance_id);
    if (var && var->value.type == VAR_TYPE_INT) {
        return var->value.int_val;
    }
    return 0;
}

// Backwards-compatible integer setter
void ir_executor_set_var_int(IRExecutorContext* ctx, const char* name, int64_t val, uint32_t instance_id) {
    IRValue value = ir_value_int(val);
    ir_executor_set_var(ctx, name, value, instance_id);
    ir_value_free(&value);
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

// Evaluate expression and return IRValue (supports all types)
static IRValue eval_expr_value(IRExecutorContext* ctx, IRExpression* expr, uint32_t instance_id) {
    if (!expr) return ir_value_null();

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            return ir_value_int(expr->int_value);

        case EXPR_LITERAL_BOOL:
            return ir_value_int(expr->bool_value ? 1 : 0);

        case EXPR_LITERAL_STRING:
            return ir_value_string(expr->string_value);

        case EXPR_VAR_REF:
            return ir_executor_get_var(ctx, expr->var_ref.name, instance_id);

        case EXPR_BINARY: {
            IRValue left = eval_expr_value(ctx, expr->binary.left, instance_id);
            IRValue right = eval_expr_value(ctx, expr->binary.right, instance_id);

            // Only support int operations for now
            if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
                int64_t result = 0;
                switch (expr->binary.op) {
                    case BINARY_OP_ADD: result = left.int_val + right.int_val; break;
                    case BINARY_OP_SUB: result = left.int_val - right.int_val; break;
                    case BINARY_OP_MUL: result = left.int_val * right.int_val; break;
                    case BINARY_OP_DIV: result = right.int_val != 0 ? left.int_val / right.int_val : 0; break;
                    case BINARY_OP_MOD: result = right.int_val != 0 ? left.int_val % right.int_val : 0; break;
                    case BINARY_OP_EQ:  result = left.int_val == right.int_val; break;
                    case BINARY_OP_NEQ: result = left.int_val != right.int_val; break;
                    case BINARY_OP_LT:  result = left.int_val < right.int_val; break;
                    case BINARY_OP_LTE: result = left.int_val <= right.int_val; break;
                    case BINARY_OP_GT:  result = left.int_val > right.int_val; break;
                    case BINARY_OP_GTE: result = left.int_val >= right.int_val; break;
                    case BINARY_OP_AND: result = left.int_val && right.int_val; break;
                    case BINARY_OP_OR:  result = left.int_val || right.int_val; break;
                    default: result = 0; break;
                }
                ir_value_free(&left);
                ir_value_free(&right);
                return ir_value_int(result);
            }

            ir_value_free(&left);
            ir_value_free(&right);
            return ir_value_null();
        }

        case EXPR_UNARY: {
            IRValue operand = eval_expr_value(ctx, expr->unary.operand, instance_id);
            if (operand.type == VAR_TYPE_INT) {
                int64_t result = 0;
                switch (expr->unary.op) {
                    case UNARY_OP_NEG: result = -operand.int_val; break;
                    case UNARY_OP_NOT: result = !operand.int_val; break;
                    default: result = 0; break;
                }
                ir_value_free(&operand);
                return ir_value_int(result);
            }
            ir_value_free(&operand);
            return ir_value_null();
        }

        case EXPR_INDEX: {
            // Array indexing: arr[index]
            IRValue arr = eval_expr_value(ctx, expr->index_access.array, instance_id);
            IRValue idx = eval_expr_value(ctx, expr->index_access.index, instance_id);

            if (arr.type == VAR_TYPE_ARRAY && idx.type == VAR_TYPE_INT) {
                int index = (int)idx.int_val;
                if (index >= 0 && index < arr.array_val.count) {
                    IRValue result = ir_value_copy(&arr.array_val.items[index]);
                    ir_value_free(&arr);
                    ir_value_free(&idx);
                    return result;
                }
            }

            ir_value_free(&arr);
            ir_value_free(&idx);
            return ir_value_null();
        }

        case EXPR_CALL: {
            // Function call that returns a value (e.g., array.length())
            IRBuiltinFunc func = ir_executor_find_builtin(ctx, expr->call.function);
            if (!func) {
                printf("[executor] ERROR: Unknown function '%s'\n", expr->call.function);
                return ir_value_null();
            }

            // Evaluate arguments
            IRValue* args = (IRValue*)calloc(expr->call.arg_count, sizeof(IRValue));
            for (int i = 0; i < expr->call.arg_count; i++) {
                args[i] = eval_expr_value(ctx, expr->call.args[i], instance_id);
            }

            // Call function and get result
            IRValue result = func(ctx, args, expr->call.arg_count, instance_id);

            // Free arguments
            for (int i = 0; i < expr->call.arg_count; i++) {
                ir_value_free(&args[i]);
            }
            free(args);

            return result;
        }

        default:
            return ir_value_null();
    }
}

// Legacy int-only evaluator (for backwards compatibility with conditionals)
static int64_t eval_expr(IRExecutorContext* ctx, IRExpression* expr, uint32_t instance_id) {
    if (!expr) return 0;

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            return expr->int_value;

        case EXPR_LITERAL_BOOL:
            return expr->bool_value ? 1 : 0;

        case EXPR_VAR_REF:
            return ir_executor_get_var_int(ctx, expr->var_ref.name, instance_id);

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
            // Check if expression is a string literal
            if (stmt->assign.expr && stmt->assign.expr->type == EXPR_LITERAL_STRING) {
                // Create string IRValue
                IRValue str_val = {
                    .type = VAR_TYPE_STRING,
                    .string_val = strdup(stmt->assign.expr->string_value ? stmt->assign.expr->string_value : "")
                };
                ir_executor_set_var(ctx, stmt->assign.target, str_val, instance_id);
                printf("[executor] Assigned string '%s' to var '%s'\n",
                       str_val.string_val, stmt->assign.target);
            } else {
                // Integer expression
                int64_t value = eval_expr(ctx, stmt->assign.expr, instance_id);
                ir_executor_set_var_int(ctx, stmt->assign.target, value, instance_id);
            }
            break;
        }

        case STMT_ASSIGN_OP: {
            int64_t current = ir_executor_get_var_int(ctx, stmt->assign_op.target, instance_id);
            int64_t delta = eval_expr(ctx, stmt->assign_op.expr, instance_id);
            int64_t result = current;

            switch (stmt->assign_op.op) {
                case ASSIGN_OP_ADD: result = current + delta; break;
                case ASSIGN_OP_SUB: result = current - delta; break;
                case ASSIGN_OP_MUL: result = current * delta; break;
                case ASSIGN_OP_DIV: result = delta != 0 ? current / delta : 0; break;
            }

            ir_executor_set_var_int(ctx, stmt->assign_op.target, result, instance_id);
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

        case STMT_CALL: {
            printf("[executor] Executing function call: %s\n", stmt->call.function);

            // Look up built-in function
            IRBuiltinFunc func = ir_executor_find_builtin(ctx, stmt->call.function);
            if (!func) {
                printf("[executor] ERROR: Unknown function '%s'\n", stmt->call.function);
                break;
            }

            // Special handling for array modification functions
            // They need direct access to the variable, not a copy
            if (strcmp(stmt->call.function, "add") == 0 ||
                strcmp(stmt->call.function, "remove") == 0 ||
                strcmp(stmt->call.function, "clear") == 0) {

                // First arg should be a variable reference
                if (stmt->call.arg_count < 1 || stmt->call.args[0]->type != EXPR_VAR_REF) {
                    printf("[executor] ERROR: %s requires variable as first argument\n", stmt->call.function);
                    break;
                }

                const char* var_name = stmt->call.args[0]->var_ref.name;
                IRExecutorVar* var = ir_executor_find_var(ctx, var_name, instance_id);

                if (!var || var->value.type != VAR_TYPE_ARRAY) {
                    printf("[executor] ERROR: Variable '%s' is not an array\n", var_name);
                    break;
                }

                // Prepare arguments: [array_value, other_args...]
                IRValue* args = (IRValue*)calloc(stmt->call.arg_count, sizeof(IRValue));

                // Track array pointer BEFORE modification
                void* old_items_ptr = var->value.type == VAR_TYPE_ARRAY ? var->value.array_val.items : NULL;
                int old_count = var->value.type == VAR_TYPE_ARRAY ? var->value.array_val.count : 0;

                args[0] = var->value;  // Direct reference (no copy)

                for (int i = 1; i < stmt->call.arg_count; i++) {
                    args[i] = eval_expr_value(ctx, stmt->call.args[i], instance_id);
                }

                // Call function (modifies args[0] in place)
                IRValue result = func(ctx, args, stmt->call.arg_count, instance_id);
                ir_value_free(&result);

                // Track array pointer AFTER modification
                void* new_items_ptr = args[0].type == VAR_TYPE_ARRAY ? args[0].array_val.items : NULL;
                int new_count = args[0].type == VAR_TYPE_ARRAY ? args[0].array_val.count : 0;

                printf("[executor] Array modification: ptr %p->%p, count %d->%d, moved=%s\n",
                       old_items_ptr, new_items_ptr, old_count, new_count,
                       old_items_ptr != new_items_ptr ? "YES" : "NO");

                // Write modified array back to variable
                // NOTE: Don't free old value! args[0] is a shallow copy of var->value,
                // so they initially point to the same items array. The function may have
                // reallocated that array, making var->value.array_val.items stale.
                // Freeing it would cause double-free or use-after-free.
                var->value = args[0];               // Transfer ownership (don't copy!)
                printf("[executor] Updated variable '%s' after modification (new count=%d)\n",
                       var_name, var->value.type == VAR_TYPE_ARRAY ? var->value.array_val.count : 0);

                // Update conditionals for reactive updates
                ir_executor_update_conditionals(ctx);

                // Re-render for-loops (arrays may have changed)
                ir_executor_render_for_loops(ctx);

                // Free evaluated arguments (but not args[0] which is the variable)
                for (int i = 1; i < stmt->call.arg_count; i++) {
                    ir_value_free(&args[i]);
                }
                free(args);

            } else {
                // Regular function call (pure, returns value)
                IRValue* args = (IRValue*)calloc(stmt->call.arg_count, sizeof(IRValue));
                for (int i = 0; i < stmt->call.arg_count; i++) {
                    args[i] = eval_expr_value(ctx, stmt->call.args[i], instance_id);
                }

                IRValue result = func(ctx, args, stmt->call.arg_count, instance_id);
                ir_value_free(&result);

                for (int i = 0; i < stmt->call.arg_count; i++) {
                    ir_value_free(&args[i]);
                }
                free(args);
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
    if (comp->type == IR_COMPONENT_TEXT && comp->text_expression) {
        const char* expr = comp->text_expression;

        // Try to parse as JSON expression first (e.g., {"op":"add","left":"- ","right":{"var":"todo"}})
        if (expr[0] == '{') {
            cJSON* expr_json = cJSON_Parse(expr);
            if (expr_json) {
                cJSON* op = cJSON_GetObjectItem(expr_json, "op");
                if (op && cJSON_IsString(op) && strcmp(op->valuestring, "add") == 0) {
                    cJSON* left = cJSON_GetObjectItem(expr_json, "left");
                    cJSON* right = cJSON_GetObjectItem(expr_json, "right");

                    if (left && right) {
                        char result[256] = "";

                        // Handle left side (string literal or var)
                        if (cJSON_IsString(left)) {
                            strcat(result, left->valuestring);
                        }

                        // Handle right side (usually {"var":"name"})
                        if (cJSON_IsObject(right)) {
                            cJSON* var_ref = cJSON_GetObjectItem(right, "var");
                            if (var_ref && cJSON_IsString(var_ref)) {
                                // Look up the variable
                                // For loop variables, try component's own ID first
                                IRExecutorVar* var = ir_executor_find_var(ctx, var_ref->valuestring, comp->id);
                                if (!var) {
                                    // Fall back to parent's instance ID
                                    var = ir_executor_find_var(ctx, var_ref->valuestring, instance_id);
                                }
                                printf("[executor] Variable lookup: '%s' with comp_id=%u, instance_id=%u, found=%s\n",
                                       var_ref->valuestring, comp->id, instance_id, var ? "yes" : "no");
                                if (var) {
                                    if (var->value.type == VAR_TYPE_STRING && var->value.string_val) {
                                        strcat(result, var->value.string_val);
                                        printf("[executor]   -> STRING: '%s'\n", var->value.string_val);
                                    } else if (var->value.type == VAR_TYPE_INT) {
                                        char num[32];
                                        snprintf(num, sizeof(num), "%ld", (long)var->value.int_val);
                                        strcat(result, num);
                                        printf("[executor]   -> INT: %ld\n", (long)var->value.int_val);
                                    }
                                }
                            }
                        }

                        // Update text_content WITH layout invalidation
                        char* old_text = comp->text_content;
                        comp->text_content = strdup(result);

                        // CRITICAL: Invalidate layout if text changed
                        // This ensures the component resizes correctly
                        if (!old_text || strcmp(old_text, result) != 0) {
                            ir_layout_mark_dirty(comp);
                            ir_layout_mark_dirty(comp);
                            comp->dirty_flags |= IR_DIRTY_CONTENT | IR_DIRTY_LAYOUT;
                        }
                        if (old_text) {
                            free(old_text);
                        }
                        printf("[executor] Evaluated expression -> '%s'\n", result);
                    }
                }
                cJSON_Delete(expr_json);
            }
        }
        // Try {{variable}} format (old format)
        else {
            const char* start = strstr(expr, "{{");
            if (start && comp->owner_instance_id == instance_id) {
                const char* end = strstr(start, "}}");
                if (end) {
                    // Extract variable name
                    int nameLen = end - start - 2;
                    if (nameLen > 0 && nameLen < 64) {
                        char varName[65];
                        strncpy(varName, start + 2, nameLen);
                        varName[nameLen] = '\0';

                        // Get value and update text
                        int64_t value = ir_executor_get_var_int(ctx, varName, instance_id);

                        // Build new text from expression template
                        char newText[256];
                        int prefixLen = start - expr;
                        strncpy(newText, expr, prefixLen);
                        newText[prefixLen] = '\0';

                        char valueStr[32];
                        snprintf(valueStr, sizeof(valueStr), "%ld", (long)value);
                        strcat(newText, valueStr);
                        strcat(newText, end + 2);

                        // Update text_content WITH layout invalidation
                        char* old_text = comp->text_content;
                        comp->text_content = strdup(newText);

                        // CRITICAL: Invalidate layout if text changed
                        // This ensures the component resizes correctly
                        if (!old_text || strcmp(old_text, newText) != 0) {
                            ir_layout_mark_dirty(comp);
                            ir_layout_mark_dirty(comp);
                            comp->dirty_flags |= IR_DIRTY_CONTENT | IR_DIRTY_LAYOUT;
                        }
                        if (old_text) {
                            free(old_text);
                        }
                        printf("[executor] Updated text: %s -> %s\n", expr, newText);
                    }
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
