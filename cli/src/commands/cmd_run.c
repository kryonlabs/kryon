/**
 * Run Command
 * Executes source files or KIR files using C backend
 */

#include "kryon_cli.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_serialization.h"
#include "../../ir/ir_executor.h"
#include "../../backends/desktop/ir_desktop_renderer.h"
#include "../../codegens/kotlin/kotlin_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* detect_frontend(const char* file) {
    const char* ext = path_extension(file);

    if (strcmp(ext, ".kry") == 0) return "kry";
    else if (strcmp(ext, ".kir") == 0) return "kir";
    else if (strcmp(ext, ".md") == 0) return "markdown";
    else if (strcmp(ext, ".html") == 0) return "html";
    else if (strcmp(ext, ".tsx") == 0 || strcmp(ext, ".jsx") == 0) return "tsx";
    else if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) return "c";
    else if (strcmp(ext, ".lua") == 0) return "lua";
    else return NULL;
}

static int android_setup_temp_project(const char* temp_dir, const char* kir_file) {
    char cmd[4096];

    // Create directory structure
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/app/src/main/assets\"", temp_dir);
    if (system(cmd) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/app/src/main/kotlin/com/kryon/temp\"", temp_dir);
    if (system(cmd) != 0) return -1;

    // Copy KIR file to assets
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s/app/src/main/assets/app.kir\"", kir_file, temp_dir);
    if (system(cmd) != 0) return -1;

    // Create local.properties with SDK location (not ndk.dir - that's deprecated)
    char local_props[2048];
    snprintf(local_props, sizeof(local_props), "%s/local.properties", temp_dir);
    FILE* props_f = fopen(local_props, "w");
    if (props_f) {
        const char* android_sdk = getenv("ANDROID_HOME");
        if (android_sdk) {
            fprintf(props_f, "sdk.dir=%s\n", android_sdk);
        }
        // Don't set ndk.dir - use ndkVersion in build.gradle.kts instead
        fclose(props_f);
    }

    // Create settings.gradle.kts with plugin management
    char settings_file[2048];
    snprintf(settings_file, sizeof(settings_file), "%s/settings.gradle.kts", temp_dir);
    FILE* f = fopen(settings_file, "w");
    if (!f) return -1;
    fprintf(f, "pluginManagement {\n");
    fprintf(f, "    repositories {\n");
    fprintf(f, "        google()\n");
    fprintf(f, "        mavenCentral()\n");
    fprintf(f, "        gradlePluginPortal()\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n\n");
    fprintf(f, "dependencyResolutionManagement {\n");
    fprintf(f, "    repositoriesMode.set(RepositoriesMode.PREFER_SETTINGS)\n");
    fprintf(f, "    repositories {\n");
    fprintf(f, "        google()\n");
    fprintf(f, "        mavenCentral()\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n\n");
    fprintf(f, "rootProject.name = \"kryon-temp\"\n");
    fprintf(f, "include(\":app\")\n");
    fprintf(f, "include(\":kryon\")\n");
    fprintf(f, "project(\":kryon\").projectDir = file(\"/mnt/storage/Projects/kryon/bindings/kotlin\")\n");
    fclose(f);

    // Create gradle.properties with AndroidX enabled
    char gradle_props[2048];
    snprintf(gradle_props, sizeof(gradle_props), "%s/gradle.properties", temp_dir);
    f = fopen(gradle_props, "w");
    if (!f) return -1;
    fprintf(f, "android.useAndroidX=true\n");
    fprintf(f, "android.enableJetifier=true\n");
    fclose(f);

    // Create root build.gradle.kts (proper multi-module pattern)
    char root_build[2048];
    snprintf(root_build, sizeof(root_build), "%s/build.gradle.kts", temp_dir);
    f = fopen(root_build, "w");
    if (!f) return -1;
    fprintf(f, "// Top-level build file for Kryon temporary Android project\n");
    fprintf(f, "plugins {\n");
    fprintf(f, "    id(\"com.android.application\") version \"8.2.0\" apply false\n");
    fprintf(f, "    id(\"com.android.library\") version \"8.2.0\" apply false\n");
    fprintf(f, "    id(\"org.jetbrains.kotlin.android\") version \"1.9.20\" apply false\n");
    fprintf(f, "}\n\n");
    fprintf(f, "allprojects {\n");
    fprintf(f, "    repositories {\n");
    fprintf(f, "        google()\n");
    fprintf(f, "        mavenCentral()\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n");
    fclose(f);

    // Create app/build.gradle.kts
    char app_build[2048];
    snprintf(app_build, sizeof(app_build), "%s/app/build.gradle.kts", temp_dir);
    f = fopen(app_build, "w");
    if (!f) return -1;
    fprintf(f, "plugins {\n");
    fprintf(f, "    id(\"com.android.application\")\n");
    fprintf(f, "    id(\"org.jetbrains.kotlin.android\")\n");
    fprintf(f, "}\n\n");
    fprintf(f, "android {\n");
    fprintf(f, "    namespace = \"com.kryon.temp\"\n");
    fprintf(f, "    compileSdk = 34\n\n");
    fprintf(f, "    defaultConfig {\n");
    fprintf(f, "        applicationId = \"com.kryon.temp\"\n");
    fprintf(f, "        minSdk = 26\n");
    fprintf(f, "        targetSdk = 34\n");
    fprintf(f, "        versionCode = 1\n");
    fprintf(f, "        versionName = \"1.0\"\n");
    fprintf(f, "    }\n\n");
    fprintf(f, "    compileOptions {\n");
    fprintf(f, "        sourceCompatibility = JavaVersion.VERSION_17\n");
    fprintf(f, "        targetCompatibility = JavaVersion.VERSION_17\n");
    fprintf(f, "    }\n\n");
    fprintf(f, "    kotlinOptions {\n");
    fprintf(f, "        jvmTarget = \"17\"\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n\n");
    fprintf(f, "dependencies {\n");
    fprintf(f, "    implementation(project(\":kryon\"))\n");
    fprintf(f, "}\n");
    fclose(f);

    // Create AndroidManifest.xml
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/app/src/main\"", temp_dir);
    system(cmd);

    char manifest[2048];
    snprintf(manifest, sizeof(manifest), "%s/app/src/main/AndroidManifest.xml", temp_dir);
    f = fopen(manifest, "w");
    if (!f) return -1;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    fprintf(f, "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\">\n");
    fprintf(f, "    <application\n");
    fprintf(f, "        android:label=\"Kryon App\"\n");
    fprintf(f, "        android:theme=\"@android:style/Theme.NoTitleBar.Fullscreen\">\n");
    fprintf(f, "        <activity\n");
    fprintf(f, "            android:name=\".MainActivity\"\n");
    fprintf(f, "            android:exported=\"true\"\n");
    fprintf(f, "            android:screenOrientation=\"portrait\">\n");
    fprintf(f, "            <intent-filter>\n");
    fprintf(f, "                <action android:name=\"android.intent.action.MAIN\" />\n");
    fprintf(f, "                <category android:name=\"android.intent.category.LAUNCHER\" />\n");
    fprintf(f, "            </intent-filter>\n");
    fprintf(f, "        </activity>\n");
    fprintf(f, "    </application>\n");
    fprintf(f, "</manifest>\n");
    fclose(f);

    // Generate MainActivity.kt from KIR using Kotlin codegen
    char mainactivity[2048];
    snprintf(mainactivity, sizeof(mainactivity), "%s/app/src/main/kotlin/com/kryon/temp/MainActivity.kt", temp_dir);

    printf("Generating Kotlin code: %s\n", mainactivity);
    if (!ir_generate_kotlin_code(kir_file, mainactivity)) {
        fprintf(stderr, "Error: Failed to generate Kotlin code from KIR\n");
        return -1;
    }
    printf("✓ Generated MainActivity.kt from KIR\n");

    return 0;
}

static int run_android(const char* kir_file, const char* source_file) {
    printf("Android target selected\n");

    // 1. Validate environment variables
    const char* android_home = getenv("ANDROID_HOME");
    const char* android_ndk = getenv("ANDROID_NDK_HOME");

    if (!android_home) {
        fprintf(stderr, "Error: ANDROID_HOME not set\n");
        fprintf(stderr, "Set it to your Android SDK path, e.g.:\n");
        fprintf(stderr, "  export ANDROID_HOME=$HOME/Android/Sdk\n");
        return 1;
    }

    if (!android_ndk) {
        fprintf(stderr, "Error: ANDROID_NDK_HOME not set\n");
        fprintf(stderr, "Install NDK via: sdkmanager --install 'ndk;26.1.10909125'\n");
        fprintf(stderr, "Then set: export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/26.1.10909125\n");
        return 1;
    }

    // 2. Check for connected device
    printf("Checking for connected devices...\n");
    FILE* adb_check = popen("adb devices 2>&1 | grep -w 'device' | head -1", "r");
    if (!adb_check) {
        fprintf(stderr, "Error: Failed to run adb command\n");
        fprintf(stderr, "Make sure adb is installed and in your PATH\n");
        return 1;
    }

    char device_line[256] = {0};
    fgets(device_line, sizeof(device_line), adb_check);
    pclose(adb_check);

    if (strlen(device_line) == 0) {
        fprintf(stderr, "\n");
        fprintf(stderr, "╭────────────────────────────────────────────────────────╮\n");
        fprintf(stderr, "│  No Android devices found                             │\n");
        fprintf(stderr, "╰────────────────────────────────────────────────────────╯\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Please connect an Android device and enable USB debugging:\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "  1. Connect your device via USB\n");
        fprintf(stderr, "  2. Enable Developer Options:\n");
        fprintf(stderr, "     Settings → About Phone → Tap \"Build Number\" 7 times\n");
        fprintf(stderr, "  3. Enable USB Debugging:\n");
        fprintf(stderr, "     Settings → Developer Options → USB Debugging\n");
        fprintf(stderr, "  4. Verify connection:\n");
        fprintf(stderr, "     adb devices\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Then run this command again.\n");
        fprintf(stderr, "\n");
        return 1;
    }

    // Extract device ID (before tab character)
    char device_id[128] = {0};
    char* tab = strchr(device_line, '\t');
    if (tab) {
        size_t len = tab - device_line;
        if (len < sizeof(device_id)) {
            strncpy(device_id, device_line, len);
            device_id[len] = '\0';
        }
    } else {
        // No tab, use whole line (trim newline)
        strncpy(device_id, device_line, sizeof(device_id) - 1);
        char* newline = strchr(device_id, '\n');
        if (newline) *newline = '\0';
    }

    printf("Using device: %s\n", device_id);

    // 3. Create temporary project directory
    char temp_dir[512];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/kryon_android_%d", getpid());

    printf("Creating temporary Android project: %s\n", temp_dir);
    if (android_setup_temp_project(temp_dir, kir_file) != 0) {
        fprintf(stderr, "Error: Failed to setup Android project\n");
        return 1;
    }

    // Also create local.properties in bindings/kotlin for subproject
    char bindings_props[2048];
    snprintf(bindings_props, sizeof(bindings_props), "/mnt/storage/Projects/kryon/bindings/kotlin/local.properties");
    FILE* bindings_f = fopen(bindings_props, "w");
    if (bindings_f) {
        if (android_home) {
            fprintf(bindings_f, "sdk.dir=%s\n", android_home);
        }
        // Don't set ndk.dir - use ndkVersion in build.gradle.kts instead
        fclose(bindings_f);
    }

    // 4. Build APK with Gradle (native libs built automatically via buildNativeLibs task)
    printf("Building APK (this may take a minute)...\n");

    // Check if gradle is available
    int gradle_check = system("command -v gradle >/dev/null 2>&1");
    if (gradle_check != 0) {
        fprintf(stderr, "Error: Gradle not found in PATH\n");
        fprintf(stderr, "Please install Gradle:\n");
        fprintf(stderr, "  - NixOS: nix-env -iA nixpkgs.gradle\n");
        fprintf(stderr, "  - Ubuntu/Debian: sudo apt install gradle\n");
        fprintf(stderr, "  - Or download from: https://gradle.org/install/\n");
        fprintf(stderr, "Temporary project preserved at: %s\n", temp_dir);
        return 1;
    }

    // Build using gradle
    char build_cmd[2048];
    snprintf(build_cmd, sizeof(build_cmd),
             "cd \"%s\" && gradle assembleDebug 2>&1",
             temp_dir);

    int build_result = system(build_cmd);
    if (build_result != 0) {
        fprintf(stderr, "Error: Gradle build failed\n");
        fprintf(stderr, "Temporary project preserved at: %s\n", temp_dir);
        fprintf(stderr, "You can debug it manually with: cd %s && gradle assembleDebug\n", temp_dir);
        return 1;
    }

    printf("✓ APK built successfully\n");

    // 5. Install APK
    char apk_path[1024];
    snprintf(apk_path, sizeof(apk_path), "%s/app/build/outputs/apk/debug/app-debug.apk", temp_dir);

    printf("Installing APK on device %s...\n", device_id);
    char install_cmd[2048];
    snprintf(install_cmd, sizeof(install_cmd),
             "adb -s \"%s\" install -r \"%s\" 2>&1",
             device_id, apk_path);

    int install_result = system(install_cmd);
    if (install_result != 0) {
        fprintf(stderr, "Error: Failed to install APK\n");
        fprintf(stderr, "Try manually: adb uninstall com.kryon.temp\n");
        return 1;
    }

    printf("✓ APK installed\n");

    // 6. Launch app
    printf("Launching app...\n");
    char launch_cmd[2048];
    snprintf(launch_cmd, sizeof(launch_cmd),
             "adb -s \"%s\" shell am start -n com.kryon.temp/.MainActivity 2>&1",
             device_id);

    int launch_result = system(launch_cmd);
    if (launch_result != 0) {
        fprintf(stderr, "Warning: Failed to launch app (but it's installed)\n");
    } else {
        printf("✓ App launched\n");
    }

    // 7. Show log instructions
    printf("\nTo view logs, run:\n");
    printf("  adb logcat | grep -i kryon\n\n");

    // 8. Cleanup temp directory
    printf("Cleaning up temporary files...\n");
    char cleanup_cmd[1024];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", temp_dir);
    system(cleanup_cmd);

    printf("✓ Done!\n");
    _exit(0);  // Use _exit to skip libc cleanup and avoid double-free
}

int cmd_run(int argc, char** argv) {
    const char* target_file = NULL;
    bool free_target = false;
    const char* target_platform = NULL;
    bool explicit_target = false;

    // Parse --target flag
    int new_argc = 0;
    char* new_argv[128];
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--target=", 9) == 0) {
            target_platform = argv[i] + 9;
            explicit_target = true;
        } else if (new_argc < 128) {
            new_argv[new_argc++] = argv[i];
        }
    }

    // Update argc/argv to exclude --target flag
    argc = new_argc;
    for (int i = 0; i < new_argc; i++) {
        argv[i] = new_argv[i];
    }

    // Default to desktop if not specified
    if (!target_platform) {
        target_platform = "desktop";
    }

    // Validate target platform
    if (strcmp(target_platform, "desktop") != 0 &&
        strcmp(target_platform, "android") != 0 &&
        strcmp(target_platform, "web") != 0) {
        fprintf(stderr, "Error: Invalid target platform: %s\n", target_platform);
        fprintf(stderr, "Supported targets: desktop, android, web\n");
        return 1;
    }

    // If no file specified, use entry from config
    if (argc < 1) {
        KryonConfig* config = config_find_and_load();
        if (!config) {
            fprintf(stderr, "Error: No file specified and no kryon.toml found\n\n");
            fprintf(stderr, "Usage: kryon run <file>\n\n");
            fprintf(stderr, "Supported file types:\n");
            fprintf(stderr, "  .kry        - Kryon DSL\n");
            fprintf(stderr, "  .kir        - Kryon IR (compiled)\n");
            fprintf(stderr, "  .md         - Markdown documents\n");
            fprintf(stderr, "  .tsx, .jsx  - TypeScript/JavaScript with JSX\n");
            fprintf(stderr, "  .lua        - Lua with Kryon API\n");
            return 1;
        }

        if (!config->build_entry) {
            fprintf(stderr, "Error: No file specified and no build.entry in kryon.toml\n\n");
            fprintf(stderr, "Add to your kryon.toml:\n");
            fprintf(stderr, "  [build]\n");
            fprintf(stderr, "  entry = \"your-file.kry\"\n\n");
            fprintf(stderr, "Or run with a file:\n");
            fprintf(stderr, "  kryon run <file>\n");
            config_free(config);
            return 1;
        }

        target_file = str_copy(config->build_entry);
        free_target = true;
        config_free(config);
    } else {
        target_file = argv[0];
    }

    // Check if file exists
    if (!file_exists(target_file)) {
        fprintf(stderr, "Error: File not found: %s\n", target_file);
        if (free_target) free((char*)target_file);
        return 1;
    }

    // Detect frontend and validate BEFORE printing "Running"
    const char* frontend = detect_frontend(target_file);
    if (!frontend) {
        const char* ext = path_extension(target_file);
        fprintf(stderr, "Error: Unsupported file type: %s\n\n", ext);
        fprintf(stderr, "Supported INPUT formats:\n");
        fprintf(stderr, "  .kry        - Kryon DSL\n");
        fprintf(stderr, "  .kir        - Kryon IR (compiled)\n");
        fprintf(stderr, "  .md         - Markdown documents\n");
        fprintf(stderr, "  .html       - HTML documents\n");
        fprintf(stderr, "  .tsx, .jsx  - TypeScript/JavaScript with JSX\n");
        fprintf(stderr, "  .c, .h      - C source with Kryon API\n");
        fprintf(stderr, "  .lua        - Lua with Kryon API\n");
        fprintf(stderr, "\nNote: .nim is an OUTPUT format (generated by codegen)\n");
        fprintf(stderr, "Use: kryon codegen <lang> <input.kir> <output>\n");
        if (free_target) free((char*)target_file);
        return 1;
    }

    printf("Running: %s\n", target_file);

    // Get the path to the desktop renderer library
    const char* desktop_lib = getenv("KRYON_DESKTOP_LIB");
    if (!desktop_lib) {
        // Try default locations
        if (file_exists("/home/wao/.local/lib/libkryon_desktop.so")) {
            desktop_lib = "/home/wao/.local/lib/libkryon_desktop.so";
        } else if (file_exists("/mnt/storage/Projects/kryon/build/libkryon_desktop.so")) {
            desktop_lib = "/mnt/storage/Projects/kryon/build/libkryon_desktop.so";
        } else {
            fprintf(stderr, "Error: Desktop renderer library not found\n");
            fprintf(stderr, "Please run: make install\n");
            fprintf(stderr, "Or set KRYON_DESKTOP_LIB environment variable\n");
            if (free_target) free((char*)target_file);
            return 1;
        }
    }

    // Build command to run the file
    char cmd[4096];

    // For C files, compile to executable and run directly (not KIR)
    if (strcmp(frontend, "c") == 0) {
        // C files need to be compiled to native executable
        char exe_file[1024];
        snprintf(exe_file, sizeof(exe_file), "/tmp/kryon_app_%d", getpid());

        printf("Compiling %s to native executable...\n", target_file);
        char compile_cmd[2048];
        snprintf(compile_cmd, sizeof(compile_cmd),
                 "gcc -std=c99 -O2 \"%s\" -o \"%s\" "
                 "-I/mnt/storage/Projects/kryon/bindings/c "
                 "-I/mnt/storage/Projects/kryon/ir "
                 "-I/mnt/storage/Projects/kryon/backends/desktop "
                 "-L/mnt/storage/Projects/kryon/build "
                 "-L/mnt/storage/Projects/kryon/bindings/c "
                 "-lkryon_c -lkryon_ir -lkryon_desktop -lm -lraylib 2>&1",
                 target_file, exe_file);

        int result = system(compile_cmd);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to compile C file\n");
            if (free_target) free((char*)target_file);
            return 1;
        }

        printf("✓ Compiled successfully\n");
        printf("Running application...\n");

        // Run the executable
        char run_cmd[2048];
        snprintf(run_cmd, sizeof(run_cmd),
                 "LD_LIBRARY_PATH=/mnt/storage/Projects/kryon/build:$LD_LIBRARY_PATH \"%s\"",
                 exe_file);
        result = system(run_cmd);

        // Cleanup
        remove(exe_file);

        if (free_target) free((char*)target_file);
        return result == 0 ? 0 : 1;
    }

    if (strcmp(frontend, "kir") == 0) {
        // Route to Android if target is Android
        if (strcmp(target_platform, "android") == 0) {
            int result = run_android(target_file, target_file);
            if (free_target) free((char*)target_file);
            return result;
        }

        // Run KIR file directly on desktop
        snprintf(cmd, sizeof(cmd),
                 "LD_LIBRARY_PATH=/home/wao/.local/lib:\"/mnt/storage/Projects/kryon/build\":$LD_LIBRARY_PATH "
                 "KRYON_LIB_PATH=\"%s\" "
                 "%s \"%s\"",
                 desktop_lib, desktop_lib, target_file);
    } else {
        // AUTO-COMPILE OTHER FORMATS TO KIR FIRST

        // Extract basename from target_file
        const char* basename = strrchr(target_file, '/');
        basename = basename ? basename + 1 : target_file;

        // Create KIR filename
        char kir_file[1024];
        snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s", basename);

        // Change extension to .kir
        char* dot = strrchr(kir_file, '.');
        if (dot) {
            strcpy(dot, ".kir");
        }

        // Compile to KIR (works for ALL formats now: .kry, .md, .html, .tsx, .lua, .c)
        printf("Compiling %s...\n", target_file);
        char compile_cmd[2048];
        snprintf(compile_cmd, sizeof(compile_cmd),
                 "kryon compile \"%s\" 2>&1", target_file);

        int compile_result = system(compile_cmd);
        if (compile_result != 0) {
            fprintf(stderr, "Error: Failed to compile %s\n", target_file);
            if (free_target) free((char*)target_file);
            return 1;
        }

        // Now execute the compiled KIR
        printf("✓ Compiled to KIR: %s\n", kir_file);

        // Route to Android if target is Android
        if (strcmp(target_platform, "android") == 0) {
            int result = run_android(kir_file, target_file);
            if (free_target) free((char*)target_file);
            return result;
        }

        // Load the KIR file
        IRComponent* root = ir_read_json_file(kir_file);
        if (!root) {
            fprintf(stderr, "Error: Failed to load KIR file: %s\n", kir_file);
            if (free_target) free((char*)target_file);
            return 1;
        }

        // Create and initialize executor for event handling
        IRExecutorContext* executor = ir_executor_create();
        if (executor) {
            printf("[executor] Loading KIR file for event handling\n");
            ir_executor_load_kir_file(executor, kir_file);
            ir_executor_set_root(executor, root);
            ir_executor_set_global(executor);
            printf("[executor] Global executor initialized\n");
        } else {
            fprintf(stderr, "[executor] WARNING: Failed to create executor!\n");
        }

        // Create desktop renderer config with defaults
        int window_width = 800;
        int window_height = 600;
        const char* window_title = "Kryon App";

        // Override with window properties from IR metadata if available
        extern IRContext* g_ir_context;
        if (g_ir_context) {
            if (g_ir_context->metadata) {
                if (g_ir_context->metadata->window_width > 0) {
                    window_width = g_ir_context->metadata->window_width;
                }
                if (g_ir_context->metadata->window_height > 0) {
                    window_height = g_ir_context->metadata->window_height;
                }
                if (g_ir_context->metadata->window_title) {
                    window_title = g_ir_context->metadata->window_title;
                }
            }
        }

        DesktopRendererConfig config = desktop_renderer_config_sdl3(window_width, window_height, window_title);

        // Read kryon.toml to determine renderer
        KryonConfig* kryon_config = config_find_and_load();
        if (kryon_config && kryon_config->desktop_renderer) {
            if (strcmp(kryon_config->desktop_renderer, "raylib") == 0) {
                printf("Renderer: raylib (from kryon.toml)\n");
                config.backend_type = DESKTOP_BACKEND_RAYLIB;
            } else {
                printf("Renderer: sdl3 (from kryon.toml)\n");
                config.backend_type = DESKTOP_BACKEND_SDL3;
            }
            config_free(kryon_config);
        } else {
            printf("Renderer: sdl3 (default)\n");
            config.backend_type = DESKTOP_BACKEND_SDL3;
        }

        // Run the application
        printf("Running application...\n");
        fprintf(stderr, "[DEBUG] About to call desktop_render_ir_component\n");
        fflush(stderr);
        bool success = desktop_render_ir_component(root, &config);
        fprintf(stderr, "[DEBUG] desktop_render_ir_component returned: %d\n", success);

        // Cleanup
        if (executor) {
            ir_executor_destroy(executor);
        }
        ir_destroy_component(root);

        if (free_target) free((char*)target_file);
        return success ? 0 : 1;
    }

    // Execute KIR files directly
    IRComponent* root = ir_read_json_file(target_file);
    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file: %s\n", target_file);
        if (free_target) free((char*)target_file);
        return 1;
    }

    // Create and initialize executor for event handling
    IRExecutorContext* executor = ir_executor_create();
    if (executor) {
        printf("[executor] Loading KIR file for event handling\n");
        ir_executor_load_kir_file(executor, target_file);
        ir_executor_set_root(executor, root);
        ir_executor_set_global(executor);
        printf("[executor] Global executor initialized\n");
    } else {
        fprintf(stderr, "[executor] WARNING: Failed to create executor!\n");
    }

    // Create desktop renderer config with defaults
    int window_width = 800;
    int window_height = 600;
    const char* window_title = "Kryon App";

    // Override with window properties from IR metadata if available
    extern IRContext* g_ir_context;
    if (g_ir_context) {
        if (g_ir_context->metadata) {
            if (g_ir_context->metadata->window_width > 0) {
                window_width = g_ir_context->metadata->window_width;
            }
            if (g_ir_context->metadata->window_height > 0) {
                window_height = g_ir_context->metadata->window_height;
            }
            if (g_ir_context->metadata->window_title) {
                window_title = g_ir_context->metadata->window_title;
            }
        }
    }

    DesktopRendererConfig config = desktop_renderer_config_sdl3(window_width, window_height, window_title);

    // Read kryon.toml to determine renderer
    KryonConfig* kryon_config = config_find_and_load();
    if (kryon_config && kryon_config->desktop_renderer) {
        if (strcmp(kryon_config->desktop_renderer, "raylib") == 0) {
            printf("Renderer: raylib (from kryon.toml)\n");
            config.backend_type = DESKTOP_BACKEND_RAYLIB;
        } else {
            printf("Renderer: sdl3 (from kryon.toml)\n");
            config.backend_type = DESKTOP_BACKEND_SDL3;
        }
        config_free(kryon_config);
    } else {
        printf("Renderer: sdl3 (default)\n");
        config.backend_type = DESKTOP_BACKEND_SDL3;
    }

    // Run the application
    printf("Running application...\n");
    bool success = desktop_render_ir_component(root, &config);

    // Cleanup
    if (executor) {
        ir_executor_destroy(executor);
    }
    ir_destroy_component(root);

    if (free_target) free((char*)target_file);
    return success ? 0 : 1;
}
