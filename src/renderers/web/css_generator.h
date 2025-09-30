/**
 * @file css_generator.h
 * @brief CSS Generation System for Web Renderer
 *
 * Converts Kryon element properties and styles to CSS rules
 */

#ifndef KRYON_CSS_GENERATOR_H
#define KRYON_CSS_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "renderer_interface.h"
#include <stddef.h>
#include <stdbool.h>

// =============================================================================
// CSS GENERATION
// =============================================================================

/**
 * Generate CSS for a render command
 * @param cmd Render command
 * @param element_id Unique element ID
 * @param out_buffer Output buffer for CSS
 * @param buffer_size Buffer size
 * @return Number of bytes written
 */
size_t kryon_css_generate_for_command(const KryonRenderCommand* cmd,
                                     const char* element_id,
                                     char* out_buffer,
                                     size_t buffer_size);

/**
 * Convert Kryon color to CSS rgba() string
 * @param color Kryon color
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 */
void kryon_css_color_to_string(KryonColor color, char* out_buffer, size_t buffer_size);

/**
 * Generate CSS for rectangle drawing
 * @param data Rectangle data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_css_generate_rect(const KryonDrawRectData* data,
                               const char* element_id,
                               char* out_buffer,
                               size_t buffer_size);

/**
 * Generate CSS for text drawing
 * @param data Text data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_css_generate_text(const KryonDrawTextData* data,
                               const char* element_id,
                               char* out_buffer,
                               size_t buffer_size);

/**
 * Generate CSS for button
 * @param data Button data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_css_generate_button(const KryonDrawButtonData* data,
                                 const char* element_id,
                                 char* out_buffer,
                                 size_t buffer_size);

/**
 * Generate CSS for input field
 * @param data Input data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_css_generate_input(const KryonDrawInputData* data,
                               const char* element_id,
                               char* out_buffer,
                               size_t buffer_size);

/**
 * Generate CSS for container (with flexbox support)
 * @param data Container data
 * @param element_id Element ID
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t kryon_css_generate_container(const KryonBeginContainerData* data,
                                   const char* element_id,
                                   char* out_buffer,
                                   size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // KRYON_CSS_GENERATOR_H