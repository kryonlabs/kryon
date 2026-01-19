#ifndef IR_EVENT_BUILDER_H
#define IR_EVENT_BUILDER_H

#include <stdint.h>
#include <stdbool.h>

#include "../include/ir_core.h"

// ============================================================================
// Event Management
// ============================================================================

IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data);
void ir_destroy_event(IREvent* event);
void ir_add_event(IRComponent* component, IREvent* event);
void ir_remove_event(IRComponent* component, IREvent* event);
IREvent* ir_find_event(IRComponent* component, IREventType type);

// ============================================================================
// Event Bytecode Support (IR v2.1)
// ============================================================================

void ir_event_set_bytecode_function_id(IREvent* event, uint32_t function_id);
uint32_t ir_event_get_bytecode_function_id(IREvent* event);

// ============================================================================
// Handler Source Management (for Lua source preservation in KIR)
// ============================================================================

IRHandlerSource* ir_create_handler_source(const char* language, const char* code, const char* file, int line);
void ir_destroy_handler_source(IRHandlerSource* source);
void ir_event_set_handler_source(IREvent* event, IRHandlerSource* source);

// Set closure metadata on a handler source
// vars: array of variable name strings
// count: number of variables
int ir_handler_source_set_closures(IRHandlerSource* source, const char** vars, int count);

#endif // IR_EVENT_BUILDER_H
