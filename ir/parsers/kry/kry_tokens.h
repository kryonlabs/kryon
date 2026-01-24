/**
 * KRY Tokens - Basic Token Parsing Functions
 *
 * Provides functions for parsing identifiers, strings, and numbers.
 */

#ifndef KRY_TOKENS_H
#define KRY_TOKENS_H

#include "kry_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse an identifier (alphanumeric, underscore, hyphen)
 *
 * @param p Parser context
 * @return Parsed identifier string (allocated from parser chunk), or NULL on error
 */
char* kry_parse_identifier(KryParser* p);

/**
 * Parse a quoted string literal
 *
 * Handles escape sequences. The opening quote must be present.
 *
 * @param p Parser context
 * @return Parsed string content (allocated from parser chunk), or NULL on error
 */
char* kry_parse_string(KryParser* p);

/**
 * Parse a numeric literal (integer or floating point)
 *
 * Handles:
 * - Negative numbers
 * - Decimal points
 * - Rejects unquoted percentages
 *
 * @param p Parser context
 * @return Parsed numeric value
 */
double kry_parse_number(KryParser* p);

#ifdef __cplusplus
}
#endif

#endif // KRY_TOKENS_H
