/*
 * Kryon Display Client - Main Entry Point
 * C89/C90 compliant
 */

#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/*
 * Signal handler for graceful shutdown
 */
static volatile int running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

/*
 * Print usage
 */
static void print_usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --host HOST    Server host (default: 127.0.0.1)\n");
    fprintf(stderr, "  --port PORT    Server port (default: 17010)\n");
    fprintf(stderr, "  --width W      Window width (default: 800)\n");
    fprintf(stderr, "  --height H     Window height (default: 600)\n");
    fprintf(stderr, "  --verbose      Enable verbose output\n");
    fprintf(stderr, "  --stay-open    Keep window open (ignore SDL_QUIT events)\n");
    fprintf(stderr, "  --dump-screen  Dump screenshot to /tmp/display_after.raw\n");
    fprintf(stderr, "  --help         Show this help message\n");
    fprintf(stderr, "\n");
}

/*
 * Parse command line arguments
 */
static int parse_args(int argc, char **argv, DisplayConfig *config)
{
    int i;

    /* Set defaults */
    config->host = "127.0.0.1";
    config->port = 17010;  /* Marrow's default port */
    config->width = 800;
    config->height = 600;
    config->verbose = 0;
    config->stay_open = 0;
    config->dump_screen = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --host requires an argument\n");
                return -1;
            }
            config->host = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --port requires an argument\n");
                return -1;
            }
            config->port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--width") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --width requires an argument\n");
                return -1;
            }
            config->width = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--height") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --height requires an argument\n");
                return -1;
            }
            config->height = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = 1;
        } else if (strcmp(argv[i], "--stay-open") == 0) {
            config->stay_open = 1;
        } else if (strcmp(argv[i], "--dump-screen") == 0) {
            config->dump_screen = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 1;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    return 0;
}

/*
 * Main entry point
 */
int main(int argc, char **argv)
{
    DisplayConfig config;
    DisplayClient *dc;
    int result;

    /* Parse arguments */
    result = parse_args(argc, argv, &config);
    if (result < 0) {
        return 1;
    }
    if (result > 0) {
        return 0;
    }

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Create display client */
    fprintf(stderr, "Kryon Display Client v0.1\n");
    fprintf(stderr, "Connecting to %s:%d...\n", config.host, config.port);

    dc = display_client_create(&config);
    if (dc == NULL) {
        fprintf(stderr, "Failed to create display client\n");
        return 1;
    }

    /* Run main loop */
    result = display_client_run(dc);

    /* Cleanup */
    display_client_destroy(dc);

    fprintf(stderr, "Shutdown complete\n");

    return result;
}
