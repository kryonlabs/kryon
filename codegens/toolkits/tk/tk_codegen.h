/**
 * @file tk_codegen.h
 * @brief Tk Toolkit Code Generator
 *
 * Toolkit-specific code generation for Tcl/Tk.
 * Handles Tk widget types, properties, layout, and events.
 */

#ifndef TK_CODEGEN_H
#define TK_CODEGEN_H

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct cJSON;
typedef struct cJSON cJSON;
struct WIRWidget;

/**
 * Generate Tk widget creation command
 * @param output Output file
 * @param widget WIR widget
 * @return true on success, false on failure
 */
bool tk_generate_widget_creation(FILE* output, struct WIRWidget* widget);

/**
 * Generate Tk property assignment
 * @param output Output file
 * @param widget_id Widget ID
 * @param property Property name
 * @param value Property value (cJSON)
 * @return true on success, false on failure
 */
bool tk_generate_property_assignment(FILE* output, const char* widget_id,
                                     const char* property, cJSON* value);

/**
 * Generate Tk layout command
 * @param output Output file
 * @param layout WIR layout options
 * @return true on success, false on failure
 */
bool tk_generate_layout_command(FILE* output, void* layout);

/**
 * Generate Tk event binding
 * @param output Output file
 * @param widget_id Widget ID
 * @param event Event name
 * @param handler Handler code
 * @return true on success, false on failure
 */
bool tk_generate_event_binding(FILE* output, const char* widget_id,
                                const char* event, const char* handler);

#ifdef __cplusplus
}
#endif

#endif // TK_CODEGEN_H
