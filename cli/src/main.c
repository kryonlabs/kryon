/*
 * Kryon CLI - Main Entry Point
 * C89 compliant
 *
 * Flutter-like command-line interface for Kryon development
 * Replaces kryon.sh bash wrapper with native C implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "cli.h"

/*
 * Get Kryon root directory
 * Returns the path to the Kryon installation
 */
static const char* get_kryon_root(void)
{
    static char root_path[CLI_MAX_PATH] = {0};
    static int initialized = 0;

    if (!initialized) {
        const char *env = getenv("KRYON_ROOT");
        if (env != NULL && strlen(env) > 0) {
            strncpy(root_path, env, sizeof(root_path) - 1);
        } else {
            /* Use executable path */
            FILE *fp = fopen("/proc/self/exe", "r");
            if (fp != NULL) {
                memset(root_path, 0, sizeof(root_path));
                /* Read link would be better, but use C89 approach */
                fclose(fp);

                /* Fallback: use current directory */
                getcwd(root_path, sizeof(root_path) - 1);
            }
        }
        initialized = 1;
    }

    return root_path;
}

/*
 * Print error message to stderr
 */
void cli_print_error(const char *fmt, ...)
{
    va_list args;
    fprintf(stderr, "[tjk] Error: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/*
 * Print info message to stdout
 */
void cli_print_info(const char *fmt, ...)
{
    va_list args;
    printf("[tjk] ");
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

/*
 * Print success message to stdout
 */
void cli_print_success(const char *fmt, ...)
{
    va_list args;
    printf("[tjk] ");
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

/*
 * Print usage information for a command
 */
void cli_print_usage(const char *command)
{
    if (command == NULL || strlen(command) == 0) {
        /* General help */
        printf("Usage: tjk <command> [options]\n");
        printf("\n");
        printf("Available commands:\n");
        printf("  create <name>     Create a new Kryon project\n");
        printf("  build [target]    Build the project for a target\n");
        printf("  run [file.kry]    Run a .kry file (or current project)\n");
        printf("  package [target]  Package the project for distribution\n");
        printf("  device            List connected devices\n");
        printf("  doctor            Diagnose environment issues\n");
        printf("  stop [--all]      Stop running services\n");
        printf("  status            Show status of services\n");
        printf("  list              List available example files\n");
        printf("  help [command]    Show help for a command\n");
        printf("\n");
        printf("Run 'tjk help <command>' for more information on a command.\n");
    } else {
        /* Command-specific help */
        if (cli_streq(command, "create")) {
            printf("Usage: tjk create <name>\n");
            printf("\n");
            printf("Create a new Kryon project with the given name.\n");
            printf("\n");
            printf("Arguments:\n");
            printf("  name              Project name (must be a valid identifier)\n");
            printf("\n");
            printf("Example:\n");
            printf("  tjk create myapp\n");
        } else if (cli_streq(command, "build")) {
            printf("Usage: tjk build [target]\n");
            printf("\n");
            printf("Build the current project for the specified target.\n");
            printf("\n");
            printf("Targets:\n");
            printf("  linux             Build native Linux binary (default)\n");
            printf("  appimage          Build AppImage package\n");
            printf("  wasm              Build WebAssembly version\n");
            printf("  android           Build Android APK\n");
            printf("\n");
            printf("Example:\n");
            printf("  tjk build linux\n");
        } else if (cli_streq(command, "run")) {
            printf("Usage: tjk run [file.kry] [--watch] [--stay-open]\n");
            printf("\n");
            printf("Run a .kry file. If no file is specified, runs the project\n");
            printf("defined in kryon.yaml.\n");
            printf("\n");
            printf("Options:\n");
            printf("  --watch           Enable hot-reload when file changes\n");
            printf("  --stay-open       Keep window open after WM exits\n");
            printf("\n");
            printf("Example:\n");
            printf("  tjk run examples/minimal.kry\n");
            printf("  tjk run --watch\n");
        } else if (cli_streq(command, "stop")) {
            printf("Usage: tjk stop [--all]\n");
            printf("\n");
            printf("Stop running Kryon services.\n");
            printf("\n");
            printf("Options:\n");
            printf("  --all             Also stop Marrow server\n");
            printf("\n");
            printf("Example:\n");
            printf("  tjk stop\n");
            printf("  tjk stop --all\n");
        } else {
            printf("Unknown command: %s\n", command);
            printf("Run 'tjk help' for list of commands.\n");
        }
    }
}

/*
 * String comparison helper
 */
int cli_streq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    return strcmp(a, b) == 0;
}

/*
 * String prefix check helper
 */
int cli_strstarts(const char *str, const char *prefix)
{
    if (str == NULL || prefix == NULL) {
        return 0;
    }
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

/*
 * Get basename from path
 */
char* cli_basename(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return (char*)path;
    }
    return (char*)(last_slash + 1);
}

/*
 * Get dirname from path
 */
char* cli_dirname(const char *path, char *buf, size_t buflen)
{
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL || last_slash == path) {
        strncpy(buf, ".", buflen - 1);
        buf[buflen - 1] = '\0';
    } else {
        size_t len = last_slash - path;
        if (len >= buflen) {
            len = buflen - 1;
        }
        strncpy(buf, path, len);
        buf[len] = '\0';
    }
    return buf;
}

/*
 * Main entry point
 */
int main(int argc, char **argv)
{
    const char *command;

    /* Skip program name */
    if (argc < 2) {
        cli_print_usage(NULL);
        return CLI_INVALID_ARGS;
    }

    command = argv[1];

    /* Dispatch command */
    /* Check for help first */
    if (cli_streq(command, "help") || cli_streq(command, "-h") ||
        cli_streq(command, "--help")) {
        if (argc >= 3) {
            cli_print_usage(argv[2]);
        } else {
            cli_print_usage(NULL);
        }
        return CLI_OK;
    }

    /* Command table lookup (string-based for simplicity) */
    if (cli_streq(command, "create")) {
        return cmd_create(argc - 1, argv + 1);
    } else if (cli_streq(command, "build")) {
        return cmd_build(argc - 1, argv + 1);
    } else if (cli_streq(command, "run")) {
        return cmd_run(argc - 1, argv + 1);
    } else if (cli_streq(command, "package")) {
        return cmd_package(argc - 1, argv + 1);
    } else if (cli_streq(command, "device")) {
        return cmd_device(argc - 1, argv + 1);
    } else if (cli_streq(command, "doctor")) {
        return cmd_doctor(argc - 1, argv + 1);
    } else if (cli_streq(command, "stop")) {
        return cmd_stop(argc - 1, argv + 1);
    } else if (cli_streq(command, "status")) {
        return cmd_status(argc - 1, argv + 1);
    } else if (cli_streq(command, "list")) {
        return cmd_list(argc - 1, argv + 1);
    } else {
        cli_print_error("Unknown command '%s'", command);
        printf("Run 'tjk help' for usage information.\n");
        return CLI_INVALID_ARGS;
    }

    return CLI_OK;
}
