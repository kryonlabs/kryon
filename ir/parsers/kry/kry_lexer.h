/**
 * KRY Lexer - Character-level parsing utilities
 *
 * Provides basic lexing operations: peek, advance, match,
 * whitespace/comment skipping, and balanced delimiter handling.
 */

#ifndef KRY_LEXER_H
#define KRY_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct KryParser KryParser;

/**
 * Peek at current character without advancing
 *
 * @param p Parser context
 * @return Current character, or '\0' if at end
 */
char kry_peek(KryParser* p);

/**
 * Advance parser position and return current character
 *
 * @param p Parser context
 * @return Character at current position before advancing, or '\0' if at end
 */
char kry_advance(KryParser* p);

/**
 * Match and consume expected character
 *
 * @param p Parser context
 * @param expected Character to match
 * @return true if matched and consumed, false otherwise
 */
bool kry_match(KryParser* p, char expected);

/**
 * Skip whitespace and comments
 *
 * Handles:
 * - Whitespace: space, tab, carriage return, newline
 * - Line comments: // ...
 * - Block comments (C-style)
 *
 * @param p Parser context
 */
void kry_skip_whitespace(KryParser* p);

/**
 * Balanced delimiter capture result
 */
typedef struct {
    char open;      // Opening delimiter: '(', '[', '{'
    char close;     // Closing delimiter: ')', ']', '}'
    size_t start;   // Start position (after opening delimiter)
    size_t end;     // End position (before closing delimiter)
} KryBalancedCapture;

/**
 * Skip balanced delimiters and return captured range
 *
 * @param p Parser context
 * @param capture Output structure for captured range
 * @return true if successfully captured balanced content
 */
bool kry_skip_balanced(KryParser* p, KryBalancedCapture* capture);

/**
 * Parser checkpoint for lookahead/backtracking
 */
typedef struct {
    size_t pos;
    uint32_t line;
    uint32_t column;
} KryParserCheckpoint;

/**
 * Save parser position for potential backtracking
 *
 * @param p Parser context
 * @return Checkpoint that can be restored later
 */
KryParserCheckpoint kry_checkpoint_save(KryParser* p);

/**
 * Restore parser to previously saved checkpoint
 *
 * @param p Parser context
 * @param cp Checkpoint to restore
 */
void kry_checkpoint_restore(KryParser* p, KryParserCheckpoint cp);

#ifdef __cplusplus
}
#endif

#endif // KRY_LEXER_H
