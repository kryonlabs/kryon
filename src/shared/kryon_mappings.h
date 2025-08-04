/**
 * @file kryon_mappings.h
 * @brief Unified Mapping System - Properties and Widgets for KRY Compiler and Runtime
 * @details This file serves as the single source of truth for ALL hex code assignments:
 *          - Property mappings (0x0000-0x0FFF): posX, width, color, padding, etc.
 *          - Widget mappings (0x1000+): Container, Text, Button, App, etc.
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
     KRYON_TYPE_HINT_REFERENCE,   // Should be treated as widget reference
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
 
 //==============================================================================
 // External Data Declarations
 //==============================================================================
 
 // Centralized property groups - used by both compiler and runtime
 extern const KryonPropertyGroup kryon_property_groups[];
 
 // Centralized element groups - used by both compiler and runtime
 extern const KryonElementGroup kryon_element_groups[];
 
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
  * @brief Get widget hex code from name (canonical or alias)
  * @param name Widget name to lookup
  * @return 16-bit hex code, or 0 if not found
  */
 uint16_t kryon_get_element_hex(const char *name);
 
 /**
  * @brief Get canonical widget name from hex code
  * @param hex_code 16-bit widget identifier
  * @return Canonical widget name, or NULL if not found
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
  * @brief Get all aliases for a widget (useful for IDE/tooling)
  * @param name Widget name (canonical or alias)
  * @return NULL-terminated array of aliases, or NULL if not found
  */
 const char **kryon_get_element_aliases(const char *name);
 
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
 
 // Widget-Specific Properties
 #define KRYON_PROPERTY_RANGE_WIDGET_START  0x0600
 #define KRYON_PROPERTY_RANGE_WIDGET_END    0x06FF
 
 // Window Properties
 #define KRYON_PROPERTY_RANGE_WINDOW_START  0x0700
 #define KRYON_PROPERTY_RANGE_WINDOW_END    0x07FF
 
 //==============================================================================
 // Widget Range Constants (from KRB Binary Format Specification v0.1)
 //==============================================================================
 
 // Base Widget
 #define KRYON_WIDGET_BASE                  0x0000
 
 // Layout Widgets
 #define KRYON_WIDGET_RANGE_LAYOUT_START    0x0001
 #define KRYON_WIDGET_RANGE_LAYOUT_END      0x00FF
 
 // Content Widgets  
 #define KRYON_WIDGET_RANGE_CONTENT_START   0x0400
 #define KRYON_WIDGET_RANGE_CONTENT_END     0x04FF
 
 // Application Widgets
 #define KRYON_WIDGET_RANGE_APPLICATION_START 0x1000
 #define KRYON_WIDGET_RANGE_APPLICATION_END   0x1FFF
 
 // Custom Widgets (user-defined)
 #define KRYON_WIDGET_RANGE_CUSTOM_START    0x2000
 #define KRYON_WIDGET_RANGE_CUSTOM_END      0xFFFF
 
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
  * @brief Check if hex code is in a specific widget range
  * @param hex_code Widget hex code to check
  * @param range_start Start of range (inclusive)
  * @param range_end End of range (inclusive)
  * @return true if in range, false otherwise
  */
 static inline bool kryon_widget_in_range(uint16_t hex_code, uint16_t range_start, uint16_t range_end) {
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
  * @brief Check if widget is a layout widget
  * @param hex_code Widget hex code
  * @return true if layout widget, false otherwise
  */
 static inline bool kryon_is_layout_widget(uint16_t hex_code) {
     return kryon_widget_in_range(hex_code, KRYON_WIDGET_RANGE_LAYOUT_START, KRYON_WIDGET_RANGE_LAYOUT_END);
 }
 
 /**
  * @brief Check if widget is a content widget
  * @param hex_code Widget hex code
  * @return true if content widget, false otherwise
  */
 static inline bool kryon_is_content_widget(uint16_t hex_code) {
     return kryon_widget_in_range(hex_code, KRYON_WIDGET_RANGE_CONTENT_START, KRYON_WIDGET_RANGE_CONTENT_END);
 }
 
 /**
 * @brief Get widget type name from hex code (unified API)
 * @param hex_code 16-bit widget hex code
 * @return Widget type name, or NULL if not found
 */
const char *kryon_get_element_type_name(uint16_t hex_code);

#endif // KRYON_MAPPINGS_H
 