#ifndef IR_CSS_STYLESHEET_H
#define IR_CSS_STYLESHEET_H

#include "css_parser.h"
#include "../include/ir_core.h"
#include "../../include/ir_style.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * CSS Stylesheet Parser
 *
 * Parses external CSS files and matches selectors to HTML elements.
 * This enables HTML → KIR round-trip with style preservation.
 */

// A single CSS rule (selector + properties)
typedef struct {
    char* selector;           // e.g., ".button", ".row", "#header"
    CSSProperty* properties;  // Array of property/value pairs
    uint32_t property_count;
} CSSRule;

// CSS Variable (custom property)
typedef struct {
    char* name;   // Variable name without -- prefix (e.g., "primary")
    char* value;  // Resolved value (e.g., "#6366f1")
} CSSVariable;

// CSS Media Query (stores raw condition and content for roundtrip)
typedef struct {
    char* condition;   // e.g., "max-width: 640px"
    char* css_content; // Raw CSS rules inside the media query
} CSSMediaQuery;

// A parsed stylesheet containing multiple rules
typedef struct {
    CSSRule* rules;
    uint32_t rule_count;
    uint32_t rule_capacity;

    // CSS Variables from :root
    CSSVariable* variables;
    uint32_t variable_count;
    uint32_t variable_capacity;

    // Media Queries
    CSSMediaQuery* media_queries;
    uint32_t media_query_count;
    uint32_t media_query_capacity;
} CSSStylesheet;

// Parse CSS content into a stylesheet
CSSStylesheet* ir_css_parse_stylesheet(const char* css_content);

// Parse CSS file into a stylesheet
CSSStylesheet* ir_css_parse_stylesheet_file(const char* filepath);

// Free stylesheet and all its rules
void ir_css_stylesheet_free(CSSStylesheet* stylesheet);

// Merge another stylesheet into target (rules from 'other' are appended)
// This allows loading multiple CSS files and combining them
void ir_css_stylesheet_merge(CSSStylesheet* target, CSSStylesheet* other);

// Check if a selector matches an element's tag name
// selector: "div", "button", "span"
// tag_name: element's tag (e.g., "div")
bool ir_css_selector_matches_element(const char* selector, const char* tag_name);

// Check if a selector matches an element's class name
// selector: ".button" or ".row"
// class_name: "button primary" (space-separated classes)
bool ir_css_selector_matches_class(const char* selector, const char* class_name);

// Check if a selector matches an element's ID
// selector: "#header"
// id: "header"
bool ir_css_selector_matches_id(const char* selector, const char* id);

// Apply all matching rules from stylesheet to an element
// tag_name: element's HTML tag (e.g., "div", "span")
// class_name: element's class attribute (e.g., "button primary")
// id: element's id attribute (e.g., "header")
// style: IR style to populate
// layout: IR layout to populate
void ir_css_apply_stylesheet_rules(
    CSSStylesheet* stylesheet,
    const char* tag_name,
    const char* class_name,
    const char* id,
    IRStyle* style,
    IRLayout* layout
);

// Debug: Print stylesheet contents
void ir_css_stylesheet_print(CSSStylesheet* stylesheet);

// ============================================================================
// IRStylesheet Conversion (Bridge to new stylesheet system)
// ============================================================================

// Forward declarations for ir_stylesheet types
typedef struct IRStylesheet IRStylesheet;
typedef struct IRStyleProperties IRStyleProperties;

// Convert CSSProperty array to IRStyleProperties
// Bridges existing CSS parsing with new IRStyleProperties structure
void ir_css_to_style_properties(const CSSProperty* props, uint32_t count, IRStyleProperties* out);

// Convert a CSSStylesheet to IRStylesheet
// Creates a new IRStylesheet with parsed selectors and style properties
// Returns NULL on failure. Caller must free with ir_stylesheet_free()
IRStylesheet* ir_css_stylesheet_to_ir_stylesheet(CSSStylesheet* css_stylesheet);

// CSS Variable Functions
// Resolve var(--name) references in a value string
// Returns newly allocated string with variables resolved, or NULL if no variables
// Caller must free the returned string
char* ir_css_resolve_variables(CSSStylesheet* stylesheet, const char* value);

// Get a CSS variable value by name (without -- prefix)
const char* ir_css_get_variable(CSSStylesheet* stylesheet, const char* name);

// Add a CSS variable to the stylesheet
void ir_css_add_variable(CSSStylesheet* stylesheet, const char* name, const char* value);

// Add a media query to the stylesheet
void ir_css_add_media_query(CSSStylesheet* stylesheet, const char* condition, const char* css_content);

#endif // IR_CSS_STYLESHEET_H
