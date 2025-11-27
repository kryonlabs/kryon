#include "ir_style_vars.h"
#include "ir_core.h"
#include <string.h>

// Global style variable registry
IRStyleVarEntry ir_style_vars[IR_MAX_STYLE_VARS];

// Dirty flag - set when any variable changes
static bool style_vars_dirty = false;
static bool style_vars_initialized = false;

// Auto-initialize on first use
static void ensure_initialized(void) {
    if (!style_vars_initialized) {
        memset(ir_style_vars, 0, sizeof(ir_style_vars));
        style_vars_initialized = true;
    }
}

// Set a style variable by RGBA components
void ir_style_var_set(IRStyleVarId id, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    ensure_initialized();
    if (id >= IR_MAX_STYLE_VARS) return;

    ir_style_vars[id].r = r;
    ir_style_vars[id].g = g;
    ir_style_vars[id].b = b;
    ir_style_vars[id].a = a;
    ir_style_vars[id].defined = true;
    style_vars_dirty = true;
}

// Set a style variable from packed hex color (0xAARRGGBB or 0xRRGGBB)
void ir_style_var_set_hex(IRStyleVarId id, uint32_t hex_color) {
    ensure_initialized();
    if (id >= IR_MAX_STYLE_VARS) return;

    // Handle both 0xRRGGBB and 0xAARRGGBB formats
    uint8_t a = (hex_color >> 24) & 0xFF;
    uint8_t r = (hex_color >> 16) & 0xFF;
    uint8_t g = (hex_color >> 8) & 0xFF;
    uint8_t b = hex_color & 0xFF;

    // If alpha is 0 but color has values, assume fully opaque
    if (a == 0 && (r || g || b)) {
        a = 255;
    }

    ir_style_vars[id].r = r;
    ir_style_vars[id].g = g;
    ir_style_vars[id].b = b;
    ir_style_vars[id].a = a;
    ir_style_vars[id].defined = true;
    style_vars_dirty = true;
}

// Get a style variable's current value
bool ir_style_var_get(IRStyleVarId id, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    ensure_initialized();
    if (id >= IR_MAX_STYLE_VARS || !ir_style_vars[id].defined) {
        return false;
    }

    if (r) *r = ir_style_vars[id].r;
    if (g) *g = ir_style_vars[id].g;
    if (b) *b = ir_style_vars[id].b;
    if (a) *a = ir_style_vars[id].a;
    return true;
}

// Clear a style variable
void ir_style_var_clear(IRStyleVarId id) {
    ensure_initialized();
    if (id >= IR_MAX_STYLE_VARS) return;

    ir_style_vars[id].defined = false;
    ir_style_vars[id].r = 0;
    ir_style_vars[id].g = 0;
    ir_style_vars[id].b = 0;
    ir_style_vars[id].a = 0;
    style_vars_dirty = true;
}

// Apply a complete theme (sets all 16 builtin variables at once)
void ir_style_vars_apply_theme(const uint32_t colors[16]) {
    ensure_initialized();

    // Set all 16 builtin theme variables (IDs 1-16)
    for (int i = 0; i < 16; i++) {
        ir_style_var_set_hex((IRStyleVarId)(IR_VAR_PRIMARY + i), colors[i]);
    }

    style_vars_dirty = true;
}

// Check if style variables have changed since last clear
bool ir_style_vars_is_dirty(void) {
    return style_vars_dirty;
}

// Clear the dirty flag (call after re-rendering)
void ir_style_vars_clear_dirty(void) {
    style_vars_dirty = false;
}

// Resolve IRColor to actual RGBA values
// If it's a variable reference, look up the current value
bool ir_color_resolve(const IRColor* color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (!color) {
        if (r) *r = 0;
        if (g) *g = 0;
        if (b) *b = 0;
        if (a) *a = 0;
        return false;
    }

    switch (color->type) {
        case IR_COLOR_VAR_REF:
            // Look up the variable
            return ir_style_var_get(color->data.var_id, r, g, b, a);

        case IR_COLOR_SOLID:
            if (r) *r = color->data.r;
            if (g) *g = color->data.g;
            if (b) *b = color->data.b;
            if (a) *a = color->data.a;
            return true;

        case IR_COLOR_TRANSPARENT:
            if (r) *r = 0;
            if (g) *g = 0;
            if (b) *b = 0;
            if (a) *a = 0;
            return true;

        case IR_COLOR_GRADIENT:
            // Gradients not yet implemented - fall back to first color
            if (r) *r = color->data.r;
            if (g) *g = color->data.g;
            if (b) *b = color->data.b;
            if (a) *a = color->data.a;
            return true;

        default:
            if (r) *r = 0;
            if (g) *g = 0;
            if (b) *b = 0;
            if (a) *a = 0;
            return false;
    }
}
