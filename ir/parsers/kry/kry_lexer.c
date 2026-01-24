/**
 * KRY Lexer - Character-level parsing implementation
 */

#include "kry_lexer.h"
#include "kry_ast.h"

char kry_peek(KryParser* p) {
    return (p->pos < p->length) ? p->source[p->pos] : '\0';
}

char kry_advance(KryParser* p) {
    if (p->pos >= p->length) return '\0';

    char c = p->source[p->pos++];

    if (c == '\n') {
        p->line++;
        p->column = 1;
    } else {
        p->column++;
    }

    return c;
}

bool kry_match(KryParser* p, char expected) {
    if (kry_peek(p) == expected) {
        kry_advance(p);
        return true;
    }
    return false;
}

void kry_skip_whitespace(KryParser* p) {
    while (kry_peek(p) != '\0') {
        char c = kry_peek(p);

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            kry_advance(p);
            continue;
        }

        // Skip // comments
        if (c == '/' && p->pos + 1 < p->length && p->source[p->pos + 1] == '/') {
            kry_advance(p);  // Skip first /
            kry_advance(p);  // Skip second /
            while (kry_peek(p) != '\n' && kry_peek(p) != '\0') {
                kry_advance(p);
            }
            continue;
        }

        // Skip /* */ comments
        if (c == '/' && p->pos + 1 < p->length && p->source[p->pos + 1] == '*') {
            kry_advance(p);  // Skip /
            kry_advance(p);  // Skip *
            while (p->pos + 1 < p->length) {
                if (kry_peek(p) == '*' && p->source[p->pos + 1] == '/') {
                    kry_advance(p);  // Skip *
                    kry_advance(p);  // Skip /
                    break;
                }
                kry_advance(p);
            }
            continue;
        }

        break;
    }
}

bool kry_skip_balanced(KryParser* p, KryBalancedCapture* capture) {
    if (!p || !capture) return false;

    char open = kry_peek(p);
    if (open != '(' && open != '[' && open != '{') return false;

    char close = (open == '(') ? ')' : (open == '[') ? ']' : '}';
    capture->open = open;
    capture->close = close;

    kry_advance(p);  // Skip opening delimiter
    capture->start = p->pos;

    int depth = 1;

    while (depth > 0 && kry_peek(p) != '\0') {
        char c = kry_peek(p);
        if (c == open) depth++;
        else if (c == close) depth--;

        if (depth > 0) kry_advance(p);
    }

    capture->end = p->pos;

    if (depth != 0) {
        kry_parser_add_error_fmt(p, KRY_ERROR_ERROR, KRY_ERROR_SYNTAX,
                                 "Unbalanced '%c' (missing '%c')", open, close);
        return false;
    }

    if (kry_peek(p) == close) kry_advance(p);  // Consume closing delimiter
    return true;
}

KryParserCheckpoint kry_checkpoint_save(KryParser* p) {
    KryParserCheckpoint cp = { p->pos, p->line, p->column };
    return cp;
}

void kry_checkpoint_restore(KryParser* p, KryParserCheckpoint cp) {
    p->pos = cp.pos;
    p->line = cp.line;
    p->column = cp.column;
}
