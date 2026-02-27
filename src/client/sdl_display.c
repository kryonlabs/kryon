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
 * Convert from BGRA (server) to actual SDL texture format
 * Server sends: [B][G][R][A] (BGRA)
 * SDL textures vary by platform - we need to query and convert
 */
static void convert_pixels_to_sdl_format(uint8_t *dst, const uint8_t *src, int width, int height, uint32_t sdl_format)
{
    int i;
    int pixel_count = width * height;

    /* Check the actual SDL format and convert accordingly */
    switch (sdl_format) {
    case 0x16462004:  /* SDL_PIXELFORMAT_ARGB8888 on some systems */
    case 0x16862004:  /* SDL_PIXELFORMAT_ARGB8888 variant - actually RGBA in memory! */
        /* The format ID says ARGB but the actual memory layout is RGBA */
        /* Need to convert from BGRA (server) to RGBA (texture) */
        for (i = 0; i < pixel_count; i++) {
            dst[i * 4 + 0] = src[i * 4 + 2];  /* R <- R */
            dst[i * 4 + 1] = src[i * 4 + 1];  /* G <- G */
            dst[i * 4 + 2] = src[i * 4 + 0];  /* B <- B */
            dst[i * 4 + 3] = src[i * 4 + 3];  /* A <- A */
        }
        break;

    case 0x36314742:  /* SDL_PIXELFORMAT_RGBA8888 */
        /* BGRA -> RGBA: [B,G,R,A] -> [R,G,B,A] */
        for (i = 0; i < pixel_count; i++) {
            dst[i * 4 + 0] = src[i * 4 + 2];  /* R <- R */
            dst[i * 4 + 1] = src[i * 4 + 1];  /* G <- G */
            dst[i * 4 + 2] = src[i * 4 + 0];  /* B <- B */
            dst[i * 4 + 3] = src[i * 4 + 3];  /* A <- A */
        }
        break;

    case 0x32415842:  /* SDL_PIXELFORMAT_BGRA8888 */
        /* Direct copy - no conversion needed */
        for (i = 0; i < pixel_count * 4; i++) {
            dst[i] = src[i];
        }
        break;

    default:
        /* Unknown format - try direct copy */
        fprintf(stderr, "Warning: Unknown SDL format 0x%X, attempting direct copy\n", (unsigned int)sdl_format);
        for (i = 0; i < pixel_count * 4; i++) {
            dst[i] = src[i];
        }
        break;
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
    char addr_str[256];

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

    /* Format address as tcp!host!port */
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

    /* Attach to server */
    if (p9_authenticate(dc->p9, P9_AUTH_NONE, "none", "") < 0) {
        fprintf(stderr, "Failed to attach: %s\n", p9_error(dc->p9));
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Open screen device */
    dc->screen_fd = p9_open(dc->p9, "/dev/screen", P9_OREAD);
    if (dc->screen_fd < 0) {
        fprintf(stderr, "Failed to open /dev/screen: %s\n", p9_error(dc->p9));
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

    /* Open mouse device */
    dc->mouse_fd = p9_open(dc->p9, "/dev/mouse", P9_OWRITE);
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
        p9_close(dc->p9, dc->screen_fd);
        p9_disconnect(dc->p9);
        free(dc);
        return NULL;
    }

#ifdef HAVE_SDL2
    /* Initialize SDL2 */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL2: %s\n", SDL_GetError());
        free(dc->screen_buf);
        p9_close(dc->p9, dc->screen_fd);
        p9_disconnect(dc->p9);
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
            p9_close(dc->p9, dc->screen_fd);
            p9_disconnect(dc->p9);
            free(dc);
            return NULL;
        }

        dc->sdl_window = window;

        fprintf(stderr, "SDL2 window created successfully\n");

        /* Create renderer */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer == NULL) {
            fprintf(stderr, "Failed to create SDL2 renderer: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            free(dc->screen_buf);
            p9_close(dc->p9, dc->screen_fd);
            p9_disconnect(dc->p9);
            free(dc);
            return NULL;
        }

        dc->sdl_renderer = renderer;

        /* Set clear color to black (was defaulting to unknown color) */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        /* Create texture */
        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_BGRA8888,  /* Match BGRA format from server - no conversion needed */
            SDL_TEXTUREACCESS_STREAMING,
            width, height
        );

        if (texture == NULL) {
            fprintf(stderr, "Failed to create SDL2 texture: %s\n", SDL_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            free(dc->screen_buf);
            p9_close(dc->p9, dc->screen_fd);
            p9_disconnect(dc->p9);
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
        p9_close(dc->p9, dc->screen_fd);
    }
    if (dc->mouse_fd >= 0) {
        p9_close(dc->p9, dc->mouse_fd);
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
    static int read_count = 0;
    static uint32_t cached_sdl_format = 0;
#ifdef HAVE_SDL2
    SDL_Texture *texture = (SDL_Texture *)dc->sdl_texture;
    uint32_t sdl_format;
#endif

    total = 0;
    to_read = dc->screen_width * dc->screen_height * 4;

    /* Reset FID offset to 0 before reading full frame */
    p9_reset_fid(dc->p9, dc->screen_fd);

    /* Read raw data into temporary buffer */
    while (total < to_read) {
        nread = p9_read(dc->p9, dc->screen_fd,
                               (char *)(dc->temp_buf + total),
                               to_read - total);
        if (nread < 0) {
            /* Connection lost - try to reopen /dev/screen */
            fprintf(stderr, "Connection lost, attempting to reconnect...\n");

            /* Close old file descriptor */
            p9_close(dc->p9, dc->screen_fd);

            /* Try to reopen /dev/screen */
            dc->screen_fd = p9_open(dc->p9, "/dev/screen", P9_OREAD);

            if (dc->screen_fd < 0) {
                /* Still failed - wait and try again next frame */
                fprintf(stderr, "Reconnection failed, will retry...\n");
                usleep(100000);  /* 100ms */
                return 0;  /* Don't exit, try again next frame */
            }

            fprintf(stderr, "Successfully reconnected to /dev/screen\n");

            /* Reset to beginning of frame for new connection */
            p9_reset_fid(dc->p9, dc->screen_fd);
            total = 0;  /* Restart the read */

            /* Try reading again */
            continue;
        }
        if (nread == 0) {
            fprintf(stderr, "Error: Server closed connection, got %zu of %zu bytes\n",
                    total, to_read);
            return -1;  /* Fail fast instead of filling with garbage */
        }
        total += nread;
    }

    /* Verify we read all the data */
    if (total != to_read) {
        fprintf(stderr, "Error: Only read %zu of %zu bytes - incomplete frame!\n", total, to_read);
        /* Fill remaining with BLACK (all zeros in BGRA) */
        if (total < to_read) {
            size_t i;
            size_t remaining = to_read - total;
            uint8_t *dst = dc->temp_buf + total;
            /* Fill with black pixels (B=0, G=0, R=0, A=255) */
            for (i = 0; i < remaining; i += 4) {
                dst[i + 0] = 0x00;  /* B */
                dst[i + 1] = 0x00;  /* G */
                dst[i + 2] = 0x00;  /* R */
                dst[i + 3] = 0xFF;  /* A */
            }
        }
    }

    read_count++;

    /* Log successful read */
    if (read_count == 1 || read_count % 60 == 0) {
        fprintf(stderr, "read_screen: frame %d, read %zu bytes successfully\n",
                read_count, total);
    }

#ifdef HAVE_SDL2
    /* Get actual SDL format on first read, then cache it */
    if (cached_sdl_format == 0 && texture != NULL) {
        SDL_QueryTexture(texture, &sdl_format, NULL, NULL, NULL);
        cached_sdl_format = sdl_format;
        fprintf(stderr, "Detected SDL format: 0x%X\n", (unsigned int)sdl_format);
    }

    /* Print first pixel from server on first read (should be BGRA) */
    if (read_count == 1) {
        fprintf(stderr, "First pixel from server: %02X %02X %02X %02X (expected BGRA for DBlue: FF 00 00 FF)\n",
                dc->temp_buf[0], dc->temp_buf[1], dc->temp_buf[2], dc->temp_buf[3]);
    }

    /* Convert from server BGRA to actual SDL format */
    convert_pixels_to_sdl_format(dc->screen_buf, dc->temp_buf,
                                  dc->screen_width, dc->screen_height, cached_sdl_format);

    /* Print first pixel after conversion on first read */
    if (read_count == 1) {
        fprintf(stderr, "First pixel after conversion: %02X %02X %02X %02X\n",
                dc->screen_buf[0], dc->screen_buf[1], dc->screen_buf[2], dc->screen_buf[3]);
    }
#endif

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
    static int frame_count = 0;
    static int first_call = 1;
    uint32_t format;
    int i;
    int button_offset;
    int label_offset;
    FILE *dump_file;
    size_t buf_size;
    size_t written;

    if (texture == NULL || renderer == NULL) {
        return -1;
    }

    /* Debug: Print texture format and first 4 pixels on first call */
    if (first_call) {
        SDL_QueryTexture(texture, &format, NULL, NULL, NULL);
        fprintf(stderr, "Texture format: 0x%X (RGBA8888=0x36314742)\n", (unsigned int)format);
        fprintf(stderr, "First 4 pixels (16 bytes): ");
        for (i = 0; i < 16 && i < dc->screen_width * dc->screen_height * 4; i++) {
            fprintf(stderr, "%02X ", dc->screen_buf[i]);
        }
        fprintf(stderr, "\n");

        /* Check widget pixels */
        button_offset = ((50 * dc->screen_width) + 50) * 4;  /* (50,50) */
        label_offset = ((120 * dc->screen_width) + 50) * 4;  /* (50,120) */

        fprintf(stderr, "Display sees button at (50,50): %02X %02X %02X %02X (after conversion to SDL format)\n",
                dc->screen_buf[button_offset], dc->screen_buf[button_offset+1],
                dc->screen_buf[button_offset+2], dc->screen_buf[button_offset+3]);

        fprintf(stderr, "Display sees label at (50,120): %02X %02X %02X %02X (after conversion to SDL format)\n",
                dc->screen_buf[label_offset], dc->screen_buf[label_offset+1],
                dc->screen_buf[label_offset+2], dc->screen_buf[label_offset+3]);

        first_call = 0;

        /* Dump screenshot if requested */
        if (dc->dump_screen) {
            dump_file = fopen("/tmp/display_after.raw", "wb");
            if (dump_file != NULL) {
                buf_size = dc->screen_width * dc->screen_height * 4;
                written = fwrite(dc->screen_buf, 1, buf_size, dump_file);
                fclose(dump_file);
                if (written == buf_size) {
                    fprintf(stderr, "Dumped display buffer to /tmp/display_after.raw (%zu bytes)\n", written);
                } else {
                    fprintf(stderr, "Warning: Only wrote %zu of %zu bytes to screenshot\n", written, buf_size);
                }
            } else {
                fprintf(stderr, "Warning: Failed to open /tmp/display_after.raw for writing\n");
            }
        }
    }

    frame_count++;

    /* Log every 60 frames */
    if (frame_count % 60 == 0) {
        fprintf(stderr, "Frame %d: displaying %dx%d\n", frame_count, dc->screen_width, dc->screen_height);
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
    case SDL_QUIT:
        if (dc->stay_open) {
            fprintf(stderr, "SDL_QUIT received but ignored (stay-open mode)\n");
        } else {
            fprintf(stderr, "SDL_QUIT event received, exiting...\n");
            dc->running = 0;
        }
        break;

    case SDL_KEYDOWN:
        /* ESC key always exits, even in stay-open mode */
        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            fprintf(stderr, "ESC pressed, exiting...\n");
            dc->running = 0;
            break;
        }
        /* Fall through to normal key handling */
        display_client_send_key(dc,
                                ev->key.keysym.scancode,
                                1);
        break;
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

    case SDL_KEYUP:
        /* Handle key release */
        display_client_send_key(dc,
                                ev->key.keysym.scancode,
                                0);
        break;

    case SDL_WINDOWEVENT:
        if (ev->window.event == SDL_WINDOWEVENT_EXPOSED) {
            dc->refresh_needed = 1;
        }
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

    return p9_write(dc->p9, dc->mouse_fd, msg, len);
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
