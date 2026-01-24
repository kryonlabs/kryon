/**
 * Target Handler Registry - Dynamic Implementation
 *
 * Provides a registry-based system for handling different build targets.
 * This eliminates hardcoded target checks and allows targets to be
 * registered dynamically with their capabilities and handler functions.
 *
 * KEY CHANGES:
 * - No hardcoded limits (MAX_TARGET_HANDLERS removed)
 * - Dynamic registry that grows as needed
 * - Proper cleanup of dynamically allocated handlers
 * - Thread-local context instead of global g_current_target_name
 */

#include "kryon_cli.h"
#include "build.h"
#include "../../../codegens/kotlin/kotlin_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Forward declare TargetHandler for use in registry */
struct TargetHandler;
typedef struct TargetHandler TargetHandler;

/* ============================================================================
 * Global Registry - Dynamic Allocation
 * ============================================================================ */

typedef struct TargetHandlerRegistry {
    TargetHandler** handlers;
    size_t count;
    size_t capacity;
} TargetHandlerRegistry;

static TargetHandlerRegistry g_registry = {
    .handlers = NULL,
    .count = 0,
    .capacity = 0
};

static bool g_initialized = false;

/* ============================================================================
 * Handler Context (Explicit, not global)
 * ============================================================================ */

/**
 * Thread-local context for current target invocation
 * Replaces fragile g_current_target_name global
 */
static __thread TargetHandlerContext t_current_context = {0};

const char* target_handler_get_current_name(void) {
    return t_current_context.target_name;
}

/* ============================================================================
 * Registry Growth
 * ============================================================================ */

/**
 * Grow the handler registry by doubling capacity
 */
static void registry_grow(TargetHandlerRegistry* reg) {
    size_t new_capacity = reg->capacity == 0 ? 8 : reg->capacity * 2;
    TargetHandler** new_handlers = realloc(reg->handlers,
                                     new_capacity * sizeof(TargetHandler*));
    if (!new_handlers) {
        fprintf(stderr, "Error: Failed to grow target handler registry\n");
        return;
    }
    reg->handlers = new_handlers;
    reg->capacity = new_capacity;
    printf("[target] Registry grown to %zu handlers\n", new_capacity);
}

/* ============================================================================
 * Built-in Target Handler Implementations
 * ============================================================================ */

/**
 * Web target build handler
 */
static int web_target_build(const char* kir_file, const char* output_dir,
                            const char* project_name, const KryonConfig* config) {
    (void)config;  // Not used in web build
    return generate_html_from_kir(kir_file, output_dir, project_name, ".");
}

/**
 * Web target run handler
 */
static int web_target_run(const char* kir_file, const KryonConfig* config) {
    (void)kir_file;
    (void)config;
    fprintf(stderr, "Error: Web target should be run via dev server\n");
    fprintf(stderr, "Use 'kryon dev' or 'kryon run' without explicit target\n");
    return 1;
}

/**
 * Desktop target build handler
 * Currently not implemented - returns error
 */
static int desktop_target_build(const char* kir_file, const char* output_dir,
                                const char* project_name, const KryonConfig* config) {
    (void)kir_file;
    (void)output_dir;
    (void)project_name;
    (void)config;
    fprintf(stderr, "Error: Desktop target build not yet implemented\n");
    return 1;
}

/**
 * Desktop target run handler
 * Runs KIR on desktop using the desktop renderer
 */
static int desktop_target_run(const char* kir_file, const KryonConfig* config) {
    (void)config;
    // Get renderer from config if available
    const char* renderer = NULL;
    if (config) {
        TargetConfig* desktop = config_get_target(config, "desktop");
        if (desktop) {
            renderer = target_config_get_option(desktop, "renderer");
        }
    }
    return run_kir_on_desktop(kir_file, NULL, renderer);
}

/**
 * Terminal target build handler
 */
static int terminal_target_build(const char* kir_file, const char* output_dir,
                                 const char* project_name, const KryonConfig* config) {
    (void)kir_file;
    (void)output_dir;
    (void)project_name;
    (void)config;
    fprintf(stderr, "Error: Terminal target build not yet implemented\n");
    return 1;
}

/**
 * Terminal target run handler
 */
static int terminal_target_run(const char* kir_file, const KryonConfig* config) {
    (void)kir_file;
    (void)config;
    fprintf(stderr, "Error: Terminal target not yet implemented\n");
    return 1;
}

/* ============================================================================
 * Android Target Handler
 * ============================================================================ */

/**
 * Setup Android project structure from KIR
 * Extracted and refactored from cmd_run.c:android_setup_temp_project
 */
static int android_setup_project(const char* output_dir, const char* kir_file) {
    char cmd[4096];

    // Create directory structure
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/app/src/main/assets\"", output_dir);
    if (system(cmd) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/app/src/main/kotlin/com/kryon/temp\"", output_dir);
    if (system(cmd) != 0) return -1;

    // Copy KIR file to assets
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s/app/src/main/assets/app.kir\"", kir_file, output_dir);
    if (system(cmd) != 0) return -1;

    // Create local.properties with SDK location
    char local_props[2048];
    snprintf(local_props, sizeof(local_props), "%s/local.properties", output_dir);
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
    snprintf(settings_file, sizeof(settings_file), "%s/settings.gradle.kts", output_dir);
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
    snprintf(gradle_props, sizeof(gradle_props), "%s/gradle.properties", output_dir);
    f = fopen(gradle_props, "w");
    if (!f) return -1;
    fprintf(f, "android.useAndroidX=true\n");
    fprintf(f, "android.enableJetifier=true\n");
    fclose(f);

    // Create root build.gradle.kts
    char root_build[2048];
    snprintf(root_build, sizeof(root_build), "%s/build.gradle.kts", output_dir);
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
    snprintf(app_build, sizeof(app_build), "%s/app/build.gradle.kts", output_dir);
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
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/app/src/main\"", output_dir);
    int mkdir_result = system(cmd);
    if (mkdir_result != 0) {
        fprintf(stderr, "Warning: Failed to create Android manifest directory (code %d)\n", mkdir_result);
    }

    char manifest[2048];
    snprintf(manifest, sizeof(manifest), "%s/app/src/main/AndroidManifest.xml", output_dir);
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
    snprintf(mainactivity, sizeof(mainactivity), "%s/app/src/main/kotlin/com/kryon/temp/MainActivity.kt", output_dir);

    printf("Generating Kotlin code: %s\n", mainactivity);
    if (!ir_generate_kotlin_code(kir_file, mainactivity)) {
        fprintf(stderr, "Error: Failed to generate Kotlin code from KIR\n");
        return -1;
    }
    printf("✓ Generated MainActivity.kt from KIR\n");

    return 0;
}

/**
 * Android target build handler
 * Generates Android project structure from KIR
 */
static int android_target_build(const char* kir_file, const char* output_dir,
                                const char* project_name, const KryonConfig* config) {
    (void)project_name;
    (void)config;

    printf("[android] Building project to %s\n", output_dir);

    // Generate Android project structure
    if (android_setup_project(output_dir, kir_file) != 0) {
        fprintf(stderr, "Error: Failed to setup Android project\n");
        return 1;
    }

    return 0;
}

/**
 * Android target run handler
 * Builds and installs to connected device
 */
static int android_target_run(const char* kir_file, const KryonConfig* config) {
    (void)config;
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
    if (fgets(device_line, sizeof(device_line), adb_check) == NULL) {
        device_line[0] = '\0';
    }
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
        size_t copy_len = strlen(device_line);
        if (copy_len >= sizeof(device_id)) {
            copy_len = sizeof(device_id) - 1;
        }
        memcpy(device_id, device_line, copy_len);
        device_id[copy_len] = '\0';
        char* newline = strchr(device_id, '\n');
        if (newline) *newline = '\0';
    }

    printf("Using device: %s\n", device_id);

    // 3. Create temporary project directory
    char temp_dir[512];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/kryon_android_%d", getpid());

    printf("Creating temporary Android project: %s\n", temp_dir);
    if (android_setup_project(temp_dir, kir_file) != 0) {
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

    int launch_result = system(launch_cmd);
    if (launch_result != 0) {
        fprintf(stderr, "Warning: Failed to launch app (code %d)\n", launch_result);
    }

    // 7. Cleanup
    printf("Cleaning up temporary files...\n");
    char cleanup_cmd[1024];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", temp_dir);
    int cleanup_result = system(cleanup_cmd);
    if (cleanup_result != 0) {
        fprintf(stderr, "Warning: Failed to cleanup temp files (code %d)\n", cleanup_result);
    }

    printf("✓ Done!\n");
    _exit(0);
}

/* ============================================================================
 * Built-in Handler Definitions
 * ============================================================================ */

static TargetHandler g_web_handler = {
    .name = "web",
    .capabilities = TARGET_REQUIRES_CODEGEN,  // Web requires codegen
    .build_handler = web_target_build,
    .run_handler = web_target_run,
};

static TargetHandler g_desktop_handler = {
    .name = "desktop",
    .capabilities = TARGET_CAN_INSTALL,  // Desktop can be installed
    .build_handler = desktop_target_build,
    .run_handler = desktop_target_run,
};

static TargetHandler g_terminal_handler = {
    .name = "terminal",
    .capabilities = 0,  // No special capabilities
    .build_handler = terminal_target_build,
    .run_handler = terminal_target_run,
};

static TargetHandler g_android_handler = {
    .name = "android",
    .capabilities = TARGET_CAN_INSTALL,
    .build_handler = android_target_build,
    .run_handler = android_target_run,
};

/* ============================================================================
 * Command Target Handlers
 * ============================================================================ */

/**
 * Variable expansion for command targets
 * Replaces ${KIR_FILE}, ${OUTPUT_DIR}, ${PROJECT_NAME}, ${ENTRY}
 */
static char* expand_command_vars(const char* cmd_template,
                                 const char* kir_file,
                                 const char* output_dir,
                                 const char* project_name,
                                 const KryonConfig* config) {
    if (!cmd_template) return NULL;

    char* result = strdup(cmd_template);

    // Expand ${KIR_FILE}
    if (kir_file && strstr(result, "${KIR_FILE}")) {
        char* temp = result;
        result = string_replace(temp, "${KIR_FILE}", kir_file);
        free(temp);
    }

    // Expand ${OUTPUT_DIR}
    if (output_dir && strstr(result, "${OUTPUT_DIR}")) {
        char* temp = result;
        result = string_replace(temp, "${OUTPUT_DIR}", output_dir);
        free(temp);
    }

    // Expand ${PROJECT_NAME}
    if (project_name && strstr(result, "${PROJECT_NAME}")) {
        char* temp = result;
        result = string_replace(temp, "${PROJECT_NAME}", project_name);
        free(temp);
    }

    // Expand ${ENTRY}
    if (config && config->build_entry && strstr(result, "${ENTRY}")) {
        char* temp = result;
        result = string_replace(temp, "${ENTRY}", config->build_entry);
        free(temp);
    }

    return result;
}

/**
 * Generic command target build handler
 * Uses thread-local context to identify which specific target config to use
 */
static int command_target_build(const char* kir_file, const char* output_dir,
                                const char* project_name, const KryonConfig* config) {
    if (!config) {
        fprintf(stderr, "Error: Config required for command targets\n");
        return 1;
    }

    const char* target_name = target_handler_get_current_name();
    if (!target_name) {
        fprintf(stderr, "Error: Target name not set in context\n");
        return 1;
    }

    // Find the specific target config by name
    TargetConfig* target = config_get_target(config, target_name);
    if (!target) {
        fprintf(stderr, "Error: Target config not found: %s\n", target_name);
        return 1;
    }

    if (!target->build_cmd) {
        fprintf(stderr, "Error: Target '%s' missing build command\n", target_name);
        return 1;
    }

    char* cmd = expand_command_vars(target->build_cmd, kir_file,
                                    output_dir, project_name, config);

    printf("[command:%s] Building: %s\n", target->name, cmd);
    int result = system(cmd);

    free(cmd);
    return result;
}

/**
 * Generic command target run handler
 * Uses thread-local context to identify which specific target config to use
 */
static int command_target_run(const char* kir_file, const KryonConfig* config) {
    if (!config) {
        fprintf(stderr, "Error: Config required for command targets\n");
        return 1;
    }

    const char* target_name = target_handler_get_current_name();
    if (!target_name) {
        fprintf(stderr, "Error: Target name not set in context\n");
        return 1;
    }

    // Find the specific target config by name
    TargetConfig* target = config_get_target(config, target_name);
    if (!target) {
        fprintf(stderr, "Error: Target config not found: %s\n", target_name);
        return 1;
    }

    if (!target->run_cmd) {
        fprintf(stderr, "Error: Target '%s' missing run command\n", target_name);
        return 1;
    }

    char* cmd = expand_command_vars(target->run_cmd, kir_file,
                                    NULL, NULL, config);

    printf("[command:%s] Running: %s\n", target->name, cmd);
    int result = system(cmd);

    free(cmd);
    return result;
}

/**
 * Alias target build handler
 * Delegates to the base target's build handler
 */
static int alias_target_build(const char* kir_file, const char* output_dir,
                             const char* project_name, const KryonConfig* config) {
    if (!config) {
        fprintf(stderr, "Error: Config required for alias targets\n");
        return 1;
    }

    const char* target_name = target_handler_get_current_name();
    if (!target_name) {
        fprintf(stderr, "Error: Target name not set in context\n");
        return 1;
    }

    // Find the alias target config
    TargetConfig* alias = config_get_target(config, target_name);
    if (!alias || !alias->alias_target) {
        fprintf(stderr, "Error: Alias target config not found: %s\n", target_name);
        return 1;
    }

    printf("[alias:%s] Delegating to base target: %s\n", target_name, alias->alias_target);

    // Call the base target's build handler
    return target_handler_build(alias->alias_target, kir_file, output_dir, project_name, config);
}

/**
 * Alias target run handler
 * Delegates to the base target's run handler
 */
static int alias_target_run(const char* kir_file, const KryonConfig* config) {
    if (!config) {
        fprintf(stderr, "Error: Config required for alias targets\n");
        return 1;
    }

    const char* target_name = target_handler_get_current_name();
    if (!target_name) {
        fprintf(stderr, "Error: Target name not set in context\n");
        return 1;
    }

    // Find the alias target config
    TargetConfig* alias = config_get_target(config, target_name);
    if (!alias || !alias->alias_target) {
        fprintf(stderr, "Error: Alias target config not found: %s\n", target_name);
        return 1;
    }

    printf("[alias:%s] Delegating to base target: %s\n", target_name, alias->alias_target);

    // Call the base target's run handler
    return target_handler_run(alias->alias_target, kir_file, config);
}

/* ============================================================================
 * Registry API Implementation
 * ============================================================================ */

/**
 * Register a target handler
 * The handler pointer must remain valid for the lifetime of the program
 */
void target_handler_register(TargetHandler* handler) {
    if (!handler || !handler->name) {
        fprintf(stderr, "Error: Invalid handler (must have name)\n");
        return;
    }

    // Check for duplicates
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i] &&
            strcmp(g_registry.handlers[i]->name, handler->name) == 0) {
            fprintf(stderr, "Warning: Handler '%s' already registered\n", handler->name);
            return;
        }
    }

    // Grow if needed
    if (g_registry.count >= g_registry.capacity) {
        registry_grow(&g_registry);
    }

    // Take ownership of handler (caller should NOT free it)
    g_registry.handlers[g_registry.count++] = handler;
    printf("[target] Registered: %s\n", handler->name);
}

/**
 * Unregister a handler by name (with cleanup)
 */
void target_handler_unregister(const char* name) {
    if (!name) return;

    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i] &&
            strcmp(g_registry.handlers[i]->name, name) == 0) {

            // Free handler memory
            if (g_registry.handlers[i]->cleanup) {
                g_registry.handlers[i]->cleanup(g_registry.handlers[i]);
            }

            free(g_registry.handlers[i]->name);
            free(g_registry.handlers[i]);

            // Shift remaining handlers
            for (size_t j = i; j < g_registry.count - 1; j++) {
                g_registry.handlers[j] = g_registry.handlers[j + 1];
            }

            g_registry.count--;
            printf("[target] Unregistered: %s\n", name);
            return;
        }
    }
}

/**
 * Cleanup all dynamic handlers
 * Should be called at program shutdown
 */
void target_handler_cleanup(void) {
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i]) {
            if (g_registry.handlers[i]->cleanup) {
                g_registry.handlers[i]->cleanup(g_registry.handlers[i]);
            }
            free(g_registry.handlers[i]->name);
            free(g_registry.handlers[i]);
        }
    }
    free(g_registry.handlers);
    g_registry.handlers = NULL;
    g_registry.count = 0;
    g_registry.capacity = 0;
}

/**
 * Find a target handler by name
 * Returns NULL if target not found
 */
TargetHandler* target_handler_find(const char* target_name) {
    if (!target_name) {
        return NULL;
    }

    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i] &&
            strcmp(g_registry.handlers[i]->name, target_name) == 0) {
            return g_registry.handlers[i];
        }
    }

    return NULL;
}

/**
 * Check if a target has a specific capability
 */
bool target_has_capability(const char* target_name, TargetCapability capability) {
    TargetHandler* handler = target_handler_find(target_name);
    if (!handler) {
        return false;
    }
    return (handler->capabilities & capability) != 0;
}

/**
 * Initialize the target handler registry with built-in handlers
 * This function is idempotent - calling it multiple times is safe
 */
void target_handler_initialize(void) {
    if (g_initialized) {
        return;  // Already initialized
    }

    // Register built-in handlers (static, not freed)
    target_handler_register(&g_web_handler);
    target_handler_register(&g_desktop_handler);
    target_handler_register(&g_terminal_handler);
    target_handler_register(&g_android_handler);

    g_initialized = true;
}

/* ============================================================================
 * Helper Functions for Commands
 * ============================================================================ */

/**
 * Invoke the build handler for a target
 * Returns 0 on success, non-zero on failure
 */
int target_handler_build(const char* target_name, const char* kir_file,
                         const char* output_dir, const char* project_name,
                         const KryonConfig* config) {
    TargetHandler* handler = target_handler_find(target_name);
    if (!handler) {
        fprintf(stderr, "Error: Unknown target '%s'\n", target_name);
        fprintf(stderr, "Valid targets: ");
        for (size_t i = 0; i < g_registry.count; i++) {
            if (g_registry.handlers[i]) {
                fprintf(stderr, "%s%s", i > 0 ? ", " : "", g_registry.handlers[i]->name);
            }
        }
        fprintf(stderr, "\n");
        return 1;
    }

    if (!handler->build_handler) {
        fprintf(stderr, "Error: Target '%s' does not support build operation\n", target_name);
        return 1;
    }

    // Set thread-local context
    t_current_context.target_name = target_name;
    t_current_context.config = config;
    t_current_context.kir_file = kir_file;
    t_current_context.output_dir = output_dir;
    t_current_context.project_name = project_name;

    int result = handler->build_handler(kir_file, output_dir, project_name, config);

    // Clear context
    memset(&t_current_context, 0, sizeof(t_current_context));

    return result;
}

/**
 * Invoke the run handler for a target
 * Returns 0 on success, non-zero on failure
 */
int target_handler_run(const char* target_name, const char* kir_file,
                       const KryonConfig* config) {
    TargetHandler* handler = target_handler_find(target_name);
    if (!handler) {
        fprintf(stderr, "Error: Unknown target '%s'\n", target_name);
        return 1;
    }

    if (!handler->run_handler) {
        fprintf(stderr, "Error: Target '%s' does not support run operation\n", target_name);
        return 1;
    }

    // Set thread-local context
    t_current_context.target_name = target_name;
    t_current_context.config = config;
    t_current_context.kir_file = kir_file;

    int result = handler->run_handler(kir_file, config);

    // Clear context
    memset(&t_current_context, 0, sizeof(t_current_context));

    return result;
}

/**
 * Get list of all registered target names
 * Returns a NULL-terminated array of strings (caller should not free)
 */
const char** target_handler_list_names(void) {
    static const char** names = NULL;
    static size_t names_capacity = 0;

    // Free previous allocation
    if (names) {
        free(names);
    }

    // Allocate new array
    names = calloc(g_registry.count + 1, sizeof(const char*));
    if (!names) return NULL;

    names_capacity = g_registry.count + 1;

    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i]) {
            names[i] = g_registry.handlers[i]->name;
        }
    }
    names[g_registry.count] = NULL;

    return names;
}

/**
 * Register command targets dynamically from config
 * Called after config is loaded to register targets with build/run commands or aliases
 */
void target_handler_register_command_targets(const KryonConfig* config) {
    if (!config) return;

    for (int i = 0; i < config->targets_count; i++) {
        TargetConfig* target = &config->targets[i];

        // Check if this is an alias target
        bool is_alias = (target->alias_target != NULL);

        // Check if this is a command target
        bool is_command = (target->build_cmd != NULL || target->run_cmd != NULL);

        // Skip if this is neither a command target nor an alias
        if (!is_command && !is_alias) {
            continue;
        }

        // Check if already registered
        bool already_registered = false;
        for (size_t j = 0; j < g_registry.count; j++) {
            if (g_registry.handlers[j] &&
                strcmp(g_registry.handlers[j]->name, target->name) == 0) {
                already_registered = true;
                break;
            }
        }

        if (already_registered) continue;

        // Grow if needed
        if (g_registry.count >= g_registry.capacity) {
            registry_grow(&g_registry);
        }

        // Allocate new handler (dynamically allocated, will be freed on cleanup)
        TargetHandler* handler = (TargetHandler*)calloc(1, sizeof(TargetHandler));
        if (!handler) {
            fprintf(stderr, "Warning: Failed to allocate handler for target %s\n", target->name);
            continue;
        }

        handler->name = strdup(target->name);
        handler->context = NULL;
        handler->cleanup = NULL;  // Will be freed by cleanup function

        if (is_alias) {
            // Alias target - delegates to base target
            handler->capabilities = 0;  // Inherits from base target
            handler->build_handler = alias_target_build;
            handler->run_handler = alias_target_run;
            printf("[target] Registered alias target: %s -> %s\n", target->name, target->alias_target);
        } else {
            // Command target - executes shell commands
            handler->capabilities = 0;  // No special capabilities for command targets
            handler->build_handler = command_target_build;
            handler->run_handler = command_target_run;
            printf("[target] Registered command target: %s\n", target->name);
        }

        // Register the handler
        g_registry.handlers[g_registry.count++] = handler;
    }
}
