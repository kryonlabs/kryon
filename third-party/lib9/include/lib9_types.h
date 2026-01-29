#ifndef LIB9_TYPES_H
#define LIB9_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

/*
 * lib9 Type Definitions
 *
 * Provides lib9 types as aliases to standard C types for compatibility.
 */

/* Basic type aliases */
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long vlong;
typedef unsigned long long uvlong;

/* Rune type for Unicode code points */
typedef int Rune;

/* nil macro */
#ifndef nil
#define nil ((void*)0)
#endif

/* Boolean-like constants */
#ifndef nelem
#define nelem(x) (sizeof(x)/sizeof((x)[0]))
#endif

#ifndef USED
#define USED(x) if(x){}else{}
#endif

#ifndef SET
#define SET(x) ((x)=0)
#endif

/* Standard constants */
#ifndef OREAD
enum {
    OREAD   = 0,     /* open for read */
    OWRITE  = 1,     /* write */
    ORDWR   = 2,     /* read and write */
    OEXEC   = 3,     /* execute, == read but check execute permission */
    OTRUNC  = 16,    /* or'ed in (except for exec), truncate file first */
    OCEXEC  = 32,    /* or'ed in, close on exec */
    ORCLOSE = 64,    /* or'ed in, remove on close */
    OEXCL   = 0x1000 /* or'ed in, exclusive use (create only) */
};
#endif

/* UTF-8 and Rune constants */
enum {
    UTFmax      = 4,        /* maximum bytes per rune */
    Runesync    = 0x80,     /* cannot represent part of a UTF sequence (<) */
    Runeself    = 0x80,     /* rune and UTF sequences are the same (<) */
    Runeerror   = 0xFFFD,   /* decoding error in UTF */
    Runemax     = 0x10FFFF, /* maximum rune value */
};

#endif /* LIB9_TYPES_H */
