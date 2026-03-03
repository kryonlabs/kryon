/*
 * Kryon Build System - Target Abstraction
 * C89 compliant
 *
 * Defines target platform abstraction for multi-platform builds
 */

#ifndef KRYON_TARGET_H
#define KRYON_TARGET_H

#include <stdio.h>

/* Maximum path lengths */
#define KRYON_MAX_PATH 512

/* Target platforms */
typedef enum KryonPlatform {
    KRYON_PLATFORM_LINUX,
    KRYON_PLATFORM_APPIMAGE,
    KRYON_PLATFORM_WASM,
    KRYON_PLATFORM_ANDROID
} KryonPlatform;

/* Target architecture */
typedef enum KryonArch {
    KRYON_ARCH_X86_64,
    KRYON_ARCH_ARM64,
    KRYON_ARCH_ARMv7
} KryonArch;

/* Target configuration */
typedef struct KryonTarget {
    const char *name;
    KryonPlatform platform;
    KryonArch arch;
    const char *cc;              /* C compiler */
    const char *cxx;             /* C++ compiler (for linkers) */
    const char *cflags;
    const char *ldflags;
    const char *libs;
    const char *output_dir;       /* Relative to build/ */
    const char *output_ext;       /* e.g., "", ".appimage", ".wasm" */
    int needs_emscripten;        /* Requires Emscripten */
    int needs_android_ndk;       /* Requires Android NDK */
} KryonTarget;

/* Target definitions */
extern KryonTarget g_target_linux;
extern KryonTarget g_target_appimage;
extern KryonTarget g_target_wasm;
extern KryonTarget g_target_android;

/* Target management functions */
const KryonTarget* target_find(const char *name);
int target_is_available(const KryonTarget *target);
int target_build(const KryonTarget *target, const char *project_dir);
const char* target_get_platform_name(KryonPlatform platform);

/* Build output functions */
typedef struct KryonBuildOutput {
    char binary[KRYON_MAX_PATH];    /* Main executable/bundle */
    char resources[KRYON_MAX_PATH]; /* Resource directory */
    char assets[KRYON_MAX_PATH];     /* Assets directory */
} KryonBuildOutput;

int target_create_output_dirs(const KryonTarget *target, const char *project_dir,
                               KryonBuildOutput *output);
int target_copy_assets(const KryonTarget *target, KryonBuildOutput *output);
int target_post_process(const KryonTarget *target, KryonBuildOutput *output);

#endif /* KRYON_TARGET_H */
