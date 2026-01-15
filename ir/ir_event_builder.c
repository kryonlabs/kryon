// IR Event Builder Module
// Event management functions extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_event_builder.h"
#include "ir_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for handler source destroy
extern void ir_destroy_handler_source(IRHandlerSource* source);

// ============================================================================
// Event Management
// ============================================================================

IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data) {
    IREvent* event = malloc(sizeof(IREvent));
    if (!event) return NULL;

    memset(event, 0, sizeof(IREvent));

    event->type = type;
    event->logic_id = logic_id ? strdup(logic_id) : NULL;
    event->handler_data = handler_data ? strdup(handler_data) : NULL;

    // Plugin event name handling is now done via the capability system
    event->event_name = NULL;  // Can be set by caller if needed

    return event;
}

void ir_destroy_event(IREvent* event) {
    if (!event) return;

    if (event->event_name) free(event->event_name);
    if (event->logic_id) free(event->logic_id);
    if (event->handler_data) free(event->handler_data);
    if (event->handler_source) ir_destroy_handler_source(event->handler_source);

    free(event);
}

void ir_add_event(IRComponent* component, IREvent* event) {
    if (!component || !event) return;

    event->next = component->events;
    component->events = event;
}

void ir_remove_event(IRComponent* component, IREvent* event) {
    if (!component || !event || !component->events) return;

    if (component->events == event) {
        component->events = event->next;
        return;
    }

    IREvent* current = component->events;
    while (current->next) {
        if (current->next == event) {
            current->next = event->next;
            return;
        }
        current = current->next;
    }
}

IREvent* ir_find_event(IRComponent* component, IREventType type) {
    if (!component || !component->events) return NULL;

    IREvent* event = component->events;
    while (event) {
        if (event->type == type) return event;
        event = event->next;
    }

    return NULL;
}

// ============================================================================
// Event Bytecode Support (IR v2.1)
// ============================================================================

void ir_event_set_bytecode_function_id(IREvent* event, uint32_t function_id) {
    if (!event) return;
    event->bytecode_function_id = function_id;
}

uint32_t ir_event_get_bytecode_function_id(IREvent* event) {
    if (!event) return 0;
    return event->bytecode_function_id;
}

// ============================================================================
// Handler Source Management (for Lua source preservation in KIR)
// ============================================================================

IRHandlerSource* ir_create_handler_source(const char* language, const char* code, const char* file, int line) {
    IRHandlerSource* source = calloc(1, sizeof(IRHandlerSource));
    if (!source) return NULL;

    source->language = language ? strdup(language) : NULL;
    source->code = code ? strdup(code) : NULL;
    source->file = file ? strdup(file) : NULL;
    source->line = line;

    return source;
}

void ir_destroy_handler_source(IRHandlerSource* source) {
    if (!source) return;
    if (source->language) free(source->language);
    if (source->code) free(source->code);
    if (source->file) free(source->file);

    // Free closure metadata
    if (source->closure_vars) {
        for (int i = 0; i < source->closure_var_count; i++) {
            free(source->closure_vars[i]);
        }
        free(source->closure_vars);
    }

    free(source);
}

void ir_event_set_handler_source(IREvent* event, IRHandlerSource* source) {
    if (!event) return;
    // Free existing handler source if any
    if (event->handler_source) {
        ir_destroy_handler_source(event->handler_source);
    }
    event->handler_source = source;
}

// Set closure metadata on a handler source
// vars: array of variable name strings
// count: number of variables
int ir_handler_source_set_closures(IRHandlerSource* source, const char** vars, int count) {
    if (!source) return -1;
    if (!vars || count <= 0) {
        source->uses_closures = false;
        source->closure_vars = NULL;
        source->closure_var_count = 0;
        return 0;
    }

    source->uses_closures = true;
    source->closure_var_count = count;
    source->closure_vars = calloc(count, sizeof(char*));

    if (!source->closure_vars) {
        source->closure_var_count = 0;
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (vars[i]) {
            source->closure_vars[i] = strdup(vars[i]);
        }
    }

    return 0;
}
