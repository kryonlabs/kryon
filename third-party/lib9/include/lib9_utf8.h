#ifndef LIB9_UTF8_H
#define LIB9_UTF8_H

#include "lib9_types.h"

/*
 * lib9 UTF-8 and Rune Support
 *
 * Functions for encoding/decoding UTF-8 and working with Unicode runes.
 */

/* UTF-8 encoding/decoding */
int     chartorune(Rune *rune, char *str);
int     runetochar(char *str, Rune *rune);
int     runelen(long c);
int     fullrune(char *str, int n);

/* UTF-8 string operations */
int     utflen(char *s);
int     utfnlen(char *s, long n);
char*   utfrune(char *s, long c);
char*   utfrrune(char *s, long c);
char*   utfecpy(char *to, char *e, char *from);

/* Rune string operations */
int     runestrlen(Rune *s);
Rune*   runestrchr(Rune *s, Rune c);
int     runestrcmp(Rune *s1, Rune *s2);
Rune*   runestrcpy(Rune *s1, Rune *s2);
Rune*   runestrdup(Rune *s);
Rune*   runestrecpy(Rune *s1, Rune *e, Rune *s2);
Rune*   runestrncpy(Rune *s1, Rune *s2, long n);
Rune*   runestrcat(Rune *s1, Rune *s2);
Rune*   runestrncat(Rune *s1, Rune *s2, long n);
Rune*   runestrstr(Rune *s1, Rune *s2);

/* Rune character classification */
int     isalpharune(Rune c);
int     isbaserune(Rune c);
int     isdigitrune(Rune c);
int     islowerrune(Rune c);
int     isspacerune(Rune c);
int     istitlerune(Rune c);
int     isupperrune(Rune c);

/* Rune case conversion */
Rune    tolowerrune(Rune c);
Rune    totitlerune(Rune c);
Rune    toupperrune(Rune c);

#endif /* LIB9_UTF8_H */
