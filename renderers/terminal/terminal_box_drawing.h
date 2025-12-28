/**
 * Kryon Terminal Renderer - Box Drawing Characters
 *
 * Provides Unicode and ASCII box drawing character sets for terminal UIs.
 * Automatically selects appropriate characters based on terminal capabilities.
 */

#ifndef KRYON_TERMINAL_BOX_DRAWING_H
#define KRYON_TERMINAL_BOX_DRAWING_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Box Drawing Character Sets
// ============================================================================

/**
 * Box drawing style types
 */
typedef enum {
    BOX_STYLE_SINGLE,      // Single line: ─│┌┐└┘
    BOX_STYLE_DOUBLE,      // Double line: ═║╔╗╚╝ (for focus/emphasis)
    BOX_STYLE_ROUNDED,     // Rounded corners: ╭─╮│╰─╯
    BOX_STYLE_HEAVY,       // Heavy/bold: ━┃┏┓┗┛
    BOX_STYLE_ASCII        // ASCII fallback: -|++++
} BoxDrawingStyle;

/**
 * Box drawing character set
 */
typedef struct {
    const char* horizontal;      // ─ or -
    const char* vertical;        // │ or |
    const char* top_left;        // ┌ or +
    const char* top_right;       // ┐ or +
    const char* bottom_left;     // └ or +
    const char* bottom_right;    // ┘ or +
    const char* cross;           // ┼ or +
    const char* t_down;          // ┬ or +
    const char* t_up;            // ┴ or +
    const char* t_right;         // ├ or +
    const char* t_left;          // ┤ or +
} BoxChars;

/**
 * Checkbox character set
 */
typedef struct {
    const char* unchecked;       // ☐ or [ ]
    const char* checked;         // ☑ or [x]
    const char* indeterminate;   // ☒ or [-]
} CheckboxChars;

/**
 * Special symbols
 */
typedef struct {
    const char* bullet;          // • or *
    const char* arrow_up;        // ↑ or ^
    const char* arrow_down;      // ↓ or v
    const char* arrow_left;      // ← or <
    const char* arrow_right;     // → or >
    const char* triangle_down;   // ▼ or v (for dropdowns)
    const char* block_full;      // █ or #
    const char* block_half;      // ▓ or #
    const char* cursor;          // ▋ or |
} SpecialChars;

// ============================================================================
// Character Set Selection
// ============================================================================

// Forward declarations removed - functions defined as static inline below

// ============================================================================
// Predefined Character Sets
// ============================================================================

// Single line Unicode: ─│┌┐└┘
static const BoxChars BOX_CHARS_SINGLE_UNICODE = {
    .horizontal = "─",
    .vertical = "│",
    .top_left = "┌",
    .top_right = "┐",
    .bottom_left = "└",
    .bottom_right = "┘",
    .cross = "┼",
    .t_down = "┬",
    .t_up = "┴",
    .t_right = "├",
    .t_left = "┤"
};

// Double line Unicode: ═║╔╗╚╝
static const BoxChars BOX_CHARS_DOUBLE_UNICODE = {
    .horizontal = "═",
    .vertical = "║",
    .top_left = "╔",
    .top_right = "╗",
    .bottom_left = "╚",
    .bottom_right = "╝",
    .cross = "╬",
    .t_down = "╦",
    .t_up = "╩",
    .t_right = "╠",
    .t_left = "╣"
};

// Rounded Unicode: ╭─╮│╰─╯
static const BoxChars BOX_CHARS_ROUNDED_UNICODE = {
    .horizontal = "─",
    .vertical = "│",
    .top_left = "╭",
    .top_right = "╮",
    .bottom_left = "╰",
    .bottom_right = "╯",
    .cross = "┼",
    .t_down = "┬",
    .t_up = "┴",
    .t_right = "├",
    .t_left = "┤"
};

// Heavy Unicode: ━┃┏┓┗┛
static const BoxChars BOX_CHARS_HEAVY_UNICODE = {
    .horizontal = "━",
    .vertical = "┃",
    .top_left = "┏",
    .top_right = "┓",
    .bottom_left = "┗",
    .bottom_right = "┛",
    .cross = "╋",
    .t_down = "┳",
    .t_up = "┻",
    .t_right = "┣",
    .t_left = "┫"
};

// ASCII fallback: -|++++
static const BoxChars BOX_CHARS_ASCII = {
    .horizontal = "-",
    .vertical = "|",
    .top_left = "+",
    .top_right = "+",
    .bottom_left = "+",
    .bottom_right = "+",
    .cross = "+",
    .t_down = "+",
    .t_up = "+",
    .t_right = "+",
    .t_left = "+"
};

// Checkbox Unicode: ☐☑☒
static const CheckboxChars CHECKBOX_CHARS_UNICODE = {
    .unchecked = "☐",
    .checked = "☑",
    .indeterminate = "☒"
};

// Checkbox ASCII: [ ][x][-]
static const CheckboxChars CHECKBOX_CHARS_ASCII = {
    .unchecked = "[ ]",
    .checked = "[x]",
    .indeterminate = "[-]"
};

// Special symbols Unicode
static const SpecialChars SPECIAL_CHARS_UNICODE = {
    .bullet = "•",
    .arrow_up = "↑",
    .arrow_down = "↓",
    .arrow_left = "←",
    .arrow_right = "→",
    .triangle_down = "▼",
    .block_full = "█",
    .block_half = "▓",
    .cursor = "▋"
};

// Special symbols ASCII
static const SpecialChars SPECIAL_CHARS_ASCII = {
    .bullet = "*",
    .arrow_up = "^",
    .arrow_down = "v",
    .arrow_left = "<",
    .arrow_right = ">",
    .triangle_down = "v",
    .block_full = "#",
    .block_half = "#",
    .cursor = "|"
};

// ============================================================================
// Inline Implementations
// ============================================================================

static inline BoxChars terminal_get_box_chars(BoxDrawingStyle style, bool unicode_support) {
    if (!unicode_support) {
        return BOX_CHARS_ASCII;
    }

    switch (style) {
        case BOX_STYLE_SINGLE:  return BOX_CHARS_SINGLE_UNICODE;
        case BOX_STYLE_DOUBLE:  return BOX_CHARS_DOUBLE_UNICODE;
        case BOX_STYLE_ROUNDED: return BOX_CHARS_ROUNDED_UNICODE;
        case BOX_STYLE_HEAVY:   return BOX_CHARS_HEAVY_UNICODE;
        case BOX_STYLE_ASCII:   return BOX_CHARS_ASCII;
        default:                return BOX_CHARS_SINGLE_UNICODE;
    }
}

static inline CheckboxChars terminal_get_checkbox_chars(bool unicode_support) {
    return unicode_support ? CHECKBOX_CHARS_UNICODE : CHECKBOX_CHARS_ASCII;
}

static inline SpecialChars terminal_get_special_chars(bool unicode_support) {
    return unicode_support ? SPECIAL_CHARS_UNICODE : SPECIAL_CHARS_ASCII;
}

#ifdef __cplusplus
}
#endif

#endif // KRYON_TERMINAL_BOX_DRAWING_H
