#ifndef LIB9_PRINT_H
#define LIB9_PRINT_H

#include "lib9_types.h"
#include <stdarg.h>

/*
 * lib9 Print Functions
 *
 * Provides Plan 9-style formatted I/O functions.
 */

/* Format state structure */
typedef struct Fmt Fmt;
struct Fmt {
    uchar   runes;          /* output buffer is runes or chars? */
    void    *start;         /* of buffer */
    void    *to;            /* current place in the buffer */
    void    *stop;          /* end of the buffer; overwritten if flush fails */
    int     (*flush)(Fmt*); /* called when to == stop */
    void    *farg;          /* to make flush a closure */
    int     nfmt;           /* num chars formatted so far */
    va_list args;           /* args passed to dofmt */
    int     r;              /* % format Rune */
    int     width;
    int     prec;
    ulong   flags;
};

/* Format flags */
enum {
    FmtWidth    = 1,
    FmtLeft     = FmtWidth << 1,
    FmtPrec     = FmtLeft << 1,
    FmtSharp    = FmtPrec << 1,
    FmtSpace    = FmtSharp << 1,
    FmtSign     = FmtSpace << 1,
    FmtZero     = FmtSign << 1,
    FmtUnsigned = FmtZero << 1,
    FmtShort    = FmtUnsigned << 1,
    FmtLong     = FmtShort << 1,
    FmtVLong    = FmtLong << 1,
    FmtComma    = FmtVLong << 1,
    FmtByte     = FmtComma << 1,
    FmtLDouble  = FmtByte << 1,
    FmtFlag     = FmtLDouble << 1
};

/* Print function family */
int     print(char *fmt, ...);
int     fprint(int fd, char *fmt, ...);
int     snprint(char *buf, int len, char *fmt, ...);
int     vsnprint(char *buf, int len, char *fmt, va_list args);

/* Safe print functions */
char*   seprint(char *buf, char *e, char *fmt, ...);
char*   vseprint(char *buf, char *e, char *fmt, va_list args);

/* Malloc-based print */
char*   smprint(char *fmt, ...);
char*   vsmprint(char *fmt, va_list args);

/* Sprint (deprecated but included for compatibility) */
int     sprint(char *buf, char *fmt, ...);

/* Format infrastructure */
int     fmtinstall(int c, int (*f)(Fmt*));
int     fmtprint(Fmt *f, char *fmt, ...);
int     fmtvprint(Fmt *f, char *fmt, va_list args);
int     fmtrune(Fmt *f, int r);
int     fmtstrcpy(Fmt *f, char *s);
char*   fmtstrflush(Fmt *f);
int     fmtstrinit(Fmt *f);

/* Format helpers */
void*   fmtdispatch(Fmt *f, void *fmt, int isrunes);
int     dofmt(Fmt *f, char *fmt);
int     dorfmt(Fmt *f, const Rune *fmt);

/* Quote/escape functions */
int     fmtquote(Fmt *f, char *s, int len, char *quote);
int     quotestrfmt(Fmt *f);
int     quoterunestrfmt(Fmt *f);
void    quotefmtinstall(void);

/* Rune print functions */
int     runefmtstrinit(Fmt *f);
char*   runefmtstrflush(Fmt *f);
int     runesnprint(char *buf, int len, Rune *fmt, ...);
int     runeseprint(char *buf, char *e, Rune *fmt, ...);
int     runesmprint(char *buf, Rune *fmt, ...);
int     runevseprint(char *buf, char *e, Rune *fmt, va_list args);
int     runevsnprint(char *buf, int len, Rune *fmt, va_list args);

#endif /* LIB9_PRINT_H */
