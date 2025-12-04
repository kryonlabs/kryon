#define _POSIX_C_SOURCE 200809L
#include "ir_executor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

void ir_executor_set_root(IRExecutorContext* ctx, IRComponent* root) {
    if (ctx) {
        ctx->root = root;
    }
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

bool ir_executor_handle_event(IRExecutorContext* ctx,
                               uint32_t component_id,
                               const char* event_type) {
    if (!ctx || !event_type) return false;

    printf("[executor] Event '%s' on component %u\n", event_type, component_id);

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

    cJSON_Delete(root);
    return true;
}
