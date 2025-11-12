#include "include/kryon.h"

// ============================================================================
// Color Utility Functions
// ============================================================================

uint32_t kryon_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | a;
}

uint32_t kryon_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return kryon_color_rgba(r, g, b, 255); // Full opacity
}

void kryon_color_get_components(uint32_t color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (r) *r = (uint8_t)(color >> 24);
    if (g) *g = (uint8_t)((color >> 16) & 0xFF);
    if (b) *b = (uint8_t)((color >> 8) & 0xFF);
    if (a) *a = (uint8_t)(color & 0xFF);
}

uint8_t kryon_color_get_red(uint32_t color) {
    return (uint8_t)(color >> 24);
}

uint8_t kryon_color_get_green(uint32_t color) {
    return (uint8_t)((color >> 16) & 0xFF);
}

uint8_t kryon_color_get_blue(uint32_t color) {
    return (uint8_t)((color >> 8) & 0xFF);
}

uint8_t kryon_color_get_alpha(uint32_t color) {
    return (uint8_t)(color & 0xFF);
}

uint32_t kryon_color_lerp(uint32_t color1, uint32_t color2, kryon_fp_t t) {
    // Extract components
    uint8_t r1 = kryon_color_get_red(color1);
    uint8_t g1 = kryon_color_get_green(color1);
    uint8_t b1 = kryon_color_get_blue(color1);
    uint8_t a1 = kryon_color_get_alpha(color1);

    uint8_t r2 = kryon_color_get_red(color2);
    uint8_t g2 = kryon_color_get_green(color2);
    uint8_t b2 = kryon_color_get_blue(color2);
    uint8_t a2 = kryon_color_get_alpha(color2);

    // Interpolate
    uint8_t r = (uint8_t)(r1 + KRYON_FP_TO_INT(KRYON_FP_MUL(KRYON_FP_FROM_INT(r2 - r1), t)));
    uint8_t g = (uint8_t)(g1 + KRYON_FP_TO_INT(KRYON_FP_MUL(KRYON_FP_FROM_INT(g2 - g1), t)));
    uint8_t b = (uint8_t)(b1 + KRYON_FP_TO_INT(KRYON_FP_MUL(KRYON_FP_FROM_INT(b2 - b1), t)));
    uint8_t a = (uint8_t)(a1 + KRYON_FP_TO_INT(KRYON_FP_MUL(KRYON_FP_FROM_INT(a2 - a1), t)));

    return kryon_color_rgba(r, g, b, a);
}

// ============================================================================
// Fixed-Point Arithmetic (Always Available)
// ============================================================================

#if !KRYON_NO_FLOAT
#include <math.h>
#endif

kryon_fp_t kryon_fp_from_float(float f) {
#if KRYON_NO_FLOAT
    // For MCU builds without floating point, this would need integer input
    return KRYON_FP_FROM_INT((int32_t)f);
#else
    return KRYON_FP_16_16(f);
#endif
}

float kryon_fp_to_float(kryon_fp_t fp) {
#if KRYON_NO_FLOAT
    // For MCU builds, this would return integer approximation
    return (float)KRYON_FP_TO_INT(fp);
#else
    // For desktop builds, kryon_fp_t is already float, just return it
    return (float)fp;
#endif
}

kryon_fp_t kryon_fp_from_int(int32_t i) {
    return KRYON_FP_FROM_INT(i);
}

int32_t kryon_fp_to_int_round(kryon_fp_t fp) {
#if KRYON_NO_FLOAT
    // Simple truncation for MCU
    return KRYON_FP_TO_INT(fp);
#else
    // Proper rounding for desktop
    return (int32_t)(kryon_fp_to_float(fp) + 0.5f);
#endif
}

// Fixed-point math operations (always available)
kryon_fp_t kryon_fp_add(kryon_fp_t a, kryon_fp_t b) {
    return a + b;
}

kryon_fp_t kryon_fp_sub(kryon_fp_t a, kryon_fp_t b) {
    return a - b;
}

kryon_fp_t kryon_fp_mul(kryon_fp_t a, kryon_fp_t b) {
    return KRYON_FP_MUL(a, b);
}

kryon_fp_t kryon_fp_div(kryon_fp_t a, kryon_fp_t b) {
    return KRYON_FP_DIV(a, b);
}

kryon_fp_t kryon_fp_sqrt(kryon_fp_t value) {
#if KRYON_NO_FLOAT
    // Integer square root approximation for MCU
    // Using binary search method
    if (value <= 0) return 0;

    int32_t int_val = KRYON_FP_TO_INT(value);
    if (int_val <= 0) return 0;

    int32_t result = 0;
    int32_t bit = 1 << 30; // The second-to-top bit

    // "bit" starts at the highest power of four <= the argument.
    while (bit > int_val)
        bit >>= 2;

    while (bit != 0) {
        if (int_val >= result + bit) {
            int_val -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return KRYON_FP_FROM_INT(result);
#else
    // Use floating point sqrt for desktop
    return KRYON_FP_16_16(sqrtf(kryon_fp_to_float(value)));
#endif
}

kryon_fp_t kryon_fp_abs(kryon_fp_t value) {
    return (value < 0) ? -value : value;
}

kryon_fp_t kryon_fp_min(kryon_fp_t a, kryon_fp_t b) {
    return (a < b) ? a : b;
}

kryon_fp_t kryon_fp_max(kryon_fp_t a, kryon_fp_t b) {
    return (a > b) ? a : b;
}

kryon_fp_t kryon_fp_clamp(kryon_fp_t value, kryon_fp_t min, kryon_fp_t max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// ============================================================================
// Utility Functions
// ============================================================================

uint32_t kryon_get_timestamp_ms(void) {
    // Platform-specific implementation would go here
    // For now, return a simple counter (would be replaced by platform code)
    static uint32_t counter = 0;
    return ++counter;
}

void kryon_delay_ms(uint32_t ms) {
    // Platform-specific delay implementation
    // This would be implemented by platform-specific code
    (void)ms; // Suppress unused parameter warning
}

bool kryon_point_in_rect(int16_t x, int16_t y, int16_t rx, int16_t ry, uint16_t rw, uint16_t rh) {
    return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);
}

bool kryon_rect_intersect(int16_t x1, int16_t y1, uint16_t w1, uint16_t h1,
                         int16_t x2, int16_t y2, uint16_t w2, uint16_t h2) {
    return !(x1 >= x2 + w2 || x2 >= x1 + w1 || y1 >= y2 + h2 || y2 >= y1 + h1);
}

void kryon_rect_union(int16_t x1, int16_t y1, uint16_t w1, uint16_t h1,
                     int16_t x2, int16_t y2, uint16_t w2, uint16_t h2,
                     int16_t* ux, int16_t* uy, uint16_t* uw, uint16_t* uh) {
    if (ux == NULL || uy == NULL || uw == NULL || uh == NULL) {
        return;
    }

    *ux = (x1 < x2) ? x1 : x2;
    *uy = (y1 < y2) ? y1 : y2;

    int16_t right1 = x1 + w1;
    int16_t right2 = x2 + w2;
    int16_t bottom1 = y1 + h1;
    int16_t bottom2 = y2 + h2;

    *uw = (right1 > right2) ? (right1 - *ux) : (right2 - *ux);
    *uh = (bottom1 > bottom2) ? (bottom1 - *uy) : (bottom2 - *uy);
}

// ============================================================================
// String Utilities (Safe versions for MCU)
// ============================================================================

uint32_t kryon_str_len(const char* str) {
    if (str == NULL) return 0;

    uint32_t len = 0;
    while (str[len] != '\0' && len < KRYON_MAX_TEXT_LENGTH) {
        len++;
    }
    return len;
}

int32_t kryon_str_cmp(const char* str1, const char* str2) {
    if (str1 == NULL || str2 == NULL) {
        return (str1 == str2) ? 0 : ((str1 == NULL) ? -1 : 1);
    }

    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }

    return (int32_t)(*str1) - (int32_t)(*str2);
}

void kryon_str_copy(char* dest, const char* src, uint32_t max_len) {
    if (dest == NULL || src == NULL) {
        return;
    }

    uint32_t i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

bool kryon_str_equals(const char* str1, const char* str2) {
    return kryon_str_cmp(str1, str2) == 0;
}

// ============================================================================
// Memory Utilities (Safe for MCU)
// ============================================================================

void* kryon_memset(void* ptr, int value, uint32_t size) {
    if (ptr == NULL || size == 0) {
        return ptr;
    }

    uint8_t* bytes = (uint8_t*)ptr;
    for (uint32_t i = 0; i < size; i++) {
        bytes[i] = (uint8_t)value;
    }

    return ptr;
}

void* kryon_memcpy(void* dest, const void* src, uint32_t size) {
    if (dest == NULL || src == NULL || size == 0) {
        return dest;
    }

    uint8_t* dest_bytes = (uint8_t*)dest;
    const uint8_t* src_bytes = (const uint8_t*)src;

    for (uint32_t i = 0; i < size; i++) {
        dest_bytes[i] = src_bytes[i];
    }

    return dest;
}

int32_t kryon_memcmp(const void* ptr1, const void* ptr2, uint32_t size) {
    if (ptr1 == NULL || ptr2 == NULL) {
        return (ptr1 == ptr2) ? 0 : ((ptr1 == NULL) ? -1 : 1);
    }

    const uint8_t* bytes1 = (const uint8_t*)ptr1;
    const uint8_t* bytes2 = (const uint8_t*)ptr2;

    for (uint32_t i = 0; i < size; i++) {
        if (bytes1[i] != bytes2[i]) {
            return (int32_t)bytes1[i] - (int32_t)bytes2[i];
        }
    }

    return 0;
}

// ============================================================================
// Math Utilities
// ============================================================================

kryon_fp_t kryon_lerp(kryon_fp_t a, kryon_fp_t b, kryon_fp_t t) {
    return a + KRYON_FP_MUL(b - a, t);
}

kryon_fp_t kryon_smoothstep(kryon_fp_t edge0, kryon_fp_t edge1, kryon_fp_t x) {
    // Clamp, smooth step formula: 3t² - 2t³
    if (x <= edge0) return 0;
    if (x >= edge1) return KRYON_FP_FROM_INT(1);

    kryon_fp_t t = KRYON_FP_DIV(x - edge0, edge1 - edge0);
    kryon_fp_t t_squared = KRYON_FP_MUL(t, t);
    kryon_fp_t t_cubed = KRYON_FP_MUL(t_squared, t);

    return KRYON_FP_MUL(KRYON_FP_FROM_INT(3), t_cubed) - KRYON_FP_MUL(KRYON_FP_FROM_INT(2), t_squared);
}

bool kryon_fp_equal(kryon_fp_t a, kryon_fp_t b, kryon_fp_t epsilon) {
    return kryon_fp_abs(a - b) <= epsilon;
}