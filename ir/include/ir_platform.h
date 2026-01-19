/**
 * Platform Abstraction Header
 *
 * Provides compatibility layer between POSIX (Linux/macOS/Windows)
 * and Plan 9/9front C environments
 */

#ifndef IR_PLATFORM_H
#define IR_PLATFORM_H

#ifdef PLAN9
    /* Plan 9 standard headers */
    #include <u.h>
    #include <libc.h>

    /* Function name mappings */
    #define snprintf snprint
    #define fprintf fprint
    #define printf print
    #define sprintf sprint

    /* Type mappings */
    #define int8_t schar
    #define uint8_t uchar
    #define int16_t short
    #define uint16_t ushort
    #define int32_t long
    #define uint32_t ulong
    #define int64_t vlong
    #define uint64_t uvlong

    /* NULL is nil in Plan 9 */
    #ifndef NULL
        #define NULL nil
    #endif

    /* Bool type (Plan 9 C doesn't have <stdbool.h>) */
    #ifndef bool
        #define bool int
        #define true 1
        #define false 0
    #endif

#else
    /* POSIX standard headers */
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <stdint.h>
    #include <stdbool.h>

    /* Plan 9 compatibility for POSIX */
    #ifndef nil
        #define nil NULL
    #endif
#endif

#endif /* IR_PLATFORM_H */
