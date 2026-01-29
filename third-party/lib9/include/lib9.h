/*
 * lib9.h - Plan 9 C Library Interface
 * Modified for Kryon compatibility layer
 *
 * Original source: TaijiOS/FreeBSD lib9.h
 */

#ifndef _LIB9_H_
#define _LIB9_H_

/* Define _BSD_SOURCE to use ISO C, POSIX, and 4.3BSD things. */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <setjmp.h>
#include <float.h>
#include <time.h>
#include <ctype.h>

#define getwd infgetwd
#define round infround
#define fmax inffmax
#define log2 inflog2

/* Math module dtoa */
#define __LITTLE_ENDIAN

#define nil ((void*)0)

typedef unsigned char uchar;
typedef signed char schar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned int Rune;
typedef long long int vlong;
typedef unsigned long long int uvlong;
typedef unsigned int u32;
typedef uvlong u64int;

typedef unsigned int mpdigit;
typedef unsigned short u16int;
typedef unsigned char u8int;
typedef unsigned long uintptr;
typedef long intptr;

typedef signed char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;

#define USED(x) if(x){}else{}
#define SET(x)

#undef nelem
#define nelem(x) (sizeof(x)/sizeof((x)[0]))
#undef offsetof
#define offsetof(s, m) (ulong)(&(((s*)0)->m))
#undef assert
#define assert(x) if(x){}else _assert("x")

/*
 * String routines (most declared by ANSI/POSIX)
 */
extern char* strecpy(char*, char*, char*);
extern char* strdup(const char*);
extern int cistrncmp(char*, char*, int);
extern int cistrcmp(char*, char*);
extern char* cistrstr(char*, char*);
extern int tokenize(char*, char**, int);

enum {
    UTFmax = 4,        /* maximum bytes per rune */
    Runesync = 0x80,   /* cannot represent part of a UTF sequence (<) */
    Runeself = 0x80,   /* rune and UTF sequences are the same (<) */
    Runeerror = 0xFFFD, /* decoding error in UTF */
    Runemax = 0x10FFFF, /* 21-bit rune */
    Runemask = 0x1FFFFF, /* bits used by runes */
    ERRMAX = 128,      /* maximum error string length */
};

/*
 * Rune routines
 */
extern int runetochar(char*, Rune*);
extern int chartorune(Rune*, char*);
extern int runelen(long);
extern int runenlen(Rune*, int);
extern int fullrune(char*, int);
extern int utflen(char*);
extern int utfnlen(char*, long);
extern char* utfrune(char*, long);
extern char* utfrrune(char*, long);
extern char* utfutf(char*, char*);
extern char* utfecpy(char*, char*, char*);

extern Rune* runestrcat(Rune*, Rune*);
extern Rune* runestrchr(Rune*, Rune);
extern int runestrcmp(Rune*, Rune*);
extern Rune* runestrcpy(Rune*, Rune*);
extern Rune* runestrncpy(Rune*, Rune*, long);
extern Rune* runestrecpy(Rune*, Rune*, Rune*);
extern Rune* runestrdup(Rune*);
extern Rune* runestrncat(Rune*, Rune*, long);
extern int runestrncmp(Rune*, Rune*, long);
extern Rune* runestrrchr(Rune*, Rune);
extern long runestrlen(Rune*);
extern Rune* runestrstr(Rune*, Rune*);

extern Rune tolowerrune(Rune);
extern Rune totitlerune(Rune);
extern Rune toupperrune(Rune);
extern int isalpharune(Rune);
extern int islowerrune(Rune);
extern int isspacerune(Rune);
extern int istitlerune(Rune);
extern int isupperrune(Rune);

/*
 * Malloc
 */
extern void* mallocz(ulong, int);

/*
 * Print routines
 */
typedef struct Fmt Fmt;
struct Fmt {
    uchar runes;           /* output buffer is runes or chars? */
    void* start;           /* of buffer */
    void* to;              /* current place in the buffer */
    void* stop;            /* end of the buffer; overwritten if flush fails */
    int (*flush)(Fmt*);    /* called when to == stop */
    void* farg;            /* to make flush a closure */
    int nfmt;              /* num chars formatted so far */
    va_list args;          /* args passed to dofmt */
    int r;                 /* % format Rune */
    int width;
    int prec;
    ulong flags;
};

enum {
    FmtWidth = 1,
    FmtLeft = FmtWidth << 1,
    FmtPrec = FmtLeft << 1,
    FmtSharp = FmtPrec << 1,
    FmtSpace = FmtSharp << 1,
    FmtSign = FmtSpace << 1,
    FmtZero = FmtSign << 1,
    FmtUnsigned = FmtZero << 1,
    FmtShort = FmtUnsigned << 1,
    FmtLong = FmtShort << 1,
    FmtVLong = FmtLong << 1,
    FmtComma = FmtVLong << 1,
    FmtByte = FmtComma << 1,
    FmtFlag = FmtByte << 1
};

extern int print(char*, ...);
extern char* seprint(char*, char*, char*, ...);
extern char* vseprint(char*, char*, char*, va_list);
extern int snprint(char*, int, char*, ...);
extern int vsnprint(char*, int, char*, va_list);
extern char* smprint(char*, ...);
extern char* vsmprint(char*, va_list);
extern int sprint(char*, char*, ...);
extern int fprint(int, char*, ...);
extern int vfprint(int, char*, va_list);

extern int runesprint(Rune*, char*, ...);
extern int runesnprint(Rune*, int, char*, ...);
extern int runevsnprint(Rune*, int, char*, va_list);
extern Rune* runeseprint(Rune*, Rune*, char*, ...);
extern Rune* runevseprint(Rune*, Rune*, char*, va_list);
extern Rune* runesmprint(char*, ...);
extern Rune* runevsmprint(char*, va_list);

extern int fmtfdinit(Fmt*, int, char*, int);
extern int fmtfdflush(Fmt*);
extern int fmtstrinit(Fmt*);
extern char* fmtstrflush(Fmt*);
extern int runefmtstrinit(Fmt*);
extern Rune* runefmtstrflush(Fmt*);

extern int fmtinstall(int, int (*)(Fmt*));
extern int dofmt(Fmt*, char*);
extern int dorfmt(Fmt*, Rune*);
extern int fmtprint(Fmt*, char*, ...);
extern int fmtvprint(Fmt*, char*, va_list);
extern int fmtrune(Fmt*, int);
extern int fmtstrcpy(Fmt*, char*);
extern int fmtrunestrcpy(Fmt*, Rune*);

/* Error string for %r */
extern int errfmt(Fmt* f);

/*
 * Quoted strings
 */
extern char* unquotestrdup(char*);
extern Rune* unquoterunestrdup(Rune*);
extern char* quotestrdup(char*);
extern Rune* quoterunestrdup(Rune*);
extern int quotestrfmt(Fmt*);
extern int quoterunestrfmt(Fmt*);
extern void quotefmtinstall(void);
extern int (*doquote)(int);

/*
 * One-of-a-kind
 */
extern void _assert(char*);
extern int getfields(char*, char**, int, int, char*);
extern void sysfatal(char*, ...);

/*
 * Math functions (wrappers for POSIX)
 */
#define isNaN(f) isnan(f)
#define isInf(f, sign) (isinf(f) && (sign == 0 || (sign > 0 ? (f) > 0 : (f) < 0)))
extern double pow10(int);

/*
 * System interface
 */
extern char* argv0;
extern void exits(char*);
extern char* getenv(const char*);
extern int putenv(char*);

extern int errstr(char*, uint);
extern void rerrstr(char*, uint);
extern void werrstr(char*, ...);

#endif /* _LIB9_H_ */
