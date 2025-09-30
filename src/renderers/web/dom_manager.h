/**
 * @file dom_manager.h
 * @brief DOM Element Generation for Web Renderer
 *
 * Generates HTML elements from Kryon render commands
 */

#ifndef KRYON_DOM_MANAGER_H
#define KRYON_DOM_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "renderer_interface.h"
#include <stddef.h>
#include <stdbool.h>

// =============================================================================
// DOM GENERATION
// =============================================================================

/**
 * Generate HTML for a render command
 * @param cmd Render command
 * @param element_id Unique element ID
 * @param out_buffer Output buffer for HTML
 * @param buffer_size Buffer size
 * @return Number of bytes written
 */
size_t kryon_dom_generate_for_command(const KryonRenderCommand* cmd,
                                     const char* element_id,
                                     char* out_buffer,
                                     size_t buffer_size);

/**
 * Generate HTML opening tag for element
 * @param tag_name HTML tag name
 * @param element_id Element ID
 * @param class_name Optional CSS class
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_open_tag(const char* tag_name,
                         const char* element_id,
                         const char* class_name,
                         char* out_buffer,
                         size_t buffer_size);

/**
 * Generate HTML closing tag
 * @param tag_name HTML tag name
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_close_tag(const char* tag_name,
                          char* out_buffer,
                          size_t buffer_size);

/**
 * Generate HTML for button element
 * @param data Button data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_generate_button(const KryonDrawButtonData* data,
                                const char* element_id,
                                char* out_buffer,
                                size_t buffer_size);

/**
 * Generate HTML for input element
 * @param data Input data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_generate_input(const KryonDrawInputData* data,
                               const char* element_id,
                               char* out_buffer,
                               size_t buffer_size);

/**
 * Generate HTML for text element
 * @param data Text data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_generate_text(const KryonDrawTextData* data,
                              const char* element_id,
                              char* out_buffer,
                              size_t buffer_size);

/**
 * Generate HTML for container element
 * @param data Container data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_generate_container(const KryonBeginContainerData* data,
                                   const char* element_id,
                                   char* out_buffer,
                                   size_t buffer_size);

/**
 * Generate HTML for checkbox element
 * @param data Checkbox data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_generate_checkbox(const KryonDrawCheckboxData* data,
                                  const char* element_id,
                                  char* out_buffer,
                                  size_t buffer_size);

/**
 * Generate HTML for dropdown element
 * @param data Dropdown data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_generate_dropdown(const KryonDrawDropdownData* data,
                                  const char* element_id,
                                  char* out_buffer,
                                  size_t buffer_size);

/**
 * Escape HTML special characters
 * @param input Input string
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_dom_escape_html(const char* input,
                            char* out_buffer,
                            size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // KRYON_DOM_MANAGER_H