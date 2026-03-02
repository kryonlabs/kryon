/*
 * Kryon CLI - Internal API
 * C89 compliant
 *
 * Flutter-like command-line interface for Kryon development
 */

#ifndef CLI_H
#define CLI_H

#include <stdio.h>
#include <stddef.h>

/* Maximum path lengths */
#define CLI_MAX_PATH 512
#define CLI_MAX_LINE 256

/* Command status codes */
#define CLI_OK 0
#define CLI_ERROR 1
#define CLI_INVALID_ARGS 2
#define CLI_FILE_NOT_FOUND 3
#define CLI_BUILD_FAILED 4

/* Command dispatch table entry */
typedef struct CLICommand {
    const char *name;
    int (*func)(int argc, char **argv);
    const char *description;
    const char *usage;
} CLICommand;

/* Forward declarations for commands */
int cmd_create(int argc, char **argv);
int cmd_build(int argc, char **argv);
int cmd_run(int argc, char **argv);
int cmd_package(int argc, char **argv);
int cmd_device(int argc, char **argv);
int cmd_doctor(int argc, char **argv);
int cmd_stop(int argc, char **argv);
int cmd_status(int argc, char **argv);
int cmd_list(int argc, char **argv);
int cmd_help(int argc, char **argv);

/* Utility functions */
void cli_print_usage(const char *command);
void cli_print_error(const char *fmt, ...);
void cli_print_info(const char *fmt, ...);
void cli_print_success(const char *fmt, ...);

/* File utilities */
int cli_file_exists(const char *path);
int cli_dir_exists(const char *path);
int cli_create_dir(const char *path);
int cli_copy_file(const char *src, const char *dest);
int cli_copy_dir(const char *src, const char *dest);

/* String utilities */
int cli_streq(const char *a, const char *b);
int cli_strstarts(const char *str, const char *prefix);
char* cli_basename(const char *path);
char* cli_dirname(const char *path, char *buf, size_t buflen);

/* YAML parsing (simple line-based) */
typedef struct CLIKryonYaml {
    char name[64];
    char version[32];
    char kryon_version[32];
    char entry_point[CLI_MAX_PATH];
    int targets[8];      /* Target flags */
    int target_count;
} CLIKryonYaml;

#define TARGET_LINUX    (1 << 0)
#define TARGET_APPIMAGE (1 << 1)
#define TARGET_WASM     (1 << 2)
#define TARGET_ANDROID  (1 << 3)

int cli_yaml_parse(const char *path, CLIKryonYaml *yaml);
void cli_yaml_print_default(const char *project_name);

#endif /* CLI_H */
