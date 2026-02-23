/*
 * Kryon SDL2 Display Client Implementation
 * C89/C90 compliant
 */

#include "display.h"
#include "p9client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#ifdef HAVE_SDL2
#include <SDL2/SDL.h>
#endif

/*
 * usleep prototype for C89 compatibility
 */
extern int usleep(unsigned int usec);

/*
 * Default configuration
 */
#define DEFAULT_WIDTH  800
#define DEFAULT_HEIGHT 600
#define DEFAULT_PORT   17019

/*
 * Convert from BGRA (server) to RGBA (SDL)
 * Server sends: [B][G][R][A]
 * SDL RGBA8888 expects: [R][G][B][A]
 */
static void convert_pixels_argb_to_rgba(uint8_t *dst, const uint8_t *src, int width, int height)
{
    int i;
    int pixel_count = width * height;

    for (i = 0; i < pixel_count; i++) {
        dst[i * 4 + 0] = src[i * 4 + 2];  /* R <- B's position which has R */
        dst[i * 4 + 1] = src[i * 4 + 1];  /* G <- G */
        dst[i * 4 + 2] = src[i * 4 + 0];  /* B <- B's position which has B */
        dst[i * 4 + 3] = src[i * 4 + 3];  /* A <- A */
    }
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

    if (config != NULL) {
        host = config->host;
        port = config->port;
        width = config->width;
        height = config->height;
    } else {
        host = "127.0.0.1";
        port = DEFAULT_PORT;
        width = DEFAULT_WIDTH;
        height = DEFAULT_HEIGHT;
    }

    dc = (DisplayClient *)calloc(1, sizeof(DisplayClient));
    if (dc == NULL) {
        return NULL;
    }

    /* Connect to Kryon server */
    dc->p9 = p9_client_connect(host, port);
    if (dc->p9 == NULL) {
        fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
        free(dc);
        return NULL;
    }

    /* Attach to server */
    if (p9_client_attach(dc->p9, "") < 0) {
        fprintf(stderr, "Failed to attach: %s\n", p9_client_error(dc->p9));
        p9_client_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Open screen device */
    dc->screen_fd = p9_client_open_path(dc->p9, "/dev/screen", P9_OREAD);
    if (dc->screen_fd < 0) {
        fprintf(stderr, "Failed to open /dev/screen: %s\n", p9_client_error(dc->p9));
        p9_client_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Open mouse device */
    dc->mouse_fd = p9_client_open_path(dc->p9, "/dev/mouse", P9_OWRITE);
    if (dc->mouse_fd < 0) {
        fprintf(stderr, "Warning: Failed to open /dev/mouse\n");
    }

    /* Allocate screen buffer (RGBA32) - with extra space for conversion */
    dc->screen_width = width;
    dc->screen_height = height;
    dc->screen_buf = (uint8_t *)malloc(width * height * 4);
    dc->temp_buf = (uint8_t *)malloc(width * height * 4);  /* For raw data from server */
    if (dc->screen_buf == NULL || dc->temp_buf == NULL) {
        fprintf(stderr, "Failed to allocate screen buffer\n");
        free(dc->screen_buf);
        free(dc->temp_buf);
        p9_client_clunk(dc->p9, dc->screen_fd);
        p9_client_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

#ifdef HAVE_SDL2
    /* Initialize SDL2 */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL2: %s\n", SDL_GetError());
        free(dc->screen_buf);
        p9_client_clunk(dc->p9, dc->screen_fd);
        p9_client_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Create window */
    {
        SDL_Window *window;
        SDL_Renderer *renderer;
        SDL_Texture *texture;

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
            free(dc->screen_buf);
            p9_client_clunk(dc->p9, dc->screen_fd);
            p9_client_disconnect(dc->p9);
            free(dc);
            return NULL;
        }

        dc->sdl_window = window;

        /* Create renderer */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer == NULL) {
            fprintf(stderr, "Failed to create SDL2 renderer: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            free(dc->screen_buf);
            p9_client_clunk(dc->p9, dc->screen_fd);
            p9_client_disconnect(dc->p9);
            free(dc);
            return NULL;
        }

        dc->sdl_renderer = renderer;

        /* Create texture */
        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,  /* Use RGBA format */
            SDL_TEXTUREACCESS_STREAMING,
            width, height
        );

        if (texture == NULL) {
            fprintf(stderr, "Failed to create SDL2 texture: %s\n", SDL_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            free(dc->screen_buf);
            p9_client_clunk(dc->p9, dc->screen_fd);
            p9_client_disconnect(dc->p9);
            free(dc);
            return NULL;
        }

        dc->sdl_texture = texture;
    }
#else
    fprintf(stderr, "Warning: SDL2 not available, display will be limited\n");
#endif

    dc->running = 1;
    dc->refresh_needed = 1;

    fprintf(stderr, "Display client connected to %s:%d\n", host, port);

    return dc;
}

/*
 * Destroy display client
 */
void display_client_destroy(DisplayClient *dc)
{
    if (dc == NULL) {
        return;
    }

#ifdef HAVE_SDL2
    if (dc->sdl_texture != NULL) {
        SDL_DestroyTexture((SDL_Texture *)dc->sdl_texture);
    }
    if (dc->sdl_renderer != NULL) {
        SDL_DestroyRenderer((SDL_Renderer *)dc->sdl_renderer);
    }
    if (dc->sdl_window != NULL) {
        SDL_DestroyWindow((SDL_Window *)dc->sdl_window);
    }
    SDL_Quit();
#endif

    if (dc->screen_buf != NULL) {
        free(dc->screen_buf);
    }
    if (dc->temp_buf != NULL) {
        free(dc->temp_buf);
    }

    if (dc->screen_fd >= 0) {
        p9_client_clunk(dc->p9, dc->screen_fd);
    }
    if (dc->mouse_fd >= 0) {
        p9_client_clunk(dc->p9, dc->mouse_fd);
    }

    if (dc->p9 != NULL) {
        p9_client_disconnect(dc->p9);
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
    static int read_count = 0;

    total = 0;
    to_read = dc->screen_width * dc->screen_height * 4;

    /* Reset FID offset to 0 before reading full frame */
    p9_client_reset_fid(dc->p9, dc->screen_fd);

    /* Read raw data into temporary buffer */
    while (total < to_read) {
        nread = p9_client_read(dc->p9, dc->screen_fd,
                               (char *)(dc->temp_buf + total),
                               to_read - total);
        if (nread < 0) {
            return -1;
        }
        if (nread == 0) {
            break;
        }
        total += nread;
    }

    /* Debug: Print first few pixels on first read */
    if (read_count == 0 && total >= 16) {
        unsigned char *p = dc->temp_buf;
        fprintf(stderr, "First 4 pixels (raw from server): %02X %02X %02X %02X | %02X %02X %02X %02X | %02X %02X %02X %02X | %02X %02X %02X %02X\n",
                p[0], p[1], p[2], p[3],
                p[4], p[5], p[6], p[7],
                p[8], p[9], p[10], p[11],
                p[12], p[13], p[14], p[15]);
    }
    read_count++;

    /* Convert from BGRA (server) to RGBA (SDL) by swapping R and B */
    convert_pixels_argb_to_rgba(dc->screen_buf, dc->temp_buf,
                                  dc->screen_width, dc->screen_height);

    return 0;
}

/*
 * Update display with screen contents
 */
static int update_display(DisplayClient *dc)
{
#ifdef HAVE_SDL2
    SDL_Texture *texture = (SDL_Texture *)dc->sdl_texture;
    SDL_Renderer *renderer = (SDL_Renderer *)dc->sdl_renderer;
    static int first_call = 1;
    uint32_t format;

    if (texture == NULL || renderer == NULL) {
        return -1;
    }

    /* Debug: Print texture format on first call */
    if (first_call) {
        SDL_QueryTexture(texture, &format, NULL, NULL, NULL);
        fprintf(stderr, "SDL texture format: 0x%08X (%s)\n", format,
                format == SDL_PIXELFORMAT_BGRA8888 ? "BGRA8888" :
                format == SDL_PIXELFORMAT_RGBA8888 ? "RGBA8888" :
                format == SDL_PIXELFORMAT_ARGB8888 ? "ARGB8888" : "Unknown");

        /* Print first pixel from server data */
        fprintf(stderr, "First pixel from server: %02X%02X%02X%02X (BGR A)\n",
                dc->screen_buf[0], dc->screen_buf[1], dc->screen_buf[2], dc->screen_buf[3]);

        /* Check button and label pixels */
        int button_offset = (50 * 800 + 50) * 4;
        int label_offset = (120 * 800 + 50) * 4;
        fprintf(stderr, "Button pixel[50,50]: %02X%02X%02X%02X (B=%02X G=%02X R=%02X A=%02X) = RED\n",
                dc->screen_buf[button_offset], dc->screen_buf[button_offset+1],
                dc->screen_buf[button_offset+2], dc->screen_buf[button_offset+3],
                dc->screen_buf[button_offset], dc->screen_buf[button_offset+1],
                dc->screen_buf[button_offset+2], dc->screen_buf[button_offset+3]);
        fprintf(stderr, "Label pixel[50,120]: %02X%02X%02X%02X (B=%02X G=%02X R=%02X A=%02X) = YELLOW\n",
                dc->screen_buf[label_offset], dc->screen_buf[label_offset+1],
                dc->screen_buf[label_offset+2], dc->screen_buf[label_offset+3],
                dc->screen_buf[label_offset], dc->screen_buf[label_offset+1],
                dc->screen_buf[label_offset+2], dc->screen_buf[label_offset+3]);
        fprintf(stderr, "BGRA format: [Blue][Green][Red][Alpha] in memory\n");
        first_call = 0;
    }

    /* Update texture from screen buffer */
    if (SDL_UpdateTexture(texture, NULL, dc->screen_buf, dc->screen_width * 4) < 0) {
        fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    /* Render */
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    return 0;
#else
    fprintf(stderr, "No display backend available\n");
    return -1;
#endif
}

/*
 * Handle SDL2 event
 */
static int handle_sdl_event(DisplayClient *dc
#ifdef HAVE_SDL2
                            , SDL_Event *ev
#endif
                           )
{
#ifdef HAVE_SDL2
    switch (ev->type) {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        display_client_send_mouse(dc,
                                  ev->button.x,
                                  ev->button.y,
                                  (ev->type == SDL_MOUSEBUTTONDOWN) ? ev->button.button : 0);
        break;

    case SDL_MOUSEMOTION:
        display_client_send_mouse(dc,
                                  ev->motion.x,
                                  ev->motion.y,
                                  0);
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        display_client_send_key(dc,
                                ev->key.keysym.scancode,
                                ev->type == SDL_KEYDOWN);
        break;

    case SDL_WINDOWEVENT:
        if (ev->window.event == SDL_WINDOWEVENT_EXPOSED) {
            dc->refresh_needed = 1;
        }
        break;

    case SDL_QUIT:
        dc->running = 0;
        break;

    default:
        break;
    }
#endif

    return 0;
}

/*
 * Send mouse event to server
 */
int display_client_send_mouse(DisplayClient *dc, int x, int y, int buttons)
{
    char msg[64];
    int len;

    if (dc == NULL || dc->mouse_fd < 0) {
        return -1;
    }

    /* Format: m resized x y buttons msec */
    len = sprintf(msg, "m 0 %11d %11d %11d 0\n", x, y, buttons);

    if (len < 0 || len >= sizeof(msg)) {
        return -1;
    }

    return p9_client_write(dc->p9, dc->mouse_fd, msg, len);
}

/*
 * Send key event to server
 */
int display_client_send_key(DisplayClient *dc, int keycode, int pressed)
{
    (void)dc;
    (void)keycode;
    (void)pressed;

    /* TODO: Implement keyboard event sending */
    return 0;
}

/*
 * Request screen refresh
 */
void display_client_request_refresh(DisplayClient *dc)
{
    if (dc != NULL) {
        dc->refresh_needed = 1;
    }
}

/*
 * Run display client main loop
 */
int display_client_run(DisplayClient *dc)
{
    int frame_count = 0;

    if (dc == NULL) {
        return -1;
    }

    fprintf(stderr, "Display client running...\n");

    while (dc->running) {
#ifdef HAVE_SDL2
        /* Check for screen updates (~60 FPS) */
        if (dc->refresh_needed || frame_count % 60 == 0) {
            if (read_screen(dc) < 0) {
                fprintf(stderr, "Error reading screen\n");
                break;
            }
            if (update_display(dc) < 0) {
                fprintf(stderr, "Error updating display\n");
                break;
            }
            dc->refresh_needed = 0;
        }

        /* Process SDL events */
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                handle_sdl_event(dc, &ev);
            }
        }
#endif

        /* Sleep for ~60 FPS */
        usleep(16667);

        frame_count++;
    }

    fprintf(stderr, "Display client stopped\n");
    return 0;
}
