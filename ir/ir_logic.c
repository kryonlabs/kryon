#define _POSIX_C_SOURCE 200809L
#include "ir_logic.h"
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
// CONSTRUCTOR FUNCTIONS
// ============================================================================

IRLogicFunction* ir_logic_function_create(const char* name) {
    IRLogicFunction* func = calloc(1, sizeof(IRLogicFunction));
    if (!func) return NULL;

    func->name = safe_strdup(name);
    func->has_universal = false;
    func->params = NULL;
    func->param_count = 0;
    func->statements = NULL;
    func->statement_count = 0;
    func->source_count = 0;

    return func;
}

void ir_logic_function_add_param(IRLogicFunction* func, const char* name, const char* type) {
    if (!func) return;

    func->params = realloc(func->params, (func->param_count + 1) * sizeof(IRLogicParam));
    if (!func->params) return;

    func->params[func->param_count].name = safe_strdup(name);
    func->params[func->param_count].type = safe_strdup(type);
    func->param_count++;
}

void ir_logic_function_add_stmt(IRLogicFunction* func, IRStatement* stmt) {
    if (!func || !stmt) return;

    func->statements = realloc(func->statements, (func->statement_count + 1) * sizeof(IRStatement*));
    if (!func->statements) return;

    func->statements[func->statement_count] = stmt;
    func->statement_count++;
    func->has_universal = true;
}

void ir_logic_function_add_source(IRLogicFunction* func, const char* language, const char* source) {
    if (!func || !language || !source) return;
    if (func->source_count >= IR_LOGIC_MAX_SOURCES) return;

    func->sources[func->source_count].language = safe_strdup(language);
    func->sources[func->source_count].source = safe_strdup(source);
    func->source_count++;
}

IREventBinding* ir_event_binding_create(uint32_t component_id, const char* event_type, const char* handler_name) {
    IREventBinding* binding = calloc(1, sizeof(IREventBinding));
    if (!binding) return NULL;

    binding->component_id = component_id;
    binding->event_type = safe_strdup(event_type);
    binding->handler_name = safe_strdup(handler_name);

    return binding;
}

IRLogicBlock* ir_logic_block_create(void) {
    IRLogicBlock* block = calloc(1, sizeof(IRLogicBlock));
    if (!block) return NULL;

    block->functions = NULL;
    block->function_count = 0;
    block->event_bindings = NULL;
    block->event_binding_count = 0;

    return block;
}

void ir_logic_block_add_function(IRLogicBlock* block, IRLogicFunction* func) {
    if (!block || !func) return;

    block->functions = realloc(block->functions, (block->function_count + 1) * sizeof(IRLogicFunction*));
    if (!block->functions) return;

    block->functions[block->function_count] = func;
    block->function_count++;
}

void ir_logic_block_add_binding(IRLogicBlock* block, IREventBinding* binding) {
    if (!block || !binding) return;

    block->event_bindings = realloc(block->event_bindings,
                                    (block->event_binding_count + 1) * sizeof(IREventBinding*));
    if (!block->event_bindings) return;

    block->event_bindings[block->event_binding_count] = binding;
    block->event_binding_count++;
}

int ir_logic_block_get_function_count(IRLogicBlock* block) {
    if (!block) return 0;
    return block->function_count;
}

int ir_logic_block_get_binding_count(IRLogicBlock* block) {
    if (!block) return 0;
    return block->event_binding_count;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void ir_logic_function_free(IRLogicFunction* func) {
    if (!func) return;

    free(func->name);

    for (int i = 0; i < func->param_count; i++) {
        free(func->params[i].name);
        free(func->params[i].type);
    }
    free(func->params);

    for (int i = 0; i < func->statement_count; i++) {
        ir_stmt_free(func->statements[i]);
    }
    free(func->statements);

    for (int i = 0; i < func->source_count; i++) {
        free(func->sources[i].language);
        free(func->sources[i].source);
    }

    free(func);
}

void ir_event_binding_free(IREventBinding* binding) {
    if (!binding) return;
    free(binding->event_type);
    free(binding->handler_name);
    free(binding);
}

void ir_logic_block_free(IRLogicBlock* block) {
    if (!block) return;

    for (int i = 0; i < block->function_count; i++) {
        ir_logic_function_free(block->functions[i]);
    }
    free(block->functions);

    for (int i = 0; i < block->event_binding_count; i++) {
        ir_event_binding_free(block->event_bindings[i]);
    }
    free(block->event_bindings);

    free(block);
}

// ============================================================================
// LOOKUP FUNCTIONS
// ============================================================================

IRLogicFunction* ir_logic_block_find_function(IRLogicBlock* block, const char* name) {
    if (!block || !name) return NULL;

    for (int i = 0; i < block->function_count; i++) {
        if (block->functions[i]->name && strcmp(block->functions[i]->name, name) == 0) {
            return block->functions[i];
        }
    }

    return NULL;
}

IREventBinding** ir_logic_block_find_bindings_for_component(IRLogicBlock* block,
                                                             uint32_t component_id,
                                                             int* out_count) {
    *out_count = 0;
    if (!block) return NULL;

    // First pass: count bindings
    int count = 0;
    for (int i = 0; i < block->event_binding_count; i++) {
        if (block->event_bindings[i]->component_id == component_id) {
            count++;
        }
    }

    if (count == 0) return NULL;

    // Second pass: collect bindings
    IREventBinding** result = calloc(count, sizeof(IREventBinding*));
    if (!result) return NULL;

    int idx = 0;
    for (int i = 0; i < block->event_binding_count; i++) {
        if (block->event_bindings[i]->component_id == component_id) {
            result[idx++] = block->event_bindings[i];
        }
    }

    *out_count = count;
    return result;
}

const char* ir_logic_block_get_handler(IRLogicBlock* block, uint32_t component_id, const char* event_type) {
    if (!block || !event_type) return NULL;

    for (int i = 0; i < block->event_binding_count; i++) {
        IREventBinding* binding = block->event_bindings[i];
        if (binding->component_id == component_id &&
            binding->event_type && strcmp(binding->event_type, event_type) == 0) {
            return binding->handler_name;
        }
    }

    return NULL;
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

cJSON* ir_logic_function_to_json(IRLogicFunction* func) {
    if (!func) return cJSON_CreateNull();

    cJSON* json = cJSON_CreateObject();

    // Universal statements
    if (func->has_universal) {
        cJSON* universal = cJSON_CreateObject();

        // Parameters
        if (func->param_count > 0) {
            cJSON* params = cJSON_CreateArray();
            for (int i = 0; i < func->param_count; i++) {
                cJSON* param = cJSON_CreateObject();
                cJSON_AddStringToObject(param, "name", func->params[i].name ? func->params[i].name : "");
                cJSON_AddStringToObject(param, "type", func->params[i].type ? func->params[i].type : "any");
                cJSON_AddItemToArray(params, param);
            }
            cJSON_AddItemToObject(universal, "params", params);
        }

        // Statements
        cJSON* stmts = cJSON_CreateArray();
        for (int i = 0; i < func->statement_count; i++) {
            cJSON_AddItemToArray(stmts, ir_stmt_to_json(func->statements[i]));
        }
        cJSON_AddItemToObject(universal, "statements", stmts);

        cJSON_AddItemToObject(json, "universal", universal);
    } else {
        cJSON_AddNullToObject(json, "universal");
    }

    // Embedded sources
    if (func->source_count > 0) {
        cJSON* sources = cJSON_CreateObject();
        for (int i = 0; i < func->source_count; i++) {
            if (func->sources[i].language && func->sources[i].source) {
                cJSON_AddStringToObject(sources, func->sources[i].language, func->sources[i].source);
            }
        }
        cJSON_AddItemToObject(json, "sources", sources);
    }

    return json;
}

cJSON* ir_event_binding_to_json(IREventBinding* binding) {
    if (!binding) return cJSON_CreateNull();

    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "component_id", binding->component_id);
    cJSON_AddStringToObject(json, "event", binding->event_type ? binding->event_type : "");
    cJSON_AddStringToObject(json, "handler", binding->handler_name ? binding->handler_name : "");

    return json;
}

cJSON* ir_logic_block_to_json(IRLogicBlock* block) {
    if (!block) return cJSON_CreateNull();

    cJSON* json = cJSON_CreateObject();

    // Functions as object with name keys
    cJSON* functions = cJSON_CreateObject();
    for (int i = 0; i < block->function_count; i++) {
        IRLogicFunction* func = block->functions[i];
        if (func->name) {
            cJSON_AddItemToObject(functions, func->name, ir_logic_function_to_json(func));
        }
    }
    cJSON_AddItemToObject(json, "functions", functions);

    // Event bindings as array
    cJSON* bindings = cJSON_CreateArray();
    for (int i = 0; i < block->event_binding_count; i++) {
        cJSON_AddItemToArray(bindings, ir_event_binding_to_json(block->event_bindings[i]));
    }
    cJSON_AddItemToObject(json, "event_bindings", bindings);

    return json;
}

IRLogicFunction* ir_logic_function_from_json(const char* name, cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    IRLogicFunction* func = ir_logic_function_create(name);
    if (!func) return NULL;

    // Parse universal section
    cJSON* universal = cJSON_GetObjectItem(json, "universal");
    if (universal && cJSON_IsObject(universal)) {
        // Parse parameters
        cJSON* params = cJSON_GetObjectItem(universal, "params");
        if (params && cJSON_IsArray(params)) {
            int param_count = cJSON_GetArraySize(params);
            for (int i = 0; i < param_count; i++) {
                cJSON* param = cJSON_GetArrayItem(params, i);
                cJSON* param_name = cJSON_GetObjectItem(param, "name");
                cJSON* param_type = cJSON_GetObjectItem(param, "type");
                if (param_name && cJSON_IsString(param_name)) {
                    ir_logic_function_add_param(func, param_name->valuestring,
                        param_type && cJSON_IsString(param_type) ? param_type->valuestring : "any");
                }
            }
        }

        // Parse statements
        cJSON* stmts = cJSON_GetObjectItem(universal, "statements");
        if (stmts && cJSON_IsArray(stmts)) {
            int stmt_count = cJSON_GetArraySize(stmts);
            for (int i = 0; i < stmt_count; i++) {
                IRStatement* stmt = ir_stmt_from_json(cJSON_GetArrayItem(stmts, i));
                if (stmt) {
                    ir_logic_function_add_stmt(func, stmt);
                }
            }
        }
    }

    // Parse embedded sources
    cJSON* sources = cJSON_GetObjectItem(json, "sources");
    if (sources && cJSON_IsObject(sources)) {
        cJSON* source = NULL;
        cJSON_ArrayForEach(source, sources) {
            if (source->string && cJSON_IsString(source)) {
                ir_logic_function_add_source(func, source->string, source->valuestring);
            }
        }
    }

    return func;
}

IREventBinding* ir_event_binding_from_json(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    cJSON* component_id = cJSON_GetObjectItem(json, "component_id");
    cJSON* event = cJSON_GetObjectItem(json, "event");
    cJSON* handler = cJSON_GetObjectItem(json, "handler");

    if (!component_id || !cJSON_IsNumber(component_id)) return NULL;
    if (!event || !cJSON_IsString(event)) return NULL;
    if (!handler || !cJSON_IsString(handler)) return NULL;

    return ir_event_binding_create(
        (uint32_t)component_id->valuedouble,
        event->valuestring,
        handler->valuestring
    );
}

IRLogicBlock* ir_logic_block_from_json(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    IRLogicBlock* block = ir_logic_block_create();
    if (!block) return NULL;

    // Parse functions
    cJSON* functions = cJSON_GetObjectItem(json, "functions");
    if (functions && cJSON_IsObject(functions)) {
        cJSON* func_json = NULL;
        cJSON_ArrayForEach(func_json, functions) {
            if (func_json->string) {
                IRLogicFunction* func = ir_logic_function_from_json(func_json->string, func_json);
                if (func) {
                    ir_logic_block_add_function(block, func);
                }
            }
        }
    }

    // Parse event bindings
    cJSON* bindings = cJSON_GetObjectItem(json, "event_bindings");
    if (bindings && cJSON_IsArray(bindings)) {
        int binding_count = cJSON_GetArraySize(bindings);
        for (int i = 0; i < binding_count; i++) {
            IREventBinding* binding = ir_event_binding_from_json(cJSON_GetArrayItem(bindings, i));
            if (binding) {
                ir_logic_block_add_binding(block, binding);
            }
        }
    }

    return block;
}

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

IRLogicFunction* ir_logic_create_increment(const char* name, const char* var_name) {
    IRLogicFunction* func = ir_logic_function_create(name);
    if (!func) return NULL;

    // value = value + 1
    IRStatement* stmt = ir_stmt_assign(
        var_name,
        ir_expr_add(ir_expr_var(var_name), ir_expr_int(1))
    );

    ir_logic_function_add_stmt(func, stmt);
    return func;
}

IRLogicFunction* ir_logic_create_decrement(const char* name, const char* var_name) {
    IRLogicFunction* func = ir_logic_function_create(name);
    if (!func) return NULL;

    // value = value - 1
    IRStatement* stmt = ir_stmt_assign(
        var_name,
        ir_expr_sub(ir_expr_var(var_name), ir_expr_int(1))
    );

    ir_logic_function_add_stmt(func, stmt);
    return func;
}

IRLogicFunction* ir_logic_create_toggle(const char* name, const char* var_name) {
    IRLogicFunction* func = ir_logic_function_create(name);
    if (!func) return NULL;

    // value = !value
    IRStatement* stmt = ir_stmt_assign(
        var_name,
        ir_expr_unary(UNARY_OP_NOT, ir_expr_var(var_name))
    );

    ir_logic_function_add_stmt(func, stmt);
    return func;
}

IRLogicFunction* ir_logic_create_reset(const char* name, const char* var_name, int64_t initial_value) {
    IRLogicFunction* func = ir_logic_function_create(name);
    if (!func) return NULL;

    // value = initial_value
    IRStatement* stmt = ir_stmt_assign(var_name, ir_expr_int(initial_value));

    ir_logic_function_add_stmt(func, stmt);
    return func;
}

// ============================================================================
// DEBUG / PRINT
// ============================================================================

void ir_logic_function_print(IRLogicFunction* func) {
    if (!func) return;

    printf("Function: %s\n", func->name ? func->name : "<unnamed>");

    if (func->param_count > 0) {
        printf("  Parameters: ");
        for (int i = 0; i < func->param_count; i++) {
            if (i > 0) printf(", ");
            printf("%s: %s", func->params[i].name ? func->params[i].name : "?",
                   func->params[i].type ? func->params[i].type : "any");
        }
        printf("\n");
    }

    if (func->has_universal) {
        printf("  Universal statements:\n");
        for (int i = 0; i < func->statement_count; i++) {
            printf("    ");
            ir_stmt_print(func->statements[i], 2);
        }
    }

    if (func->source_count > 0) {
        printf("  Embedded sources:\n");
        for (int i = 0; i < func->source_count; i++) {
            printf("    [%s]: %zu chars\n",
                   func->sources[i].language ? func->sources[i].language : "?",
                   func->sources[i].source ? strlen(func->sources[i].source) : 0);
        }
    }
}

void ir_logic_block_print(IRLogicBlock* block) {
    if (!block) return;

    printf("=== Logic Block ===\n");
    printf("Functions: %d\n", block->function_count);

    for (int i = 0; i < block->function_count; i++) {
        ir_logic_function_print(block->functions[i]);
        printf("\n");
    }

    printf("Event Bindings: %d\n", block->event_binding_count);
    for (int i = 0; i < block->event_binding_count; i++) {
        IREventBinding* binding = block->event_bindings[i];
        printf("  Component %u, %s -> %s\n",
               binding->component_id,
               binding->event_type ? binding->event_type : "?",
               binding->handler_name ? binding->handler_name : "?");
    }
}
