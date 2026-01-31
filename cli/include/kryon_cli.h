#ifndef KRYON_CLI_H
#define KRYON_CLI_H

#include <stddef.h>
#include <stdbool.h>

// Version information
#define KRYON_CLI_VERSION "2.0.0-alpha"
#define KRYON_CLI_LICENSE "0BSD"

// ============================================================================
// CLI Arguments
// ============================================================================

typedef struct {
    char* command;      // Command name (e.g., "build", "new")
    int argc;           // Remaining argument count
    char** argv;        // Remaining arguments
    bool help;          // --help flag
    bool version;       // --version flag
} CLIArgs;

CLIArgs* cli_args_parse(int argc, char** argv);
void cli_args_free(CLIArgs* args);

// ============================================================================
// String Utilities
// ============================================================================

char* str_format(const char* fmt, ...);  // NEW: Dynamic string formatting
char* str_concat(const char* s1, const char* s2);
char* str_copy(const char* src);
bool str_ends_with(const char* str, const char* suffix);
bool str_starts_with(const char* str, const char* prefix);
char** str_split(const char* str, char delim, int* count);
void str_free_array(char** arr, int count);
char* string_replace(const char* str, const char* old, const char* new_str);

// ============================================================================
// File Utilities
// ============================================================================

bool file_exists(const char* path);
bool file_is_directory(const char* path);
char* file_read(const char* path);
bool file_write(const char* path, const char* content);
bool dir_create(const char* path);
bool dir_create_recursive(const char* path);
char* dir_get_current(void);
int dir_list_files(const char* dir, const char* ext, char*** files, int* count);
char* path_join(const char* p1, const char* p2);
char* path_resolve_canonical(const char* path, const char* base_dir);
const char* path_extension(const char* path);
char* path_expand_env_vars(const char* path);  // Expand $HOME, $XDG_* vars

// ============================================================================
// Path Discovery
// ============================================================================

char* paths_get_kryon_root(void);
char* paths_get_home_dir(void);
char* paths_get_build_path(void);
char* paths_get_bindings_path(void);
char* paths_get_scripts_path(void);
char* paths_find_library(const char* lib_name);
char* paths_find_plugin(const char* plugin_name, const char* explicit_path, const char* config_dir);

// ============================================================================
// Limbo Module Discovery
// ============================================================================

char** limbo_modules_scan(int* count);  // Scan $TAIJI_PATH/module/*.m for modules
char* limbo_module_get_path(const char* module_alias);  // Get module PATH constant

// ============================================================================
// Process Utilities
// ============================================================================

int process_run(const char* cmd, char** output);
int process_run_async(const char* cmd, void (*callback)(int status));

// ============================================================================
// JSON Utilities (cJSON wrapper)
// ============================================================================

void* json_parse(const char* str);
void json_free(void* json);
char* json_to_string(void* json);

// ============================================================================
// TOML Utilities
// ============================================================================

typedef struct TOMLTable TOMLTable;

TOMLTable* kryon_toml_parse_file(const char* path);
const char* kryon_toml_get_string(TOMLTable* table, const char* key, const char* default_value);
int kryon_toml_get_int(TOMLTable* table, const char* key, int default_value);
bool kryon_toml_get_bool(TOMLTable* table, const char* key, bool default_value);
char** kryon_toml_get_plugin_names(TOMLTable* table, int* count);
void kryon_toml_free(TOMLTable* table);

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    char* name;              // Plugin name (e.g., "storage")
    char* path;              // Path to plugin directory (absolute or relative)
    char* git;               // Git URL for auto-cloning (optional)
    char* branch;            // Git branch for auto-cloning (optional, defaults to "main")
    char* subdir;            // Subdirectory within git repo (optional, for sparse checkout)
    char* version;           // Optional version constraint
    bool enabled;            // Whether plugin is enabled (default: true)
    char* resolved_path;     // Resolved absolute path (populated at runtime)
} PluginDep;

// ============================================================================
// Install Configuration
// ============================================================================

typedef enum {
    INSTALL_MODE_SYMLINK,
    INSTALL_MODE_COPY,
    INSTALL_MODE_SYSTEM
} InstallMode;

typedef struct {
    char* source;            // Source file path (relative to project root)
    char* target;            // Target installation path
} InstallFile;

typedef struct {
    bool enabled;            // Whether to create desktop entry
    char* name;              // Desktop entry name
    char* icon;              // Icon file path
    char** categories;       // Desktop categories array
    int categories_count;    // Number of categories
} InstallDesktop;

typedef struct {
    InstallMode mode;        // Installation mode (symlink, copy, system)
    char* binary_path;       // Where to install binary
    char* binary_name;       // Name of installed binary
    bool binary_executable;  // Make binary executable
    InstallFile* files;      // Additional files to install
    int files_count;         // Number of additional files
    InstallDesktop desktop;  // Desktop entry configuration
    char* target;            // Target to install (desktop, web, etc.) - NULL for auto-detect
} InstallConfig;

// ============================================================================
// Target Configuration
// ============================================================================

/**
 * Target-specific configuration with flexible key-value storage
 * Supports any target type with extensible options
 * Each target only has the options relevant to it (no hardcoded fields)
 */
typedef struct {
    char* name;              // Target name (e.g., "web", "desktop", "custom")

    // For command-based targets (auto-detected by presence):
    char* build_cmd;         // Build command to execute
    char* run_cmd;           // Run command to execute
    char* install_cmd;       // Install command to execute

    // For alias targets (references another target):
    char* alias_target;      // Name of target to alias (e.g., "web")

    // For composite targets (builds multiple targets):
    char** composite_targets;  // Array of target names to build
    int composite_count;       // Number of targets in composite

    // For builtin targets (flexible options):
    char** keys;             // Array of option keys (e.g., "output", "renderer")
    char** values;           // Array of option values (parallel to keys)
    int options_count;       // Number of options
} TargetConfig;

// ============================================================================
// Development Configuration
// ============================================================================

/**
 * Development configuration
 * Uses [dev] section format
 */
typedef struct {
    bool hot_reload;         // Enable hot reload during dev run
    int port;                // Dev server port
    bool auto_open;          // Auto-open browser/server
} DevConfig;

typedef struct {
    // Project metadata
    char* project_name;
    char* project_version;
    char* project_description;

    // Build settings
    char* build_target;         // For backward compat
    char** build_targets;       // Array of target names (required)
    int build_targets_count;
    char* build_output_dir;
    char* build_entry;          // Entry point file (e.g., "main.kry")
    char* build_frontend;       // Frontend language (e.g., "kry")

    // TaijiOS DIS VM configuration
    char* taiji_path;          // Path to TaijiOS installation (for DIS target)
    bool taiji_use_run_app;    // Use run-app.sh wrapper (true) or direct emu invocation (false)
    char* taiji_arch;          // Architecture: x86_64, aarch64, etc.

    // Target-specific configurations
    TargetConfig* targets;      // Array of target-specific configs
    int targets_count;

    // Development configuration
    DevConfig* dev;             // NULL if [dev] section not present

    // Plugins
    PluginDep* plugins;
    int plugins_count;

    // Install configuration
    InstallConfig* install;

    // Limbo module configuration
    char** limbo_modules;       // Array of module names to include (e.g., "string", "sh")
    int limbo_modules_count;    // Number of modules
} KryonConfig;

KryonConfig* config_load(const char* config_path);
KryonConfig* config_find_and_load(void);
bool config_load_plugins(KryonConfig* config);
bool config_validate(KryonConfig* config);
void config_free(KryonConfig* config);

// Target configuration functions
TargetConfig* config_get_target(const KryonConfig* config, const char* target_name);
const char* target_config_get_option(const TargetConfig* target, const char* key);
void target_config_free(TargetConfig* target);
void dev_config_free(DevConfig* dev);

// ============================================================================
// Target Handler Registry
// ============================================================================

/**
 * Target capabilities flags
 */
typedef enum {
    TARGET_CAN_INSTALL = (1 << 0),      // Target can be installed as a command
    TARGET_REQUIRES_CODEGEN = (1 << 1), // Target requires code generation step
} TargetCapability;

/**
 * Screenshot options for targets that support window capture
 */
typedef struct {
    const char* screenshot_path;      // Path where screenshot will be saved
    int screenshot_after_frames;      // Frames to wait before capturing
} ScreenshotRunOptions;

/**
 * Handler context structure
 * Provides explicit context to handlers instead of fragile globals
 */
typedef struct TargetHandlerContext {
    const char* target_name;
    const KryonConfig* config;
    const char* kir_file;
    const char* output_dir;
    const char* project_name;
    ScreenshotRunOptions* screenshot; // Screenshot options (optional)
} TargetHandlerContext;

/**
 * Target handler structure
 * Defines capabilities and handlers for a specific target type
 */
typedef struct TargetHandler {
    char* name;                    // Target name (e.g., "web", "desktop")

    // Target capabilities (bitfield)
    int capabilities;

    // Handler functions (function pointers)
    int (*build_handler)(const char* kir_file, const char* output_dir,
                        const char* project_name, const KryonConfig* config);
    // Run handler can optionally use screenshot_opts if target supports it
    int (*run_handler)(const char* kir_file, const KryonConfig* config,
                       const ScreenshotRunOptions* screenshot_opts);

    // Context pointer for dynamic targets (e.g., command targets)
    // For command targets, this points to the target name string
    void* context;

    // Optional cleanup function for dynamically allocated handlers
    void (*cleanup)(struct TargetHandler* handler);

} TargetHandler;

// Target handler registry functions
void target_handler_register(TargetHandler* handler);
void target_handler_unregister(const char* name);  // NEW: Unregister with cleanup
void target_handler_cleanup(void);  // NEW: Cleanup all dynamic handlers
TargetHandler* target_handler_find(const char* target_name);
bool target_has_capability(const char* target_name, TargetCapability capability);
void target_handler_initialize(void);
const char* target_handler_get_current_name(void);
void target_handler_register_command_targets(const KryonConfig* config);
bool target_is_alias(const char* alias, const char** resolved);  // Check if target is an alias

// Target handler helper functions for commands
int target_handler_build(const char* target_name, const char* kir_file,
                         const char* output_dir, const char* project_name,
                         const KryonConfig* config);
int target_handler_run(const char* target_name, const char* kir_file,
                       const KryonConfig* config,
                       const ScreenshotRunOptions* screenshot_opts);
const char** target_handler_list_names(void);

// ============================================================================
// Command Handlers
// ============================================================================

int cmd_build(int argc, char** argv);
int cmd_new(int argc, char** argv);
int cmd_compile(int argc, char** argv);
int cmd_run(int argc, char** argv);
int cmd_plugin(int argc, char** argv);
int cmd_codegen(int argc, char** argv);
int cmd_inspect(int argc, char** argv);
int cmd_diff(int argc, char** argv);
int cmd_config(int argc, char** argv);
int cmd_test(int argc, char** argv);
int cmd_dev(int argc, char** argv);
int cmd_doctor(int argc, char** argv);
int cmd_install(int argc, char** argv);
int cmd_uninstall(int argc, char** argv);
int cmd_targets(int argc, char** argv);

#endif // KRYON_CLI_H
