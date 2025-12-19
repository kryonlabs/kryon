#ifndef IR_HTML_PARSER_H
#define IR_HTML_PARSER_H

#include "ir_html_ast.h"
#include <stddef.h>

// HTML Parser - Parses Kryon-generated HTML back to AST
// Designed to parse ONLY the HTML output from html_generator.c (transpilation mode)
// NOT a full HTML5 spec parser

// Parse HTML string into AST
// Returns root HtmlNode on success, NULL on failure
// Caller must free the tree with html_node_free()
HtmlNode* ir_html_parse(const char* html, size_t length);

// Parse HTML from file path
// Returns root HtmlNode on success, NULL on failure
// Caller must free the tree with html_node_free()
HtmlNode* ir_html_parse_file(const char* filepath);

// Convert HTML AST to JSON .kir format
// Returns JSON string (caller must free), NULL on failure
char* ir_html_to_kir(const char* html, size_t length);

// Convert HTML file to JSON .kir format
// Returns JSON string (caller must free), NULL on failure
char* ir_html_file_to_kir(const char* filepath);

#endif // IR_HTML_PARSER_H
