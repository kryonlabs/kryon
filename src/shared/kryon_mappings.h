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
#include <stddef.h>
 
 //==============================================================================
// Type Hint System
//==============================================================================

// Property category indices - use enum for type safety
typedef enum {
    KRYON_CATEGORY_BASE = 0,          // Core properties (id, class, style)
    KRYON_CATEGORY_LAYOUT,            // Layout and positioning (width, height, padding)
    KRYON_CATEGORY_VISUAL,            // Visual appearance (background, color, border)
    KRYON_CATEGORY_TYPOGRAPHY,        // Text and font properties (fontSize, textAlign)
    KRYON_CATEGORY_TRANSFORM,         // Transform and animation properties
    KRYON_CATEGORY_INTERACTIVE,       // User interaction properties (onClick, cursor)
    KRYON_CATEGORY_ELEMENT_SPECIFIC,  // Element-specific properties
    KRYON_CATEGORY_WINDOW,            // Window management properties
    KRYON_CATEGORY_CHECKBOX,          // Checkbox-specific properties
    KRYON_CATEGORY_DIRECTIVE,         // Control flow directive properties

    KRYON_CATEGORY_COUNT              // Total number of categories
} KryonPropertyCategoryIndex;

// Element category indices
typedef enum {
    KRYON_ELEM_CATEGORY_BASE = 0,           // Base element
    KRYON_ELEM_CATEGORY_LAYOUT,             // Layout elements (Column, Row, Container)
    KRYON_ELEM_CATEGORY_CONTENT,            // Content elements (Text, Button, Image)
    KRYON_ELEM_CATEGORY_TEXT,               // Text elements
    KRYON_ELEM_CATEGORY_INTERACTIVE,        // Interactive elements (Button, Input)
    KRYON_ELEM_CATEGORY_APPLICATION,        // Application elements (App, Modal)

    KRYON_ELEM_CATEGORY_COUNT              // Total number of element categories
} KryonElementCategoryIndex;


 
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
// Hierarchical Category System (Phase 3 Implementation)
//==============================================================================

// Property category structure - defines property inheritance
typedef struct {
    const char *name;               // Category name (e.g., "Layout", "Visual", "Interactive")
    uint16_t range_start;          // Hex code range start (e.g., 0x0100)
    uint16_t range_end;            // Hex code range end (e.g., 0x01FF)
    uint16_t *inherited_ranges;    // NULL-terminated array of inherited category ranges
    const char *description;       // Human-readable description
} KryonPropertyCategory;

// Element category structure - defines element inheritance
typedef struct {
    const char *name;               // Category name (e.g., "Layout", "Content", "Interactive")
    uint16_t range_start;          // Hex code range start (e.g., 0x0001)
    uint16_t range_end;            // Hex code range end (e.g., 0x00FF)
    uint16_t *inherited_ranges;    // NULL-terminated array of inherited category ranges
    uint16_t *valid_properties;    // NULL-terminated array of valid property hex codes
    const char *description;       // Human-readable description
} KryonElementCategory;

// Property group structure - simplified without redundant category_index
typedef struct {
    const char *canonical;          // Canonical property name
    const char **aliases;           // NULL-terminated array of aliases
    uint16_t hex_code;             // 16-bit property identifier
    KryonValueTypeHint type_hint;  // Type hint for validation
} KryonPropertyGroup;

// Element group structure - clean property registration system
typedef struct {
    const char *canonical;          // Canonical element name
    const char **aliases;           // NULL-terminated array of aliases
    uint16_t hex_code;             // 16-bit element identifier
    KryonValueTypeHint type_hint;  // Type hint for validation
    KryonPropertyCategoryIndex *allowed_categories; // NULL-terminated array of allowed property categories
} KryonElementGroup;

// Syntax keyword group structure - for @directives and special syntax
typedef struct {
    const char *canonical;          // Canonical syntax keyword name
    const char **aliases;           // NULL-terminated array of aliases
    uint16_t hex_code;             // 16-bit syntax identifier
    KryonValueTypeHint type_hint;  // Type hint for validation
} KryonSyntaxGroup;
 
  //==============================================================================
 // Enhanced Performance System (Phase 1 Implementation)
 //==============================================================================

// Optimized hash table entry with alias tracking
typedef struct {
    const char *name;       // Property name (canonical or alias)
    uint16_t hex_code;      // Property hex code
    uint16_t group_index;   // Index to original group for type hints
    bool is_alias;          // Flag to distinguish aliases from canonical names
} KryonPropertyHashEntry;

// Internal hash table structure (private to kryon_mappings.c)
typedef struct {
    KryonPropertyHashEntry *entries; // Hash table entries
    uint32_t size;                  // Hash table size (power of 2)
    uint32_t count;                 // Number of entries
    bool initialized;              // Initialization flag
} KryonPropertyHashTable;

// String table for deduplication
typedef struct {
    const char **strings;   // Array of unique strings
    uint32_t count;         // Number of strings
    uint32_t capacity;      // Capacity of strings array
} KryonStringTable;

//==============================================================================
// External Data Declarations
//==============================================================================

// Property categories - define inheritance hierarchies
extern const KryonPropertyCategory kryon_property_categories[];

// Element categories - define element inheritance hierarchies
extern const KryonElementCategory kryon_element_categories[];

// Centralized property groups - used by both compiler and runtime
extern const KryonPropertyGroup kryon_property_groups[];

// Separated property arrays by category for better organization
extern const KryonPropertyGroup kryon_base_properties[];
extern const KryonPropertyGroup kryon_layout_properties[];
extern const KryonPropertyGroup kryon_visual_properties[];
extern const KryonPropertyGroup kryon_typography_properties[];
extern const KryonPropertyGroup kryon_transform_properties[];
extern const KryonPropertyGroup kryon_interactive_properties[];
extern const KryonPropertyGroup kryon_element_specific_properties[];
extern const KryonPropertyGroup kryon_window_properties[];
extern const KryonPropertyGroup kryon_checkbox_properties[];
extern const KryonPropertyGroup kryon_directive_properties[];

// Centralized element groups - used by both compiler and runtime
extern const KryonElementGroup kryon_element_groups[];

// Separated element arrays by category
extern const KryonElementGroup kryon_base_elements[];
extern const KryonElementGroup kryon_layout_elements[];
extern const KryonElementGroup kryon_content_elements[];
extern const KryonElementGroup kryon_text_elements[];
extern const KryonElementGroup kryon_interactive_elements[];
extern const KryonElementGroup kryon_application_elements[];

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
// Enhanced Validation Functions (Phase 2 Implementation)
//==============================================================================

/**
 * @brief Check if a property name is an alias (for KRY→KRB compilation only)
 * @param name Property name to check
 * @return true if alias, false if canonical or not found
 */
bool kryon_is_property_alias(const char *name);

/**
 * @brief Check if a property is valid for a specific element
 * @param element_hex Element hex code
 * @param property_hex Property hex code
 * @return true if property is valid for the element
 */
bool kryon_is_valid_property_for_element(uint16_t element_hex, uint16_t property_hex);

/**
 * @brief Test function to demonstrate the enhanced mappings system
 */
void kryon_mappings_test(void);

/**
 * @brief Quick test of the mappings system (for debugging)
 */
void kryon_mappings_quick_test(void);

/**
 * @brief Test properties specifically used in button example
 */
void kryon_mappings_test_button_example(void);

/**
 * @brief Test the new category-based hierarchical system
 */
void kryon_category_system_test(void);

/**
 * @brief Test checkbox property validation specifically
 */
void kryon_test_checkbox_validation(void);

//==============================================================================
// Category-Based Functions (Phase 3 Implementation)
//==============================================================================

/**
 * @brief Check if a property is valid for an element using category inheritance
 * @param element_hex Element hex code
 * @param property_hex Property hex code
 * @return true if property is valid for the element through inheritance
 */
bool kryon_is_valid_property_for_element_category(uint16_t element_hex, uint16_t property_hex);

/**
 * @brief Get all valid properties for an element through category inheritance
 * @param element_hex Element hex code
 * @param properties_out Array to store valid property hex codes
 * @param max_properties Maximum number of properties to return
 * @return Number of valid properties found
 */
size_t kryon_get_element_valid_properties(uint16_t element_hex, uint16_t *properties_out, size_t max_properties);

/**
 * @brief Get category name for a property
 * @param property_hex Property hex code
 * @return Category name string
 */
const char *kryon_get_property_category_name(uint16_t property_hex);

//==============================================================================
// CLEAN PROPERTY REGISTRATION SYSTEM
//==============================================================================

/**
 * @brief Check if an element allows a property (simple category-based validation)
 * @param element_hex Element hex code
 * @param property_hex Property hex code
 * @return true if element allows the property based on categories
 */
bool kryon_element_allows_property(uint16_t element_hex, uint16_t property_hex);

/**
 * @brief Get list of allowed categories for an element
 * @param element_hex Element hex code
 * @return Array of allowed categories, NULL-terminated with KRYON_CATEGORY_COUNT
 */
const KryonPropertyCategoryIndex* kryon_get_element_allowed_categories(uint16_t element_hex);

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
 