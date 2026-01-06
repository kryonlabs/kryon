/**
 * HTML/CSS/JS Transpiler (Codegen Frontend)
 *
 * This is a transpilation/codegen frontend that generates browser-renderable code.
 *
 * Architecture:
 * - Rendering backends (SDL3, Terminal) use Kryon's renderer to draw UI directly
 * - Transpilation frontends (HTML/web) generate code that browsers render
 *
 * This module:
 * - Takes KIR (Kryon Intermediate Representation) as input
 * - Outputs HTML/CSS/JavaScript code for browsers
 * - Does NOT render anything itself - the browser does the rendering
 */

#ifndef HTML_GENERATOR_H
#define HTML_GENERATOR_H

#include "../../ir/ir_core.h"
#include "../../ir/ir_logic.h"
#include <stdbool.h>
#include <stdarg.h>

// HTML Generator Options
typedef struct {
    bool minify;            // Minify output (remove whitespace)
    bool embedded_css;      // Embed CSS in <style> tag instead of external file
    bool include_runtime;   // Include JavaScript runtime for event handlers
} HtmlGeneratorOptions;

// Collected handler for embedding in Lua registry
typedef struct {
    uint32_t component_id;  // Component ID for the handler key
    char* code;             // Lua function source code
} CollectedHandler;

// HTML Generator Context
typedef struct HTMLGenerator {
    char* output_buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    int indent_level;
    bool pretty_print;
    HtmlGeneratorOptions options;  // Generation options
    IRLogicBlock* logic_block;     // Logic block for event handlers (transpile mode)
    IRReactiveManifest* manifest;  // Manifest for CSS variables (optional)

    // Collected handlers for Lua registry generation
    CollectedHandler* handlers;
    size_t handler_count;
    size_t handler_capacity;
} HTMLGenerator;

// HTML Generator Functions
HTMLGenerator* html_generator_create(void);
HTMLGenerator* html_generator_create_with_options(HtmlGeneratorOptions options);
void html_generator_destroy(HTMLGenerator* generator);

// Helper to get default options
HtmlGeneratorOptions html_generator_default_options(void);

void html_generator_set_pretty_print(HTMLGenerator* generator, bool pretty);
void html_generator_set_manifest(HTMLGenerator* generator, IRReactiveManifest* manifest);

bool html_generator_write_string(HTMLGenerator* generator, const char* string);
bool html_generator_write_format(HTMLGenerator* generator, const char* format, ...);

// Main HTML generation function
const char* html_generator_generate(HTMLGenerator* generator, IRComponent* root);
const char* html_generator_generate_with_logic(HTMLGenerator* generator, IRComponent* root, IRLogicBlock* logic_block);
bool html_generator_write_to_file(HTMLGenerator* generator, IRComponent* root, const char* filename);

// Utility functions
size_t html_generator_get_size(HTMLGenerator* generator);
const char* html_generator_get_buffer(HTMLGenerator* generator);

#endif // HTML_GENERATOR_H