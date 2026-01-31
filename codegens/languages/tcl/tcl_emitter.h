/**
 * Kryon Tcl Language Emitter
 *
 * Generates Tcl syntax from toolkit-specific IR (TkIR).
 * This is the language emitter, NOT the toolkit IR!
 */

#ifndef TCL_EMITTER_H
#define TCL_EMITTER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct TkIRRoot TkIRRoot;
typedef struct TkIRWidget TkIRWidget;

/**
 * Generate Tcl code from TkIR
 *
 * @param root TkIR root
 * @return Generated Tcl code (caller must free), or NULL on error
 */
char* tcl_emit_from_tkir(TkIRRoot* root);

/**
 * Generate widget declaration in Tcl
 *
 * @param widget TkIR widget
 * @param indent Indentation level
 * @return Generated code (caller must free), or NULL on error
 */
char* tcl_emit_widget_decl(TkIRWidget* widget, int indent);

/**
 * Generate event handler in Tcl
 *
 * @param widget TkIR widget
 * @param event_name Event name
 * @param handler_name Handler function name
 * @param indent Indentation level
 * @return Generated code (caller must free), or NULL on error
 */
char* tcl_emit_event_handler(TkIRWidget* widget,
                             const char* event_name,
                             const char* handler_name,
                             int indent);

/**
 * Escape a string for Tcl
 *
 * @param input Input string
 * @return Escaped string (caller must free), or NULL on error
 */
char* tcl_escape_string(const char* input);

#ifdef __cplusplus
}
#endif

#endif // TCL_EMITTER_H
