/**
 * KRY Tokens - Basic Token Parsing Implementation
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_tokens.h"
#include "kry_lexer.h"
#include "kry_allocator.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Buffer size for number parsing
#define KRY_NUMBER_BUFFER_SIZE 256

char* kry_parse_identifier(KryParser* p) {
    size_t start = p->pos;

    while (kry_peek(p) != '\0' && (isalnum(kry_peek(p)) || kry_peek(p) == '_' || kry_peek(p) == '-')) {
        kry_advance(p);
    }

    size_t len = p->pos - start;
    char* result = kry_strndup(p, p->source + start, len);

    return result;
}

char* kry_parse_string(KryParser* p) {
    if (!kry_match(p, '"')) {
        kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_SYNTAX, "Expected string literal");
        return NULL;
    }

    size_t start = p->pos;

    while (kry_peek(p) != '"' && kry_peek(p) != '\0') {
        if (kry_peek(p) == '\\') {
            kry_advance(p);  // Skip escape char
            if (kry_peek(p) != '\0') kry_advance(p);  // Skip escaped char
        } else {
            kry_advance(p);
        }
    }

    size_t len = p->pos - start;
    char* str = kry_strndup(p, p->source + start, len);

    if (!kry_match(p, '"')) {
        kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_SYNTAX, "Unterminated string literal");
        return NULL;
    }

    return str;
}

double kry_parse_number(KryParser* p) {
    size_t start = p->pos;

    // Handle negative numbers
    if (kry_peek(p) == '-') {
        kry_advance(p);
    }

    // Parse integer part
    while (isdigit(kry_peek(p))) {
        kry_advance(p);
    }

    // Parse decimal part
    // BUT NOT if this is the start of a range operator (..)
    if (kry_peek(p) == '.' && p->pos + 1 < p->length && p->source[p->pos + 1] != '.') {
        kry_advance(p);
        while (isdigit(kry_peek(p))) {
            kry_advance(p);
        }
    }

    // Reject % suffix - percentages must be quoted strings (e.g., "100%" not 100%)
    if (kry_peek(p) == '%') {
        kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_SYNTAX,
            "Percentage values must be quoted (e.g., \"100%%\" not 100%%)");
        kry_advance(p);  // Consume the % for error recovery
    }

    size_t len = p->pos - start;
    char* num_str = kry_strndup(p, p->source + start, len);

    // Remove % from the string before parsing
    char clean_num[KRY_NUMBER_BUFFER_SIZE];
    size_t clean_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (clean_len >= sizeof(clean_num) - 1) {
            kry_parser_add_error(p, KRY_ERROR_ERROR, KRY_ERROR_BUFFER_OVERFLOW,
                                 "Number literal too long (max 255 characters)");
            clean_num[sizeof(clean_num) - 1] = '\0';
            break;
        }
        if (num_str[i] != '%') {
            clean_num[clean_len++] = num_str[i];
        }
    }
    clean_num[clean_len] = '\0';

    double value = atof(clean_num);

    return value;
}
