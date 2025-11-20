#ifndef CSS_GENERATOR_H
#define CSS_GENERATOR_H

#include "../../ir/ir_core.h"
#include <stdbool.h>
#include <stdarg.h>

// CSS Generator Context (opaque)
typedef struct CSSGenerator CSSGenerator;

// CSS Generator Functions
CSSGenerator* css_generator_create(void);
void css_generator_destroy(CSSGenerator* generator);

void css_generator_set_pretty_print(CSSGenerator* generator, bool pretty);

// Main CSS generation function
const char* css_generator_generate(CSSGenerator* generator, IRComponent* root);
bool css_generator_write_to_file(CSSGenerator* generator, IRComponent* root, const char* filename);

// Utility functions
size_t css_generator_get_size(CSSGenerator* generator);
const char* css_generator_get_buffer(CSSGenerator* generator);

#endif // CSS_GENERATOR_H