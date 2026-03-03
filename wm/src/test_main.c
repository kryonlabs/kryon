/*
 * Kryon Window Manager - Test Connection to Marrow
 * C89/C90 compliant
 *
 * Simple test program that connects to Marrow
 */

#include "p9client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/*
 * Marrow client connection (for /dev/draw access)
 */
static P9Client *g_marrow_client = NULL;
static int g_marrow_draw_fd = -1;

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
 * Main entry point
 */
int main(int argc, char **argv)
{
    int i;

    /* Parse arguments - now just takes Marrow address */
    char *marrow_addr = "tcp!localhost!17010";
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--marrow") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --marrow requires an argument\n");
                return 1;
            }
            marrow_addr = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--help") == 0) {
            fprintf(stderr, "Usage: %s [OPTIONS]\n", argv[0]);
            fprintf(stderr, "\n");
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "  --marrow ADDR   Marrow server address (default: tcp!localhost!17010)\n");
            fprintf(stderr, "  --help          Show this help message\n");
            fprintf(stderr, "\n");
            return 0;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Use --help for usage\n");
            return 1;
        }
    }

    /* Initialize subsystems */
    fprintf(stderr, "Kryon Window Manager v0.3 - Test Mode\n");
    fprintf(stderr, "Connecting to Marrow at %s...\n", marrow_addr);

    /* Connect to Marrow */
    g_marrow_client = p9_connect(marrow_addr);
    if (g_marrow_client == NULL) {
        fprintf(stderr, "Error: failed to connect to Marrow at %s\n", marrow_addr);
        fprintf(stderr, "Is Marrow running? Start it with: ./marrow/bin/marrow\n");
        return 1;
    }

    /* Authenticate with Marrow */
    if (p9_authenticate(g_marrow_client, 0, "none", "") < 0) {
        fprintf(stderr, "Error: failed to authenticate with Marrow\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    fprintf(stderr, "Authenticated with Marrow\n");

    /* Open /dev/draw/new to get screen info */
    g_marrow_draw_fd = p9_open(g_marrow_client, "/dev/draw/new", 0);
    if (g_marrow_draw_fd < 0) {
        fprintf(stderr, "Error: failed to open /dev/draw/new on Marrow\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    /* Read 144-byte screen info from Marrow */
    {
        unsigned char screen_info[144];
        ssize_t n = p9_read(g_marrow_client, g_marrow_draw_fd, screen_info, sizeof(screen_info));
        if (n < 144) {
            fprintf(stderr, "Error: only read %d bytes of screen info (expected 144)\n", (int)n);
            p9_disconnect(g_marrow_client);
            return 1;
        }
        fprintf(stderr, "Connected to Marrow screen (read %d bytes info)\n", (int)n);
    }

    fprintf(stderr, "\nSUCCESS: Kryon can connect to Marrow and use /dev/draw!\n");

    /* Simple event loop */
    fprintf(stderr, "\nKryon window manager running (test mode)\n");
    fprintf(stderr, "Press Ctrl-C to exit\n");

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Simple loop - just wait for signals */
    while (running) {
        sleep(1);
    }

    /* Cleanup */
    fprintf(stderr, "\nShutting down...\n");

    p9_disconnect(g_marrow_client);

    fprintf(stderr, "Shutdown complete\n");

    return 0;
}
