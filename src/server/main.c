/*
 * Kryon Server - Main Entry Point
 * C89/C90 compliant
 */

#include "kryon.h"
#include "tcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

/* Forward declarations for tree functions that need node access */
extern P9Node *tree_create_file(P9Node *parent, const char *name, void *data,
                                ssize_t (*read)(char *, size_t, uint64_t),
                                ssize_t (*write)(const char *, size_t, uint64_t));

/*
 * Toggle state - the single bit we're flipping
 */
static int toggle_state = 0;

/*
 * Toggle read function
 * Returns "0" or "1" based on current state
 */
ssize_t toggle_read(char *buf, size_t count, uint64_t offset)
{
    const char *val;

    if (buf == NULL || count == 0) {
        return 0;
    }

    val = toggle_state ? "1" : "0";

    /* Handle offset */
    if (offset >= 1) {
        return 0;  /* EOF */
    }

    return (ssize_t)snprintf(buf, count, "%s", val);
}

/*
 * Toggle write function
 * Sets state based on written value ("0" or "1")
 */
ssize_t toggle_write(const char *buf, size_t count, uint64_t offset)
{
    if (buf == NULL || count == 0) {
        return 0;
    }

    /* Only care about the first character */
    if (buf[0] == '1') {
        toggle_state = 1;
        fprintf(stderr, "Toggle: 0 -> 1\n");
    } else if (buf[0] == '0') {
        toggle_state = 0;
        fprintf(stderr, "Toggle: 1 -> 0\n");
    }

    return (ssize_t)count;
}

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
    fprintf(stderr, "  --port PORT    TCP port to listen on (default: 17019)\n");
    fprintf(stderr, "  --help         Show this help message\n");
    fprintf(stderr, "\n");
}

/*
 * Parse command line arguments
 */
static int parse_args(int argc, char **argv, int *port)
{
    int i;

    *port = 17019;  /* Default port */

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --port requires an argument\n");
                return -1;
            }
            *port = atoi(argv[i + 1]);
            i++;
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
 * Handle a single client connection
 */
static void handle_client(int client_fd)
{
    uint8_t msg_buf[P9_MAX_MSG];
    uint8_t resp_buf[P9_MAX_MSG];
    int msg_len;
    size_t resp_len;
    int result;

    fprintf(stderr, "Client connected\n");

    /* Message loop */
    while (running) {
        /* Receive message */
        msg_len = tcp_recv_msg(client_fd, msg_buf, sizeof(msg_buf));
        if (msg_len < 0) {
            fprintf(stderr, "Error receiving message\n");
            break;
        }
        if (msg_len == 0) {
            /* No data available */
            usleep(10000);  /* 10ms */
            continue;
        }

        /* Dispatch message */
        resp_len = dispatch_9p(msg_buf, (size_t)msg_len, resp_buf);
        if (resp_len == 0) {
            fprintf(stderr, "Error dispatching message\n");
            break;
        }

        /* Send response */
        result = tcp_send_msg(client_fd, resp_buf, resp_len);
        if (result < 0) {
            fprintf(stderr, "Error sending response\n");
            break;
        }
    }

    fprintf(stderr, "Client disconnected\n");
}

/*
 * Main entry point
 */
int main(int argc, char **argv)
{
    int port;
    int listen_fd;
    int client_fd;
    int result;
    P9Node *toggle_node;

    /* Parse arguments */
    result = parse_args(argc, argv, &port);
    if (result < 0) {
        return 1;
    }
    if (result > 0) {
        return 0;
    }

    /* Initialize subsystems */
    fprintf(stderr, "Kryon Core Server v0.1\n");
    fprintf(stderr, "Initializing...\n");

    if (tree_init() < 0) {
        fprintf(stderr, "Error: failed to initialize file tree\n");
        return 1;
    }

    if (fid_init() < 0) {
        fprintf(stderr, "Error: failed to initialize FID table\n");
        return 1;
    }

    /* Create toggle node */
    toggle_node = tree_create_file(NULL, "toggle", NULL, toggle_read, toggle_write);
    if (toggle_node == NULL) {
        fprintf(stderr, "Error: failed to create toggle node\n");
        return 1;
    }
    fprintf(stderr, "Created /toggle node (initial state: %d)\n", toggle_state);

    /* Start listening */
    fprintf(stderr, "Listening on port %d...\n", port);
    listen_fd = tcp_listen(port);
    if (listen_fd < 0) {
        fprintf(stderr, "Error: failed to listen on port %d\n", port);
        return 1;
    }

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Main loop */
    while (running) {
        client_fd = tcp_accept(listen_fd);
        if (client_fd < 0) {
            if (running) {
                fprintf(stderr, "Error accepting connection\n");
            }
            break;
        }

        /* Handle client (single-threaded for Phase 1) */
        handle_client(client_fd);

        /* Clean up */
        tcp_close(client_fd);
    }

    /* Cleanup */
    fprintf(stderr, "\nShutting down...\n");
    tcp_close(listen_fd);
    fid_cleanup();
    tree_cleanup();

    return 0;
}
