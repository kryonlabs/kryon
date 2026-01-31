/**
 * Test Command
 * Runs Kryon test suites (unit, integration, functional)
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

/**
 * Print usage information
 */
static void print_test_usage(void) {
    printf("\n" COLOR_BOLD "Kryon Test Runner" COLOR_RESET "\n\n");
    printf("Usage:\n");
    printf("  kryon test [options] [file.kyt]\n\n");
    printf("Options:\n");
    printf("  --unit         Run unit tests\n");
    printf("  --integration  Run integration tests\n");
    printf("  --functional   Run functional tests\n");
    printf("  --all          Run all tests (default)\n");
    printf("  --coverage     Run tests with coverage analysis\n");
    printf("  --memcheck     Run tests with valgrind\n");
    printf("  --help         Show this help\n\n");
    printf("Examples:\n");
    printf("  kryon test                 # Run all tests\n");
    printf("  kryon test --unit          # Run unit tests only\n");
    printf("  kryon test test_file.kyt   # Run specific test\n\n");
}

/**
 * Run make in tests directory with specified target
 */
static int run_tests_makefile(const char* target) {
    printf(COLOR_BOLD COLOR_CYAN "Running tests..." COLOR_RESET "\n");

    pid_t pid = fork();
    if (pid == 0) {
        // Child process: run make
        execlp("make", "make", "-C", "tests", target, NULL);
        perror("execlp make");
        exit(1);
    } else if (pid > 0) {
        // Parent: wait for child
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                printf(COLOR_GREEN "✓ Tests passed" COLOR_RESET "\n");
                return 0;
            } else {
                printf(COLOR_RED "✗ Tests failed with exit code %d" COLOR_RESET "\n", exit_code);
                return 1;
            }
        } else {
            printf(COLOR_RED "✗ Tests terminated abnormally" COLOR_RESET "\n");
            return 1;
        }
    } else {
        perror("fork");
        return 1;
    }
}

/**
 * Run specific .kyt file
 */
static int run_kyt_file(const char* filepath) {
    // Check if file exists
    if (!file_exists(filepath)) {
        fprintf(stderr, "Error: Test file not found: %s\n", filepath);
        return 1;
    }

    // For now, show info since .kyt runner isn't fully implemented
    printf("Note: .kyt functional test runner not yet fully implemented\n");
    printf("Test file: %s\n", filepath);
    printf("\nTo run functional tests manually:\n");
    printf("  make -C tests test-functional\n\n");

    // TODO: Parse .kyt file and execute test
    return 0;
}

/**
 * Test Command
 * Runs Kryon test suites based on arguments
 */
int cmd_test(int argc, char** argv) {
    if (argc == 0) {
        // No arguments: run all tests
        return run_tests_makefile("test");
    }

    const char* arg = argv[0];

    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
        print_test_usage();
        return 0;
    }

    if (strcmp(arg, "--unit") == 0) {
        return run_tests_makefile("test-unit");
    }

    if (strcmp(arg, "--integration") == 0) {
        return run_tests_makefile("test-integration");
    }

    if (strcmp(arg, "--functional") == 0) {
        return run_tests_makefile("test-functional");
    }

    if (strcmp(arg, "--all") == 0) {
        return run_tests_makefile("test");
    }

    if (strcmp(arg, "--coverage") == 0) {
        return run_tests_makefile("test-coverage");
    }

    if (strcmp(arg, "--memcheck") == 0) {
        return run_tests_makefile("test-memcheck");
    }

    // Check if it's a .kyt file
    if (strstr(arg, ".kyt") != NULL) {
        return run_kyt_file(arg);
    }

    // Unknown argument
    fprintf(stderr, "Error: Unknown option '%s'\n", arg);
    print_test_usage();
    return 1;
}
