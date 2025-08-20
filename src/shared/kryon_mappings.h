/**
 * @file kryon_mappings.h
 * @brief Unified Mapping System - Properties and Elements for KRY Compiler and Runtime
 * @details This file serves as the single source of truth for ALL hex code assignments:
 *          - Property mappings (0x0000-0x0FFF): posX, width, color, padding, etc.
 *          - Element mappings (0x1000+): Container, Text, Button, App, etc.
 *          Uses grouped structure with canonical names and aliases for maintainability.
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

 #ifndef KRYON_MAPPINGS_H
 #define KRYON_MAPPINGS_H
 
 #include <stdint.h>
 #include <stdbool.h>
 
 //==============================================================================
 // Type Hint System
 //==============================================================================
 
 // Value type hints for property validation and conversion
 typedef enum {
     KRYON_TYPE_HINT_ANY,         // No specific type conversion needed
     KRYON_TYPE_HINT_STRING,      // Should be treated as string
     KRYON_TYPE_HINT_INTEGER,     // Should be treated as integer
     KRYON_TYPE_HINT_FLOAT,       // Should be treated as float
     KRYON_TYPE_HINT_BOOLEAN,     // Should be treated as boolean
     KRYON_TYPE_HINT_COLOR,       // Should be parsed as color (#RRGGBB, rgb(), etc.)
     KRYON_TYPE_HINT_DIMENSION,   // Should be treated as dimension (px, %, em, rem, etc.)
     KRYON_TYPE_HINT_SPACING,     // Should be treated as spacing value (TRBL format)
     KRYON_TYPE_HINT_REFERENCE,   // Should be treated as element reference
     KRYON_TYPE_HINT_COMPONENT,   // Should be treated as component reference
     KRYON_TYPE_HINT_ARRAY,       // Should be treated as array of values
     KRYON_TYPE_HINT_UNIT         // Should be treated as unit value (legacy)
 } KryonValueTypeHint;
 
 //==============================================================================
 // Grouped Structure Definitions
 //==============================================================================
 
 // Property group structure - canonical name with aliases
 typedef struct {
     const char *canonical;          // Canonical property name
     const char **aliases;           // NULL-terminated array of aliases
     uint16_t hex_code;             // 16-bit property identifier
     KryonValueTypeHint type_hint;  // Type hint for validation
 } KryonPropertyGroup;
 
 // Element group structure - canonical name with aliases  
 typedef struct {
     const char *canonical;          // Canonical element name
     const char **aliases;           // NULL-terminated array of aliases
     uint16_t hex_code;             // 16-bit element identifier
     KryonValueTypeHint type_hint;  // Type hint for validation
 } KryonElementGroup;

// Syntax keyword group structure - for @directives and special syntax
typedef struct {
    const char *canonical;          // Canonical syntax keyword name
    const char **aliases;           // NULL-terminated array of aliases
    uint16_t hex_code;             // 16-bit syntax identifier
    KryonValueTypeHint type_hint;  // Type hint for validation
} KryonSyntaxGroup;
 
 //==============================================================================
 // External Data Declarations
 //==============================================================================
 
 // Centralized property groups - used by both compiler and runtime
 extern const KryonPropertyGroup kryon_property_groups[];
 
 // Centralized element groups - used by both compiler and runtime
 extern const KryonElementGroup kryon_element_groups[];

// Centralized syntax keyword groups - used by both compiler and runtime
extern const KryonSyntaxGroup kryon_syntax_groups[];
 
 //==============================================================================
 // Core Utility Functions
 //==============================================================================
 
 /**
  * @brief Get property hex code from name (canonical or alias)
  * @param name Property name to lookup
  * @return 16-bit hex code, or 0 if not found
  */
 uint16_t kryon_get_property_hex(const char *name);
 
 /**
  * @brief Get canonical property name from hex code
  * @param hex_code 16-bit property identifier
  * @return Canonical property name, or NULL if not found
  */
 const char *kryon_get_property_name(uint16_t hex_code);
 
 /**
  * @brief Get property type hint from hex code
  * @param hex_code 16-bit property identifier
  * @return Type hint for the property
  */
 KryonValueTypeHint kryon_get_property_type_hint(uint16_t hex_code);
 
 /**
  * @brief Get element hex code from name (canonical or alias)
  * @param name Element name to lookup
  * @return 16-bit hex code, or 0 if not found
  */
 uint16_t kryon_get_element_hex(const char *name);
 
 /**
  * @brief Get canonical element name from hex code
  * @param hex_code 16-bit element identifier
  * @return Canonical element name, or NULL if not found
  */
 const char *kryon_get_element_name(uint16_t hex_code);
 
 //==============================================================================
 // Extended Utility Functions
 //==============================================================================
 
 /**
  * @brief Get all aliases for a property (useful for IDE/tooling)
  * @param name Property name (canonical or alias)
  * @return NULL-terminated array of aliases, or NULL if not found
  */
 const char **kryon_get_property_aliases(const char *name);
 
 /**
  * @brief Get all aliases for an element (useful for IDE/tooling)
  * @param name Element name (canonical or alias)
  * @return NULL-terminated array of aliases, or NULL if not found
  */
 const char **kryon_get_element_aliases(const char *name);

/**
 * @brief Get syntax keyword hex code from name (canonical or alias)
 * @param name Syntax keyword name to lookup
 * @return 16-bit hex code, or 0 if not found
 */
uint16_t kryon_get_syntax_hex(const char *name);

/**
 * @brief Get canonical syntax keyword name from hex code
 * @param hex_code 16-bit syntax identifier
 * @return Canonical syntax keyword name, or NULL if not found
 */
const char *kryon_get_syntax_name(uint16_t hex_code);

/**
 * @brief Get all aliases for a syntax keyword (useful for IDE/tooling)
 * @param name Syntax keyword name (canonical or alias)
 * @return NULL-terminated array of aliases, or NULL if not found
 */
const char **kryon_get_syntax_aliases(const char *name);
 
 //==============================================================================
 // Property Range Constants (from KRB Binary Format Specification v0.1)
 //==============================================================================
 
 // Meta & System Properties
 #define KRYON_PROPERTY_RANGE_META_START    0x0000
 #define KRYON_PROPERTY_RANGE_META_END      0x00FF
 
 // Layout Properties  
 #define KRYON_PROPERTY_RANGE_LAYOUT_START  0x0100
 #define KRYON_PROPERTY_RANGE_LAYOUT_END    0x01FF
 
 // Visual Properties
 #define KRYON_PROPERTY_RANGE_VISUAL_START  0x0200
 #define KRYON_PROPERTY_RANGE_VISUAL_END    0x02FF
 
 // Typography Properties
 #define KRYON_PROPERTY_RANGE_TYPOGRAPHY_START 0x0300
 #define KRYON_PROPERTY_RANGE_TYPOGRAPHY_END   0x03FF
 
 // Transform & Animation Properties
 #define KRYON_PROPERTY_RANGE_TRANSFORM_START 0x0400
 #define KRYON_PROPERTY_RANGE_TRANSFORM_END   0x04FF
 
 // Interaction & Event Properties
 #define KRYON_PROPERTY_RANGE_INTERACTION_START 0x0500
 #define KRYON_PROPERTY_RANGE_INTERACTION_END   0x05FF
 
 // Element-Specific Properties
 #define KRYON_PROPERTY_RANGE_ELEMENT_START 0x0600
 #define KRYON_PROPERTY_RANGE_ELEMENT_END   0x06FF
 
 // Window Properties
 #define KRYON_PROPERTY_RANGE_WINDOW_START  0x0700
 #define KRYON_PROPERTY_RANGE_WINDOW_END    0x07FF
 
 //==============================================================================
 // Element Range Constants (from KRB Binary Format Specification v0.1)
 //==============================================================================
 
 // Base Element
 #define KRYON_ELEMENT_BASE                 0x0000
 
 // Layout Elements
 #define KRYON_ELEMENT_RANGE_LAYOUT_START    0x0001
 #define KRYON_ELEMENT_RANGE_LAYOUT_END     0x00FF
 
 // Content Elements  
 #define KRYON_ELEMENT_RANGE_CONTENT_START  0x0400
 #define KRYON_ELEMENT_RANGE_CONTENT_END    0x04FF
 
 // Application Elements
 #define KRYON_ELEMENT_RANGE_APPLICATION_START 0x1000
 #define KRYON_ELEMENT_RANGE_APPLICATION_END  0x1FFF
 
 // Custom Elements (user-defined)
 #define KRYON_ELEMENT_RANGE_CUSTOM_START   0x2000
 #define KRYON_ELEMENT_RANGE_CUSTOM_END     0x7FFF

//==============================================================================
// Syntax Keyword Range Constants
//==============================================================================

// Syntax Keywords and Directives
#define KRYON_SYNTAX_RANGE_START           0x8000
#define KRYON_SYNTAX_RANGE_END             0x8FFF
 
 //==============================================================================
 // Validation Helpers
 //==============================================================================
 
 /**
  * @brief Check if hex code is in a specific property range
  * @param hex_code Property hex code to check
  * @param range_start Start of range (inclusive)
  * @param range_end End of range (inclusive)
  * @return true if in range, false otherwise
  */
 static inline bool kryon_property_in_range(uint16_t hex_code, uint16_t range_start, uint16_t range_end) {
     return hex_code >= range_start && hex_code <= range_end;
 }
 
 /**
  * @brief Check if hex code is in a specific element range
  * @param hex_code Element hex code to check
  * @param range_start Start of range (inclusive)
  * @param range_end End of range (inclusive)
  * @return true if in range, false otherwise
  */
 static inline bool kryon_element_in_range(uint16_t hex_code, uint16_t range_start, uint16_t range_end) {
     return hex_code >= range_start && hex_code <= range_end;
 }
 
 /**
  * @brief Check if property is a layout property
  * @param hex_code Property hex code
  * @return true if layout property, false otherwise
  */
 static inline bool kryon_is_layout_property(uint16_t hex_code) {
     return kryon_property_in_range(hex_code, KRYON_PROPERTY_RANGE_LAYOUT_START, KRYON_PROPERTY_RANGE_LAYOUT_END);
 }
 
 /**
  * @brief Check if property is a visual property
  * @param hex_code Property hex code
  * @return true if visual property, false otherwise
  */
 static inline bool kryon_is_visual_property(uint16_t hex_code) {
     return kryon_property_in_range(hex_code, KRYON_PROPERTY_RANGE_VISUAL_START, KRYON_PROPERTY_RANGE_VISUAL_END);
 }
 
 /**
  * @brief Check if element is a layout element
  * @param hex_code Element hex code
  * @return true if layout element, false otherwise
  */
 static inline bool kryon_is_layout_element(uint16_t hex_code) {
     return kryon_element_in_range(hex_code, KRYON_ELEMENT_RANGE_LAYOUT_START, KRYON_ELEMENT_RANGE_LAYOUT_END);
 }
 
 /**
  * @brief Check if element is a content element
  * @param hex_code Element hex code
  * @return true if content element, false otherwise
  */
 static inline bool kryon_is_content_element(uint16_t hex_code) {
     return kryon_element_in_range(hex_code, KRYON_ELEMENT_RANGE_CONTENT_START, KRYON_ELEMENT_RANGE_CONTENT_END);
 }
 
 /**
 * @brief Get element type name from hex code (unified API)
 * @param hex_code 16-bit element hex code
 * @return Element type name, or NULL if not found
 */
const char *kryon_get_element_type_name(uint16_t hex_code);

/**
 * @brief Check if hex code is a syntax keyword
 * @param hex_code Syntax hex code
 * @return true if syntax keyword, false otherwise
 */
static inline bool kryon_is_syntax_keyword(uint16_t hex_code) {
    return hex_code >= KRYON_SYNTAX_RANGE_START && hex_code <= KRYON_SYNTAX_RANGE_END;
}

#endif // KRYON_MAPPINGS_H
 