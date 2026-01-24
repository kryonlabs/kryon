/**
 * C Code Generator - Internal Shared Declarations
 *
 * Common types and forward declarations shared across C codegen modules.
 */

#ifndef IR_C_INTERNAL_H
#define IR_C_INTERNAL_H

#include <stdio.h>
#include <stdbool.h>
#include "../../third_party/cJSON/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Code Generation Context
// ============================================================================

typedef struct {
    FILE* output;
    int indent_level;
    cJSON* root_json;           // Full KIR JSON
    cJSON* component_tree;      // Root component
    cJSON* c_metadata;          // C metadata section
    cJSON* variables;           // Variables array
    cJSON* event_handlers;      // Event handlers array
    cJSON* helper_functions;    // Helper functions array
    cJSON* includes;            // Includes array
    cJSON* preprocessor_dirs;   // Preprocessor directives array

    // Reactive state tracking
    cJSON* reactive_manifest;   // reactive_manifest from KIR
    cJSON* reactive_vars;       // reactive_manifest.variables array
    bool has_reactive_state;    // True if any reactive variables exist
    const char* current_scope;  // Current component scope during tree traversal (e.g., "Counter#0")
} CCodegenContext;

#ifdef __cplusplus
}
#endif

#endif // IR_C_INTERNAL_H
