/*
 * Kryon Window Manager - Main Entry Point
 * C89/C90 compliant
 *
 * Kryon is now a window manager service that:
 * - Connects to Marrow (port 17010) for graphics via /dev/draw
 * - Registers as window-manager service
 * - Mounts /mnt/wm filesystem
 * - Provides window management
 */

#define _DEFAULT_SOURCE  /* For usleep() */
#include "window.h"
#include "widget.h"
#include "wm.h"
#include "vdev.h"
#include "p9client.h"
#include "marrow.h"
#include "kryon.h"
#include "parser.h"
#include "graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

/*
 * Forward declarations for render functions
 */
void render_set_screen(Memimage *screen);
void render_all(void);

/*
 * Forward declaration for nested WM mode
 */
static int run_nested_wm(const char *win_id, const char *pipe_fd_str);

/*
 * Forward declarations for file watch functions (hot-reload)
 */
static int setup_file_watch(const char *filename);
static void cleanup_file_watch(void);
static int check_file_watch(void);
static int reload_kryon_file(void);

/*
 * Forward declarations for event loop handler functions
 */
static void handle_mouse_events(void);
static void handle_keyboard_events(void);
static void process_render_phase(void);

/*
 * List .kry files in a directory recursively
 */
static void list_kry_files(const char *base_path, const char *prefix)
{
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char path[512];
    int has_files = 0;

    dir = opendir(base_path);
    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Build full path */
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        /* Get file info */
        if (stat(path, &statbuf) < 0) {
            continue;
        }

        /* If directory, recurse */
        if (S_ISDIR(statbuf.st_mode)) {
            char new_prefix[512];
            snprintf(new_prefix, sizeof(new_prefix), "%s%s/", prefix, entry->d_name);
            list_kry_files(path, new_prefix);
        } else if (S_ISREG(statbuf.st_mode)) {
            /* Check if .kry file */
            size_t len = strlen(entry->d_name);
            if (len > 4 && strcmp(entry->d_name + len - 4, ".kry") == 0) {
                if (!has_files) {
                    has_files = 1;
                }
                /* Print without .kry extension */
                printf("  %-30s %s%.*s\n", prefix, prefix, (int)(len - 4), entry->d_name);
            }
        }
    }

    closedir(dir);
}

/*
 * Marrow client connection (for /dev/draw access)
 * Made non-static so namespace.c can access it
 */
P9Client *g_marrow_client = NULL;
static int g_marrow_draw_fd = -1;

/*
 * Global state for file watch mode (hot-reload).
 * Uses stat() polling — portable, no Linux-specific inotify.
 */
static const char *g_watch_file = NULL;
static time_t      g_watch_mtime = 0;

/*
 * Render-dirty flag: set by vdev when mouse/kbd state changes.
 * Main loop calls render_all() when this is non-zero.
 */
int g_render_dirty = 0;

/* Mouse/keyboard FDs on Marrow for reading events */
static int g_marrow_mouse_fd = -1;
static int g_marrow_kbd_fd = -1;

/* Persistent /dev/screen FID — opened once, reused every frame. */
static int g_marrow_screen_fd = -1;

/*
 * Global screen reference for hot-reload
 */
static Memimage *g_screen_buffer = NULL;
static int g_render_width = 0;
static int g_render_height = 0;

/*
 * Push g_screen_buffer pixels to Marrow's /dev/screen.
 * Called after every render_all() so the display client sees updated pixels.
 */
static void push_screen_to_marrow(void)
{
    int chunk_size, bytes_remaining, total_written;
    const unsigned char *buffer_ptr;
    ssize_t written;

    if (g_screen_buffer == NULL || g_render_width == 0 || g_render_height == 0)
        return;

    /* Open /dev/screen/data once; reset offset to 0 on subsequent calls */
    if (g_marrow_screen_fd < 0) {
        g_marrow_screen_fd = p9_open(g_marrow_client, "/dev/screen/data", P9_OWRITE);
        if (g_marrow_screen_fd < 0)
            return;
    } else {
        p9_reset_fid(g_marrow_client, g_marrow_screen_fd);
    }

    chunk_size      = 7000;
    bytes_remaining = g_render_width * g_render_height * 4;
    buffer_ptr      = g_screen_buffer->data->bdata;
    total_written   = 0;

    while (bytes_remaining > 0) {
        int to_write = bytes_remaining < chunk_size ? bytes_remaining : chunk_size;
        written = p9_write(g_marrow_client, g_marrow_screen_fd, buffer_ptr, to_write);
        if (written < 0) break;
        total_written   += (int)written;
        bytes_remaining -= (int)written;
        buffer_ptr      += written;
    }
    (void)total_written;
    /* Do NOT close — keep the FID alive for the next frame */
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
 * ========== Event Loop Handler Functions ==========
 */

/*
 * Handle mouse events from /dev/mouse
 */
static void handle_mouse_events(void)
{
    char mbuf[128];
    ssize_t nr = p9_read(g_marrow_client, g_marrow_mouse_fd, mbuf, sizeof(mbuf) - 1);

    if (nr > 0) {
        mbuf[nr] = '\0';
        KryonWindow *win = window_get(1);
        if (win != NULL) {
            vdev_mouse_write(mbuf, nr, 0, win);
            g_render_dirty = 1;  /* Only mark dirty if window exists */
        }
    }
}

/*
 * Handle keyboard events from /dev/kbd
 */
static void handle_keyboard_events(void)
{
    char kbuf[64];
    ssize_t nr = p9_read(g_marrow_client, g_marrow_kbd_fd, kbuf, sizeof(kbuf) - 1);
    if (nr > 0) {
        KryonWindow *win = window_get(1);
        if (win != NULL) {
            vdev_kbd_write(kbuf, nr, 0, win);
            g_render_dirty = 1;
        }
    }
}


/*
 * Process rendering phase
 */
static void process_render_phase(void)
{
    if (g_render_dirty) {
        g_render_dirty = 0;
        render_all();
        push_screen_to_marrow();
    }
}

/*
 * Main entry point
 */
int main(int argc, char **argv)
{
    int i;
    const char *win_id;
    const char *pipe_fd_str;
    char *marrow_addr;
    int dump_screen;
    const char *load_file;
    int list_examples;
    int blank_mode;
    int watch_mode;

    /* Parse arguments - check for --nested mode first */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--nested") == 0) {
            /* Running in nested WM mode */
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --nested requires window ID argument\n");
                return 1;
            }
            win_id = argv[i + 1];
            pipe_fd_str = getenv("KRYON_NESTED_PIPE_FD");
            return run_nested_wm(win_id, pipe_fd_str);
        }
    }

    /* Normal WM mode */
    marrow_addr = "tcp!localhost!17010";
    dump_screen = 0;  /* Flag to enable screenshot dumps */
    load_file = NULL;
    list_examples = 0;
    blank_mode = 0;
    watch_mode = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--marrow") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --marrow requires an argument\n");
                return 1;
            }
            marrow_addr = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--dump-screen") == 0) {
            dump_screen = 1;
        } else if (strcmp(argv[i], "--run") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --run requires a .kry file path\n");
                return 1;
            }
            load_file = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--list-examples") == 0) {
            list_examples = 1;
        } else if (strcmp(argv[i], "--blank") == 0) {
            blank_mode = 1;
        } else if (strcmp(argv[i], "--watch") == 0) {
            watch_mode = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            fprintf(stderr, "Usage: %s [OPTIONS]\n", argv[0]);
            fprintf(stderr, "\n");
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "  --marrow ADDR        Marrow server address (default: tcp!localhost!17010)\n");
            fprintf(stderr, "  --dump-screen        Dump screenshot to /tmp/wm_before.raw\n");
            fprintf(stderr, "  --run FILE.kry       Load and run the specified .kry file\n");
            fprintf(stderr, "  --watch              Enable hot-reload for .kry file\n");
            fprintf(stderr, "  --list-examples      List available example files\n");
            fprintf(stderr, "  --blank              Clear screen to black and wait\n");
            fprintf(stderr, "  --help               Show this help message\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "Examples:\n");
            fprintf(stderr, "  %s --run examples/minimal.kry              # Run minimal example\n", argv[0]);
            fprintf(stderr, "  %s --run examples/widgets/button.kry       # Run button widget\n", argv[0]);
            fprintf(stderr, "  %s --run myapp.kry                        # Run custom file\n", argv[0]);
            fprintf(stderr, "  %s --blank                                # Clear screen (no UI)\n", argv[0]);
            fprintf(stderr, "\n");
            return 0;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Use --help for usage\n");
            return 1;
        }
    }

    /* Handle --list-examples */
    if (list_examples) {
        printf("Available .kry files in examples/:\n");
        printf("\n");
        list_kry_files("examples", "");
        printf("\n");
        printf("Usage: %s --run <file.kry>\n", argv[0]);
        return 0;
    }

    /* Initialize subsystems */
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
            fprintf(stderr, "Error: only read %ld bytes of screen info (expected 144)\n", (long)n);
            p9_disconnect(g_marrow_client);
            return 1;
        }
        fprintf(stderr, "Connected to Marrow screen (read %ld bytes info)\n", (long)n);
    }

    /* Initialize Kryon's file tree */
    if (tree_init() < 0) {
        fprintf(stderr, "Error: failed to initialize tree\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    /* Create /mnt/wm filesystem */
    if (wm_service_init(tree_root()) < 0) {
        fprintf(stderr, "Error: failed to initialize /mnt/wm filesystem\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    if (window_registry_init() < 0) {
        fprintf(stderr, "Error: failed to initialize window registry\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    if (widget_registry_init() < 0) {
        fprintf(stderr, "Error: failed to initialize widget registry\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    /* NOTE: Event system disabled for now - needs graphics functions from Marrow */
    /*
    if (event_system_init() < 0) {
        fprintf(stderr, "Error: failed to initialize event system\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }
    */

    fprintf(stderr, "Window manager initialized\n");
    fprintf(stderr, "Using Marrow's /dev/draw for rendering\n");

    /* Handle blank mode - clear screen and wait */
    if (blank_mode) {
        Memimage *screen;
        Rectangle screen_rect;
        int screen_width = 800;
        int screen_height = 600;
        int pixel_size;
        unsigned char *pixel_data;
        int screen_fd;
        ssize_t written;

        fprintf(stderr, "Blank mode: clearing screen to black\n");

        /* Create screen rectangle */
        screen_rect.min.x = 0;
        screen_rect.min.y = 0;
        screen_rect.max.x = screen_width;
        screen_rect.max.y = screen_height;

        /* Allocate screen image (RGBA32 format) */
        screen = memimage_alloc(screen_rect, RGBA32);
        if (screen == NULL) {
            fprintf(stderr, "Error: failed to allocate screen image\n");
            p9_disconnect(g_marrow_client);
            return 1;
        }

        /* Clear screen to black */
        {
            Rectangle clear_rect;
            clear_rect.min.x = 0;
            clear_rect.min.y = 0;
            clear_rect.max.x = screen_width;
            clear_rect.max.y = screen_height;
            memfillcolor_rect(screen, clear_rect, 0x000000FF);  /* Black in BGRA */
        }

        /* Extract pixel data */
        pixel_size = screen_width * screen_height * 4;
        pixel_data = screen->data->bdata;

        /* Open /dev/screen/data on Marrow for writing */
        screen_fd = p9_open(g_marrow_client, "/dev/screen/data", P9_OWRITE);
        if (screen_fd < 0) {
            fprintf(stderr, "Warning: failed to open /dev/screen/data for writing\n");
        } else {
            /* Write pixel data to Marrow's screen in chunks */
            int chunk_size = 7000;
            int bytes_remaining = pixel_size;
            const unsigned char *buffer_ptr = pixel_data;
            int total_written = 0;

            while (bytes_remaining > 0) {
                int bytes_to_write = bytes_remaining;
                if (bytes_to_write > chunk_size) {
                    bytes_to_write = chunk_size;
                }

                written = p9_write(g_marrow_client, screen_fd,
                                  buffer_ptr, bytes_to_write);
                if (written < 0) {
                    fprintf(stderr, "Error: failed to write chunk at position %d\n",
                            total_written);
                    break;
                }

                total_written += written;
                bytes_remaining -= written;
                buffer_ptr += written;
            }

            if (total_written != pixel_size) {
                fprintf(stderr, "Warning: only wrote %d of %d bytes\n", total_written, pixel_size);
            }
            p9_close(g_marrow_client, screen_fd);
        }

        /* Wait for Ctrl-C */
        fprintf(stderr, "\nBlank screen mode - waiting for Ctrl-C\n");
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        while (running) {
            sleep(1);
        }

        /* Cleanup */
        fprintf(stderr, "\nShutting down...\n");
        marrow_service_unregister(g_marrow_client, "kryon");
        tree_cleanup();
        p9_disconnect(g_marrow_client);
        fprintf(stderr, "Shutdown complete\n");
        return 0;
    }

    /* Load .kry file(s) */
    {
        int result;

        if (load_file != NULL) {
            /* Load specified file */
            result = kryon_load_file(load_file);
            if (result < 0) {
                fprintf(stderr, "Error: failed to load file: %s\n", load_file);
                fprintf(stderr, "Use --list-examples to see available files\n");
                p9_disconnect(g_marrow_client);
                return 1;
            }
            if (result == 0) {
                fprintf(stderr, "No windows defined in file: %s\n", load_file);
            }
        } else {
            /* No file specified - show help and exit */
            fprintf(stderr, "Error: No .kry file specified\n");
            fprintf(stderr, "Usage: %s --run <file.kry>\n", argv[0]);
            fprintf(stderr, "Use --list-examples to see available files\n");
            p9_disconnect(g_marrow_client);
            return 1;
        }

        fprintf(stderr, "Loaded %d window(s)\n", result);
    }

    /* Setup watch mode if requested */
    if (watch_mode) {
        if (setup_file_watch(load_file) < 0) {
            fprintf(stderr, "Warning: failed to setup file watch\n");
            fprintf(stderr, "Continuing without hot-reload\n");
            watch_mode = 0;
        }
    }

    /* NOTE: Rendering is now done through Marrow's /dev/draw */
    /* Create screen buffer and render windows */
    {
        Memimage *screen;
        Rectangle screen_rect;
        int screen_width = 0;
        int screen_height = 0;

        /* Derive dimensions from first window definition */
        {
            KryonWindow *win = window_get(1);
            if (win != NULL && win->rect != NULL) {
                int rx = 0, ry = 0;
                sscanf(win->rect, "%d %d %d %d", &rx, &ry, &screen_width, &screen_height);
            }
        }

        /* Set Marrow's screen size to match our window dimensions */
        if (screen_width > 0 && screen_height > 0) {
            int ctl_fd;
            char cmd[64];
            ssize_t written;
            int cmd_len;

            cmd_len = snprintf(cmd, sizeof(cmd), "screen %dx%d\n", screen_width, screen_height);
            if (cmd_len > 0 && cmd_len < (int)sizeof(cmd)) {
                ctl_fd = p9_open(g_marrow_client, "/dev/screen/ctl", P9_OWRITE);
                if (ctl_fd >= 0) {
                    written = p9_write(g_marrow_client, ctl_fd, cmd, cmd_len);
                    if (written == cmd_len) {
                        fprintf(stderr, "Set Marrow screen size to %dx%d via /dev/screen/ctl\n",
                                screen_width, screen_height);
                    } else {
                        fprintf(stderr, "Warning: failed to write to /dev/screen/ctl\n");
                    }
                    p9_close(g_marrow_client, ctl_fd);
                } else {
                    fprintf(stderr, "Warning: failed to open /dev/screen/ctl\n");
                }
            }

            /* Also write to /dev/display/ctl for display client coordination */
            {
                int display_ctl_fd;
                char display_cmd[64];
                ssize_t display_written;
                int display_cmd_len;

                display_cmd_len = snprintf(display_cmd, sizeof(display_cmd), "%dx%d\n", screen_width, screen_height);
                if (display_cmd_len > 0 && display_cmd_len < (int)sizeof(display_cmd)) {
                    display_ctl_fd = p9_open(g_marrow_client, "/dev/display/ctl", P9_OWRITE);
                    if (display_ctl_fd >= 0) {
                        display_written = p9_write(g_marrow_client, display_ctl_fd, display_cmd, display_cmd_len);
                        if (display_written == display_cmd_len) {
                            fprintf(stderr, "Set display size to %dx%d via /dev/display/ctl\n",
                                    screen_width, screen_height);
                        } else {
                            fprintf(stderr, "Warning: failed to write to /dev/display/ctl\n");
                        }
                        p9_close(g_marrow_client, display_ctl_fd);
                    } else {
                        fprintf(stderr, "Warning: failed to open /dev/display/ctl\n");
                    }
                }
            }
        }

        /* If no window defined: nothing to render */
        if (screen_width == 0 || screen_height == 0) {
            fprintf(stderr, "No windows defined — screen is 0x0, skipping render\n");
            goto event_loop;
        }

        g_render_width = screen_width;
        g_render_height = screen_height;

        /* Create screen rectangle */
        screen_rect.min.x = 0;
        screen_rect.min.y = 0;
        screen_rect.max.x = screen_width;
        screen_rect.max.y = screen_height;

        /* Allocate screen image (RGBA32 format) */
        screen = memimage_alloc(screen_rect, RGBA32);
        if (screen == NULL) {
            fprintf(stderr, "Error: failed to allocate screen image\n");
            p9_disconnect(g_marrow_client);
            return 1;
        }

        /* Store screen buffer globally for hot-reload */
        g_screen_buffer = screen;

        /* Set screen as global for rendering */
        render_set_screen(screen);

        /* Clear screen to Tk-style gray background */
        {
            Rectangle clear_rect;
            clear_rect.min.x = 0;
            clear_rect.min.y = 0;
            clear_rect.max.x = screen_width;
            clear_rect.max.y = screen_height;
            memfillcolor_rect(screen, clear_rect, 0xD9D9D9FF);  /* Tk-style gray in BGRA */
        }

        /* Render all windows to screen and push pixels to Marrow */
        render_all();
        push_screen_to_marrow();

        /* Open /dev/mouse and /dev/kbd for reading events */
        /* Use streaming API for these devices - they always read from offset 0 */
        g_marrow_mouse_fd = p9_open(g_marrow_client, "/dev/mouse", 0);
        if (g_marrow_mouse_fd >= 0) {
            p9_mark_stream(g_marrow_client, g_marrow_mouse_fd);
        }
        if (g_marrow_mouse_fd < 0) {
            fprintf(stderr, "Warning: failed to open /dev/mouse\n");
        }

        g_marrow_kbd_fd = p9_open(g_marrow_client, "/dev/kbd", 0);
        if (g_marrow_kbd_fd >= 0) {
            p9_mark_stream(g_marrow_client, g_marrow_kbd_fd);
        }
        if (g_marrow_kbd_fd < 0) {
            fprintf(stderr, "Warning: failed to open /dev/kbd\n");
        }
        /* Dump screenshot if requested */
        if (dump_screen) {
            int pixel_size2 = screen_width * screen_height * 4;
            unsigned char *pixel_data2 = screen->data->bdata;
            FILE *dump_file = fopen("/tmp/wm_before.raw", "wb");
            if (dump_file != NULL) {
                fwrite(pixel_data2, 1, pixel_size2, dump_file);
                fclose(dump_file);
            }
        }
    }

    /* Register Kryon as window-manager service and mount /mnt/wm */
    if (marrow_service_register(g_marrow_client, "kryon", "window-manager",
                                "Kryon Window Manager") < 0) {
        fprintf(stderr, "Warning: failed to register with Marrow (continuing anyway)\n");
    }

    if (marrow_namespace_mount(g_marrow_client, "/mnt/wm") < 0) {
        fprintf(stderr, "Warning: failed to mount /mnt/wm\n");
    }

    /* Simple event loop */
    fprintf(stderr, "\nKryon window manager running\n");
    fprintf(stderr, "Press Ctrl-C to exit\n");

event_loop:
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Unified event loop following Plan 9 philosophy */
    if (watch_mode) {
        fprintf(stderr, "Watch mode active - monitoring for file changes\n");
    }

    while (running) {
        struct pollfd fds[2];
        int poll_result;
        int nfds = 0;
        /* C90: declare at top of block */
        int timeout_ms;

        /* Build FD array: mouse and keyboard */
        if (g_marrow_mouse_fd >= 0) {
            fds[nfds].fd = g_marrow_mouse_fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }
        if (g_marrow_kbd_fd >= 0) {
            fds[nfds].fd = g_marrow_kbd_fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }

        if (nfds == 0) {
            /* No input devices - just sleep */
            usleep(10000);
            goto check_controls;
        }

        /* In watch mode, use 1-second timeout to poll file mtime */
        timeout_ms = watch_mode ? 1000 : 16;  /* 16ms = ~60FPS */

        poll_result = poll(fds, nfds, timeout_ms);

        if (poll_result < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "Error: poll() failed: %s\n", strerror(errno));
            break;
        }

        /* Always try to read from streaming devices (mouse/keyboard)
         * regardless of poll() result, since 9P streaming doesn't
         * integrate with poll() properly */
        if (g_marrow_mouse_fd >= 0) {
            handle_mouse_events();
        }

        if (g_marrow_kbd_fd >= 0) {
            handle_keyboard_events();
        }

check_controls:
        /* Check for file changes in watch mode (on timeout or after events) */
        if (watch_mode && check_file_watch()) {
            reload_kryon_file();
        }

        /* Check WM control flags (set by /mnt/wm/ctl writes) */
        if (g_wm_quit_requested) {
            running = 0;
            break;
        }
        if (g_wm_reload_requested) {
            g_wm_reload_requested = 0;
            if (g_wm_load_path[0] != '\0') {
                g_watch_file = g_wm_load_path;
            }
            reload_kryon_file();
            g_wm_load_path[0] = '\0';
        }

        /* Render phase: process dirty flag */
        process_render_phase();
    }

    /* Cleanup watch mode resources */
    if (watch_mode) {
        cleanup_file_watch();
    }

    /* Cleanup */
    fprintf(stderr, "\nShutting down...\n");

    /* Close mouse and keyboard FDs */
    if (g_marrow_mouse_fd >= 0) {
        p9_close(g_marrow_client, g_marrow_mouse_fd);
        g_marrow_mouse_fd = -1;
    }
    if (g_marrow_kbd_fd >= 0) {
        p9_close(g_marrow_client, g_marrow_kbd_fd);
        g_marrow_kbd_fd = -1;
    }

    /* Close persistent screen FID */
    if (g_marrow_screen_fd >= 0) {
        p9_close(g_marrow_client, g_marrow_screen_fd);
        g_marrow_screen_fd = -1;
    }

    /* Unregister from Marrow */
    marrow_service_unregister(g_marrow_client, "kryon");

    /* event_system_cleanup();  TODO: Re-enable when graphics linked */
    tree_cleanup();

    p9_disconnect(g_marrow_client);

    fprintf(stderr, "Shutdown complete\n");

    return 0;
}

/*
 * ========== File Watch Functions for Hot-Reload ==========
 *
 * Uses stat() mtime polling — portable POSIX, no Linux-specific inotify.
 */

/*
 * Setup file watch: record filename and initial mtime.
 */
static int setup_file_watch(const char *filename)
{
    struct stat st;

    if (filename == NULL) {
        return -1;
    }

    if (stat(filename, &st) < 0) {
        fprintf(stderr, "Error: stat(%s) failed: %s\n", filename, strerror(errno));
        return -1;
    }

    g_watch_file  = filename;
    g_watch_mtime = st.st_mtime;

    fprintf(stderr, "Watching for changes: %s\n", filename);
    return 0;
}

/*
 * Cleanup file watch state.
 */
static void cleanup_file_watch(void)
{
    g_watch_file  = NULL;
    g_watch_mtime = 0;
}

/*
 * Check whether the watched file has changed since last check.
 * Returns 1 if changed (and updates the recorded mtime), 0 otherwise.
 */
static int check_file_watch(void)
{
    struct stat st;
    time_t new_mtime;

    if (g_watch_file == NULL) {
        return 0;
    }

    if (stat(g_watch_file, &st) < 0) {
        return 0;
    }

    new_mtime = st.st_mtime;
    if (new_mtime != g_watch_mtime) {
        g_watch_mtime = new_mtime;
        return 1;
    }

    return 0;
}

/*
 * External functions from parser and registries
 */
extern KryonNode *kryon_parse_file(const char *filename);
extern int kryon_execute_ast(KryonNode *ast);
extern void kryon_free_ast(KryonNode *ast);
extern int window_registry_init(void);
extern int widget_registry_init(void);
extern void window_registry_cleanup(void);
extern void widget_registry_cleanup(void);
extern void render_all(void);
extern KryonWindow *window_get(uint32_t index);
extern int window_build_namespace(struct KryonWindow *win);

/*
 * External Marrow client for sending screen updates
 */
extern P9Client *g_marrow_client;

/*
 * Reload the .kry file
 * Parses first, then destroys old state if parse succeeds
 * Returns: number of windows on success, -1 on error (old state preserved)
 */
static int reload_kryon_file(void)
{
    KryonNode *ast;
    int result;
    int screen_width = 0;
    int screen_height = 0;
    int retry_count;
    struct timespec delay;

    fprintf(stderr, "\n=== Reloading %s ===\n", g_watch_file);

    /* Parse the file first with retry for empty file (editor save race) */
    for (retry_count = 0; retry_count < 5; retry_count++) {
        ast = kryon_parse_file(g_watch_file);
        if (ast != NULL) {
            break;  /* Success */
        }

        /* File might be momentarily empty during editor save */
        fprintf(stderr, "Parse failed (attempt %d/5), retrying after brief delay...\n", retry_count + 1);

        /* Sleep for 100ms */
        delay.tv_sec = 0;
        delay.tv_nsec = 100000000;  /* 100 milliseconds */
        nanosleep(&delay, NULL);
    }

    if (ast == NULL) {
        fprintf(stderr, "Parse error after retries: keeping old windows running\n");
        fprintf(stderr, "Fix the file and save to retry\n");
        return -1;
    }

    /* Parse succeeded - destroy old windows/widgets */
    fprintf(stderr, "Parse successful - destroying old windows...\n");
    window_registry_cleanup();
    widget_registry_reset();  /* Free array without re-destroying widgets */

    /* Clear global namespace to prevent using freed namespace tree */
    p9_set_namespace(NULL);
    fprintf(stderr, "Cleared global namespace after cleanup\n");

    /* Re-initialize registries */
    if (window_registry_init() < 0) {
        fprintf(stderr, "Error: failed to re-initialize window registry\n");
        kryon_free_ast(ast);
        return -1;
    }

    if (widget_registry_init() < 0) {
        fprintf(stderr, "Error: failed to re-initialize widget registry\n");
        kryon_free_ast(ast);
        return -1;
    }

    /* Execute new AST */
    result = kryon_execute_ast(ast);

    if (result < 0) {
        fprintf(stderr, "Error: failed to execute new AST\n");
        kryon_free_ast(ast);
        return -1;
    }

    /* Derive new screen dimensions from window definition */
    {
        KryonWindow *win = window_get(1);
        if (win != NULL && win->rect != NULL) {
            int rx = 0, ry = 0;
            sscanf(win->rect, "%d %d %d %d", &rx, &ry, &screen_width, &screen_height);
        }
    }

    if (result == 0 || screen_width == 0 || screen_height == 0) {
        fprintf(stderr, "No windows defined after reload — screen is 0x0\n");
        g_render_width = 0;
        g_render_height = 0;
        kryon_free_ast(ast);
        return result;
    }

    fprintf(stderr, "Reloaded successfully: %d window(s) at %dx%d\n",
            result, screen_width, screen_height);

    /* Reallocate screen buffer if dimensions changed */
    if (screen_width != g_render_width || screen_height != g_render_height) {
        Rectangle screen_rect;
        fprintf(stderr, "Screen size changed from %dx%d to %dx%d — reallocating\n",
                g_render_width, g_render_height, screen_width, screen_height);
        if (g_screen_buffer != NULL) {
            memimage_free(g_screen_buffer);
            g_screen_buffer = NULL;
        }
        screen_rect.min.x = 0;
        screen_rect.min.y = 0;
        screen_rect.max.x = screen_width;
        screen_rect.max.y = screen_height;
        g_screen_buffer = memimage_alloc(screen_rect, RGBA32);
        if (g_screen_buffer == NULL) {
            fprintf(stderr, "Error: failed to reallocate screen buffer\n");
            kryon_free_ast(ast);
            return -1;
        }
        g_render_width = screen_width;
        g_render_height = screen_height;
    }

    /* Set screen for rendering */
    render_set_screen(g_screen_buffer);

    /* Build namespace trees for all newly created windows */
    {
        uint32_t i;
        int ns_built = 0;
        int ns_failed = 0;

        fprintf(stderr, "Building namespace trees for %d window(s)...\n", result);

        for (i = 1; ; i++) {
            KryonWindow *win = window_get(i);
            if (win == NULL) {
                break;
            }

            if (window_build_namespace(win) < 0) {
                fprintf(stderr, "Warning: failed to build namespace for window %u\n", win->id);
                ns_failed++;
            } else {
                ns_built++;
                fprintf(stderr, "Built namespace for window %u (id=%u, %d widgets)\n",
                        i, win->id, win->nwidgets);
            }
        }

        fprintf(stderr, "Namespace build complete: %d succeeded, %d failed\n",
                ns_built, ns_failed);
    }

    /* Re-render all windows and push pixels to Marrow */
    render_all();
    push_screen_to_marrow();

    kryon_free_ast(ast);
    return result;
}

/*
 * Run WM in nested mode
 *
 * In nested mode:
 * - Connects to Marrow as a 9P client
 * - Uses namespace bind mounts to access per-window virtual devices
 * - /dev/screen resolves to /dev/win{N}/screen via bind mount
 * - Receives input events via pipe from parent WM
 */

/*
 * Validate namespace pointer
 * Checks if the namespace looks valid before using it
 *
 * Returns 0 if valid, -1 if invalid
 */
static int validate_namespace_ptr(P9Node *ns_root, uint32_t expected_win_id)
{
    /* Check if namespace looks valid */
    if (ns_root == NULL) {
        fprintf(stderr, "validate_namespace_ptr: NULL pointer\n");
        return -1;
    }

    /* Walk the tree to see if it's structured correctly */
    if (ns_root->name != NULL && strlen(ns_root->name) > 0) {
        /* Non-empty root name is suspicious for our namespace trees */
        fprintf(stderr, "validate_namespace_ptr: suspicious root name '%s'\n",
                ns_root->name);
        return -1;
    }

    /* Check for /dev directory */
    if (ns_root->children == NULL || ns_root->nchildren == 0) {
        fprintf(stderr, "validate_namespace_ptr: no children in namespace\n");
        return -1;
    }

    fprintf(stderr, "validate_namespace_ptr: namespace %p looks valid\n", (void *)ns_root);
    return 0;
}

static int run_nested_wm(const char *win_id, const char *pipe_fd_str)
{
    P9Client *marrow_client;
    char *ns_ptr_str;
    P9Node *ns_root;
    int screen_fd;
    int pipe_fd = -1;
    char event_buf[256];
    ssize_t n;

    fprintf(stderr, "Kryon Nested WM (window %s)\n", win_id);

    /* Connect to Marrow */
    marrow_client = p9_connect("tcp!localhost!17010");
    if (marrow_client == NULL) {
        fprintf(stderr, "Failed to connect to Marrow\n");
        return 1;
    }

    fprintf(stderr, "Connected to Marrow\n");

    /* Authenticate */
    if (p9_authenticate(marrow_client, 0, "none", "") < 0) {
        fprintf(stderr, "Authentication failed\n");
        p9_disconnect(marrow_client);
        return 1;
    }

    /* Get namespace pointer from environment */
    ns_ptr_str = getenv("KRYON_NAMESPACE_PTR");
    if (ns_ptr_str != NULL) {
        /* Parse pointer (format: "0x12345678") */
        unsigned long ptr_val;
        sscanf(ns_ptr_str, "%lx", &ptr_val);
        ns_root = (P9Node *)ptr_val;

        /* Validate namespace pointer */
        if (validate_namespace_ptr(ns_root, atoi(win_id)) < 0) {
            fprintf(stderr, "Warning: Invalid namespace pointer %p, ignoring\n", (void *)ns_root);
            ns_root = NULL;
        } else {
            /* Set namespace for all subsequent 9P operations */
            p9_set_namespace(ns_root);
            fprintf(stderr, "Nested WM using namespace %p\n", (void *)ns_root);
        }
    } else {
        fprintf(stderr, "Warning: No namespace passed from parent WM\n");
    }

    /* Now when we open /dev/screen/data, the namespace resolver
     * will follow the bind mount and actually open /dev/win{N}/screen
     * (which is the virtual device registered with Marrow) */

    screen_fd = p9_open(marrow_client, "/dev/screen/data", P9_OWRITE);
    if (screen_fd < 0) {
        fprintf(stderr, "Failed to open /dev/screen/data (resolves to /dev/win%s/screen)\n",
                win_id);
        p9_disconnect(marrow_client);
        return 1;
    }

    fprintf(stderr, "Opened /dev/screen (fd=%d)\n", screen_fd);

    /* Parse pipe FD from environment */
    if (pipe_fd_str != NULL) {
        pipe_fd = atoi(pipe_fd_str);
        fprintf(stderr, "Using pipe FD %d for input\n", pipe_fd);
    }

    /* TODO: Get parent window and use virtual framebuffer */
    /* For now, we write directly to Marrow's /dev/win{N}/screen */

    fprintf(stderr, "Nested WM running (press Ctrl-C to exit)\n");

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Event loop - render and handle events */
    while (running) {
        /* TODO: Render to /dev/screen (writes to parent's virtual framebuffer) */
        /* For now, just write some test data */
        {
            const char *test_data = "Nested WM test data";
            p9_write(marrow_client, screen_fd, test_data, strlen(test_data));
        }

        if (pipe_fd >= 0) {
            /* Check for input events from parent WM */
            n = read(pipe_fd, event_buf, sizeof(event_buf) - 1);
            if (n > 0) {
                event_buf[n] = '\0';
                /* TODO: Parse and handle input events */
                fprintf(stderr, "Nested WM received: %s", event_buf);
            }
        }

        /* Small sleep to avoid busy waiting */
        usleep(10000);  /* 10ms */
    }

    /* Cleanup */
    p9_close(marrow_client, screen_fd);
    p9_disconnect(marrow_client);

    fprintf(stderr, "Nested WM shutting down\n");
    return 0;
}

