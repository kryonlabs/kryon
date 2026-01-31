/**
 * Kryon Limbo Language Emitter
 *
 * Generates Limbo syntax from toolkit-specific IR (DrawIR).
 * This is the language emitter, NOT the toolkit IR!
 */

#ifndef LIMBO_EMITTER_H
#define LIMBO_EMITTER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct DrawIRRoot DrawIRRoot;
typedef struct DrawIRWidget DrawIRWidget;

/**
 * Generate Limbo code from DrawIR
 *
 * @param root DrawIR root
 * @return Generated Limbo code (caller must free), or NULL on error
 */
char* limbo_emit_from_drawir(DrawIRRoot* root);

/**
 * Generate widget declaration in Limbo
 *
 * @param widget DrawIR widget
 * @param indent Indentation level
 * @return Generated code (caller must free), or NULL on error
 */
char* limbo_emit_widget_decl(DrawIRWidget* widget, int indent);

/**
 * Generate event handler in Limbo
 *
 * @param widget DrawIR widget
 * @param event_name Event name
 * @param handler_name Handler function name
 * @param indent Indentation level
 * @return Generated code (caller must free), or NULL on error
 */
char* limbo_emit_event_handler(DrawIRWidget* widget,
                               const char* event_name,
                               const char* handler_name,
                               int indent);

/**
 * Escape a string for Limbo
 *
 * @param input Input string
 * @return Escaped string (caller must free), or NULL on error
 */
char* limbo_escape_string(const char* input);

#ifdef __cplusplus
}
#endif

#endif // LIMBO_EMITTER_H
