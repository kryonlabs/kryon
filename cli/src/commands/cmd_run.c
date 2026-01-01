/**
 * Run Command
 * Executes source files or KIR files using C backend
 */

#define _POSIX_C_SOURCE 200809L

#include "kryon_cli.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_serialization.h"
#include "../../ir/ir_executor.h"
#include "../../backends/desktop/ir_desktop_renderer.h"
#include "../../renderers/terminal/terminal_backend.h"
#include "../../codegens/kotlin/kotlin_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

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

/**
 * Run KIR file in terminal renderer
 *
 * CRITICAL: This function is source-language agnostic.
 * Works with KIR from ANY frontend: TSX, Kry, HTML, Markdown, Lua, C, etc.
 */
static int run_terminal(const char* kir_file) {
    (void)kir_file;  // Unused parameter
    fprintf(stderr, "Error: Terminal renderer is not available in this build\n");
    fprintf(stderr, "Please use desktop or other rendering backends instead.\n");
    return 1;

    /* Terminal renderer disabled - not built
    printf("\n");
    printf("╭────────────────────────────────────────────────────────╮\n");
    printf("│  Terminal Renderer - Running KIR in Terminal          │\n");
    printf("╰────────────────────────────────────────────────────────╯\n");
    printf("\n");
    printf("File: %s\n", kir_file);
    printf("Source-agnostic: Works with KIR from TSX, Kry, HTML, etc.\n");
    printf("\n");

    // 1. Load KIR file (source-agnostic - works for ANY frontend)
    printf("Loading KIR file...\n");
    IRComponent* root = ir_read_json_file(kir_file);
    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file: %s\n", kir_file);
        return 1;
    }
    printf("✓ KIR loaded successfully\n");

    // 2. Create terminal renderer
    printf("Initializing terminal renderer...\n");
    kryon_renderer_t* renderer = kryon_terminal_renderer_create();
    if (!renderer) {
        fprintf(stderr, "Error: Failed to create terminal renderer\n");
        // // ir_component_destroy(root);  // Function doesn't exist
        return 1;
    }

    // Initialize renderer
    if (!renderer->ops->init(renderer, NULL)) {
        fprintf(stderr, "Error: Failed to initialize terminal renderer\n");
        kryon_terminal_renderer_destroy(renderer);
        // ir_component_destroy(root);
        return 1;
    }
    printf("✓ Terminal renderer initialized\n");

    // 3. Initialize IR executor (handles logic from any source)
    printf("Setting up IR executor...\n");
    IRExecutorContext* executor = ir_executor_get_global();
    if (!executor) {
        executor = ir_executor_create();
        ir_executor_set_global(executor);
    }
    ir_executor_set_root(executor, root);
    printf("✓ IR executor ready\n");

    printf("\n");
    printf("Rendering to terminal (Press 'q' or ESC to exit)...\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    // 4. Set terminal to raw mode for interactive input
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
    new_term.c_cc[VMIN] = 0;   // Non-blocking read
    new_term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    // 5. Interactive event loop
    bool running = true;
    bool success = true;

    while (running) {
        // Render frame
        if (!kryon_terminal_render_ir_tree(renderer, root)) {
            success = false;
            break;
        }

        // Poll for keyboard input (non-blocking)
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n > 0) {
            // Process input
            if (c == 27 || c == 'q' || c == 'Q') {  // ESC or 'q'/'Q' to quit
                running = false;
            } else if (c == '\n' || c == '\r') {  // Enter
                // TODO: Trigger click on focused component
                // For now, just mark as needing redraw
            }
            // TODO: Handle arrow keys, tab, etc.
        }

        // Sleep to avoid busy-waiting (30 FPS)
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 33333333 };  // 33.333ms = ~30 FPS
        nanosleep(&ts, NULL);
    }

    // 6. Restore terminal mode
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

    // Clear screen and show cursor
    printf("\033[2J\033[H\033[?25h");

    if (success) {
        printf("\n");
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("✓ Exited successfully\n");
    }

    // 7. Cleanup
    renderer->ops->shutdown(renderer);
    kryon_terminal_renderer_destroy(renderer);
    // ir_component_destroy(root);

    return success ? 0 : 1;
    */
}

int cmd_run(int argc, char** argv) {
    const char* target_file = NULL;
    bool free_target = false;
    const char* target_platform = NULL;
    bool explicit_target = false;

    // Parse --target flag and detect invalid flags
    int new_argc = 0;
    char* new_argv[128];
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--target=", 9) == 0) {
            target_platform = argv[i] + 9;
            explicit_target = true;
        } else if (strncmp(argv[i], "--renderer=", 11) == 0) {
            fprintf(stderr, "Error: Invalid flag '--renderer=%s'\n", argv[i] + 11);
            fprintf(stderr, "Did you mean '--target=%s'?\n", argv[i] + 11);
            fprintf(stderr, "\nSupported targets:\n");
            fprintf(stderr, "  --target=desktop   (default)\n");
            fprintf(stderr, "  --target=android\n");
            fprintf(stderr, "  --target=terminal\n");
            fprintf(stderr, "  --target=web\n");
            return 1;
        } else if (strncmp(argv[i], "--", 2) == 0 && strchr(argv[i], '=')) {
            // Unknown flag with '='
            fprintf(stderr, "Error: Unknown flag '%s'\n", argv[i]);
            fprintf(stderr, "\nValid flags:\n");
            fprintf(stderr, "  --target=<platform>    Platform to run on (desktop, android, terminal, web)\n");
            return 1;
        } else if (new_argc < 128) {
            new_argv[new_argc++] = argv[i];
        }
    }

    // Update argc/argv to exclude --target flag
    argc = new_argc;
    for (int i = 0; i < new_argc; i++) {
        argv[i] = new_argv[i];
    }

    // If no explicit target specified, try to get it from config
    if (!target_platform) {
        KryonConfig* temp_config = config_find_and_load();
        if (temp_config && temp_config->build_targets_count > 0 && temp_config->build_targets[0]) {
            // Use first target from config
            target_platform = temp_config->build_targets[0];
            fprintf(stderr, "[kryon] Using target from kryon.toml: %s\n", target_platform);
        } else {
            // Default to desktop if no config or no targets
            target_platform = "desktop";
        }
        // Note: We don't free temp_config here as target_platform points to its memory
        // It will be freed later when we reload the config
    }

    // Validate target platform
    if (strcmp(target_platform, "desktop") != 0 &&
        strcmp(target_platform, "android") != 0 &&
        strcmp(target_platform, "terminal") != 0 &&
        strcmp(target_platform, "web") != 0) {
        fprintf(stderr, "Error: Invalid target platform: %s\n", target_platform);
        fprintf(stderr, "Supported targets: desktop, android, terminal, web\n");
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

        // Validate config (will show detailed errors)
        if (!config_validate(config)) {
            config_free(config);
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

        // If a file is specified, still try to load and validate config if it exists
        // This ensures proper error messages for invalid kryon.toml files
        KryonConfig* config = config_find_and_load();
        if (config) {
            // If config exists, validate it
            if (!config_validate(config)) {
                config_free(config);
                if (free_target) free((char*)target_file);
                return 1;
            }
            config_free(config);
        }
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

        // Find additional .c files in the same directory
        char additional_sources[2048] = "";
        {
            // Extract directory from target_file
            char dir_path[1024];
            const char* last_slash = strrchr(target_file, '/');
            if (last_slash) {
                size_t dir_len = last_slash - target_file;
                strncpy(dir_path, target_file, dir_len);
                dir_path[dir_len] = '\0';
            } else {
                strcpy(dir_path, ".");
            }

            // Find all .c files in the directory recursively (exclude target file)
            char find_cmd[2048];
            snprintf(find_cmd, sizeof(find_cmd),
                     "find \"%s\" -name '*.c' ! -name '%s' 2>/dev/null",
                     dir_path,
                     last_slash ? last_slash + 1 : target_file);

            FILE* fp = popen(find_cmd, "r");
            if (fp) {
                char line[512];
                while (fgets(line, sizeof(line), fp)) {
                    // Remove newline
                    line[strcspn(line, "\n")] = 0;
                    if (strlen(line) > 0) {
                        strncat(additional_sources, "\"", sizeof(additional_sources) - strlen(additional_sources) - 1);
                        strncat(additional_sources, line, sizeof(additional_sources) - strlen(additional_sources) - 1);
                        strncat(additional_sources, "\" ", sizeof(additional_sources) - strlen(additional_sources) - 1);
                    }
                }
                pclose(fp);
            }
        }

        char compile_cmd[4096];  // Increased size for additional sources

        // Extract directory for -I flag
        char dir_include[1024] = "";
        const char* last_slash = strrchr(target_file, '/');
        if (last_slash) {
            size_t dir_len = last_slash - target_file;
            snprintf(dir_include, sizeof(dir_include), "-I%.*s ", (int)dir_len, target_file);
        }

        snprintf(compile_cmd, sizeof(compile_cmd),
                 "gcc -std=c99 -O2 \"%s\" %s"
                 "/mnt/storage/Projects/kryon/bindings/c/kryon.c "
                 "/mnt/storage/Projects/kryon/bindings/c/kryon_dsl.c "
                 "-o \"%s\" "
                 "%s"  // Add directory include path
                 "-Iinclude "  // Add include directory for project headers
                 "-I/mnt/storage/Projects/kryon/bindings/c "
                 "-I/mnt/storage/Projects/kryon/ir "
                 "-I/mnt/storage/Projects/kryon/backends/desktop "
                 "-I/mnt/storage/Projects/kryon/ir/third_party/cJSON "
                 "-L/mnt/storage/Projects/kryon/build "
                 "-lkryon_desktop -lkryon_ir -lm -lraylib",
                 target_file, additional_sources, exe_file, dir_include);

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
        // Route to platform-specific execution
        if (strcmp(target_platform, "android") == 0) {
            int result = run_android(target_file, target_file);
            if (free_target) free((char*)target_file);
            return result;
        } else if (strcmp(target_platform, "terminal") == 0) {
            // Run KIR file in terminal renderer (source-agnostic)
            int result = run_terminal(target_file);
            if (free_target) free((char*)target_file);
            return result;
        } else if (strcmp(target_platform, "web") == 0) {
            // Generate HTML and serve
            printf("Web target selected - generating HTML and starting dev server\n");

            // Load config to get output directory and dev server settings
            KryonConfig* web_config = config_find_and_load();
            const char* output_dir = (web_config && web_config->build_output_dir) ?
                                      web_config->build_output_dir : "build";
            int port = (web_config && web_config->dev_port > 0) ?
                       web_config->dev_port : 3000;
            bool auto_open = (web_config && web_config->dev_auto_open) ?
                             web_config->dev_auto_open : true;

            // Create output directory
            if (!file_is_directory(output_dir)) {
                dir_create_recursive(output_dir);
            }

            // Generate HTML from KIR using web codegen
            printf("Generating HTML files...\n");

            // Get absolute paths
            char* cwd = dir_get_current();
            char abs_kir_path[2048];
            char abs_output_dir[2048];

            if (target_file[0] == '/') {
                snprintf(abs_kir_path, sizeof(abs_kir_path), "%s", target_file);
            } else {
                snprintf(abs_kir_path, sizeof(abs_kir_path), "%s/%s", cwd, target_file);
            }

            if (output_dir[0] == '/') {
                snprintf(abs_output_dir, sizeof(abs_output_dir), "%s", output_dir);
            } else {
                snprintf(abs_output_dir, sizeof(abs_output_dir), "%s/%s", cwd, output_dir);
            }

            // Source directory is current working directory (where assets are located)
            char* source_dir = cwd;

            char codegen_cmd[8192];
            snprintf(codegen_cmd, sizeof(codegen_cmd),
                     "cd /mnt/storage/Projects/kryon/codegens/web && "
                     "gcc -std=c99 -O2 "
                     "-I../../ir -I../../ir/third_party/cJSON "
                     "kir_to_html.c ir_web_renderer.c html_generator.c css_generator.c wasm_bridge.c style_analyzer.c "
                     "../../ir/third_party/cJSON/cJSON.c "
                     "-L../../build -lkryon_ir -lm "
                     "-o /tmp/kryon_web_gen_%d 2>&1 && "
                     "LD_LIBRARY_PATH=/mnt/storage/Projects/kryon/build:$LD_LIBRARY_PATH "
                     "/tmp/kryon_web_gen_%d \"%s\" \"%s\" \"%s\" 2>&1",
                     getpid(), getpid(), source_dir, abs_kir_path, abs_output_dir);

            free(cwd);

            int codegen_result = system(codegen_cmd);
            if (codegen_result != 0) {
                fprintf(stderr, "Error: HTML generation failed\n");
                if (web_config) config_free(web_config);
                if (free_target) free((char*)target_file);
                return 1;
            }

            printf("✓ HTML files generated in %s/\n\n", output_dir);

            // Start dev server
            extern int start_dev_server(const char* document_root, int port, bool auto_open);
            int result = start_dev_server(output_dir, port, auto_open);

            if (web_config) config_free(web_config);
            if (free_target) free((char*)target_file);
            return result;
        }

        // Run KIR file directly on desktop (default)
        snprintf(cmd, sizeof(cmd),
                 "LD_LIBRARY_PATH=/home/wao/.local/lib:\"/mnt/storage/Projects/kryon/build\":$LD_LIBRARY_PATH "
                 "KRYON_LIB_PATH=\"%s\" "
                 "%s \"%s\"",
                 desktop_lib, desktop_lib, target_file);
    } else {
        // AUTO-COMPILE SOURCE FILES TO KIR FIRST (including .lua)

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

        // Compile to KIR (works for ALL formats now: .kry, .md, .html, .tsx, .c)
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

        // Route to web if target is web
        if (strcmp(target_platform, "web") == 0) {
            printf("Web target selected - generating HTML and starting dev server\n");

            // Load config to get output directory and dev server settings
            KryonConfig* web_config = config_find_and_load();
            const char* output_dir = (web_config && web_config->build_output_dir) ?
                                      web_config->build_output_dir : "build";
            int port = (web_config && web_config->dev_port > 0) ?
                       web_config->dev_port : 3000;
            bool auto_open = (web_config && web_config->dev_auto_open) ?
                             web_config->dev_auto_open : true;

            // Create output directory
            if (!file_is_directory(output_dir)) {
                dir_create_recursive(output_dir);
            }

            // Generate HTML from KIR using web codegen
            printf("Generating HTML files...\n");

            // Get absolute paths
            char* cwd2 = dir_get_current();
            char abs_kir_path2[2048];
            char abs_output_dir2[2048];

            if (kir_file[0] == '/') {
                snprintf(abs_kir_path2, sizeof(abs_kir_path2), "%s", kir_file);
            } else {
                snprintf(abs_kir_path2, sizeof(abs_kir_path2), "%s/%s", cwd2, kir_file);
            }

            if (output_dir[0] == '/') {
                snprintf(abs_output_dir2, sizeof(abs_output_dir2), "%s", output_dir);
            } else {
                snprintf(abs_output_dir2, sizeof(abs_output_dir2), "%s/%s", cwd2, output_dir);
            }

            char codegen_cmd[8192];
            snprintf(codegen_cmd, sizeof(codegen_cmd),
                     "cd /mnt/storage/Projects/kryon/codegens/web && "
                     "gcc -std=c99 -O2 "
                     "-I../../ir -I../../ir/third_party/cJSON "
                     "kir_to_html.c ir_web_renderer.c html_generator.c css_generator.c wasm_bridge.c style_analyzer.c "
                     "../../ir/third_party/cJSON/cJSON.c "
                     "-L../../build -lkryon_ir -lm "
                     "-o /tmp/kryon_web_gen_%d 2>&1 && "
                     "LD_LIBRARY_PATH=/mnt/storage/Projects/kryon/build:$LD_LIBRARY_PATH "
                     "/tmp/kryon_web_gen_%d \"%s\" \"%s\" \"%s\" 2>&1",
                     getpid(), getpid(), cwd2, abs_kir_path2, abs_output_dir2);

            free(cwd2);

            int codegen_result = system(codegen_cmd);
            if (codegen_result != 0) {
                fprintf(stderr, "Error: HTML generation failed\n");
                if (web_config) config_free(web_config);
                if (free_target) free((char*)target_file);
                return 1;
            }

            printf("✓ HTML files generated in %s/\n\n", output_dir);

            // Start dev server
            extern int start_dev_server(const char* document_root, int port, bool auto_open);
            int result = start_dev_server(output_dir, port, auto_open);

            if (web_config) config_free(web_config);
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

        // Check if KIR has Lua source metadata - if so, delegate to Lua runtime
        extern IRContext* g_ir_context;
        bool needs_lua_runtime = false;
        fprintf(stderr, "[DEBUG] g_ir_context = %p\n", (void*)g_ir_context);
        if (g_ir_context) {
            fprintf(stderr, "[DEBUG] g_ir_context->source_metadata = %p\n", (void*)g_ir_context->source_metadata);
            if (g_ir_context->source_metadata) {
                IRSourceMetadata* meta = g_ir_context->source_metadata;
                fprintf(stderr, "[DEBUG] source_language = %s\n", meta->source_language ? meta->source_language : "NULL");
                if (meta->source_language && strcmp(meta->source_language, "lua") == 0) {
                    needs_lua_runtime = true;
                    fprintf(stderr, "[DEBUG] Setting needs_lua_runtime = true\n");
                }
            }
        }
        fprintf(stderr, "[DEBUG] needs_lua_runtime = %s\n", needs_lua_runtime ? "true" : "false");

        if (needs_lua_runtime) {
            fprintf(stderr, "[kryon] KIR requires Lua runtime, delegating to Runtime.loadKIR\n");

            // Get LUA_PATH for Kryon bindings
            char* kryon_root = getenv("KRYON_ROOT");
            if (!kryon_root) {
                kryon_root = "/mnt/storage/Projects/kryon";  // fallback
            }

            // Load kryon.toml to get plugin paths
            KryonConfig* kryon_config = config_find_and_load();

            // Build plugin paths from config and environment
            char plugin_paths_buffer[2048] = "";
            char* plugin_paths = getenv("KRYON_PLUGIN_PATHS");

            // If config has plugins, build paths
            if (kryon_config && kryon_config->plugins && kryon_config->plugins_count > 0) {
                char* cwd = dir_get_current();
                for (int i = 0; i < kryon_config->plugins_count; i++) {
                    PluginDep* plugin = &kryon_config->plugins[i];
                    if (!plugin->enabled || !plugin->path) continue;

                    // Resolve relative paths
                    char* resolved_path = NULL;
                    if (plugin->path[0] == '/') {
                        // Absolute path
                        resolved_path = str_copy(plugin->path);
                    } else {
                        // Relative to project directory
                        resolved_path = path_join(cwd, plugin->path);
                    }

                    // Add to plugin_paths_buffer
                    if (strlen(plugin_paths_buffer) > 0) {
                        strncat(plugin_paths_buffer, ":", sizeof(plugin_paths_buffer) - strlen(plugin_paths_buffer) - 1);
                    }
                    strncat(plugin_paths_buffer, resolved_path, sizeof(plugin_paths_buffer) - strlen(plugin_paths_buffer) - 1);
                    free(resolved_path);
                }
                free(cwd);

                // If env var is also set, append it
                if (plugin_paths && strlen(plugin_paths) > 0) {
                    if (strlen(plugin_paths_buffer) > 0) {
                        strncat(plugin_paths_buffer, ":", sizeof(plugin_paths_buffer) - strlen(plugin_paths_buffer) - 1);
                    }
                    strncat(plugin_paths_buffer, plugin_paths, sizeof(plugin_paths_buffer) - strlen(plugin_paths_buffer) - 1);
                }

                // Use the combined paths
                if (strlen(plugin_paths_buffer) > 0) {
                    plugin_paths = plugin_paths_buffer;
                }
            }

            // Build Lua command to load KIR with Runtime.loadKIR
            char lua_cmd[4096];
            if (plugin_paths && strlen(plugin_paths) > 0) {
                // Build LUA_PATH, LUA_CPATH, and LD_LIBRARY_PATH from plugin paths
                char lua_path[2048] = "";
                char lua_cpath[2048] = "";
                char ld_library_path[2048] = "";

                // Start with kryon root paths
                snprintf(lua_path, sizeof(lua_path),
                         "%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua",
                         kryon_root, kryon_root);

                // Parse plugin_paths (colon-separated)
                char* paths_copy = strdup(plugin_paths);
                char* path_token = strtok(paths_copy, ":");
                while (path_token != NULL) {
                    // Append to LUA_PATH
                    size_t len = strlen(lua_path);
                    snprintf(lua_path + len, sizeof(lua_path) - len,
                             ";%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua",
                             path_token, path_token);

                    // Append to LUA_CPATH
                    if (strlen(lua_cpath) > 0) {
                        strncat(lua_cpath, ";", sizeof(lua_cpath) - strlen(lua_cpath) - 1);
                    }
                    len = strlen(lua_cpath);
                    snprintf(lua_cpath + len, sizeof(lua_cpath) - len,
                             "%s/build/?.so", path_token);

                    // Append to LD_LIBRARY_PATH
                    if (strlen(ld_library_path) > 0) {
                        strncat(ld_library_path, ":", sizeof(ld_library_path) - strlen(ld_library_path) - 1);
                    }
                    strncat(ld_library_path, path_token, sizeof(ld_library_path) - strlen(ld_library_path) - 1);
                    strncat(ld_library_path, "/build", sizeof(ld_library_path) - strlen(ld_library_path) - 1);

                    path_token = strtok(NULL, ":");
                }
                free(paths_copy);

                // Terminate LUA_PATH and LUA_CPATH
                strncat(lua_path, ";;", sizeof(lua_path) - strlen(lua_path) - 1);
                strncat(lua_cpath, ";;", sizeof(lua_cpath) - strlen(lua_cpath) - 1);

                snprintf(lua_cmd, sizeof(lua_cmd),
                         "KRYON_PLUGIN_PATHS=\"%s\" "
                         "LUA_PATH=\"%s\" "
                         "LUA_CPATH=\"%s\" "
                         "LD_LIBRARY_PATH=\"%s:%s/build:$LD_LIBRARY_PATH\" "
                         "luajit -e '"
                         "local Runtime = require(\"kryon.runtime\"); "
                         "local app = Runtime.loadKIR(\"%s\"); "
                         "Runtime.runDesktop(app)'",
                         plugin_paths, lua_path, lua_cpath, ld_library_path, kryon_root, kir_file);
            } else {
                snprintf(lua_cmd, sizeof(lua_cmd),
                         "LUA_PATH=\"%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua;;\" "
                         "LD_LIBRARY_PATH=\"%s/build:$LD_LIBRARY_PATH\" "
                         "luajit -e '"
                         "local Runtime = require(\"kryon.runtime\"); "
                         "local app = Runtime.loadKIR(\"%s\"); "
                         "Runtime.runDesktop(app)'",
                         kryon_root, kryon_root, kryon_root, kir_file);
            }

            fprintf(stderr, "[DEBUG] Executing: %s\n", lua_cmd);
            int result = system(lua_cmd);

            // Cleanup
            if (kryon_config) config_free(kryon_config);
            ir_destroy_component(root);
            if (free_target) free((char*)target_file);
            return (result == 0) ? 0 : 1;
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

    // Check if KIR has Lua source metadata - if so, delegate to Lua runtime
    extern IRContext* g_ir_context;
    bool needs_lua_runtime_kir = false;
    fprintf(stderr, "[DEBUG KIR] g_ir_context = %p\n", (void*)g_ir_context);
    if (g_ir_context) {
        fprintf(stderr, "[DEBUG KIR] g_ir_context->source_metadata = %p\n", (void*)g_ir_context->source_metadata);
        if (g_ir_context->source_metadata) {
            IRSourceMetadata* meta = g_ir_context->source_metadata;
            fprintf(stderr, "[DEBUG KIR] source_language = %s\n", meta->source_language ? meta->source_language : "NULL");
            if (meta->source_language && strcmp(meta->source_language, "lua") == 0) {
                needs_lua_runtime_kir = true;
                fprintf(stderr, "[DEBUG KIR] Setting needs_lua_runtime_kir = true\n");
            }
        }
    }
    fprintf(stderr, "[DEBUG KIR] needs_lua_runtime_kir = %s\n", needs_lua_runtime_kir ? "true" : "false");

    if (needs_lua_runtime_kir) {
        fprintf(stderr, "[kryon] KIR requires Lua runtime, delegating to Runtime.loadKIR\n");

        // Get LUA_PATH for Kryon bindings
        char* kryon_root = getenv("KRYON_ROOT");
        if (!kryon_root) {
            kryon_root = "/mnt/storage/Projects/kryon";  // fallback
        }

        // Build Lua command to load KIR with Runtime.loadKIR
        char lua_cmd[4096];
        snprintf(lua_cmd, sizeof(lua_cmd),
                 "LUA_PATH=\"%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua;;\" "
                 "LD_LIBRARY_PATH=\"%s/build:$LD_LIBRARY_PATH\" "
                 "luajit -e '"
                 "local Runtime = require(\"kryon.runtime\"); "
                 "local app = Runtime.loadKIR(\"%s\"); "
                 "Runtime.runDesktop(app)'",
                 kryon_root, kryon_root, kryon_root, target_file);

        int result = system(lua_cmd);
        ir_destroy_component(root);
        if (free_target) free((char*)target_file);
        return (result == 0) ? 0 : 1;
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
