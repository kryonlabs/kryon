/**
 * Plan 9 Backend - Text Layout Measurement
 *
 * Provides text measurement callbacks for the layout system
 */

#include "plan9_internal.h"
#include "../desktop/abstract/desktop_fonts.h"
#include "../desktop/abstract/desktop_text.h"
#include "../../ir/include/ir_log.h"

/* External font operations */
extern DesktopFontOps g_plan9_font_ops;

/* Text measurement callback for layout system */
void
plan9_text_measure_callback(const char* text, const char* font_name,
                            int font_size, float* out_width, float* out_height,
                            void* user_data)
{
    DesktopFont* font_handle;
    Font* font;
    int width;
    int height;

    if (!text || !out_width || !out_height) {
        if (out_width) *out_width = 0.0f;
        if (out_height) *out_height = 0.0f;
        return;
    }

    /* Use default font if none specified or display not initialized */
    if (!font_name || !display) {
        if (display && display->defaultfont) {
            font = display->defaultfont;
        } else {
            /* No display available - return estimate */
            *out_width = (float)(strlen(text) * font_size * 0.6f);
            *out_height = (float)font_size;
            return;
        }
    } else {
        /* Load requested font */
        font_handle = g_plan9_font_ops.load_font(font_name, font_size);
        if (!font_handle) {
            /* Fall back to default font */
            font = display->defaultfont;
        } else {
            font = (Font*)font_handle;
        }
    }

    /* Measure text */
    width = stringwidth(font, (char*)text);
    height = font->height;

    *out_width = (float)width;
    *out_height = (float)height;
}
