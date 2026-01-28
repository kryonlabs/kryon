/**
 * @file krl_parser.h
 * @brief KRL (KRyon Lisp) S-expression Parser
 *
 * Parses Lisp-style S-expressions and generates KIR JSON directly.
 * Pipeline: .krl → S-expr parser → KIR JSON → existing codegen → KRB
 */

#ifndef KRYON_KRL_PARSER_H
#define KRYON_KRL_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include "parser.h"

// =============================================================================
// KRL TOKEN TYPES
// =============================================================================

typedef enum {
    KRL_TOKEN_LPAREN,           // (
    KRL_TOKEN_RPAREN,           // )
    KRL_TOKEN_LBRACKET,         // [
    KRL_TOKEN_RBRACKET,         // ]
    KRL_TOKEN_SYMBOL,           // identifier or keyword
    KRL_TOKEN_STRING,           // "string"
    KRL_TOKEN_INTEGER,          // 42
    KRL_TOKEN_FLOAT,            // 3.14
    KRL_TOKEN_KEYWORD,          // :keyword
    KRL_TOKEN_VARIABLE,         // $variable
    KRL_TOKEN_TRUE,             // true
    KRL_TOKEN_FALSE,            // false
    KRL_TOKEN_EOF,
    KRL_TOKEN_ERROR
} KRLTokenType;

typedef struct {
    KRLTokenType type;
    const char *start;
    size_t length;
    int line;
    int column;
} KRLToken;

// =============================================================================
// KRL LEXER
// =============================================================================

typedef struct {
    const char *source;
    const char *start;
    const char *current;
    int line;
    int column;
    const char *filename;
} KRLLexer;

/**
 * @brief Initialize KRL lexer
 */
void krl_lexer_init(KRLLexer *lexer, const char *source, const char *filename);

/**
 * @brief Get next token
 */
KRLToken krl_lexer_next_token(KRLLexer *lexer);

// =============================================================================
// KRL S-EXPRESSION NODES
// =============================================================================

typedef enum {
    KRL_SEXP_SYMBOL,
    KRL_SEXP_STRING,
    KRL_SEXP_INTEGER,
    KRL_SEXP_FLOAT,
    KRL_SEXP_BOOLEAN,
    KRL_SEXP_LIST
} KRLSExpType;

typedef struct KRLSExp KRLSExp;

struct KRLSExp {
    KRLSExpType type;
    int line;
    int column;

    union {
        char *symbol;           // SYMBOL
        char *string;           // STRING
        int64_t integer;        // INTEGER
        double float_val;       // FLOAT
        bool boolean;           // BOOLEAN
        struct {
            KRLSExp **items;    // LIST items
            size_t count;
            size_t capacity;
        } list;
    } data;
};

/**
 * @brief Create S-expression node
 */
KRLSExp *krl_sexp_create_symbol(const char *symbol, int line, int column);
KRLSExp *krl_sexp_create_string(const char *string, int line, int column);
KRLSExp *krl_sexp_create_integer(int64_t value, int line, int column);
KRLSExp *krl_sexp_create_float(double value, int line, int column);
KRLSExp *krl_sexp_create_boolean(bool value, int line, int column);
KRLSExp *krl_sexp_create_list(int line, int column);

/**
 * @brief Add item to list S-expression
 */
void krl_sexp_list_add(KRLSExp *list, KRLSExp *item);

/**
 * @brief Free S-expression
 */
void krl_sexp_free(KRLSExp *sexp);

// =============================================================================
// KRL PARSER
// =============================================================================

typedef struct {
    KRLLexer lexer;
    KRLToken current;
    KRLToken previous;
    bool had_error;
    bool panic_mode;
} KRLParser;

/**
 * @brief Initialize KRL parser
 */
void krl_parser_init(KRLParser *parser, const char *source, const char *filename);

/**
 * @brief Parse KRL source into list of S-expressions
 */
KRLSExp **krl_parse(KRLParser *parser, size_t *out_count);

// =============================================================================
// KRL TO KIR CONVERSION
// =============================================================================

/**
 * @brief Convert KRL S-expressions to KIR JSON and write to file
 *
 * @param sexps Array of S-expressions
 * @param count Number of S-expressions
 * @param source_file Source filename for metadata
 * @param output_path Output .kir file path
 * @return true on success, false on error
 */
bool krl_to_kir_file(KRLSExp **sexps, size_t count, const char *source_file, const char *output_path);

/**
 * @brief Convert KRL file to KIR file
 *
 * @param input_path Input .krl file
 * @param output_path Output .kir file
 * @return true on success, false on error
 */
bool krl_compile_to_kir(const char *input_path, const char *output_path);

#ifdef __cplusplus
}
#endif

#endif // KRYON_KRL_PARSER_H
