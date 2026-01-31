#ifndef IR_SERIALIZATION_H
#define IR_SERIALIZATION_H

#include "ir_core.h"
#include "ir_buffer.h"
#include <stdio.h>

// Forward declarations
typedef struct IRLogicBlock IRLogicBlock;

// Forward declarations for functions defined in ir_builder.c
IRStyle* ir_create_style(void);
void ir_destroy_style(IRStyle* style);
IRLayout* ir_create_layout(void);
void ir_destroy_layout(IRLayout* layout);
IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data);
void ir_destroy_event(IREvent* event);
IRLogic* ir_create_logic(const char* id, LogicSourceType type, const char* source_code);
void ir_destroy_logic(IRLogic* logic);
void ir_destroy_component(IRComponent* component);

// IR Serialization Format Version
// v2.0: Complete serialization with layout, animations, transitions, pseudo-styles, etc.
#define IR_FORMAT_VERSION_MAJOR 2
#define IR_FORMAT_VERSION_MINOR 0

// Serialization Types
typedef enum {
    IR_SERIALIZE_BINARY = 1,
    IR_SERIALIZE_JSON = 2
} IRSerializeType;

// Binary Serialization Functions
IRBuffer* ir_serialize_binary(IRComponent* root);
IRComponent* ir_deserialize_binary(IRBuffer* buffer);
bool ir_write_binary_file(IRComponent* root, const char* filename);
IRComponent* ir_read_binary_file(const char* filename);

// ============================================================================
// KIR Serialization - ONE Complete Format
// ============================================================================

// Serialize to KIR JSON (complete format with manifest, logic, and metadata)
char* ir_serialize_json_complete(
    IRComponent* root,
    IRReactiveManifest* manifest,
    struct IRLogicBlock* logic_block,
    IRSourceMetadata* source_metadata,
    IRSourceStructures* source_structures
);

// Legacy function (kept for backwards compatibility, uses NULL for logic/metadata)
char* ir_serialize_json(IRComponent* root, IRReactiveManifest* manifest);

bool ir_write_json_file(IRComponent* root, IRReactiveManifest* manifest, const char* filename);

// Deserialize from KIR JSON
IRComponent* ir_deserialize_json(const char* json_string);
IRComponent* ir_read_json_file(const char* filename);

// Deserialize with manifest and source_structures (returned via out parameters)
IRComponent* ir_read_json_file_with_manifest(const char* filename,
                                              IRReactiveManifest** out_manifest,
                                              IRSourceStructures** out_source_structures);

// For loop runtime expansion (call after loading KIR file)
// Pass source_structures from manifest to resolve const variable references
// For backward compatibility, source_structures can be NULL
// This function expands compile-time For loops; runtime For loops are handled differently
void ir_expand_for(IRComponent* root, IRSourceStructures* source_structures);

// Legacy function name (deprecated, use ir_expand_for instead)
void ir_expand_foreach(IRComponent* root, IRSourceStructures* source_structures);

// Component type conversion (for parsers)
IRComponentType ir_string_to_component_type(const char* str);

// Validation Functions
bool ir_validate_binary_format(IRBuffer* buffer);
bool ir_validate_json_format(const char* json_string);

// Utility Functions
size_t ir_calculate_serialized_size(IRComponent* root);
void ir_print_component_info(IRComponent* component, int depth);
void ir_print_buffer_info(IRBuffer* buffer);

#endif // IR_SERIALIZATION_H