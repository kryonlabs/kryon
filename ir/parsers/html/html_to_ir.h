#ifndef IR_HTML_TO_IR_H
#define IR_HTML_TO_IR_H

#include "html_ast.h"
#include "../../ir_core.h"

// HTML to IR Conversion - Convert HTML AST to Kryon IR components
// Maps HTML tags to IR component types for roundtrip compatibility

// Convert HTML AST tree to IR component tree
// Returns root IRComponent on success, NULL on failure
// Caller must free the tree with ir_component_free()
IRComponent* ir_html_to_ir_convert(HtmlNode* html_root);

// Convert single HTML node to IR component (recursive)
IRComponent* ir_html_node_to_component(HtmlNode* node);

// Helper: Map HTML tag name to IR component type
IRComponentType ir_html_tag_to_component_type(const char* tag_name);

// Helper: Infer component type from CSS class names
IRComponentType ir_infer_type_from_classes(const char* class_string);

// Helper: Parse inline CSS style attribute to IRStyle
void ir_html_parse_inline_css(const char* style_str, IRStyle* ir_style);

// Helper: Parse inline CSS to both style and layout
void ir_html_parse_inline_css_full(const char* style_str, IRStyle* ir_style, IRLayout* ir_layout);

#endif // IR_HTML_TO_IR_H
