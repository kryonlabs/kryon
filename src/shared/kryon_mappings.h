/**
 * @file kryon_mappings.h
 * @brief Unified Mapping System - Properties and Elements for KRY Compiler and Runtime
 * @details This header defines the public interface for the Kryon mapping system.
 *          It includes type definitions, enums, and function prototypes for resolving
 *          property, element, and syntax names to their corresponding hex codes.
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
 // Type Hint and Category Enums
 //==============================================================================
 
 // Property category indices - used for data-driven validation.
 typedef enum {
     KRYON_CATEGORY_BASE = 0,
     KRYON_CATEGORY_LAYOUT,
     KRYON_CATEGORY_VISUAL,
     KRYON_CATEGORY_TYPOGRAPHY,
     KRYON_CATEGORY_TRANSFORM,
     KRYON_CATEGORY_INTERACTIVE,
     KRYON_CATEGORY_ELEMENT_SPECIFIC,
     KRYON_CATEGORY_WINDOW,
     KRYON_CATEGORY_CHECKBOX,
     KRYON_CATEGORY_DIRECTIVE,
     KRYON_CATEGORY_COUNT // Sentinel value, marks the end of lists
 } KryonPropertyCategoryIndex;
 
 // Value type hints for property validation and conversion.
 typedef enum {
     KRYON_TYPE_HINT_ANY,
     KRYON_TYPE_HINT_STRING,
     KRYON_TYPE_HINT_INTEGER,
     KRYON_TYPE_HINT_FLOAT,
     KRYON_TYPE_HINT_BOOLEAN,
     KRYON_TYPE_HINT_COLOR,
     KRYON_TYPE_HINT_DIMENSION,
     KRYON_TYPE_HINT_SPACING,
     KRYON_TYPE_HINT_REFERENCE,
     KRYON_TYPE_HINT_COMPONENT,
     KRYON_TYPE_HINT_ARRAY,
     KRYON_TYPE_HINT_UNIT
 } KryonValueTypeHint;
 
 //==============================================================================
 // Core Data Structures
 //==============================================================================
 
 // Defines a property's characteristics. This is the single source of truth.
 typedef struct {
     const char *canonical;
     const char **aliases;
     uint16_t hex_code;
     KryonValueTypeHint type_hint;
     bool inheritable;  // Whether this property inherits from parent elements (like CSS)
 } KryonPropertyGroup;
 
 // Defines an element and which categories of properties it is allowed to use.
 typedef struct {
     const char *canonical;
     const char **aliases;
     uint16_t hex_code;
     KryonValueTypeHint type_hint; // Added this missing field
     const KryonPropertyCategoryIndex* allowed_categories;
 } KryonElementGroup;
 
 // Defines a syntax keyword (e.g., @for, @if).
 typedef struct {
     const char *canonical;
     const char **aliases;
     uint16_t hex_code;
     KryonValueTypeHint type_hint;
 } KryonSyntaxGroup;
 
 // Defines the metadata for a property category.
 typedef struct {
     const char *name;
     uint16_t range_start;
     uint16_t range_end;
     const char *description; // Removed unused 'inherited_ranges'
 } KryonPropertyCategory;
 
 
 //==============================================================================
// Element Range Constants (for compatibility and validation)
//==============================================================================

// Custom Elements (user-defined)
#define KRYON_ELEMENT_RANGE_CUSTOM_START   0x2000
#define KRYON_ELEMENT_RANGE_CUSTOM_END     0x7FFF

//==============================================================================
// External Data Declarations
//==============================================================================

// Property categories - define inheritance hierarchies
extern const KryonPropertyCategory kryon_property_categories[];

//==============================================================================
// Internal Structures (for context, implementation is in .c file)
//==============================================================================
 
 // Optimized hash table entry with direct pointer to the source property definition.
 typedef struct {
     const char *name;
     uint16_t hex_code;
     bool is_alias;
     const KryonPropertyGroup* group_ptr; // Changed from group_index to a direct pointer
 } KryonPropertyHashEntry;
 
 // Internal hash table structure. Not meant for direct use outside kryon_mappings.c.
 typedef struct {
     KryonPropertyHashEntry *entries;
     uint32_t size;
     uint32_t count;
 } KryonPropertyHashTable;
 
 
 //==============================================================================
 // Public API Functions
 //==============================================================================
 
 /**
  * @brief Get property hex code from its name (canonical or alias).
  * @details Uses a high-performance hash table for O(1) average lookup time.
  * @param name The property name string (e.g., "width", "bgColor").
  * @return 16-bit hex code, or 0 if not found.
  */
 uint16_t kryon_get_property_hex(const char *name);
 
 /**
  * @brief Get the canonical property name from its hex code.
  * @param hex_code 16-bit property identifier.
  * @return A const pointer to the canonical name string, or NULL if not found.
  */
 const char *kryon_get_property_name(uint16_t hex_code);
 
 /**
  * @brief Get the type hint for a property from its hex code.
  * @param hex_code 16-bit property identifier.
  * @return The KryonValueTypeHint enum for the property.
  */
 KryonValueTypeHint kryon_get_property_type_hint(uint16_t hex_code);
 
 /**
  * @brief Get element hex code from its name (canonical or alias).
  * @param name The element name string (e.g., "Button", "Container").
  * @return 16-bit hex code, or 0 if not found.
  */
 uint16_t kryon_get_element_hex(const char *name);
 
 /**
  * @brief Get the canonical element name from its hex code.
  * @param hex_code 16-bit element identifier.
  * @return A const pointer to the canonical name string, or NULL if not found.
  */
 const char *kryon_get_element_name(uint16_t hex_code);
 
 /**
  * @brief Get syntax keyword hex code from its name (canonical or alias).
  * @param name The syntax keyword string (e.g., "for", "if").
  * @return 16-bit hex code, or 0 if not found.
  */
 uint16_t kryon_get_syntax_hex(const char *name);
 
 /**
  * @brief Get the canonical syntax keyword name from its hex code.
  * @param hex_code 16-bit syntax identifier.
  * @return A const pointer to the canonical name string, or NULL if not found.
  */
 const char *kryon_get_syntax_name(uint16_t hex_code);
 
 /**
  * @brief Check if a property name is an alias.
  * @param name The property name to check.
  * @return true if the name is an alias, false if it's canonical or not found.
  */
 bool kryon_is_property_alias(const char *name);
 
 /**
 * @brief Validates if a property is allowed for a given element.
 * @details This function uses the data-driven `allowed_categories` list
 *          defined for each element, making it flexible and easy to maintain.
 * @param element_hex The 16-bit hex code of the element.
 * @param property_hex The 16-bit hex code of the property.
 * @return true if the property is valid for the element, false otherwise.
 */
bool kryon_is_valid_property_for_element(uint16_t element_hex, uint16_t property_hex);

/**
 * @brief Get the list of allowed property categories for an element.
 * @param element_hex The 16-bit hex code of the element.
 * @return A const pointer to an array of allowed categories, NULL-terminated with KRYON_CATEGORY_COUNT.
 */
const KryonPropertyCategoryIndex* kryon_get_element_allowed_categories(uint16_t element_hex);
 
 /**
 * @brief Check if hex code is in a specific element range
 * @param hex_code Element hex code to check
 * @param range_start Start of range (inclusive)
 * @param range_end End of range (inclusive)
 * @return true if in range, false otherwise
 */
bool kryon_element_in_range(uint16_t hex_code, uint16_t range_start, uint16_t range_end);

/**
 * @brief Check if a property is inheritable (like CSS inheritance)
 * @param property_hex The 16-bit hex code of the property
 * @return true if the property inherits from parent elements, false otherwise
 */
bool kryon_is_property_inheritable(uint16_t property_hex);

/**
 * @brief Runs a comprehensive test suite on the mapping system.
 * @details Initializes the system and prints test results to stdout.
 */
void kryon_mappings_test(void);
 
 #endif // KRYON_MAPPINGS_H