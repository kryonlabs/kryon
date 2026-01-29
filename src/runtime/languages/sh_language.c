/**
 * @file sh_language.c
 * @brief Inferno shell language plugin
 *
 * This plugin allows Kryon functions to be written in Inferno sh (formerly rc).
 * It executes shell scripts via the TaijiOS/Inferno emu and provides
 * kryonget/kryonset commands for state interaction.
 *
 * Only compiled when KRYON_PLUGIN_INFERNO is defined.
 */

#ifdef KRYON_PLUGIN_INFERNO

#include "runtime.h"
#include "language_plugins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * Check if the emu command is available
 */
static bool check_emu_available(void) {
    // Try to find emu in PATH
    return (system("which emu > /dev/null 2>&1") == 0);
}

/**
 * Create a temporary script file with kryonget/kryonset helpers
 */
static char* create_temp_script(KryonRuntime *runtime, KryonScriptFunction *function) {
    // Create temporary file
    static char temp_path[] = "/tmp/kryon_sh_XXXXXX";
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        return NULL;
    }

    FILE *f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(temp_path);
        return NULL;
    }

    // Write helper functions
    fprintf(f, "#!/dis/sh\n\n");
    fprintf(f, "# Kryon helper functions\n");
    fprintf(f, "fn kryonget { echo GET:$1 }\n");
    fprintf(f, "fn kryonset { echo SET:$1:$2 }\n\n");

    // Write user's function code
    fprintf(f, "# User function code\n");
    if (function->code) {
        fprintf(f, "%s\n", function->code);
    }

    fclose(f);
    chmod(temp_path, 0755);

    return strdup(temp_path);
}

/**
 * Execute the shell script and capture output
 */
static bool execute_shell_script(
    KryonRuntime *runtime,
    const char *script_path,
    char *error_buffer,
    size_t error_buffer_size
) {
    // Build command: emu -r. dis/sh.dis <script>
    char command[1024];
    snprintf(command, sizeof(command), "emu -r. dis/sh.dis %s 2>&1", script_path);

    // Execute and capture output
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size, "Failed to execute shell command");
        }
        return false;
    }

    // Read output line by line
    char line[1024];
    bool success = true;
    while (fgets(line, sizeof(line), pipe)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        // Parse special commands
        if (strncmp(line, "GET:", 4) == 0) {
            // kryonget varname - retrieve and print variable
            const char *varname = line + 4;
            const char *value = kryon_runtime_get_variable(runtime, varname);
            fprintf(stdout, "%s\n", value ? value : "");
        }
        else if (strncmp(line, "SET:", 4) == 0) {
            // kryonset varname value - update variable
            // Format: SET:varname:value
            const char *rest = line + 4;
            const char *colon = strchr(rest, ':');
            if (colon) {
                size_t name_len = colon - rest;
                char *varname = malloc(name_len + 1);
                if (varname) {
                    memcpy(varname, rest, name_len);
                    varname[name_len] = '\0';

                    const char *value = colon + 1;
                    kryon_runtime_set_variable(runtime, varname, value);

                    free(varname);
                }
            }
        }
        else {
            // Regular output - just print to stderr for debugging
            fprintf(stderr, "[sh] %s\n", line);
        }
    }

    int status = pclose(pipe);
    if (status != 0) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Shell script exited with status %d", status);
        }
        success = false;
    }

    return success;
}

// =============================================================================
// SH LANGUAGE EXECUTION
// =============================================================================

static KryonLanguageExecutionResult execute_sh_function(
    KryonRuntime *runtime,
    KryonElement *element,
    KryonScriptFunction *function,
    char *error_buffer,
    size_t error_buffer_size
) {
    (void)element; // Not used for sh scripts

    if (!runtime || !function || !function->code) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size, "Invalid runtime or function");
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    // Create temporary script
    char *script_path = create_temp_script(runtime, function);
    if (!script_path) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size, "Failed to create temporary script");
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    // Execute script
    bool success = execute_shell_script(runtime, script_path, error_buffer, error_buffer_size);

    // Clean up
    unlink(script_path);
    free(script_path);

    return success ? KRYON_LANG_RESULT_SUCCESS : KRYON_LANG_RESULT_ERROR;
}

// =============================================================================
// PLUGIN INTERFACE
// =============================================================================

static bool sh_init(KryonRuntime *runtime) {
    (void)runtime;
    // Check if emu is available during init
    if (!check_emu_available()) {
        fprintf(stderr, "[Kryon/Language] Warning: 'emu' command not found, "
                        "sh language will not be available\n");
    }
    return true;
}

static void sh_shutdown(KryonRuntime *runtime) {
    (void)runtime;
    // No cleanup needed
}

static bool sh_is_available(KryonRuntime *runtime) {
    (void)runtime;
    return check_emu_available();
}

static KryonLanguagePlugin sh_plugin = {
    .identifier = "sh",
    .name = "Inferno Shell",
    .version = "1.0.0",
    .init = sh_init,
    .shutdown = sh_shutdown,
    .is_available = sh_is_available,
    .execute = execute_sh_function,
};

// =============================================================================
// AUTO-REGISTRATION
// =============================================================================

__attribute__((constructor))
static void register_sh_language(void) {
    kryon_language_register(&sh_plugin);
}

#endif // KRYON_PLUGIN_INFERNO
