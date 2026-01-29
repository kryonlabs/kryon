/*
 * lib9.h wrapper for Kryon
 *
 * This header provides a unified entry point for lib9 functionality.
 * It routes to the appropriate implementation based on the build mode.
 */

#ifndef KRYON_LIB9_WRAPPER_H
#define KRYON_LIB9_WRAPPER_H

#if defined(INFERNO) || defined(TAIJIOS) || defined(EMU)
    /* Native TaijiOS/Inferno build - use real lib9.h from system */
    /* This relies on -I$(INFERNO)/include being in the include path */
    /* which provides the real lib9.h */
    #error "For INFERNO builds, lib9.h should come from system includes"
#else
    /* Standalone Linux build - pull in compatibility lib9.h */
    /* We need to get third-party/lib9/include/lib9.h */
    /* Since -I$(INCLUDE_DIR) comes first, we use a relative path */
    #include "../third-party/lib9/include/lib9.h"
#endif

#endif /* KRYON_LIB9_WRAPPER_H */
