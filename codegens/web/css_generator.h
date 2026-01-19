#ifndef CSS_GENERATOR_H
#define CSS_GENERATOR_H

#include "../../ir/include/ir_core.h"
#include <stdbool.h>
#include <stdarg.h>

// CSS Variable entry (for :root block generation)
typedef struct {
    char* name;   // Variable name without "--" prefix (e.g., "bg", "primary")
    char* value;  // CSS value (e.g., "#0f0f13", "16px")
} CSSVariable;

// CSS Generator Context (opaque)
typedef struct CSSGenerator CSSGenerator;

// CSS Generator Functions
CSSGenerator* css_generator_create(void);
void css_generator_destroy(CSSGenerator* generator);

void css_generator_set_pretty_print(CSSGenerator* generator, bool pretty);

// Set CSS variables from reactive manifest (for :root block output)
// Call this before css_generator_generate() to output :root with custom properties
void css_generator_set_css_variables(CSSGenerator* generator, CSSVariable* variables, size_t count);

// Set CSS variables from IRReactiveManifest (convenience function)
// Extracts variables with "css:" prefix from manifest
void css_generator_set_manifest(CSSGenerator* generator, IRReactiveManifest* manifest);

// Main CSS generation function
const char* css_generator_generate(CSSGenerator* generator, IRComponent* root);
bool css_generator_write_to_file(CSSGenerator* generator, IRComponent* root, const char* filename);

// Utility functions
size_t css_generator_get_size(CSSGenerator* generator);
const char* css_generator_get_buffer(CSSGenerator* generator);

#endif // CSS_GENERATOR_H