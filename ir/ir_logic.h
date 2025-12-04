#ifndef IR_LOGIC_H
#define IR_LOGIC_H

#include "ir_expression.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// LOGIC BLOCK TYPES
// ============================================================================
// Logic blocks contain functions and event bindings that can be:
// 1. Universal expressions (transpilable to any target)
// 2. Embedded source code (language-specific)

// Maximum number of languages for embedded sources
#define IR_LOGIC_MAX_SOURCES 8

// Embedded source code for a specific language
typedef struct {
    char* language;  // "nim", "lua", "js", etc.
    char* source;    // The actual source code
} IRLogicSource;

// Function parameter
typedef struct {
    char* name;
    char* type;  // "int", "string", "bool", "any", etc.
} IRLogicParam;

// Logic function definition
typedef struct {
    char* name;  // e.g., "Counter:increment"

    // Universal representation (if available)
    bool has_universal;
    IRLogicParam* params;
    int param_count;
    IRStatement** statements;
    int statement_count;

    // Embedded source code (for complex handlers)
    IRLogicSource sources[IR_LOGIC_MAX_SOURCES];
    int source_count;
} IRLogicFunction;

// Event binding (maps component event to handler)
typedef struct {
    uint32_t component_id;
    char* event_type;    // "click", "change", "submit", etc.
    char* handler_name;  // Reference to IRLogicFunction.name
} IREventBinding;

// Complete logic block
typedef struct IRLogicBlock {
    IRLogicFunction** functions;
    int function_count;

    IREventBinding** event_bindings;
    int event_binding_count;
} IRLogicBlock;

// ============================================================================
// CONSTRUCTOR FUNCTIONS
// ============================================================================

// Create a new logic function with universal statements
IRLogicFunction* ir_logic_function_create(const char* name);

// Add a parameter to a logic function
void ir_logic_function_add_param(IRLogicFunction* func, const char* name, const char* type);

// Add a statement to a logic function
void ir_logic_function_add_stmt(IRLogicFunction* func, IRStatement* stmt);

// Add embedded source code for a language
void ir_logic_function_add_source(IRLogicFunction* func, const char* language, const char* source);

// Create an event binding
IREventBinding* ir_event_binding_create(uint32_t component_id, const char* event_type, const char* handler_name);

// Create an empty logic block
IRLogicBlock* ir_logic_block_create(void);

// Add a function to the logic block
void ir_logic_block_add_function(IRLogicBlock* block, IRLogicFunction* func);

// Add an event binding to the logic block
void ir_logic_block_add_binding(IRLogicBlock* block, IREventBinding* binding);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void ir_logic_function_free(IRLogicFunction* func);
void ir_event_binding_free(IREventBinding* binding);
void ir_logic_block_free(IRLogicBlock* block);

// ============================================================================
// LOOKUP FUNCTIONS
// ============================================================================

// Find a function by name
IRLogicFunction* ir_logic_block_find_function(IRLogicBlock* block, const char* name);

// Find event bindings for a component
IREventBinding** ir_logic_block_find_bindings_for_component(IRLogicBlock* block,
                                                             uint32_t component_id,
                                                             int* out_count);

// Get the handler name for a component event
const char* ir_logic_block_get_handler(IRLogicBlock* block, uint32_t component_id, const char* event_type);

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

struct cJSON;

// Serialize logic function to JSON
struct cJSON* ir_logic_function_to_json(IRLogicFunction* func);

// Serialize event binding to JSON
struct cJSON* ir_event_binding_to_json(IREventBinding* binding);

// Serialize entire logic block to JSON
struct cJSON* ir_logic_block_to_json(IRLogicBlock* block);

// Parse logic function from JSON
IRLogicFunction* ir_logic_function_from_json(const char* name, struct cJSON* json);

// Parse event binding from JSON
IREventBinding* ir_event_binding_from_json(struct cJSON* json);

// Parse entire logic block from JSON
IRLogicBlock* ir_logic_block_from_json(struct cJSON* json);

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

// Create increment handler: value = value + 1
IRLogicFunction* ir_logic_create_increment(const char* name, const char* var_name);

// Create decrement handler: value = value - 1
IRLogicFunction* ir_logic_create_decrement(const char* name, const char* var_name);

// Create toggle handler: value = !value
IRLogicFunction* ir_logic_create_toggle(const char* name, const char* var_name);

// Create reset handler: value = initial
IRLogicFunction* ir_logic_create_reset(const char* name, const char* var_name, int64_t initial_value);

// ============================================================================
// DEBUG / PRINT
// ============================================================================

void ir_logic_function_print(IRLogicFunction* func);
void ir_logic_block_print(IRLogicBlock* block);

#endif // IR_LOGIC_H
