/*
 * Kryon Server - Main Entry Point
 * C89/C90 compliant
 */

#include "kryon.h"
#include "graphics.h"
#include "window.h"
#include "widget.h"
#include "events.h"
#include "tcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>

/*
 * Device initialization functions (external)
 */
extern int devscreen_init(P9Node *dev_dir, Memimage *screen);
extern void devscreen_cleanup(void);
extern int devmouse_init(P9Node *dev_dir);
extern int devdraw_new_init(P9Node *draw_dir);
extern int drawconn_init(struct Memimage *screen);
extern void drawconn_cleanup(void);
extern int devcons_init(P9Node *dev_dir);
extern int devkbd_init(P9Node *dev_dir);
extern int devfd_init(P9Node *dev_dir);
extern int devproc_init(P9Node *root);
extern int devenv_init(P9Node *root);
extern void render_set_screen(Memimage *screen);
extern void render_all(void);

/*
 * CPU server initialization (external)
 */
#ifdef INCLUDE_CPU_SERVER
extern int cpu_server_init(P9Node *root);
extern void p9_set_client_fd(int fd);
#endif

/*
 * Namespace manager initialization (external)
 */
#ifdef INCLUDE_NAMESPACE
extern int namespace_init(void);
#endif

/*
 * Client tracking for multi-client support
 */
#define MAX_CLIENTS 16

typedef struct {
    int fd;
    uint32_t client_id;
    time_t connect_time;
    int is_display_client;
} ClientInfo;

static ClientInfo g_clients[MAX_CLIENTS];
static int g_nclients = 0;
static uint32_t g_next_client_id = 1;

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
 * Add client to tracking
 */
static int add_client(int fd)
{
    if (g_nclients >= MAX_CLIENTS) {
        return -1;
    }

    g_clients[g_nclients].fd = fd;
    g_clients[g_nclients].client_id = g_next_client_id++;
    g_clients[g_nclients].connect_time = time(NULL);
    g_clients[g_nclients].is_display_client = 0;
    g_nclients++;

    fprintf(stderr, "Client %u connected (fd=%d, total=%d)\n",
            g_clients[g_nclients - 1].client_id, fd, g_nclients);

    return 0;
}

/*
 * Remove client from tracking
 */
static void remove_client(int fd)
{
    int i;

    for (i = 0; i < g_nclients; i++) {
        if (g_clients[i].fd == fd) {
            fprintf(stderr, "Client %u disconnected (fd=%d)\n",
                    g_clients[i].client_id, fd);
            tcp_close(fd);

            /* Move last entry into this slot */
            if (i < g_nclients - 1) {
                g_clients[i] = g_clients[g_nclients - 1];
            }
            g_nclients--;
            return;
        }
    }
}

/*
 * Handle a single client request (non-blocking)
 */
static int handle_client_request(ClientInfo *client)
{
    uint8_t msg_buf[P9_MAX_MSG];
    uint8_t resp_buf[P9_MAX_MSG];
    int msg_len;
    size_t resp_len;
    int result;

    /* Set current client fd for CPU server tracking */
#ifdef INCLUDE_CPU_SERVER
    extern void p9_set_client_fd(int fd);
    p9_set_client_fd(client->fd);
#endif

    /* Receive message (non-blocking) */
    msg_len = tcp_recv_msg(client->fd, msg_buf, sizeof(msg_buf));
    if (msg_len < 0) {
        /* Error or disconnect */
        return -1;
    }
    if (msg_len == 0) {
        /* No data available */
        return 0;
    }

    /* Dispatch message */
    resp_len = dispatch_9p(msg_buf, (size_t)msg_len, resp_buf);
    if (resp_len == 0) {
        /* Error */
        return -1;
    }

    /* Send response */
    result = tcp_send_msg(client->fd, resp_buf, resp_len);
    if (result < 0) {
        return -1;
    }

    return 1;  /* Handled a message */
}

/*
 * Create static file (read-only)
 */
typedef struct {
    const char *content;
} StaticFileData;

static ssize_t static_file_read(char *buf, size_t count, uint64_t offset, void *vdata)
{
    StaticFileData *data = (StaticFileData *)vdata;
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

    if (event_system_init() < 0) {
        fprintf(stderr, "Error: failed to initialize event system\n");
        return 1;
    }

#ifdef INCLUDE_NAMESPACE
    /* Initialize namespace manager */
    if (namespace_init() < 0) {
        fprintf(stderr, "Error: failed to initialize namespace manager\n");
        return 1;
    }
#endif

    /* Get root node */
    root = tree_root();
    if (root == NULL) {
        fprintf(stderr, "Error: failed to get root node\n");
        return 1;
    }

#ifdef INCLUDE_CPU_SERVER
    /* Initialize CPU server */
    if (cpu_server_init(root) < 0) {
        fprintf(stderr, "Warning: failed to initialize CPU server\n");
    }
#endif

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

        /* Verify screen buffer allocation */
        {
            Rectangle screen_rect = Rect(0, 0, 800, 600);
            fprintf(stderr, "Screen alloc: ptr=%p, size=%lu, first_pixel=%02X%02X%02X%02X\n",
                    (void *)screen->data->bdata,
                    (unsigned long)(Dx(screen_rect) * Dy(screen_rect) * 4),
                    screen->data->bdata[0], screen->data->bdata[1],
                    screen->data->bdata[2], screen->data->bdata[3]);
        }

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

    /* Initialize draw connection system */
    if (drawconn_init(screen) < 0) {
        fprintf(stderr, "Error: failed to initialize draw connection system\n");
        return 1;
    }

    /* Create /dev/draw directory */
    {
        P9Node *draw_dir;
        draw_dir = tree_create_dir(dev_dir, "draw");
        if (draw_dir == NULL) {
            fprintf(stderr, "Error: failed to create draw directory\n");
            return 1;
        }

        /* Initialize /dev/draw/new */
        if (devdraw_new_init(draw_dir) < 0) {
            fprintf(stderr, "Warning: failed to initialize /dev/draw/new\n");
        }
    }

    /* Initialize devices */
    if (devscreen_init(dev_dir, screen) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/screen\n");
    }

    if (devmouse_init(dev_dir) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/mouse\n");
    }

    if (devkbd_init(dev_dir) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/kbd\n");
    }

    if (devcons_init(dev_dir) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/cons\n");
    }

    if (devfd_init(dev_dir) < 0) {
        fprintf(stderr, "Warning: failed to initialize /dev/fd\n");
    }

    if (devproc_init(root) < 0) {
        fprintf(stderr, "Warning: failed to initialize /proc\n");
    }

    if (devenv_init(root) < 0) {
        fprintf(stderr, "Warning: failed to initialize /env\n");
    }


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

    /* Set window rectangle to start at 0,0 to be fully visible */
    window_set_rect(win1, "0 0 800 600");

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

    /* Set button rectangle: x y width height */
    free(btn->prop_rect);
    btn->prop_rect = (char *)malloc(16);
    if (btn->prop_rect != NULL) {
        strcpy(btn->prop_rect, "50 50 200 50");  /* x=50, y=50, w=200, h=50 */
    }

    result = widget_create_fs_entries(btn, win1->widgets_node);
    if (result < 0) {
        fprintf(stderr, "Error: failed to create button FS entries\n");
        return 1;
    }

    window_add_widget(win1, btn);
    fprintf(stderr, "  Created widget %u: button (text='%s', rect='%s')\n",
            btn->id, btn->prop_text, btn->prop_rect);

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

    /* Set label rectangle: x y width height */
    free(lbl->prop_rect);
    lbl->prop_rect = (char *)malloc(16);
    if (lbl->prop_rect != NULL) {
        strcpy(lbl->prop_rect, "50 120 300 40");  /* x=50, y=120, w=300, h=40 */
    }

    result = widget_create_fs_entries(lbl, win1->widgets_node);
    if (result < 0) {
        fprintf(stderr, "Error: failed to create label FS entries\n");
        return 1;
    }

    window_add_widget(win1, lbl);
    fprintf(stderr, "  Created widget %u: label (text='%s', rect='%s')\n",
            lbl->id, lbl->prop_text, lbl->prop_rect);

    /* Initial render */
    render_all();
    fprintf(stderr, "  Render complete\n");

    /* Start listening */
    fprintf(stderr, "Listening on 0.0.0.0:%d...\n", port);
    listen_fd = tcp_listen(port);
    if (listen_fd < 0) {
        fprintf(stderr, "Error: failed to listen on port %d\n", port);
        return 1;
    }

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Main loop - select-based I/O multiplexing */
    fprintf(stderr, "Main loop started (multi-client mode)\n");

    /* Initialize render timing */
    {
        struct timeval last_render;
        int render_interval_ms = 33;  /* ~30 FPS */

        gettimeofday(&last_render, NULL);

        while (running) {
            fd_set readfds;
            struct timeval tv;
            int max_fd;
            int i;
            int select_result;
            struct timeval current_time;
            double elapsed;

            FD_ZERO(&readfds);
            FD_SET(listen_fd, &readfds);
            max_fd = listen_fd;

            /* Add all clients to fd_set */
            for (i = 0; i < g_nclients; i++) {
                FD_SET(g_clients[i].fd, &readfds);
                if (g_clients[i].fd > max_fd) {
                    max_fd = g_clients[i].fd;
                }
            }

            /* Select with 100ms timeout */
            tv.tv_sec = 0;
            tv.tv_usec = 100000;

            select_result = select(max_fd + 1, &readfds, NULL, NULL, &tv);
            if (select_result < 0) {
                if (running) {
                    fprintf(stderr, "select error\n");
                }
                break;
            }

            /* Check for new connections */
            if (FD_ISSET(listen_fd, &readfds)) {
                client_fd = tcp_accept(listen_fd);
                if (client_fd >= 0) {
                    if (add_client(client_fd) < 0) {
                        fprintf(stderr, "Too many clients, rejecting connection\n");
                        tcp_close(client_fd);
                    }
                }
            }

            /* Handle client requests */
            for (i = g_nclients - 1; i >= 0; i--) {
                if (FD_ISSET(g_clients[i].fd, &readfds)) {
                    int result = handle_client_request(&g_clients[i]);
                    if (result < 0) {
                        /* Client disconnected */
                        remove_client(g_clients[i].fd);
                    }
                }
            }

            /* Check if render needed */
            gettimeofday(&current_time, NULL);
            elapsed = (current_time.tv_sec - last_render.tv_sec) * 1000.0 +
                     (current_time.tv_usec - last_render.tv_usec) / 1000.0;

            /* ALWAYS render every 33ms (30 FPS) */
            if (elapsed >= render_interval_ms) {
                /* Verify render timing */
                static int render_count = 0;
                if (++render_count <= 3) {
                    fprintf(stderr, "Render %d completed at %ld ms\n", render_count,
                            (long)(current_time.tv_sec * 1000 + current_time.tv_usec / 1000));
                }
                render_all();
                gettimeofday(&last_render, NULL);
            }
        }
    }

    /* Cleanup */
    fprintf(stderr, "\nShutting down...\n");

    /* Close all client connections */
    {
        int i;
        for (i = 0; i < g_nclients; i++) {
            tcp_close(g_clients[i].fd);
        }
    }
    g_nclients = 0;

    tcp_close(listen_fd);
    fid_cleanup();
    event_system_cleanup();

    /* Cleanup device states before tree cleanup */
    drawconn_cleanup();
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
