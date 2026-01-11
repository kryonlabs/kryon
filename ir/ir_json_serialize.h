#ifndef IR_JSON_SERIALIZE_H
#define IR_JSON_SERIALIZE_H

#include "ir_core.h"
#include "cJSON.h"

// Forward declarations
typedef struct IRLogicBlock IRLogicBlock;
typedef struct IRSourceMetadata IRSourceMetadata;
typedef struct IRSourceStructures IRSourceStructures;

// ============================================================================
// Main Serialization Functions
// ============================================================================

// Serialize component tree to JSON string
// Returns: Newly allocated string that must be freed by caller, or NULL on error
char* ir_json_serialize_component_tree(IRComponent* root);

// Serialize complete IR to cJSON object (with manifest, logic, metadata)
cJSON* ir_json_serialize_complete(
    IRComponent* root,
    IRReactiveManifest* manifest,
    IRLogicBlock* logic_block,
    IRSourceMetadata* source_metadata,
    IRSourceStructures* source_structures
);

// ============================================================================
// Component Serialization
// ============================================================================

// Serialize a component recursively to cJSON object
cJSON* ir_json_serialize_component_recursive(IRComponent* component);

// Serialize a component as a template (full tree, no references)
cJSON* ir_json_serialize_component_as_template(IRComponent* component);

// ============================================================================
// Property Serialization Helpers
// ============================================================================

// Serialize dimension to JSON string (e.g., "100px", "50%", "auto")
char* ir_json_serialize_dimension_to_string(IRDimension dim);

// Serialize color to JSON string (e.g., "#rrggbb" or "#rrggbbaa")
char* ir_json_serialize_color_to_string(IRColor color);

// Serialize gradient to JSON object
cJSON* ir_json_serialize_gradient(IRGradient* gradient);

// ============================================================================
// Style and Layout Serialization
// ============================================================================

// Serialize style properties to JSON object
void ir_json_serialize_style(cJSON* obj, IRStyle* style, IRComponent* component);

// Serialize layout properties to JSON object
void ir_json_serialize_layout(cJSON* obj, IRLayout* layout, IRComponent* component);

// ============================================================================
// Manifest and Metadata Serialization
// ============================================================================

// Serialize reactive manifest to JSON object
cJSON* ir_json_serialize_reactive_manifest(IRReactiveManifest* manifest);

// Serialize component definitions to JSON array
cJSON* ir_json_serialize_component_definitions(IRReactiveManifest* manifest);

// Serialize source metadata to JSON object
cJSON* ir_json_serialize_metadata(IRSourceMetadata* metadata);

// Serialize logic block to JSON object
cJSON* ir_json_serialize_logic_block(IRLogicBlock* logic_block);

// ============================================================================
// Stylesheet Serialization
// ============================================================================

// Serialize stylesheet to JSON object
cJSON* ir_json_serialize_stylesheet(IRStylesheet* stylesheet);

// ============================================================================
// Source Structures Serialization (for Kry -> KIR -> Kry round-trip)
// ============================================================================

// Serialize source structures to JSON object
cJSON* ir_json_serialize_source_structures(IRSourceStructures* ss);

// ============================================================================
// C Metadata Serialization (for C->KIR->C round-trip)
// ============================================================================

// Serialize C metadata to JSON object
cJSON* ir_json_serialize_c_metadata(void);

#endif // IR_JSON_SERIALIZE_H
