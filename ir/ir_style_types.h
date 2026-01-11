#ifndef IR_STYLE_TYPES_H
#define IR_STYLE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Color Types
// ============================================================================

typedef enum {
    IR_COLOR_SOLID,
    IR_COLOR_TRANSPARENT,
    IR_COLOR_GRADIENT,
    IR_COLOR_VAR_REF    // Reference to style variable
} IRColorType;

// Style variable ID type (for IR_COLOR_VAR_REF)
typedef uint16_t IRStyleVarId;

// Forward declarations
typedef struct IRGradient IRGradient;

// Union for color data - either RGBA, style variable reference, or gradient pointer
typedef union IRColorData {
    struct { uint8_t r, g, b, a; };  // Direct RGBA (anonymous for convenience)
    IRStyleVarId var_id;              // Reference to style variable
    IRGradient* gradient;             // Pointer to gradient definition
} IRColorData;

typedef struct IRColor {
    IRColorType type;
    IRColorData data;
    char* var_name;  // CSS variable name (e.g., "--bg") for roundtrip preservation
} IRColor;

// Helper macros for creating IRColor values
#define IR_COLOR_RGBA(R, G, B, A) ((IRColor){ .type = IR_COLOR_SOLID, .data = { .r = (R), .g = (G), .b = (B), .a = (A) } })
#define IR_COLOR_VAR(ID) ((IRColor){ .type = IR_COLOR_VAR_REF, .data = { .var_id = (ID) } })
#define IR_COLOR_GRADIENT(GRAD_PTR) ((IRColor){ .type = IR_COLOR_GRADIENT, .data = { .gradient = (GRAD_PTR) } })
#define IR_COLOR_TRANSPARENT_VALUE ((IRColor){ .type = 1, .data = { .r = 0, .g = 0, .b = 0, .a = 0 } })  // IRColorType 1 = IR_COLOR_TRANSPARENT

// ============================================================================
// Gradient Types
// ============================================================================

typedef enum {
    IR_GRADIENT_LINEAR,
    IR_GRADIENT_RADIAL,
    IR_GRADIENT_CONIC
} IRGradientType;

// Gradient Color Stop
typedef struct {
    float position;  // 0.0 to 1.0
    uint8_t r, g, b, a;
} IRGradientStop;

// Gradient Definition
struct IRGradient {
    IRGradientType type;
    IRGradientStop stops[8];  // Maximum 8 color stops
    uint8_t stop_count;
    float angle;  // For linear gradients (degrees)
    float center_x, center_y;  // For radial/conic (0.0 to 1.0)
};

// ============================================================================
// Color Utilities
// ============================================================================

IRColor ir_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
bool ir_color_equals(IRColor a, IRColor b);
void ir_color_free(IRColor* color);

#endif // IR_STYLE_TYPES_H
