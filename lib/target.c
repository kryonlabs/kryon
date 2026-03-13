/*
 * Kryon Build System - Target Implementation
 * C89 compliant
 *
 * Target platform definitions and management
 */

#include <lib9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "target.h"

/*
 * Target configurations
 */

/* Linux native target */
KryonTarget g_target_linux = {
    "linux",
    KRYON_PLATFORM_LINUX,
    KRYON_ARCH_X86_64,
    "gcc",           /* CC */
    NULL,            /* CXX */
    "-std=c89 -Wall -Wpedantic -g -D_POSIX_C_SOURCE=200112L",
    "-lm -lpthread",
    "-lSDL2 -lssl -lcrypto -lasound",
    "build/linux",
    "",
    0,               /* needs_emscripten */
    0                /* needs_android_ndk */
};

/* AppImage target */
KryonTarget g_target_appimage = {
    "appimage",
    KRYON_PLATFORM_APPIMAGE,
    KRYON_ARCH_X86_64,
    "gcc",
    NULL,
    "-std=c89 -Wall -Wpedantic -g -D_POSIX_C_SOURCE=200112L",
    "-lm -lpthread",
    "-lSDL2 -lssl -lcrypto -lasound",
    "build/appimage",
    ".AppImage",
    0,
    0
};

/* WebAssembly target */
KryonTarget g_target_wasm = {
    "wasm",
    KRYON_PLATFORM_WASM,
    KRYON_ARCH_X86_64,  /* WebAssembly is arch-agnostic */
    "emcc",          /* Emscripten compiler */
    "em++",
    "-std=c89 -Wall -Wpedantic -g -D_POSIX_C_SOURCE=200112L "
    "-s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MAX_WEBGL_VERSION=2 "
    "-s USE_SDL=2 -s SDL2_IMAGE_FORMATS='[\"png\"]'",
    "-s USE_SDL=2 -s USE_SSL=1 -lws",
    "",
    "build/web",
    ".js",           /* Emscripten outputs .js + .wasm */
    1,               /* needs_emscripten */
    0
};

/* Android target */
KryonTarget g_target_android = {
    "android",
    KRYON_PLATFORM_ANDROID,
    KRYON_ARCH_ARM64,
    "aarch64-linux-android21-clang",
    "aarch64-linux-android21-clang++",
    "-std=c89 -Wall -Wpedantic -g -D_POSIX_C_SOURCE=200112L "
    "-DANDROID -fPIC",
    "-llog -landroid -lGLESv3 -lEGL",
    "",
    "build/android",
    ".apk",
    0,
    1                /* needs_android_ndk */
};

/*
 * Find target by name
 */
const KryonTarget* target_find(const char *name)
{
    if (name == NULL) {
        return &g_target_linux;  /* Default */
    }

    if (strcmp(name, "linux") == 0) {
        return &g_target_linux;
    } else if (strcmp(name, "appimage") == 0) {
        return &g_target_appimage;
    } else if (strcmp(name, "wasm") == 0) {
        return &g_target_wasm;
    } else if (strcmp(name, "android") == 0) {
        return &g_target_android;
    }

    return NULL;
}

/*
 * Check if target toolchain is available
 */
int target_is_available(const KryonTarget *target)
{
    if (target == NULL) {
        return 0;
    }

    /* Check if compiler exists */
    if (target->cc != NULL) {
        char cmd[256];
        int result;

        snprint(cmd, sizeof(cmd), "which %s > /dev/null 2>&1", target->cc);
        result = system(cmd);
        if (result != 0) {
            return 0;  /* Compiler not found */
        }
    }

    /* Check for Emscripten */
    if (target->needs_emscripten) {
        if (system("which emcc > /dev/null 2>&1") != 0) {
            return 0;
        }
    }

    /* Check for Android NDK */
    if (target->needs_android_ndk) {
        const char *ndk = getenv("ANDROID_NDK");
        if (ndk == NULL) {
            return 0;
        }
    }

    return 1;
}

/*
 * Get platform name as string
 */
const char* target_get_platform_name(KryonPlatform platform)
{
    switch (platform) {
        case KRYON_PLATFORM_LINUX:    return "Linux";
        case KRYON_PLATFORM_APPIMAGE: return "AppImage";
        case KRYON_PLATFORM_WASM:     return "WebAssembly";
        case KRYON_PLATFORM_ANDROID:  return "Android";
        default:                       return "Unknown";
    }
}

/*
 * Create output directories for target
 */
int target_create_output_dirs(const KryonTarget *target, const char *project_dir,
                               KryonBuildOutput *output)
{
    char path[KRYON_MAX_PATH];
    struct stat st;

    if (target == NULL || project_dir == NULL || output == NULL) {
        return -1;
    }

    /* Create output directory */
    snprint(path, sizeof(path), "%s/%s", project_dir, target->output_dir);
    if (stat(path, &st) != 0) {
        /* Create directory */
        char cmd[KRYON_MAX_PATH];
        snprint(cmd, sizeof(cmd), "mkdir -p %s", path);
        if (system(cmd) != 0) {
            return -1;
        }
    }

    /* Initialize output structure */
    snprint(output->binary, sizeof(output->binary), "%s/app%s",
          target->output_dir, target->output_ext);
    snprint(output->resources, sizeof(output->resources), "%s/resources",
          target->output_dir);
    snprint(output->assets, sizeof(output->assets), "%s/assets",
          target->output_dir);

    /* Create subdirectories */
    snprint(path, sizeof(path), "%s/%s/resources", project_dir, target->output_dir);
    mkdir(path, 0755);

    snprint(path, sizeof(path), "%s/%s/assets", project_dir, target->output_dir);
    mkdir(path, 0755);

    return 0;
}

/*
 * Copy assets to output directory
 */
int target_copy_assets(const KryonTarget *target, KryonBuildOutput *output)
{
    /* TODO: Implement asset copying */
    (void)target;
    (void)output;
    return 0;
}

/*
 * Post-process build output
 */
int target_post_process(const KryonTarget *target, KryonBuildOutput *output)
{
    char cmd[KRYON_MAX_PATH * 2];

    if (target == NULL || output == NULL) {
        return -1;
    }

    /* For AppImage: run AppImage bundler */
    if (target->platform == KRYON_PLATFORM_APPIMAGE) {
        printf("Running AppImage bundler...\n");
        snprint(cmd, sizeof(cmd), "scripts/appimage-builder.sh %s %s",
               output->binary, target->output_dir);
        return system(cmd);
    }

    /* For Android: run gradle build */
    if (target->platform == KRYON_PLATFORM_ANDROID) {
        printf("Running Gradle build...\n");
        snprint(cmd, sizeof(cmd), "cd platform/android && ./gradlew assembleRelease");
        return system(cmd);
    }

    /* For WASM: no post-processing needed */
    /* For Linux: no post-processing needed */
    return 0;
}

/*
 * Build project for target
 */
int target_build(const KryonTarget *target, const char *project_dir)
{
    KryonBuildOutput output;
    char cmd[KRYON_MAX_PATH * 2];
    int result;

    if (target == NULL || project_dir == NULL) {
        return -1;
    }

    printf("Building for %s (%s)...\n", target->name,
           target_get_platform_name(target->platform));

    /* Create output directories */
    if (target_create_output_dirs(target, project_dir, &output) < 0) {
        fprintf(stderr, "Failed to create output directories\n");
        return -1;
    }

    /* Build based on platform */
    switch (target->platform) {
        case KRYON_PLATFORM_LINUX:
            printf("Building native Linux binary...\n");
            /* Use make to build */
            snprint(cmd, sizeof(cmd), "make clean && make all");
            result = system(cmd);
            if (result != 0) {
                fprintf(stderr, "Build failed\n");
                return -1;
            }
            /* Copy binary to output directory */
            snprint(cmd, sizeof(cmd), "cp bin/kryon-wm %s 2>/dev/null || true",
                  target->output_dir);
            system(cmd);
            break;

        case KRYON_PLATFORM_APPIMAGE:
            printf("Building AppImage package...\n");
            /* First build Linux binary */
            snprint(cmd, sizeof(cmd), "make clean && make all");
            result = system(cmd);
            if (result != 0) {
                fprintf(stderr, "Build failed\n");
                return -1;
            }
            /* Post-process to create AppImage */
            result = target_post_process(target, &output);
            if (result != 0) {
                fprintf(stderr, "AppImage bundling failed\n");
                return -1;
            }
            break;

        case KRYON_PLATFORM_WASM:
            printf("Building WebAssembly version...\n");
            /* Use Emscripten build script */
            snprint(cmd, sizeof(cmd), "platform/wasm/build.sh");
            result = system(cmd);
            if (result != 0) {
                fprintf(stderr, "WebAssembly build failed\n");
                return -1;
            }
            break;

        case KRYON_PLATFORM_ANDROID:
            printf("Building Android APK...\n");
            /* Use Gradle build */
            result = target_post_process(target, &output);
            if (result != 0) {
                fprintf(stderr, "Android build failed\n");
                return -1;
            }
            break;

        default:
            fprintf(stderr, "Unknown platform\n");
            return -1;
    }

    /* Copy assets */
    target_copy_assets(target, &output);

    printf("Build complete: %s\n", output.binary);
    return 0;
}
