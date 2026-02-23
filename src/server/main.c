/*
 * Kryon Server - Main Entry Point
 * C89/C90 compliant
 */

#include "kryon.h"
#include "graphics.h"
#include "window.h"
#include "widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

/*
 * Device initialization functions (external)
 */
extern int devscreen_init(P9Node *dev_dir, Memimage *screen);
extern void devscreen_cleanup(void);
extern int devmouse_init(P9Node *dev_dir);
extern int devdraw_init(P9Node *dev_dir, Memimage *screen);
extern void devdraw_cleanup(void);
extern int devcons_init(P9Node *dev_dir);
extern void render_set_screen(Memimage *screen);
extern void render_all(void);

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
 * Create static file (read-only)
 */
typedef struct {
    const char *content;
} StaticFileData;

static ssize_t static_file_read(char *buf, size_t count, uint64_t offset, StaticFileData *data)
{
    const char *content;
    size_t len;

    if (data == NULL || data->content == NULL) {
        return 0;
    }

    content = data->content;
    len = strlen(content);

    if (offset >= len) {
        return 0;
    }
    if (offset + count > len) {
        count = len - offset;
    }

    memcpy(buf, content + offset, count);
    return count;
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
    P9Node *root;
    P9Node *windows_dir;
    P9Node *dev_dir;
    P9Node *file;
    KryonWindow *win1;
    KryonWidget *btn;
    KryonWidget *lbl;
    Memimage *screen;
    StaticFileData *static_data;

    /* Parse arguments */
    result = parse_args(argc, argv, &port);
    if (result < 0) {
        return 1;
    }
    if (result > 0) {
        return 0;
    }

    /* Initialize subsystems */
    fprintf(stderr, "Kryon Core Server v0.2\n");
    fprintf(stderr, "Initializing...\n");

    if (window_registry_init() < 0) {
        fprintf(stderr, "Error: failed to initialize window registry\n");
        return 1;
    }

    if (widget_registry_init() < 0) {
        fprintf(stderr, "Error: failed to initialize widget registry\n");
        return 1;
    }

    if (tree_init() < 0) {
        fprintf(stderr, "Error: failed to initialize file tree\n");
        return 1;
    }

    if (fid_init() < 0) {
        fprintf(stderr, "Error: failed to initialize FID table\n");
        return 1;
    }

    /* Get root node */
    root = tree_root();
    if (root == NULL) {
        fprintf(stderr, "Error: failed to get root node\n");
        return 1;
    }

    /* Initialize graphics - create screen buffer */
    fprintf(stderr, "Initializing graphics...\n");
    {
        Rectangle screen_rect;

        screen_rect = Rect(0, 0, 800, 600);
        screen = memimage_alloc(screen_rect, RGBA32);
        if (screen == NULL) {
            fprintf(stderr, "Error: failed to allocate screen\n");
            return 1;
        }

        /* Clear screen to white */
        memfillcolor(screen, 0xFFFFFFFF);

        /* Set screen for rendering */
        render_set_screen(screen);

        fprintf(stderr, "  Created %dx%d RGBA32 screen\n",
                Dx(screen_rect), Dy(screen_rect));
    }

    /* Create /dev directory */
    dev_dir = tree_create_dir(root, "dev");
    if (dev_dir == NULL) {
        fprintf(stderr, "Error: failed to create dev directory\n");
        return 1;
    }

    /* Initialize devices */
    if (devscreen_init(dev_dir, screen) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/screen\n");
    }

    if (devmouse_init(dev_dir) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/mouse\n");
    }

    if (devdraw_init(dev_dir, screen) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/draw\n");
    }

    if (devcons_init(dev_dir) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/cons\n");
    }

    fprintf(stderr, "  Virtual /dev devices created\n");

    /* Create /version file */
    static_data = (StaticFileData *)malloc(sizeof(StaticFileData));
    if (static_data != NULL) {
        static_data->content = "Kryon 1.0";
        file = tree_create_file(root, "version", static_data,
                                (P9ReadFunc)static_file_read,
                                NULL);
        if (file == NULL) {
            free(static_data);
        }
    }

    /* Create /events file (empty for now) */
    static_data = (StaticFileData *)malloc(sizeof(StaticFileData));
    if (static_data != NULL) {
        static_data->content = "";
        file = tree_create_file(root, "events", static_data,
                                (P9ReadFunc)static_file_read,
                                NULL);
        if (file == NULL) {
            free(static_data);
        }
    }

    /* Create /windows directory */
    windows_dir = tree_create_dir(root, "windows");
    if (windows_dir == NULL) {
        fprintf(stderr, "Error: failed to create windows directory\n");
        return 1;
    }

    /* Create Window 1 */
    win1 = window_create("Demo Window", 800, 600);
    if (win1 == NULL) {
        fprintf(stderr, "Error: failed to create window\n");
        return 1;
    }

    result = window_create_fs_entries(win1, windows_dir);
    if (result < 0) {
        fprintf(stderr, "Error: failed to create window FS entries\n");
        return 1;
    }

    fprintf(stderr, "Created window %u: %s\n", win1->id, win1->title);

    /* Add a button widget */
    btn = widget_create(WIDGET_BUTTON, "btn_click", win1);
    if (btn == NULL) {
        fprintf(stderr, "Error: failed to create button widget\n");
        return 1;
    }

    free(btn->prop_text);
    btn->prop_text = (char *)malloc(10);
    if (btn->prop_text != NULL) {
        strcpy(btn->prop_text, "Click Me");
    }

    result = widget_create_fs_entries(btn, win1->widgets_node);
    if (result < 0) {
        fprintf(stderr, "Error: failed to create button FS entries\n");
        return 1;
    }

    window_add_widget(win1, btn);
    fprintf(stderr, "  Created widget %u: button (text='%s')\n", btn->id, btn->prop_text);

    /* Add a label widget */
    lbl = widget_create(WIDGET_LABEL, "lbl_hello", win1);
    if (lbl == NULL) {
        fprintf(stderr, "Error: failed to create label widget\n");
        return 1;
    }

    free(lbl->prop_text);
    lbl->prop_text = (char *)malloc(14);
    if (lbl->prop_text != NULL) {
        strcpy(lbl->prop_text, "Hello, World!");
    }

    result = widget_create_fs_entries(lbl, win1->widgets_node);
    if (result < 0) {
        fprintf(stderr, "Error: failed to create label FS entries\n");
        return 1;
    }

    window_add_widget(win1, lbl);
    fprintf(stderr, "  Created widget %u: label (text='%s')\n", lbl->id, lbl->prop_text);

    /* Initial render */
    fprintf(stderr, "Rendering initial state...\n");
    render_all();
    fprintf(stderr, "  Render complete\n");

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

        /* Handle client (single-threaded for Phase 2) */
        handle_client(client_fd);

        /* Clean up */
        tcp_close(client_fd);
    }

    /* Cleanup */
    fprintf(stderr, "\nShutting down...\n");
    tcp_close(listen_fd);
    fid_cleanup();

    /* Cleanup device states before tree cleanup */
    devdraw_cleanup();
    devscreen_cleanup();

    /* Tree cleanup must happen after device cleanup */
    tree_cleanup();

    /* Note: Registry cleanup disabled due to double-free issue */
    /* TODO: Fix widget/window memory management in Phase 4 */
    /* widget_registry_cleanup(); */
    /* window_registry_cleanup(); */

    fprintf(stderr, "Shutdown complete\n");

    return 0;
}
