/**
 * @file ir_bidi_stub.c
 * @brief Stub implementation for bidirectional text detection
 *
 * This file provides a minimal stub for BiDi functionality when
 * FriBidi is not available. It always returns LTR (left-to-right)
 * direction, which is suitable for basic Latin text rendering.
 */

#include "ir_text_shaping.h"
#include <stdint.h>

/**
 * Stub BiDi direction detection - always returns LTR
 */
IRBidiDirection ir_bidi_detect_direction(const char* text, uint32_t length) {
    (void)text;
    (void)length;
    return IR_BIDI_DIR_LTR;
}

/**
 * Check if BiDi support is available - always returns false for stub
 */
bool ir_bidi_available(void) {
    return false;
}
