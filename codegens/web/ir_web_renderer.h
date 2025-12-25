#ifndef IR_WEB_RENDERER_H
#define IR_WEB_RENDERER_H

#include "../../ir/ir_core.h"
#include <stdbool.h>

// Forward declarations for IR functions
bool ir_validate_component(IRComponent* component);
void ir_print_component_info(IRComponent* component, int depth);

// Web IR Renderer Context (opaque)
typedef struct WebIRRenderer WebIRRenderer;

// Web IR Renderer Functions
WebIRRenderer* web_ir_renderer_create(void);
void web_ir_renderer_destroy(WebIRRenderer* renderer);

// Configuration
void web_ir_renderer_set_output_directory(WebIRRenderer* renderer, const char* directory);
void web_ir_renderer_set_generate_separate_files(WebIRRenderer* renderer, bool separate);
void web_ir_renderer_set_include_javascript_runtime(WebIRRenderer* renderer, bool include);
void web_ir_renderer_set_include_wasm_modules(WebIRRenderer* renderer, bool include);

// Main rendering function
bool web_ir_renderer_render(WebIRRenderer* renderer, IRComponent* root);

// Memory-based rendering (returns allocated strings that must be freed)
bool web_ir_renderer_render_to_memory(WebIRRenderer* renderer, IRComponent* root,
                                       char** html_output, char** css_output, char** js_output);

// Validation and debugging
bool web_ir_renderer_validate_ir(WebIRRenderer* renderer, IRComponent* root);
void web_ir_renderer_print_tree_info(WebIRRenderer* renderer, IRComponent* root);
void web_ir_renderer_print_stats(WebIRRenderer* renderer, IRComponent* root);

// Convenience function for quick rendering
bool web_render_ir_component(IRComponent* root, const char* output_dir);

#endif // IR_WEB_RENDERER_H