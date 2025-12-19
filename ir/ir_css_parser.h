#ifndef IR_CSS_PARSER_H
#define IR_CSS_PARSER_H

#include "ir_core.h"
#include <stdbool.h>

// CSS Parser - Parse inline CSS styles for roundtrip HTML → IR

// CSS property parsed from style attribute
typedef struct {
    const char* property;
    const char* value;
} CSSProperty;

// Parse inline CSS string into property/value pairs
// Input: "width: 100px; height: 50px; background-color: rgba(255, 0, 0, 1.0);"
// Output: Array of CSSProperty structs
CSSProperty* ir_css_parse_inline(const char* style_string, uint32_t* count);

// Free CSS property array
void ir_css_free_properties(CSSProperty* props, uint32_t count);

// Apply CSS properties to IR style
// This maps CSS properties to IR style fields
void ir_css_apply_to_style(IRStyle* style, const CSSProperty* props, uint32_t count);

// Individual property parsers (helpers)
bool ir_css_parse_color(const char* value, IRColor* out_color);
bool ir_css_parse_dimension(const char* value, IRDimension* out_dimension);
bool ir_css_parse_spacing(const char* value, IRSpacing* out_spacing);

#endif // IR_CSS_PARSER_H
