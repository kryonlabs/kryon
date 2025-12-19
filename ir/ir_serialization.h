#ifndef IR_SERIALIZATION_H
#define IR_SERIALIZATION_H

#include "ir_core.h"
#include <stdio.h>

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

// JSON Serialization Functions (for debugging/tooling)
char* ir_serialize_json(IRComponent* root);
IRComponent* ir_deserialize_json(const char* json_string);
bool ir_write_json_file(IRComponent* root, const char* filename);
IRComponent* ir_read_json_file(const char* filename);

// JSON v2 Serialization (complete property coverage)
char* ir_serialize_json_v2(IRComponent* root);
bool ir_write_json_v2_file(IRComponent* root, const char* filename);

// JSON v2.1 Serialization with Reactive Manifest (POC)
char* ir_serialize_json_v2_with_manifest(IRComponent* root, IRReactiveManifest* manifest);
bool ir_write_json_v2_with_manifest_file(IRComponent* root, IRReactiveManifest* manifest, const char* filename);

// Component type conversion (for HTML parser and other tools)
IRComponentType ir_string_to_component_type(const char* str);

// Forward declaration for logic block (defined in ir_logic.h)
struct IRLogicBlock;

// JSON v3.0 Serialization with Logic Block Support
char* ir_serialize_json_v3(IRComponent* root, IRReactiveManifest* manifest, struct IRLogicBlock* logic);
bool ir_write_json_v3_file(IRComponent* root, IRReactiveManifest* manifest, struct IRLogicBlock* logic, const char* filename);

// JSON v2 Deserialization
IRComponent* ir_deserialize_json_v2(const char* json_string);
IRComponent* ir_read_json_v2_file(const char* filename);

// Buffer Management
IRBuffer* ir_buffer_create(size_t initial_capacity);
IRBuffer* ir_buffer_create_from_file(const char* filename);
void ir_buffer_destroy(IRBuffer* buffer);
bool ir_buffer_write(IRBuffer* buffer, const void* data, size_t size);
bool ir_buffer_read(IRBuffer* buffer, void* data, size_t size);
bool ir_buffer_seek(IRBuffer* buffer, size_t position);
size_t ir_buffer_tell(IRBuffer* buffer);
size_t ir_buffer_size(IRBuffer* buffer);

// Validation Functions
bool ir_validate_binary_format(IRBuffer* buffer);
bool ir_validate_json_format(const char* json_string);

// Utility Functions
size_t ir_calculate_serialized_size(IRComponent* root);
void ir_print_component_info(IRComponent* component, int depth);
void ir_print_buffer_info(IRBuffer* buffer);

#endif // IR_SERIALIZATION_H