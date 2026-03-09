/*
 * Kryon View - Display Viewer using SDL2
 * C89/C90 compliant
 *
 * Connects to Kryon WM via 9P and displays screen using SDL2.
 */

#include "display.h"
#include "p9client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <SDL2/SDL.h>

/*
 * usleep prototype for C89 compatibility
 */
extern int usleep(unsigned int usec);

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
    fprintf(stderr, "  --width W      Override window width (default: from WM)\n");
    fprintf(stderr, "  --height H     Override window height (default: from WM)\n");
    fprintf(stderr, "  --verbose      Enable verbose output\n");
    fprintf(stderr, "  --stay-open    Keep window open (ignore quit events)\n");
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

    config->host = "127.0.0.1";
    config->port = 17010;
    config->width = 0;
    config->height = 0;
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
 * Create display client
 */
DisplayClient *display_client_create(const DisplayConfig *config)
{
    DisplayClient *dc;
    const char *host;
    int port;
    int width;
    int height;
    char addr_str[256];
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    if (config != NULL) {
        host = config->host;
        port = config->port;
        width = config->width;
        height = config->height;
    } else {
        host = "127.0.0.1";
        port = 17010;
        width = 0;
        height = 0;
    }

    sprintf(addr_str, "tcp!%s!%d", host, port);

    dc = (DisplayClient *)calloc(1, sizeof(DisplayClient));
    if (dc == NULL) {
        return NULL;
    }

    /* Connect to Kryon server */
    dc->p9 = p9_connect(addr_str);
    if (dc->p9 == NULL) {
        fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
        free(dc);
        return NULL;
    }

    strncpy(dc->server_addr, addr_str, sizeof(dc->server_addr) - 1);
    dc->server_addr[sizeof(dc->server_addr) - 1] = '\0';
    dc->connected = 1;

    /* Attach to server */
    if (p9_authenticate(dc->p9, P9_AUTH_NONE, "none", "") < 0) {
        fprintf(stderr, "Failed to attach: %s\n", p9_error(dc->p9));
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Query screen dimensions from /dev/display/ctl */
    if (width == 0 || height == 0) {
        int retry;
        int display_ctl_fd;
        char buf[64];
        ssize_t n;

        fprintf(stderr, "Reading display size from /dev/display/ctl...\n");

        for (retry = 0; retry < 50; retry++) {
            display_ctl_fd = p9_open(dc->p9, "/dev/display/ctl", P9_OREAD);
            if (display_ctl_fd >= 0) {
                n = p9_read(dc->p9, display_ctl_fd, buf, sizeof(buf) - 1);
                p9_close(dc->p9, display_ctl_fd);

                if (n > 0) {
                    buf[n] = '\0';
                    if (sscanf(buf, "%dx%d", &width, &height) == 2) {
                        if (width > 0 && height > 0) {
                            fprintf(stderr, "Got display size from WM: %dx%d\n", width, height);
                            break;
                        }
                    }
                }
            }
            if (retry == 0) {
                fprintf(stderr, "Waiting for WM to set display size...\n");
            }
            usleep(100000);
        }
    }

    if (width == 0 || height == 0) {
        fprintf(stderr, "Screen is 0x0 — no windows defined\n");
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Initialize SDL2 */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL2: %s\n", SDL_GetError());
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Create window */
    window = SDL_CreateWindow(
        "Kryon Display",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN
    );

    if (window == NULL) {
        fprintf(stderr, "Failed to create SDL2 window: %s\n", SDL_GetError());
        SDL_Quit();
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Create renderer */
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "Failed to create SDL2 renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Create texture */
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height
    );

    if (texture == NULL) {
        fprintf(stderr, "Failed to create SDL2 texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Store SDL objects (we'll use screen_buf to hold them for C89 compatibility) */
    dc->screen_width = width;
    dc->screen_height = height;
    dc->screen_buf = (uint8_t *)malloc(width * height * 4);
    if (dc->screen_buf == NULL) {
        fprintf(stderr, "Failed to allocate screen buffer\n");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Store SDL objects in unused fields for now (this is a hack) */
    *((SDL_Window **)&dc->sdl_window) = window;
    *((SDL_Renderer **)&dc->sdl_renderer) = renderer;
    *((SDL_Texture **)&dc->sdl_texture) = texture;

    /* Open screen device */
    dc->screen_fd = p9_open(dc->p9, "/dev/screen/data", P9_OREAD);
    if (dc->screen_fd < 0) {
        fprintf(stderr, "Failed to open /dev/screen: %s\n", p9_error(dc->p9));
        free(dc->screen_buf);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Open persistent mouse and keyboard connections */
    dc->mouse_fd = p9_open(dc->p9, "/dev/mouse", P9_OWRITE);
    if (dc->mouse_fd < 0) {
        fprintf(stderr, "Warning: failed to open /dev/mouse\n");
    }

    dc->kbd_fd = p9_open(dc->p9, "/dev/kbd", P9_OWRITE);
    if (dc->kbd_fd < 0) {
        fprintf(stderr, "Warning: failed to open /dev/kbd\n");
    }

    dc->running = 1;
    dc->stay_open = (config != NULL) ? config->stay_open : 0;
    dc->dump_screen = (config != NULL) ? config->dump_screen : 0;

    fprintf(stderr, "Display client connected to %s:%d\n", host, port);

    return dc;
}

/*
 * Destroy display client
 */
void display_client_destroy(DisplayClient *dc)
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    if (dc == NULL) {
        return;
    }

    /* Extract SDL objects */
    window = *((SDL_Window **)&dc->sdl_window);
    renderer = *((SDL_Renderer **)&dc->sdl_renderer);
    texture = *((SDL_Texture **)&dc->sdl_texture);

    if (texture != NULL) {
        SDL_DestroyTexture(texture);
    }
    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
    }
    if (window != NULL) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();

    if (dc->screen_buf != NULL) {
        free(dc->screen_buf);
    }

    /* Close persistent connections */
    if (dc->mouse_fd >= 0) {
        p9_close(dc->p9, dc->mouse_fd);
        dc->mouse_fd = -1;
    }

    if (dc->kbd_fd >= 0) {
        p9_close(dc->p9, dc->kbd_fd);
        dc->kbd_fd = -1;
    }

    if (dc->screen_fd >= 0) {
        p9_close(dc->p9, dc->screen_fd);
    }

    if (dc->p9 != NULL) {
        p9_disconnect(dc->p9);
    }

    free(dc);
}

/*
 * Read screen from server
 */
static int read_screen(DisplayClient *dc)
{
    ssize_t nread;
    size_t total;
    size_t to_read;

    /* Reset file offset to 0 before reading each frame */
    p9_reset_fid(dc->p9, dc->screen_fd);

    total = 0;
    to_read = dc->screen_width * dc->screen_height * 4;

    while (total < to_read) {
        nread = p9_read(dc->p9, dc->screen_fd,
                       (char *)(dc->screen_buf + total),
                       to_read - total);
        if (nread < 0) {
            fprintf(stderr, "Connection lost, attempting to reconnect...\n");

            if (dc->screen_fd >= 0) {
                p9_close(dc->p9, dc->screen_fd);
                dc->screen_fd = -1;
            }

            dc->connected = 0;

            if (p9_reconnect((P9Client **)&dc->p9, dc->server_addr) < 0) {
                fprintf(stderr, "Reconnection failed, will retry...\n");
                usleep(100000);
                return 0;
            }

            {
                int screen_open_retry = 0;
                while (screen_open_retry < 20) {
                    dc->screen_fd = p9_open(dc->p9, "/dev/screen/data", P9_OREAD);
                    if (dc->screen_fd >= 0) {
                        break;
                    }
                    fprintf(stderr, "Failed to open /dev/screen (attempt %d), retrying...\n",
                            screen_open_retry + 1);
                    usleep(100000 * (1 << screen_open_retry));
                    screen_open_retry++;
                }

                if (dc->screen_fd < 0) {
                    usleep(500000);
                    return 0;
                }
            }

            dc->connected = 1;
            fprintf(stderr, "Successfully reconnected to server\n");

            p9_reset_fid(dc->p9, dc->screen_fd);
            total = 0;
            continue;
        }
        if (nread == 0) {
            fprintf(stderr, "Error: Server closed connection\n");
            return -1;
        }
        total += nread;
    }

    return 0;
}

/*
 * Update display
 */
static int update_display(DisplayClient *dc)
{
    SDL_Texture *texture = *((SDL_Texture **)&dc->sdl_texture);
    SDL_Renderer *renderer = *((SDL_Renderer **)&dc->sdl_renderer);

    static int first_call = 1;

    if (first_call) {
        first_call = 0;
        if (dc->dump_screen) {
            FILE *dump_file = fopen("/tmp/display_after.raw", "wb");
            if (dump_file != NULL) {
                fwrite(dc->screen_buf, 1, dc->screen_width * dc->screen_height * 4, dump_file);
                fclose(dump_file);
                fprintf(stderr, "Dumped screenshot to /tmp/display_after.raw\n");
            }
        }
    }

    if (SDL_UpdateTexture(texture, NULL, dc->screen_buf, dc->screen_width * 4) < 0) {
        fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    return 0;
}

/*
 * Send mouse event to server
 */
int display_client_send_mouse(DisplayClient *dc, int x, int y, int buttons)
{
    char msg[64];
    int len;

    if (dc == NULL || !dc->connected) {
        return -1;
    }

    len = sprintf(msg, "m 0 %11d %11d %11d 0\n", x, y, buttons);

    {
        int mouse_fd = p9_open(dc->p9, "/dev/mouse", P9_OWRITE);
        if (mouse_fd >= 0) {
            p9_write(dc->p9, mouse_fd, msg, len);
            p9_close(dc->p9, mouse_fd);
            return 0;
        }
    }

    return -1;
}

/*
 * Send key event to server
 */
int display_client_send_key(DisplayClient *dc, int keycode, int pressed)
{
    char msg[64];
    int len;

    if (dc == NULL || !dc->connected) {
        return -1;
    }

    len = sprintf(msg, "k %d %d\n", keycode, pressed);

    {
        int keybd_fd = p9_open(dc->p9, "/dev/kbd", P9_OWRITE);
        if (keybd_fd >= 0) {
            p9_write(dc->p9, keybd_fd, msg, len);
            p9_close(dc->p9, keybd_fd);
            return 0;
        }
    }

    return -1;
}

/*
 * Request screen refresh
 */
void display_client_request_refresh(DisplayClient *dc)
{
    (void)dc;
}

/*
 * Run display client main loop
 */
int display_client_run(DisplayClient *dc)
{
    int read_error_count = 0;

    if (dc == NULL) {
        return -1;
    }

    fprintf(stderr, "Display client running...\n");

    while (dc->running && running) {
        if (read_screen(dc) < 0) {
            if (++read_error_count > 10) {
                fprintf(stderr, "Error reading screen after 10 attempts, exiting...\n");
                break;
            }
            usleep(100000);
            continue;
        }
        read_error_count = 0;

        if (update_display(dc) < 0) {
            fprintf(stderr, "Error updating display\n");
            break;
        }

        /* Process SDL events */
        if (dc->connected) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                switch (ev.type) {
                case SDL_QUIT:
                    if (dc->stay_open) {
                        fprintf(stderr, "SDL_QUIT received but ignored (stay-open mode)\n");
                    } else {
                        fprintf(stderr, "SDL_QUIT event received, exiting...\n");
                        dc->running = 0;
                    }
                    break;

                case SDL_KEYDOWN:
                    if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        fprintf(stderr, "ESC pressed, exiting...\n");
                        dc->running = 0;
                    } else {
                        display_client_send_key(dc, ev.key.keysym.scancode, 1);
                    }
                    break;

                case SDL_KEYUP:
                    display_client_send_key(dc, ev.key.keysym.scancode, 0);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    display_client_send_mouse(dc, ev.button.x, ev.button.y, ev.button.button);
                    break;

                case SDL_MOUSEBUTTONUP:
                    display_client_send_mouse(dc, ev.button.x, ev.button.y, 0);
                    break;

                case SDL_MOUSEMOTION:
                    display_client_send_mouse(dc, ev.motion.x, ev.motion.y, ev.motion.state);
                    break;
                }
            }
        }

        usleep(16667);
    }

    fprintf(stderr, "Display client stopped\n");
    return 0;
}

/*
 * Display entry point
 */
int display_main(int argc, char **argv)
{
    DisplayConfig config;
    DisplayClient *dc;
    int result;

    result = parse_args(argc, argv, &config);
    if (result < 0) {
        return 1;
    }
    if (result > 0) {
        return 0;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fprintf(stderr, "Kryon Display Client\n");
    fprintf(stderr, "Connecting to %s:%d...\n", config.host, config.port);

    dc = display_client_create(&config);
    if (dc == NULL) {
        fprintf(stderr, "Failed to create display client\n");
        return 1;
    }

    result = display_client_run(dc);

    display_client_destroy(dc);

    fprintf(stderr, "Shutdown complete\n");

    return result;
}

/*
 * Main entry point
 */
int main(int argc, char **argv)
{
    return display_main(argc, argv);
}
