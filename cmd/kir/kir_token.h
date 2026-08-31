#ifndef KRYON_KIR_TOKEN_H
#define KRYON_KIR_TOKEN_H

#include "kir.h"

#include <stddef.h>

typedef enum KirTokenKind {
    KIR_TOKEN_EOF = 0,
    KIR_TOKEN_IDENT,
    KIR_TOKEN_INT,
    KIR_TOKEN_FLOAT,
    KIR_TOKEN_STRING,
    KIR_TOKEN_CHAR,
    KIR_TOKEN_DIRECTIVE,
    KIR_TOKEN_OPERATOR,
    KIR_TOKEN_PUNCT,
    KIR_TOKEN_UNKNOWN
} KirTokenKind;

typedef struct KirToken {
    KirTokenKind kind;
    char text[KIR_TEXT_MAX];
    KirSourceSpan span;
} KirToken;

typedef struct KirLexer {
    const char *src;
    const char *path;
    size_t pos;
    int line;
    int column;
} KirLexer;

void KirLexerInit(KirLexer *lx, const char *src, const char *path);
KirToken KirLexerNext(KirLexer *lx);
const char *KirTokenKindName(KirTokenKind kind);

#endif /* KRYON_KIR_TOKEN_H */
