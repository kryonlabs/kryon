/*
 * plan9_ui_globals.c - default embedded-asset table for the native Plan 9
 * Kryon library build.
 */
#if defined(KRYON_PLATFORM_PLAN9) || defined(KRYON_NATIVE_PLAN9)

#include "embedded_assets.h"

const EmbeddedAsset embedded_assets[] = {{0, 0, 0, 0}};
const unsigned int embedded_asset_count = 0;

#endif /* KRYON_PLATFORM_PLAN9 */
