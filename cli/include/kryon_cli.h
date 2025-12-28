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

char* str_concat(const char* s1, const char* s2);
char* str_copy(const char* src);
bool str_ends_with(const char* str, const char* suffix);
bool str_starts_with(const char* str, const char* prefix);
char** str_split(const char* str, char delim, int* count);
void str_free_array(char** arr, int count);

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
char* path_join(const char* p1, const char* p2);
const char* path_extension(const char* path);

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

TOMLTable* toml_parse_file(const char* path);
const char* toml_get_string(TOMLTable* table, const char* key, const char* default_value);
int toml_get_int(TOMLTable* table, const char* key, int default_value);
bool toml_get_bool(TOMLTable* table, const char* key, bool default_value);
char** toml_get_plugin_names(TOMLTable* table, int* count);
void toml_free(TOMLTable* table);

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    char* name;
    char* route;
    char* source;
} PageEntry;

typedef struct {
    char* name;              // Plugin name (e.g., "storage")
    char* path;              // Path to plugin directory (absolute or relative)
    char* version;           // Optional version constraint
    bool enabled;            // Whether plugin is enabled (default: true)
    char* resolved_path;     // Resolved absolute path (populated at runtime)
} PluginDep;

typedef struct {
    // Project metadata
    char* project_name;
    char* project_version;
    char* project_author;
    char* project_description;

    // Build settings
    char* build_target;
    char** build_targets;
    int build_targets_count;
    char* build_output_dir;
    char* build_entry;
    char* build_frontend;

    // Pages (multi-page support)
    PageEntry* build_pages;
    int pages_count;

    // Optimization
    bool optimization_enabled;
    bool optimization_minify_css;
    bool optimization_minify_js;
    bool optimization_tree_shake;

    // Development
    bool dev_hot_reload;
    int dev_port;
    bool dev_auto_open;

    // Docs
    bool docs_enabled;
    char* docs_directory;

    // Plugins
    PluginDep* plugins;
    int plugins_count;

    // Desktop renderer configuration
    char* desktop_renderer;  // "sdl3" or "raylib"
} KryonConfig;

KryonConfig* config_load(const char* config_path);
KryonConfig* config_find_and_load(void);
bool config_validate(KryonConfig* config);
void config_free(KryonConfig* config);

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

#endif // KRYON_CLI_H
