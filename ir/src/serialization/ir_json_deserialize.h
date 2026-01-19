#ifndef IR_JSON_DESERIALIZE_H
#define IR_JSON_DESERIALIZE_H

#include "../include/ir_core.h"
#include "../include/ir_stylesheet.h"
#include "../include/ir_json_context.h"
#include "cJSON.h"

// Forward declarations
typedef struct IRLogicBlock IRLogicBlock;
typedef struct IRReactiveManifest IRReactiveManifest;

// ============================================================================
// Main Deserialization Functions
// ============================================================================

// Deserialize JSON string to component tree
// Returns: Root component, or NULL on error
IRComponent* ir_json_deserialize_component_tree(const char* json_string);

// Deserialize cJSON object to component tree
IRComponent* ir_json_deserialize_from_cjson(cJSON* root);

// ============================================================================
// Component Deserialization
// ============================================================================

// Deserialize a single component from JSON with component definition context
IRComponent* ir_json_deserialize_component_with_context(cJSON* json, ComponentDefContext* ctx);

// ============================================================================
// Property Deserialization Helpers
// ============================================================================

// Parse dimension from string (e.g., "100px", "50%", "auto")
IRDimension ir_json_parse_dimension(const char* str);

// Parse color from string (e.g., "#rrggbb", "transparent", "var(--border)")
IRColor ir_json_parse_color(const char* str);

// Parse spacing from JSON (number or array)
IRSpacing ir_json_parse_spacing(cJSON* json);

// ============================================================================
// Style and Layout Deserialization
// ============================================================================

// Deserialize style from JSON object into IRStyle
void ir_json_deserialize_style(cJSON* obj, IRStyle* style);

// Deserialize layout from JSON object into IRLayout
void ir_json_deserialize_layout(cJSON* obj, IRLayout* layout);

// Deserialize style properties from JSON into IRStyleProperties
void ir_json_deserialize_style_properties(cJSON* propsObj, IRStyleProperties* props);

// ============================================================================
// Enum Parsing Helpers
// ============================================================================

// Parse text alignment from string
IRTextAlign ir_json_parse_text_align(const char* str);

// Parse alignment (justify-content, align-items) from string
IRAlignment ir_json_parse_alignment(const char* str);

// Parse grid track type from string
IRGridTrackType ir_json_parse_grid_track_type(const char* str);

// ============================================================================
// Gradient Deserialization
// ============================================================================

// Deserialize gradient from JSON object
IRGradient* ir_json_deserialize_gradient(cJSON* obj);

// ============================================================================
// C Metadata Deserialization (for C->KIR->C round-trip)
// ============================================================================

// Deserialize C metadata from JSON into global g_c_metadata
void ir_json_deserialize_c_metadata(cJSON* c_meta_obj);

// ============================================================================
// Stylesheet Deserialization
// ============================================================================

// Deserialize stylesheet from JSON object
IRStylesheet* ir_json_deserialize_stylesheet(cJSON* obj);

// ============================================================================
// Component Definition Context
// ============================================================================

// The context functions are in ir_json_context.h
#include "../include/ir_json_context.h"

#endif // IR_JSON_DESERIALIZE_H
