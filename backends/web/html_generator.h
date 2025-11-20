#ifndef HTML_GENERATOR_H
#define HTML_GENERATOR_H

#include "../../ir/ir_core.h"
#include <stdbool.h>
#include <stdarg.h>

// HTML Generator Context
typedef struct HTMLGenerator {
    char* output_buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    int indent_level;
    bool pretty_print;
} HTMLGenerator;

// HTML Generator Functions
HTMLGenerator* html_generator_create(void);
void html_generator_destroy(HTMLGenerator* generator);

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