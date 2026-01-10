/**
 * Run Command
 * Executes source files or KIR files using C backend
 */

#define _POSIX_C_SOURCE 200809L

#include "kryon_cli.h"
#include "build.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_serialization.h"
#include "../../ir/ir_executor.h"
#include "../../backends/desktop/ir_desktop_renderer.h"
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
#include <limits.h>

/* ============================================================================
 * Android Support
 * ============================================================================ */

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

    // Create local.properties with SDK location
    char local_props[2048];
    snprintf(local_props, sizeof(local_props), "%s/local.properties", temp_dir);
    FILE* props_f = fopen(local_props, "w");
    if (props_f) {
        const char* android_sdk = getenv("ANDROID_HOME");
        if (android_sdk) {
            fprintf(props_f, "sdk.dir=%s\n", android_sdk);
        }
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

    // Use dynamic path for Kotlin bindings
    char* kryon_root = paths_get_kryon_root();
    char* kotlin_path = kryon_root ? path_join(kryon_root, "bindings/kotlin") : str_copy("bindings/kotlin");
    fprintf(f, "project(\":kryon\").projectDir = file(\"%s\")\n", kotlin_path);
    free(kotlin_path);
    if (kryon_root) free(kryon_root);

    fclose(f);

    // Create gradle.properties with AndroidX enabled
    char gradle_props[2048];
    snprintf(gradle_props, sizeof(gradle_props), "%s/gradle.properties", temp_dir);
    f = fopen(gradle_props, "w");
    if (!f) return -1;
    fprintf(f, "android.useAndroidX=true\n");
    fprintf(f, "android.enableJetifier=true\n");
    fclose(f);

    // Create root build.gradle.kts
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
    (void)source_file;  // Unused parameter
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
        return 1;
    }

    // Extract device ID
    char device_id[128] = {0};
    char* tab = strchr(device_line, '\t');
    if (tab) {
        size_t len = tab - device_line;
        if (len < sizeof(device_id)) {
            strncpy(device_id, device_line, len);
            device_id[len] = '\0';
        }
    } else {
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

    // 4. Build APK
    printf("Building APK (this may take a minute)...\n");

    int gradle_check = system("command -v gradle >/dev/null 2>&1");
    if (gradle_check != 0) {
        fprintf(stderr, "Error: Gradle not found in PATH\n");
        return 1;
    }

    char build_cmd[2048];
    snprintf(build_cmd, sizeof(build_cmd),
             "cd \"%s\" && gradle assembleDebug 2>&1",
             temp_dir);

    int build_result = system(build_cmd);
    if (build_result != 0) {
        fprintf(stderr, "Error: Gradle build failed\n");
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
        return 1;
    }

    printf("✓ APK installed\n");

    // 6. Launch app
    printf("Launching app...\n");
    char launch_cmd[2048];
    snprintf(launch_cmd, sizeof(launch_cmd),
             "adb -s \"%s\" shell am start -n com.kryon.temp/.MainActivity 2>&1",
             device_id);

    system(launch_cmd);

    // 7. Cleanup
    printf("Cleaning up temporary files...\n");
    char cleanup_cmd[1024];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", temp_dir);
    system(cleanup_cmd);

    printf("✓ Done!\n");
    _exit(0);  // Use _exit to skip libc cleanup
}

/* ============================================================================
 * Terminal Renderer
 * ============================================================================ */

static int run_terminal(const char* kir_file) {
    (void)kir_file;  // Currently unused
    fprintf(stderr, "Error: Terminal renderer is not available in this build\n");
    fprintf(stderr, "Please use desktop or other rendering backends instead.\n");
    return 1;
}

/* ============================================================================
 * Web Target
 * ============================================================================ */

static int run_web_target(const char* kir_file) {
    KryonConfig* config = config_find_and_load();
    const char* output_dir = (config && config->build_output_dir) ?
                              config->build_output_dir : "build";
    int port = (config && config->dev_port > 0) ? config->dev_port : 3000;
    bool auto_open = (config && config->dev_auto_open) ? config->dev_auto_open : true;

    // Create output directory
    if (!file_is_directory(output_dir)) {
        dir_create_recursive(output_dir);
    }

    printf("Generating HTML files...\n");
    int result = generate_html_from_kir(kir_file, output_dir,
                                        config ? config->project_name : NULL,
                                        ".");

    if (result != 0) {
        if (config) config_free(config);
        return 1;
    }

    printf("✓ HTML files generated in %s/\n\n", output_dir);

    // Start dev server
    extern int start_dev_server(const char*, int, bool);
    result = start_dev_server(output_dir, port, auto_open);

    if (config) config_free(config);
    return result;
}

/* ============================================================================
 * KIR File Execution
 * ============================================================================ */

static int run_kir_file(const char* kir_file, const char* target_platform, const char* renderer,
                         bool hot_reload, const char* watch_path) {
    (void)hot_reload;
    (void)watch_path;

    if (strcmp(target_platform, "android") == 0) {
        return run_android(kir_file, kir_file);
    }
    if (strcmp(target_platform, "terminal") == 0) {
        return run_terminal(kir_file);
    }
    if (strcmp(target_platform, "web") == 0) {
        return run_web_target(kir_file);
    }
    // Default: desktop
    if (hot_reload) {
        return run_kir_on_desktop_with_hot_reload(kir_file, NULL, renderer, watch_path);
    }
    return run_kir_on_desktop(kir_file, NULL, renderer);
}

/* ============================================================================
 * C File Compilation and Execution
 * ============================================================================ */

static int run_c_file(const char* target_file) {
    char exe_file[1024];
    snprintf(exe_file, sizeof(exe_file), "/tmp/kryon_app_%d", getpid());

    printf("Compiling %s to native executable...\n", target_file);

    // Find additional .c files in the same directory
    char additional_sources[2048] = "";
    {
        char dir_path[1024];
        const char* last_slash = strrchr(target_file, '/');
        if (last_slash) {
            size_t dir_len = last_slash - target_file;
            strncpy(dir_path, target_file, dir_len);
            dir_path[dir_len] = '\0';
        } else {
            strcpy(dir_path, ".");
        }

        char find_cmd[2048];
        snprintf(find_cmd, sizeof(find_cmd),
                 "find \"%s\" -name '*.c' ! -name '%s' 2>/dev/null",
                 dir_path,
                 last_slash ? last_slash + 1 : target_file);

        FILE* fp = popen(find_cmd, "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
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

    char compile_cmd[8192];
    char dir_include[1024] = "";
    const char* last_slash = strrchr(target_file, '/');
    if (last_slash) {
        size_t dir_len = last_slash - target_file;
        snprintf(dir_include, sizeof(dir_include), "-I%.*s ", (int)dir_len, target_file);
    }

    char* kryon_root = paths_get_kryon_root();
    char* bindings_path = kryon_root ? path_join(kryon_root, "bindings/c") : paths_get_bindings_path();
    char* ir_path = kryon_root ? path_join(kryon_root, "ir") : str_copy("ir");
    char* desktop_path = kryon_root ? path_join(kryon_root, "backends/desktop") : str_copy("backends/desktop");
    char* cjson_path = kryon_root ? path_join(kryon_root, "ir/third_party/cJSON") : str_copy("ir/third_party/cJSON");
    char* build_path = kryon_root ? path_join(kryon_root, "build") : paths_get_build_path();

    char kryon_c[PATH_MAX];
    char kryon_dsl_c[PATH_MAX];
    snprintf(kryon_c, sizeof(kryon_c), "%s/kryon.c", bindings_path);
    snprintf(kryon_dsl_c, sizeof(kryon_dsl_c), "%s/kryon_dsl.c", bindings_path);

    snprintf(compile_cmd, sizeof(compile_cmd),
             "gcc -std=c99 -O2 \"%s\" %s"
             "\"%s\" \"%s\" "
             "-o \"%s\" "
             "%s"
             "-Iinclude "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-L\"%s\" "
             "-lkryon_desktop -lkryon_ir -lm -lraylib",
             target_file, additional_sources,
             kryon_c, kryon_dsl_c, exe_file, dir_include,
             bindings_path, ir_path, desktop_path, cjson_path, build_path);

    free(bindings_path);
    free(ir_path);
    free(desktop_path);
    free(cjson_path);
    free(build_path);
    if (kryon_root) free(kryon_root);

    int result = system(compile_cmd);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to compile C file\n");
        return 1;
    }

    printf("✓ Compiled successfully\n");
    printf("Running application...\n");

    // Run the executable
    char* lib_build_path = paths_get_build_path();
    char run_cmd[2048];
    snprintf(run_cmd, sizeof(run_cmd),
             "LD_LIBRARY_PATH=\"%s:$LD_LIBRARY_PATH\" \"%s\"",
             lib_build_path, exe_file);
    free(lib_build_path);
    result = system(run_cmd);

    // Cleanup
    remove(exe_file);

    return result == 0 ? 0 : 1;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int cmd_run(int argc, char** argv) {
    const char* target_file = NULL;
    bool free_target = false;
    const char* target_platform = NULL;
    bool explicit_target = false;
    const char* renderer_override = NULL;
    bool enable_hot_reload = false;
    const char* watch_path = NULL;

    // Parse --target, --renderer, and --watch flags
    int new_argc = 0;
    char* new_argv[128];
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--target=", 9) == 0) {
            target_platform = argv[i] + 9;
            explicit_target = true;
        } else if (strncmp(argv[i], "--renderer=", 11) == 0) {
            renderer_override = argv[i] + 11;
        } else if (strcmp(argv[i], "--watch") == 0 || strcmp(argv[i], "-w") == 0) {
            enable_hot_reload = true;
        } else if (strncmp(argv[i], "--watch-path=", 13) == 0) {
            enable_hot_reload = true;
            watch_path = argv[i] + 13;
        } else if (strncmp(argv[i], "--", 2) == 0 && strchr(argv[i], '=')) {
            fprintf(stderr, "Error: Unknown flag '%s'\n", argv[i]);
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

    // Load config early to check for target names in positional args
    KryonConfig* early_config = config_find_and_load();

    // Check if first positional argument is a target name
    if (!explicit_target && argc >= 1 && early_config && early_config->build_targets_count > 0) {
        for (int i = 0; i < early_config->build_targets_count; i++) {
            if (early_config->build_targets[i] && strcmp(argv[0], early_config->build_targets[i]) == 0) {
                target_platform = argv[0];
                explicit_target = true;
                // Shift args
                for (int j = 0; j < argc - 1; j++) {
                    argv[j] = argv[j + 1];
                }
                argc--;
                break;
            }
        }
    }

    // If no explicit target specified, try to get it from config
    if (!target_platform) {
        if (early_config && early_config->build_targets_count > 0 && early_config->build_targets[0]) {
            target_platform = early_config->build_targets[0];
            fprintf(stderr, "[kryon] Using target from kryon.toml: %s\n", target_platform);
        } else {
            target_platform = "desktop";
        }
    }

    // Validate target platform
    if (strcmp(target_platform, "desktop") != 0 &&
        strcmp(target_platform, "android") != 0 &&
        strcmp(target_platform, "terminal") != 0 &&
        strcmp(target_platform, "web") != 0) {
        fprintf(stderr, "Error: Invalid target platform: %s\n", target_platform);
        return 1;
    }

    // If no file specified, use entry from config
    if (argc < 1) {
        KryonConfig* config = config_find_and_load();
        if (!config) {
            fprintf(stderr, "Error: No file specified and no kryon.toml found\n");
            return 1;
        }

        if (!config_validate(config)) {
            config_free(config);
            return 1;
        }

        if (!config->build_entry) {
            fprintf(stderr, "Error: No file specified and no build.entry in kryon.toml\n");
            config_free(config);
            return 1;
        }

        target_file = str_copy(config->build_entry);
        free_target = true;
        config_free(config);
    } else {
        target_file = argv[0];

        // Validate config if it exists
        KryonConfig* config = config_find_and_load();
        if (config) {
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

    // Detect frontend
    const char* frontend = detect_frontend_type(target_file);
    if (!frontend) {
        const char* ext = path_extension(target_file);
        fprintf(stderr, "Error: Unsupported file type: %s\n", ext);
        if (free_target) free((char*)target_file);
        return 1;
    }

    printf("Running: %s\n", target_file);

    // Get desktop lib path
    const char* desktop_lib = getenv("KRYON_DESKTOP_LIB");
    if (!desktop_lib) {
        char* found_lib = paths_find_library("libkryon_desktop.so");
        if (!found_lib) {
            fprintf(stderr, "Error: Desktop renderer library not found\n");
            if (free_target) free((char*)target_file);
            return 1;
        }
        desktop_lib = found_lib;  // Note: leaked but used immediately
    }

    int result = 0;

    // C files: compile and run directly
    if (strcmp(frontend, "c") == 0) {
        result = run_c_file(target_file);
        if (free_target) free((char*)target_file);
        return result;
    }

    // KIR files: execute directly
    if (strcmp(frontend, "kir") == 0) {
        result = run_kir_file(target_file, target_platform, renderer_override,
                             enable_hot_reload, watch_path);
        if (free_target) free((char*)target_file);
        return result;
    }

    // All other source files: compile to KIR first, then run
    char kir_file[1024];
    const char* basename = strrchr(target_file, '/');
    basename = basename ? basename + 1 : target_file;
    snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s", basename);
    char* dot = strrchr(kir_file, '.');
    if (dot) strcpy(dot, ".kir");

    printf("Compiling %s...\n", target_file);
    int compile_result = compile_source_to_kir(target_file, kir_file);
    if (compile_result != 0) {
        fprintf(stderr, "Error: Failed to compile %s\n", target_file);
        if (free_target) free((char*)target_file);
        return 1;
    }

    printf("✓ Compiled to KIR: %s\n", kir_file);
    result = run_kir_file(kir_file, target_platform, renderer_override,
                         enable_hot_reload, watch_path);

    if (free_target) free((char*)target_file);
    return result;
}

/* ============================================================================
 * Weak Stub Implementations (for when desktop backend not linked)
 * ============================================================================ */

__attribute__((weak))
DesktopRendererConfig desktop_renderer_config_sdl3(int width, int height, const char* title) {
    (void)width;
    (void)height;
    (void)title;
    fprintf(stderr, "Error: Desktop backend not available. Please build the desktop backend.\n");
    fprintf(stderr, "Run: cd backends/desktop && make\n");
    DesktopRendererConfig config = {0};
    return config;
}

__attribute__((weak))
bool desktop_render_ir_component(IRComponent* root, const DesktopRendererConfig* config) {
    (void)root;
    (void)config;
    fprintf(stderr, "Error: Desktop backend not available. Please build the desktop backend.\n");
    fprintf(stderr, "Run: cd backends/desktop && make\n");
    return false;
}
