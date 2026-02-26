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

#include "window.h"
#include "widget.h"
#include "wm.h"
#include "p9client.h"
#include "marrow.h"
#include "kryon.h"
#include "../include/graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

/*
 * Forward declarations for render functions
 */
void render_set_screen(Memimage *screen);
void render_all(void);

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
    KryonWindow *win1;
    KryonWidget *btn;
    KryonWidget *lbl;
    int i;

    /* Parse arguments - now just takes Marrow address */
    char *marrow_addr = "tcp!localhost!17010";
    int dump_screen = 0;  /* Flag to enable screenshot dumps */
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
        } else if (strcmp(argv[i], "--help") == 0) {
            fprintf(stderr, "Usage: %s [OPTIONS]\n", argv[0]);
            fprintf(stderr, "\n");
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "  --marrow ADDR   Marrow server address (default: tcp!localhost!17010)\n");
            fprintf(stderr, "  --dump-screen   Dump screenshot to /tmp/wm_before.raw\n");
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
    fprintf(stderr, "Kryon Window Manager v0.3\n");
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
            fprintf(stderr, "Error: only read %zd bytes of screen info (expected 144)\n", n);
            p9_disconnect(g_marrow_client);
            return 1;
        }
        fprintf(stderr, "Connected to Marrow screen (read %zd bytes info)\n", n);
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
    fprintf(stderr, "Created /mnt/wm filesystem\n");

    /* Register Kryon as window-manager service */
    if (marrow_service_register(g_marrow_client, "kryon", "window-manager",
                                "Kryon Window Manager") < 0) {
        fprintf(stderr, "Error: failed to register with Marrow\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    /* Mount /mnt/wm into Marrow's namespace */
    if (marrow_namespace_mount(g_marrow_client, "/mnt/wm") < 0) {
        fprintf(stderr, "Warning: failed to mount /mnt/wm\n");
        /* Continue anyway - service is registered */
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

    /* Create a test window */
    win1 = window_create("Demo Window", 800, 600);
    if (win1 == NULL) {
        fprintf(stderr, "Error: failed to create window\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    fprintf(stderr, "Created window %u: %s\n", win1->id, win1->title);

    /* Add a button widget */
    btn = widget_create(WIDGET_BUTTON, "btn_click", win1);
    if (btn == NULL) {
        fprintf(stderr, "Error: failed to create button widget\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    free(btn->prop_text);
    btn->prop_text = (char *)malloc(10);
    if (btn->prop_text != NULL) {
        strcpy(btn->prop_text, "Click Me");
    }

    free(btn->prop_rect);
    btn->prop_rect = (char *)malloc(16);
    if (btn->prop_rect != NULL) {
        strcpy(btn->prop_rect, "50 50 200 50");
    }

    window_add_widget(win1, btn);
    fprintf(stderr, "  Created widget %u: button (text='%s', rect='%s')\n",
            btn->id, btn->prop_text, btn->prop_rect);

    /* Add a label widget */
    lbl = widget_create(WIDGET_LABEL, "lbl_hello", win1);
    if (lbl == NULL) {
        fprintf(stderr, "Error: failed to create label widget\n");
        p9_disconnect(g_marrow_client);
        return 1;
    }

    free(lbl->prop_text);
    lbl->prop_text = (char *)malloc(14);
    if (lbl->prop_text != NULL) {
        strcpy(lbl->prop_text, "Hello, World!");
    }

    free(lbl->prop_rect);
    lbl->prop_rect = (char *)malloc(16);
    if (lbl->prop_rect != NULL) {
        strcpy(lbl->prop_rect, "50 120 300 40");
    }

    window_add_widget(win1, lbl);
    fprintf(stderr, "  Created widget %u: label (text='%s', rect='%s')\n",
            lbl->id, lbl->prop_text, lbl->prop_rect);

    /* NOTE: Rendering is now done through Marrow's /dev/draw */
    /* Create screen buffer and render windows */
    fprintf(stderr, "\nInitializing rendering...\n");

    {
        Memimage *screen;
        Rectangle screen_rect;
        int screen_width = 800;
        int screen_height = 600;
        int pixel_size;
        unsigned char *pixel_data;
        int screen_fd;
        ssize_t written;

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

        fprintf(stderr, "Created screen image: %dx%d\n", screen_width, screen_height);

        /* Set screen as global for rendering */
        render_set_screen(screen);

        /* Render all windows to screen */
        fprintf(stderr, "Rendering windows...\n");
        render_all();

        /* Extract pixel data */
        pixel_size = screen_width * screen_height * 4;
        pixel_data = screen->data->bdata;

        fprintf(stderr, "Extracted %d bytes of pixel data\n", pixel_size);

        /* Verify we have pixel data by checking first few pixels */
        {
            int i;
            fprintf(stderr, "First 20 pixels of screen data: ");
            for (i = 0; i < 20 && i < pixel_size; i++) {
                fprintf(stderr, "%02X ", pixel_data[i]);
            }
            fprintf(stderr, "\n");
        }

        /* Check widget pixels: button at (50,50) should be RED, label at (50,120) should be YELLOW */
        {
            int button_offset = ((50 * screen_width) + 50) * 4;  /* (50,50) */
            int label_offset = ((120 * screen_width) + 50) * 4;  /* (50,120) */

            fprintf(stderr, "Button pixel at (50,50): %02X %02X %02X %02X (expect RED: FF 00 00 FF)\n",
                    pixel_data[button_offset], pixel_data[button_offset+1],
                    pixel_data[button_offset+2], pixel_data[button_offset+3]);

            fprintf(stderr, "Label pixel at (50,120): %02X %02X %02X %02X (expect YELLOW: FF FF 00 FF)\n",
                    pixel_data[label_offset], pixel_data[label_offset+1],
                    pixel_data[label_offset+2], pixel_data[label_offset+3]);
        }

        /* Dump screenshot if requested */
        if (dump_screen) {
            FILE *dump_file = fopen("/tmp/wm_before.raw", "wb");
            if (dump_file != NULL) {
                size_t written = fwrite(pixel_data, 1, pixel_size, dump_file);
                fclose(dump_file);
                if (written == (size_t)pixel_size) {
                    fprintf(stderr, "Dumped WM screen to /tmp/wm_before.raw (%d bytes)\n", pixel_size);
                } else {
                    fprintf(stderr, "Warning: Only wrote %zu of %d bytes to screenshot\n", written, pixel_size);
                }
            } else {
                fprintf(stderr, "Warning: Failed to open /tmp/wm_before.raw for writing\n");
            }
        }

        /* Open /dev/screen on Marrow for writing */
        fprintf(stderr, "Opening /dev/screen on Marrow for writing...\n");
        screen_fd = p9_open(g_marrow_client, "/dev/screen", P9_OWRITE);
        if (screen_fd < 0) {
            fprintf(stderr, "Warning: failed to open /dev/screen for writing\n");
            fprintf(stderr, "Pixels rendered locally but not sent to Marrow\n");
        } else {
            /* Write pixel data to Marrow's screen in chunks */
            /* 9P has a max message size of ~8KB, so we need to chunk the writes */
            /* Account for message overhead: header (23 bytes) + data */
            int chunk_size = 7000;  /* Safe chunk size after overhead */
            int bytes_remaining = pixel_size;
            const unsigned char *buffer_ptr = pixel_data;
            int total_written = 0;

            fprintf(stderr, "Writing %d bytes to Marrow screen in chunks of %d...\n",
                    pixel_size, chunk_size);

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

            if (total_written == pixel_size) {
                fprintf(stderr, "Successfully wrote %d bytes to Marrow screen\n", total_written);
            } else {
                fprintf(stderr, "Warning: only wrote %d of %d bytes\n", total_written, pixel_size);
            }
            p9_close(g_marrow_client, screen_fd);
        }

        fprintf(stderr, "Rendering complete!\n");
    }

    /* Simple event loop */
    fprintf(stderr, "\nKryon window manager running\n");
    fprintf(stderr, "Press Ctrl-C to exit\n");

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Simple loop - just wait for signals */
    while (running) {
        sleep(1);
        /* TODO: Poll Marrow for events */
        /* TODO: Handle window manager events */
    }

    /* Cleanup */
    fprintf(stderr, "\nShutting down...\n");

    /* Unregister from Marrow */
    marrow_service_unregister(g_marrow_client, "kryon");

    /* event_system_cleanup();  TODO: Re-enable when graphics linked */
    tree_cleanup();

    p9_disconnect(g_marrow_client);

    fprintf(stderr, "Shutdown complete\n");

    return 0;
}
