#ifndef KRYON_LIB9_COMPAT_H
#define KRYON_LIB9_COMPAT_H

/*
 * lib9 Compatibility Layer Routing Header
 *
 * This header provides a unified entry point for lib9 functionality.
 * It routes to the appropriate implementation based on the build mode:
 *
 * - TaijiOS/Inferno native build: Uses real lib9.h from TaijiOS
 * - Standalone Linux build: Uses compatibility layer implementation
 */

#if defined(INFERNO) || defined(TAIJIOS) || defined(EMU)
    /* Native TaijiOS/Inferno build - use real lib9.h from TaijiOS */
    #include <lib9.h>
#else
    /* Standalone Linux build - use compatibility layer */
    /* The main lib9.h from third-party includes all declarations */
    #ifndef _LIB9_H_
        #include <lib9.h>
    #endif
#endif

#endif /* KRYON_LIB9_COMPAT_H */
