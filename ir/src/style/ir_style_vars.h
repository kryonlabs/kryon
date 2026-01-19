#ifndef IR_STYLE_VARS_H
#define IR_STYLE_VARS_H

#include <stdint.h>
#include <stdbool.h>

// Maximum style variables (256 slots: 16 builtin + 240 custom)
#define IR_MAX_STYLE_VARS 256

// Style variable ID type
typedef uint16_t IRStyleVarId;

// Reserved variable IDs for theme colors
typedef enum {
    IR_VAR_NONE = 0,
    // Primary colors
    IR_VAR_PRIMARY = 1,
    IR_VAR_PRIMARY_HOVER,
    IR_VAR_SECONDARY,
    IR_VAR_ACCENT,
    // Background colors
    IR_VAR_BACKGROUND,
    IR_VAR_SURFACE,
    IR_VAR_SURFACE_HOVER,
    IR_VAR_CARD,
    // Text colors
    IR_VAR_TEXT,
    IR_VAR_TEXT_MUTED,
    IR_VAR_TEXT_ON_PRIMARY,
    // Semantic colors
    IR_VAR_SUCCESS,
    IR_VAR_WARNING,
    IR_VAR_ERROR,
    // Border/divider
    IR_VAR_BORDER,
    IR_VAR_DIVIDER,
    // Custom variables start here
    IR_VAR_CUSTOM_START = 64
} IRStyleVarBuiltin;

// Style variable entry
typedef struct {
    uint8_t r, g, b, a;
    bool defined;
} IRStyleVarEntry;

// Global style variable registry
extern IRStyleVarEntry ir_style_vars[IR_MAX_STYLE_VARS];

// API functions (auto-initializes on first use)
void ir_style_var_set(IRStyleVarId id, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_style_var_set_hex(IRStyleVarId id, uint32_t hex_color);
bool ir_style_var_get(IRStyleVarId id, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);
void ir_style_var_clear(IRStyleVarId id);

// Apply a complete theme (sets all 16 builtin variables at once)
// colors array: [primary, primaryHover, secondary, accent, background, surface,
//                surfaceHover, card, text, textMuted, textOnPrimary, success,
//                warning, error, border, divider]
void ir_style_vars_apply_theme(const uint32_t colors[16]);

// Dirty flag for re-render (set automatically when vars change)
bool ir_style_vars_is_dirty(void);
void ir_style_vars_clear_dirty(void);

// Color resolution - resolve IRColor to actual RGBA values
// If it's a variable reference, look up the current value
// Returns true if resolution succeeded
struct IRColor;  // Forward declaration
bool ir_color_resolve(const struct IRColor* color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);

#endif // IR_STYLE_VARS_H
