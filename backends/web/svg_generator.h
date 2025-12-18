#ifndef SVG_GENERATOR_H
#define SVG_GENERATOR_H

#include "../../ir/ir_core.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * SVG theme configuration
 */
typedef enum {
    SVG_THEME_DEFAULT,
    SVG_THEME_DARK,
    SVG_THEME_LIGHT,
    SVG_THEME_HIGH_CONTRAST
} SVGTheme;

/**
 * SVG generation options
 */
typedef struct {
    int width;                  // Max width (0 = no limit, use content width)
    int height;                 // Max height (0 = no limit, use content height)
    SVGTheme theme;             // Color theme
    bool interactive;           // Enable interactive features (hover, click, zoom)
    bool accessibility;         // Include ARIA attributes and semantic markup
    bool use_intrinsic_size;    // Use computed content bounds instead of fixed size
    const char* title;          // Flowchart title (for <title> tag)
    const char* description;    // Flowchart description (for <desc> tag)
} SVGOptions;

/**
 * Generate SVG from flowchart IR component
 *
 * @param flowchart  The flowchart component (must be IR_COMPONENT_FLOWCHART)
 * @param options    SVG generation options (can be NULL for defaults)
 * @return           Dynamically allocated SVG string (caller must free), or NULL on error
 */
char* flowchart_to_svg(const IRComponent* flowchart, const SVGOptions* options);

/**
 * Get default SVG options
 *
 * @return  Default options structure (800x600, default theme, interactive enabled)
 */
SVGOptions svg_options_default(void);

/**
 * Parse theme string to enum
 *
 * @param theme_str  Theme name ("default", "dark", "light", "high-contrast")
 * @return           Theme enum value, or SVG_THEME_DEFAULT if invalid
 */
SVGTheme svg_parse_theme(const char* theme_str);

/**
 * Get theme CSS class suffix
 *
 * @param theme  Theme enum value
 * @return       CSS class suffix (e.g., "dark", "light")
 */
const char* svg_theme_to_class(SVGTheme theme);

#endif // SVG_GENERATOR_H
