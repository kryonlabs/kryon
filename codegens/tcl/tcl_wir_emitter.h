/**
 * @file tcl_wir_emitter.h
 * @brief Tcl Language WIR Emitter
 *
 * Implements the WIRLanguageEmitter interface for Tcl.
 * Handles Tcl-specific syntax: procs, variables, control flow, string escaping.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#ifndef TCL_WIR_EMITTER_H
#define TCL_WIR_EMITTER_H

#include "../common/wir_composer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a Tcl language emitter.
 * @return Newly allocated language emitter (caller must free with tcl_wir_language_emitter_free()),
 *         or NULL on error
 */
WIRLanguageEmitter* tcl_wir_language_emitter_create(void);

/**
 * Free a Tcl language emitter.
 * @param emitter Language emitter to free (can be NULL)
 */
void tcl_wir_language_emitter_free(WIRLanguageEmitter* emitter);

/**
 * Tcl string escaping.
 * Escapes special Tcl characters: \ $ [ ] " { }
 * @param input Input string
 * @return Newly allocated escaped string (caller must free), or NULL on error
 */
char* tcl_escape_string(const char* input);

/**
 * Tcl string quoting.
 * Wraps string in braces {} if needed, escapes special characters.
 * @param input Input string
 * @return Newly allocated quoted string (caller must free), or NULL on error
 */
char* tcl_quote_string(const char* input);

/**
 * Initialize the Tcl WIR emitter.
 * Registers the Tcl language emitter with the WIR composer.
 * Call this during initialization.
 */
void tcl_wir_emitter_init(void);

/**
 * Cleanup the Tcl WIR emitter.
 * Unregisters the Tcl language emitter from the WIR composer.
 */
void tcl_wir_emitter_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // TCL_WIR_EMITTER_H
