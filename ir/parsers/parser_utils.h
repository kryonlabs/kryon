/**
 * Parser Utilities
 * Shared utilities for all parsers to reduce code duplication
 */

#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include "../include/ir_core.h"
#include "ir_log.h"
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Error Macros
// ============================================================================

#define PARSER_ERROR(fmt, ...) IR_LOG_ERROR("PARSER", fmt, ##__VA_ARGS__)
#define PARSER_WARN(fmt, ...)  IR_LOG_WARN("PARSER", fmt, ##__VA_ARGS__)
#define PARSER_INFO(fmt, ...)  IR_LOG_INFO("PARSER", fmt, ##__VA_ARGS__)
#define PARSER_DEBUG(fmt, ...) IR_LOG_DEBUG("PARSER", fmt, ##__VA_ARGS__)

// ============================================================================
// Component Creation Helpers
// ============================================================================

/**
 * Create a component of the given type with default settings
 */
IRComponent* parser_create_component(IRComponentType type);

/**
 * Set the text content of a component
 * Returns true on success, false on failure
 */
bool parser_set_component_text(IRComponent* comp, const char* text);

/**
 * Add a child component to a parent
 * Returns true on success, false on failure
 */
bool parser_add_child_component(IRComponent* parent, IRComponent* child);

// ============================================================================
// Style Parsing Helpers
// ============================================================================

/**
 * Parse a color string into RGBA components
 * Supports formats: #RGB, #RGBA, #RRGGBB, #RRGGBBAA, named colors
 * Returns true on success, false on failure
 */
bool parser_parse_color(const char* color_str, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);

/**
 * Parse a dimension string into type and value
 * Supports formats: "100px", "50%", "auto", "10.5em"
 * Returns true on success, false on failure
 */
bool parser_parse_dimension(const char* dim_str, IRDimensionType* type, float* value);

/**
 * Parse a pixel value string like "200.0px" or "200px"
 * Returns the numeric value, or 0.0f on failure
 */
float parser_parse_px_value(const char* str);

/**
 * Check if a color string represents a transparent color
 * Returns true for "#00000000", "transparent", etc.
 */
bool parser_is_transparent_color(const char* color);

/**
 * Parse a color string into a packed uint32_t (RGBA format)
 * Returns color as (r << 24) | (g << 16) | (b << 8) | a
 * Returns 0x000000FF (opaque black) on failure
 */
uint32_t parser_parse_color_packed(const char* color_str);

// ============================================================================
// String Utilities
// ============================================================================

/**
 * Case-insensitive string comparison
 * Returns true if strings are equal (ignoring case)
 */
bool parser_str_equal_ignore_case(const char* a, const char* b);

/**
 * Trim leading and trailing whitespace from a string
 * Returns a newly allocated string (caller must free)
 */
char* parser_trim_whitespace(const char* str);

/**
 * Check if a string starts with a prefix
 */
bool parser_starts_with(const char* str, const char* prefix);

/**
 * Check if a string ends with a suffix
 */
bool parser_ends_with(const char* str, const char* suffix);

#endif // PARSER_UTILS_H
