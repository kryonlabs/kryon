#ifndef HTML_GENERATOR_H
#define HTML_GENERATOR_H

#include "../../ir/ir_core.h"
#include <stdbool.h>
#include <stdarg.h>

// HTML Generation Modes
typedef enum {
    HTML_MODE_DISPLAY,      // Runtime rendering with JS (current behavior)
    HTML_MODE_TRANSPILE,    // Static transpilation for roundtrip (new)
} HtmlGeneratorMode;

// HTML Generator Options
typedef struct {
    HtmlGeneratorMode mode;
    bool minify;            // Minify output (remove whitespace)
    bool inline_css;        // Inline CSS vs external file
    bool preserve_ids;      // Preserve component IDs for debugging
} HtmlGeneratorOptions;

// HTML Generator Context
typedef struct HTMLGenerator {
    char* output_buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    int indent_level;
    bool pretty_print;
    HtmlGeneratorOptions options;  // Generation options
} HTMLGenerator;

// HTML Generator Functions
HTMLGenerator* html_generator_create(void);
HTMLGenerator* html_generator_create_with_options(HtmlGeneratorOptions options);
void html_generator_destroy(HTMLGenerator* generator);

// Helper to get default options
HtmlGeneratorOptions html_generator_default_options(void);

void html_generator_set_pretty_print(HTMLGenerator* generator, bool pretty);

bool html_generator_write_string(HTMLGenerator* generator, const char* string);
bool html_generator_write_format(HTMLGenerator* generator, const char* format, ...);

// Main HTML generation function
const char* html_generator_generate(HTMLGenerator* generator, IRComponent* root);
bool html_generator_write_to_file(HTMLGenerator* generator, IRComponent* root, const char* filename);

// Utility functions
size_t html_generator_get_size(HTMLGenerator* generator);
const char* html_generator_get_buffer(HTMLGenerator* generator);

#endif // HTML_GENERATOR_H