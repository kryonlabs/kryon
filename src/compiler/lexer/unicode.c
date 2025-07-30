/**
 * @file unicode.c
 * @brief Unicode Support for Kryon Lexer
 */

#include "internal/lexer.h"
#include <stdint.h>

// =============================================================================
// UTF-8 DECODING
// =============================================================================

size_t kryon_lexer_decode_utf8(const char *source, uint32_t *out_codepoint) {
    if (!source || !out_codepoint) return 0;
    
    const uint8_t *bytes = (const uint8_t*)source;
    uint32_t codepoint = 0;
    size_t length = 0;
    
    // Determine sequence length from first byte
    if ((bytes[0] & 0x80) == 0) {
        // 0xxxxxxx - ASCII (1 byte)
        codepoint = bytes[0];
        length = 1;
    } else if ((bytes[0] & 0xE0) == 0xC0) {
        // 110xxxxx 10xxxxxx - 2 bytes
        if ((bytes[1] & 0xC0) != 0x80) return 0; // Invalid continuation
        codepoint = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
        length = 2;
        // Check for overlong encoding
        if (codepoint < 0x80) return 0;
    } else if ((bytes[0] & 0xF0) == 0xE0) {
        // 1110xxxx 10xxxxxx 10xxxxxx - 3 bytes
        if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) return 0;
        codepoint = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
        length = 3;
        // Check for overlong encoding
        if (codepoint < 0x800) return 0;
        // Check for surrogates (invalid in UTF-8)
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return 0;
    } else if ((bytes[0] & 0xF8) == 0xF0) {
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx - 4 bytes
        if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) return 0;
        codepoint = ((bytes[0] & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) | 
                   ((bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F);
        length = 4;
        // Check for overlong encoding
        if (codepoint < 0x10000) return 0;
        // Check for values beyond Unicode range
        if (codepoint > 0x10FFFF) return 0;
    } else {
        // Invalid UTF-8 sequence
        return 0;
    }
    
    *out_codepoint = codepoint;
    return length;
}

// =============================================================================
// UNICODE CHARACTER CLASSIFICATION
// =============================================================================

bool kryon_lexer_is_identifier_char(uint32_t codepoint, bool is_start) {
    // ASCII fast path
    if (codepoint < 128) {
        if (is_start) {
            return (codepoint >= 'a' && codepoint <= 'z') ||
                   (codepoint >= 'A' && codepoint <= 'Z') ||
                   codepoint == '_';
        } else {
            return (codepoint >= 'a' && codepoint <= 'z') ||
                   (codepoint >= 'A' && codepoint <= 'Z') ||
                   (codepoint >= '0' && codepoint <= '9') ||
                   codepoint == '_';
        }
    }
    
    // Unicode identifier rules (simplified)
    // This is a basic implementation - a full implementation would use
    // the Unicode character database for proper classification
    
    if (is_start) {
        // Start characters: Letters and certain symbols
        return (codepoint >= 0x00C0 && codepoint <= 0x00D6) ||    // Latin-1 Supplement letters
               (codepoint >= 0x00D8 && codepoint <= 0x00F6) ||    // Latin-1 Supplement letters
               (codepoint >= 0x00F8 && codepoint <= 0x02FF) ||    // Latin Extended
               (codepoint >= 0x0370 && codepoint <= 0x037D) ||    // Greek and Coptic
               (codepoint >= 0x037F && codepoint <= 0x1FFF) ||    // Various scripts
               (codepoint >= 0x200C && codepoint <= 0x200D) ||    // Zero width non-joiner/joiner
               (codepoint >= 0x2070 && codepoint <= 0x218F) ||    // Superscripts and subscripts
               (codepoint >= 0x2C00 && codepoint <= 0x2FEF) ||    // Various scripts
               (codepoint >= 0x3001 && codepoint <= 0xD7FF) ||    // CJK and other scripts
               (codepoint >= 0xF900 && codepoint <= 0xFDCF) ||    // CJK Compatibility
               (codepoint >= 0xFDF0 && codepoint <= 0xFFFD) ||    // Specials
               (codepoint >= 0x10000 && codepoint <= 0xEFFFF);    // Supplementary planes
    } else {
        // Continuation characters: Letters, digits, marks, and certain symbols
        return kryon_lexer_is_identifier_char(codepoint, true) ||
               (codepoint >= '0' && codepoint <= '9') ||           // ASCII digits
               (codepoint >= 0x0300 && codepoint <= 0x036F) ||    // Combining diacritics
               (codepoint >= 0x203F && codepoint <= 0x2040) ||    // Undertie, character tie
               (codepoint >= 0x0660 && codepoint <= 0x0669) ||    // Arabic-Indic digits
               (codepoint >= 0x06F0 && codepoint <= 0x06F9) ||    // Extended Arabic-Indic digits
               (codepoint >= 0x0966 && codepoint <= 0x096F) ||    // Devanagari digits
               (codepoint >= 0xFF10 && codepoint <= 0xFF19);      // Fullwidth digits
    }
}

bool kryon_lexer_is_whitespace(uint32_t codepoint) {
    // ASCII whitespace
    if (codepoint < 128) {
        return codepoint == ' ' || codepoint == '\t' || codepoint == '\n' || 
               codepoint == '\r' || codepoint == '\f' || codepoint == '\v';
    }
    
    // Unicode whitespace (basic set)
    switch (codepoint) {
        case 0x0085:  // Next Line
        case 0x00A0:  // Non-breaking space
        case 0x1680:  // Ogham space mark
        case 0x2000:  // En quad
        case 0x2001:  // Em quad
        case 0x2002:  // En space
        case 0x2003:  // Em space
        case 0x2004:  // Three-per-em space
        case 0x2005:  // Four-per-em space
        case 0x2006:  // Six-per-em space
        case 0x2007:  // Figure space
        case 0x2008:  // Punctuation space
        case 0x2009:  // Thin space
        case 0x200A:  // Hair space
        case 0x2028:  // Line separator
        case 0x2029:  // Paragraph separator
        case 0x202F:  // Narrow no-break space
        case 0x205F:  // Medium mathematical space
        case 0x3000:  // Ideographic space
            return true;
        default:
            return false;
    }
}