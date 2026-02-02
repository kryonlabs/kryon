/**
 * @file tk_wir_emitter.h
 * @brief Tk Toolkit WIR Emitter
 *
 * Implements the WIRToolkitEmitter interface for Tk.
 * Handles Tk-specific widgets, properties, layout, and event binding.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#ifndef TK_WIR_EMITTER_H
#define TK_WIR_EMITTER_H

#include "../../common/wir_composer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a Tk toolkit emitter.
 * @return Newly allocated toolkit emitter (caller must free with tk_wir_toolkit_emitter_free()),
 *         or NULL on error
 */
WIRToolkitEmitter* tk_wir_toolkit_emitter_create(void);

/**
 * Free a Tk toolkit emitter.
 * @param emitter Toolkit emitter to free (can be NULL)
 */
void tk_wir_toolkit_emitter_free(WIRToolkitEmitter* emitter);

/**
 * Map KIR widget type to Tk widget type.
 * @param kir_type KIR widget type (e.g., "Button", "Text", "Container")
 * @return Tk widget type string (e.g., "button", "label", "frame")
 */
const char* tk_map_widget_type(const char* kir_type);

/**
 * Map KIR event name to Tk event name.
 * @param event KIR event name (e.g., "click", "keypress", "focus")
 * @return Tk event name (e.g., "Button-1", "Key", "FocusIn")
 */
const char* tk_map_event_name(const char* event);

/**
 * Initialize the Tk WIR emitter.
 * Registers the Tk toolkit emitter with the WIR composer.
 * Call this during initialization.
 */
void tk_wir_emitter_init(void);

/**
 * Cleanup the Tk WIR emitter.
 * Unregisters the Tk toolkit emitter from the WIR composer.
 */
void tk_wir_emitter_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // TK_WIR_EMITTER_H
